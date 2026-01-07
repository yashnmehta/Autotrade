// Message 6541 - BC_CIRCUIT_CHECK (Heartbeat Pulse)
// NSE Capital Market Multicast UDP Receiver - Message 6541 Header
// 
// PURPOSE: Process message code 6541 (BC_CIRCUIT_CHECK - Heartbeat Pulse)
// SESSION: Continuous monitoring - Heartbeat sent every ~9 seconds when no other data
// OUTPUT: csv_output/message_6541_TIMESTAMP.csv
//
// STRUCTURE: BCAST_HEADER only (40 bytes) - No additional message fields
// Unlike other messages, 6541 contains only the standard broadcast header
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Page 138
// Converted from Go to C++

#ifndef MESSAGE_6541_LIVE_H
#define MESSAGE_6541_LIVE_H

#include <string>
#include <cstdint>

// =============================================================================
// MESSAGE STRUCTURE DEFINITIONS
// =============================================================================

// BC_CIRCUIT_CHECK structure (only BCAST_HEADER: 40 bytes)
// No additional message fields - just the broadcast header
#pragma pack(push, 1)
struct Message6541Data {
    // BCAST_HEADER fields (40 bytes total)
    char reserved1[4];          // Reserved field - 4 bytes
    uint32_t log_time;          // Log time - 4 bytes
    char alpha_char[2];         // Alpha char - 2 bytes
    uint16_t transaction_code;  // Transaction code (should be 6541) - 2 bytes
    uint16_t error_code;        // Error code - 2 bytes
    uint32_t bc_seq_no;         // Broadcast sequence number - 4 bytes
    char reserved2[4];          // Reserved field 2 - 4 bytes
    char time_stamp2[8];        // Timestamp 2 - 8 bytes
    char filler2[8];            // Filler field - 8 bytes
    uint16_t message_length;    // Message length - 2 bytes
    // Total: 40 bytes (no additional message data)
};
#pragma pack(pop)

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

// Main receiver function for message 6541
bool runMessage6541Receiver(const std::string& multicastIP, int port);

// Stop receiver function
void stopMessage6541Receiver();

// Message parsing function
void process6541Message(const uint8_t* data, int dataLen = 40);

// UDP packet processing function
bool processUDPPacket6541(const uint8_t* data, int dataLen);

// CSV initialization and export functions
bool initialize6541CSV();
void exportTo6541CSV(uint16_t transactionCode, uint32_t logTime, 
                    const std::string& alphaChar, uint16_t errorCode,
                    uint32_t bcSeqNo, const std::string& timeStamp2, 
                    uint16_t messageLength, int64_t heartbeatNumber, 
                    double secondsSinceLast);

// Statistics functions
void printStats6541();
void printFinalStats6541();

#endif // MESSAGE_6541_LIVE_H
