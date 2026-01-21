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
    if (msgType == 2020) {
        // std::cout << "2020" << std::endl;
    }
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

    size_t payloadSize = length - HEADER_SIZE;
    size_t recordSlotSize = 0;
    bool isManualFormat = false;

    // Detect format based on payload size modulo
    if (payloadSize > 0) {
        if (payloadSize % 264 == 0) {
            recordSlotSize = 264;
            isManualFormat = false;
        } else if (payloadSize % 416 == 0) {
            recordSlotSize = 416;
            isManualFormat = true;
        } else if (payloadSize % 420 == 0) {
            recordSlotSize = 420; // 2021 Manual Format
            isManualFormat = true;
        } else {
            // Fallback or Unknown - Try to guess based on MsgType
             if (msgType == 2021) recordSlotSize = 420; // Assumption
             else if (msgType == 2020) recordSlotSize = 416; // Assumption
        }
    }

    if (recordSlotSize == 0) return;

    size_t numRecords = payloadSize / recordSlotSize;
    
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    // Determine target store
    PriceStore* store = nullptr;
    if (marketSegment_ == 12 || marketSegment_ == 3) {
        store = &g_bseFoPriceStore;
    } else if (marketSegment_ == 11 || marketSegment_ == 2) {
        store = &g_bseCmPriceStore;
    }

    if (!store) return; 

    for (size_t i = 0; i < numRecords; i++) {
        size_t recordOffset = HEADER_SIZE + (i * recordSlotSize);
        if (recordOffset + recordSlotSize > length) break;
        
        const uint8_t* record = buffer + recordOffset;
        
        uint32_t token = 0;
        double open = 0, close = 0, high = 0, low = 0, ltp = 0, atp = 0;
        double lowerCir = 0, upperCir = 0;
        uint64_t volume = 0, turnover = 0, totalBuy = 0, totalSell = 0, ltq = 0;
        DecodedDepthLevel bids[5] = {};
        DecodedDepthLevel asks[5] = {};

        if (!isManualFormat) {
            // --- LEGACY / EMPIRICAL 264-BYTE FORMAT ---
            token = le32toh_func(*(uint32_t*)(record + 0));
            if (token == 0) continue;
            
            open = le32toh_func(*(uint32_t*)(record + 4)) / 100.0;
            close = le32toh_func(*(uint32_t*)(record + 8)) / 100.0; // Pclose
            high = le32toh_func(*(uint32_t*)(record + 12)) / 100.0;
            low = le32toh_func(*(uint32_t*)(record + 16)) / 100.0;
            volume = le32toh_func(*(uint32_t*)(record + 24));
            turnover = le32toh_func(*(uint32_t*)(record + 28)); 
            ltp = le32toh_func(*(uint32_t*)(record + 36)) / 100.0;
            
            // Additional fields
            totalBuy = le32toh_func(*(uint32_t*)(record + 64));
            totalSell = le32toh_func(*(uint32_t*)(record + 68));
            lowerCir = le32toh_func(*(uint32_t*)(record + 76)) / 100.0;
            upperCir = le32toh_func(*(uint32_t*)(record + 80)) / 100.0;
            atp = le32toh_func(*(uint32_t*)(record + 84)) / 100.0;
            
            // Depth
            for (int level = 0; level < 5; level++) {
                 int bidOffset = 104 + (level * 32); 
                 int askOffset = bidOffset + 16;       
                 
                 bids[level].price = le32toh_func(*(uint32_t*)(record + bidOffset)) / 100.0;
                 bids[level].quantity = le64toh_func(*(uint64_t*)(record + bidOffset + 4));
                 
                 asks[level].price = le32toh_func(*(uint32_t*)(record + askOffset)) / 100.0;
                 asks[level].quantity = le64toh_func(*(uint64_t*)(record + askOffset + 4));
            }
        } 
        else {
            // --- MANUAL V5.0 FORMAT (416/420 Bytes) ---
            // Offsets based on Manual description
            size_t off = 0;
            
            // 1. Instrument Code
            if (recordSlotSize == 420) { // 2021: Long Long
                token = (uint32_t)le64toh_func(*(uint64_t*)(record + off));
                off += 8;
            } else { // 2020: Long
                token = le32toh_func(*(uint32_t*)(record + off));
                off += 4;
            }
            
            if (token == 0) continue;
            
            // 2. No of Trades (Unsigned Long) - Skip
            off += 4;
            
            // 3. Traded Quantity (Long Long) -> Volume
            volume = le64toh_func(*(uint64_t*)(record + off));
            off += 8;
            
            // 4. Traded Value (Long Long) -> Turnover
            turnover = le64toh_func(*(uint64_t*)(record + off));
            off += 8; // Value
            
            // 5. Trade Value Flag (Char)
            // 6. Res (Char) x3
            off += 4; 
            
            // 7. Mkt Type (Short) + Session (Short)
            off += 4;
            
            // 8. Time (8 bytes total: 1+1+1+3 + 2 res)
            off += 8;
            
            // 9. Res (2) + Res (8) + NoPricePoints (2)
            off += 12;
            
            // 10. Timestamp (8)
            off += 8;
            
            // NOW AT DATA BLOCK assumption (Offset ~60-64 depending on start)
            // Let's count from 0 again to be sure using explicit offsets relative to start
            // If Token is 4 bytes (2020):
            // 0: Token(4)
            // 4: NoTrades(4)
            // 8: Vol(8)
            // 16: Turn(8)
            // 24: Float/Res(4)
            // 28: Mkt/Sess(4)
            // 32: Time(8)
            // 40: Res(12) -> 52
            // 52: Timestamp(8) -> 60
            
            // Field Order from Manual after Timestamp:
            // CloseRate (4)
            // LTQ (8)
            // LTP (4)
            // Open (4)
            // PrevClose (4)
            // High (4)
            // Low (4)
            
            int baseOff = (recordSlotSize == 420) ? 64 : 60;
            
            // Close (This might be Day Close if session ended, manual says "Close Price for current trading day... else 0")
            // So we treat this as Close? Or PrevClose? Manual has separate "Previous Close Rate" later.
            // Let's use this as 'Close' if non-zero, or ignore? 
            // We usually want PrevClose for the dashboard 'Close' column until market close.
            // Let's read PrevClose later.
            
            // Skip Close (4)
            
            // LTQ (8)
            ltq = le64toh_func(*(uint64_t*)(record + baseOff + 4));
            
            // LTP (4)
            ltp = le32toh_func(*(int32_t*)(record + baseOff + 12)) / 100.0;
            
            // Open (4)
            open = le32toh_func(*(int32_t*)(record + baseOff + 16)) / 100.0;
            
            // Prev Close (4)
            close = le32toh_func(*(int32_t*)(record + baseOff + 20)) / 100.0;
            
            // High (4)
            high = le32toh_func(*(int32_t*)(record + baseOff + 24)) / 100.0;
            
            // Low (4)
            low = le32toh_func(*(int32_t*)(record + baseOff + 28)) / 100.0;
            
            // Skip BlockRef(4), IndEq(4), IndEqQty(8) -> 16 bytes
            // Total Bid (8)
            totalBuy = le64toh_func(*(uint64_t*)(record + baseOff + 48));
            
            // Total Ask (8)
            totalSell = le64toh_func(*(uint64_t*)(record + baseOff + 56));
            
            // Lower Circuit (4)
            lowerCir = le32toh_func(*(int32_t*)(record + baseOff + 64)) / 100.0;
            
            // Upper Circuit (4)
            upperCir = le32toh_func(*(int32_t*)(record + baseOff + 68)) / 100.0;
            
            // ATP (Weighted Avg) (4)
            atp = le32toh_func(*(int32_t*)(record + baseOff + 72)) / 100.0;
            
            // Depth Starts after WAP.
            // Offset so far: baseOff + 76.
            // 2020: 60 + 76 = 136. Matches estimation.
            int depthStart = baseOff + 76;
            
            for (int level = 0; level < 5; level++) {
                int levelOff = depthStart + (level * 56); // 56 bytes per level
                
                // Bid: Rate(4), Qty(8), Ord(4), Imp(8), Res(4) = 28 bytes
                bids[level].price = le32toh_func(*(int32_t*)(record + levelOff)) / 100.0;
                bids[level].quantity = le64toh_func(*(uint64_t*)(record + levelOff + 4));
                bids[level].numOrders = le32toh_func(*(uint32_t*)(record + levelOff + 12));
                
                // Ask: Starts at +28
                asks[level].price = le32toh_func(*(int32_t*)(record + levelOff + 28)) / 100.0;
                asks[level].quantity = le64toh_func(*(uint64_t*)(record + levelOff + 32));
                asks[level].numOrders = le32toh_func(*(uint32_t*)(record + levelOff + 40));
            }
        }

        //Update Store
        store->updateMarketPicture(token, ltp, open, high, low, close, volume, turnover, ltq, atp,
                                  totalBuy, totalSell, lowerCir, upperCir, bids, asks, now);
        
        stats.packetsDecoded++;
        
        // Dispatch Callback (Token Only)
        static int debugCount = 0;
        bool enabled = isEnabled(token);
        bool hasCallback = recordCallback_ != nullptr;
        
        if (debugCount++ < 20) {
            std::cout << "[BSE Parser] Decoded Token: " << token 
                      << " Fmt: " << (isManualFormat ? "MANUAL" : "LEGACY")
                      << " LTP: " << ltp
                      << " Size: " << recordSlotSize
                      << std::endl;
        }
        
        if (recordCallback_ && enabled) {
            std::cout << "--------------------------------------------------" << std::endl;
            std::cout << "[BSE Parser DEBUG] Subscribed Token Found: " << token << std::endl;
            std::cout << "MsgType: " << msgType << " Format: " << (isManualFormat ? "MANUAL (V5.0)" : "LEGACY") << std::endl;
            std::cout << "LTP: " << ltp << " Volume: " << volume << " Turnover: " << turnover << std::endl;
            std::cout << "Open: " << open << " High: " << high << " Low: " << low << " Close: " << close << std::endl;
            std::cout << "ATP: " << atp << " LTQ: " << ltq << std::endl;
            std::cout << "Total Buy: " << totalBuy << " Total Sell: " << totalSell << std::endl;
            std::cout << "Lower Cir: " << lowerCir << " Upper Cir: " << upperCir << std::endl;
            std::cout << "Create Time: " << now << std::endl;
            
            std::cout << "Best 5 Bids:" << std::endl;
            for(int k=0; k<5; k++) {
                if(bids[k].price > 0 || bids[k].quantity > 0)
                     std::cout << "  Bid[" << k << "] P: " << bids[k].price << " Q: " << bids[k].quantity << std::endl;
            }
            
            std::cout << "Best 5 Asks:" << std::endl;
            for(int k=0; k<5; k++) {
                 if(asks[k].price > 0 || asks[k].quantity > 0)
                      std::cout << "  Ask[" << k << "] P: " << asks[k].price << " Q: " << asks[k].quantity << std::endl;
            }
            std::cout << "--------------------------------------------------" << std::endl;

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
    // 2012 Index Record - Manual V5.0 Section 4.12
    size_t recordSlotSize = 40; // Manual: 4+4+4+4+4+4+7+1+1+1+2+2+2 = 40 bytes
    if ((length - HEADER_SIZE) < recordSlotSize) return;

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
        if (offset + recordSlotSize > length) break; 
        const uint8_t* record = buffer + offset;
        
        // Manual Offsets:
        // 0: Index Code (4)
        // 4: High (4)
        // 8: Low (4)
        // 12: Open (4)
        // 16: Prev Close (4)
        // 20: Index Value / LTP (4)
        // 24: Index ID (7 chars)
        
        uint32_t token = le32toh_func(*(uint32_t*)(record + 0));
        
        double high = le32toh_func(*(int32_t*)(record + 4)) / 100.0;
        double low = le32toh_func(*(int32_t*)(record + 8)) / 100.0;
        double open = le32toh_func(*(int32_t*)(record + 12)) / 100.0;
        double close = le32toh_func(*(int32_t*)(record + 16)) / 100.0; // Pclose
        double ltp = le32toh_func(*(int32_t*)(record + 20)) / 100.0;
        
        // Extract Name (Index ID)
        char name[8];
        std::memcpy(name, record + 24, 7);
        name[7] = '\0'; // Null terminate
        
        double changePerc = 0.0;
        if (close > 0) changePerc = ((ltp - close) / close) * 100.0;
        
        if (store) {
            store->updateIndex(token, name, ltp, open, high, low, close, changePerc, now);
        }
        
        stats.packetsDecoded++;
        
        // Debug Log first few
        static int debugCount = 0;
        if (debugCount++ < 5) {
             std::cout << "[BSE Index] Token: " << token << " Name: " << name 
                       << " LTP: " << ltp << " PCl: " << close << std::endl;
        }

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
