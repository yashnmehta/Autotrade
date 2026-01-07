// NSE Capital Market Multicast UDP Receiver - Message 7306 Header
// 
// FOCUS: Only process message code 7306 (BCAST_PART_MSTR_CHG - Participant Master Change)
// OUTPUT: csv_output/message_7306_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3
// Structure: PARTICIPANT MASTER CHANGE (44 bytes after BCAST_HEADER)
// Contains: Participant information and status changes
//
// Converted from Go to C++

#ifndef MESSAGE_7306_LIVE_H
#define MESSAGE_7306_LIVE_H

#include <string>
#include <cstdint>

// =============================================================================
// MESSAGE 7306 STRUCTURE
// =============================================================================

// Message7306Data - BCAST_PART_MSTR_CHG (Participant Master Change)
// 
// This message broadcasts participant master changes including:
// - Participant ID and name
// - Participant status and flags
// - Suspension dates and details
// - Market access permissions
// 
// Structure: BCAST_HEADER (40 bytes) + Participant Information (44 bytes)
// Total packet size: 84 bytes
struct Message7306Data {
    uint16_t transactionCode;     // Always 7306
    char participantId[5];        // 5 bytes - Participant ID
    char participantName[25];     // 25 bytes - Participant name
    uint16_t participantStatus;   // 2 bytes - Status (Active/Suspended/etc.)
    uint32_t suspendedDate;       // 4 bytes - Suspension date
    uint32_t effectiveDate;       // 4 bytes - Effective date
    uint16_t marketAccess;        // 2 bytes - Market access flags
    uint16_t tradingRights;       // 2 bytes - Trading rights
    uint8_t reserved[2];          // 2 bytes - Reserved bytes
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

bool runMessage7306Receiver(const std::string& multicastIP, int port);
void stopMessage7306Receiver();

#endif // MESSAGE_7306_LIVE_H
