#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <chrono>
#include <cstring>

namespace nsecm {

void parse_message_6013(const MS_SEC_OPEN_MSGS* msg) {
    // ============================================================================
    // Message 6013: SECURITY_OPEN_PRICE
    // ============================================================================
    // Protocol Reference: NSE CM NNF Protocol v6.3, Table 42, Page 137-138
    // Structure: MS_SEC_OPEN_MSGS (58 bytes)
    // Type: Broadcast (B)
    // Purpose: Broadcasts the opening price of a security when market opens
    // 
    // NOTE: Per protocol page 137:
    // "The Following transcode SECURITY_OPEN_PRICE (6013) will not be sent by exchange."
    // This message is documented but may not be actively broadcast by NSE.
    // Implemented for completeness and future compatibility.
    // ============================================================================
    
    // STEP 1: Extract security information
    char symbol[11] = {0};
    std::memcpy(symbol, msg->secInfo.symbol, 10);
    symbol[10] = '\0';
    
    char series[3] = {0};
    std::memcpy(series, msg->secInfo.series, 2);
    series[2] = '\0';
    
    // STEP 2: Extract token (big-endian conversion)
    // Note: Protocol specifies SHORT (2 bytes) for token in this message
    int16_t token = be16toh_func(msg->token);
    
    // STEP 3: Extract opening price (big-endian conversion, paise to rupees)
    int32_t openingPricePaise = be32toh_func(msg->openingPrice);
    double openingPrice = openingPricePaise / 100.0;
    
    // STEP 4: Extract market type (big-endian conversion)
    int16_t marketType = be16toh_func(msg->marketType);
    
    // STEP 5: Capture timestamps for latency tracking
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // STEP 6: Create parsed data for callback
    // Using AdminMessage structure as this is a market status message
    AdminMessage adminMsg;
    adminMsg.token = static_cast<uint32_t>(token);
    adminMsg.timestamp = static_cast<uint32_t>(now / 1000000);  // Convert to seconds
    adminMsg.actionCode = "OPEN";  // Security opening
    
    // Create human-readable message
    std::string symbolStr(symbol);
    std::string seriesStr(series);
    
    // Trim trailing spaces
    size_t endPos = symbolStr.find_last_not_of(' ');
    if (endPos != std::string::npos) {
        symbolStr = symbolStr.substr(0, endPos + 1);
    }
    endPos = seriesStr.find_last_not_of(' ');
    if (endPos != std::string::npos) {
        seriesStr = seriesStr.substr(0, endPos + 1);
    }
    
    // Format message with security details
    char msgBuffer[256];
    std::snprintf(msgBuffer, sizeof(msgBuffer), 
                  "Security Opened: %s-%s (Token: %d, Market: %d) Opening Price: %.2f",
                  symbolStr.c_str(), seriesStr.c_str(), token, marketType, openingPrice);
    adminMsg.message = std::string(msgBuffer);
    
    // STEP 7: Log for debugging (optional - can be removed in production)
    // Uncomment for debugging:
    // std::cout << "[6013] " << adminMsg.message << std::endl;
    
    // STEP 8: Validate data
    if (openingPrice < 0) {
        std::cerr << "[parse_message_6013] Invalid opening price: " << openingPrice << std::endl;
        return;
    }
    
    // STEP 9: Dispatch callback
    // Note: Using Admin callback as this is a market status/system message
    // Alternatively, you could create a dedicated SecurityOpen callback type
    MarketDataCallbackRegistry::instance().dispatchAdmin(adminMsg);
}

// Alias function for convenience
void parse_security_open_price(const MS_SEC_OPEN_MSGS* msg) {
    parse_message_6013(msg);
}

} // namespace nsecm
