#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <chrono>
#include <cstring>

namespace nsecm {

void parse_message_6501(const MS_BCAST_MESSAGE* msg) {
    // STEP 1: Extract basic identification
    int16_t branchNumber = be16toh_func(msg->branchNumber);
    
    // STEP 2: Capture timestamps for latency tracking
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // STEP 3: Parse message fields
    AdminMessage adminMsg;
    adminMsg.token = 0;  // Admin messages don't have a token
    adminMsg.timestamp = static_cast<uint32_t>(now / 1000000);  // Convert to seconds
    
    // Extract broker number (5 bytes) - ensure null-termination
    char brokerNumber[6] = {0};
    std::memcpy(brokerNumber, msg->brokerNumber, 5);
    brokerNumber[5] = '\0';
    
    // Extract action code (3 bytes) - ensure null-termination
    char actionCodeBuf[4] = {0};
    std::memcpy(actionCodeBuf, msg->actionCode, 3);
    actionCodeBuf[3] = '\0';
    adminMsg.actionCode = std::string(actionCodeBuf);
    
    // Trim trailing spaces from action code
    size_t endPos = adminMsg.actionCode.find_last_not_of(' ');
    if (endPos != std::string::npos) {
        adminMsg.actionCode = adminMsg.actionCode.substr(0, endPos + 1);
    }
    
    // Extract message length and content
    int16_t msgLength = be16toh_func(msg->broadcastMessageLength);
    
    // Validate message length
    if (msgLength < 0 || msgLength > 240) {
        std::cerr << "[parse_message_6501] Invalid message length: " << msgLength << std::endl;
        return;
    }
    
    // Extract broadcast message (up to msgLength from the 240 byte buffer)
    std::string message;
    for (int i = 0; i < msgLength && i < 240; i++) {
        char c = msg->broadcastMessage[i];
        if (c == '\0') break;  // Stop at null terminator
        message += c;
    }
    
    // Trim trailing spaces from message
    endPos = message.find_last_not_of(' ');
    if (endPos != std::string::npos) {
        message = message.substr(0, endPos + 1);
    }
    
    adminMsg.message = message;
    
    // STEP 4: Log for debugging (optional - can be removed in production)
    // Uncomment for debugging:
    // std::cout << "[6501] Branch: " << branchNumber 
    //           << " | Broker: " << brokerNumber 
    //           << " | Action: " << adminMsg.actionCode 
    //           << " | Msg: " << adminMsg.message << std::endl;
    
    // STEP 5: Validate data
    if (adminMsg.message.empty() && msgLength > 0) {
        std::cerr << "[parse_message_6501] Empty message despite length: " << msgLength << std::endl;
        return;
    }
    
    // STEP 6: Dispatch callback
    MarketDataCallbackRegistry::instance().dispatchAdmin(adminMsg);
}

// Alias function
void parse_jrnl_vct_msg(const MS_BCAST_MESSAGE* msg) {
    parse_message_6501(msg);
}

} // namespace nsecm
