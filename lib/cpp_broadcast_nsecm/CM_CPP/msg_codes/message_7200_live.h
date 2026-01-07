// NSE Capital Market Multicast UDP Receiver - Message 7200 Header
// 
// FOCUS: Only process message code 7200 (BCAST_MBO_MBP_UPDATE)
// OUTPUT: csv_output/message_7200_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Page 113-117
// Structure: BCAST_MBO_MBP_UPDATE (482 bytes)
// Contains: Order book depth with MBO (10 levels) + MBP (10 levels) + market data
//
// Converted from Go to C++

#ifndef MESSAGE_7200_LIVE_H
#define MESSAGE_7200_LIVE_H

#include <string>
#include <cstdint>

// =============================================================================
// MESSAGE 7200 STRUCTURE
// =============================================================================

// Message7200Data - BCAST_MBO_MBP_UPDATE (482 bytes)
// Protocol: NSE CM NNF Protocol v6.3, Page 113
struct Message7200Data {
    uint32_t token;
    uint16_t bookType;
    uint16_t tradingStatus;
    uint64_t volumeTradedToday;
    uint32_t lastTradedPrice;
    uint8_t netChangeIndicator;
    uint32_t netPriceChange;
    uint32_t lastTradeQuantity;
    uint32_t lastTradeTime;
    uint32_t averageTradePrice;
    uint64_t totalBuyQuantity;
    uint64_t totalSellQuantity;
    uint32_t closingPrice;
    uint32_t openPrice;
    uint32_t highPrice;
    uint32_t lowPrice;
    uint32_t bestBuyPrice;
    uint32_t bestSellPrice;
    uint64_t bestBuyQty;
    uint64_t bestSellQty;
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

bool runMessage7200Receiver(const std::string& multicastIP, int port);
void stopMessage7200Receiver();

#endif // MESSAGE_7200_LIVE_H
