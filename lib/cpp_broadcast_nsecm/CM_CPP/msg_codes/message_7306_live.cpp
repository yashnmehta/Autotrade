// NSE Capital Market Multicast UDP Receiver - Message 7306 Only
// 
// FOCUS: Only process message code 7306 (BCAST_PART_MSTR_CHG - Participant Master Change)
// OUTPUT: csv_output/message_7306_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3
// Structure: PARTICIPANT MASTER CHANGE (44 bytes after BCAST_HEADER)
// Contains: Participant information and status changes
//
// Converted from Go to C++

#include "message_7306_live.h"
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
#include <cmath>

// Platform-specific includes - moved to utilities.h

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

namespace Message7306Receiver {
    // Basic counters
    std::atomic<int64_t> packetCount{0};
    std::atomic<int64_t> totalBytes{0};
    std::atomic<int64_t> compressedCount{0};
    std::atomic<int64_t> decompressedCount{0};
    std::atomic<int64_t> decompressionErrors{0};
    
    // 7306 specific counters
    std::atomic<int64_t> message7306Count{0};
    std::atomic<int64_t> message7306Saved{0};
    
    // CSV file for 7306
    std::ofstream csvFile7306;
    
    // Control variables
    std::chrono::steady_clock::time_point startTime;
    std::atomic<bool> shutdownFlag{false};
    
    // Track message codes for statistics
    std::map<uint16_t, int64_t> messageCodeCounts;
    std::mutex messageCodeMutex;
}

using namespace Message7306Receiver;

// =============================================================================
// CSV FUNCTIONS
// =============================================================================

bool initialize7306CSV() {
    // Create output directory
#ifdef _WIN32
    system("mkdir csv_output 2>nul");  // Suppress error if directory exists
#else
    system("mkdir -p csv_output");
#endif
    
    std::string timestamp = getFileTimestamp();
    std::string filename = "csv_output/message_7306_" + timestamp + ".csv";
    
    csvFile7306.open(filename);
    if (!csvFile7306.is_open()) {
        std::cerr << "❌ Failed to create CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write headers
    csvFile7306 << "Timestamp,TransactionCode,ParticipantId,ParticipantName,"
                << "ParticipantStatus,SuspendedDate,EffectiveDate,MarketAccess,"
                << "TradingRights\n";
    csvFile7306.flush();
    
    std::cout << "📁 Created CSV file for Message 7306: " << filename << std::endl;
    return true;
}

std::string getParticipantStatusName(uint16_t status) {
    switch (status) {
        case 0:
            return "Inactive";
        case 1:
            return "Active";
        case 2:
            return "Suspended";
        case 3:
            return "Debarred";
        case 4:
            return "Expelled";
        default:
            return "Unknown(" + std::to_string(status) + ")";
    }
}

void exportTo7306CSV(const Message7306Data& msg) {
    if (!csvFile7306.is_open()) {
        return;
    }
    
    // Clean up participant ID and name
    std::string participantId;
    for (int i = 0; i < 5; i++) {
        if (msg.participantId[i] != '\0') {
            participantId += msg.participantId[i];
        }
    }
    
    std::string participantName;
    for (int i = 0; i < 25; i++) {
        if (msg.participantName[i] != '\0') {
            participantName += msg.participantName[i];
        }
    }
    // Trim trailing spaces
    participantName.erase(participantName.find_last_not_of(" \t") + 1);
    
    csvFile7306 << getCurrentTimestamp() << ","
                << msg.transactionCode << ","
                << "\"" << participantId << "\","
                << "\"" << participantName << "\","
                << "\"" << getParticipantStatusName(msg.participantStatus) << "\","
                << msg.suspendedDate << ","
                << msg.effectiveDate << ","
                << msg.marketAccess << ","
                << msg.tradingRights << "\n";
    csvFile7306.flush();
    
    message7306Saved++;
}

// =============================================================================
// MESSAGE PROCESSING
// =============================================================================

void process7306Message(const uint8_t* data, int dataLen) {
    if (dataLen < 84) { // Minimum length check - 40-byte header + 44 bytes data
        return;
    }
    
    message7306Count++;
    int64_t currentCount = message7306Count.load();
    
    // Parse BCAST_HEADER (40 bytes)
    // Per NSE Protocol documentation:
    // Offset 10-12: TransactionCode (7306)
    
    uint16_t transactionCode = readUint16BigEndian(data, 10);
    
    // Show first message info
    if (currentCount == 1) {
        std::cout << "\n✅ First Message 7306 received: Participant Master Change\n" << std::endl;
    }
    
    // Parse message structure (BigEndian encoding)
    Message7306Data msg;
    memset(&msg, 0, sizeof(msg));
    
    // Transaction code from header
    msg.transactionCode = transactionCode;
    
    // Parse participant information starting after BCAST_HEADER (40 bytes)
    int offset = 40;
    
    // Participant ID (5 bytes)
    std::memcpy(msg.participantId, data + offset, 5);
    offset += 5;
    
    // Participant Name (25 bytes)
    std::memcpy(msg.participantName, data + offset, 25);
    offset += 25;
    
    // Participant Status (2 bytes)
    msg.participantStatus = readUint16BigEndian(data, offset);
    offset += 2;
    
    // Dates (4 bytes each)
    msg.suspendedDate = readUint32BigEndian(data, offset);
    offset += 4;
    msg.effectiveDate = readUint32BigEndian(data, offset);
    offset += 4;
    
    // Access and rights (2 bytes each)
    msg.marketAccess = readUint16BigEndian(data, offset);
    offset += 2;
    msg.tradingRights = readUint16BigEndian(data, offset);
    offset += 2;
    
    // Reserved bytes
    std::memcpy(msg.reserved, data + offset, 2);
    
    // Export to CSV
    exportTo7306CSV(msg);
}

bool processUDPPacket7306(const uint8_t* data, int dataLen) {
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
    
    // Only process message 7306
    if (transactionCode != 7306) {
        return false;
    }
    
    process7306Message(processData.data(), processData.size());
    return true;
}

// =============================================================================
// STATISTICS
// =============================================================================

void printStats7306() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t compressed = compressedCount.load();
    int64_t msg7306 = message7306Count.load();
    int64_t saved = message7306Saved.load();
    
    if (seconds > 0) {
        std::string status = "❌ NOT FOUND";
        if (msg7306 > 0) {
            status = "✅ RECEIVING";
        }
        
        std::cout << "⏱️  " << std::fixed << std::setprecision(0) << seconds 
                  << "s | 📦 " << packets << " pkts (" 
                  << std::setprecision(0) << (packets / seconds) << "/s) | 🗜️  " 
                  << compressed << " compressed | 🎯 7306: " << status 
                  << " | " << msg7306 << " msgs, " << saved << " saved" << std::endl;
    }
}

std::string formatNumber7306(int64_t n) {
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

void printFinalStats7306() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t bytes = totalBytes.load();
    double totalMB = static_cast<double>(bytes) / (1024.0 * 1024.0);
    int64_t compressed = compressedCount.load();
    int64_t decompressed = decompressedCount.load();
    int64_t errors = decompressionErrors.load();
    int64_t msg7306 = message7306Count.load();
    int64_t saved = message7306Saved.load();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "FINAL STATISTICS - MESSAGE 7306 DECODER (BCAST_PART_MSTR_CHG)" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    std::cout << "📊 LISTENER PERFORMANCE" << std::endl;
    std::cout << "  Runtime:              " << std::fixed << std::setprecision(0) 
              << seconds << " seconds" << std::endl;
    std::cout << "  Total Packets:        " << formatNumber7306(packets) << std::endl;
    std::cout << "  Total Data:           " << std::setprecision(1) << totalMB << " MB" << std::endl;
    
    if (seconds > 0) {
        std::cout << "  Avg Packet Rate:      " << std::setprecision(2) 
                  << (packets / seconds) << " packets/sec" << std::endl;
        std::cout << "  Avg Data Rate:        " << std::setprecision(2) 
                  << (totalMB * 1024 / seconds) << " KB/sec" << std::endl;
    }
    
    std::cout << std::endl << "📦 DECOMPRESSION STATISTICS" << std::endl;
    std::cout << "  Compressed Packets:   " << formatNumber7306(compressed);
    if (packets > 0) {
        std::cout << " (" << std::setprecision(1) 
                  << (static_cast<double>(compressed) * 100.0 / packets) << "%)";
    }
    std::cout << std::endl;
    std::cout << "  Decompressed OK:      " << formatNumber7306(decompressed) << std::endl;
    std::cout << "  Decompression Errors: " << formatNumber7306(errors) << std::endl;
    if (compressed > 0) {
        std::cout << "  Success Rate:         " << std::setprecision(1) 
                  << (static_cast<double>(decompressed) * 100.0 / compressed) << "%" << std::endl;
    }
    
    std::cout << std::endl << "🎯 MESSAGE 7306 STATISTICS (PARTICIPANT MASTER CHANGE)" << std::endl;
    std::cout << "  Messages Found:       " << formatNumber7306(msg7306) << std::endl;
    std::cout << "  Records Saved:        " << formatNumber7306(saved) << std::endl;
    
    std::cout << std::endl << "📁 CSV FILE CREATED" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "  Location: csv_output/" << std::endl;
    std::cout << "  Records: " << formatNumber7306(saved) << std::endl;
    std::cout << "  Format: Participant master change data" << std::endl;
    
    // Show all message codes found
    if (!messageCodeCounts.empty()) {
        std::cout << std::endl << "📋 MESSAGE CODES DETECTED (" << messageCodeCounts.size() << " unique)" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        std::cout << std::setw(8) << "Code" << std::setw(40) << "Description" << std::setw(10) << "Count" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        for (const auto& pair : messageCodeCounts) {
            std::string name = "Unknown";
            if (pair.first == 7306) {
                name = "BCAST_PART_MSTR_CHG";
            }
            
            if (pair.first == 7306) {
                std::cout << "🎯 " << std::setw(6) << pair.first << std::setw(38) << name 
                          << std::setw(10) << formatNumber7306(pair.second) << " ← TARGET!" << std::endl;
            } else {
                std::cout << "   " << std::setw(6) << pair.first << std::setw(38) << name 
                          << std::setw(10) << formatNumber7306(pair.second) << std::endl;
            }
        }
    }
    
    std::cout << std::endl << std::string(80, '=') << std::endl;
    if (msg7306 > 0) {
        std::cout << "✅ SUCCESS: Participant Master Change Messages (7306) processing completed" << std::endl;
    } else {
        std::cout << "⚠️  WARNING: No Participant Master Change Messages (7306) found during session" << std::endl;
        std::cout << "💡 Note: These messages contain participant status updates and changes" << std::endl;
    }
    std::cout << "✅ Check csv_output/ for message_7306_*.csv file" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

// =============================================================================
// MAIN RECEIVER FUNCTION
// =============================================================================

bool runMessage7306Receiver(const std::string& multicastIP, int port) {
    startTime = std::chrono::steady_clock::now();
    shutdownFlag = false;
    
    // Reset counters
    packetCount = 0;
    totalBytes = 0;
    compressedCount = 0;
    decompressedCount = 0;
    decompressionErrors = 0;
    message7306Count = 0;
    message7306Saved = 0;
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
    std::cout << "NSE CM UDP Receiver - Message 7306 (BCAST_PART_MSTR_CHG)" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Listening for message code 7306 (0x1C8A in hex)" << std::endl;
    std::cout << "Purpose: Participant master change notifications" << std::endl;
    std::cout << "Structure: 84 bytes total (40-byte header + 44-byte data)" << std::endl;
    std::cout << "Contains: Participant ID, name, status, dates, access rights" << std::endl;
    std::cout << "Multicast: " << multicastIP << ":" << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    // Initialize CSV
    if (!initialize7306CSV()) {
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
                printStats7306();
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
                processUDPPacket7306(reinterpret_cast<uint8_t*>(buffer), n);
            }
        }
    }
    
    // Cleanup
    shutdownFlag = true;
    statsThread.join();
    SOCKET_CLOSE(sockfd);
    csvFile7306.close();

#ifdef _WIN32
    WSACleanup();
#endif
    
    printFinalStats7306();
    return true;
}

void stopMessage7306Receiver() {
    shutdownFlag = true;
}
