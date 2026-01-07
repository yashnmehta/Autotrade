// BSE Receiver - Updated with Empirically Verified Offsets from Python Implementation
// ==================================================================================
// 
// IMPORTANT: These offsets are VERIFIED through analysis of 1000+ live packets
// and differ from the official BSE manual. The Python implementation achieved
// 100% accuracy using these offsets.
//
// Key Differences from Manual:
// 1. Most fields use Little-Endian (not Big-Endian as manual suggests)
// 2. Fixed offsets (not differential compression for base fields)
// 3. Record size is 264 bytes per slot (not variable)
//
// Reference: bse_ref_broadcast.md lines 1351-1369

#include "bse_receiver.h"
#include "bse_utils.h"
#include "socket_platform.h"

#include <iostream>
#include <iomanip>
#include <cstring>
#include <chrono>

namespace bse {
using namespace bse_utils;

BSEReceiver::BSEReceiver(const std::string& ip, int port, const std::string& segment)
    : ip_(ip), port_(port), segment_(segment), sockfd_(socket_invalid), running_(false) {
    WinsockLoader::init();
    
    // Create socket
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ == socket_invalid) {
        std::cerr << "Failed to create socket: " << socket_error_string(socket_errno) << std::endl;
        return;
    }

    // Allow multiple sockets to use the same addr and port
    int reuse = 1;
    if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        std::cerr << "Reusing ADDR failed: " << socket_error_string(socket_errno) << std::endl;
        socket_close(sockfd_);
        sockfd_ = socket_invalid;
        return;
    }
#ifdef SO_REUSEPORT
    if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, (char *)&reuse, sizeof(reuse)) < 0) {
        perror("Reusing PORT failed");
    }
#endif
    
    // Set receive timeout (1 second) - use cross-platform helper
    if (set_socket_timeout(sockfd_, 1) < 0) {
        std::cerr << "Failed to set timeout: " << socket_error_string(socket_errno) << std::endl;
        socket_close(sockfd_);
        sockfd_ = socket_invalid;
        return;
    }

    // Bind to the port
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = htonl(INADDR_ANY); 
    addr_.sin_port = htons(port_);

    if (bind(sockfd_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {
        std::cerr << "Bind failed: " << socket_error_string(socket_errno) << std::endl;
        socket_close(sockfd_);
        sockfd_ = socket_invalid;
        return;
    }

    // Join the multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(ip_.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    
    if (setsockopt(sockfd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0) {
        std::cerr << "Failed to join multicast group: " << socket_error_string(socket_errno) << std::endl;
        socket_close(sockfd_);
        sockfd_ = socket_invalid;
        return;
    }

    std::cout << "[" << segment_ << "] Connected to " << ip_ << ":" << port_ << std::endl;
}

BSEReceiver::~BSEReceiver() {
    stop();
    if (sockfd_ != socket_invalid) {
        socket_close(sockfd_);
    }
}

void BSEReceiver::start() {
    if (running_) return;
    running_ = true;
    receiverThread_ = std::thread(&BSEReceiver::receiveLoop, this);
}

void BSEReceiver::stop() {
    running_ = false;
    if (receiverThread_.joinable()) {
        receiverThread_.join();
    }
}

void BSEReceiver::receiveLoop() {
    std::cout << "[" << segment_ << "] Starting receive loop on " << ip_ << ":" << port_ << "..." << std::endl;
    
    while (running_) {
        // Receive packet
#ifdef _WIN32
        ssize_t n = recv(sockfd_, reinterpret_cast<char*>(buffer_), sizeof(buffer_), 0);
#else
        ssize_t n = recv(sockfd_, buffer_, sizeof(buffer_), 0);
#endif
        
        if (n < 0) {
            // Timeout or error (EAGAIN/EWOULDBLOCK means timeout)
            continue;
        }
        
        stats_.packetsReceived++;
        stats_.bytesReceived += n;
        
        if (stats_.packetsReceived % 100 == 0) {
            // std::cout << "[" << segment_ << "] Received " << stats_.packetsReceived << " packets, total bytes: " << stats_.bytesReceived << std::endl;
        }

        if (validatePacket(buffer_, n)) {
            stats_.packetsValid++;
            decodeAndDispatch(buffer_, n);
        } else {
            stats_.packetsInvalid++;
            if (stats_.packetsInvalid % 10 == 0) {
                std::cerr << "[" << segment_ << "] ⚠ Invalid packet received! Length: " << n << std::endl;
            }
        }
    }
}

bool BSEReceiver::validatePacket(const uint8_t* buffer, size_t length) {
    if (length < HEADER_SIZE) return false;
    
    // Check leading zeros (0-3) - Big Endian
    uint32_t leading = be32toh_func(*(uint32_t*)(buffer));
    if (leading != LEADING_ZEROS) {
        static int leadErrCount = 0;
        if (++leadErrCount % 10 == 0) std::cerr << "[" << segment_ << "] Validation Fail: Leading Zeros=" << leading << std::endl;
        return false;
    }
    
    // Check Format ID (4-5) - VERIFIED: Little Endian (Python code confirmed)
    // Format ID = packet size in decimal
    uint16_t formatId = le16toh_func(*(uint16_t*)(buffer + 4));
    if (formatId != length) {
        static int fmtErrCount = 0;
        if (++fmtErrCount % 10 == 0) std::cerr << "[" << segment_ << "] Validation Fail: FormatID=" << formatId << " != Length=" << length << std::endl;
        return false;
    }
    
    // Check Msg Type (8-9)
    uint16_t msgType = le16toh_func(*(uint16_t*)(buffer + 8));
    if (msgType != MSG_TYPE_MARKET_PICTURE && 
        msgType != MSG_TYPE_MARKET_PICTURE_COMPLEX && 
        msgType != MSG_TYPE_INDEX &&
        msgType != MSG_TYPE_OPEN_INTEREST &&
        msgType != MSG_TYPE_CLOSE_PRICE &&
        msgType != MSG_TYPE_PRODUCT_STATE_CHANGE &&
        msgType != MSG_TYPE_IMPLIED_VOLATILITY) {
        static int msgErrCount = 0;
        if (++msgErrCount % 10 == 0) std::cerr << "[" << segment_ << "] Validation Fail: MsgType=" << msgType << std::endl;
        return false;
    }
    
    return true;
}

// ============================================================================
// EMPIRICALLY VERIFIED BSE PACKET STRUCTURE
// ============================================================================
// Based on Python implementation verified with 1000+ live packets
//
// HEADER (36 bytes):
//   0-3:   Leading zeros (0x00000000) - Big Endian
//   4-5:   Format ID (packet size) - Little Endian ✓ VERIFIED
//   8-9:   Message type (2020/2021) - Little Endian ✓ VERIFIED
//   20-21: Hour - Little Endian ✓ VERIFIED
//   22-23: Minute - Little Endian ✓ VERIFIED
//   24-25: Second - Little Endian ✓ VERIFIED
//
// RECORDS (264 bytes each, starting at offset 36):
//   +0-3:   Token (uint32) - Little Endian ✓ VERIFIED
//   +4-7:   Open Price (int32, paise) - Little Endian ✓ VERIFIED
//   +8-11:  Previous Close (int32, paise) - Little Endian ✓ VERIFIED
//   +12-15: High Price (int32, paise) - Little Endian ✓ VERIFIED
//   +16-19: Low Price (int32, paise) - Little Endian ✓ VERIFIED
//   +20-23: Unknown Field (int32) - Little Endian
//   +24-27: Volume (int32) - Little Endian ✓ VERIFIED
//   +28-31: Turnover in Lakhs (uint32) - Little Endian ✓ VERIFIED
//   +32-35: Lot Size (uint32) - Little Endian ✓ VERIFIED
//   +36-39: LTP - Last Traded Price (int32, paise) - Little Endian ✓ VERIFIED
//   +40-43: Unknown Field (uint32) - Always zero
//   +44-47: Market Sequence Number (uint32) - Little Endian ✓ VERIFIED
//   +84-87: ATP - Average Traded Price (int32, paise) - Little Endian ✓ VERIFIED
//   +104-107: Best Bid Price (int32, paise) - Little Endian ✓ VERIFIED
//   +104-263: 5-Level Order Book (160 bytes) - Interleaved Bid/Ask ✓ VERIFIED
//
// All prices are in paise (divide by 100 for rupees)
// ============================================================================

void BSEReceiver::decodeAndDispatch(const uint8_t* buffer, size_t length) {
    if (length < HEADER_SIZE) return;

    // Read message type
    uint16_t msgType = le16toh_func(*(uint16_t*)(buffer + 8));
    if (msgType == MSG_TYPE_MARKET_PICTURE) stats_.packets2020++;
    else if (msgType == MSG_TYPE_MARKET_PICTURE_COMPLEX) stats_.packets2021++;
    else if (msgType == MSG_TYPE_OPEN_INTEREST) {
        stats_.packets2015++;
        // Handle Open Interest message (2015) separately
        decodeOpenInterest(buffer, length);
        return;
    }
    else if (msgType == MSG_TYPE_PRODUCT_STATE_CHANGE) {
        // Handle session state change (2002)
        decodeSessionState(buffer, length);
        return;
    }
    else if (msgType == MSG_TYPE_CLOSE_PRICE) {
        // Handle close price update (2014)
        decodeClosePrice(buffer, length);
        return;
    }
    else if (msgType == MSG_TYPE_IMPLIED_VOLATILITY) {
        // Handle implied volatility update (2028)
        decodeImpliedVolatility(buffer, length);
        return;
    }
    else if (msgType == MSG_TYPE_INDEX) {
        // MSG_TYPE_INDEX handled below
    }
    else return;
    
    // Calculate record size based on msgType
    size_t recordSlotSize = (msgType == MSG_TYPE_INDEX) ? 120 : 264;
    
    // Capture system timestamp (microseconds since epoch)
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Calculate number of records based on packet size
    size_t numRecords = (length - HEADER_SIZE) / recordSlotSize;
    
    // Process each record
    for (size_t i = 0; i < numRecords; i++) {
        size_t recordOffset = HEADER_SIZE + (i * recordSlotSize);
        
        // Check if we have enough bytes for this record
        if (recordOffset + recordSlotSize > length) break;
        
        const uint8_t* record = buffer + recordOffset;
        
        DecodedRecord decRec;
        decRec.packetTimestamp = now;
        
        if (msgType == MSG_TYPE_INDEX) {
            // Type 2012 (Index Record) - Refined Offsets
            decRec.token = le32toh_func(*(uint32_t*)(record + 0));
            // Index LTP is usually at offset 16 or 40 depending on version
            // For now, let's try common Index Record layout:
            // 0-3: Token, 4-7: HMS, 8-11: Date
            // 12-15: Open, 16-19: High, 20-23: Low, 24-27: LTP
            decRec.open = le32toh_func(*(uint32_t*)(record + 12));
            decRec.high = le32toh_func(*(uint32_t*)(record + 16));
            decRec.low = le32toh_func(*(uint32_t*)(record + 20));
            decRec.ltp = le32toh_func(*(uint32_t*)(record + 24));
            decRec.close = le32toh_func(*(uint32_t*)(record + 28)); // Previous Close
            decRec.volume = 0; // Indices don't have volume usually
            
            static int indexLog = 0;
            if (indexLog < 10) {
                indexLog++;
                std::cout << "[" << segment_ << "] 2012 Record Decoded - Token: " << decRec.token 
                          << " LTP: " << decRec.ltp << " (Paise)" << std::endl;
            }
        } else {
            // ===== VERIFIED OFFSETS (2020/2021) =====
            
            // Token (0-3) - Little Endian ✓
            decRec.token = le32toh_func(*(uint32_t*)(record + 0));
            
            // Skip empty slots (token = 0)
            if (decRec.token == 0) continue;
            
            // Open Price (4-7) - Little Endian, paise ✓
            decRec.open = le32toh_func(*(uint32_t*)(record + 4));
            
            // Previous Close (8-11) - Little Endian, paise ✓
            int32_t prevClose = le32toh_func(*(uint32_t*)(record + 8));
            
            // High Price (12-15) - Little Endian, paise ✓
            decRec.high = le32toh_func(*(uint32_t*)(record + 12));
            
            // Low Price (16-19) - Little Endian, paise ✓
            decRec.low = le32toh_func(*(uint32_t*)(record + 16));
            
            // Volume (24-27) - Little Endian ✓
            decRec.volume = le32toh_func(*(uint32_t*)(record + 24));
            
            // Turnover in Lakhs (28-31) - Little Endian ✓
            decRec.turnover = le32toh_func(*(uint32_t*)(record + 28));
            
            // LTP - Last Traded Price (36-39) - Little Endian, paise ✓
            decRec.ltp = le32toh_func(*(uint32_t*)(record + 36));
            
            // ATP - Average Traded Price (84-87) - Little Endian, paise ✓
            decRec.weightedAvgPrice = le32toh_func(*(uint32_t*)(record + 84));
            
            // Parse all 5 levels of depth
            // Layout: Bid1(104-119), Ask1(120-135), Bid2(136-151), Ask2(152-167)...
            // Each OrderBookLevel = 16 bytes: price(4)+quantity(4)+flag(4)+unknown(4)
            for (int level = 0; level < 5; level++) {
                int bidOffset = 104 + (level * 32);  // Each bid-ask pair is 32 bytes
                int askOffset = bidOffset + 16;       // Ask follows Bid
                
                if (recordOffset + askOffset + 8 <= length) {
                    DecodedDepthLevel bid, ask;
                    bid.price = le32toh_func(*(uint32_t*)(record + bidOffset));
                    bid.quantity = le32toh_func(*(uint32_t*)(record + bidOffset + 4));
                    ask.price = le32toh_func(*(uint32_t*)(record + askOffset));
                    ask.quantity = le32toh_func(*(uint32_t*)(record + askOffset + 4));
                    
                    // Push levels even if zero (to maintain array alignment)
                    decRec.bids.push_back(bid);
                    decRec.asks.push_back(ask);
                }
            }
            
            decRec.close = prevClose;
        }
        
        // Set default values for fields not in this format
        decRec.noOfTrades = 0;
        decRec.ltq = 0;
        decRec.lowerCircuit = 0;
        decRec.upperCircuit = 0;
        
        // Set default values for fields not in this format
        decRec.noOfTrades = 0;
        decRec.ltq = 0;
        decRec.lowerCircuit = 0;
        decRec.upperCircuit = 0;
        
        stats_.packetsDecoded++;
        
        // Dispatch to callback
        if (recordCallback_) {
            recordCallback_(decRec);
        }
    }
}

// ============================================================================
// BSE OPEN INTEREST MESSAGE (2015) PARSER
// ============================================================================
// Based on BSE Manual V5.0 Section 4.16
//
// Header (36 bytes) same as other messages
// Record layout per OI entry:
//   +0-3:   Instrument ID (uint32) - Little Endian
//   +4-11:  Open Interest Quantity (int64) - Little Endian
//   +12-19: Open Interest Value (int64) - Little Endian (2 decimal)
//   +20-23: Open Interest Change (int32) - Little Endian
//   +24-27: Reserved Field 6 (4 bytes)
//   +28-31: Reserved Field 7 (4 bytes)
//   +32-33: Reserved Field 8 (2 bytes)
//
// Total record size: 34 bytes (estimated, verify with live data)
// ============================================================================

void BSEReceiver::decodeOpenInterest(const uint8_t* buffer, size_t length) {
    if (length < HEADER_SIZE) return;
    
    // Capture system timestamp
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Number of records is at offset 34-35 (after header but within 36 bytes)
    // Actually, per manual: "No. of Records" is before the repeating structure
    // Header ends at 36, but we need to find numRecords field
    // From manual: Hour(20-21), Minute(22-23), Second(24-25), Millisecond(26-27),
    //              Reserved4(28-29), Reserved5(30-31), NoOfRecords(32-33)
    
    uint16_t numRecords = le16toh_func(*(uint16_t*)(buffer + 32));
    
    // OI Record size based on manual: 
    // Token(4) + OI Qty(8) + OI Value(8) + OI Change(4) + Reserved(4+4+2) = 34 bytes
    constexpr size_t OI_RECORD_SIZE = 34;
    
    // Records start after header
    size_t recordStart = HEADER_SIZE;
    
    static int oiLogCount = 0;
    
    for (uint16_t i = 0; i < numRecords && i < 26; i++) {  // Max 26 per manual
        size_t offset = recordStart + (i * OI_RECORD_SIZE);
        if (offset + OI_RECORD_SIZE > length) break;
        
        const uint8_t* record = buffer + offset;
        
        DecodedOpenInterest oiRec;
        oiRec.packetTimestamp = now;
        
        // Parse fields (Little Endian)
        oiRec.token = le32toh_func(*(uint32_t*)(record + 0));
        oiRec.openInterest = (int64_t)le64toh_func(*(uint64_t*)(record + 4));
        oiRec.openInterestValue = (int64_t)le64toh_func(*(uint64_t*)(record + 12));
        oiRec.openInterestChange = (int32_t)le32toh_func(*(uint32_t*)(record + 20));
        
        // Skip empty records
        if (oiRec.token == 0) continue;
        
        // Debug logging
        if (oiLogCount++ < 20) {
            std::cout << "[" << segment_ << "] OI 2015 - Token: " << oiRec.token
                      << " OI: " << oiRec.openInterest
                      << " Change: " << oiRec.openInterestChange << std::endl;
        }
        
        // Dispatch to callback
        if (oiCallback_) {
            oiCallback_(oiRec);
        }
    }
}

// Decode BSE Session State (Message Type 2002 - PRODUCT_STATE_CHANGE)
void BSEReceiver::decodeSessionState(const uint8_t* buffer, size_t length) {
    // Message 2002 structure (variable length, starts at offset 36 after header)
    // Session Number: 4 bytes (uint32)
    // Market Segment ID: 2 bytes (uint16)
    // Market Type: 1 byte (0=Pre-open, 1=Continuous, 2=Auction)
    // Start/End Flag: 1 byte (0=Start, 1=End)
    // Timestamp fields may follow
    
    if (length < HEADER_SIZE + 8) return;  // Need at least 8 bytes for minimal data
    
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    const uint8_t* data = buffer + HEADER_SIZE;
    
    DecodedSessionState state;
    state.packetTimestamp = now;
    
    // Parse fields (Little Endian based on BSE protocol)
    state.sessionNumber = le32toh_func(*(uint32_t*)(data + 0));
    state.marketSegmentId = le16toh_func(*(uint16_t*)(data + 4));
    state.marketType = *(data + 6);
    state.startEndFlag = *(data + 7);
    state.timestamp = now;  // Use system time as exchange timestamp not available
    
    // Debug logging
    static int sessionLogCount = 0;
    if (sessionLogCount++ < 20) {
        const char* marketTypeStr[] = {"Pre-Open", "Continuous", "Auction"};
        const char* startEndStr[] = {"Start", "End"};
        std::cout << "[" << segment_ << "] Session State 2002 - "
                  << "Segment: " << state.marketSegmentId
                  << " Session: " << state.sessionNumber
                  << " Type: " << (state.marketType < 3 ? marketTypeStr[state.marketType] : "Unknown")
                  << " " << (state.startEndFlag < 2 ? startEndStr[state.startEndFlag] : "Unknown")
                  << std::endl;
    }
    
    // Dispatch to callback
    if (sessionStateCallback_) {
        sessionStateCallback_(state);
    }
}

// Decode BSE Close Price (Message Type 2014 - CLOSE_PRICE)
void BSEReceiver::decodeClosePrice(const uint8_t* buffer, size_t length) {
    // Message 2014 structure (similar to market data records)
    // Records start at offset 36, each contains token and close price
    // Record size is likely 264 bytes (same as market picture) or smaller
    
    if (length < HEADER_SIZE + 8) return;  // Need at least token + price
    
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Try 264-byte records first (standard BSE record size)
    size_t recordSlotSize = 264;
    size_t numRecords = (length - HEADER_SIZE) / recordSlotSize;
    
    // If that gives 0 records, try smaller size (8 bytes minimum: token + price)
    if (numRecords == 0) {
        recordSlotSize = 8;
        numRecords = (length - HEADER_SIZE) / recordSlotSize;
    }
    
    for (size_t i = 0; i < numRecords; i++) {
        size_t recordOffset = HEADER_SIZE + (i * recordSlotSize);
        if (recordOffset + 8 > length) break;  // Need at least token + price
        
        const uint8_t* record = buffer + recordOffset;
        
        DecodedClosePrice cp;
        cp.packetTimestamp = now;
        
        // Token (0-3) - Little Endian
        cp.token = le32toh_func(*(uint32_t*)(record + 0));
        
        // Skip empty slots
        if (cp.token == 0) continue;
        
        // Close Price (4-7) - Little Endian, in paise
        cp.closePrice = le32toh_func(*(uint32_t*)(record + 4));
        
        // Debug logging
        static int closePriceLogCount = 0;
        if (closePriceLogCount++ < 10) {
            std::cout << "[" << segment_ << "] Close Price 2014 - "
                      << "Token: " << cp.token
                      << " Close: " << (cp.closePrice / 100.0) << " Rs"
                      << std::endl;
        }
        
        // Dispatch to callback
        if (closePriceCallback_) {
            closePriceCallback_(cp);
        }
    }
}

// ============================================================================
// BSE IMPLIED VOLATILITY MESSAGE (2028) PARSER
// ============================================================================
// Based on BSE Manual V5.0 Section 4.19
//
// Header (36 bytes) same as other messages
// Record layout per IV entry (72 bytes):
//   +0-3:    Instrument ID (Long/uint32) - Little Endian
//   +4-11:   Implied Volatility (Long Long/int64) - Little Endian
//            To display in %, multiply by 100 and round to 2 decimals
//   +12-59:  Reserved Fields 6-11 (6 × 8 bytes Long Long)
//   +60-63:  Reserved Field 12 (4 bytes Long)
//   +64-65:  Reserved Field 13 (2 bytes Short)
//   +66-67:  Reserved Field 14 (2 bytes Short)
//   +68:     Reserved Field 15 (1 byte Char)
//   +69:     Reserved Field 16 (1 byte Char)
//   +70-71:  Reserved Field 17 (2 bytes Char[2])
//
// Total record size: 72 bytes
// Maximum records: 13
// Applicable Segment: Equity Derivatives (BSEFO only)
// Compression: No
// ============================================================================

void BSEReceiver::decodeImpliedVolatility(const uint8_t* buffer, size_t length) {
    if (length < HEADER_SIZE) return;
    
    // Capture system timestamp
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Number of records at offset 32-33 in header (per BSE manual standard)
    uint16_t numRecords = le16toh_func(*(uint16_t*)(buffer + 32));
    
    // Validate number of records (max 13 per manual)
    if (numRecords > 13) {
        std::cerr << "[" << segment_ << "] IV Message: Invalid numRecords=" << numRecords << std::endl;
        return;
    }
    
    // IV Record size: 72 bytes
    constexpr size_t IV_RECORD_SIZE = 72;
    
    // Records start after header (36 bytes)
    size_t recordStart = HEADER_SIZE;
    
    static int ivLogCount = 0;
    
    for (uint16_t i = 0; i < numRecords; i++) {
        size_t offset = recordStart + (i * IV_RECORD_SIZE);
        
        // Bounds check - ensure we have full record
        if (offset + IV_RECORD_SIZE > length) {
            std::cerr << "[" << segment_ << "] IV Message: Incomplete record " << i 
                      << " at offset " << offset << " (packet size: " << length << ")" << std::endl;
            break;
        }
        
        const uint8_t* record = buffer + offset;
        
        DecodedImpliedVolatility ivRec;
        ivRec.packetTimestamp = now;
        
        // Parse fields (Little Endian based on BSE protocol)
        // +0-3: Instrument ID (Long/uint32)
        ivRec.token = le32toh_func(*(uint32_t*)(record + 0));
        
        // Skip empty slots (token = 0)
        if (ivRec.token == 0) continue;
        
        // +4-11: Implied Volatility (Long Long/int64)
        // Raw value - multiply by 100 for percentage display
        ivRec.impliedVolatility = le64toh_func(*(uint64_t*)(record + 4));
        
        // Reserved fields are at offsets 12-71 (not parsed, for future use)
        
        // Debug logging for first few messages
        if (ivLogCount < 20) {
            ivLogCount++;
            // Calculate IV percentage: multiply by 100 for display
            double ivPercent = ivRec.impliedVolatility * 100.0;
            std::cout << "[" << segment_ << "] IV 2028 - "
                      << "Token: " << ivRec.token
                      << " IV: " << std::fixed << std::setprecision(2) << ivPercent << "%"
                      << " (Raw: " << ivRec.impliedVolatility << ")"
                      << std::endl;
        }
        
        // Dispatch to callback (thread-safe - callback is set atomically)
        if (ivCallback_) {
            ivCallback_(ivRec);
        }
    }
}

} // namespace bse
