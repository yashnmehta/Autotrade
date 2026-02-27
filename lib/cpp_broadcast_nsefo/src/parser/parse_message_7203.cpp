#include "nse_parsers.h"
#include "nse_index_messages.h"
#include "nsefo_callback.h"
#include <chrono>
#include <cstring>

namespace nsefo {

void parse_message_7203(const MS_BCAST_INDUSTRY_INDICES* msg) {
    parse_bcast_industry_indices(msg);
}

void parse_bcast_industry_indices(const MS_BCAST_INDUSTRY_INDICES* msg) {
    if (!msg) {
        return;
    }
    
    auto& registry = MarketDataCallbackRegistry::instance();
    auto timestampParsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    uint16_t numRecords = msg->noOfRecs;
    if (numRecords > 20) {
        numRecords = 20;  // Clamp to max array size
    }
    
    for (uint16_t i = 0; i < numRecords; ++i) {
        const INDUSTRY_INDICES& idx = msg->industryIndices[i];
        
        // Convert to callback structure
        IndustryIndexData data;
        
        // Copy industry name (ensure null-termination)
        std::memcpy(data.name, idx.industryName, 15);
        data.name[15] = '\0';  // Force null termination
        
        // Convert uint32_t value to double (divide by 100 for 2 decimal places)
        // NSE sends index values multiplied by 100
        data.value = idx.indexValue / 100.0;
        
        // Latency tracking
        data.timestampRecv = 0;  // Would need to be passed from receiver
        data.timestampParsed = timestampParsed;
        
        // Dispatch to callback
        registry.dispatchIndustryIndex(data);
    }
}

} // namespace nsefo
