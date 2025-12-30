#ifndef NSE_INDEX_MESSAGES_H
#define NSE_INDEX_MESSAGES_H

#include "nse_common.h"

// Ensure 1-byte alignment for all structures
#pragma pack(push, 1)

// ============================================================================
// INDEX BROADCAST MESSAGES
// ============================================================================

// MS_INDICES - 71 bytes
// Individual index information (Same in FO and CM)
struct MS_INDICES {
    char indexName[21];                    // Offset 0
    int32_t indexValue;                    // Offset 21
    int32_t highIndexValue;                // Offset 25
    int32_t lowIndexValue;                 // Offset 29
    int32_t openingIndex;                  // Offset 33
    int32_t closingIndex;                  // Offset 37
    int32_t percentChange;                 // Offset 41
    int32_t yearlyHigh;                    // Offset 45
    int32_t yearlyLow;                     // Offset 49
    int32_t noOfUpmoves;                   // Offset 53
    int32_t noOfDownmoves;                 // Offset 57
    double marketCapitalisation;           // Offset 61
    char netChangeIndicator;               // Offset 69
    char filler;                           // Offset 70
};

// MS_BCAST_INDICES - 474 bytes (468 used + 6 padding)
// Transaction Code: 7207
struct MS_BCAST_INDICES {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t numberOfRecords;              // Offset 40
    MS_INDICES indices[6];                 // Offset 42 (426 bytes)
    uint8_t reserved[6];                   // Pad to 474 bytes as per PDF
};

// ============================================================================
// INDUSTRY INDEX MESSAGES
// ============================================================================

// INDUSTRY_INDICES - 25 bytes
struct INDUSTRY_INDICES {
    char industryName[21];                 // Offset 0
    int32_t indexValue;                    // Offset 21
};

// MS_BCAST_INDUSTRY_INDICES - 484 bytes (467 used + 17 reserved?)
// Transaction Code: 7203 (BCAST_IND_INDICES)
struct MS_BCAST_INDUSTRY_INDICES {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t noOfRecs;                     // Offset 40
    INDUSTRY_INDICES industryIndices[17];  // Offset 42 (425 bytes)
    uint8_t reserved[17];                   // Pad to 484 if needed? (40+2+425=467, 484-467=17)
};

#pragma pack(pop)

#endif // NSE_INDEX_MESSAGES_H
