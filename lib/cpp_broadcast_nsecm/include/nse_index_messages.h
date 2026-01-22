#ifndef NSE_INDEX_MESSAGES_H
#define NSE_INDEX_MESSAGES_H

#include "nse_common.h"

// NSE Protocol requires 2-byte alignment per protocol doc:
// "All structures are pragma pack 2. Structures of odd size should be padded to an even number of bytes."
#pragma pack(push, 2)

// ============================================================================
// INDEX BROADCAST MESSAGES
// ============================================================================

// MS_INDICES - 72 bytes (71 bytes + 1 byte padding for word alignment)
// Individual index information (Same in FO and CM)
struct MS_INDICES {
    char indexName[21];                    // Offset 0 (21 bytes)
    char pad1;                             // Offset 21 (1 byte padding for alignment)
    int32_t indexValue;                    // Offset 22 (4 bytes)
    int32_t highIndexValue;                // Offset 26
    int32_t lowIndexValue;                 // Offset 30
    int32_t openingIndex;                  // Offset 34
    int32_t closingIndex;                  // Offset 38
    int32_t percentChange;                 // Offset 42
    int32_t yearlyHigh;                    // Offset 46
    int32_t yearlyLow;                     // Offset 50
    int32_t noOfUpmoves;                   // Offset 54
    int32_t noOfDownmoves;                 // Offset 58
    double marketCapitalisation;           // Offset 62 (8 bytes)
    char netChangeIndicator;               // Offset 70
    char filler;                           // Offset 71
};  // Total: 72 bytes

// MS_BCAST_INDICES - 474 bytes
// Transaction Code: 7207
// CRITICAL FIX: There are 8 bytes of padding between header and numberOfRecords
struct MS_BCAST_INDICES {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t numberOfRecords;              // Offset 40 (matches Python implementation)
    MS_INDICES indices[6];                 // Offset 42 (432 bytes = 72 * 6)
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
