#ifndef NSE_DATABASE_UPDATES_H
#define NSE_DATABASE_UPDATES_H

#include "nse_common.h"

// Ensure 1-byte alignment for all structures
#pragma pack(push, 1)

// ============================================================================
// SUPPORTING STRUCTURES FOR DATABASE UPDATES
// ============================================================================

// ST_SEC_ELIGIBILITY_PER_MARKET - 3 bytes
// Security eligibility per market
struct ST_SEC_ELIGIBILITY_PER_MARKET {
    uint8_t flags;                         // Offset 0 (bit 0: Eligibility, bits 1-7: Reserved)
    uint16_t status;                       // Offset 1
};

// ST_ELIGIBILITY_INDICATORS - 2 bytes
// Eligibility indicators
struct ST_ELIGIBILITY_INDICATORS {
    uint8_t flags;                         // Offset 0 (bits for MinimumFill, AON, ParticipateInMarketIndex)
    uint8_t reserved;                      // Offset 1
};

// ST_PURPOSE - 2 bytes
// Purpose flags for corporate actions
struct ST_PURPOSE {
    uint16_t flags;                        // Offset 0 (bits for various corporate actions)
    // Bits: ExerciseStyle, EGM, AGM, Interest, Bonus, Rights, Dividend, etc.
};

// ============================================================================
// SECURITY MASTER CHANGE
// ============================================================================

// MS_SECURITY_UPDATE_INFO - 298 bytes
// Transaction Code: 7305 (BCAST_SECURITY_MSTR_CHG)
//                   7340 (BCAST_SEC_MSTR_CHNG_PERIODIC)
// Security master change notification
struct MS_SECURITY_UPDATE_INFO {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint32_t token;                        // Offset 40
    SEC_INFO secInfo;                      // Offset 44 (30 bytes)
    uint16_t permittedToTrade;             // Offset 74
    double issuedCapital;                  // Offset 76
    uint32_t warningQuantity;              // Offset 84
    uint32_t freezeQuantity;               // Offset 88
    char creditRating[12];                 // Offset 92
    ST_SEC_ELIGIBILITY_PER_MARKET eligibilityPerMarket[4]; // Offset 104 (12 bytes)
    uint16_t issueRate;                    // Offset 116
    uint32_t issueStartDate;               // Offset 118
    uint32_t interestPaymentDate;          // Offset 122
    uint32_t issueMaturityDate;            // Offset 126
    uint32_t marginPercentage;             // Offset 130
    uint32_t minimumLotQuantity;           // Offset 134
    uint32_t boardLotQuantity;             // Offset 138
    uint32_t tickSize;                     // Offset 142
    char name[25];                         // Offset 146
    char reserved1;                        // Offset 171
    uint32_t listingDate;                  // Offset 172
    uint32_t expulsionDate;                // Offset 176
    uint32_t reAdmissionDate;              // Offset 180
    uint32_t recordDate;                   // Offset 184
    uint32_t lowPriceRange;                // Offset 188
    uint32_t highPriceRange;               // Offset 192
    uint32_t expiryDate;                   // Offset 196
    uint32_t noDeliveryStartDate;          // Offset 200
    uint32_t noDeliveryEndDate;            // Offset 204
    ST_ELIGIBILITY_INDICATORS eligibilityIndicators; // Offset 208 (2 bytes)
    uint32_t bookClosureStartDate;         // Offset 210
    uint32_t bookClosureEndDate;           // Offset 214
    uint32_t exerciseStartDate;            // Offset 218
    uint32_t exerciseEndDate;              // Offset 222
    uint32_t oldToken;                     // Offset 226
    char assetInstrument[6];               // Offset 230
    char assetName[10];                    // Offset 236
    uint32_t assetToken;                   // Offset 246
    uint32_t intrinsicValue;               // Offset 250
    uint32_t extrinsicValue;               // Offset 254
    ST_PURPOSE purpose;                    // Offset 258 (2 bytes)
    uint32_t localUpdateDateTime;          // Offset 260
    char deleteFlag;                       // Offset 264
    char remark[25];                       // Offset 265
    uint32_t basePrice;                    // Offset 290
    uint32_t reserved2;                    // Offset 294 (padding to 298 bytes)
};

// ============================================================================
// PARTICIPANT MASTER CHANGE
// ============================================================================

// MS_PARTICIPANT_UPDATE_INFO - 84 bytes
// Transaction Code: 7306 (BCAST_PART_MSTR_CHG)
// Participant master change notification
struct MS_PARTICIPANT_UPDATE_INFO {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    char participantId[12];                // Offset 40
    char participantName[25];              // Offset 52
    char participantStatus;                // Offset 77
    uint32_t participantUpdateDateTime;    // Offset 78
    char deleteFlag;                       // Offset 82
    char reserved;                         // Offset 83
};

// ============================================================================
// INSTRUMENT MASTER CHANGE
// ============================================================================

// MS_INSTRUMENT_UPDATE_INFO - 80 bytes
// Transaction Code: 7324 (BCAST_INSTR_MSTR_CHG)
// Instrument master change notification
struct MS_INSTRUMENT_UPDATE_INFO {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t instrumentId;                 // Offset 40
    char instrumentName[6];                // Offset 42
    char instrumentDescription[25];        // Offset 48
    uint32_t instrumentUpdateTime;         // Offset 73
    char deleteFlag;                       // Offset 77
    char reserved[2];                      // Offset 78 (padding to 80 bytes)
};

// ============================================================================
// SPREAD MASTER CHANGE
// ============================================================================

// MS_SPD_MSTR_CHG - Variable size
// Transaction Code: 7309 (BCAST_SPD_MSTR_CHG)
//                   7341 (BCAST_SPD_MSTR_CHG_PERIODIC)
// Spread master change notification
struct MS_SPD_MSTR_CHG {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint32_t spreadToken;                  // Offset 40
    char instrumentName1[6];               // Offset 44
    char symbol1[10];                      // Offset 50
    uint32_t expiryDate1;                  // Offset 60
    uint32_t strikePrice1;                 // Offset 64
    char optionType1[2];                   // Offset 68
    uint16_t caLevel1;                     // Offset 70
    char instrumentName2[6];               // Offset 72
    char symbol2[10];                      // Offset 78
    uint32_t expiryDate2;                  // Offset 88
    uint32_t strikePrice2;                 // Offset 92
    char optionType2[2];                   // Offset 96
    uint16_t caLevel2;                     // Offset 98
    uint32_t spreadHighPrice;              // Offset 100
    uint32_t spreadLowPrice;               // Offset 104
    char deleteFlag;                       // Offset 108
    char reserved[3];                      // Offset 109 (padding)
};

// ============================================================================
// INDEX MASTER CHANGE
// ============================================================================

// MS_INDEX_MSTR_CHG - Variable size
// Transaction Code: 7325 (BCAST_INDEX_MSTR_CHG)
// Index master change notification
struct MS_INDEX_MSTR_CHG {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint32_t indexToken;                   // Offset 40
    char indexName[21];                    // Offset 44
    char deleteFlag;                       // Offset 65
    char reserved[2];                      // Offset 66 (padding)
};

// ============================================================================
// INDEX MAP TABLE
// ============================================================================

// INDEX_MAP_ENTRY - Variable size
// Individual index mapping entry
struct INDEX_MAP_ENTRY {
    uint32_t securityToken;                // Offset 0
    uint32_t indexToken;                   // Offset 4
    uint32_t weight;                       // Offset 8
};

// MS_INDEX_MAP_TABLE - Variable size
// Transaction Code: 7326 (BCAST_INDEX_MAP_TABLE)
// Index mapping table
struct MS_INDEX_MAP_TABLE {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t numberOfEntries;              // Offset 40
    INDEX_MAP_ENTRY entries[100];          // Offset 42 (variable, max 100)
};

// ============================================================================
// LOCAL DATABASE UPDATE MESSAGES
// ============================================================================

// MS_UPDATE_LOCALDB_HEADER - 48 bytes
// Transaction Code: 7307 (UPDATE_LOCALDB_HEADER)
// Local database update header
struct MS_UPDATE_LOCALDB_HEADER {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t updateType;                   // Offset 40
    uint32_t totalRecords;                 // Offset 42
    uint16_t reserved;                     // Offset 46
};

// MS_UPDATE_LOCALDB_TRAILER - 48 bytes
// Transaction Code: 7308 (UPDATE_LOCALDB_TRAILER)
// Local database update trailer
struct MS_UPDATE_LOCALDB_TRAILER {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t updateType;                   // Offset 40
    uint32_t totalRecordsSent;             // Offset 42
    uint16_t reserved;                     // Offset 46
};

// MS_UPDATE_LOCALDB_DATA - Variable size
// Transaction Code: 7304 (UPDATE_LOCALDB_DATA)
// Local database data update (contains nested structures)
struct MS_UPDATE_LOCALDB_DATA {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t innerTransactionCode;         // Offset 40
    char data[512];                        // Offset 42 (variable data based on inner transaction code)
};

#pragma pack(pop)

#endif // NSE_DATABASE_UPDATES_H
