#ifndef NSE_MARKET_DATA_H
#define NSE_MARKET_DATA_H

#include "nse_common.h"

// Ensure 1-byte alignment for all structures
#pragma pack(push, 1)

// ============================================================================
// BCAST_ONLY_MBP (7208) - Only Market By Price
// ============================================================================

// MBP_INFORMATION - 16 bytes
struct MBP_INFORMATION {
    int64_t quantity;                  // Offset 0 (LONG LONG)
    int32_t price;                     // Offset 8 (LONG)
    int16_t numberOfOrders;            // Offset 12 (SHORT)
    int16_t bbBuySellFlag;             // Offset 14 (SHORT)
};

// INTERACTIVE_ONLY_MBP_DATA - 262 bytes
struct INTERACTIVE_ONLY_MBP_DATA {
    uint32_t token;                        // Offset 0
    uint16_t bookType;                     // Offset 4
    uint16_t tradingStatus;                // Offset 6
    int64_t volumeTradedToday;             // Offset 8 (LONG LONG)
    int32_t lastTradedPrice;               // Offset 16 (LONG)
    char netChangeIndicator;               // Offset 20
    char filler;                           // Offset 21
    int32_t netPriceChangeFromClosingPrice;// Offset 22 (LONG)
    int32_t lastTradeQuantity;             // Offset 26 (LONG)
    int32_t lastTradeTime;                 // Offset 30 (LONG)
    int32_t averageTradePrice;             // Offset 34 (LONG)
    uint16_t auctionNumber;                // Offset 38
    uint16_t auctionStatus;                // Offset 40
    uint16_t initiatorType;                // Offset 42
    int32_t initiatorPrice;                // Offset 44
    int32_t initiatorQuantity;             // Offset 48
    int32_t auctionPrice;                  // Offset 52
    int32_t auctionQuantity;               // Offset 56
    MBP_INFORMATION recordBuffer[10];      // Offset 60 (160 bytes)
    uint16_t bbTotalBuyFlag;               // Offset 220
    uint16_t bbTotalSellFlag;              // Offset 222
    int64_t totalBuyQuantity;              // Offset 224 (LONG LONG)
    int64_t totalSellQuantity;             // Offset 232 (LONG LONG)
    ST_INDICATOR stIndicator;              // Offset 240 (2 bytes)
    int32_t closingPrice;                  // Offset 242
    int32_t openPrice;                     // Offset 246
    int32_t highPrice;                     // Offset 250
    int32_t lowPrice;                      // Offset 254
    int32_t indicativeClosePrice;          // Offset 258
};

// MS_BCAST_ONLY_MBP - 566 bytes
// Transaction Code: 7208
struct MS_BCAST_ONLY_MBP {
    BCAST_HEADER header;                       // Offset 0 (40 bytes)
    uint16_t noOfRecords;                      // Offset 40
    INTERACTIVE_ONLY_MBP_DATA data[2];         // Offset 42 (524 bytes)
};

// ============================================================================
// BCAST_MBO_MBP_UPDATE (7200) - Market By Order / Market By Price
// ============================================================================

// MBO_INFORMATION - 18 bytes
struct MBO_INFORMATION {
    int32_t traderId;                  // Offset 0 (LONG)
    int32_t quantity;                  // Offset 4 (LONG)
    int32_t price;                     // Offset 8 (LONG)
    uint16_t terms;                    // Offset 12 (ST MBO MBP TERMS)
    int32_t minFillQty;                // Offset 14 (LONG)
};

// INTERACTIVE_MBO_DATA - 240 bytes
struct INTERACTIVE_MBO_DATA {
    uint32_t token;                        // Offset 0
    uint16_t bookType;                     // Offset 4
    uint16_t tradingStatus;                // Offset 6
    int64_t volumeTradedToday;             // Offset 8 (LONG LONG)
    int32_t lastTradedPrice;               // Offset 16 (LONG)
    char netChangeIndicator;               // Offset 20
    char filler;                           // Offset 21
    int32_t netPriceChangeFromClosingPrice;// Offset 22 (LONG)
    int32_t lastTradeQuantity;             // Offset 26 (LONG)
    int32_t lastTradeTime;                 // Offset 30 (LONG)
    int32_t averageTradePrice;             // Offset 34 (LONG)
    uint16_t auctionNumber;                // Offset 38
    uint16_t auctionStatus;                // Offset 40
    uint16_t initiatorType;                // Offset 42
    int32_t initiatorPrice;                // Offset 44
    int32_t initiatorQuantity;             // Offset 48
    int32_t auctionPrice;                  // Offset 52
    int32_t auctionQuantity;               // Offset 56
    MBO_INFORMATION mboBuffer[10];         // Offset 60 (180 bytes)
};

// MS_BCAST_MBO_MBP - 482 bytes
// Transaction Code: 7200
struct MS_BCAST_MBO_MBP {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    INTERACTIVE_MBO_DATA mboData;          // Offset 40 (240 bytes)
    MBP_INFORMATION mbpBuffer[10];         // Offset 280 (160 bytes)
    uint16_t bbTotalBuyFlag;               // Offset 440
    uint16_t bbTotalSellFlag;              // Offset 442
    int64_t totalBuyQuantity;              // Offset 444 (LONG LONG)
    int64_t totalSellQuantity;             // Offset 452 (LONG LONG)
    ST_INDICATOR stIndicator;              // Offset 460 (2 bytes)
    int32_t closingPrice;                  // Offset 462
    int32_t openPrice;                     // Offset 466
    int32_t highPrice;                     // Offset 470
    int32_t lowPrice;                      // Offset 474
    uint8_t reserved[4];                   // Offset 478
};

// ============================================================================
// BCAST_TICKER_AND_MKT_INDEX (18703) - Ticker and Market Index
// ============================================================================

// ST_TICKER_INDEX_INFO - 18 bytes
struct ST_TICKER_INDEX_INFO {
    uint32_t token;                    // Offset 0
    uint16_t marketType;               // Offset 4
    int32_t fillPrice;                 // Offset 6
    int32_t fillVolume;                // Offset 10
    int32_t marketIndexValue;          // Offset 14
};

// MS_TICKER_TRADE_DATA - 546 bytes
// Transaction Code: 18703
struct MS_TICKER_TRADE_DATA {
    BCAST_HEADER header;               // Offset 0 (40 bytes)
    uint16_t numberOfRecords;          // Offset 40
    ST_TICKER_INDEX_INFO records[28];  // Offset 42 (504 bytes)
};

// ============================================================================
// BCAST_MW_ROUND_ROBIN (7201) - Market Watch Round Robin
// ============================================================================

// ST_MKT_WISE_INFO - 34 bytes
struct ST_MKT_WISE_INFO {
    ST_INDICATOR stIndicator;          // Offset 0
    int64_t buyVolume;                 // Offset 2 (LONG LONG)
    int32_t buyPrice;                  // Offset 10 (LONG)
    int64_t sellVolume;                // Offset 14 (LONG LONG)
    int32_t sellPrice;                 // Offset 22 (LONG)
    int32_t lastTradePrice;            // Offset 26 (LONG)
    int32_t lastTradeTime;             // Offset 30 (LONG)
};

// ST_MARKET_WATCH_BCAST - 106 bytes
struct ST_MARKET_WATCH_BCAST {
    uint32_t token;                    // Offset 0
    ST_MKT_WISE_INFO mktWiseInfo[3];   // Offset 4 (102 bytes)
};

// MS_BCAST_INQ_RESP_2 - 466 bytes
// Transaction Code: 7201
struct MS_BCAST_INQ_RESP_2 {
    BCAST_HEADER header;               // Offset 0 (40 bytes)
    uint16_t noOfRecords;              // Offset 40
    ST_MARKET_WATCH_BCAST records[4];  // Offset 42 (424 bytes)
};

// ============================================================================
// BCAST_BUY_BACK (18708) - Buyback Information
// ============================================================================

// BUYBACKDATA - 64 bytes (per security)
// Transaction Code: 18708 (BCAST_BUY_BACK)
// Protocol Reference: NSE CM NNF Protocol v6.3, Table 46.1, Page 146
struct BUYBACKDATA {
    int32_t token;                     // Offset 0 (4 bytes) - Security token
    char symbol[10];                   // Offset 4 (10 bytes) - Security symbol
    char series[2];                    // Offset 14 (2 bytes) - Series
    double pdayCumVol;                 // Offset 16 (8 bytes) - Previous day cumulative volume
    int32_t pdayHighPrice;             // Offset 24 (4 bytes) - Previous day high price (paise)
    int32_t pdayLowPrice;              // Offset 28 (4 bytes) - Previous day low price (paise)
    int32_t pdayWtAvg;                 // Offset 32 (4 bytes) - Previous day weighted avg (paise)
    double cdayCumVol;                 // Offset 36 (8 bytes) - Current day cumulative volume
    int32_t cdayHighPrice;             // Offset 44 (4 bytes) - Current day high price (paise)
    int32_t cdayLowPrice;              // Offset 48 (4 bytes) - Current day low price (paise)
    int32_t cdayWtAvg;                 // Offset 52 (4 bytes) - Current day weighted avg (paise)
    int32_t startDate;                 // Offset 56 (4 bytes) - Buyback start date
    int32_t endDate;                   // Offset 60 (4 bytes) - Buyback end date
};
// Total: 4+10+2+8+4+4+4+8+4+4+4+4+4 = 64 bytes ✅

// MS_BCAST_BUY_BACK - 426 bytes
// Transaction Code: 18708
// Contains: Buyback information for up to 6 securities
// Broadcasted every hour from market open till close
struct MS_BCAST_BUY_BACK {
    BCAST_HEADER header;               // Offset 0 (40 bytes)
    int16_t numberOfRecords;           // Offset 40 (2 bytes) - Number of securities (max 6)
    BUYBACKDATA buyBackData[6];        // Offset 42 (384 bytes) - Array of 6 records
};
// Total: 40 + 2 + 384 = 426 bytes ✅

// ============================================================================
// BCAST_TURNOVER_EXCEEDED (9010) - Turnover Limit Exceeded Alert
// ============================================================================

// Protocol Reference: NSE CM NNF Protocol v6.3, Table 32, Page 102-104
// Structure: BROADCAST_LIMIT_EXCEEDED
// Total Size: 77 bytes
// Purpose: Alerts when broker turnover limit is about to exceed or has exceeded
// Frequency: Sent when threshold is reached
struct MS_BCAST_TURNOVER_EXCEEDED {
    BCAST_HEADER header;           // Offset 0 (40 bytes) - Broadcast header
    char brokerCode[5];            // Offset 40 (5 bytes) - Broker who exceeded limit
    char counterBrokerCode[5];     // Offset 45 (5 bytes) - Not in use
    int16_t warningType;           // Offset 50 (2 bytes) - 1=About to exceed, 2=Exceeded
    SEC_INFO secInfo;              // Offset 52 (12 bytes) - Symbol and series
    int32_t tradeNumber;           // Offset 64 (4 bytes) - Last trade number
    int32_t tradePrice;            // Offset 68 (4 bytes) - Last trade price (in paise)
    int32_t tradeVolume;           // Offset 72 (4 bytes) - Last trade quantity
    char final;                    // Offset 76 (1 byte) - Final auction trade indicator
};
// Total: 40 + 5 + 5 + 2 + 12 + 4 + 4 + 4 + 1 = 77 bytes ✅

#pragma pack(pop)

#endif // NSE_MARKET_DATA_H
