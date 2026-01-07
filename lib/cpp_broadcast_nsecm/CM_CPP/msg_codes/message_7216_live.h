// NSE Capital Market Multicast UDP Receiver - Message 7216 Header
// 
// FOCUS: Only process message code 7216 (BCAST_INDICES_VIX - India VIX Index)
// OUTPUT: csv_output/message_7216_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Pages 142-144
// Structure: INDICES (71 bytes per record)
// Maximum Records: 6 per broadcast packet
// Contains: India VIX volatility index
//
// Converted from Go to C++

#ifndef MESSAGE_7216_LIVE_H
#define MESSAGE_7216_LIVE_H

#include <string>
#include <cstdint>

// =============================================================================
// MESSAGE 7216 STRUCTURE
// =============================================================================

// IndexInfo7216 - INDICES structure (71 bytes per record)
// Protocol: NSE CM NNF Protocol v6.3, Pages 142-144
struct IndexInfo7216 {
    char indexName[21];            // 21 bytes, offset 0 - Index name (e.g., "India VIX")
    uint32_t indexValue;           // 4 bytes, offset 21 - Current index value (scaled)
    uint32_t highIndexValue;       // 4 bytes, offset 25 - Day high (scaled)
    uint32_t lowIndexValue;        // 4 bytes, offset 29 - Day low (scaled)
    uint32_t openingIndex;         // 4 bytes, offset 33 - Opening value (scaled)
    uint32_t closingIndex;         // 4 bytes, offset 37 - Closing value (scaled)
    uint32_t percentChange;        // 4 bytes, offset 41 - Percent change (basis points)
    uint32_t yearlyHigh;           // 4 bytes, offset 45 - 52-week high (scaled)
    uint32_t yearlyLow;            // 4 bytes, offset 49 - 52-week low (scaled)
    uint32_t noOfUpmoves;          // 4 bytes, offset 53 - Number of stocks up
    uint32_t noOfDownmoves;        // 4 bytes, offset 57 - Number of stocks down
    uint8_t reserved;              // 1 byte, offset 61 - Reserved/padding byte
    double marketCapitalisation;   // 8 bytes, offset 62-69 - Market cap (DOUBLE)
    uint8_t netChangeIndicator;    // 1 byte, offset 70 - '+' or '-' or ' '
    // Note: Each record is exactly 71 bytes
};

// Message7216Data - BCAST_INDICES_VIX (India VIX Index)
struct Message7216Data {
    uint16_t transactionCode; // Always 7216
    uint16_t noOfRecords;     // Number of index records (max 6)
    IndexInfo7216 indices[6]; // Array of up to 6 indices (typically just VIX)
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

bool runMessage7216Receiver(const std::string& multicastIP, int port);
void stopMessage7216Receiver();

#endif // MESSAGE_7216_LIVE_H
