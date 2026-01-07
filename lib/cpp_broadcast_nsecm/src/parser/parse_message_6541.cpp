#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <chrono>

namespace nsecm {

void parse_message_6541(const MS_BC_CIRCUIT_CHECK* msg) {
    // STEP 1: Extract basic identification from header
    uint32_t logTime = be32toh_func(msg->header.logTime);
    uint16_t txCode = be16toh_func(msg->header.transactionCode);
    
    // STEP 2: Capture timestamps for latency tracking
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // STEP 3: Create circuit check / heartbeat message
    // This is a simple heartbeat/circuit check message with only header
    AdminMessage circuitCheck;
    circuitCheck.token = 0;  // Circuit check messages don't have a token
    circuitCheck.timestamp = logTime;
    circuitCheck.actionCode = "CCK";  // Circuit Check code
    circuitCheck.message = "Circuit Check - Heartbeat Pulse";
    
    // STEP 4: Log for debugging (optional - can be removed in production)
    // Uncomment for debugging:
    // std::cout << "[6541] Circuit Check received at: " << logTime << std::endl;
    
    // STEP 5: Dispatch callback
    // Circuit check messages use the admin callback for consistency
    MarketDataCallbackRegistry::instance().dispatchAdmin(circuitCheck);
}

} // namespace nsecm
