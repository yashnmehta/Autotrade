#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <chrono>
#include <unordered_map>
#include "services/PriceCacheZeroCopy.h"
#include "utils/PreferencesManager.h"

namespace nsecm {

void parse_message_18703(const MS_TICKER_TRADE_DATA* msg) {
    uint16_t numRecords = be16toh_func(msg->numberOfRecords);
    
    // Capture timestamps for latency tracking
    static uint64_t refNoCounter = 0;
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    for (int i = 0; i < numRecords && i < 28; i++) {
        const auto& rec = msg->records[i];
        uint32_t token = be32toh_func(rec.token);
        
        if (token > 0) {
            uint64_t refNo = ++refNoCounter;
            
            // =====================================================================
            // CONFIG-DRIVEN EITHER/OR PATH
            // =====================================================================
            
            if (PreferencesManager::instance().getUseLegacyPriceCache()) {
                // =================================================================
                // LEGACY PATH: Callbacks (use_legacy_priceCache = true)
                // =================================================================
                
                // Parse ticker data
                TickerData ticker;
                ticker.token = token;
                ticker.refNo = refNo;
                ticker.timestampRecv = now;
                ticker.timestampParsed = now;
                ticker.fillPrice = be32toh_func(rec.fillPrice) / 100.0;
                ticker.fillVolume = be32toh_func(rec.fillVolume);
                ticker.marketIndexValue = be32toh_func(rec.marketIndexValue);
                ticker.marketType = be16toh_func(rec.marketType);
                ticker.openInterest = 0; // Not in CM 18703
                
                // Dispatch ticker callback
                MarketDataCallbackRegistry::instance().dispatchTicker(ticker);
                
            } else {
                // =================================================================
                // NEW PATH: PriceCache Direct Write (use_legacy_priceCache = false)
                // =================================================================
                
                // Static cache for WriteHandles (per-thread, 28 tokens/packet performance)
                static std::unordered_map<uint32_t, 
                    PriceCacheTypes::PriceCacheZeroCopy::WriteHandle> s_handleCache;
                
                // Get or create WriteHandle
                auto it = s_handleCache.find(token);
                if (it == s_handleCache.end()) {
                    auto handle = PriceCacheTypes::PriceCacheZeroCopy::getInstance()
                        .getWriteHandle(token, PriceCacheTypes::MarketSegment::NSE_CM);
                    
                    if (!handle.valid) {
                        // Token not in PriceCache (expected for non-subscribed tokens)
                        continue;  // Skip to next token in packet
                    }
                    
                    it = s_handleCache.emplace(token, handle).first;
                }
                
                auto& h = it->second;
                
                // Atomic write with sequence (torn-read prevention)
                uint64_t seq = h.sequencePtr->fetch_add(1, std::memory_order_relaxed);
                
                // DIRECT field writes (Message 18703 - Fast LTP Ticker)
                // IMPORTANT: fillPrice is in PAISE (no /100 needed for array)
                
                // Fast LTP update (most important field for ticker)
                h.dataPtr->lastTradedPrice = be32toh_func(rec.fillPrice);
                
                // Additional ticker fields
                h.dataPtr->fillPrice = be32toh_func(rec.fillPrice);
                h.dataPtr->fillVolume = be32toh_func(rec.fillVolume);
                h.dataPtr->marketType = be16toh_func(rec.marketType);
                
                // Note: 18703 provides minimal data - just fast LTP
                // Other fields (OHLC, depth) come from 7200/7208
                
                // Publish update (memory_order_release ensures all writes visible to readers)
                h.sequencePtr->store(seq + 1, std::memory_order_release);
            }
        }
    }
}

void parse_ticker_trade_data(const MS_TICKER_TRADE_DATA* msg) {
    parse_message_18703(msg);
}

} // namespace nsecm
