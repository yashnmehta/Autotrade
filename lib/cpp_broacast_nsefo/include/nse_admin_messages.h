#ifndef NSE_ADMIN_MESSAGES_H
#define NSE_ADMIN_MESSAGES_H

#include "nse_common.h"

// Ensure 1-byte alignment for all structures
#pragma pack(push, 1)

// ============================================================================
// CIRCUIT BREAKER AND MARKET STATUS MESSAGES
// ============================================================================

// MS_BC_CIRCUIT_CHECK - 40 bytes
// Transaction Code: 6541
// Circuit breaker check message
struct MS_BC_CIRCUIT_CHECK {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
};

// MS_BC_OPEN_MSG - 40 bytes
// Transaction Code: 6511
// Market open message
struct MS_BC_OPEN_MSG {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
};

// MS_BC_CLOSE_MSG - 40 bytes
// Transaction Code: 6521
// Market close message
struct MS_BC_CLOSE_MSG {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
};

// MS_BC_POSTCLOSE_MSG - 40 bytes
// Transaction Code: 6522
// Post-close message
struct MS_BC_POSTCLOSE_MSG {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
};

// MS_BC_PRE_OR_POST_DAY_MSG - 40 bytes
// Transaction Code: 6531
// Pre or post day message
struct MS_BC_PRE_OR_POST_DAY_MSG {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
};

// MS_BC_NORMAL_MKT_PREOPEN_ENDED - 40 bytes
// Transaction Code: 6571
// Normal market pre-open ended message
struct MS_BC_NORMAL_MKT_PREOPEN_ENDED {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
};

// ============================================================================
// BROADCAST MESSAGES
// ============================================================================

// ST_BCAST_DESTINATION - 2 bytes
// Broadcast destination flags
struct ST_BCAST_DESTINATION {
    uint8_t flags;                         // Offset 0 (bit flags)
    uint8_t reserved;                      // Offset 1
    // Bit layout (little endian):
    // bit 0: TraderWorkstation
    // bit 1: ControlWorkstation
    // bit 2: Tandem
    // bit 3: JournalingRequired
    // bits 4-7: Reserved
};

// MS_BCAST_MESSAGE - 320 bytes
// Transaction Code: 6501 (BCAST_JRNL_VCT_MSG)
// General broadcast message
struct MS_BCAST_MESSAGE {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t branchNumber;                 // Offset 40
    char brokerNumber[5];                  // Offset 42
    char actionCode[3];                    // Offset 47 (e.g., 'SYS', 'LIS', 'MWL')
    ST_BCAST_DESTINATION destination;      // Offset 50 (2 bytes)
    char reserved[26];                     // Offset 52
    uint16_t broadcastMessageLength;       // Offset 78
    char broadcastMessage[239];            // Offset 80
};

// ============================================================================
// CONTROL MESSAGES
// ============================================================================

// MS_CTRL_MSG_TO_TRADER - 290 bytes
// Transaction Code: 5295
// Control message to specific trader
struct MS_CTRL_MSG_TO_TRADER {
    MESSAGE_HEADER header;                 // Offset 0 (40 bytes)
    uint32_t traderId;                     // Offset 40
    char actionCode[3];                    // Offset 44 (e.g., 'MAR', 'OTH')
    char reserved;                         // Offset 47
    uint16_t broadcastMessageLength;       // Offset 48
    char broadcastMessage[239];            // Offset 50
};

// ============================================================================
// SECURITY STATUS MESSAGES
// ============================================================================

// ST_SEC_STATUS_PER_MARKET - 2 bytes
// Security status for a specific market
struct ST_SEC_STATUS_PER_MARKET {
    uint16_t status;                       // Offset 0
    // Status values:
    // 1 - Pre-open (Normal market only)
    // 2 - Open
    // 3 - Suspended
    // 4 - Pre-open extended
    // 5 - Stock Open With Market
    // 6 - Price Discovery
};

// TOKEN_AND_ELIGIBILITY - 12 bytes
// Token with market-specific status
struct TOKEN_AND_ELIGIBILITY {
    uint32_t token;                        // Offset 0
    ST_SEC_STATUS_PER_MARKET status[4];    // Offset 4 (8 bytes)
};

// MS_SECURITY_STATUS_UPDATE_INFO - 462 bytes
// Transaction Code: 7320 (BCAST_SECURITY_STATUS_CHG)
//                   7210 (BCAST_SECURITY_STATUS_CHG_PREOPEN)
// Security status change notification
struct MS_SECURITY_STATUS_UPDATE_INFO {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t numberOfRecords;              // Offset 40
    TOKEN_AND_ELIGIBILITY records[35];     // Offset 42 (420 bytes)
};

// ============================================================================
// SYSTEM INFORMATION
// ============================================================================

// MS_SYSTEM_INFO_DATA - 106 bytes
// Transaction Code: 7206 (BCAST_SYSTEM_INFORMATION_OUT)
// System information broadcast
// Note: This structure contains system-wide parameters
// Refer to MS_SYSTEM_INFO_OUT in Chapter 3 of protocol doc
struct MS_SYSTEM_INFO_DATA {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t normalMarketStatus;           // Offset 40
    uint16_t oddLotMarketStatus;           // Offset 42
    uint16_t spotMarketStatus;             // Offset 44
    uint16_t auctionMarketStatus;          // Offset 46
    uint32_t defaultSettlementPeriod;      // Offset 48
    char competitionPeriod;                // Offset 52
    char disclosedQuantityPercentAllowed;  // Offset 53
    uint16_t regularLotSize;               // Offset 54
    char tickSize;                         // Offset 56
    char reserved[49];                     // Offset 57 (padding to 106 bytes)
};

// MS_PARTIAL_SYSTEM_INFORMATION - Variable size
// Transaction Code: 7321
// Partial system information update
struct MS_PARTIAL_SYSTEM_INFORMATION {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint16_t updateType;                   // Offset 40
    char data[256];                        // Offset 42 (variable data)
};

// ============================================================================
// SECURITY OPEN PRICE
// ============================================================================

// MS_SECURITY_OPEN_PRICE - 48 bytes
// Transaction Code: 6013
// Security opening price notification
struct MS_SECURITY_OPEN_PRICE {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    uint32_t token;                        // Offset 40
    uint32_t openPrice;                    // Offset 44
};

// ============================================================================
// BROKER STATUS MESSAGES
// ============================================================================

// MS_BCAST_TURNOVER_EXCEEDED - 48 bytes
// Transaction Code: 9010
// Turnover limit exceeded alert
struct MS_BCAST_TURNOVER_EXCEEDED {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    char brokerNumber[5];                  // Offset 40
    char reserved[3];                      // Offset 45
};

// MS_BROADCAST_BROKER_REACTIVATED - 48 bytes
// Transaction Code: 9011
// Broker reactivated notification
struct MS_BROADCAST_BROKER_REACTIVATED {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    char brokerNumber[5];                  // Offset 40
    char reserved[3];                      // Offset 45
};

#pragma pack(pop)

#endif // NSE_ADMIN_MESSAGES_H
