#include "bse_parser.h"
#include "bse_utils.h"
#include "bse_price_store.h"
#include <iostream>
#include <chrono>
#include <unordered_map>

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
        case MSG_TYPE_RBI_REFERENCE_RATE:
            stats.packets2022++;
            decodeRBIReferenceRate(buffer, length, stats);
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

    size_t recordSlotSize = 264; // Fixed slot size
    size_t numRecords = (length - HEADER_SIZE) / recordSlotSize;
    
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    // Determine target store
    PriceStore* store = nullptr;
    if (marketSegment_ == 12 || marketSegment_ == 3) {
        store = &g_bseFoPriceStore;
    } else if (marketSegment_ == 11 || marketSegment_ == 2) {
        store = &g_bseCmPriceStore;
    }

    if (!store) return; // Should not happen if configured correctly

    for (size_t i = 0; i < numRecords; i++) {
        size_t recordOffset = HEADER_SIZE + (i * recordSlotSize);
        if (recordOffset + recordSlotSize > length) break;
        
        const uint8_t* record = buffer + recordOffset;
        uint32_t token = le32toh_func(*(uint32_t*)(record + 0));
        if (token == 0) continue;
        
        // Extract Data
        double open = le32toh_func(*(uint32_t*)(record + 4)) / 100.0;
        double close = le32toh_func(*(uint32_t*)(record + 8)) / 100.0; // Pclose
        double high = le32toh_func(*(uint32_t*)(record + 12)) / 100.0;
        double low = le32toh_func(*(uint32_t*)(record + 16)) / 100.0;
        uint64_t volume = le32toh_func(*(uint32_t*)(record + 24));
        uint64_t turnover = le32toh_func(*(uint32_t*)(record + 28)); // Checking if turnover is 32 or 64. Manual says Long (4).
        double ltp = le32toh_func(*(uint32_t*)(record + 36)) / 100.0;
        
        // Additional fields
        uint64_t totalBuy = le32toh_func(*(uint32_t*)(record + 64));
        uint64_t totalSell = le32toh_func(*(uint32_t*)(record + 68));
        double lowerCir = le32toh_func(*(uint32_t*)(record + 76)) / 100.0;
        double upperCir = le32toh_func(*(uint32_t*)(record + 80)) / 100.0;
        double atp = le32toh_func(*(uint32_t*)(record + 84)) / 100.0;
        
        // Depth
        DecodedDepthLevel bids[5];
        DecodedDepthLevel asks[5];
        
        for (int level = 0; level < 5; level++) {
             int bidOffset = 104 + (level * 32);
             int askOffset = bidOffset + 16;
             
             bids[level].price = le32toh_func(*(uint32_t*)(record + bidOffset)) / 100.0;
             bids[level].quantity = le32toh_func(*(uint32_t*)(record + bidOffset + 4));
             bids[level].numOrders = 0; // Not in legacy parse
             
             asks[level].price = le32toh_func(*(uint32_t*)(record + askOffset)) / 100.0;
             asks[level].quantity = le32toh_func(*(uint32_t*)(record + askOffset + 4));
             asks[level].numOrders = 0;
        }

        // Update Store
        store->updateMarketPicture(token, ltp, open, high, low, close, volume, turnover, 0, atp,
                                  totalBuy, totalSell, lowerCir, upperCir, bids, asks, now);
        
        stats.packetsDecoded++;
        
        // Dispatch Callback (Token Only)
        if (recordCallback_ && isEnabled(token)) {
            recordCallback_(token);
        }
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

    // Determine target store
    PriceStore* store = nullptr;
    if (marketSegment_ == 12 || marketSegment_ == 3) {
        store = &g_bseFoPriceStore;
    } else if (marketSegment_ == 11 || marketSegment_ == 2) {
        store = &g_bseCmPriceStore;
    }

    for (uint16_t i = 0; i < numRecords && i < 40; i++) { // Safety cap
        size_t offset = recordStart + (i * OI_RECORD_SIZE);
        if (offset + OI_RECORD_SIZE > length) break;
        
        const uint8_t* record = buffer + offset;
        
        uint32_t token = le32toh_func(*(uint32_t*)(record + 0));
        int64_t openInterest = (int64_t)le64toh_func(*(uint64_t*)(record + 4));
        int64_t openInterestValue = (int64_t)le64toh_func(*(uint64_t*)(record + 12));
        int32_t openInterestChange = (int32_t)le32toh_func(*(uint32_t*)(record + 20));
        
        if (token == 0) continue;
        
        if (store) {
            store->updateOpenInterest(token, openInterest, openInterestChange, now);
        }
        
        stats.packetsDecoded++;
        if (oiCallback_ && isEnabled(token)) oiCallback_(token);
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

    // Determine target store
    PriceStore* store = nullptr;
    if (marketSegment_ == 12 || marketSegment_ == 3) {
        store = &g_bseFoPriceStore;
    } else if (marketSegment_ == 11 || marketSegment_ == 2) {
        store = &g_bseCmPriceStore;
    }

    for (size_t i = 0; i < numRecords; i++) {
        size_t offset = HEADER_SIZE + (i * recordSlotSize);
        if (offset + 8 > length) break;
        
        const uint8_t* record = buffer + offset;
        uint32_t token = le32toh_func(*(uint32_t*)(record + 0));
        double closePrice = le32toh_func(*(uint32_t*)(record + 4)) / 100.0;
        
        if (token == 0) continue;
        
        if (store) {
            store->updateClosePrice(token, closePrice, now);
        }
        
        stats.packetsDecoded++;
        if (closePriceCallback_ && isEnabled(token)) closePriceCallback_(token);
    }
}

void BSEParser::decodeIndex(const uint8_t* buffer, size_t length, ParserStats& stats) {
    // 2012 Index Record
    size_t recordSlotSize = 120; // Estimated/Previous assumption
    size_t numRecords = (length - HEADER_SIZE) / recordSlotSize;
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    // Determine target index store
    IndexStore* store = nullptr;
    if (marketSegment_ == 12 || marketSegment_ == 3) {
        store = &g_bseFoIndexStore;
    } else if (marketSegment_ == 11 || marketSegment_ == 2) {
        store = &g_bseCmIndexStore;
    }

    for (size_t i=0; i<numRecords; i++) {
        size_t offset = HEADER_SIZE + (i * recordSlotSize);
        if (offset + 28 > length) break; 
        const uint8_t* record = buffer + offset;
        
        uint32_t token = le32toh_func(*(uint32_t*)(record + 0));
        double open = le32toh_func(*(uint32_t*)(record + 12)) / 100.0;
        double high = le32toh_func(*(uint32_t*)(record + 16)) / 100.0;
        double low = le32toh_func(*(uint32_t*)(record + 20)) / 100.0;
        double ltp = le32toh_func(*(uint32_t*)(record + 24)) / 100.0;
        double close = le32toh_func(*(uint32_t*)(record + 28)) / 100.0;
        
        double changePerc = 0.0;
        if (close > 0) changePerc = ((ltp - close) / close) * 100.0;
        
        if (store) {
            store->updateIndex(token, nullptr, ltp, open, high, low, close, changePerc, now);
        }
        
        stats.packetsDecoded++;
        if (indexCallback_) indexCallback_(token);
        else if (recordCallback_) recordCallback_(token); // Fallback
    }
}

void BSEParser::decodeImpliedVolatility(const uint8_t* buffer, size_t length, ParserStats& stats) {
    // Message Type 2028
    if (length < 34) return;
    
    uint16_t numRecords = le16toh_func(*(uint16_t*)(buffer + 32));
    size_t recordSize = 72; 
    size_t recordStart = HEADER_SIZE;
    
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    // Determine target store
    PriceStore* store = nullptr;
    if (marketSegment_ == 12 || marketSegment_ == 3) {
        store = &g_bseFoPriceStore;
    } else if (marketSegment_ == 11 || marketSegment_ == 2) {
        store = &g_bseCmPriceStore;
    }

    for (uint16_t i=0; i<numRecords; i++) {
        size_t offset = recordStart + (i * recordSize);
        if (offset + 12 > length) break;
        
        const uint8_t* record = buffer + offset;
        uint32_t token = le32toh_func(*(uint32_t*)(record + 0));
        int64_t impliedVolatility = (int64_t)le64toh_func(*(uint64_t*)(record + 4));
        
        if (token == 0) continue;
        
        if (store) {
            store->updateImpliedVolatility(token, impliedVolatility, now);
        }
        
        stats.packetsDecoded++;
        if (ivCallback_ && isEnabled(token)) ivCallback_(token);
    }
}



void BSEParser::decodeRBIReferenceRate(const uint8_t* buffer, size_t length, ParserStats& stats) {
    if (length < 34) return; // Header + NoOfRecords check

    uint16_t numRecords = le16toh_func(*(uint16_t*)(buffer + 32));
    size_t recordSize = 18; // From manual: Asset(4)+Rate(4)+Res(2)+Res(2)+Date(11)+Filler(1) does not sum to 18?
    // Manual: 
    // Underlying Asset Id (Long) = 4
    // RBI Rate (Long) = 4
    // Reserved 6 (Short) = 2
    // Reserved 7 (Short) = 2
    // Date (Char 11) = 11
    // Filler (Char 1) = 1
    // Total = 4+4+2+2+11+1 = 24 bytes?
    // Let's check manual again. 4.18 RBI Reference Rate.
    // "No. Of Records" (Short) at field 5? 
    // Header is 36 bytes. NoOfRecords is usually in header or after.
    // Manual says: "No. Of Records" is after "Reserved Field 5".
    // 2022 Header: 36 bytes.
    // "No. Of Records" (Short, 2 bytes) -> Total 38 bytes before records.
    // Loop repeats NoOfRecords times.
    // Record Structure:
    // AssetId (4)
    // Rate (4)
    // Res6 (2)
    // Res7 (2)
    // Date (11)
    // Filler (1)
    // Total = 4+4+2+2+11+1 = 24 bytes.
    
    // Safety check on offset
    size_t recordStart = 38; 
    
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (uint16_t i = 0; i < numRecords; i++) {
        size_t offset = recordStart + (i * 24);
        if (offset + 24 > length) break;

        const uint8_t* record = buffer + offset;
        DecodedRBIReferenceRate rbi;
        rbi.packetTimestamp = now;
        
        rbi.underlyingAssetId = le32toh_func(*(uint32_t*)(record + 0));
        rbi.rbiRate = le32toh_func(*(int32_t*)(record + 4));
        
        // Date (11 bytes)
        memcpy(rbi.date, record + 12, 11);
        rbi.date[11] = '\0'; // Null terminate
        
        stats.packetsDecoded++;
        if (rbiCallback_) rbiCallback_(rbi);
    }
}

} // namespace bse
