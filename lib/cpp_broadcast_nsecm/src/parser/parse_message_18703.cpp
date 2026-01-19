#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include "../../include/nsecm_price_store.h"
#include <iostream>
#include <chrono>

namespace nsecm {

void parse_message_18703(const MS_TICKER_TRADE_DATA* msg) {
    if (!msg) return;
    
    uint16_t numRecords = be16toh_func(msg->numberOfRecords);
    
    for (int i = 0; i < numRecords && i < 28; i++) {
        const auto& rec = msg->records[i];
        uint32_t token = be32toh_func(rec.token);
        
        if (token == 0) continue;
        
        // Parse Fields
        double fillPrice = be32toh_func(rec.fillPrice) / 100.0;
        uint64_t fillVolume = be32toh_func(rec.fillVolume); // Assuming 32-bit but struct says int32_t.
                                                              // Wait, nse_market_data.h says int32_t fillVolume.
                                                              // be32toh_func returns uint32_t.
                                                              // UnifiedTokenState uses uint64_t.
        
        // Update Global Price Store
        g_nseCmPriceStore.updateTicker(token, fillPrice, (uint64_t)fillVolume);
        
        // Dispatch Callback (Signal only)
        MarketDataCallbackRegistry::instance().dispatchTicker(token);
    }
}

void parse_ticker_trade_data(const MS_TICKER_TRADE_DATA* msg) {
    parse_message_18703(msg);
}

} // namespace nsecm
