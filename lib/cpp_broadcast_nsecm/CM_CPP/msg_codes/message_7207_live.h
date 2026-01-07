// NSE Capital Market Multicast UDP Receiver - Message 7207 Header
// 
// FOCUS: Only process message code 7207 (BCAST_INDICES)
// OUTPUT: csv_output/message_7207_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Page 139
// Structure: INDICES (71 bytes per record)
// Maximum Records: 6 per broadcast packet
//
// Converted from Go to C++

#ifndef MESSAGE_7207_LIVE_H
#define MESSAGE_7207_LIVE_H

#include <string>
#include <cstdint>

// =============================================================================
// MESSAGE 7207 STRUCTURE
// =============================================================================

// IndicesInfo7207 - Individual Index record (71 bytes per protocol, 72 with padding)
// Per NSE CM Protocol Table 43.1 (Page 139) - EXACT MATCH CONFIRMED BY DIAGNOSTIC
// Record stride is 72 bytes (71 data + 1 padding byte between records)
struct IndicesInfo7207 {
    char indexName[21];        // 21 bytes, offset 0 - Index name (e.g., "Nifty IT")
    int32_t indexValue;        // 4 bytes, offset 21 - Current index value (in paise)
    int32_t highIndexValue;    // 4 bytes, offset 25 - Day high (in paise)
    int32_t lowIndexValue;     // 4 bytes, offset 29 - Day low (in paise)
    int32_t openingIndex;      // 4 bytes, offset 33 - Opening value (in paise)
    int32_t closingIndex;      // 4 bytes, offset 37 - Closing value (in paise)
    int32_t percentChange;     // 4 bytes, offset 41 - Percent change (basis points)
    int32_t yearlyHigh;        // 4 bytes, offset 45 - 52-week high (in paise)
    int32_t yearlyLow;         // 4 bytes, offset 49 - 52-week low (in paise)
    int32_t noOfUpmoves;       // 4 bytes, offset 53 - Number of stocks up
    int32_t noOfDownmoves;     // 4 bytes, offset 57 - Number of stocks down
    uint8_t reserved;          // 1 byte, offset 61 - Reserved/padding byte
    double marketCapitalisation; // 8 bytes, offset 62-69 - Market cap (DOUBLE)
    uint8_t netChangeIndicator;  // 1 byte, offset 70 - '+' or '-' or ' '
    // Note: 1 additional padding byte (offset 71) between records
};

// Message7207Data - BCAST_INDICES (Broadcast Indices)
struct Message7207Data {
    uint16_t transactionCode; // Always 7207
    uint16_t noOfRecords;     // Number of index records (max 6)
    IndicesInfo7207 indices[6]; // Array of up to 6 indices
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

bool runMessage7207Receiver(const std::string& multicastIP, int port);
void stopMessage7207Receiver();

#endif // MESSAGE_7207_LIVE_H
