#include "multicast_receiver.h"
#include "packet.h"
#include "protocol.h" 
#include "utils/parse_compressed_message.h"
#include "utils/parse_uncompressed_packet.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <cerrno>


MulticastReceiver::MulticastReceiver(const std::string& ip, int port) 
    : sockfd(-1), running(false), lastSeqNo(0) {
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
    }

    // Set socket options
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        close(sockfd);
        sockfd = -1;
        throw std::runtime_error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
    }
    
    // Set receive timeout (1 second) for graceful shutdown
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        close(sockfd);
        sockfd = -1;
        throw std::runtime_error("Failed to set SO_RCVTIMEO: " + std::string(strerror(errno)));
    }

    // Bind socket
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        sockfd = -1;
        throw std::runtime_error("Failed to bind socket: " + std::string(strerror(errno)));
    }

    // Join multicast group
    struct ip_mreq group;
    group.imr_multiaddr.s_addr = inet_addr(ip.c_str());
    group.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
        close(sockfd);
        sockfd = -1;
        throw std::runtime_error("Failed to join multicast group: " + std::string(strerror(errno)));
    }
    
    std::cout << "MulticastReceiver initialized successfully (buffer size: " << kBufferSize << " bytes)" << std::endl;
}

MulticastReceiver::~MulticastReceiver() {
    stop();
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
}

void MulticastReceiver::start() {
    if (sockfd < 0) {
        std::cerr << "Error: Cannot start - receiver not properly initialized" << std::endl;
        return;
    }
    
    running = true;
    std::cout << "Starting MulticastReceiver..." << std::endl;
    
    int packet_count = 0;
    
    while (running) {
        ssize_t n = recv(sockfd, buffer, kBufferSize, 0);
        
        // Handle timeout (allows checking running flag)
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // Timeout, check running flag and continue
            }
            std::cerr << "recv() error: " << strerror(errno) << std::endl;
            break;
        }

        if (n < (ssize_t)sizeof(Packet)) {
            stats.update(0, 0, 0, true);  // Record error
            continue;
        }

        packet_count++;

        // Parse packet header
        Packet* pkt = reinterpret_cast<Packet*>(buffer);
        pkt->iNoOfMsgs = be16toh_func(pkt->iNoOfMsgs);

        // Parse messages
        char* ptr = pkt->cPackData;
        char* end = buffer + n;

        for (int i = 0; i < pkt->iNoOfMsgs; ++i) {
            try {
                if (ptr + sizeof(int16_t) > end) {
                    stats.update(0, 0, 0, true);  // Record error
                    break;
                }

                // Read iCompLen (first 2 bytes of MessageData)
                int16_t iCompLen = be16toh_func(*((int16_t*)ptr));
                
                if (iCompLen > 0) {
                    // std::cout << "Processing compressed message of length " << iCompLen << std::endl;
                    // Compressed message
                    ptr += sizeof(int16_t);
                    
                    if (ptr + iCompLen > end) {
                        stats.update(0, 0, 0, true);  // Record error
                        break;
                    }
                    
                    // Parse compressed message and update stats with transaction code
                    parse_compressed_message(ptr, iCompLen, stats);
                    
                    ptr += iCompLen;
                    
                } else {
                    // Uncompressed message
                    if (ptr + 54 > end) {
                        stats.update(0, 0, 0, true);  // Record error
                        break;
                    }
                    
                    // MessageLength is at offset 52 from packet start (ptr points to offset 4)
                    uint16_t msgLen = be16toh_func(*((uint16_t*)(ptr + 48)));
                    
                    if (ptr + 10 + msgLen > end) {
                        stats.update(0, 0, 0, true);  // Record error
                        break;
                    }
                    
                    // Extract transaction code for stats
                    uint16_t txCode = be16toh_func(*((uint16_t*)(ptr + 20)));  // Offset 10 of BCAST_HEADER
                    
                    // Update stats
                    stats.update(txCode, 0, msgLen, false);
                    
                    // Parse uncompressed message
                    parse_uncompressed_message(ptr + 10, msgLen);
                    
                    ptr += 10 + msgLen;
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception processing message " << i << ": " << e.what() << std::endl;
                stats.update(0, 0, 0, true);  // Record error
                break;  // Skip rest of messages in this packet
            } catch (...) {
                std::cerr << "Unknown exception processing message " << i << std::endl;
                stats.update(0, 0, 0, true);  // Record error
                break;  // Skip rest of messages in this packet
            }
        }
    }
    
    std::cout << "MulticastReceiver stopped" << std::endl;
}

void MulticastReceiver::stop() {
    running = false;
}
