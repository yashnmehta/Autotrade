#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <chrono>

namespace nsecm {

void parse_message_7206(const MS_BCAST_SYSTEM_INFORMATION* msg) {
    // STEP 1: Capture timestamps for latency tracking
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // STEP 2: Parse system information data
    SystemInformationData sysInfo;
    sysInfo.timestampRecv = now;
    
    // Market status fields (endianness conversion)
    sysInfo.normalMarketStatus = be16toh_func(msg->normal);
    sysInfo.oddlotMarketStatus = be16toh_func(msg->oddlot);
    sysInfo.spotMarketStatus = be16toh_func(msg->spot);
    sysInfo.auctionMarketStatus = be16toh_func(msg->auction);
    sysInfo.callAuction1Status = be16toh_func(msg->callAuction1);
    sysInfo.callAuction2Status = be16toh_func(msg->callAuction2);
    
    // Market parameters
    sysInfo.marketIndex = be32toh_func(msg->marketIndex);
    sysInfo.defaultSettlementPeriodNormal = be16toh_func(msg->defaultSettlementPeriodNormal);
    sysInfo.defaultSettlementPeriodSpot = be16toh_func(msg->defaultSettlementPeriodSpot);
    sysInfo.defaultSettlementPeriodAuction = be16toh_func(msg->defaultSettlementPeriodAuction);
    sysInfo.competitorPeriod = be16toh_func(msg->competitorPeriod);
    sysInfo.solicitorPeriod = be16toh_func(msg->solicitorPeriod);
    
    // Risk parameters
    sysInfo.warningPercent = be16toh_func(msg->warningPercent);
    sysInfo.volumeFreezePercent = be16toh_func(msg->volumeFreezePercent);
    sysInfo.terminalIdleTime = be16toh_func(msg->terminalIdleTime);
    
    // Trading parameters
    sysInfo.boardLotQuantity = be32toh_func(msg->boardLotQuantity);
    sysInfo.tickSize = be32toh_func(msg->tickSize);
    sysInfo.maximumGtcDays = be16toh_func(msg->maximumGtcDays);
    sysInfo.disclosedQuantityPercentAllowed = be16toh_func(msg->disclosedQuantityPercentAllowed);
    
    // Parse security eligible indicators bit flags
    uint16_t indicators = be16toh_func(msg->securityEligibleIndicators);
    
    // For Big Endian machines (as per protocol doc):
    // Bit 0 (LSB): AON
    // Bit 1: Minimum Fill
    // Bit 2: Books Merged
    // Bits 3-7: Reserved
    sysInfo.aonAllowed = (indicators & 0x01) != 0;
    sysInfo.minimumFillAllowed = (indicators & 0x02) != 0;
    sysInfo.booksMerged = (indicators & 0x04) != 0;
    
    // STEP 3: Log for debugging (optional - can be removed in production)
    // Uncomment for debugging:
    // std::cout << "[7206] Market Index: " << sysInfo.marketIndex 
    //           << " | Normal Status: " << sysInfo.normalMarketStatus
    //           << " | Tick Size: " << sysInfo.tickSize
    //           << " | Books Merged: " << (sysInfo.booksMerged ? "Yes" : "No")
    //           << std::endl;
    
    // STEP 4: Validate data (optional but recommended)
    if (sysInfo.marketIndex < 0) {
        std::cerr << "[parse_message_7206] Invalid market index: " << sysInfo.marketIndex << std::endl;
        // Don't return - still dispatch, let handler decide
    }
    
    if (sysInfo.tickSize <= 0) {
        std::cerr << "[parse_message_7206] Invalid tick size: " << sysInfo.tickSize << std::endl;
    }
    
    // STEP 5: Dispatch callback
    MarketDataCallbackRegistry::instance().dispatchSystemInformation(sysInfo);
}

// Alias function
void parse_system_information(const MS_BCAST_SYSTEM_INFORMATION* msg) {
    parse_message_7206(msg);
}

} // namespace nsecm
