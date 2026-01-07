#ifndef BSE_PROTOCOL_H
#define BSE_PROTOCOL_H

#include <cstdint>
#include <vector>
#include <array>

namespace bse {

// Constants
constexpr uint32_t LEADING_ZEROS = 0x00000000;
constexpr uint16_t MSG_TYPE_MARKET_PICTURE = 2020;        // 0x07E4
constexpr uint16_t MSG_TYPE_MARKET_PICTURE_COMPLEX = 2021; // 0x07E5
constexpr uint16_t MSG_TYPE_PRODUCT_STATE_CHANGE = 2002;  // 0x07D2 - Session state
constexpr uint16_t MSG_TYPE_INDEX = 2012;                 // 0x07DC
constexpr uint16_t MSG_TYPE_CLOSE_PRICE = 2014;           // Close price broadcast
constexpr uint16_t MSG_TYPE_OPEN_INTEREST = 2015;         // OI for derivatives
constexpr uint16_t MSG_TYPE_IMPLIED_VOLATILITY = 2028;    // IV for options

constexpr size_t HEADER_SIZE = 36;
constexpr size_t RECORD_SLOT_SIZE = 264;

#pragma pack(push, 1)

// Packet Header (36 bytes)
struct PacketHeader {
    uint32_t leadingZeros;      // 0-3: Big Endian (Should be 0)
    uint16_t formatId;          // 4-5: Big Endian
    uint16_t padding1;          // 6-7
    uint16_t msgType;           // 8-9: Little Endian
    uint8_t padding2[10];       // 10-19
    uint16_t hour;              // 20-21: Big Endian (?) 
    uint16_t minute;            // 22-23: Big Endian (?)
    uint16_t second;            // 24-25: Big Endian (?)
    uint8_t padding3[10];       // 26-35 wait, 26 to 35 is 10 bytes. 
};

// Order Book Level (16 bytes)
struct OrderBookLevel {
    int32_t price;      // Little Endian (paise)
    int32_t quantity;   // Little Endian
    int32_t flag;       // Little Endian
    int32_t unknown;    // Little Endian
};

// Market Data Record (variable used, but slot is 264 bytes)
// We will parse this manually using offsets rather than a full struct 
// because of the "Variable Length" nature described (64-108 bytes minimal vs 264 full),
// though the Python code says "Each record occupies EXACTLY 264 bytes (fixed slot size)".
// If it's a fixed slot size in the packet structure (N * 264), we can map a struct max size.

struct RecordData {
    uint32_t token;             // 0-3: Little Endian
    int32_t openPrice;          // 4-7: LE
    int32_t prevClose;          // 8-11: LE
    int32_t highPrice;          // 12-15: LE
    int32_t lowPrice;           // 16-19: LE
    int32_t unknown_20_23;      // 20-23
    int32_t volume;             // 24-27: LE
    uint32_t turnoverLakhs;     // 28-31: LE
    uint32_t lotSize;           // 32-35: LE
    int32_t ltp;                // 36-39: LE
    uint32_t unknown_40_43;     // 40-43
    uint32_t sequenceNumber;    // 44-47: LE
    uint8_t padding_48_83[36];  // 48-83
    int32_t atp;                // 84-87: LE
    uint8_t padding_88_103[16]; // 88-103
    
    // Order Book starts at 104
    // Interleaved: Bid1, Ask1, Bid2, Ask2...
    // Total 5 levels * 2 sides * 16 bytes = 160 bytes
    // 104 + 160 = 264. Matches record size.
    OrderBookLevel depth[10];   // 0=Bid1, 1=Ask1, 2=Bid2, 3=Ask2 ...
};

#pragma pack(pop)

// Decoded Structures (Host Byte Order)

struct DecodedHeader {
    uint16_t formatId;
    uint16_t msgType;
    uint64_t timestamp; // Unix timestamp or raw micros
};

struct DecodedDepthLevel {
    int32_t price;
    uint64_t quantity; // V5.0: Long Long
    uint32_t numOrders; // V5.0: Unsigned Long
    uint64_t impliedQty; // V5.0: Long Long
};

struct DecodedRecord {
    uint32_t token;
    uint64_t packetTimestamp; // System time of receipt
    
    // V5.0 Protocol Fields
    uint32_t noOfTrades;   // V5.0: Unsigned Long
    uint64_t volume;       // V5.0: Long Long
    uint64_t turnover;     // V5.0: Long Long (Traded Value)
    uint64_t ltq;          // V5.0: Long Long
    
    int32_t ltp;
    int32_t open;
    int32_t high;
    int32_t low;
    int32_t close;         // prevClose
    
    // Additional fields from manual
    int32_t weightedAvgPrice; 
    int32_t lowerCircuit;
    int32_t upperCircuit;
    
    std::vector<DecodedDepthLevel> bids;
    std::vector<DecodedDepthLevel> asks;
};

// Decoded Open Interest Record (Message Type 2015)
struct DecodedOpenInterest {
    uint32_t token;
    int64_t openInterest;        // OI in quantity
    int64_t openInterestValue;   // OI in value (2 decimal)
    int32_t openInterestChange;  // Change from previous day
    uint64_t packetTimestamp;    // System time of receipt
};

// Decoded Session State (Message Type 2002)
struct DecodedSessionState {
    uint32_t sessionNumber;
    uint16_t marketSegmentId;
    uint8_t marketType;          // 0=Pre-open, 1=Continuous, 2=Auction
    uint8_t startEndFlag;        // 0=Start, 1=End
    uint64_t timestamp;          // Exchange timestamp
    uint64_t packetTimestamp;    // System time of receipt
};

// Decoded Close Price (Message Type 2014)
struct DecodedClosePrice {
    uint32_t token;
    int32_t closePrice;          // Closing price in paise
    uint64_t packetTimestamp;    // System time of receipt
};

// Decoded Implied Volatility (Message Type 2028)
struct DecodedImpliedVolatility {
    uint32_t token;              // Instrument ID
    int64_t impliedVolatility;   // IV in raw format (multiply by 100 for percentage)
    uint64_t packetTimestamp;    // System time of receipt
};

#pragma pack(pop)

} // namespace bse

#endif // BSE_PROTOCOL_H
