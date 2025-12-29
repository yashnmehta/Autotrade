#include "nse_parsers.h"
#include "protocol.h"
#include "market_data_callback.h"
#include <iostream>
#include <chrono>

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
            
            // Parse ticker data
            TickerData ticker;
            ticker.token = token;
            ticker.refNo = refNo;
            ticker.timestampRecv = now;
            ticker.timestampParsed = now;
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
