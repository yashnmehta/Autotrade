#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <chrono>
#include <unordered_map>
#include "services/PriceCacheZeroCopy.h"
#include "utils/PreferencesManager.h"

namespace nsecm {

void parse_message_7200(const MS_BCAST_MBO_MBP* msg) {
    uint32_t token = be32toh_func(msg->mboData.token);
    
    // Capture timestamps for latency tracking
    static uint64_t refNoCounter = 0;
    uint64_t refNo = ++refNoCounter;
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // =========================================================================
    // CONFIG-DRIVEN EITHER/OR PATH
    // =========================================================================
    
    if (PreferencesManager::instance().getUseLegacyPriceCache()) {
        // =====================================================================
        // LEGACY PATH: Callbacks (use_legacy_priceCache = true)
        // =====================================================================
        
        // Parse touchline data
        TouchlineData touchline;
        touchline.token = token;
        touchline.refNo = refNo;
        touchline.timestampRecv = now;
        touchline.timestampParsed = now;
        touchline.ltp = be32toh_func(msg->mboData.lastTradedPrice) / 100.0;
        
        touchline.open = be32toh_func(msg->openPrice) / 100.0;
        touchline.high = be32toh_func(msg->highPrice) / 100.0;
        touchline.low = be32toh_func(msg->lowPrice) / 100.0;
        touchline.close = be32toh_func(msg->closingPrice) / 100.0;
        touchline.volume = be64toh_func(msg->mboData.volumeTradedToday); // CM is LONG LONG
        touchline.lastTradeQty = be32toh_func(msg->mboData.lastTradeQuantity);
        touchline.lastTradeTime = be32toh_func(msg->mboData.lastTradeTime);
        touchline.avgPrice = be32toh_func(msg->mboData.averageTradePrice) / 100.0;
        touchline.netChangeIndicator = msg->mboData.netChangeIndicator;
        touchline.netChange = be32toh_func(msg->mboData.netPriceChangeFromClosingPrice) / 100.0;
        touchline.tradingStatus = be16toh_func(msg->mboData.tradingStatus);
        touchline.bookType = be16toh_func(msg->mboData.bookType);
        
        // Dispatch touchline callback
        MarketDataCallbackRegistry::instance().dispatchTouchline(touchline);
        
        // Parse market depth data (from mbpBuffer)
        MarketDepthData depth;
        depth.token = token;
        depth.refNo = refNo;
        depth.timestampRecv = now;
        depth.timestampParsed = now;
        depth.totalBuyQty = be64toh_func(msg->totalBuyQuantity); // CM is LONG LONG
        depth.totalSellQty = be64toh_func(msg->totalSellQuantity); // CM is LONG LONG
        
        // Parse bid/ask levels from MBP Information (next 5 Buy, next 5 Sell)
        for (int i = 0; i < 5; i++) {
            depth.bids[i].quantity = be64toh_func(msg->mbpBuffer[i].quantity); // CM is LONG LONG
            depth.bids[i].price = be32toh_func(msg->mbpBuffer[i].price) / 100.0;
            depth.bids[i].orders = be16toh_func(msg->mbpBuffer[i].numberOfOrders);
        }
        
        for (int i = 5; i < 10; i++) {
            int idx = i - 5;
            depth.asks[idx].quantity = be64toh_func(msg->mbpBuffer[i].quantity); // CM is LONG LONG
            depth.asks[idx].price = be32toh_func(msg->mbpBuffer[i].price) / 100.0;
            depth.asks[idx].orders = be16toh_func(msg->mbpBuffer[i].numberOfOrders);
        }
        
        // Dispatch market depth callback
        MarketDataCallbackRegistry::instance().dispatchMarketDepth(depth);
        
    } else {
        // =====================================================================
        // NEW PATH: PriceCache Direct Write (use_legacy_priceCache = false)
        // =====================================================================
        
        // Static cache for WriteHandles (per-thread, 15K ticks/sec performance)
        static std::unordered_map<uint32_t, 
            PriceCacheTypes::PriceCacheZeroCopy::WriteHandle> s_handleCache;
        
        // Get or create WriteHandle
        auto it = s_handleCache.find(token);
        if (it == s_handleCache.end()) {
            auto handle = PriceCacheTypes::PriceCacheZeroCopy::getInstance()
                .getWriteHandle(token, PriceCacheTypes::MarketSegment::NSE_CM);
            
            if (!handle.valid) {
                // Token not in PriceCache (expected for non-subscribed tokens)
                return;
            }
            
            it = s_handleCache.emplace(token, handle).first;
        }
        
        auto& h = it->second;
        
        // Atomic write with sequence (torn-read prevention)
        uint64_t seq = h.sequencePtr->fetch_add(1, std::memory_order_relaxed);
        
        // DIRECT field writes (values already in paise - no /100 needed)
        // IMPORTANT: Protocol values are in PAISE, ConsolidatedMarketData expects PAISE
        
        // Core price fields
        h.dataPtr->lastTradedPrice = be32toh_func(msg->mboData.lastTradedPrice);
        h.dataPtr->openPrice = be32toh_func(msg->openPrice);
        h.dataPtr->highPrice = be32toh_func(msg->highPrice);
        h.dataPtr->lowPrice = be32toh_func(msg->lowPrice);
        h.dataPtr->closePrice = be32toh_func(msg->closingPrice);
        
        // Volume and trade data
        h.dataPtr->volumeTradedToday = be64toh_func(msg->mboData.volumeTradedToday);
        h.dataPtr->lastTradeQuantity = be32toh_func(msg->mboData.lastTradeQuantity);
        h.dataPtr->lastTradeTime = be32toh_func(msg->mboData.lastTradeTime);
        h.dataPtr->averageTradePrice = be32toh_func(msg->mboData.averageTradePrice);
        
        // Price change indicators
        h.dataPtr->netPriceChange = be32toh_func(msg->mboData.netPriceChangeFromClosingPrice);
        h.dataPtr->netChangeIndicator = msg->mboData.netChangeIndicator;
        
        // Status fields
        h.dataPtr->tradingStatus = be16toh_func(msg->mboData.tradingStatus);
        h.dataPtr->bookType = be16toh_func(msg->mboData.bookType);
        
        // Market depth aggregates
        h.dataPtr->totalBuyQuantity = be64toh_func(msg->totalBuyQuantity);
        h.dataPtr->totalSellQuantity = be64toh_func(msg->totalSellQuantity);
        
        // 5-level market depth (MBP Buffer: 0-4 = Bids, 5-9 = Asks)
        for (int i = 0; i < 5; i++) {
            // Bid levels
            h.dataPtr->bidPrice[i] = be32toh_func(msg->mbpBuffer[i].price);
            h.dataPtr->bidQuantity[i] = be64toh_func(msg->mbpBuffer[i].quantity);
            h.dataPtr->bidOrders[i] = be16toh_func(msg->mbpBuffer[i].numberOfOrders);
            
            // Ask levels
            h.dataPtr->askPrice[i] = be32toh_func(msg->mbpBuffer[i + 5].price);
            h.dataPtr->askQuantity[i] = be64toh_func(msg->mbpBuffer[i + 5].quantity);
            h.dataPtr->askOrders[i] = be16toh_func(msg->mbpBuffer[i + 5].numberOfOrders);
        }
        
        // Publish update (memory_order_release ensures all writes visible to readers)
        h.sequencePtr->store(seq + 1, std::memory_order_release);
    }
    
    // Note: MBO Data (Individual Orders) is in msg->mboData.mboBuffer
    // We currently don't have a callback for individual orders, but the data is parsed.
}

void parse_bcast_mbo_mbp(const MS_BCAST_MBO_MBP* msg) {
    parse_message_7200(msg);
}

} // namespace nsecm
