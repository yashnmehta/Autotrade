// ===========================================================================
// NSE Capital Market - Message Parser for Transaction Code 9011
// ===========================================================================
// Message: BROADCAST_BROKER_REACTIVATED
// Purpose: Notifies when a previously deactivated broker is reactivated
// Protocol: NSE CM NNF Protocol v6.3, Table 32, Page 102-104
// Structure: MS_BCAST_TURNOVER_EXCEEDED (77 bytes) - Same as 9010
// Type: Broadcast (B) - Uncompressed
// ===========================================================================
// NOTE: This message uses the SAME structure as message 9010
//       (BCAST_TURNOVER_EXCEEDED) but only the BrokerCode field is relevant.
//       Other fields (warningType, symbol, series, trade details) are not used.
// ===========================================================================

#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include <algorithm>

namespace nsecm {

void parse_message_9011(const MS_BCAST_TURNOVER_EXCEEDED* msg) {
    // ========================================================================
    // STEP 1: Extract Basic Identification
    // ========================================================================
    
    // Capture timestamp for latency tracking (microseconds)
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Extract broker code (5 bytes, null-terminated)
    // Per protocol: "If the transaction code is BROADCAST_BROKER_REACTIVATED,
    // then this broker is reactivated."
    std::string brokerCode;
    brokerCode.reserve(6);
    for (int i = 0; i < 5; i++) {
        if (msg->brokerCode[i] != '\0') {
            brokerCode += msg->brokerCode[i];
        }
    }
    // Trim trailing spaces
    brokerCode.erase(std::find_if(brokerCode.rbegin(), brokerCode.rend(), 
        [](unsigned char ch) { return !std::isspace(ch); }).base(), brokerCode.end());
    
    // ========================================================================
    // STEP 2: Format Message
    // ========================================================================
    
    // Per protocol: For message 9011, only BrokerCode field is relevant
    // Other fields (warningType, symbol, series, tradeNumber, tradePrice, 
    // tradeVolume, final) are NOT in use for broker reactivation
    
    // Create formatted message string
    char messageBuffer[256];
    std::snprintf(messageBuffer, sizeof(messageBuffer),
        "Broker: %s | STATUS: Broker reactivated and can resume trading",
        brokerCode.c_str()
    );
    
    // ========================================================================
    // STEP 3: Dispatch via Admin Callback
    // ========================================================================
    
    // Create AdminMessage structure for callback
    AdminMessage adminMsg;
    adminMsg.token = 0;  // No specific token for broker reactivation
    adminMsg.timestamp = static_cast<uint32_t>(now / 1000000);  // Convert to seconds
    adminMsg.message = std::string(messageBuffer);
    adminMsg.actionCode = "REACTIVATE";  // Action code for broker reactivation
    
    // Dispatch to registered admin callback
    MarketDataCallbackRegistry::instance().dispatchAdmin(adminMsg);
    
    // ========================================================================
    // DEBUG OUTPUT (Optional - Uncomment for testing)
    // ========================================================================
    
    // std::cout << "[9011] Broker Reactivated: " << adminMsg.message << std::endl;
}

// Alias function for alternate naming convention
void parse_broker_reactivated(const MS_BCAST_TURNOVER_EXCEEDED* msg) {
    parse_message_9011(msg);
}

} // namespace nsecm
