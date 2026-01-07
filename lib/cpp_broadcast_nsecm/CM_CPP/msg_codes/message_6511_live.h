// NSE Capital Market Multicast UDP Receiver - Message 6511 Header
// 
// FOCUS: Only process message code 6511 (BC_OPEN_MESSAGE)
//
// Protocol Reference: NSE CM NNF Protocol v6.3
// Structure: BCAST_VCT_MESSAGES (298 bytes)
// Session: Regular Market - Market open notification

#ifndef MESSAGE_6511_LIVE_H
#define MESSAGE_6511_LIVE_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>

// =============================================================================
// MESSAGE STRUCTURE FOR 6511
// =============================================================================

// Message6511Data - BC_OPEN_MESSAGE (Market Open Messages)
// Per NSE CM Protocol - BCAST_VCT_MESSAGES structure
// Total packet: 298 bytes
struct Message6511Data {
    uint16_t transactionCode;  // Always 6511
    uint16_t branchNumber;     // 2 bytes
    uint8_t brokerNumber[5];   // 5 bytes
    uint8_t actionCode[3];     // 3 bytes - Action code
    uint8_t reserved[4];       // 4 bytes
    uint8_t traderWsBit;       // 1 byte - bit flags
    uint8_t reserved2;         // 1 byte
    uint16_t msgLength;        // 2 bytes
    uint8_t msg[240];          // 240 bytes - Market open message content
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

// Main receiver function
// Parameters:
//   multicastIP: Multicast IP address to listen on
//   port: Port number to listen on
// Returns: true on success, false on error
bool runMessage6511Receiver(const std::string& multicastIP, int port);

// Stop the receiver
void stopMessage6511Receiver();

// Helper functions for message processing
bool processUDPPacket6511(const uint8_t* data, int dataLen);
void process6511Message(const uint8_t* data, int dataLen = 0);

// CSV functions
bool initialize6511CSV();
void exportTo6511CSV(uint16_t transactionCode, uint16_t branchNumber, 
                    const std::string& brokerNumber, const std::string& actionCode,
                    uint8_t traderWsBit, uint16_t msgLength, const std::string& message);

// Statistics functions
void printStats6511();
void printFinalStats6511();

// Utility functions
std::string getMessageCodeDescription6511(uint16_t code);
std::string formatNumber6511(int64_t n);

#endif // MESSAGE_6511_LIVE_H
