#include "udp_receiver.h"
#include "protocol.h"
#include "constants.h"
#include "lzo_decompress.h"
#include "socket_platform.h"



#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>
#include <thread>

namespace nsefo {

UDPStats::UDPStats() {
    startTime = std::chrono::steady_clock::now();
}

void UDPStats::update(uint16_t code, int compressedSize, int rawSize, bool error) {
    totalPackets++;
    totalBytes += compressedSize;
    
    if (isCompressed(code)) {
        compressedPackets++;
        if (error) {
            decompressionFailures++;
        } else {
            decompressedPackets++;
        }
    }

    auto& stat = messageStats[code];
    stat.transactionCode = code;
    stat.count++;
    stat.totalCompressedSize += compressedSize;
    stat.totalRawSize += rawSize;
}

void UDPStats::recordPacket() {
    totalPackets++;
}

void UDPStats::print() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

    std::cout << "\n====================================================================================================" << std::endl;
    std::cout << "NSE MULTICAST UDP RECEIVER - STATISTICS" << std::endl;
    std::cout << "====================================================================================================" << std::endl;
    std::cout << "Runtime: " << duration << "s" << std::endl;
    std::cout << "Total Packets: " << totalPackets << std::endl;
    std::cout << "Total Bytes: " << totalBytes / 1024.0 / 1024.0 << " MB" << std::endl;
    
    std::cout << "\nCode   Name                            Count       Comp(KB)    Raw(KB)" << std::endl;
    std::cout << "--------------------------------------------------------------------------------" << std::endl;
    
    for (const auto& pair : messageStats) {
        const auto& s = pair.second;
        std::string name = getTxCodeName(s.transactionCode);
        std::cout << std::left << std::setw(6) << s.transactionCode 
                  << std::setw(32) << name
                  << std::setw(12) << s.count 
                  << std::setw(12) << std::fixed << std::setprecision(2) << s.totalCompressedSize / 1024.0 
                  << std::setw(12) << s.totalRawSize / 1024.0 << std::endl;
    }
    std::cout << std::endl;
}

std::ostream& operator<<(std::ostream& os, const UDPStats& stats) {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - stats.startTime).count();

    os << "\n====================================================================================================" << std::endl;
    os << "NSE MULTICAST UDP RECEIVER - STATISTICS" << std::endl;
    os << "====================================================================================================" << std::endl;
    os << "Runtime: " << duration << "s" << std::endl;
    os << "Total Packets: " << stats.totalPackets << std::endl;
    os << "Compressed: " << stats.compressedPackets << " | Decompressed: " << stats.decompressedPackets 
       << " | Failures: " << stats.decompressionFailures << std::endl;
    os << "Total Bytes: " << stats.totalBytes / 1024.0 / 1024.0 << " MB" << std::endl;
    
    if (!stats.messageStats.empty()) {
        os << "\nCode   Name                            Count       Comp(KB)    Raw(KB)" << std::endl;
        os << "--------------------------------------------------------------------------------" << std::endl;
        
        for (const auto& pair : stats.messageStats) {
            const auto& s = pair.second;
            std::string name = getTxCodeName(s.transactionCode);
            os << std::left << std::setw(6) << s.transactionCode 
               << std::setw(32) << name
               << std::setw(12) << s.count 
               << std::setw(12) << std::fixed << std::setprecision(2) << s.totalCompressedSize / 1024.0 
               << std::setw(12) << s.totalRawSize / 1024.0 << std::endl;
        }
    }
    
    return os;
}

void UDPReceiver::startListener(int port, UDPStats& stats) {
    WinsockLoader::init(); socket_t sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == socket_invalid) {
        std::cerr << "socket: " << socket_error_string(socket_errno) << std::endl;
        return;
    }

    // Allow multiple sockets to use the same PORT number
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        std::cerr << "setting SO_REUSEADDR: " << socket_error_string(socket_errno) << std::endl;
        socket_close(sockfd);
        return;
    }

    struct sockaddr_in local_addr;
    std::memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        std::cerr << "bind: " << socket_error_string(socket_errno) << std::endl;
        socket_close(sockfd);
        return;
    }

    // Join multicast group
    struct ip_mreq group;
    group.imr_multiaddr.s_addr = inet_addr("233.1.2.5");
    group.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
        std::cerr << "adding multicast group: " << socket_error_string(socket_errno) << std::endl;
        socket_close(sockfd);
        return;
    }

    // Increase buffer size
    int rcvbuf = 2 * 1024 * 1024;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    std::cout << "Listening on 233.1.2.5:" << port << std::endl;

    std::vector<uint8_t> buffer(65535);
    std::vector<uint8_t> decompressBuffer(65535 * 4); 
    
    auto lastLogTime = std::chrono::steady_clock::now();
    int packetsSinceLog = 0;

    while (true) {
        std::cout << "Waiting for UDP packets..." << std::endl;
        ssize_t n = recv(sockfd, buffer.data(), buffer.size(), 0);
        if (n < 0) break;
        
        if (n < 4) continue; // Too short

        // NSE UDP Packet parsing
        // Skip first 4 bytes (NetID + NoOfPackets) -> Offset 4 is cPackData
        // Inside cPackData: first 2 bytes iCompLen
        
        if (n < 6) continue;
        
        uint16_t iCompLen = be16toh_func(*reinterpret_cast<uint16_t*>(&buffer[4]));
        uint16_t transactionCode = 0;
        int compressedSize = n;
        int rawSize = n;
        bool error = false;

        if (iCompLen > 0) {
            // Compressed
            // Compressed data starts at buffer[6]
            size_t compDataLen = iCompLen;
            if (6 + compDataLen > (size_t)n) {
                // Invalid length
                continue;
            }

            std::vector<uint8_t> src(buffer.begin() + 6, buffer.begin() + 6 + compDataLen);
            decompressBuffer.clear();
            decompressBuffer.resize(65535*4); // Reset size

            try {
                int decompLen = common::LzoDecompressor::decompress(src, decompressBuffer);
                rawSize = decompLen;
                
                // Get transaction code from offset 18 of DECOMPRESSED data
                // Header is 40 bytes, TransactionCode is at offset 10
                // Wait, protocol doc says transaction code is at offset 10 of BroadcastHeader
                // Go code checks offset 18?
                // Ref Go: header.TransactionCode = binary.BigEndian.Uint16(data[10:12]) (ParseBroadcastHeader)
                // BUT ProcessUDPPacket says:
                // "Successfully decompressed - message code is at offset 18"
                // Let's check the Go ParseBroadcastHeader again.
                // It takes 'data'.
                // If the packet is NOT compressed, the data is passed directly.
                // If it IS compressed, the decompressed buffer is used.
                // The Go `ProcessUDPPacket` logic:
                //   if len(decompressedPacket) >= 20 { transactionCode = ... get from [18:20] }
                // Wait, why 18?
                // BroadcastHeader struct:
                // Reserved1 (2) + Reserved2 (2) + LogTime (4) + AlphaChar (2) + TransCode (2)...
                // 2+2+4+2 = 10. So TransCode is at 10.
                // Let's re-read Go code carefully.
                // 
                // Go: header.TransactionCode = binary.BigEndian.Uint16(data[10:12])
                //
                // Go `ProcessUDPPacket`:
                // "transactionCode = binary.BigEndian.Uint16(compressedPacket[18:20])" (on error fallback)
                // "transactionCode = binary.BigEndian.Uint16(decompressedPacket[18:20])" (on success)
                // Uncompressed: "transactionCode = binary.BigEndian.Uint16(cPackData[18:20])"
                //
                // Why 18?
                // Maybe the `cPackData` includes some outer header?
                // "Bytes 0-1: NetID, 2-3: iNoOfPackets, 4+: cPackData[]"
                // "First 2 bytes: iCompLen"
                //
                // Use logic:
                // If compressed: 
                //   src = buffer[6...6+len]
                //   dst = decompress(src)
                //   header starts at dst[0] ? No, usually LZO decompresses the payload.
                //   The payload usually IS the BroadcastHeader + Data.
                //   So if dst[0] is start of header, then TransCode should be at 10.
                //
                // Let's check Go `ParseBroadcastHeader` usage. It is passed `data`.
                // In main loop (not shown fully in snippet? No, I saw it).
                // Wait, `ProcessUDPPacket` DOES NOT call `ParseBroadcastHeader`.
                // It manually extracts `transactionCode` potentially for stats only.
                // And it extracts it from 18.
                // 18 = 0x12.
                //
                // Maybe the buffer has 8 bytes of initial garbage or sequence number?
                // Or maybe the Go code is parsing `cPackData` which starts at offset 4 of UDP packet.
                // cPackData[0:2] = iCompLen.
                // If uncompressed, payload starts at cPackData[2]?
                // If Uncompressed: "transactionCode = binary.BigEndian.Uint16(cPackData[18:20])"
                // cPackData[18] corresponds to UDP[4+18] = UDP[22].
                //
                // If protocol says TransCode is at 10.
                // Difference is 8 bytes.
                // Maybe there is an 8 byte header BEFORE the BroadcastHeader?
                //
                // Let's stick to what Go code does for extraction for now: index 18 of the payload buffer.
                
                if (decompLen >= 20) {
                     // Go uses offset 18.
                     // But Standard Broadcast Header has TransCode at 10.
                     // Let's trust Go's offset 18 for now as "magic" offset for this feed.
                     // Or maybe I should check if there's an 8 byte offset.
                     // Let's use 18 to be safe as per reference.
                     const uint8_t* ptr = decompressBuffer.data();
                     transactionCode = be16toh_func(*reinterpret_cast<const uint16_t*>(ptr + 18));
                     // printf("Decompressed TxCode: %u\n", transactionCode); // Debug
                     std::cout << "  [Decompressed] TxCode at offset 18: " << transactionCode << std::endl;
                }

            } catch (const std::exception& e) {
                error = true;
                // Try from compressed data offset 18 as fallback (like Go msg)
                if (src.size() >= 20) {
                     transactionCode = be16toh_func(*reinterpret_cast<const uint16_t*>(src.data() + 18));
                }
            }
        } else {
            // Uncompressed
            // Payload starts at buffer[6] (skip iCompLen)
            if ((size_t)n >= 6 + 20) {
                 transactionCode = be16toh_func(*reinterpret_cast<uint16_t*>(&buffer[6 + 18]));
            }
        }

        stats.update(transactionCode, compressedSize, rawSize, error);
        
        packetsSinceLog++;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastLogTime).count() >= 5) {
             std::cout << "[STATUS] Received " << packetsSinceLog << " packets in last 5s. Total: " << stats.totalPackets << std::endl;
             lastLogTime = now;
             packetsSinceLog = 0;
        }
    }
    
    socket_close(sockfd);
}

} // namespace nsefo
