// NSE Capital Market Multicast UDP Receiver - Message 7207 Only
// 
// FOCUS: Only process message code 7207 (BCAST_INDICES)
// OUTPUT: csv_output/message_7207_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Page 139
// Structure: INDICES (71 bytes per record)
// Maximum Records: 6 per broadcast packet
//
// Converted from Go to C++

#include "message_7207_live.h"
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

namespace Message7207Receiver {
    // Basic counters
    std::atomic<int64_t> packetCount{0};
    std::atomic<int64_t> totalBytes{0};
    std::atomic<int64_t> compressedCount{0};
    std::atomic<int64_t> decompressedCount{0};
    std::atomic<int64_t> decompressionErrors{0};
    
    // 7207 specific counters
    std::atomic<int64_t> message7207Count{0};
    std::atomic<int64_t> message7207Saved{0};
    
    // CSV file for 7207
    std::ofstream csvFile7207;
    
    // Control variables
    std::chrono::steady_clock::time_point startTime;
    std::atomic<bool> shutdownFlag{false};
    
    // Track message codes for statistics
    std::map<uint16_t, int64_t> messageCodeCounts;
    std::mutex messageCodeMutex;
}

using namespace Message7207Receiver;

// =============================================================================
// CSV FUNCTIONS
// =============================================================================

bool initialize7207CSV() {
    // Create output directory
#ifdef _WIN32
    system("mkdir csv_output 2>nul");  // Suppress error if directory exists
#else
    system("mkdir -p csv_output");
#endif
    
    std::string timestamp = getFileTimestamp();
    std::string filename = "csv_output/message_7207_" + timestamp + ".csv";
    
    csvFile7207.open(filename);
    if (!csvFile7207.is_open()) {
        std::cerr << "❌ Failed to create CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write headers
    csvFile7207 << "Timestamp,TransactionCode,NoOfRecords,IndexName,IndexValue,"
                << "HighIndexValue,LowIndexValue,OpeningIndex,ClosingIndex,"
                << "PercentChange,YearlyHigh,YearlyLow,NoOfUpmoves,NoOfDownmoves,"
                << "MarketCapitalisation,NetChangeIndicator\n";
    csvFile7207.flush();
    
    std::cout << "📁 Created CSV file for Message 7207: " << filename << std::endl;
    return true;
}

void exportTo7207CSV(uint16_t transactionCode, uint16_t noOfRecords, const std::string& indexName,
                    int32_t indexValue, int32_t highIndexValue, int32_t lowIndexValue,
                    int32_t openingIndex, int32_t closingIndex, int32_t percentChange,
                    int32_t yearlyHigh, int32_t yearlyLow, int32_t noOfUpmoves,
                    int32_t noOfDownmoves, double marketCap, uint8_t netChangeIndicator) {
    
    if (!csvFile7207.is_open()) {
        return;
    }
    
    csvFile7207 << getCurrentTimestamp() << ","
                << transactionCode << ","
                << noOfRecords << ","
                << "\"" << indexName << "\","
                << std::fixed << std::setprecision(2) << (static_cast<double>(indexValue) / 100.0) << ","
                << (static_cast<double>(highIndexValue) / 100.0) << ","
                << (static_cast<double>(lowIndexValue) / 100.0) << ","
                << (static_cast<double>(openingIndex) / 100.0) << ","
                << (static_cast<double>(closingIndex) / 100.0) << ","
                << std::setprecision(4) << (static_cast<double>(percentChange) / 10000.0) << "%,"
                << std::setprecision(2) << (static_cast<double>(yearlyHigh) / 100.0) << ","
                << (static_cast<double>(yearlyLow) / 100.0) << ","
                << noOfUpmoves << ","
                << noOfDownmoves << ","
                << marketCap << ","
                << static_cast<char>(netChangeIndicator) << "\n";
    csvFile7207.flush();
}

// =============================================================================
// MESSAGE PROCESSING
// =============================================================================

void process7207Message(const uint8_t* data, int dataLen) {
    if (dataLen < 42) { // 40-byte header + at least 2 bytes for NoOfRecords
        return;
    }
    
    message7207Count++;
    int64_t currentCount = message7207Count.load();
    
    // Parse BCAST_HEADER (40 bytes)
    // Per NSE Protocol documentation:
    // Offset 10-12: TransactionCode (7207)
    // After BCAST_HEADER: NoOfRecords (2 bytes)
    
    uint16_t transactionCode = readUint16BigEndian(data, 10);
    
    // NoOfRecords is right after the 40-byte BCAST_HEADER
    uint16_t noOfRecords = readUint16BigEndian(data, 40);
    
    // Show first message info
    if (currentCount == 1) {
        std::cout << "\n✅ First Message 7207 received: " << noOfRecords << " indices\n" << std::endl;
    }
    
    // Parse Indices records
    // Per documentation Table 43.1: INDICES array starts at offset 42
    // CRITICAL FIX: Actual record size is 72 bytes (71 bytes per protocol + 1 padding byte)
    // Diagnostic shows next record starting at byte 72, not 71
    
    int offset = 42;
    int recordSize = 72;
    
    for (int i = 0; i < static_cast<int>(noOfRecords) && i < 6; i++) { // Max 6 indices
        if (offset + recordSize > dataLen) {
            break;
        }
        
        // Parse INDICES: 71 bytes per NSE Protocol Table 43.1 (+ 1 padding = 72 total per record)
        // Diagnostic confirms: IndexName is 21 bytes, followed by numeric fields at correct offsets
        // The 1 extra byte is AFTER all fields (padding between records)
        
        // Extract IndexName (21 bytes as per protocol)
        std::string indexName;
        for (int j = 0; j < 21; j++) {
            if (data[offset + j] != 0) {
                indexName += static_cast<char>(data[offset + j]);
            }
        }
        // Trim trailing spaces
        indexName.erase(indexName.find_last_not_of(" \t") + 1);
        
        // Use BIG ENDIAN - NSE CM standard protocol (matches working 18703 decoder)
        // All offsets match protocol specification exactly
        int32_t indexValue = static_cast<int32_t>(readUint32BigEndian(data, offset + 21));
        int32_t highIndexValue = static_cast<int32_t>(readUint32BigEndian(data, offset + 25));
        int32_t lowIndexValue = static_cast<int32_t>(readUint32BigEndian(data, offset + 29));
        int32_t openingIndex = static_cast<int32_t>(readUint32BigEndian(data, offset + 33));
        int32_t closingIndex = static_cast<int32_t>(readUint32BigEndian(data, offset + 37));
        int32_t percentChange = static_cast<int32_t>(readUint32BigEndian(data, offset + 41));
        int32_t yearlyHigh = static_cast<int32_t>(readUint32BigEndian(data, offset + 45));
        int32_t yearlyLow = static_cast<int32_t>(readUint32BigEndian(data, offset + 49));
        int32_t noOfUpmoves = static_cast<int32_t>(readUint32BigEndian(data, offset + 53));
        int32_t noOfDownmoves = static_cast<int32_t>(readUint32BigEndian(data, offset + 57));
        
        // CRITICAL FIX: MarketCapitalisation is actually 8 bytes at offset 62-69
        // Based on diagnostic: offset 62-69 gives valid DOUBLE value
        uint64_t marketCapBits = readUint64BigEndian(data, offset + 62);
        double marketCap = 0.0;
        std::memcpy(&marketCap, &marketCapBits, sizeof(double));
        
        // NetChangeIndicator is at offset 70 (diagnostic showed '-' at byte 70)
        uint8_t netChangeIndicator = data[offset + 70];
        // Byte at offset+71 is padding between records
        
        // Export to CSV
        exportTo7207CSV(transactionCode, noOfRecords, indexName, indexValue, highIndexValue,
                       lowIndexValue, openingIndex, closingIndex, percentChange, yearlyHigh, yearlyLow,
                       noOfUpmoves, noOfDownmoves, marketCap, netChangeIndicator);
        message7207Saved++;
        
        offset += recordSize;
    }
}

bool processUDPPacket7207(const uint8_t* data, int dataLen) {
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
    
    // Only process message 7207
    if (transactionCode != 7207) {
        return false;
    }
    
    process7207Message(processData.data(), processData.size());
    return true;
}

// =============================================================================
// STATISTICS
// =============================================================================

void printStats7207() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t compressed = compressedCount.load();
    int64_t msg7207 = message7207Count.load();
    int64_t saved = message7207Saved.load();
    
    if (seconds > 0) {
        std::string status = "❌ NOT FOUND";
        if (msg7207 > 0) {
            status = "✅ RECEIVING";
        }
        
        std::cout << "⏱️  " << std::fixed << std::setprecision(0) << seconds 
                  << "s | 📦 " << packets << " pkts (" 
                  << std::setprecision(0) << (packets / seconds) << "/s) | 🗜️  " 
                  << compressed << " compressed | 🎯 7207: " << status 
                  << " | " << msg7207 << " msgs, " << saved << " indices" << std::endl;
    }
}

std::string formatNumber7207(int64_t n) {
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

void printFinalStats7207() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t bytes = totalBytes.load();
    double totalMB = static_cast<double>(bytes) / (1024.0 * 1024.0);
    int64_t compressed = compressedCount.load();
    int64_t decompressed = decompressedCount.load();
    int64_t errors = decompressionErrors.load();
    int64_t msg7207 = message7207Count.load();
    int64_t saved = message7207Saved.load();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "FINAL STATISTICS - MESSAGE 7207 DECODER (BCAST_INDICES)" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    std::cout << "📊 LISTENER PERFORMANCE" << std::endl;
    std::cout << "  Runtime:              " << std::fixed << std::setprecision(0) 
              << seconds << " seconds" << std::endl;
    std::cout << "  Total Packets:        " << formatNumber7207(packets) << std::endl;
    std::cout << "  Total Data:           " << std::setprecision(1) << totalMB << " MB" << std::endl;
    
    if (seconds > 0) {
        std::cout << "  Avg Packet Rate:      " << std::setprecision(2) 
                  << (packets / seconds) << " packets/sec" << std::endl;
        std::cout << "  Avg Data Rate:        " << std::setprecision(2) 
                  << (totalMB * 1024 / seconds) << " KB/sec" << std::endl;
    }
    
    std::cout << std::endl << "📦 DECOMPRESSION STATISTICS" << std::endl;
    std::cout << "  Compressed Packets:   " << formatNumber7207(compressed);
    if (packets > 0) {
        std::cout << " (" << std::setprecision(1) 
                  << (static_cast<double>(compressed) * 100.0 / packets) << "%)";
    }
    std::cout << std::endl;
    std::cout << "  Decompressed OK:      " << formatNumber7207(decompressed) << std::endl;
    std::cout << "  Decompression Errors: " << formatNumber7207(errors) << std::endl;
    if (compressed > 0) {
        std::cout << "  Success Rate:         " << std::setprecision(1) 
                  << (static_cast<double>(decompressed) * 100.0 / compressed) << "%" << std::endl;
    }
    
    std::cout << std::endl << "🎯 MESSAGE 7207 STATISTICS (BCAST_INDICES)" << std::endl;
    std::cout << "  Total Messages:       " << formatNumber7207(msg7207) << std::endl;
    std::cout << "  Indices Saved:        " << formatNumber7207(saved) << std::endl;
    if (msg7207 > 0) {
        std::cout << "  Avg Indices/Message:  " << std::setprecision(2) 
                  << (static_cast<double>(saved) / msg7207) << std::endl;
    }
    
    std::cout << std::endl << "📁 CSV FILE CREATED" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "  Location: csv_output/" << std::endl;
    std::cout << "  Records: " << formatNumber7207(saved) << std::endl;
    std::cout << "  Format: Stock market indices (Nifty, Sensex, etc.)" << std::endl;
    
    // Show all message codes found
    if (!messageCodeCounts.empty()) {
        std::cout << std::endl << "📋 ALL MESSAGE CODES DETECTED:" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        for (const auto& pair : messageCodeCounts) {
            double percentage = static_cast<double>(pair.second) / packets * 100;
            if (pair.first == 7207) {
                std::cout << "   🎯 Code " << std::setw(5) << pair.first << ": " 
                          << std::setw(6) << pair.second << " messages (" 
                          << std::setprecision(1) << percentage << "%) ← TARGET!" << std::endl;
            } else {
                std::cout << "      Code " << std::setw(5) << pair.first << ": " 
                          << std::setw(6) << pair.second << " messages (" 
                          << std::setprecision(1) << percentage << "%)" << std::endl;
            }
        }
        std::cout << std::string(80, '-') << std::endl;
    }
    
    std::cout << std::endl << std::string(80, '=') << std::endl;
    if (msg7207 > 0) {
        std::cout << "✅ SUCCESS: Broadcast Indices Messages (7207) processing completed" << std::endl;
        std::cout << "📊 Captured " << saved << " stock market indices" << std::endl;
    } else {
        std::cout << "⚠️  WARNING: No Broadcast Indices Messages (7207) found during session" << std::endl;
        std::cout << "💡 Note: Indices messages contain Nifty, Sensex, Bank Nifty, etc." << std::endl;
    }
    std::cout << "✅ Check csv_output/ for message_7207_*.csv file" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

// =============================================================================
// MAIN RECEIVER FUNCTION
// =============================================================================

bool runMessage7207Receiver(const std::string& multicastIP, int port) {
    startTime = std::chrono::steady_clock::now();
    shutdownFlag = false;
    
    // Reset counters
    packetCount = 0;
    totalBytes = 0;
    compressedCount = 0;
    decompressedCount = 0;
    decompressionErrors = 0;
    message7207Count = 0;
    message7207Saved = 0;
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
    std::cout << "NSE CM UDP Receiver - Message 7207 (BCAST_INDICES)" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Listening for message code 7207 (0x1C2F in hex)" << std::endl;
    std::cout << "Purpose: Broadcast stock market indices" << std::endl;
    std::cout << "Structure: Up to 6 indices per message (71 bytes each + padding)" << std::endl;
    std::cout << "Contains: Nifty, Sensex, Bank Nifty, sector indices, etc." << std::endl;
    std::cout << "Multicast: " << multicastIP << ":" << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    // Initialize CSV
    if (!initialize7207CSV()) {
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
                printStats7207();
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
                processUDPPacket7207(reinterpret_cast<uint8_t*>(buffer), n);
            }
        }
    }
    
    // Cleanup
    shutdownFlag = true;
    statsThread.join();
    SOCKET_CLOSE(sockfd);
    csvFile7207.close();

#ifdef _WIN32
    WSACleanup();
#endif
    
    printFinalStats7207();
    return true;
}

void stopMessage7207Receiver() {
    shutdownFlag = true;
}
