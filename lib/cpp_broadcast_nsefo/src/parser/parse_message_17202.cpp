#include "nse_parsers.h"
#include "protocol.h"
#include "nsefo_callback.h"
#include "nsefo_price_store.h"
#include <iostream>
#include <chrono>
#include <cstring>

namespace nsefo {


void parse_message_17202(const MS_ENHNCD_TICKER_TRADE_DATA* msg) {
    uint16_t numRecords = be16toh_func(msg->numberOfRecords);

    

    
    for (int i = 0; i < numRecords && i < 12; i++) {
        const auto& rec = msg->records[i];
        uint32_t token = be32toh_func(rec.token);
        
        if (token > 0) {
            // Unified Token State
            UnifiedTokenState state;
            std::memset(&state, 0, sizeof(state));
            
            state.token = token;
            // No timestampRecv in current parser provided?? 
            // 7200 capture "now". 17202 doesn't capture time?
            // "auto now = ..." is MISSING in 17202 parser provided in view_file?
            // Wait, view_file output had NO time capture logic.
            // I should add it.
            
            auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            state.lastPacketTimestamp = now;

            state.ltp = be32toh_func(rec.fillPrice) / 100.0;
            state.lastTradeQty = be32toh_func(rec.fillVolume);
            state.openInterest = be64toh_func((uint64_t)rec.openInterest); // 17202 has 64-bit OI
            
            // Update Store
            g_nseFoPriceStore.updateTicker(state);
            
            // Dispatch ticker callback (Token only)
            MarketDataCallbackRegistry::instance().dispatchTicker(token);
        }
    }
}




void parse_enhncd_ticker_trade_data(const MS_ENHNCD_TICKER_TRADE_DATA* msg) {
    parse_message_17202(msg);
}

} // namespace nsefo
