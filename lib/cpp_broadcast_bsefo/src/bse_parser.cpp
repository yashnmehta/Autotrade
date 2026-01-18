#include "bse_parser.h"
#include "bse_utils.h"
#include "bse_price_store.h"
#include <iostream>
#include <chrono>
#include <unordered_map>
#include "services/PriceCacheZeroCopy.h"
#include "utils/PreferencesManager.h"

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

    size_t recordSlotSize = 264; // Fixed slot size per empirical evidence
    size_t numRecords = (length - HEADER_SIZE) / recordSlotSize;
    
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    // =========================================================================
    // CONFIG-DRIVEN EITHER/OR PATH
    // =========================================================================
    bool useLegacy = PreferencesManager::instance().getUseLegacyPriceCache();
    
    // For New Path: Static cache for WriteHandles
    static std::unordered_map<uint32_t, PriceCacheTypes::PriceCacheZeroCopy::WriteHandle> s_handleCache;

    for (size_t i = 0; i < numRecords; i++) {
        size_t recordOffset = HEADER_SIZE + (i * recordSlotSize);
        if (recordOffset + recordSlotSize > length) break;
        
        const uint8_t* record = buffer + recordOffset;
        uint32_t token = le32toh_func(*(uint32_t*)(record + 0));
        if (token == 0) continue;
        
        if (useLegacy) {
            // =================================================================
            // LEGACY PATH: Callbacks with Distributed Store (use_legacy_priceCache = true)
            // =================================================================
            
            DecodedRecord decRec;
            decRec.packetTimestamp = now;
            decRec.token = token;
            
            decRec.open = le32toh_func(*(uint32_t*)(record + 4));
            int32_t prevClose = le32toh_func(*(uint32_t*)(record + 8));
            decRec.high = le32toh_func(*(uint32_t*)(record + 12));
            decRec.low = le32toh_func(*(uint32_t*)(record + 16));
            decRec.volume = le32toh_func(*(uint32_t*)(record + 24)); 
            decRec.turnover = le32toh_func(*(uint32_t*)(record + 28));
            decRec.ltp = le32toh_func(*(uint32_t*)(record + 36));
            
            // Empirically Verified Fields
            decRec.totalBuyQty = le32toh_func(*(uint32_t*)(record + 64));
            decRec.totalSellQty = le32toh_func(*(uint32_t*)(record + 68));
            
            decRec.lowerCircuit = le32toh_func(*(uint32_t*)(record + 76));
            decRec.upperCircuit = le32toh_func(*(uint32_t*)(record + 80));
            
            decRec.weightedAvgPrice = le32toh_func(*(uint32_t*)(record + 84));
            
            // Depth
            for (int level = 0; level < 5; level++) {
                int bidOffset = 104 + (level * 32);
                int askOffset = bidOffset + 16;
                
                if (recordOffset + askOffset + 8 <= length) {
                    DecodedDepthLevel bid = {0}, ask = {0}; // Zero init
                    
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
            
            // STORE IN DISTRIBUTED CACHE FIRST (keeps contract master metadata)
            const DecodedRecord* stored = nullptr;
            if (msgType == MSG_TYPE_MARKET_PICTURE || msgType == MSG_TYPE_MARKET_PICTURE_COMPLEX) {
                // Determine segment: BSE FO (segment 12) or BSE CM (segment 11)
                // Use marketSegment_ if set, otherwise infer from message structure
                if (marketSegment_ == 12 || marketSegment_ == 3) {
                    stored = g_bseFoPriceStore.updateRecord(decRec);
                } else if (marketSegment_ == 11 || marketSegment_ == 2) {
                    stored = g_bseCmPriceStore.updateRecord(decRec);
                }
            }
            
            stats.packetsDecoded++;
            
            // Dispatch callback with stored pointer if available, else original
            if (recordCallback_) {
                recordCallback_(stored ? *stored : decRec);
            }
            
        } else {
            // =================================================================
            // NEW PATH: PriceCache Direct Write (use_legacy_priceCache = false)
            // =================================================================
            
            // Check if segment is set using member variable
            if (marketSegment_ == -1) continue;
            
            // Get or create WriteHandle
            auto it = s_handleCache.find(token);
            if (it == s_handleCache.end()) {
                // Determine segment enum from stored integer
                auto segmentEnum = static_cast<PriceCacheTypes::MarketSegment>(marketSegment_);
                
                auto handle = PriceCacheTypes::PriceCacheZeroCopy::getInstance()
                    .getWriteHandle(token, segmentEnum);
                
                if (!handle.valid) {
                    // Token not in PriceCache
                    continue;
                }
                
                it = s_handleCache.emplace(token, handle).first;
            }
            
            auto& h = it->second;
            
            // Atomic write
            uint64_t seq = h.sequencePtr->fetch_add(1, std::memory_order_relaxed);
            
            // Core Data
            h.dataPtr->lastTradedPrice = le32toh_func(*(uint32_t*)(record + 36)); // Paise
            h.dataPtr->openPrice = le32toh_func(*(uint32_t*)(record + 4));
            h.dataPtr->highPrice = le32toh_func(*(uint32_t*)(record + 12));
            h.dataPtr->lowPrice = le32toh_func(*(uint32_t*)(record + 16));
            h.dataPtr->closePrice = le32toh_func(*(uint32_t*)(record + 8)); // prevClose
            
            h.dataPtr->volumeTradedToday = le32toh_func(*(uint32_t*)(record + 24));
            // h.dataPtr->turnover = le32toh_func(*(uint32_t*)(record + 28)); // No turnover field in ConsolidatedMarketData?
            // Checking PriceCacheZeroCopy.h: It has `valueTraded` or similar? 
            // It has `averageTradePrice`. 
            // Let's check struct.
            
            h.dataPtr->averageTradePrice = le32toh_func(*(uint32_t*)(record + 84));
            
            h.dataPtr->totalBuyQuantity = le32toh_func(*(uint32_t*)(record + 64));
            h.dataPtr->totalSellQuantity = le32toh_func(*(uint32_t*)(record + 68));
            
            h.dataPtr->lowerCircuit = le32toh_func(*(uint32_t*)(record + 76));
            h.dataPtr->upperCircuit = le32toh_func(*(uint32_t*)(record + 80));
            
            // Depth
            for (int level = 0; level < 5; level++) {
                int bidOffset = 104 + (level * 32);
                int askOffset = bidOffset + 16;
                
                h.dataPtr->bidPrice[level] = le32toh_func(*(uint32_t*)(record + bidOffset));
                h.dataPtr->bidQuantity[level] = le64toh_func(*(uint32_t*)(record + bidOffset + 4)); // Cast 32->64
                h.dataPtr->bidOrders[level] = 0; // Not parsed currently
                
                h.dataPtr->askPrice[level] = le32toh_func(*(uint32_t*)(record + askOffset));
                h.dataPtr->askQuantity[level] = le64toh_func(*(uint32_t*)(record + askOffset + 4));
                h.dataPtr->askOrders[level] = 0;
            }
            
            // Complete update
            h.sequencePtr->store(seq + 1, std::memory_order_release);
            
            stats.packetsDecoded++;
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
