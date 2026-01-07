#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <chrono>

namespace nsecm {

void parse_message_7210(const MS_BCAST_CALL_AUCTION_ORD_CXL* msg) {
    // STEP 1: Capture timestamps for latency tracking
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // STEP 2: Parse call auction order cancellation data
    CallAuctionOrderCxlData cxlData;
    cxlData.timestampRecv = now;
    
    // Extract number of records (max 8 securities)
    cxlData.noOfRecords = be16toh_func(msg->noOfRecords);
    
    // Validate number of records
    if (cxlData.noOfRecords < 0 || cxlData.noOfRecords > 8) {
        std::cerr << "[parse_message_7210] Invalid number of records: " << cxlData.noOfRecords << std::endl;
        return;
    }
    
    // STEP 3: Parse each security's cancellation statistics
    for (int i = 0; i < cxlData.noOfRecords && i < 8; i++) {
        const INTERACTIVE_ORD_CXL_DETAILS& record = msg->records[i];
        
        cxlData.records[i].token = be32toh_func(record.token);
        cxlData.records[i].buyOrdCxlCount = be64toh_func(record.buyOrdCxlCount);
        cxlData.records[i].buyOrdCxlVol = be64toh_func(record.buyOrdCxlVol);
        cxlData.records[i].sellOrdCxlCount = be64toh_func(record.sellOrdCxlCount);
        cxlData.records[i].sellOrdCxlVol = be64toh_func(record.sellOrdCxlVol);
        
        // STEP 4: Log for debugging (optional - can be removed in production)
        // Uncomment for debugging:
        // std::cout << "[7210] Token: " << cxlData.records[i].token
        //           << " | Buy Cxl: " << cxlData.records[i].buyOrdCxlCount 
        //           << " orders (" << cxlData.records[i].buyOrdCxlVol << " qty)"
        //           << " | Sell Cxl: " << cxlData.records[i].sellOrdCxlCount
        //           << " orders (" << cxlData.records[i].sellOrdCxlVol << " qty)"
        //           << std::endl;
    }
    
    // STEP 5: Validate data (optional but recommended)
    if (cxlData.noOfRecords > 0) {
        // Check if first record has valid token
        if (cxlData.records[0].token == 0) {
            std::cerr << "[parse_message_7210] Warning: First record has token=0" << std::endl;
            // Don't return - still dispatch, let handler decide
        }
    }
    
    // STEP 6: Dispatch callback
    MarketDataCallbackRegistry::instance().dispatchCallAuctionOrderCxl(cxlData);
}

// Alias function
void parse_call_auction_order_cxl(const MS_BCAST_CALL_AUCTION_ORD_CXL* msg) {
    parse_message_7210(msg);
}

} // namespace nsecm
