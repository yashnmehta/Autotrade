#ifndef NSE_ADMIN_MESSAGES_H
#define NSE_ADMIN_MESSAGES_H

#include "nse_common.h"

// Ensure 1-byte alignment for all structures
#pragma pack(push, 1)

// ============================================================================
// BROADCAST MESSAGE STRUCTURES
// ============================================================================

// BCAST_VCT_MESSAGES - 298 bytes (6511, 6521, etc.)
struct BCAST_VCT_MESSAGES {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    SEC_INFO secInfo;                      // Offset 40 (12 bytes)
    int16_t marketType;                    // Offset 52
    uint16_t broadcastDestination;         // Offset 54
    int16_t broadcastMessageLength;        // Offset 56
    char broadcastMessage[240];            // Offset 58
};

// MS_BCAST_MESSAGE - 298 bytes (6501)
struct MS_BCAST_MESSAGE {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    int16_t branchNumber;                  // Offset 40
    char brokerNumber[5];                  // Offset 42
    char actionCode[3];                    // Offset 47
    char reserved[4];                      // Offset 50
    uint16_t broadcastDestination;         // Offset 54
    int16_t broadcastMessageLength;        // Offset 56
    char broadcastMessage[240];            // Offset 58
};

// ============================================================================
// SECURITY STATUS MESSAGES
// ============================================================================

// MS_SEC_OPEN_MSGS - 58 bytes (6013)
struct MS_SEC_OPEN_MSGS {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    SEC_INFO secInfo;                      // Offset 40 (12 bytes)
    int16_t marketType;                    // Offset 52
    int16_t token;                         // Offset 54 (SHORT for 6013 per CM PDF)
    int32_t openingPrice;                  // Offset 56 (wait, 54+2=56)
};

// BC_SYMBOL_STATUS_CHANGE_ACTION - 58 bytes (7764)
struct BC_SYMBOL_STATUS_CHANGE_ACTION {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    SEC_INFO secInfo;                      // Offset 40 (12 bytes)
    int16_t marketType;                    // Offset 52
    int16_t reserved;                      // Offset 54
    int16_t actionCode;                    // Offset 56
};

// ============================================================================
// CIRCUIT BREAKER AND MARKET STATUS MESSAGES (SIMPLE)
// ============================================================================

// MS_BC_CIRCUIT_CHECK - 40 bytes (6541)
struct MS_BC_CIRCUIT_CHECK {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
};

// MS_BC_OPEN_MSG_SIMPLE - 40 bytes (if header only)
struct MS_BC_OPEN_MSG_SIMPLE {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
};

// ============================================================================
// SYSTEM PARAMETERS (7206)
// ============================================================================

// Placeholder for SYSTEM_INFORMATION_DATA (usually complex)
struct MS_BCAST_SYSTEM_INFORMATION {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    // Additional fields depend on System Info structure
};

#pragma pack(pop)

#endif // NSE_ADMIN_MESSAGES_H
