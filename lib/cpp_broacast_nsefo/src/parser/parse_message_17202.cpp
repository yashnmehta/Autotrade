#include "nse_parsers.h"
#include "protocol.h"
#include "market_data_callback.h"
#include <iostream>

void parse_message_17202(const MS_ENHNCD_TICKER_TRADE_DATA* msg) {
    uint16_t numRecords = be16toh_func(msg->numberOfRecords);
    
    for (int i = 0; i < numRecords && i < 12; i++) {
        const auto& rec = msg->records[i];
        int32_t token = be32toh_func(rec.token);
        
        if (token > 0) {
            // Parse enhanced ticker data
            TickerData ticker;
            ticker.token = token;
            ticker.fillPrice = be32toh_func(rec.fillPrice) / 100.0;
            ticker.fillVolume = be32toh_func(rec.fillVolume);
            ticker.openInterest = (uint32_t)be64toh_func((uint64_t)rec.openInterest);
            ticker.dayHiOI = (uint32_t)be64toh_func((uint64_t)rec.dayHiOI);
            ticker.dayLoOI = (uint32_t)be64toh_func((uint64_t)rec.dayLoOI);
            ticker.marketType = be16toh_func(rec.marketType);
            
            // Dispatch ticker callback
            MarketDataCallbackRegistry::instance().dispatchTicker(ticker);
        }
    }
}

void parse_enhncd_ticker_trade_data(const MS_ENHNCD_TICKER_TRADE_DATA* msg) {
    parse_message_17202(msg);
}
