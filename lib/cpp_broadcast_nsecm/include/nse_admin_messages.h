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

// MS_BCAST_MESSAGE - 298 bytes (6501 - Broadcast)
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

// MS_TRADER_INT_MSG - 290 bytes (5295 - Interactive)
// Transaction Code: CTRL_MSG_TO_TRADER (5295)
// Protocol Reference: NSE CM NNF Protocol v6.3, Table 22, Page 78-79
// Interactive message sent to a specific trader from NSE Control
struct MS_TRADER_INT_MSG {
    MESSAGE_HEADER header;                 // Offset 0 (40 bytes)
    int32_t traderId;                      // Offset 40 (4 bytes)
    char actionCode[3];                    // Offset 44 (3 bytes) - 'SYS', 'AUI', 'AUC', 'LIS'
    char reserved;                         // Offset 47 (1 byte)
    int16_t msgLength;                     // Offset 48 (2 bytes)
    char msg[240];                         // Offset 50 (240 bytes)
};
// Total: 40 + 4 + 3 + 1 + 2 + 240 = 290 bytes ✅

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
// CALL AUCTION ORDER CANCELLATION STATISTICS (7210)
// ============================================================================

// INTERACTIVE_ORD_CXL_DETAILS - 56 bytes (per security)
// Transaction Code: 7210 (BCAST_CALL_AUCTION_ORD_CXL_UPDATE)
// Protocol Reference: NSE CM NNF Protocol v6.3, Table 69.1, Page 148
struct INTERACTIVE_ORD_CXL_DETAILS {
    int32_t token;                         // Offset 0 (4 bytes) - Security token
    char filler[4];                        // Offset 4 (4 bytes) - Reserved
    int64_t buyOrdCxlCount;                // Offset 8 (8 bytes) - Buy orders cancelled count
    int64_t buyOrdCxlVol;                  // Offset 16 (8 bytes) - Buy orders cancelled volume
    int64_t sellOrdCxlCount;               // Offset 24 (8 bytes) - Sell orders cancelled count
    int64_t sellOrdCxlVol;                 // Offset 32 (8 bytes) - Sell orders cancelled volume
    char reserved[16];                     // Offset 40 (16 bytes) - Reserved
};

// BCAST_CALL_AUCTION_ORD_CXL_UPDATE - 490 bytes
// Transaction Code: 7210
// Contains: Order cancellation statistics for up to 8 securities during SPOS session
struct MS_BCAST_CALL_AUCTION_ORD_CXL {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    int16_t noOfRecords;                   // Offset 40 (2 bytes) - Number of securities (max 8)
    INTERACTIVE_ORD_CXL_DETAILS records[8]; // Offset 42 (448 bytes) - Array of 8 records
};

// ============================================================================
// SYSTEM PARAMETERS (7206)
// ============================================================================

// SYSTEM_INFORMATION_DATA - 90 bytes (total 130 with header)
// Transaction Code: 7206 (BCAST_SYSTEM_INFORMATION_OUT)
// Protocol Reference: NSE CM NNF Protocol v6.3, Table 10, Page 34-35
struct MS_BCAST_SYSTEM_INFORMATION {
    BCAST_HEADER header;                   // Offset 0 (40 bytes)
    int16_t normal;                        // Offset 40 (2 bytes) - Normal market status
    int16_t oddlot;                        // Offset 42 (2 bytes) - Oddlot market status
    int16_t spot;                          // Offset 44 (2 bytes) - Spot market status
    int16_t auction;                       // Offset 46 (2 bytes) - Auction market status
    int16_t callAuction1;                  // Offset 48 (2 bytes) - Call Auction 1 status
    int16_t callAuction2;                  // Offset 50 (2 bytes) - Call Auction 2 status
    int32_t marketIndex;                   // Offset 52 (4 bytes) - Market index value
    int16_t defaultSettlementPeriodNormal; // Offset 56 (2 bytes)
    int16_t defaultSettlementPeriodSpot;   // Offset 58 (2 bytes)
    int16_t defaultSettlementPeriodAuction;// Offset 60 (2 bytes)
    int16_t competitorPeriod;              // Offset 62 (2 bytes)
    int16_t solicitorPeriod;               // Offset 64 (2 bytes)
    int16_t warningPercent;                // Offset 66 (2 bytes)
    int16_t volumeFreezePercent;           // Offset 68 (2 bytes)
    char reserved1[2];                     // Offset 70 (2 bytes)
    int16_t terminalIdleTime;              // Offset 72 (2 bytes)
    int32_t boardLotQuantity;              // Offset 74 (4 bytes)
    int32_t tickSize;                      // Offset 78 (4 bytes)
    int16_t maximumGtcDays;                // Offset 82 (2 bytes)
    uint16_t securityEligibleIndicators;   // Offset 84 (2 bytes) - Bit flags
    int16_t disclosedQuantityPercentAllowed; // Offset 86 (2 bytes)
    char reserved2[6];                     // Offset 88 (6 bytes)
};

#pragma pack(pop)

#endif // NSE_ADMIN_MESSAGES_H
