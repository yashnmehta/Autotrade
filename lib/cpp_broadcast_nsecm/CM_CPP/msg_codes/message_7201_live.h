// NSE Capital Market Multicast UDP Receiver - Message 7201 Header
// 
// FOCUS: Only process message code 7201 (BCAST_MW_ROUND_ROBIN)
// OUTPUT: csv_output/message_7201_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Page 88, Table 39
// Structure: BCAST_MW_ROUND_ROBIN (466 bytes total)
// Layout: 
//   - BCAST_HEADER (40 bytes)
//   - NumberOfRecords (2 bytes) 
//   - MARKETWATCHBROADCAST[4] (4 records × 106 bytes each = 424 bytes)
//
// Converted from Go to C++

#ifndef MESSAGE_7201_LIVE_H
#define MESSAGE_7201_LIVE_H

#include <string>
#include <cstdint>

// =============================================================================
// MESSAGE 7201 STRUCTURE
// =============================================================================

// MarketWiseInformation - 34 bytes per market type
// Protocol: NSE CM NNF Protocol v6.3, Table 39
struct MarketWiseInformation {
    uint16_t mboMbpIndicator; // 2 bytes
    uint64_t buyVolume;       // 8 bytes
    uint32_t buyPrice;        // 4 bytes
    uint64_t sellVolume;      // 8 bytes
    uint32_t sellPrice;       // 4 bytes
    uint32_t lastTradePrice;  // 4 bytes
    uint32_t lastTradeTime;   // 4 bytes
};

// MarketWatchBroadcast - 106 bytes per record (4 bytes Token + 3 × 34 bytes MarketWiseInfo)
struct MarketWatchBroadcast {
    uint32_t token;                           // 4 bytes
    MarketWiseInformation marketWiseInfo[3];  // 3 × 34 = 102 bytes
};

// Message7201Data - BCAST_MW_ROUND_ROBIN (466 bytes)
// Protocol: NSE CM NNF Protocol v6.3, Page 88, Table 39
// Structure: BCAST_HEADER (40) + NumberOfRecords (2) + MARKETWATCHBROADCAST[4] (4 × 106 = 424)
struct Message7201Data {
    uint16_t numberOfRecords;                 // 2 bytes at offset 40
    MarketWatchBroadcast records[4];          // 4 × 106 = 424 bytes starting at offset 42
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

bool runMessage7201Receiver(const std::string& multicastIP, int port);
void stopMessage7201Receiver();

#endif // MESSAGE_7201_LIVE_H
