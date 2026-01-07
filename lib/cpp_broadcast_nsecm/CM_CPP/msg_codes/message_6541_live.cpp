// NSE Capital Market Multicast UDP Receiver - Message 6541 Only
// 
// FOCUS: Only process message code 6541 (BC_CIRCUIT_CHECK - Heartbeat Pulse)
// OUTPUT: csv_output/message_6541_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Page 138
// Structure: Only BCAST_HEADER (40 bytes) - No additional fields
// Purpose: Heartbeat pulse sent every ~9 seconds when no other data
//
// Converted from Go to C++

#include "message_6541_live.h"
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

namespace Message6541Receiver {
    // Basic counters
    std::atomic<int64_t> packetCount{0};
    std::atomic<int64_t> totalBytes{0};
    std::atomic<int64_t> compressedCount{0};
    std::atomic<int64_t> decompressedCount{0};
    std::atomic<int64_t> decompressionErrors{0};
    
    // 6541 specific counters
    std::atomic<int64_t> message6541Count{0};
    std::atomic<int64_t> message6541Saved{0};
    
    // Heartbeat tracking
    std::chrono::steady_clock::time_point lastHeartbeatTime;
    
    // CSV file for 6541
    std::ofstream csvFile6541;
    
    // Control variables
    std::chrono::steady_clock::time_point startTime;
    std::atomic<bool> shutdownFlag{false};
    
    // Track message codes for statistics
    std::map<uint16_t, int64_t> messageCodeCounts;
    std::mutex messageCodeMutex;
}

using namespace Message6541Receiver;

// =============================================================================
// HELPER FUNCTIONS - moved to utilities.cpp
// =============================================================================

// =============================================================================
// CSV FUNCTIONS
// =============================================================================

bool initialize6541CSV() {
    // Create output directory
#ifdef _WIN32
    system("mkdir csv_output 2>nul");  // Suppress error if directory exists
#else
    system("mkdir -p csv_output");
#endif
    
    std::string timestamp = getFileTimestamp();
    std::string filename = "csv_output/message_6541_" + timestamp + ".csv";
    
    csvFile6541.open(filename);
    if (!csvFile6541.is_open()) {
        std::cerr << "❌ Failed to create CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write headers
    csvFile6541 << "Timestamp,TransactionCode,LogTime,AlphaChar,ErrorCode,BCSeqNo,TimeStamp2,MessageLength,HeartbeatNumber,SecondsSinceLastHeartbeat\n";
    csvFile6541.flush();
    
    std::cout << "📁 Created CSV file for Message 6541: " << filename << std::endl;
    return true;
}

void exportTo6541CSV(uint16_t transactionCode, uint32_t logTime, 
                    const std::string& alphaChar, uint16_t errorCode,
                    uint32_t bcSeqNo, const std::string& timeStamp2, 
                    uint16_t messageLength, int64_t heartbeatNumber, 
                    double secondsSinceLast) {
    
    if (!csvFile6541.is_open()) {
        return;
    }
    
    csvFile6541 << getCurrentTimestamp() << ","
                << transactionCode << ","
                << logTime << ","
                << "\"" << alphaChar << "\","
                << errorCode << ","
                << bcSeqNo << ","
                << "\"" << timeStamp2 << "\","
                << messageLength << ","
                << heartbeatNumber << ","
                << std::fixed << std::setprecision(3) << secondsSinceLast << "\n";
    csvFile6541.flush();
}

// =============================================================================
// MESSAGE PROCESSING
// =============================================================================

void process6541Message(const uint8_t* data, int dataLen) {
    if (dataLen < 40) { // Only BCAST_HEADER for heartbeat messages
        return;
    }
    
    message6541Count++;
    int64_t currentCount = message6541Count.load();
    
    // Parse BCAST_HEADER fields (offset 0-39)
    // Reserved: data[0:4]
    uint32_t logTime = readUint32BigEndian(data, 4);
    
    // Extract alpha char (2 bytes)
    std::string alphaChar;
    for (int i = 0; i < 2; i++) {
        if (data[8 + i] != 0) {
            alphaChar += static_cast<char>(data[8 + i]);
        }
    }
    
    uint16_t transactionCode = readUint16BigEndian(data, 10);
    uint16_t errorCode = readUint16BigEndian(data, 12);
    uint32_t bcSeqNo = readUint32BigEndian(data, 14);
    
    // Reserved: data[18:22]
    
    // Extract timestamp2 (8 bytes)
    std::string timeStamp2;
    for (int i = 0; i < 8; i++) {
        if (data[22 + i] != 0) {
            timeStamp2 += static_cast<char>(data[22 + i]);
        }
    }
    
    // Filler2: data[30:38]
    uint16_t messageLength = readUint16BigEndian(data, 38);
    
    auto now = std::chrono::steady_clock::now();
    double secondsSinceLast = std::chrono::duration<double>(now - lastHeartbeatTime).count();
    lastHeartbeatTime = now;
    
    if (currentCount == 1) {
        std::cout << "\n💓 First Heartbeat (6541) received\n" << std::endl;
    } else {
        if (secondsSinceLast >= 8.0 && secondsSinceLast <= 10.0) {
            std::cout << "💓 Heartbeat #" << currentCount << " - " 
                      << std::fixed << std::setprecision(1) << secondsSinceLast 
                      << "s since last (NORMAL)" << std::endl;
        } else {
            std::cout << "⚠️  Heartbeat #" << currentCount << " - " 
                      << std::fixed << std::setprecision(1) << secondsSinceLast 
                      << "s since last (ABNORMAL - expected ~9s)" << std::endl;
        }
    }
    
    // Export to CSV
    exportTo6541CSV(transactionCode, logTime, alphaChar, errorCode, 
                   bcSeqNo, timeStamp2, messageLength, currentCount, secondsSinceLast);
    message6541Saved++;
}

bool processUDPPacket6541(const uint8_t* data, int dataLen) {
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
    
    // Only process message 6541
    if (transactionCode != 6541) {
        return false;
    }
    
    process6541Message(processData.data());
    return true;
}

// =============================================================================
// STATISTICS
// =============================================================================

void printStats6541() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t compressed = compressedCount.load();
    int64_t msg6541 = message6541Count.load();
    
    if (seconds > 0) {
        std::string status = "❌ NO HEARTBEAT";
        if (msg6541 > 0) {
            double avgInterval = seconds / static_cast<double>(msg6541);
            std::stringstream ss;
            ss << "💓 BEATING (avg " << std::fixed << std::setprecision(1) << avgInterval << "s)";
            status = ss.str();
        }
        
        std::cout << "⏱️  " << std::fixed << std::setprecision(0) << seconds 
                  << "s | 📦 " << packets << " pkts (" 
                  << std::setprecision(0) << (packets / seconds) << "/s) | 🗜️  " 
                  << compressed << " compressed | " << status 
                  << " | " << msg6541 << " beats" << std::endl;
    }
}

std::string formatNumber6541(int64_t n) {
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

std::string getMessageCodeDescription6541(uint16_t code) {
    switch (code) {
        case 6511:
            return "BC_OPEN_MESSAGE (Market Open)";
        case 6521:
            return "BC_CLOSE_MESSAGE (Market Close)";
        case 6531:
            return "BC_PREOPEN_SHUTDOWN_MSG (Preopen/Shutdown)";
        case 6541:
            return "BC_CIRCUIT_CHECK (Heartbeat)";
        case 6571:
            return "BC_NORMAL_MKT_PREOPEN_ENDED";
        default:
            return "Unknown";
    }
}

void printFinalStats6541() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t bytes = totalBytes.load();
    double totalMB = static_cast<double>(bytes) / (1024.0 * 1024.0);
    int64_t msg6541 = message6541Count.load();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "📊 FINAL STATISTICS - MESSAGE 6541 HEARTBEAT MONITOR" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Runtime                : " << std::fixed << std::setprecision(0) 
              << seconds << " seconds" << std::endl;
    std::cout << "Total Packets Received : " << formatNumber6541(packets) << std::endl;
    std::cout << "Total Bytes Received   : " << formatNumber6541(bytes) 
              << " (" << std::setprecision(2) << totalMB << " MB)" << std::endl;
    
    if (seconds > 0) {
        std::cout << "Packets/Second         : " << std::setprecision(2) 
                  << (packets / seconds) << std::endl;
    }
    
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "💓 Heartbeats (6541)   : " << formatNumber6541(msg6541) << std::endl;
    
    if (msg6541 > 1) {
        double avgInterval = seconds / static_cast<double>(msg6541);
        std::cout << "   Average Interval    : " << std::setprecision(2) 
                  << avgInterval << " seconds" << std::endl;
        if (avgInterval >= 8.0 && avgInterval <= 10.0) {
            std::cout << "   Status              : ✅ NORMAL (expected ~9s)" << std::endl;
        } else {
            std::cout << "   Status              : ⚠️  ABNORMAL (expected ~9s)" << std::endl;
        }
    }
    
    std::cout << std::string(80, '=') << std::endl;
    
    // Show all message codes found
    std::lock_guard<std::mutex> lock(messageCodeMutex);
    if (!messageCodeCounts.empty()) {
        std::cout << "📋 ALL MESSAGE CODES DETECTED:" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        // Convert to vector and sort
        std::vector<std::pair<uint16_t, int64_t>> sorted(messageCodeCounts.begin(), messageCodeCounts.end());
        std::sort(sorted.begin(), sorted.end());
        
        for (const auto& cf : sorted) {
            double percentage = static_cast<double>(cf.second) * 100.0 / packets;
            if (cf.first == 6541) {
                std::cout << "   💓 Code " << std::setw(5) << cf.first << ": " 
                          << std::setw(6) << cf.second << " messages (" 
                          << std::setprecision(1) << percentage << "%) ← HEARTBEAT!" << std::endl;
            } else {
                std::cout << "      Code " << std::setw(5) << cf.first << ": " 
                          << std::setw(6) << cf.second << " messages (" 
                          << std::setprecision(1) << percentage << "%)" << std::endl;
            }
        }
        std::cout << std::string(80, '-') << std::endl;
    }
    
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "✅ Heartbeat monitor stopped successfully!" << std::endl;
    if (msg6541 > 0) {
        std::cout << "📁 Check csv_output/ directory for heartbeat CSV" << std::endl;
    }
    std::cout << std::endl;
}

// =============================================================================
// MAIN RECEIVER FUNCTION
// =============================================================================

bool runMessage6541Receiver(const std::string& multicastIP, int port) {
    startTime = std::chrono::steady_clock::now();
    lastHeartbeatTime = std::chrono::steady_clock::now();
    shutdownFlag = false;
    
    // Reset counters
    packetCount = 0;
    totalBytes = 0;
    compressedCount = 0;
    decompressedCount = 0;
    decompressionErrors = 0;
    message6541Count = 0;
    message6541Saved = 0;
    messageCodeCounts.clear();

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "❌ Failed to initialize Winsock" << std::endl;
        return false;
    }
#endif
    
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║ NSE CM Message 6541 Receiver - Heartbeat Monitor         ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "📡 Multicast: " << multicastIP << ":" << port << std::endl;
    std::cout << "🎯 Target: Message 6541 (BC_CIRCUIT_CHECK - Heartbeat)" << std::endl;
    std::cout << "💓 Expected: ~9 seconds between heartbeats" << std::endl;
    std::cout << "📊 Statistics every 10 seconds" << std::endl;
    std::cout << "⏱️  Started at: " << getCurrentTimestamp() << std::endl << std::endl;
    std::cout << "Waiting for packets..." << std::endl << std::endl;
    
    // Initialize CSV
    if (!initialize6541CSV()) {
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
    
    // Start statistics thread (every 10 seconds for heartbeat monitoring)
    std::thread statsThread([&]() {
        while (!shutdownFlag) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if (!shutdownFlag) {
                printStats6541();
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
                processUDPPacket6541(reinterpret_cast<uint8_t*>(buffer), n);
            }
        }
    }
    
    // Cleanup
    shutdownFlag = true;
    statsThread.join();
    SOCKET_CLOSE(sockfd);
    csvFile6541.close();

#ifdef _WIN32
    WSACleanup();
#endif
    
    printFinalStats6541();
    return true;
}

void stopMessage6541Receiver() {
    shutdownFlag = true;
}
