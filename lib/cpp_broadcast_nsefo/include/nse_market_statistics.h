#ifndef NSE_MARKET_STATISTICS_H
#define NSE_MARKET_STATISTICS_H

#include "nse_common.h"

// Ensure 1-byte alignment for all structures
#pragma pack(push, 1)

// ============================================================================
// SUPPORTING STRUCTURES
// ============================================================================

// CONTRACT_DESC - 28 bytes
// Contract description structure (referenced in protocol Chapter 4)
struct CONTRACT_DESC {
    char symbol[10];                       // Offset 0
    char series[2];                        // Offset 10
    char instrumentName[6];                // Offset 12
    uint32_t expiryDate;                   // Offset 18
    uint32_t strikePrice;                  // Offset 22
    char optionType[2];                    // Offset 26
};

// ============================================================================
// MARKET STATISTICS REPORT HEADER
// ============================================================================

// MS_RP_HDR - 108 bytes
// Transaction Code: 1833 (RPRT_MARKET_STATS_OUT_RPT)
//                   11833 (ENHNCD_RPRT_MARKET_STATS_OUT_RPT)
// Report header for market statistics
struct MS_RP_HDR {
    MESSAGE_HEADER header;                 // Offset 0 (40 bytes)
    char messageType;                      // Offset 40 ('H', 'X', 'L')
    uint32_t reportDate;                   // Offset 41
    uint16_t userType;                     // Offset 45
    char brokerId[5];                      // Offset 47
    char firmName[25];                     // Offset 52
    uint32_t traderNumber;                 // Offset 77
    char traderName[26];                   // Offset 81
    char reserved;                         // Offset 107 (padding to 108 bytes)
};

// ============================================================================
// MARKET STATISTICS DATA
// ============================================================================

// MKT_STATS_DATA - 74 bytes
// Market statistics data for a single contract
struct MKT_STATS_DATA {
    CONTRACT_DESC contractDesc;            // Offset 0 (28 bytes)
    uint16_t marketType;                   // Offset 28
    uint32_t openPrice;                    // Offset 30
    uint32_t highPrice;                    // Offset 34
    uint32_t lowPrice;                     // Offset 38
    uint32_t closingPrice;                 // Offset 42
    uint32_t totalQuantityTraded;          // Offset 46 (UNSIGNED LONG)
    double totalValueTraded;               // Offset 50
    uint32_t previousClosePrice;           // Offset 58
    uint32_t openInterest;                 // Offset 62 (UNSIGNED LONG)
    uint32_t chgOpenInterest;              // Offset 66
    char indicator[4];                     // Offset 70
};

// MS_RP_MARKET_STATS - 488 bytes
// Transaction Code: 1833 (RPRT_MARKET_STATS_OUT_RPT)
// Market statistics report
struct MS_RP_MARKET_STATS {
    MESSAGE_HEADER header;                 // Offset 0 (40 bytes)
    char messageType;                      // Offset 40 ('R', 'Y', 'M')
    char reserved;                         // Offset 41
    uint16_t numberOfRecords;              // Offset 42
    MKT_STATS_DATA data[6];                // Offset 44 (444 bytes)
};

// ============================================================================
// ENHANCED MARKET STATISTICS DATA
// ============================================================================

// ENHNCD_MKT_STATS_DATA - 82 bytes
// Enhanced market statistics data with 64-bit open interest
struct ENHNCD_MKT_STATS_DATA {
    CONTRACT_DESC contractDesc;            // Offset 0 (28 bytes)
    uint16_t marketType;                   // Offset 28
    uint32_t openPrice;                    // Offset 30
    uint32_t highPrice;                    // Offset 34
    uint32_t lowPrice;                     // Offset 38
    uint32_t closingPrice;                 // Offset 42
    uint32_t totalQuantityTraded;          // Offset 46 (UNSIGNED LONG)
    double totalValueTraded;               // Offset 50
    uint32_t previousClosePrice;           // Offset 58
    int64_t openInterest;                  // Offset 62 (LONG LONG)
    int64_t chgOpenInterest;               // Offset 70 (LONG LONG)
    char indicator[4];                     // Offset 78
};

// ENHNCD_MS_RP_MARKET_STATS - 372 bytes
// Transaction Code: 11833 (ENHNCD_RPRT_MARKET_STATS_OUT_RPT)
// Enhanced market statistics report
struct ENHNCD_MS_RP_MARKET_STATS {
    MESSAGE_HEADER header;                 // Offset 0 (40 bytes)
    char messageType;                      // Offset 40 ('R', 'Y', 'M')
    char reserved;                         // Offset 41
    uint16_t numberOfRecords;              // Offset 42
    ENHNCD_MKT_STATS_DATA data[4];         // Offset 44 (328 bytes)
};

// ============================================================================
// MARKET MOVEMENT WITH OPEN INTEREST
// ============================================================================

// OPEN_INTEREST - 8 bytes
// Open interest for a single token
struct OPEN_INTEREST {
    uint32_t tokenNo;                      // Offset 0
    uint32_t currentOI;                    // Offset 4 (UNSIGNED LONG)
};

// CM_ASSET_OI - 504 bytes
// Transaction Code: 7130 (MKT_MVMT_CM_OI_IN)
// Market movement with open interest
struct CM_ASSET_OI {
    char reserved1[2];                     // Offset 0
    char reserved2[2];                     // Offset 2
    uint32_t logTime;                      // Offset 4
    char marketType[2];                    // Offset 8
    uint16_t transactionCode;              // Offset 10
    uint16_t noOfRecords;                  // Offset 12
    char reserved3[8];                     // Offset 14
    int64_t timeStamp;                     // Offset 22 (LONG LONG)
    char reserved4[8];                     // Offset 30
    uint16_t messageLength;                // Offset 38
    OPEN_INTEREST openInterest[58];        // Offset 40 (464 bytes)
};

// ============================================================================
// ENHANCED MARKET MOVEMENT WITH OPEN INTEREST
// ============================================================================

// ENHNCD_OPEN_INTEREST - 12 bytes
// Enhanced open interest with 64-bit values
struct ENHNCD_OPEN_INTEREST {
    uint32_t tokenNo;                      // Offset 0
    int64_t currentOI;                     // Offset 4 (LONG LONG)
};

// ENHNCD_CM_ASSET_OI - 508 bytes
// Transaction Code: 17130 (ENHNCD_MKT_MVMT_CM_OI_IN)
// Enhanced market movement with open interest
struct ENHNCD_CM_ASSET_OI {
    char reserved1[2];                     // Offset 0
    char reserved2[2];                     // Offset 2
    uint32_t logTime;                      // Offset 4
    char marketType[2];                    // Offset 8
    uint16_t transactionCode;              // Offset 10
    uint16_t noOfRecords;                  // Offset 12
    char reserved3[8];                     // Offset 14
    int64_t timeStamp;                     // Offset 22 (LONG LONG)
    char reserved4[8];                     // Offset 30
    uint16_t messageLength;                // Offset 38
    ENHNCD_OPEN_INTEREST openInterest[39]; // Offset 40 (468 bytes)
};

// ============================================================================
// SPREAD MARKET STATISTICS (from earlier in protocol)
// ============================================================================

// SPD_STATS_DATA - 78 bytes
// Spread statistics data
struct SPD_STATS_DATA {
    uint16_t marketType;                   // Offset 0
    char instrumentName1[6];               // Offset 2
    char symbol1[10];                      // Offset 8
    uint32_t expiryDate1;                  // Offset 18
    uint32_t strikePrice1;                 // Offset 22
    char optionType1[2];                   // Offset 26
    uint16_t caLevel1;                     // Offset 28
    char instrumentName2[6];               // Offset 30
    char symbol2[10];                      // Offset 36
    uint32_t expiryDate2;                  // Offset 46
    uint32_t strikePrice2;                 // Offset 50
    char optionType2[2];                   // Offset 54
    uint16_t caLevel2;                     // Offset 56
    uint32_t openPD;                       // Offset 58
    uint32_t hiPD;                         // Offset 62
    uint32_t lowPD;                        // Offset 66
    uint32_t lastTradedPD;                 // Offset 70
    uint32_t noOfContractsTraded;          // Offset 74
};

// RP_SPD_MKT_STATS - 278 bytes
// Transaction Code: 1862 (SPD_BC_JRNL_VCT_MSG)
// Spread market statistics report
struct RP_SPD_MKT_STATS {
    MESSAGE_HEADER header;                 // Offset 0 (40 bytes)
    char messageType;                      // Offset 40 ('R', 'Y', 'M')
    char reserved;                         // Offset 41
    uint16_t noOfRecords;                  // Offset 42
    SPD_STATS_DATA data[3];                // Offset 44 (234 bytes)
};

// MS_RP_TRAILER - 46 bytes
// Transaction Code: 1862 (SPD_BC_JRNL_VCT_MSG)
// Report trailer for spread statistics
struct MS_RP_TRAILER {
    MESSAGE_HEADER header;                 // Offset 0 (40 bytes)
    char messageType;                      // Offset 40 ('T', 'Z', 'N')
    uint32_t numberOfPackets;              // Offset 41
    char reserved;                         // Offset 45
};

#pragma pack(pop)

#endif // NSE_MARKET_STATISTICS_H
