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

    // Allow multiple sockets to use the same PORT number
    int reuse = 1;
    if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        std::cerr << "Reusing ADDR failed: " << socket_error_string(socket_errno) << std::endl;
        socket_close(sockfd_);
        sockfd_ = socket_invalid;
        return;
    }
    
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
    std::cout << "[" << segment_ << "] Starting receive loop..." << std::endl;
    
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
        
        if (validatePacket(buffer_, n)) {
            stats_.packetsValid++;
            decodeAndDispatch(buffer_, n);
        } else {
            stats_.packetsInvalid++;
        }
    }
}

bool BSEReceiver::validatePacket(const uint8_t* buffer, size_t length) {
    if (length < HEADER_SIZE) return false;
    
    // Check leading zeros (0-3) - Big Endian
    uint32_t leading = be32toh_func(*(uint32_t*)(buffer));
    if (leading != LEADING_ZEROS) return false;
    
    // Check Format ID (4-5) - VERIFIED: Little Endian (Python code confirmed)
    // Format ID = packet size in decimal
    uint16_t formatId = le16toh_func(*(uint16_t*)(buffer + 4));
    if (formatId != length) {
        return false;
    }
    
    // Check Msg Type (8-9) - VERIFIED: Little Endian
    uint16_t msgType = le16toh_func(*(uint16_t*)(buffer + 8));
    if (msgType != MSG_TYPE_MARKET_PICTURE && msgType != MSG_TYPE_MARKET_PICTURE_COMPLEX) {
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
    else return;
    
    // Capture system timestamp (microseconds since epoch)
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Calculate number of records based on packet size
    // Formula: (packet_size - 36) / 264 = number of records
    const size_t RECORD_SLOT_SIZE = 264;
    size_t numRecords = (length - HEADER_SIZE) / RECORD_SLOT_SIZE;
    
    // Process each record
    for (size_t i = 0; i < numRecords; i++) {
        size_t recordOffset = HEADER_SIZE + (i * RECORD_SLOT_SIZE);
        
        // Check if we have enough bytes for this record
        if (recordOffset + RECORD_SLOT_SIZE > length) break;
        
        const uint8_t* record = buffer + recordOffset;
        
        DecodedRecord decRec;
        decRec.packetTimestamp = now;
        
        // ===== VERIFIED OFFSETS (Little-Endian) =====
        
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
        
        // Unknown Field (20-23) - Skip
        // int32_t unknown1 = le32toh_func(*(uint32_t*)(record + 20));
        
        // Volume (24-27) - Little Endian ✓
        decRec.volume = le32toh_func(*(uint32_t*)(record + 24));
        
        // Turnover in Lakhs (28-31) - Little Endian ✓
        // Turnover = Traded Value / 100,000
        decRec.turnover = le32toh_func(*(uint32_t*)(record + 28));
        
        // Lot Size (32-35) - Little Endian ✓
        // uint32_t lotSize = le32toh_func(*(uint32_t*)(record + 32));
        
        // LTP - Last Traded Price (36-39) - Little Endian, paise ✓
        decRec.ltp = le32toh_func(*(uint32_t*)(record + 36));
        
        // Unknown Field (40-43) - Always zero
        // uint32_t unknown2 = le32toh_func(*(uint32_t*)(record + 40));
        
        // Market Sequence Number (44-47) - Little Endian ✓
        // Increments by 1 per tick
        // uint32_t seqNum = le32toh_func(*(uint32_t*)(record + 44));
        
        // ATP - Average Traded Price (84-87) - Little Endian, paise ✓
        decRec.weightedAvgPrice = le32toh_func(*(uint32_t*)(record + 84));
        
        // Best Bid Price (104-107) - Little Endian, paise ✓
        // This is also the first level of the order book
        int32_t bestBidPrice = le32toh_func(*(uint32_t*)(record + 104));
        
        // ===== ORDER BOOK (5 LEVELS) =====
        // Starting at offset 104, interleaved Bid/Ask
        // Each level: Price (4B) + Qty (4B) = 8 bytes
        // Pattern: Bid1, Ask1, Bid2, Ask2, Bid3, Ask3, Bid4, Ask4, Bid5, Ask5
        
        // For now, we'll extract just the best bid/ask
        // Full order book parsing can be added later
        if (recordOffset + 104 + 8 <= length) {
            DecodedDepthLevel bid1;
            bid1.price = le32toh_func(*(uint32_t*)(record + 104));
            bid1.quantity = le32toh_func(*(uint32_t*)(record + 108));
            bid1.numOrders = 0; // Not available in this format
            bid1.impliedQty = 0;
            
            if (bid1.price > 0 && bid1.quantity > 0) {
                decRec.bids.push_back(bid1);
            }
        }
        
        if (recordOffset + 112 + 8 <= length) {
            DecodedDepthLevel ask1;
            ask1.price = le32toh_func(*(uint32_t*)(record + 112));
            ask1.quantity = le32toh_func(*(uint32_t*)(record + 116));
            ask1.numOrders = 0;
            ask1.impliedQty = 0;
            
            if (ask1.price > 0 && ask1.quantity > 0) {
                decRec.asks.push_back(ask1);
            }
        }
        
        // Set close price (using previous close for now)
        decRec.close = prevClose;
        
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

} // namespace bse
