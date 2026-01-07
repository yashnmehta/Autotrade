// NSE Capital Market Multicast UDP Receiver - Message 7216 Only
// 
// FOCUS: Only process message code 7216 (BCAST_INDICES_VIX - India VIX Index)
// OUTPUT: csv_output/message_7216_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Pages 142-144
// Structure: INDICES (71 bytes per record)
// Contains: India VIX volatility index
//
// Converted from Go to C++

#include "message_7216_live.h"
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

namespace Message7216Receiver {
    // Basic counters
    std::atomic<int64_t> packetCount{0};
    std::atomic<int64_t> totalBytes{0};
    std::atomic<int64_t> compressedCount{0};
    std::atomic<int64_t> decompressedCount{0};
    std::atomic<int64_t> decompressionErrors{0};
    
    // 7216 specific counters
    std::atomic<int64_t> message7216Count{0};
    std::atomic<int64_t> message7216Saved{0};
    
    // CSV file for 7216
    std::ofstream csvFile7216;
    
    // Control variables
    std::chrono::steady_clock::time_point startTime;
    std::atomic<bool> shutdownFlag{false};
    
    // Track message codes for statistics
    std::map<uint16_t, int64_t> messageCodeCounts;
    std::mutex messageCodeMutex;
}

using namespace Message7216Receiver;

// =============================================================================
// CSV FUNCTIONS
// =============================================================================

bool initialize7216CSV() {
    // Create output directory
#ifdef _WIN32
    system("mkdir csv_output 2>nul");  // Suppress error if directory exists
#else
    system("mkdir -p csv_output");
#endif
    
    std::string timestamp = getFileTimestamp();
    std::string filename = "csv_output/message_7216_" + timestamp + ".csv";
    
    csvFile7216.open(filename);
    if (!csvFile7216.is_open()) {
        std::cerr << "❌ Failed to create CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write headers
    csvFile7216 << "Timestamp,TransactionCode,IndexName,IndexValue,"
                << "HighIndexValue,LowIndexValue,OpeningIndex,ClosingIndex,"
                << "PercentChange,YearlyHigh,YearlyLow,NoOfUpmoves,NoOfDownmoves,"
                << "MarketCapitalisation,NetChangeIndicator\n";
    csvFile7216.flush();
    
    std::cout << "📁 Created CSV file for Message 7216: " << filename << std::endl;
    return true;
}

void exportTo7216CSV(const IndexInfo7216& indexInfo) {
    if (!csvFile7216.is_open()) {
        return;
    }
    
    // Clean index name (remove null bytes and trim spaces)
    std::string indexName;
    for (int j = 0; j < 21; j++) {
        if (indexInfo.indexName[j] != 0) {
            indexName += static_cast<char>(indexInfo.indexName[j]);
        }
    }
    // Trim trailing spaces
    indexName.erase(indexName.find_last_not_of(" \t") + 1);
    
    // Convert index values from paise to actual value (÷100)
    double indexValue = static_cast<double>(indexInfo.indexValue) / 100.0;
    double highValue = static_cast<double>(indexInfo.highIndexValue) / 100.0;
    double lowValue = static_cast<double>(indexInfo.lowIndexValue) / 100.0;
    double openingValue = static_cast<double>(indexInfo.openingIndex) / 100.0;
    double closingValue = static_cast<double>(indexInfo.closingIndex) / 100.0;
    double yearlyHigh = static_cast<double>(indexInfo.yearlyHigh) / 100.0;
    double yearlyLow = static_cast<double>(indexInfo.yearlyLow) / 100.0;
    
    // Convert percent change from basis points to percentage (÷10000)
    double percentChange = static_cast<double>(indexInfo.percentChange) / 10000.0;
    
    csvFile7216 << getCurrentTimestamp() << ","
                << "7216,"
                << "\"" << indexName << "\","
                << std::fixed << std::setprecision(2) << indexValue << ","
                << highValue << ","
                << lowValue << ","
                << openingValue << ","
                << closingValue << ","
                << std::setprecision(4) << percentChange << "%,"
                << std::setprecision(2) << yearlyHigh << ","
                << yearlyLow << ","
                << indexInfo.noOfUpmoves << ","
                << indexInfo.noOfDownmoves << ","
                << indexInfo.marketCapitalisation << ","
                << static_cast<char>(indexInfo.netChangeIndicator) << "\n";
    csvFile7216.flush();
    
    message7216Saved++;
}

// =============================================================================
// MESSAGE PROCESSING
// =============================================================================

void process7216Message(const uint8_t* data, int dataLen) {
    if (dataLen < 468) { // Minimum length check for indices structure
        return;
    }
    
    message7216Count++;
    int64_t currentCount = message7216Count.load();
    
    // Parse BCAST_HEADER (40 bytes)
    // Per NSE Protocol documentation:
    // Offset 10-12: TransactionCode (7216)
    // After BCAST_HEADER: NoOfRecords (2 bytes)
    
    uint16_t transactionCode = readUint16BigEndian(data, 10);
    
    // NoOfRecords is right after the 40-byte BCAST_HEADER
    uint16_t noOfRecords = readUint16BigEndian(data, 40);
    
    // Show first message info
    if (currentCount == 1) {
        std::cout << "\n✅ First Message 7216 received: " << noOfRecords << " indices (VIX)\n" << std::endl;
    }
    
    if (noOfRecords == 0) {
        return;
    }
    
    // Parse Index records (71 bytes each)
    // Per documentation: INDICES array starts at offset 42
    int offset = 42;
    int recordSize = 71;
    
    for (int i = 0; i < static_cast<int>(noOfRecords) && i < 6; i++) { // Max 6 indices
        if (offset + recordSize > dataLen) {
            break;
        }
        
        IndexInfo7216 indexInfo;
        memset(&indexInfo, 0, sizeof(indexInfo));
        
        // Parse INDICES: 71 bytes per NSE Protocol specification
        // Use BIG ENDIAN - NSE CM standard protocol
        
        // Extract IndexName (21 bytes as per protocol)
        std::memcpy(indexInfo.indexName, data + offset, 21);
        
        // Parse numeric fields with big endian encoding
        indexInfo.indexValue = readUint32BigEndian(data, offset + 21);
        indexInfo.highIndexValue = readUint32BigEndian(data, offset + 25);
        indexInfo.lowIndexValue = readUint32BigEndian(data, offset + 29);
        indexInfo.openingIndex = readUint32BigEndian(data, offset + 33);
        indexInfo.closingIndex = readUint32BigEndian(data, offset + 37);
        indexInfo.percentChange = readUint32BigEndian(data, offset + 41);
        indexInfo.yearlyHigh = readUint32BigEndian(data, offset + 45);
        indexInfo.yearlyLow = readUint32BigEndian(data, offset + 49);
        indexInfo.noOfUpmoves = readUint32BigEndian(data, offset + 53);
        indexInfo.noOfDownmoves = readUint32BigEndian(data, offset + 57);
        
        // Reserved byte at offset 61
        indexInfo.reserved = data[offset + 61];
        
        // MarketCapitalisation is 8 bytes at offset 62-69 (DOUBLE)
        uint64_t marketCapBits = readUint64BigEndian(data, offset + 62);
        std::memcpy(&indexInfo.marketCapitalisation, &marketCapBits, sizeof(double));
        
        // NetChangeIndicator is at offset 70
        indexInfo.netChangeIndicator = data[offset + 70];
        
        // Export to CSV
        exportTo7216CSV(indexInfo);
        
        offset += recordSize;
    }
}

bool processUDPPacket7216(const uint8_t* data, int dataLen) {
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
    
    // Only process message 7216
    if (transactionCode != 7216) {
        return false;
    }
    
    process7216Message(processData.data(), processData.size());
    return true;
}

// =============================================================================
// STATISTICS
// =============================================================================

void printStats7216() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t compressed = compressedCount.load();
    int64_t msg7216 = message7216Count.load();
    int64_t saved = message7216Saved.load();
    
    if (seconds > 0) {
        std::string status = "❌ NOT FOUND";
        if (msg7216 > 0) {
            status = "✅ RECEIVING";
        }
        
        std::cout << "⏱️  " << std::fixed << std::setprecision(0) << seconds 
                  << "s | 📦 " << packets << " pkts (" 
                  << std::setprecision(0) << (packets / seconds) << "/s) | 🗜️  " 
                  << compressed << " compressed | 🎯 7216: " << status 
                  << " | " << msg7216 << " msgs, " << saved << " indices" << std::endl;
    }
}

std::string formatNumber7216(int64_t n) {
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

void printFinalStats7216() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t bytes = totalBytes.load();
    double totalMB = static_cast<double>(bytes) / (1024.0 * 1024.0);
    int64_t compressed = compressedCount.load();
    int64_t decompressed = decompressedCount.load();
    int64_t errors = decompressionErrors.load();
    int64_t msg7216 = message7216Count.load();
    int64_t saved = message7216Saved.load();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "FINAL STATISTICS - MESSAGE 7216 DECODER (BCAST_INDICES_VIX)" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    std::cout << "📊 LISTENER PERFORMANCE" << std::endl;
    std::cout << "  Runtime:              " << std::fixed << std::setprecision(0) 
              << seconds << " seconds" << std::endl;
    std::cout << "  Total Packets:        " << formatNumber7216(packets) << std::endl;
    std::cout << "  Total Data:           " << std::setprecision(1) << totalMB << " MB" << std::endl;
    
    if (seconds > 0) {
        std::cout << "  Avg Packet Rate:      " << std::setprecision(2) 
                  << (packets / seconds) << " packets/sec" << std::endl;
        std::cout << "  Avg Data Rate:        " << std::setprecision(2) 
                  << (totalMB * 1024 / seconds) << " KB/sec" << std::endl;
    }
    
    std::cout << std::endl << "📦 DECOMPRESSION STATISTICS" << std::endl;
    std::cout << "  Compressed Packets:   " << formatNumber7216(compressed);
    if (packets > 0) {
        std::cout << " (" << std::setprecision(1) 
                  << (static_cast<double>(compressed) * 100.0 / packets) << "%)";
    }
    std::cout << std::endl;
    std::cout << "  Decompressed OK:      " << formatNumber7216(decompressed) << std::endl;
    std::cout << "  Decompression Errors: " << formatNumber7216(errors) << std::endl;
    if (compressed > 0) {
        std::cout << "  Success Rate:         " << std::setprecision(1) 
                  << (static_cast<double>(decompressed) * 100.0 / compressed) << "%" << std::endl;
    }
    
    std::cout << std::endl << "🎯 MESSAGE 7216 STATISTICS (BCAST_INDICES_VIX)" << std::endl;
    std::cout << "  Total Messages:       " << formatNumber7216(msg7216) << std::endl;
    std::cout << "  Indices Saved:        " << formatNumber7216(saved) << std::endl;
    if (msg7216 > 0) {
        std::cout << "  Avg Indices/Message:  " << std::setprecision(2) 
                  << (static_cast<double>(saved) / msg7216) << std::endl;
    }
    
    std::cout << std::endl << "📁 CSV FILE CREATED" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "  Location: csv_output/" << std::endl;
    std::cout << "  Records: " << formatNumber7216(saved) << std::endl;
    std::cout << "  Format: India VIX volatility index data" << std::endl;
    
    // Show all message codes found
    if (!messageCodeCounts.empty()) {
        std::cout << std::endl << "📋 ALL MESSAGE CODES DETECTED:" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        for (const auto& pair : messageCodeCounts) {
            double percentage = static_cast<double>(pair.second) / packets * 100;
            if (pair.first == 7216) {
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
    if (msg7216 > 0) {
        std::cout << "✅ SUCCESS: India VIX Index Messages (7216) processing completed" << std::endl;
        std::cout << "📊 Captured " << saved << " VIX index records" << std::endl;
    } else {
        std::cout << "⚠️  WARNING: No India VIX Index Messages (7216) found during session" << std::endl;
        std::cout << "💡 Note: VIX messages contain volatility index data" << std::endl;
    }
    std::cout << "✅ Check csv_output/ for message_7216_*.csv file" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

// =============================================================================
// MAIN RECEIVER FUNCTION
// =============================================================================

bool runMessage7216Receiver(const std::string& multicastIP, int port) {
    startTime = std::chrono::steady_clock::now();
    shutdownFlag = false;
    
    // Reset counters
    packetCount = 0;
    totalBytes = 0;
    compressedCount = 0;
    decompressedCount = 0;
    decompressionErrors = 0;
    message7216Count = 0;
    message7216Saved = 0;
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
    std::cout << "NSE CM UDP Receiver - Message 7216 (BCAST_INDICES_VIX)" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Listening for message code 7216 (0x1C38 in hex)" << std::endl;
    std::cout << "Purpose: India VIX volatility index broadcasting" << std::endl;
    std::cout << "Structure: Up to 6 indices per message (71 bytes each)" << std::endl;
    std::cout << "Contains: VIX values, OHLC, yearly range, market cap" << std::endl;
    std::cout << "Multicast: " << multicastIP << ":" << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    // Initialize CSV
    if (!initialize7216CSV()) {
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
                printStats7216();
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
                processUDPPacket7216(reinterpret_cast<uint8_t*>(buffer), n);
            }
        }
    }
    
    // Cleanup
    shutdownFlag = true;
    statsThread.join();
    SOCKET_CLOSE(sockfd);
    csvFile7216.close();

#ifdef _WIN32
    WSACleanup();
#endif
    
    printFinalStats7216();
    return true;
}

void stopMessage7216Receiver() {
    shutdownFlag = true;
}
