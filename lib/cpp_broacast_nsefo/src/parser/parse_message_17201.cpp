#include "nse_parsers.h"
#include "protocol.h"
#include "market_data_callback.h"
#include <iostream>

void parse_message_17201(const MS_ENHNCD_BCAST_INQ_RESP_2* msg) {
    uint16_t numRecords = be16toh_func(msg->noOfRecords);
    
    for (int i = 0; i < numRecords && i < 5; i++) {
        const auto& rec = msg->records[i];
        int32_t token = be32toh_func(rec.token);
        
        if (token > 0) {
            // Parse enhanced market watch data
            MarketWatchData mw;
            mw.token = token;
            mw.openInterest = (uint32_t)be64toh_func((uint64_t)rec.openInterest);
            
            // Parse 3 market levels (Normal, Stop Loss, Auction)
            for (int j = 0; j < 3; j++) {
                const auto& mkt = rec.mktWiseInfo[j];
                MarketWatchData::MarketLevel level;
                level.buyVolume = be32toh_func(mkt.buyVolume);
                level.buyPrice = be32toh_func(mkt.buyPrice) / 100.0;
                level.sellVolume = be32toh_func(mkt.sellVolume);
                level.sellPrice = be32toh_func(mkt.sellPrice) / 100.0;
                mw.levels.push_back(level);
            }
            
            // Dispatch market watch callback
            MarketDataCallbackRegistry::instance().dispatchMarketWatch(mw);
        }
    }
}

void parse_enhncd_market_watch(const MS_ENHNCD_BCAST_INQ_RESP_2* msg) {
    parse_message_17201(msg);
}
