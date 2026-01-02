#include "nse_parsers.h"
#include "nse_market_data.h"
#include "nsefo_callback.h"
#include "protocol.h"
#include <chrono>
#include <iostream>

namespace nsefo {

void parse_message_7220(const MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE* msg) {
    parse_limit_price_protection(msg);
}

void parse_limit_price_protection(const MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE* msg) {
    if (!msg) {
        return;
    }
    
    auto& registry = MarketDataCallbackRegistry::instance();
    auto timestampParsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    uint32_t msgCount = be32toh_func(msg->data.msgCount);
    
    for (uint32_t i = 0; i < msgCount && i < 25; i++) {
        const auto& detail = msg->data.details[i];
        uint32_t token = be32toh_func(detail.tokenNumber);
        
        if (token == 0) continue;  // Skip empty slots
        
        CircuitLimitData data;
        data.token = token;
        
        // Convert circuit limit prices (already in price units, divide by 100 for decimals)
        uint32_t high = be32toh_func(detail.highExecBand);
        uint32_t low = be32toh_func(detail.lowExecBand);
        data.upperLimit = high / 100.0;
        data.lowerLimit = low / 100.0;
        
        // Latency tracking
        data.timestampRecv = 0;  // Would need to be passed from receiver
        data.timestampParsed = timestampParsed;
        
        // Dispatch to callback
        registry.dispatchCircuitLimit(data);
    }
}

} // namespace nsefo
