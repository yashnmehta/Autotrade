// ===========================================================================
// NSE Capital Market - Message Parser for Transaction Code 9010
// ===========================================================================
// Message: BCAST_TURNOVER_EXCEEDED
// Purpose: Alerts when broker turnover limit is about to exceed or has exceeded
// Protocol: NSE CM NNF Protocol v6.3, Table 32, Page 102-104
// Structure: MS_BCAST_TURNOVER_EXCEEDED (77 bytes)
// Type: Broadcast (B) - Uncompressed
// ===========================================================================

#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include <algorithm>

namespace nsecm {

void parse_message_9010(const MS_BCAST_TURNOVER_EXCEEDED* msg) {
    // ========================================================================
    // STEP 1: Extract Basic Identification
    // ========================================================================
    
    // Capture timestamp for latency tracking (microseconds)
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Extract broker code (5 bytes, null-terminated)
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
    // STEP 2: Extract Warning Type and Trade Details
    // ========================================================================
    
    // Warning Type: 1 = About to exceed, 2 = Exceeded (broker deactivated)
    int16_t warningType = be16toh_func(msg->warningType);
    
    // Extract symbol (10 bytes, null-terminated)
    std::string symbol;
    symbol.reserve(11);
    for (int i = 0; i < 10; i++) {
        if (msg->secInfo.symbol[i] != '\0') {
            symbol += msg->secInfo.symbol[i];
        }
    }
    // Trim trailing spaces
    symbol.erase(std::find_if(symbol.rbegin(), symbol.rend(), 
        [](unsigned char ch) { return !std::isspace(ch); }).base(), symbol.end());
    
    // Extract series (2 bytes, null-terminated)
    std::string series;
    series.reserve(3);
    for (int i = 0; i < 2; i++) {
        if (msg->secInfo.series[i] != '\0') {
            series += msg->secInfo.series[i];
        }
    }
    
    // ========================================================================
    // STEP 3: Extract Trade Information
    // ========================================================================
    
    // Trade number (big-endian conversion)
    int32_t tradeNumber = be32toh_func(msg->tradeNumber);
    
    // Trade price in paise -> convert to rupees
    int32_t tradePricePaise = be32toh_func(msg->tradePrice);
    double tradePrice = tradePricePaise / 100.0;
    
    // Trade volume (big-endian conversion)
    int32_t tradeVolume = be32toh_func(msg->tradeVolume);
    
    // Final flag ('Y' or 'N' for final auction trade)
    char finalFlag = msg->final;
    
    // ========================================================================
    // STEP 4: Format Message and Dispatch
    // ========================================================================
    
    // Determine warning message
    std::string warningMessage;
    if (warningType == 1) {
        warningMessage = "WARNING: Turnover limit about to exceed";
    } else if (warningType == 2) {
        warningMessage = "ALERT: Turnover limit exceeded - Broker deactivated";
    } else {
        warningMessage = "Turnover limit notification";
    }
    
    // Create formatted message string
    char messageBuffer[512];
    std::snprintf(messageBuffer, sizeof(messageBuffer),
        "Broker: %s | %s | Last Trade: %s-%s @ %.2f (Qty: %d, Trade#: %d) | Final: %c",
        brokerCode.c_str(),
        warningMessage.c_str(),
        symbol.c_str(),
        series.c_str(),
        tradePrice,
        tradeVolume,
        tradeNumber,
        finalFlag
    );
    
    // ========================================================================
    // STEP 5: Dispatch via Admin Callback
    // ========================================================================
    
    // Create AdminMessage structure for callback
    AdminMessage adminMsg;
    adminMsg.token = 0;  // No specific token for broker alerts
    adminMsg.timestamp = static_cast<uint32_t>(now / 1000000);  // Convert to seconds
    adminMsg.message = std::string(messageBuffer);
    adminMsg.actionCode = "TURNOVER";  // Action code for turnover alerts
    
    // Dispatch to registered admin callback
    MarketDataCallbackRegistry::instance().dispatchAdmin(adminMsg);
    
    // ========================================================================
    // DEBUG OUTPUT (Optional - Uncomment for testing)
    // ========================================================================
    
    // std::cout << "[9010] Turnover Alert: " << adminMsg.message << std::endl;
}

// Alias function for alternate naming convention
void parse_turnover_exceeded(const MS_BCAST_TURNOVER_EXCEEDED* msg) {
    parse_message_9010(msg);
}

} // namespace nsecm
