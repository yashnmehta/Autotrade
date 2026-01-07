#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <chrono>
#include <cstring>

namespace nsecm {

void parse_message_18708(const MS_BCAST_BUY_BACK* msg) {
    // ============================================================================
    // Message 18708: BCAST_BUY_BACK
    // ============================================================================
    // Protocol Reference: NSE CM NNF Protocol v6.3, Table 46, Page 145-147
    // Structure: MS_BCAST_BUY_BACK (426 bytes)
    // Type: Broadcast (B) - Compressed
    // Purpose: Broadcasts buyback information for securities
    // Frequency: Every hour from market open till market close
    // Max Records: 6 securities per broadcast
    // ============================================================================
    
    // STEP 1: Extract number of records (big-endian conversion)
    int16_t numberOfRecords = be16toh_func(msg->numberOfRecords);
    
    // Validate number of records (max 6)
    if (numberOfRecords < 0 || numberOfRecords > 6) {
        std::cerr << "[parse_message_18708] Invalid numberOfRecords: " << numberOfRecords << std::endl;
        return;
    }
    
    // STEP 2: Capture timestamps for latency tracking
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // STEP 3: Process each buyback record
    for (int i = 0; i < numberOfRecords; i++) {
        const BUYBACKDATA& buyback = msg->buyBackData[i];
        
        // Extract token (big-endian conversion)
        uint32_t token = be32toh_func(static_cast<uint32_t>(buyback.token));
        
        // Extract symbol and series (null-terminated strings)
        char symbol[11] = {0};
        std::memcpy(symbol, buyback.symbol, 10);
        symbol[10] = '\0';
        
        char series[3] = {0};
        std::memcpy(series, buyback.series, 2);
        series[2] = '\0';
        
        // Trim trailing spaces
        std::string symbolStr(symbol);
        size_t endPos = symbolStr.find_last_not_of(' ');
        if (endPos != std::string::npos) {
            symbolStr = symbolStr.substr(0, endPos + 1);
        }
        
        std::string seriesStr(series);
        endPos = seriesStr.find_last_not_of(' ');
        if (endPos != std::string::npos) {
            seriesStr = seriesStr.substr(0, endPos + 1);
        }
        
        // Extract previous day data
        // Note: Protocol uses DOUBLE for volume (8 bytes, not endianness converted typically)
        double pdayCumVol = buyback.pdayCumVol;  // DOUBLE - may need endianness check
        double pdayHighPrice = be32toh_func(buyback.pdayHighPrice) / 100.0;  // Paise to Rupees
        double pdayLowPrice = be32toh_func(buyback.pdayLowPrice) / 100.0;
        double pdayWtAvg = be32toh_func(buyback.pdayWtAvg) / 100.0;
        
        // Extract current day data
        double cdayCumVol = buyback.cdayCumVol;  // DOUBLE - may need endianness check
        double cdayHighPrice = be32toh_func(buyback.cdayHighPrice) / 100.0;
        double cdayLowPrice = be32toh_func(buyback.cdayLowPrice) / 100.0;
        double cdayWtAvg = be32toh_func(buyback.cdayWtAvg) / 100.0;
        
        // Extract dates (big-endian conversion)
        uint32_t startDate = be32toh_func(static_cast<uint32_t>(buyback.startDate));
        uint32_t endDate = be32toh_func(static_cast<uint32_t>(buyback.endDate));
        
        // STEP 4: Create parsed data for callback
        // Using AdminMessage structure to broadcast buyback info
        AdminMessage adminMsg;
        adminMsg.token = token;
        adminMsg.timestamp = static_cast<uint32_t>(now / 1000000);  // Convert to seconds
        adminMsg.actionCode = "BUYBACK";
        
        // Create detailed message
        char msgBuffer[512];
        std::snprintf(msgBuffer, sizeof(msgBuffer),
                     "Buyback: %s-%s (Token: %u) | "
                     "PrevDay: Vol=%.0f H=%.2f L=%.2f Avg=%.2f | "
                     "CurrDay: Vol=%.0f H=%.2f L=%.2f Avg=%.2f | "
                     "Period: %u to %u",
                     symbolStr.c_str(), seriesStr.c_str(), token,
                     pdayCumVol, pdayHighPrice, pdayLowPrice, pdayWtAvg,
                     cdayCumVol, cdayHighPrice, cdayLowPrice, cdayWtAvg,
                     startDate, endDate);
        adminMsg.message = std::string(msgBuffer);
        
        // STEP 5: Log for debugging (optional - can be removed in production)
        // Uncomment for debugging:
        // std::cout << "[18708] Record " << (i+1) << "/" << numberOfRecords 
        //           << ": " << adminMsg.message << std::endl;
        
        // STEP 6: Dispatch callback
        // Note: Each buyback record is dispatched separately
        MarketDataCallbackRegistry::instance().dispatchAdmin(adminMsg);
    }
}

// Alias function for convenience
void parse_bcast_buy_back(const MS_BCAST_BUY_BACK* msg) {
    parse_message_18708(msg);
}

} // namespace nsecm
