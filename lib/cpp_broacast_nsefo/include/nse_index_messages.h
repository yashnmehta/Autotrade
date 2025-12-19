#ifndef NSE_INDEX_MESSAGES_H
#define NSE_INDEX_MESSAGES_H

#include "nse_common.h"

// Ensure 1-byte alignment for all structures
#pragma pack(push, 1)

// ============================================================================
// INDEX BROADCAST MESSAGES
// ============================================================================

// MS_INDICES - 71 bytes
// Individual index information
struct MS_INDICES {
    char indexName[21];                    // Offset 0
    uint32_t indexValue;                   // Offset 21
    uint32_t highIndexValue;               // Offset 25
    uint32_t lowIndexValue;                // Offset 29
    uint32_t openingIndex;                 // Offset 33
    uint32_t closingIndex;                 // Offset 37
    uint32_t percentChange;                // Offset 41
    uint32_t yearlyHigh;                   // Offset 45
    uint32_t yearlyLow;                    // Offset 49
    uint32_t noOfUpmoves;                  // Offset 53
    uint32_t noOfDownmoves;                // Offset 57
    double marketCapitalisation;           // Offset 61
    char netChangeIndicator;               // Offset 69
    char reserved;                         // Offset 70
};

// MS_BCAST_INDICES - 468 bytes
// Transaction Code: 7207
// Multiple index broadcast
struct MS_BCAST_INDICES {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t numberOfRecords;              // Offset 40
    MS_INDICES indices[6];                 // Offset 42 (426 bytes)
};

// ============================================================================
// INDUSTRY INDEX MESSAGES
// ============================================================================

// INDUSTRY_INDICES - 20 bytes
// Individual industry index information
struct INDUSTRY_INDICES {
    char industryName[15];                 // Offset 0
    uint32_t indexValue;                   // Offset 15
    char reserved;                         // Offset 19 (padding to 20 bytes)
};

// MS_BCAST_INDUSTRY_INDICES - 442 bytes
// Transaction Code: 7203 (BCAST_INDUSTRY_INDEX_UPDATE)
// Industry index broadcast
struct MS_BCAST_INDUSTRY_INDICES {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t noOfRecs;                     // Offset 40
    INDUSTRY_INDICES industryIndices[20];  // Offset 42 (400 bytes)
};

// ============================================================================
// GLOBAL INDEX MESSAGES
// ============================================================================

// INDEX_DETAILS - 98 bytes
// Global index details
struct INDEX_DETAILS {
    uint32_t token;                        // Offset 0
    char name[50];                         // Offset 4
    uint32_t open;                         // Offset 54
    uint32_t high;                         // Offset 58
    uint32_t low;                          // Offset 62
    uint32_t last;                         // Offset 66
    uint32_t close;                        // Offset 70
    uint32_t prevClose;                    // Offset 74
    uint32_t lifeHigh;                     // Offset 78
    uint32_t lifeLow;                      // Offset 82
    uint32_t filler1;                      // Offset 86
    uint32_t filler2;                      // Offset 90
    uint32_t filler3;                      // Offset 94
};

// MS_GLOBAL_INDICES - 138 bytes
// Transaction Code: 7732 (GI_INDICES_ASSETS)
// Global indices broadcast
struct MS_GLOBAL_INDICES {
    MESSAGE_HEADER header;                 // Offset 0 (40 bytes)
    INDEX_DETAILS indexDetails;            // Offset 40 (98 bytes)
};

#pragma pack(pop)

#endif // NSE_INDEX_MESSAGES_H
