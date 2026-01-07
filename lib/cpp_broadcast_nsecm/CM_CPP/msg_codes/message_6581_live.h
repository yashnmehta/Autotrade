// Message 6581 - BC_AUCTION_STATUS_CHANGE
// NSE Capital Market Multicast UDP Receiver - Message 6581 Header
// 
// PURPOSE: Process message code 6581 (BC_AUCTION_STATUS_CHANGE)
// SESSION: Auction Session Management - Status transitions between auction phases
// OUTPUT: csv_output/message_6581_TIMESTAMP.csv
//
// STRUCTURE: BCAST_VCT_MESSAGES (total 298 bytes)
// - BCAST_HEADER: 40 bytes (shared header)
// - BC_AUCTION_STATUS_CHANGE: 258 bytes (actual message data)
//
// Protocol Reference: NSE CM NNF Protocol v6.3
// Converted from Go to C++

#ifndef MESSAGE_6581_LIVE_H
#define MESSAGE_6581_LIVE_H

#include <string>
#include <cstdint>

// =============================================================================
// MESSAGE STRUCTURE DEFINITIONS
// =============================================================================

// BC_AUCTION_STATUS_CHANGE structure (total: 258 bytes)
#pragma pack(push, 1)
struct Message6581Data {
    // BCAST_VCT_MESSAGES common fields
    uint16_t transaction_code;     // Transaction code (should be 6581) - 2 bytes
    uint16_t branch_number;        // Branch number - 2 bytes
    char broker_number[5];         // Broker number - 5 bytes
    char action_code[3];           // Action code - 3 bytes
    char reserved[4];              // Reserved field - 4 bytes
    uint8_t trader_ws_bit;         // Trader WS bit - 1 byte
    char reserved2[1];             // Reserved field 2 - 1 byte
    uint16_t msg_length;           // Message length - 2 bytes
    char message[240];             // Message content - 240 bytes
    // Total: 20 + 240 = 260 bytes (minus 2 for header alignment = 258)
};
#pragma pack(pop)

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

// Main receiver function for message 6581
bool runMessage6581Receiver(const std::string& multicastIP, int port);

// Stop receiver function
void stopMessage6581Receiver();

// Message parsing function
void process6581Message(const uint8_t* data, int dataLen = 298);

// UDP packet processing function
bool processUDPPacket6581(const uint8_t* data, int dataLen);

// CSV initialization and export functions
bool initialize6581CSV();
void exportTo6581CSV(uint16_t transactionCode, uint16_t branchNumber, 
                    const std::string& brokerNumber, const std::string& actionCode,
                    uint8_t traderWsBit, uint16_t msgLength, const std::string& message);

// Statistics functions
void printStats6581();
void printFinalStats6581();

#endif // MESSAGE_6581_LIVE_H
