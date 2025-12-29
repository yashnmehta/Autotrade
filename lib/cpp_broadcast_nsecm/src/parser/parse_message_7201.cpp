#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/market_data_callback.h"
#include <iostream>

void parse_message_7201(const MS_BCAST_INQ_RESP_2* msg) {
    uint16_t numRecords = be16toh_func(msg->noOfRecords);
    
    for (int i = 0; i < numRecords && i < 4; i++) {
        const auto& rec = msg->records[i];
        uint32_t token = be32toh_func(rec.token);
        
        if (token > 0) {
            // Parse market watch data
            MarketWatchData mw;
            mw.token = token;
            mw.openInterest = 0; // Not in CM 7201
            
            // Parse 3 market levels (Normal, Stop Loss, Auction)
            for (int j = 0; j < 3; j++) {
                const auto& mkt = rec.mktWiseInfo[j];
                mw.levels[j].buyVolume = be64toh_func(mkt.buyVolume); // CM is LONG LONG
                mw.levels[j].buyPrice = be32toh_func(mkt.buyPrice) / 100.0;
                mw.levels[j].sellVolume = be64toh_func(mkt.sellVolume); // CM is LONG LONG
                mw.levels[j].sellPrice = be32toh_func(mkt.sellPrice) / 100.0;
                mw.levels[j].ltp = be32toh_func(mkt.lastTradePrice) / 100.0;
                mw.levels[j].lastTradeTime = be32toh_func(mkt.lastTradeTime);
            }
            
            // Dispatch market watch callback
            MarketDataCallbackRegistry::instance().dispatchMarketWatch(mw);
        }
    }
}

void parse_market_watch(const MS_BCAST_INQ_RESP_2* msg) {
    parse_message_7201(msg);
}
