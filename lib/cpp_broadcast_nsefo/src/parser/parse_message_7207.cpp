#include "nse_parsers.h"
#include "nse_index_messages.h"
#include "nsefo_callback.h"
#include <chrono>
#include <cstring>

namespace nsefo {

void parse_message_7207(const MS_BCAST_INDICES* msg) {
    parse_bcast_indices(msg);
}

void parse_bcast_indices(const MS_BCAST_INDICES* msg) {
    if (!msg) {
        return;
    }
    
    auto& registry = MarketDataCallbackRegistry::instance();
    auto timestampParsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    uint16_t numRecords = msg->numberOfRecords;
    if (numRecords > 6) {
        numRecords = 6;  // Clamp to max array size
    }
    
    for (uint16_t i = 0; i < numRecords; ++i) {
        const MS_INDICES& idx = msg->indices[i];
        
        // Convert to callback structure
        IndexData data;
        
        // Copy index name (ensure null-termination)
        std::memcpy(data.name, idx.indexName, 21);
        data.name[20] = '\0';  // Force null termination
        
        // Convert all uint32_t values to double (divide by 100 for 2 decimal places)
        // NSE sends index values multiplied by 100 (e.g., 19500.50 sent as 1950050)
        data.value = idx.indexValue / 100.0;
        data.high = idx.highIndexValue / 100.0;
        data.low = idx.lowIndexValue / 100.0;
        data.open = idx.openingIndex / 100.0;
        data.close = idx.closingIndex / 100.0;
        data.percentChange = idx.percentChange / 100.0;
        data.yearlyHigh = idx.yearlyHigh / 100.0;
        data.yearlyLow = idx.yearlyLow / 100.0;
        
        // Copy integer fields directly
        data.noOfUpmoves = idx.noOfUpmoves;
        data.noOfDownmoves = idx.noOfDownmoves;
        
        // Market capitalization is already a double
        data.marketCap = idx.marketCapitalisation;
        
        // Copy net change indicator
        data.netChangeIndicator = idx.netChangeIndicator;
        
        // Latency tracking
        data.timestampRecv = 0;  // Would need to be passed from receiver
        data.timestampParsed = timestampParsed;
        
        // Dispatch to callback
        registry.dispatchIndex(data);
    }
}

} // namespace nsefo
