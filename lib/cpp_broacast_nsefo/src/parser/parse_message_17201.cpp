#include "nse_parsers.h"
#include "protocol.h"
#include "market_data_callback.h"
#include <iostream>

void parse_message_17201(const MS_ENHNCD_BCAST_INQ_RESP_2* msg) {
    uint16_t numRecords = be16toh_func(msg->noOfRecords);
    
    std::cout << "\n[17201 PARSER] Called with " << numRecords << " records" << std::endl;
    
    for (int i = 0; i < numRecords && i < 5; i++) {
        const auto& rec = msg->records[i];
        uint32_t token = be32toh_func(rec.token);

        if (token > 0) {
            // Parse enhanced market watch data
            MarketWatchData mw;
            mw.token = token;
            mw.openInterest = be64toh_func((uint64_t)rec.openInterest);
            
            // Parse 3 market levels (Normal, Stop Loss, Auction)
            for (int j = 0; j < 3; j++) {
                const auto& mkt = rec.mktWiseInfo[j];
                mw.levels[j].buyVolume = be32toh_func(mkt.buyVolume);
                mw.levels[j].buyPrice = be32toh_func(mkt.buyPrice) / 100.0;
                mw.levels[j].sellVolume = be32toh_func(mkt.sellVolume);
                mw.levels[j].sellPrice = be32toh_func(mkt.sellPrice) / 100.0;
            }
            
            // Dispatch market watch callback
            MarketDataCallbackRegistry::instance().dispatchMarketWatch(mw);
        }
    }
}

void parse_enhncd_market_watch(const MS_ENHNCD_BCAST_INQ_RESP_2* msg) {
    parse_message_17201(msg);
}
