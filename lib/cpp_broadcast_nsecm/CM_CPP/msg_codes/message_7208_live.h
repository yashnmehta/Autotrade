// NSE Capital Market Multicast UDP Receiver - Message 7208 Header
// 
// FOCUS: Only process message code 7208 (BCAST_ONLY_MBP - Market By Price Only)
// OUTPUT: csv_output/message_7208_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Pages 118-123
// Structure: BROADCAST ONLY MBP (262 bytes per record)
// Contains: Market By Price data (10 levels without order count)
//
// Converted from Go to C++

#ifndef MESSAGE_7208_LIVE_H
#define MESSAGE_7208_LIVE_H

#include <string>
#include <cstdint>

// =============================================================================
// MESSAGE 7208 STRUCTURE
// =============================================================================

// MBPInfo - Market By Price information (16 bytes)
struct MBPInfo {
    int64_t quantity;        // 8 bytes - Order quantity
    uint32_t price;          // 4 bytes - Price (in paise)
    uint16_t numberOfOrders; // 2 bytes - Number of orders
    uint16_t bbBuySellFlag;  // 2 bytes - Buy/Sell flag (0=Buy, 1=Sell)
};

// Message7208Data - BROADCAST ONLY MBP (262 bytes per record)
// Protocol: NSE CM NNF Protocol v6.3, Pages 118-123
struct Message7208Data {
    uint32_t token;                          // 4 bytes - Security token
    uint16_t bookType;                       // 2 bytes - Book type
    uint16_t tradingStatus;                  // 2 bytes - Trading status
    int64_t volumeTradedToday;               // 8 bytes - Total volume traded
    uint32_t lastTradedPrice;                // 4 bytes - Last traded price (paise)
    uint8_t netChangeIndicator;              // 1 byte - '+', '-', or ' '
    uint8_t reserved1;                       // 1 byte - Reserved/padding
    uint32_t netPriceChangeFromClosingPrice; // 4 bytes - Net price change (paise)
    uint32_t lastTradeQuantity;              // 4 bytes - Last trade quantity
    uint32_t lastTradeTime;                  // 4 bytes - Last trade time
    uint32_t averageTradePrice;              // 4 bytes - Average trade price (paise)
    uint16_t auctionNumber;                  // 2 bytes - Auction number
    uint16_t auctionStatus;                  // 2 bytes - Auction status
    uint16_t initiatorType;                  // 2 bytes - Initiator type
    uint32_t initiatorPrice;                 // 4 bytes - Initiator price (paise)
    uint32_t initiatorQuantity;              // 4 bytes - Initiator quantity
    uint32_t auctionPrice;                   // 4 bytes - Auction price (paise)
    uint32_t auctionQuantity;                // 4 bytes - Auction quantity
    MBPInfo mbpData[10];                     // 160 bytes - 10 price levels (16 bytes each)
    uint16_t bbTotalBuyFlag;                 // 2 bytes - Total buy flag
    uint16_t bbTotalSellFlag;                // 2 bytes - Total sell flag
    int64_t totalBuyQuantity;                // 8 bytes - Total buy quantity
    int64_t totalSellQuantity;               // 8 bytes - Total sell quantity
    uint8_t reserved2[2];                    // 2 bytes - Reserved/padding
    uint32_t closingPrice;                   // 4 bytes - Closing price (paise)
    uint32_t openPrice;                      // 4 bytes - Opening price (paise)
    uint32_t highPrice;                      // 4 bytes - High price (paise)
    uint32_t lowPrice;                       // 4 bytes - Low price (paise)
    uint32_t indicativeClosePrice;           // 4 bytes - Indicative close price (paise)
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

bool runMessage7208Receiver(const std::string& multicastIP, int port);
void stopMessage7208Receiver();

#endif // MESSAGE_7208_LIVE_H
