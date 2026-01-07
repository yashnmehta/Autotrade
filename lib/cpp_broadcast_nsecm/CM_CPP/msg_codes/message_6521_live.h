// NSE Capital Market Multicast UDP Receiver - Message 6521 Header
// 
// FOCUS: Only process message code 6521 (BC_CLOSE_MESSAGE)
//
// Protocol Reference: NSE CM NNF Protocol v6.3
// Structure: BCAST_VCT_MESSAGES (298 bytes)
// Session: Post-Market - Market close notification

#ifndef MESSAGE_6521_LIVE_H
#define MESSAGE_6521_LIVE_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>

// =============================================================================
// MESSAGE STRUCTURE FOR 6521
// =============================================================================

// Message6521Data - BC_CLOSE_MESSAGE (Market Close Messages)
// Per NSE CM Protocol - BCAST_VCT_MESSAGES structure
// Total packet: 298 bytes
struct Message6521Data {
    uint16_t transactionCode;  // Always 6521
    uint16_t branchNumber;     // 2 bytes
    uint8_t brokerNumber[5];   // 5 bytes
    uint8_t actionCode[3];     // 3 bytes - Action code
    uint8_t reserved[4];       // 4 bytes
    uint8_t traderWsBit;       // 1 byte - bit flags
    uint8_t reserved2;         // 1 byte
    uint16_t msgLength;        // 2 bytes
    uint8_t msg[240];          // 240 bytes - Market close message content
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

// Main receiver function
// Parameters:
//   multicastIP: Multicast IP address to listen on
//   port: Port number to listen on
// Returns: true on success, false on error
bool runMessage6521Receiver(const std::string& multicastIP, int port);

// Stop the receiver
void stopMessage6521Receiver();

// Helper functions for message processing
bool processUDPPacket6521(const uint8_t* data, int dataLen);
void process6521Message(const uint8_t* data, int dataLen = 0);

// CSV functions
bool initialize6521CSV();
void exportTo6521CSV(uint16_t transactionCode, uint16_t branchNumber, 
                    const std::string& brokerNumber, const std::string& actionCode,
                    uint8_t traderWsBit, uint16_t msgLength, const std::string& message);

// Statistics functions
void printStats6521();
void printFinalStats6521();

// Utility functions
std::string getMessageCodeDescription6521(uint16_t code);
std::string formatNumber6521(int64_t n);

#endif // MESSAGE_6521_LIVE_H
