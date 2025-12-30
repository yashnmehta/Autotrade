#include "nse_parsers.h"
#include "protocol.h"
#include "nsefo_callback.h"
#include <iostream>

namespace nsefo {


void parse_message_17202(const MS_ENHNCD_TICKER_TRADE_DATA* msg) {
    uint16_t numRecords = be16toh_func(msg->numberOfRecords);

    

    
    for (int i = 0; i < numRecords && i < 12; i++) {
        const auto& rec = msg->records[i];
        uint32_t token = be32toh_func(rec.token);
        
        if (token > 0) {
            // Parse enhanced ticker data
            TickerData ticker;
            ticker.token = token;
            ticker.fillPrice = be32toh_func(rec.fillPrice) / 100.0;
            ticker.fillVolume = be32toh_func(rec.fillVolume);
            ticker.openInterest = be64toh_func((uint64_t)rec.openInterest);
            ticker.dayHiOI = be64toh_func((uint64_t)rec.dayHiOI);
            ticker.dayLoOI = be64toh_func((uint64_t)rec.dayLoOI);
            ticker.marketType = be16toh_func(rec.marketType);
            
            // Dispatch ticker callback
            MarketDataCallbackRegistry::instance().dispatchTicker(ticker);
            //add debug logging for token 49543
            // std::cout << "[17202-ENHNCD-TICKER]  Open Interest: " << ticker.openInterest << std::endl;
        }
    }
}




void parse_enhncd_ticker_trade_data(const MS_ENHNCD_TICKER_TRADE_DATA* msg) {
    parse_message_17202(msg);
}

} // namespace nsefo
