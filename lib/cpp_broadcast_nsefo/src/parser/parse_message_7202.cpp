#include "nse_parsers.h"
#include "protocol.h"
#include "nsefo_callback.h"
#include "nsefo_price_store.h"
#include <iostream>
#include <chrono>

namespace nsefo {


void parse_message_7202(const MS_TICKER_TRADE_DATA* msg) {
    uint16_t numRecords = be16toh_func(msg->numberOfRecords);
    
    // Capture timestamps for latency tracking
    static uint64_t refNoCounter = 0;
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    for (int i = 0; i < numRecords && i < 17; i++) {
        const auto& rec = msg->records[i];
        uint32_t token = be32toh_func(rec.token);
        
        if (token > 0) {
            uint64_t refNo = ++refNoCounter;
            
            // Unified Token State
            UnifiedTokenState state;
            std::memset(&state, 0, sizeof(state)); 
            
            state.token = token;
            state.lastPacketTimestamp = now;
            
            state.ltp = be32toh_func(rec.fillPrice) / 100.0;
            state.lastTradeQty = be32toh_func(rec.fillVolume);
            state.openInterest = be32toh_func(rec.openInterest);
            // Note: dayHiOI/dayLoOI not stored in UnifiedTokenState currently as per PriceStore design
            
            // Update Store
            g_nseFoPriceStore.updateTicker(state);
            
            // Dispatch ticker callback (Token only)
            MarketDataCallbackRegistry::instance().dispatchTicker(token);
        }
    }
}

void parse_ticker_trade_data(const MS_TICKER_TRADE_DATA* msg) {
    parse_message_7202(msg);
}

} // namespace nsefo
