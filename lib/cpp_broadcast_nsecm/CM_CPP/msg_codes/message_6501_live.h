// NSE Capital Market Multicast UDP Receiver - Message 6501 Header
// 
// FOCUS: Only process message code 6501 (BCAST_JRNL_VCT_MSG - Journal/VCT Messages)
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Page 79-80 (Table 23)
// Structure: MS_TRADER_INT_MSG (298 bytes)
// Contains: System messages, auction notifications, margin violations, listings

#ifndef MESSAGE_6501_LIVE_H
#define MESSAGE_6501_LIVE_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>

// =============================================================================
// MESSAGE STRUCTURE FOR 6501
// =============================================================================

// Message6501Data - BCAST_JRNL_VCT_MSG (Journal/VCT Messages)
// Per NSE CM Protocol Table 23 (Page 79-80)
// Total packet: 298 bytes
struct Message6501Data {
    uint16_t transactionCode;  // Always 6501
    uint16_t branchNumber;     // 2 bytes
    uint8_t brokerNumber[5];   // 5 bytes
    uint8_t actionCode[3];     // 3 bytes - 'SYS', 'AUI', 'AUC', 'LIS', 'MAR'
    uint8_t reserved[4];       // 4 bytes
    uint8_t traderWsBit;       // 1 byte - bit flags
    uint8_t reserved2;         // 1 byte
    uint16_t msgLength;        // 2 bytes
    uint8_t msg[240];          // 240 bytes - Actual message content
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

// Main receiver function
// Parameters:
//   multicastIP: Multicast IP address to listen on
//   port: Port number to listen on
// Returns: true on success, false on error
bool runMessage6501Receiver(const std::string& multicastIP, int port);

// Stop the receiver
void stopMessage6501Receiver();

// Helper functions for message processing
bool processUDPPacket(const uint8_t* data, int dataLen);
void process6501Message(const uint8_t* data, int dataLen = 0);

// CSV functions
bool initialize6501CSV();
void exportToCSV(uint16_t transactionCode, uint16_t branchNumber, 
                const std::string& brokerNumber, const std::string& actionCode,
                uint16_t msgLength, const std::string& message);

// Statistics functions
void printStats();
void printFinalStats();

// Utility functions
std::string getCurrentTimestamp();
std::string getFileTimestamp();
uint16_t readUint16BigEndian(const uint8_t* data, int offset);

#endif // MESSAGE_6501_LIVE_H
