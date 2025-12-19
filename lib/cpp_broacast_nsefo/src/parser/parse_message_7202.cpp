#include "nse_parsers.h"
#include "protocol.h"
#include "market_data_callback.h"
#include <iostream>

void parse_message_7202(const MS_TICKER_TRADE_DATA* msg) {
    uint16_t numRecords = be16toh_func(msg->numberOfRecords);
    
    for (int i = 0; i < numRecords && i < 17; i++) {
        const auto& rec = msg->records[i];
        int32_t token = be32toh_func(rec.token);
        
        if (token > 0) {
            // Parse ticker data
            TickerData ticker;
            ticker.token = token;
            ticker.fillPrice = be32toh_func(rec.fillPrice) / 100.0;
            ticker.fillVolume = be32toh_func(rec.fillVolume);
            ticker.openInterest = be32toh_func(rec.openInterest);
            ticker.dayHiOI = be32toh_func(rec.dayHiOI);
            ticker.dayLoOI = be32toh_func(rec.dayLoOI);
            ticker.marketType = be16toh_func(rec.marketType);
            
            // Dispatch ticker callback
            MarketDataCallbackRegistry::instance().dispatchTicker(ticker);
        }
    }
}

void parse_ticker_trade_data(const MS_TICKER_TRADE_DATA* msg) {
    parse_message_7202(msg);
}
