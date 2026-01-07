// NSE Capital Market Multicast UDP Receiver - Message 6501 Only
// 
// FOCUS: Only process message code 6501 (BCAST_JRNL_VCT_MSG - Journal/VCT Messages)
// OUTPUT: csv_output/message_6501_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Page 79-80 (Table 23)
// Structure: MS_TRADER_INT_MSG (298 bytes)
// Contains: System messages, auction notifications, margin violations, listings
//
// Converted from Go to C++

#include "message_6501_live.h"
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

// Platform-specific includes - moved to utilities.h

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

namespace Message6501Receiver {
    // Basic counters
    std::atomic<int64_t> packetCount{0};
    std::atomic<int64_t> totalBytes{0};
    std::atomic<int64_t> compressedCount{0};
    std::atomic<int64_t> decompressedCount{0};
    std::atomic<int64_t> decompressionErrors{0};
    
    // 6501 specific counters
    std::atomic<int64_t> message6501Count{0};
    std::atomic<int64_t> message6501Saved{0};
    
    // CSV file for 6501
    std::ofstream csvFile6501;
    
    // Control variables
    std::chrono::steady_clock::time_point startTime;
    std::atomic<bool> shutdownFlag{false};
    
    // Track message codes for statistics
    std::map<uint16_t, int64_t> messageCodeCounts;
    std::mutex messageCodeMutex;
}

using namespace Message6501Receiver;

// =============================================================================
// HELPER FUNCTIONS - moved to utilities.cpp
// =============================================================================

// =============================================================================
// CSV FUNCTIONS
// =============================================================================

bool initialize6501CSV() {
    // Create output directory
#ifdef _WIN32
    system("mkdir csv_output 2>nul");  // Suppress error if directory exists
#else
    system("mkdir -p csv_output");
#endif
    
    std::string timestamp = getFileTimestamp();
    std::string filename = "csv_output/message_6501_" + timestamp + ".csv";
    
    csvFile6501.open(filename);
    if (!csvFile6501.is_open()) {
        std::cerr << "❌ Failed to create CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write headers
    csvFile6501 << "Timestamp,TransactionCode,BranchNumber,BrokerNumber,ActionCode,MsgLength,Message\n";
    csvFile6501.flush();
    
    std::cout << "📁 Created CSV file for Message 6501: " << filename << std::endl;
    return true;
}

void exportToCSV(uint16_t transactionCode, uint16_t branchNumber, 
                const std::string& brokerNumber, const std::string& actionCode,
                uint16_t msgLength, const std::string& message) {
    
    if (!csvFile6501.is_open()) {
        return;
    }
    
    csvFile6501 << getCurrentTimestamp() << ","
                << transactionCode << ","
                << branchNumber << ","
                << brokerNumber << ","
                << actionCode << ","
                << msgLength << ","
                << "\"" << message << "\"\n";
    csvFile6501.flush();
}

// =============================================================================
// MESSAGE PROCESSING
// =============================================================================

void process6501Message(const uint8_t* data, int dataLen) {
    if (dataLen < 298) {
        return;
    }
    
    message6501Count++;
    int64_t currentCount = message6501Count.load();
    
    uint16_t transactionCode = readUint16BigEndian(data, 10);
    
    // Parse message fields - Per Table 23
    uint16_t branchNumber = readUint16BigEndian(data, 40);
    
    // Extract broker number (5 bytes)
    std::string brokerNumber;
    for (int i = 0; i < 5; i++) {
        if (data[42 + i] != 0) {
            brokerNumber += static_cast<char>(data[42 + i]);
        }
    }
    
    // Extract action code (3 bytes)
    std::string actionCode;
    for (int i = 0; i < 3; i++) {
        if (data[47 + i] != 0) {
            actionCode += static_cast<char>(data[47 + i]);
        }
    }
    
    uint16_t msgLength = readUint16BigEndian(data, 56);
    
    // Extract message (up to msgLength from the 240 byte buffer)
    std::string message;
    int actualMsgLength = std::min(static_cast<int>(msgLength), 240);
    for (int i = 0; i < actualMsgLength; i++) {
        if (data[58 + i] != 0) {
            message += static_cast<char>(data[58 + i]);
        }
    }
    
    if (currentCount == 1) {
        std::cout << "\n✅ First Message 6501 received\n" << std::endl;
    }
    
    // Export to CSV
    exportToCSV(transactionCode, branchNumber, brokerNumber, actionCode, msgLength, message);
    message6501Saved++;
}

bool processUDPPacket(const uint8_t* data, int dataLen) {
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
    
    // Read transaction code
    uint16_t transactionCode = readUint16BigEndian(processData.data(), 10);
    
    // Track message codes
    {
        std::lock_guard<std::mutex> lock(messageCodeMutex);
        messageCodeCounts[transactionCode]++;
    }
    
    // Only process message 6501
    if (transactionCode != 6501) {
        return false;
    }
    
    process6501Message(processData.data());
    return true;
}

// =============================================================================
// STATISTICS
// =============================================================================

void printStats() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t compressed = compressedCount.load();
    int64_t msg6501 = message6501Count.load();
    
    if (seconds > 0) {
        std::string status = "❌ NOT FOUND";
        if (msg6501 > 0) {
            status = "✅ RECEIVING";
        }
        
        std::cout << "⏱️  " << std::fixed << std::setprecision(0) << seconds 
                  << "s | 📦 " << packets << " pkts (" 
                  << std::setprecision(0) << (packets / seconds) << "/s) | 🗜️  " 
                  << compressed << " compressed | 🎯 6501: " << status 
                  << " | " << msg6501 << " msgs" << std::endl;
    }
}

void printFinalStats() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t bytes = totalBytes.load();
    int64_t msg6501 = message6501Count.load();
    int64_t saved6501 = message6501Saved.load();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "📊 FINAL STATISTICS - MESSAGE 6501 DECODER" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Runtime                : " << std::fixed << std::setprecision(2) 
              << seconds << " seconds" << std::endl;
    std::cout << "Total Packets Received : " << packets << std::endl;
    std::cout << "Total Bytes Received   : " << bytes << " (" 
              << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MB)" << std::endl;
    
    if (seconds > 0) {
        std::cout << "Packets/Second         : " << std::setprecision(2) 
                  << (packets / seconds) << std::endl;
    }
    
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Message 6501 Found     : " << msg6501 << " messages" << std::endl;
    std::cout << "Messages Saved to CSV  : " << saved6501 << " records" << std::endl;
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
            double percentage = (static_cast<double>(cf.second) / packets) * 100.0;
            if (cf.first == 6501) {
                std::cout << "   🎯 Code " << std::setw(5) << cf.first << ": " 
                          << std::setw(6) << cf.second << " messages (" 
                          << std::setprecision(1) << percentage << "%) ← TARGET!" << std::endl;
            } else {
                std::cout << "      Code " << std::setw(5) << cf.first << ": " 
                          << std::setw(6) << cf.second << " messages (" 
                          << std::setprecision(1) << percentage << "%)" << std::endl;
            }
        }
        std::cout << std::string(80, '-') << std::endl;
    }
    
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "✅ Decoder stopped successfully!" << std::endl;
    if (msg6501 > 0) {
        std::cout << "📁 Check csv_output/ directory for the CSV file" << std::endl;
    }
    std::cout << std::endl;
}

// =============================================================================
// MAIN RECEIVER FUNCTION
// =============================================================================

bool runMessage6501Receiver(const std::string& multicastIP, int port) {
    startTime = std::chrono::steady_clock::now();
    shutdownFlag = false;
    
    // Reset counters
    packetCount = 0;
    totalBytes = 0;
    compressedCount = 0;
    decompressedCount = 0;
    decompressionErrors = 0;
    message6501Count = 0;
    message6501Saved = 0;
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
    std::cout << "║ NSE CM Message 6501 Receiver - Live Market Data          ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "📡 Multicast: " << multicastIP << ":" << port << std::endl;
    std::cout << "🎯 Target: Message 6501 (BCAST_JRNL_VCT_MSG)" << std::endl;
    std::cout << "📊 Statistics every 10 seconds" << std::endl;
    std::cout << "⏱️  Started at: " << getCurrentTimestamp() << std::endl << std::endl;
    std::cout << "Waiting for packets..." << std::endl << std::endl;
    
    // Initialize CSV
    if (!initialize6501CSV()) {
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
    // Windows doesn't have SO_REUSEPORT, but SO_REUSEADDR allows port reuse
#else
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    // On Linux, also set SO_REUSEPORT for better port sharing (if available)
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
    // On Windows, bind to INADDR_ANY for multicast
    addr.sin_addr.s_addr = INADDR_ANY;
#else
    // On Linux, can bind directly to multicast address
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
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if (!shutdownFlag) {
                printStats();
            }
        }
    });
    
    // Main packet processing loop
    char buffer[2048];  // Use char* for Windows compatibility
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
                // Cast buffer to uint8_t* for processing
                processUDPPacket(reinterpret_cast<uint8_t*>(buffer), n);
            }
        }
    }
    
    // Cleanup
    shutdownFlag = true;
    statsThread.join();
    SOCKET_CLOSE(sockfd);
    csvFile6501.close();

#ifdef _WIN32
    WSACleanup();
#endif
    
    printFinalStats();
    return true;
}

void stopMessage6501Receiver() {
    shutdownFlag = true;
}
