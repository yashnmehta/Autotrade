#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <chrono>
#include <cstring>

namespace nsecm {

void parse_message_5295(const MS_TRADER_INT_MSG* msg) {
    // ============================================================================
    // Message 5295: CTRL_MSG_TO_TRADER
    // ============================================================================
    // Protocol Reference: NSE CM NNF Protocol v6.3, Table 22, Page 78-79
    // Structure: MS_TRADER_INT_MSG (290 bytes)
    // Type: Interactive (sent to specific trader from NSE Control)
    // Purpose: Control messages from exchange to trader workstation
    // Action Codes: 'SYS' (System), 'AUI' (Auction Initiation), 
    //               'AUC' (Auction Complete), 'LIS' (Listing)
    // ============================================================================
    
    // STEP 1: Extract trader ID (big-endian conversion)
    int32_t traderId = be32toh_func(msg->traderId);
    
    // STEP 2: Capture timestamps for latency tracking
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // STEP 3: Parse message fields
    AdminMessage adminMsg;
    adminMsg.token = traderId;  // Use trader ID as token for identification
    adminMsg.timestamp = static_cast<uint32_t>(now / 1000000);  // Convert to seconds
    
    // STEP 4: Extract action code (3 bytes) - ensure null-termination
    char actionCodeBuf[4] = {0};
    std::memcpy(actionCodeBuf, msg->actionCode, 3);
    actionCodeBuf[3] = '\0';
    adminMsg.actionCode = std::string(actionCodeBuf);
    
    // Trim trailing spaces from action code
    size_t endPos = adminMsg.actionCode.find_last_not_of(' ');
    if (endPos != std::string::npos) {
        adminMsg.actionCode = adminMsg.actionCode.substr(0, endPos + 1);
    }
    
    // STEP 5: Extract message length (big-endian conversion)
    int16_t msgLength = be16toh_func(msg->msgLength);
    
    // Validate message length
    if (msgLength < 0 || msgLength > 240) {
        std::cerr << "[parse_message_5295] Invalid message length: " << msgLength << std::endl;
        return;
    }
    
    // STEP 6: Extract message content (up to msgLength from the 240 byte buffer)
    std::string message;
    for (int i = 0; i < msgLength && i < 240; i++) {
        char c = msg->msg[i];
        if (c == '\0') break;  // Stop at null terminator
        message += c;
    }
    
    // Trim trailing spaces from message
    endPos = message.find_last_not_of(' ');
    if (endPos != std::string::npos) {
        message = message.substr(0, endPos + 1);
    }
    
    adminMsg.message = message;
    
    // STEP 7: Log for debugging (optional - can be removed in production)
    // Uncomment for debugging:
    // std::cout << "[5295] TraderId: " << traderId 
    //           << " | Action: " << adminMsg.actionCode 
    //           << " | Msg: " << adminMsg.message << std::endl;
    
    // STEP 8: Validate data
    if (adminMsg.message.empty() && msgLength > 0) {
        std::cerr << "[parse_message_5295] Empty message despite length: " << msgLength << std::endl;
        return;
    }
    
    // STEP 9: Dispatch callback
    // Note: This is an INTERACTIVE message (5295), not a broadcast (6501)
    // Both use the same AdminMessage callback structure
    MarketDataCallbackRegistry::instance().dispatchAdmin(adminMsg);
}

// Alias function for convenience
void parse_ctrl_msg_to_trader(const MS_TRADER_INT_MSG* msg) {
    parse_message_5295(msg);
}

} // namespace nsecm
