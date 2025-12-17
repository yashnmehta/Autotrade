#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <string>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include "minilzo/minilzo.h"

// Framing
struct BcastPackData {
    char cNetId[2];       
    int16_t iNoPackets;
    // Followed by packets
};



// User Provided BCAST_HEADER
// Packet Length 40 bytes
#pragma pack(push, 1)
struct BCAST_HEADER {
    char reserved[4];
    int32_t logTime;
    char alphaChar[2];
    int16_t transCode;
    int16_t errorCode;
    int32_t bcSeqNo;
    char reserved2[4];
    char timeStamp2[8];
    char filler2[8];
    int16_t messageLength; // Offset 38
};
#pragma pack(pop)

void packet_listener(const std::string& group, int port) {
    if (lzo_init() != LZO_E_OK) {
        std::cerr << "LZO Init Failed" << std::endl;
        return;
    }
    
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }
    
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return;
    }
    
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(group.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    
    std::cout << "[Listener] Listening on " << group << ":" << port << std::endl;
    
    std::vector<uint8_t> buffer(65536);
    std::vector<uint8_t> decompBuffer(65536);
    
    while(true) {
        sockaddr_in src;
        socklen_t len = sizeof(src);
        int n = recvfrom(fd, buffer.data(), buffer.size(), 0, (struct sockaddr*)&src, &len);
        
        if (n > 0) {
            if (n >= (int)sizeof(BcastPackData)) { 
                BcastPackData* outer = (BcastPackData*)buffer.data();
                int numPackets = ntohs(outer->iNoPackets); 
                int offset = sizeof(BcastPackData);
                
                for (int m=0; m < numPackets; m++) {
                    if (offset + (int)sizeof(int16_t) > n) break;
                    
                    int16_t rawCompLen;
                    memcpy(&rawCompLen, buffer.data() + offset, sizeof(int16_t));
                    int16_t compLen = ntohs(rawCompLen); 
                    offset += sizeof(int16_t);
                    
                    if (compLen == 0) {
                        // UNCOMPRESSED
                        // Buffer: [2 Bytes CompLen] [1 Byte Mkt] [7 Ignored] [BCAST_HEADER] [Payload]
                        // We consumed 2 bytes. Offset points to 1 Byte Mkt.
                        
                        // Need 8 filler + Header
                        if (offset + 8 + (int)sizeof(BCAST_HEADER) <= n) {
                             uint8_t* hdrStart = buffer.data() + offset + 8;
                             BCAST_HEADER* bh = (BCAST_HEADER*)hdrStart;
                             int16_t totalMsgLen = ntohs(bh->messageLength); // Header + Payload
                             
                             // Total Consume = 8 + totalMsgLen
                             if (offset + 8 + totalMsgLen > n) {
                                 std::cout << "Truncated Uncompressed Msg" << std::endl;
                                 break;
                             }
                             
                             // Valid Message
                             std::cout << "   -> Pkt " << m << " Uncompressed. TransCode=" << ntohs(bh->transCode) << std::endl;
                             
                             offset += 8 + totalMsgLen;
                        } else {
                            break; 
                        }
                    } else {
                        // COMPRESSED
                        if (offset + compLen > n) {
                            std::cout << "Truncated Compressed Pkg" << std::endl;
                            break;
                        }
                        
                        lzo_uint out_len = decompBuffer.size();
                        int r = lzo1z_decompress_safe(buffer.data() + offset, compLen, decompBuffer.data(), &out_len, NULL);
                        
                        if (r == LZO_E_OK) {
                            if (out_len >= 8 + sizeof(BCAST_HEADER)) {
                                 uint8_t* hdrStart = decompBuffer.data() + 8;
                                 BCAST_HEADER* bh = (BCAST_HEADER*)hdrStart;
                                 int16_t totalMsgLen = ntohs(bh->messageLength);
                                 
                                 std::cout << "   -> Pkt " << m << " Decompressed OK. TransCode=" << ntohs(bh->transCode) << " Size=" << out_len << std::endl;
                            }
                        } else {
                             std::cout << "   -> Pkt " << m << " LZO Error " << r << std::endl;
                        }
                        
                        offset += compLen;
                    }
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    packet_listener("233.1.2.5", 34331);
    return 0;
}
