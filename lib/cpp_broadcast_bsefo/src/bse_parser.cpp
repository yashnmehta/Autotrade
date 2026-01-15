#include "bse_parser.h"
#include "bse_utils.h"
#include <iostream>
#include <chrono>

namespace bse {

using namespace bse_utils;

BSEParser::BSEParser() {}

void BSEParser::parsePacket(const uint8_t* buffer, size_t length, ParserStats& stats) {
    if (length < HEADER_SIZE) {
        stats.packetsInvalid++;
        return;
    }

    // Read message type (Offset 8, 2 bytes, Little Endian per empirical verification)
    uint16_t msgType = le16toh_func(*(uint16_t*)(buffer + 8));

    switch (msgType) {
        case MSG_TYPE_MARKET_PICTURE:
            stats.packets2020++;
            decodeMarketPicture(buffer, length, msgType, stats);
            break;
        case MSG_TYPE_MARKET_PICTURE_COMPLEX:
            stats.packets2021++;
            decodeMarketPicture(buffer, length, msgType, stats);
            break;
        case MSG_TYPE_OPEN_INTEREST:
            stats.packets2015++;
            decodeOpenInterest(buffer, length, stats);
            break;
        case MSG_TYPE_PRODUCT_STATE_CHANGE:
            stats.packets2002++;
            decodeSessionState(buffer, length, stats);
            break;
        case MSG_TYPE_CLOSE_PRICE:
            stats.packets2014++;
            decodeClosePrice(buffer, length, stats);
            break;
        case MSG_TYPE_INDEX:
            stats.packets2012++;
            decodeIndex(buffer, length, stats);
            break;
        case MSG_TYPE_IMPLIED_VOLATILITY:
            stats.packets2028++;
            decodeImpliedVolatility(buffer, length, stats);
            break;
        default:
            // Unknown or unsupported message type
            // std::cout << "Unknown MsgType: " << msgType << std::endl;
            stats.packetsInvalid++; // Or just ignore
            break;
    }
}

void BSEParser::decodeMarketPicture(const uint8_t* buffer, size_t length, uint16_t msgType, ParserStats& stats) {
    if (length < HEADER_SIZE) return;

    size_t recordSlotSize = 264; // Fixed slot size per empirical evidence
    size_t numRecords = (length - HEADER_SIZE) / recordSlotSize;
    
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (size_t i = 0; i < numRecords; i++) {
        size_t recordOffset = HEADER_SIZE + (i * recordSlotSize);
        if (recordOffset + recordSlotSize > length) break;
        
        const uint8_t* record = buffer + recordOffset;
        
        DecodedRecord decRec;
        decRec.packetTimestamp = now;
        
        // Token (0-3)
        decRec.token = le32toh_func(*(uint32_t*)(record + 0));
        if (decRec.token == 0) continue;
        
        decRec.open = le32toh_func(*(uint32_t*)(record + 4));
        int32_t prevClose = le32toh_func(*(uint32_t*)(record + 8));
        decRec.high = le32toh_func(*(uint32_t*)(record + 12));
        decRec.low = le32toh_func(*(uint32_t*)(record + 16));
        decRec.volume = le32toh_func(*(uint32_t*)(record + 24)); // NOTE: Code uses 4 bytes, Manual V5 says 8. Sticking to code for 2020/2021.
        decRec.turnover = le32toh_func(*(uint32_t*)(record + 28));
        decRec.ltp = le32toh_func(*(uint32_t*)(record + 36));
        
        // Read unknown fields for analysis (48-83)
        // Read unknown fields for analysis (48-83)
        uint32_t field_48 = le32toh_func(*(uint32_t*)(record + 48));
        uint32_t field_52 = le32toh_func(*(uint32_t*)(record + 52));
        uint32_t field_56 = le32toh_func(*(uint32_t*)(record + 56));
        uint32_t field_60 = le32toh_func(*(uint32_t*)(record + 60));
        
        // Empirically Verified Fields
        decRec.totalBuyQty = le32toh_func(*(uint32_t*)(record + 64));  // [64-67]
        decRec.totalSellQty = le32toh_func(*(uint32_t*)(record + 68)); // [68-71]
        
        uint32_t field_72 = le32toh_func(*(uint32_t*)(record + 72));
        
        decRec.lowerCircuit = le32toh_func(*(uint32_t*)(record + 76)); // [76-79]
        decRec.upperCircuit = le32toh_func(*(uint32_t*)(record + 80)); // [80-83]
        
        decRec.weightedAvgPrice = le32toh_func(*(uint32_t*)(record + 84));
        
        // Read unknown fields (88-103)
        uint32_t field_88 = le32toh_func(*(uint32_t*)(record + 88));
        uint32_t field_92 = le32toh_func(*(uint32_t*)(record + 92));
        uint32_t field_96 = le32toh_func(*(uint32_t*)(record + 96));
        uint32_t field_100 = le32toh_func(*(uint32_t*)(record + 100));
        
        // Debug print for specific tokens we want to analyze
        if (decRec.token == 11632641 ) {
            std::cout << "\n=== BSE Record Debug (Token: " << decRec.token << ") ===" << std::endl;
            std::cout << "Known Fields:" << std::endl;
            std::cout << "  LTP: " << decRec.ltp << " (paise), Volume: " << decRec.volume 
                      << ", Open: " << decRec.open << ", High: " << decRec.high << ", Low: " << decRec.low << std::endl;
            std::cout << "  Turnover: " << decRec.turnover << ", ATP: " << decRec.weightedAvgPrice << std::endl;
            std::cout << "Verified New Fields:" << std::endl;
            std::cout << "  Total Buy Qty [64-67]: " << decRec.totalBuyQty << std::endl;
            std::cout << "  Total Sell Qty [68-71]: " << decRec.totalSellQty << std::endl;
            std::cout << "  Lower Circuit [76-79]: " << decRec.lowerCircuit << std::endl;
            std::cout << "  Upper Circuit [80-83]: " << decRec.upperCircuit << std::endl;
            
            std::cout << "Remaining Unknowns:" << std::endl;
            std::cout << "  [48-51]: " << field_48 << std::endl;
            std::cout << "  [52-55]: " << field_52 << std::endl;
            std::cout << "  [56-59]: " << field_56 << std::endl;
            std::cout << "  [60-63]: " << field_60 << std::endl;
            std::cout << "  [72-75]: " << field_72 << std::endl;
            std::cout << "  [88-103]: " << field_88 << ", " << field_92 << ", " << field_96 << ", " << field_100 << std::endl;
        }
        
        // Depth
        for (int level = 0; level < 5; level++) {
            int bidOffset = 104 + (level * 32);
            int askOffset = bidOffset + 16;
            
            if (recordOffset + askOffset + 8 <= length) {
                DecodedDepthLevel bid, ask;
                // Empirical: 16 byte struct: Price(4), Qty(4), Flag(4), Unk(4)
                bid.price = le32toh_func(*(uint32_t*)(record + bidOffset));
                bid.quantity = le32toh_func(*(uint32_t*)(record + bidOffset + 4));
                
                ask.price = le32toh_func(*(uint32_t*)(record + askOffset));
                ask.quantity = le32toh_func(*(uint32_t*)(record + askOffset + 4));
                
                decRec.bids.push_back(bid);
                decRec.asks.push_back(ask);
            }
        }
        
        decRec.close = prevClose;
        
        // Defaults
        decRec.noOfTrades = 0;
        decRec.ltq = 0;
        
        stats.packetsDecoded++;
        if (recordCallback_) recordCallback_(decRec);
    }
}

void BSEParser::decodeOpenInterest(const uint8_t* buffer, size_t length, ParserStats& stats) {
    if (length < HEADER_SIZE + 2) return; // Need at least numRecords field

    // numRecords at offset 32 (Verified in previous code)
    uint16_t numRecords = le16toh_func(*(uint16_t*)(buffer + 32));
    
    constexpr size_t OI_RECORD_SIZE = 34; // Manual says 34 bytes
    size_t recordStart = HEADER_SIZE;
    
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (uint16_t i = 0; i < numRecords && i < 40; i++) { // Safety cap
        size_t offset = recordStart + (i * OI_RECORD_SIZE);
        if (offset + OI_RECORD_SIZE > length) break;
        
        const uint8_t* record = buffer + offset;
        
        DecodedOpenInterest oiRec;
        oiRec.packetTimestamp = now;
        oiRec.token = le32toh_func(*(uint32_t*)(record + 0));
        oiRec.openInterest = (int64_t)le64toh_func(*(uint64_t*)(record + 4));
        oiRec.openInterestValue = (int64_t)le64toh_func(*(uint64_t*)(record + 12));
        oiRec.openInterestChange = (int32_t)le32toh_func(*(uint32_t*)(record + 20));
        
        if (oiRec.token == 0) continue;
        
        stats.packetsDecoded++;
        if (oiCallback_) oiCallback_(oiRec);
    }
}

void BSEParser::decodeSessionState(const uint8_t* buffer, size_t length, ParserStats& stats) {
    if (length < HEADER_SIZE + 8) return;
    
    const uint8_t* data = buffer + HEADER_SIZE;
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    DecodedSessionState state;
    state.packetTimestamp = now;
    state.sessionNumber = le32toh_func(*(uint32_t*)(data + 0));
    state.marketSegmentId = le16toh_func(*(uint16_t*)(data + 4));
    state.marketType = *(data + 6);
    state.startEndFlag = *(data + 7);
    state.timestamp = now;
    
    stats.packetsDecoded++;
    if (sessionStateCallback_) sessionStateCallback_(state);
}

void BSEParser::decodeClosePrice(const uint8_t* buffer, size_t length, ParserStats& stats) {
    if (length < HEADER_SIZE + 8) return;
    
    size_t recordSlotSize = 264; // Try max first
    size_t numRecords = (length - HEADER_SIZE) / recordSlotSize;
    if (numRecords == 0) {
        recordSlotSize = 8; // Min size (Token + Price)
        numRecords = (length - HEADER_SIZE) / recordSlotSize;
    }
    
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (size_t i = 0; i < numRecords; i++) {
        size_t offset = HEADER_SIZE + (i * recordSlotSize);
        if (offset + 8 > length) break;
        
        const uint8_t* record = buffer + offset;
        DecodedClosePrice cp;
        cp.packetTimestamp = now;
        cp.token = le32toh_func(*(uint32_t*)(record + 0));
        cp.closePrice = le32toh_func(*(uint32_t*)(record + 4));
        
        if (cp.token == 0) continue;
        
        stats.packetsDecoded++;
        if (closePriceCallback_) closePriceCallback_(cp);
    }
}

void BSEParser::decodeIndex(const uint8_t* buffer, size_t length, ParserStats& stats) {
    // 2012 Index Record
    size_t recordSlotSize = 120; // Estimated/Previous assumption
    size_t numRecords = (length - HEADER_SIZE) / recordSlotSize;
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    for (size_t i=0; i<numRecords; i++) {
        size_t offset = HEADER_SIZE + (i * recordSlotSize);
        if (offset + 28 > length) break; // Need up to LTP/Close
        const uint8_t* record = buffer + offset;
        
        DecodedRecord decRec; // Reuse DecodedRecord for Index
        decRec.packetTimestamp = now;
        decRec.token = le32toh_func(*(uint32_t*)(record + 0));
        // Layout assumption: 12-15 Open, 16-19 High, 20-23 Low, 24-27 LTP, 28-31 Close
        decRec.open = le32toh_func(*(uint32_t*)(record + 12));
        decRec.high = le32toh_func(*(uint32_t*)(record + 16));
        decRec.low = le32toh_func(*(uint32_t*)(record + 20));
        decRec.ltp = le32toh_func(*(uint32_t*)(record + 24));
        decRec.close = le32toh_func(*(uint32_t*)(record + 28));
        
        // Defaults
        decRec.volume = 0;
        decRec.turnover = 0;
        
        stats.packetsDecoded++;
        stats.packetsDecoded++;
        if (indexCallback_) indexCallback_(decRec);
        else if (recordCallback_) recordCallback_(decRec); // Fallback
    }
}

void BSEParser::decodeImpliedVolatility(const uint8_t* buffer, size_t length, ParserStats& stats) {
    // Message Type 2028 (Implied Volatility)
    // Structure as per Manual V5.0:
    // Header (32 bytes up to Reserveds) + NoOfRecords(2) at offset 32? (Assuming alignment with 2015)
    // Record Size: 72 bytes (Calculated from Manual)
    
    if (length < 34) return; // Header + NoOfRecords
    
    uint16_t numRecords = le16toh_func(*(uint16_t*)(buffer + 32));
    size_t recordSize = 72; 
    size_t recordStart = HEADER_SIZE; // 36
    
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (uint16_t i=0; i<numRecords; i++) {
        size_t offset = recordStart + (i * recordSize);
        if (offset + 12 > length) break; // Need Token(4) + IV(8)
        
        const uint8_t* record = buffer + offset;
        DecodedImpliedVolatility iv;
        iv.packetTimestamp = now;
        iv.token = le32toh_func(*(uint32_t*)(record + 0));
        iv.impliedVolatility = (int64_t)le64toh_func(*(uint64_t*)(record + 4));
        
        if (iv.token == 0) continue;
        
        stats.packetsDecoded++;
        if (ivCallback_) ivCallback_(iv);
    }
}

} // namespace bse
