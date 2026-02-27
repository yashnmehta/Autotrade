#ifndef NSE_MARKET_DATA_H
#define NSE_MARKET_DATA_H

#include "nse_common.h"

// Ensure 1-byte alignment for all structures
#pragma pack(push, 1)

// ============================================================================
// BCAST_MBO_MBP_UPDATE (7200) - Market By Order/Market By Price Update
// ============================================================================

// ST_MBO_INFO - 18 bytes
// Market By Order information
struct ST_MBO_INFO {
    uint32_t traderId;                 // Offset 0
    uint32_t qty;                      // Offset 4
    uint32_t price;                    // Offset 8
    ST_MBO_MBP_TERMS terms;            // Offset 12
    uint32_t minFillQty;               // Offset 14
};

// ST_MBP_INFO - 10 bytes
// Market By Price information
struct ST_MBP_INFO {
    uint32_t qty;                      // Offset 0
    uint32_t price;                    // Offset 4
    uint16_t noOfOrders;               // Offset 8
};

// ST_INTERACTIVE_MBO_DATA - 235 bytes
// Interactive Market By Order data
struct ST_INTERACTIVE_MBO_DATA {
    uint32_t token;                    // Offset 0
    uint16_t bookType;                 // Offset 4
    uint16_t tradingStatus;            // Offset 6
    uint32_t volumeTradedToday;        // Offset 8 (UNSIGNED LONG)
    uint32_t lastTradedPrice;          // Offset 12
    char netChangeIndicator;           // Offset 16
    uint32_t netPriceChangeFromClosingPrice; // Offset 17
    uint32_t lastTradeQuantity;        // Offset 21
    uint32_t lastTradeTime;            // Offset 25
    uint32_t averageTradePrice;        // Offset 29
    uint16_t auctionNumber;            // Offset 33
    uint16_t auctionStatus;            // Offset 35
    uint16_t initiatorType;            // Offset 37
    uint32_t initiatorPrice;           // Offset 39
    uint32_t initiatorQuantity;        // Offset 43
    uint32_t auctionPrice;             // Offset 47
    uint32_t auctionQuantity;          // Offset 51
    ST_MBO_INFO recordBuffer[10];      // Offset 55 (180 bytes)
};

// MS_BCAST_MBO_MBP - 410 bytes
// Transaction Code: 7200
struct MS_BCAST_MBO_MBP {
    BCAST_HEADER header;               // Offset 0 (40 bytes)
    ST_INTERACTIVE_MBO_DATA data;      // Offset 40 (235 bytes)
    ST_MBP_INFO recordBuffer[10];      // Offset 275 (100 bytes)
    double totalBuyQuantity;           // Offset 375
    double totalSellQuantity;          // Offset 383
    ST_INDICATOR stIndicator;          // Offset 391
    uint32_t closingPrice;             // Offset 393
    uint32_t openPrice;                // Offset 397
    uint32_t highPrice;                // Offset 401
    uint32_t lowPrice;                 // Offset 405
};

// ============================================================================
// BCAST_ONLY_MBP (7208) - Market By Price Only
// ============================================================================

// MBP_INFORMATION - 12 bytes
struct MBP_INFORMATION {
    uint32_t quantity;                 // Offset 0
    uint32_t price;                    // Offset 4
    uint16_t numberOfOrders;           // Offset 8
    uint16_t bbBuySellFlag;            // Offset 10
};

// INTERACTIVE_ONLY_MBP_DATA - 214 bytes
struct INTERACTIVE_ONLY_MBP_DATA {
    uint32_t token;                        // Offset 0
    uint16_t bookType;                     // Offset 4
    uint16_t tradingStatus;                // Offset 6
    uint32_t volumeTradedToday;            // Offset 8 (UNSIGNED LONG)
    uint32_t lastTradedPrice;              // Offset 12
    char netChangeIndicator;               // Offset 16
    char volTrdTodayExcdIndc;              // Offset 17
    uint32_t netPriceChangeFromClosingPrice; // Offset 18
    uint32_t lastTradeQuantity;            // Offset 22
    uint32_t lastTradeTime;                // Offset 26
    uint32_t averageTradePrice;            // Offset 30
    uint16_t auctionNumber;                // Offset 34
    uint16_t auctionStatus;                // Offset 36
    uint16_t initiatorType;                // Offset 38
    uint32_t initiatorPrice;               // Offset 40
    uint32_t initiatorQuantity;            // Offset 44
    uint32_t auctionPrice;                 // Offset 48
    uint32_t auctionQuantity;              // Offset 52
    MBP_INFORMATION recordBuffer[10];      // Offset 56 (120 bytes)
    uint16_t bbTotalBuyFlag;               // Offset 176
    uint16_t bbTotalSellFlag;              // Offset 178
    double totalBuyQuantity;               // Offset 180
    double totalSellQuantity;              // Offset 188
    ST_INDICATOR stIndicator;              // Offset 196
    uint32_t closingPrice;                 // Offset 198
    uint32_t openPrice;                    // Offset 202
    uint32_t highPrice;                    // Offset 206
    uint32_t lowPrice;                     // Offset 210
};

// MS_BCAST_ONLY_MBP - 470 bytes
// Transaction Code: 7208
struct MS_BCAST_ONLY_MBP {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t noOfRecords;                  // Offset 40
    INTERACTIVE_ONLY_MBP_DATA data[2];     // Offset 42 (428 bytes)
};

// ============================================================================
// BCAST_TICKER_AND_MKT_INDEX (7202) - Ticker and Market Index
// ============================================================================

// ST_TICKER_INDEX_INFO - 26 bytes
struct ST_TICKER_INDEX_INFO {
    uint32_t token;                    // Offset 0
    uint16_t marketType;               // Offset 4
    uint32_t fillPrice;                // Offset 6
    uint32_t fillVolume;               // Offset 10
    uint32_t openInterest;             // Offset 14 (UNSIGNED LONG)
    uint32_t dayHiOI;                  // Offset 18 (UNSIGNED LONG)
    uint32_t dayLoOI;                  // Offset 22 (UNSIGNED LONG)
};

// MS_TICKER_TRADE_DATA - 484 bytes
// Transaction Code: 7202
struct MS_TICKER_TRADE_DATA {
    BCAST_HEADER header;               // Offset 0 (40 bytes)
    uint16_t numberOfRecords;          // Offset 40
    ST_TICKER_INDEX_INFO records[17];  // Offset 42 (442 bytes)
};

// ============================================================================
// BCAST_ENHNCD_TICKER_AND_MKT_INDEX (17202) - Enhanced Ticker
// ============================================================================

// ST_ENHNCD_TICKER_INDEX_INFO - 38 bytes
struct ST_ENHNCD_TICKER_INDEX_INFO {
    uint32_t token;                    // Offset 0
    uint16_t marketType;               // Offset 4
    uint32_t fillPrice;                // Offset 6
    uint32_t fillVolume;               // Offset 10
    int64_t openInterest;              // Offset 14 (LONG LONG)
    int64_t dayHiOI;                   // Offset 22 (LONG LONG)
    int64_t dayLoOI;                   // Offset 30 (LONG LONG)
};

// MS_ENHNCD_TICKER_TRADE_DATA - 498 bytes
// Transaction Code: 17202
struct MS_ENHNCD_TICKER_TRADE_DATA {
    BCAST_HEADER header;                      // Offset 0 (40 bytes)
    uint16_t numberOfRecords;                 // Offset 40
    ST_ENHNCD_TICKER_INDEX_INFO records[12];  // Offset 42 (456 bytes)
};

// ============================================================================
// BCAST_MW_ROUND_ROBIN (7201) - Market Watch Round Robin
// ============================================================================

// ST_MKT_WISE_INFO - 26 bytes
struct ST_MKT_WISE_INFO {
    ST_INDICATOR stIndicator;          // Offset 0
    uint32_t buyVolume;                // Offset 2
    uint32_t buyPrice;                 // Offset 6
    uint32_t sellVolume;               // Offset 10
    uint32_t sellPrice;                // Offset 14
    uint8_t reserved[8];               // Offset 18 (padding to 26 bytes)
};

// ST_MARKET_WATCH_BCAST - 86 bytes
struct ST_MARKET_WATCH_BCAST {
    uint32_t token;                    // Offset 0
    ST_MKT_WISE_INFO mktWiseInfo[3];   // Offset 4 (78 bytes)
    uint32_t openInterest;             // Offset 82 (UNSIGNED LONG)
};

// MS_BCAST_INQ_RESP_2 - 472 bytes
// Transaction Code: 7201
struct MS_BCAST_INQ_RESP_2 {
    BCAST_HEADER header;               // Offset 0 (40 bytes)
    uint16_t noOfRecords;              // Offset 40
    ST_MARKET_WATCH_BCAST records[5];  // Offset 42 (430 bytes)
};

// ============================================================================
// BCAST_ENHNCD_MW_ROUND_ROBIN (17201) - Enhanced Market Watch
// ============================================================================

// ST_ENHNCD_MARKET_WATCH_BCAST - 90 bytes
struct ST_ENHNCD_MARKET_WATCH_BCAST {
    uint32_t token;                    // Offset 0
    ST_MKT_WISE_INFO mktWiseInfo[3];   // Offset 4 (78 bytes)
    int64_t openInterest;              // Offset 82 (LONG LONG)
};

// MS_ENHNCD_BCAST_INQ_RESP_2 - 492 bytes
// Transaction Code: 17201
struct MS_ENHNCD_BCAST_INQ_RESP_2 {
    BCAST_HEADER header;                      // Offset 0 (40 bytes)
    uint16_t noOfRecords;                     // Offset 40
    ST_ENHNCD_MARKET_WATCH_BCAST records[5];  // Offset 42 (450 bytes)
};

// ============================================================================
// BCAST_SPD_MBP_DELTA (7211) - Spread Market By Price Delta
// ============================================================================

// MbpBuys - 10 bytes
struct MbpBuys {
    uint16_t noOrders;                 // Offset 0
    uint32_t volume;                   // Offset 2
    uint32_t price;                    // Offset 6
};

// MbpSells - 10 bytes
struct MbpSells {
    uint16_t noOrders;                 // Offset 0
    uint32_t volume;                   // Offset 2
    uint32_t price;                    // Offset 6
};

// TotalOrderVolume - 16 bytes
struct TotalOrderVolume {
    double buy;                        // Offset 0
    double sell;                       // Offset 8
};

// MS_SPD_MKT_INFO - 204 bytes
// Transaction Code: 7211
struct MS_SPD_MKT_INFO {
    BCAST_HEADER header;               // Offset 0 (40 bytes)
    uint32_t token1;                   // Offset 40
    uint32_t token2;                   // Offset 44
    uint16_t mbpBuy;                   // Offset 48
    uint16_t mbpSell;                  // Offset 50
    uint32_t lastActiveTime;           // Offset 52
    uint32_t tradedVolume;             // Offset 56 (UNSIGNED LONG)
    double totalTradedValue;           // Offset 60
    MbpBuys mbpBuys[5];                // Offset 68 (50 bytes)
    MbpSells mbpSells[5];              // Offset 118 (50 bytes)
    TotalOrderVolume totalOrderVolume; // Offset 168 (16 bytes)
    uint32_t openPriceDifference;      // Offset 184
    uint32_t dayHighPriceDifference;   // Offset 188
    uint32_t dayLowPriceDifference;    // Offset 192
    uint32_t lastTradedPriceDifference;// Offset 196
    uint32_t lastUpdateTime;           // Offset 200
};

// ============================================================================
// BCAST_LIMIT_PRICE_PROTECTION_RANGE (7220)
// ============================================================================

// LIMIT_PRICE_PROTECTION_RANGE_DETAILS - 12 bytes
struct LIMIT_PRICE_PROTECTION_RANGE_DETAILS {
    uint32_t tokenNumber;              // Offset 0
    uint32_t highExecBand;             // Offset 4
    uint32_t lowExecBand;              // Offset 8
};

// LIMIT_PRICE_PROTECTION_RANGE_DATA - 304 bytes
struct LIMIT_PRICE_PROTECTION_RANGE_DATA {
    uint32_t msgCount;                                    // Offset 0
    LIMIT_PRICE_PROTECTION_RANGE_DETAILS details[25];     // Offset 4 (300 bytes)
};

// MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE - 344 bytes
// Transaction Code: 7220
struct MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE {
    BCAST_HEADER header;                          // Offset 0 (40 bytes)
    LIMIT_PRICE_PROTECTION_RANGE_DATA data;       // Offset 40 (304 bytes)
};

#pragma pack(pop)

#endif // NSE_MARKET_DATA_H
