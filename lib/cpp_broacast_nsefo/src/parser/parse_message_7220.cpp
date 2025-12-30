#include "nse_parsers.h"
#include "protocol.h"
#include <iostream>

namespace nsefo {


void parse_message_7220(const MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE* msg) {
    uint32_t msgCount = be32toh_func(msg->data.msgCount);
    // std::cout << "  [LIMIT_PRICE_PROTECTION] Count: " << msgCount << std::endl;
    
    for (uint32_t i = 0; i < msgCount && i < 25; i++) {
        const auto& detail = msg->data.details[i];
        uint32_t token = be32toh_func(detail.tokenNumber);
        if (token > 0) {
            uint32_t high = be32toh_func(detail.highExecBand);
            uint32_t low = be32toh_func(detail.lowExecBand);
            // std::cout << "    [" << i << "] Token: " << token
            //           << " | High: " << high
            //           << " | Low: " << low << std::endl;
        }
    }
}

void parse_limit_price_protection(const MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE* msg) {
    parse_message_7220(msg);
}

} // namespace nsefo
