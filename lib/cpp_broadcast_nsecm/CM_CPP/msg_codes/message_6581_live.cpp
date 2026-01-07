// NSE Capital Market Multicast UDP Receiver - Message 6581 Only
// 
// FOCUS: Only process message code 6581 (BC_AUCTION_STATUS_CHANGE)
// OUTPUT: csv_output/message_6581_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3
// Structure: BCAST_VCT_MESSAGES (298 bytes)
// Session: Auction Session Management - Status transitions between auction phases
//
// Converted from Go to C++

#include "message_6581_live.h"
#include "../lzo_decompressor_safe.h"
#include "../utilities.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <atomic>
#include <map>
#include <mutex>
#include <vector>

// Platform-specific includes - moved to utilities.h

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

namespace Message6581Receiver {
    // Basic counters
    std::atomic<int64_t> packetCount{0};
    std::atomic<int64_t> totalBytes{0};
    std::atomic<int64_t> compressedCount{0};
    std::atomic<int64_t> decompressedCount{0};
    std::atomic<int64_t> decompressionErrors{0};
    
    // 6581 specific counters
    std::atomic<int64_t> message6581Count{0};
    std::atomic<int64_t> message6581Saved{0};
    
    // CSV file for 6581
    std::ofstream csvFile6581;
    
    // Control variables
    std::chrono::steady_clock::time_point startTime;
    std::atomic<bool> shutdownFlag{false};
    
    // Track message codes for statistics
    std::map<uint16_t, int64_t> messageCodeCounts;
    std::mutex messageCodeMutex;
}

using namespace Message6581Receiver;

// =============================================================================
// HELPER FUNCTIONS - moved to utilities.cpp
// =============================================================================

// =============================================================================
// CSV FUNCTIONS
// =============================================================================

bool initialize6581CSV() {
    // Create output directory
#ifdef _WIN32
    system("mkdir csv_output 2>nul");  // Suppress error if directory exists
#else
    system("mkdir -p csv_output");
#endif
    
    std::string timestamp = getFileTimestamp();
    std::string filename = "csv_output/message_6581_" + timestamp + ".csv";
    
    csvFile6581.open(filename);
    if (!csvFile6581.is_open()) {
        std::cerr << "❌ Failed to create CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write headers
    csvFile6581 << "Timestamp,TransactionCode,BranchNumber,BrokerNumber,ActionCode,TraderWsBit,MsgLength,Message\n";
    csvFile6581.flush();
    
    std::cout << "📁 Created CSV file for Message 6581: " << filename << std::endl;
    return true;
}

void exportTo6581CSV(uint16_t transactionCode, uint16_t branchNumber, 
                    const std::string& brokerNumber, const std::string& actionCode,
                    uint8_t traderWsBit, uint16_t msgLength, const std::string& message) {
    
    if (!csvFile6581.is_open()) {
        return;
    }
    
    csvFile6581 << getCurrentTimestamp() << ","
                << transactionCode << ","
                << branchNumber << ","
                << brokerNumber << ","
                << actionCode << ","
                << static_cast<int>(traderWsBit) << ","
                << msgLength << ","
                << "\"" << message << "\"\n";
    csvFile6581.flush();
}

// =============================================================================
// MESSAGE PROCESSING
// =============================================================================

void process6581Message(const uint8_t* data, int dataLen) {
    if (dataLen < 298) { // 40-byte header + 258-byte message
        return;
    }
    
    message6581Count++;
    int64_t currentCount = message6581Count.load();
    
    int offset = 40; // Skip BCAST_HEADER
    
    // Parse BCAST_VCT_MESSAGES structure
    uint16_t transactionCode = readUint16BigEndian(data, offset);
    offset += 2;
    
    uint16_t branchNumber = readUint16BigEndian(data, offset);
    offset += 2;
    
    // Extract broker number (5 bytes)
    std::string brokerNumber;
    for (int i = 0; i < 5; i++) {
        if (data[offset + i] != 0) {
            brokerNumber += static_cast<char>(data[offset + i]);
        }
    }
    offset += 5;
    
    // Extract action code (3 bytes)
    std::string actionCode;
    for (int i = 0; i < 3; i++) {
        if (data[offset + i] != 0) {
            actionCode += static_cast<char>(data[offset + i]);
        }
    }
    offset += 3;
    
    // Skip reserved field (4 bytes)
    offset += 4;
    
    uint8_t traderWsBit = data[offset];
    offset++;
    
    // Skip reserved2 field (1 byte)
    offset++;
    
    uint16_t msgLength = readUint16BigEndian(data, offset);
    offset += 2;
    
    // Extract message (240 bytes)
    std::string message;
    for (int i = 0; i < 240 && i < static_cast<int>(msgLength); i++) {
        if (data[offset + i] != 0) {
            message += static_cast<char>(data[offset + i]);
        }
    }
    
    if (currentCount == 1) {
        std::cout << "\n✅ First Message 6581 (BC_AUCTION_STATUS_CHANGE) received\n" << std::endl;
    }
    
    // Export to CSV
    exportTo6581CSV(transactionCode, branchNumber, brokerNumber, actionCode, 
                   traderWsBit, msgLength, message);
    message6581Saved++;
}

bool processUDPPacket6581(const uint8_t* data, int dataLen) {
    if (dataLen < 6) {
        return false;
    }
    
    const uint8_t* cPackData = data + 4;
    int cPackLen = dataLen - 4;
    
    if (cPackLen < 2) {
        return false;
    }
    
    uint16_t iCompLen = readUint16BigEndian(cPackData, 0);
    
    std::vector<uint8_t> finalData;
    bool isCompressed = iCompLen > 0;
    
    if (isCompressed) {
        int offset = 2;
        if (offset + iCompLen > cPackLen) {
            return false;
        }
        
        compressedCount++;
        
        try {
            std::vector<uint8_t> decompressedData(10240);
            int decompLen = DecompressUltra(cPackData + offset, iCompLen, 
                                          decompressedData.data(), decompressedData.size());
            decompressedCount++;
            finalData.assign(decompressedData.begin(), decompressedData.begin() + decompLen);
        } catch (const LZOException& e) {
            decompressionErrors++;
            return false;
        }
    } else {
        finalData.assign(cPackData + 2, cPackData + cPackLen);
    }
    
    if (finalData.size() < 28) {
        return false;
    }
    
    // Skip first 8 bytes
    if (finalData.size() < 8) {
        return false;
    }
    std::vector<uint8_t> processData(finalData.begin() + 8, finalData.end());
    
    if (processData.size() < 48) {
        return false;
    }
    
    // Read transaction code at offset 10-12 in BCAST_HEADER (after 8-byte skip)
    uint16_t transactionCode = readUint16BigEndian(processData.data(), 10);
    
    // Track message codes
    {
        std::lock_guard<std::mutex> lock(messageCodeMutex);
        messageCodeCounts[transactionCode]++;
    }
    
    // Only process message 6581
    if (transactionCode != 6581) {
        return false;
    }
    
    process6581Message(processData.data());
    return true;
}

// =============================================================================
// STATISTICS
// =============================================================================

void printStats6581() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t compressed = compressedCount.load();
    int64_t msg6581 = message6581Count.load();
    int64_t saved = message6581Saved.load();
    
    if (seconds > 0) {
        std::string status = "❌ NOT FOUND";
        if (msg6581 > 0) {
            status = "✅ RECEIVING";
        }
        
        std::cout << "⏱️  " << std::fixed << std::setprecision(0) << seconds 
                  << "s | 📦 " << packets << " pkts (" 
                  << std::setprecision(0) << (packets / seconds) << "/s) | 🗜️  " 
                  << compressed << " compressed | 🎯 6581: " << status 
                  << " | " << msg6581 << " msgs, " << saved << " saved" << std::endl;
    }
}

std::string formatNumber6581(int64_t n) {
    if (n < 1000) {
        return std::to_string(n);
    } else if (n < 1000000) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (static_cast<double>(n) / 1000.0) << "K";
        return ss.str();
    } else {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (static_cast<double>(n) / 1000000.0) << "M";
        return ss.str();
    }
}

std::string getMessageCodeDescription6581(uint16_t code) {
    switch (code) {
        case 6511:
            return "BC_OPEN_MESSAGE (Market Open)";
        case 6521:
            return "BC_CLOSE_MESSAGE (Market Close)";
        case 6531:
            return "BC_PREOPEN_SHUTDOWN_MSG (Preopen)";
        case 6541:
            return "BC_CIRCUIT_CHECK (Heartbeat)";
        case 6571:
            return "BC_NORMAL_MKT_PREOPEN_ENDED (Preopen End)";
        case 6581:
            return "BC_AUCTION_STATUS_CHANGE (Auction Status)";
        case 6583:
            return "BC_CLOSING_START (Closing Start)";
        case 6584:
            return "BC_CLOSING_END (Closing End)";
        default:
            return "Unknown";
    }
}

void printFinalStats6581() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t bytes = totalBytes.load();
    double totalMB = static_cast<double>(bytes) / (1024.0 * 1024.0);
    int64_t compressed = compressedCount.load();
    int64_t decompressed = decompressedCount.load();
    int64_t errors = decompressionErrors.load();
    int64_t msg6581 = message6581Count.load();
    int64_t saved = message6581Saved.load();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "FINAL STATISTICS - MESSAGE 6581 DECODER (BC_AUCTION_STATUS_CHANGE)" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    std::cout << "📊 LISTENER PERFORMANCE" << std::endl;
    std::cout << "  Runtime:              " << std::fixed << std::setprecision(0) 
              << seconds << " seconds" << std::endl;
    std::cout << "  Total Packets:        " << formatNumber6581(packets) << std::endl;
    std::cout << "  Total Data:           " << std::setprecision(1) << totalMB << " MB" << std::endl;
    
    if (seconds > 0) {
        std::cout << "  Avg Packet Rate:      " << std::setprecision(2) 
                  << (packets / seconds) << " packets/sec" << std::endl;
        std::cout << "  Avg Data Rate:        " << std::setprecision(2) 
                  << (totalMB * 1024 / seconds) << " KB/sec" << std::endl;
    }
    
    std::cout << std::endl << "📦 DECOMPRESSION STATISTICS" << std::endl;
    std::cout << "  Compressed Packets:   " << formatNumber6581(compressed);
    if (packets > 0) {
        std::cout << " (" << std::setprecision(1) 
                  << (static_cast<double>(compressed) * 100.0 / packets) << "%)";
    }
    std::cout << std::endl;
    std::cout << "  Decompressed OK:      " << formatNumber6581(decompressed) << std::endl;
    std::cout << "  Decompression Errors: " << formatNumber6581(errors) << std::endl;
    if (compressed > 0) {
        std::cout << "  Success Rate:         " << std::setprecision(1) 
                  << (static_cast<double>(decompressed) * 100.0 / compressed) << "%" << std::endl;
    }
    
    std::cout << std::endl << "🎯 MESSAGE 6581 STATISTICS (BC_AUCTION_STATUS_CHANGE)" << std::endl;
    std::cout << "  Total Messages:       " << formatNumber6581(msg6581) << std::endl;
    std::cout << "  Messages Saved:       " << formatNumber6581(saved) << std::endl;
    
    // Show all message codes found
    std::lock_guard<std::mutex> lock(messageCodeMutex);
    if (!messageCodeCounts.empty()) {
        std::cout << std::endl << "📋 MESSAGE CODES DETECTED (" 
                  << messageCodeCounts.size() << " unique)" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        std::cout << std::left << std::setw(8) << "Code" << " " 
                  << std::setw(40) << "Description" << " " << "Count" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        // Convert to vector and sort
        std::vector<std::pair<uint16_t, int64_t>> sorted(messageCodeCounts.begin(), messageCodeCounts.end());
        std::sort(sorted.begin(), sorted.end());
        
        for (const auto& cf : sorted) {
            std::string description = getMessageCodeDescription6581(cf.first);
            std::cout << std::left << std::setw(8) << cf.first << " " 
                      << std::setw(40) << description << " " 
                      << formatNumber6581(cf.second) << std::endl;
        }
    }
    
    std::cout << std::endl << "📁 CSV FILE CREATED" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "  Location: csv_output/" << std::endl;
    std::cout << "  Messages: " << formatNumber6581(saved) << std::endl;
    std::cout << "  Format: Auction status change notifications" << std::endl;
    
    std::cout << std::endl << std::string(80, '=') << std::endl;
    if (msg6581 > 0) {
        std::cout << "✅ SUCCESS: Auction Status Change Messages (6581) processing completed" << std::endl;
        std::cout << "📊 Captured " << saved << " auction status change notifications" << std::endl;
    } else {
        std::cout << "⚠️  WARNING: No Auction Status Change Messages (6581) found during session" << std::endl;
        std::cout << "💡 Note: Auction status messages are broadcast during auction sessions" << std::endl;
    }
    std::cout << "✅ Check csv_output/ for message_6581_*.csv file" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

// =============================================================================
// MAIN RECEIVER FUNCTION
// =============================================================================

bool runMessage6581Receiver(const std::string& multicastIP, int port) {
    startTime = std::chrono::steady_clock::now();
    shutdownFlag = false;
    
    // Reset counters
    packetCount = 0;
    totalBytes = 0;
    compressedCount = 0;
    decompressedCount = 0;
    decompressionErrors = 0;
    message6581Count = 0;
    message6581Saved = 0;
    messageCodeCounts.clear();

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "❌ Failed to initialize Winsock" << std::endl;
        return false;
    }
#endif
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "NSE CM UDP Receiver - Message 6581 (BC_AUCTION_STATUS_CHANGE)" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Listening for message code 6581 (0x19C5 in hex)" << std::endl;
    std::cout << "Purpose: Auction session status change notification" << std::endl;
    std::cout << "Session: Auction Session Management" << std::endl;
    std::cout << "Note: Status transitions between auction phases" << std::endl;
    std::cout << "Multicast: " << multicastIP << ":" << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    // Initialize CSV
    if (!initialize6581CSV()) {
        std::cerr << "❌ Failed to initialize CSV file" << std::endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }
    
    // Create socket
    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "❌ Failed to create socket" << std::endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }
    
    // Set socket options
    int reuse = 1;
#ifdef _WIN32
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
#else
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    #ifdef SO_REUSEPORT
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
    #endif
#endif
    
    // Set receive buffer size (2MB)
    int bufferSize = 2 * 1024 * 1024;
#ifdef _WIN32
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&bufferSize, sizeof(bufferSize));
#else
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));
#endif
    
    // Bind to multicast address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
#ifdef _WIN32
    addr.sin_addr.s_addr = INADDR_ANY;
#else
    addr.sin_addr.s_addr = inet_addr(multicastIP.c_str());
#endif
    addr.sin_port = htons(port);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        std::cerr << "❌ Failed to bind to multicast address: " << getWinSockError() << std::endl;
        std::cerr << "   Trying to bind to: INADDR_ANY:" << port << std::endl;
#else
        std::cerr << "❌ Failed to bind to multicast address" << std::endl;
#endif
        SOCKET_CLOSE(sockfd);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }
    
    std::cout << "✅ Successfully bound to port " << port << std::endl;
    
    // Join multicast group
#ifdef _WIN32
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicastIP.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq)) < 0) {
        std::cerr << "❌ Failed to join multicast group: " << getWinSockError() << std::endl;
        std::cerr << "   Multicast IP: " << multicastIP << std::endl;
        SOCKET_CLOSE(sockfd);
        WSACleanup();
        return false;
    }
#else
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicastIP.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        std::cerr << "❌ Failed to join multicast group" << std::endl;
        SOCKET_CLOSE(sockfd);
        return false;
    }
#endif
    
    std::cout << "✅ Successfully joined multicast group " << multicastIP << std::endl;
    std::cout << std::endl;
    
    // Start statistics thread
    std::thread statsThread([&]() {
        while (!shutdownFlag) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!shutdownFlag) {
                printStats6581();
            }
        }
    });
    
    // Main packet processing loop
    char buffer[2048];
    while (!shutdownFlag) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
#ifdef _WIN32
        int ready = select(0, &readfds, nullptr, nullptr, &timeout);
#else
        int ready = select(sockfd + 1, &readfds, nullptr, nullptr, &timeout);
#endif
        
        if (ready < 0) {
            break;
        }
        
        if (ready > 0 && FD_ISSET(sockfd, &readfds)) {
            struct sockaddr_in from;
            socklen_t fromlen = sizeof(from);
            
            int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, 
                           (struct sockaddr*)&from, &fromlen);
            
            if (n > 0) {
                packetCount++;
                totalBytes += n;
                processUDPPacket6581(reinterpret_cast<uint8_t*>(buffer), n);
            }
        }
    }
    
    // Cleanup
    shutdownFlag = true;
    statsThread.join();
    SOCKET_CLOSE(sockfd);
    csvFile6581.close();

#ifdef _WIN32
    WSACleanup();
#endif
    
    printFinalStats6581();
    return true;
}

void stopMessage6581Receiver() {
    shutdownFlag = true;
}
