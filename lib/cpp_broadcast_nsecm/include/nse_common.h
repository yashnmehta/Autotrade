#ifndef NSE_COMMON_H
#define NSE_COMMON_H

#include <cstdint>

// Ensure 1-byte alignment for all structures
#pragma pack(push, 1)

// ============================================================================
// COMMON HEADER STRUCTURE
// ============================================================================

// BCAST_HEADER - 40 bytes
// Used in all broadcast messages
// This header appears at the start of every broadcast message
struct BCAST_HEADER {
    char reserved1[2];             // Offset 0
    char reserved2[2];             // Offset 2
    int32_t logTime;               // Offset 4 (LONG = 4 bytes)
    char alphaChar[2];             // Offset 8
    int16_t transactionCode;       // Offset 10 (SHORT = 2 bytes)
    int16_t errorCode;             // Offset 12 (SHORT = 2 bytes)
    int32_t bcSeqNo;               // Offset 14 (LONG = 4 bytes)
    char reserved3[1];             // Offset 18
    char reserved4[3];             // Offset 19
    char timeStamp2[8];            // Offset 22
    uint8_t filler2[8];            // Offset 30 (BYTE = uint8_t)
    int16_t messageLength;         // Offset 38 (SHORT = 2 bytes)
};

// MESSAGE_HEADER - 40 bytes
// Used in interactive (request/response) messages
// Similar to BCAST_HEADER but for non-broadcast messages
struct MESSAGE_HEADER {
    uint8_t reserved1[4];          // Offset 0
    uint32_t logTime;              // Offset 4
    uint8_t alphaChar[2];          // Offset 8
    uint16_t transactionCode;      // Offset 10 - Identifies message type
    uint16_t errorCode;            // Offset 12
    uint8_t timeStamp1[8];         // Offset 14
    uint8_t timeStamp2[8];         // Offset 22
    uint8_t filler2[8];            // Offset 30
    uint16_t messageLength;        // Offset 38 - Total message length
};


// ============================================================================
// COMMON INDICATOR STRUCTURES
// ============================================================================

// ST_INDICATOR - 2 bytes
// Used to indicate various market conditions
struct ST_INDICATOR {
    uint16_t indicator;
};

// ST_MBO_MBP_TERMS - 2 bytes
// Terms for Market By Order/Market By Price
struct ST_MBO_MBP_TERMS {
    uint16_t terms;
};

// SEC_INFO - 12 bytes
// Common security identification
struct SEC_INFO {
    char symbol[10];               // Offset 0
    char series[2];                // Offset 10
};

#pragma pack(pop)

#endif // NSE_COMMON_H
