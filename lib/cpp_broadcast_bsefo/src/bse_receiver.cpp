#include "bse_receiver.h"
#include "bse_utils.h"

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>

namespace bse {

BSEReceiver::BSEReceiver(const std::string& ip, int port, const std::string& segment)
    : ip_(ip), port_(port), segment_(segment), sockfd_(-1), running_(false) {
    
    // Create socket
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    // Allow multiple sockets to use the same PORT number
    int reuse = 1;
    if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        perror("Reusing ADDR failed");
        close(sockfd_);
        sockfd_ = -1;
        return;
    }
    
    // Set receive timeout (1 second)
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    // Bind to the port
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = htonl(INADDR_ANY); 
    addr_.sin_port = htons(port_);

    if (bind(sockfd_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {
        perror("Bind failed");
        close(sockfd_);
        sockfd_ = -1;
        return;
    }

    // Join the multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(ip_.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    
    if (setsockopt(sockfd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0) {
        perror("mreq");
        close(sockfd_);
        sockfd_ = -1;
        return;
    }

    std::cout << "[" << segment_ << "] Connected to " << ip_ << ":" << port_ << std::endl;
}

BSEReceiver::~BSEReceiver() {
    stop();
    if (sockfd_ >= 0) {
        close(sockfd_);
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
        ssize_t n = recv(sockfd_, buffer_, sizeof(buffer_), 0);
        
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
    
    // Check Format ID (4-5) - Big Endian
    // "Format ID = packet size in decimal!"
    uint16_t formatId = be16toh_func(*(uint16_t*)(buffer + 4));
    
    // NOTE: The Python code comment says "Bytes 4-5 in packet: 0x2C01 ... actual bytes [0x2C, 0x01] ... Read as LITTLE-ENDIAN: 0x012C = 300".
    // Wait, the Python code calls `struct.unpack('<H', packet[4:6])` (Little Endian) and says "Format ID (LE): 300".
    // 300 dec = 0x012C.
    // LE representation of 0x012C is [0x2C, 0x01].
    // If the bytes are [0x2C, 0x01], then Little Endian read gives 0x012C (300).
    // The Python code changed its mind:
    // "Extract Format ID (offset 4-5, LITTLE-ENDIAN!!!)"
    // So I should read it as LITTLE ENDIAN too.
    
    uint16_t formatIdLE = le16toh_func(*(uint16_t*)(buffer + 4));
    if (formatIdLE != length) {
        // Try Big Endian just in case logic matches 
        // But prompt implies LE.
        return false;
    }
    
    // Check Msg Type (8-9) - Little Endian
    uint16_t msgType = le16toh_func(*(uint16_t*)(buffer + 8));
    if (msgType == MSG_TYPE_MARKET_PICTURE) {
        // stats_.packets2020++; // Can't access non-const stats here easily if method is const or tricky
        // Will update stats in caller
    } else if (msgType == MSG_TYPE_MARKET_PICTURE_COMPLEX) {
        // stats_.packets2021++;
    } else {
        return false;
    }
    
    return true;
}

void BSEReceiver::decodeAndDispatch(const uint8_t* buffer, size_t length) {
    // We assume validation passed
    uint16_t msgType = le16toh_func(*(uint16_t*)(buffer + 8));
    if (msgType == MSG_TYPE_MARKET_PICTURE) stats_.packets2020++;
    else if (msgType == MSG_TYPE_MARKET_PICTURE_COMPLEX) stats_.packets2021++;
    
    // Number of records
    // Header 36 bytes. Records 264 bytes.
    if (length < HEADER_SIZE) return;
    size_t dataSize = length - HEADER_SIZE;
    size_t numRecords = dataSize / RECORD_SLOT_SIZE;
    
    // Capture system timestamp (microseconds since epoch)
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    for (size_t i = 0; i < numRecords; i++) {
        size_t offset = HEADER_SIZE + (i * RECORD_SLOT_SIZE);
        if (offset + RECORD_SLOT_SIZE > length) break;
        
        const uint8_t* recPtr = buffer + offset;
        
        // Read Token (0-3 LE)
        uint32_t token = le32toh_func(*(uint32_t*)(recPtr));
        if (token == 0) continue; // Empty slot
        
        DecodedRecord decRec;
        decRec.token = token;
        decRec.packetTimestamp = now;
        
        // Parse Uncompressed Fields (Offsets from Python Decoder)
        // Offset 4: Open (int32 LE)
        decRec.open = le32toh_signed(*(int32_t*)(recPtr + 4)) / 100.0;
        
        // Offset 8: Prev Close
        decRec.close = le32toh_signed(*(int32_t*)(recPtr + 8)) / 100.0;
        
        // Offset 12: High
        decRec.high = le32toh_signed(*(int32_t*)(recPtr + 12)) / 100.0;
        
        // Offset 16: Low
        decRec.low = le32toh_signed(*(int32_t*)(recPtr + 16)) / 100.0;
        
        // Offset 24: Volume (int32 LE)
        decRec.volume = le32toh_signed(*(int32_t*)(recPtr + 24));
        
        // Offset 28: Turnover in Lakhs (uint32 LE)
        decRec.turnoverLakhs = le32toh_func(*(uint32_t*)(recPtr + 28));
        
        // Offset 32: Lot Size (uint32 LE)
        decRec.lotSize = le32toh_func(*(uint32_t*)(recPtr + 32));
        
        // Offset 36: LTP
        decRec.ltp = le32toh_signed(*(int32_t*)(recPtr + 36)) / 100.0;
        
        // Offset 44: Seq No
        decRec.sequenceNumber = le32toh_func(*(uint32_t*)(recPtr + 44));
        
        // Offset 84: ATP
        decRec.atp = le32toh_signed(*(int32_t*)(recPtr + 84)) / 100.0;
        
        // Order Book (Offsets 104-263)
        // 5 Levels, Interleaved Bid/Ask
        for (int L = 0; L < 5; L++) {
            size_t bidBase = 104 + (L * 32);
            size_t askBase = bidBase + 16;
            
            // Bid
            int32_t bPrice = le32toh_signed(*(int32_t*)(recPtr + bidBase));
            int32_t bQty   = le32toh_signed(*(int32_t*)(recPtr + bidBase + 4));
            if (bPrice > 0 && bQty > 0) {
                decRec.bids.push_back({(double)bPrice / 100.0, bQty});
            }
            
            // Ask
            int32_t aPrice = le32toh_signed(*(int32_t*)(recPtr + askBase));
            int32_t aQty   = le32toh_signed(*(int32_t*)(recPtr + askBase + 4));
            if (aPrice > 0 && aQty > 0) {
                decRec.asks.push_back({(double)aPrice / 100.0, aQty});
            }
        }
        
        stats_.packetsDecoded++;
        if (recordCallback_) {
            recordCallback_(decRec);
        }
    }
}

} // namespace bse
