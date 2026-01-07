// NSE Capital Market Multicast UDP Receiver - Message 7201 Only
// 
// FOCUS: Only process message code 7201 (BCAST_MW_ROUND_ROBIN)
// OUTPUT: csv_output/message_7201_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Page 88, Table 39
// Structure: BCAST_MW_ROUND_ROBIN (466 bytes total)
// Layout: 
//   - BCAST_HEADER (40 bytes)
//   - NumberOfRecords (2 bytes) 
//   - MARKETWATCHBROADCAST[4] (4 records × 106 bytes each = 424 bytes)
//
// Converted from Go to C++

#include "message_7201_live.h"
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

namespace Message7201Receiver {
    // Basic counters
    std::atomic<int64_t> packetCount{0};
    std::atomic<int64_t> totalBytes{0};
    std::atomic<int64_t> compressedCount{0};
    std::atomic<int64_t> decompressedCount{0};
    std::atomic<int64_t> decompressionErrors{0};
    
    // 7201 specific counters
    std::atomic<int64_t> message7201Count{0};
    std::atomic<int64_t> message7201Saved{0};
    
    // CSV file for 7201
    std::ofstream csvFile7201;
    
    // Control variables
    std::chrono::steady_clock::time_point startTime;
    std::atomic<bool> shutdownFlag{false};
    
    // Track message codes for statistics
    std::map<uint16_t, int64_t> messageCodeCounts;
    std::mutex messageCodeMutex;
}

using namespace Message7201Receiver;

// =============================================================================
// CSV FUNCTIONS
// =============================================================================

bool initialize7201CSV() {
    // Create output directory
#ifdef _WIN32
    system("mkdir csv_output 2>nul");  // Suppress error if directory exists
#else
    system("mkdir -p csv_output");
#endif
    
    std::string timestamp = getFileTimestamp();
    std::string filename = "csv_output/message_7201_" + timestamp + ".csv";
    
    csvFile7201.open(filename);
    if (!csvFile7201.is_open()) {
        std::cerr << "❌ Failed to create CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write headers
    csvFile7201 << "Timestamp,TransactionCode,NumberOfRecords,RecordIndex,Token,"
                << "MarketTypeIndex,MboMbpIndicator,BuyVolume,BuyPrice,SellVolume,"
                << "SellPrice,LastTradePrice,LastTradeTime\n";
    csvFile7201.flush();
    
    std::cout << "📁 Created CSV file for Message 7201: " << filename << std::endl;
    return true;
}

void exportTo7201CSV(const Message7201Data& msg) {
    if (!csvFile7201.is_open()) {
        return;
    }
    
    std::string timestamp = getCurrentTimestamp();
    
    // Export each record and each market type within the record
    for (int i = 0; i < static_cast<int>(msg.numberOfRecords) && i < 4; i++) {
        const MarketWatchBroadcast& record = msg.records[i];
        
        for (int j = 0; j < 3; j++) {
            const MarketWiseInformation& mwInfo = record.marketWiseInfo[j];
            
            // Convert prices from paise to rupees
            double buyPriceRupees = static_cast<double>(mwInfo.buyPrice) / 100.0;
            double sellPriceRupees = static_cast<double>(mwInfo.sellPrice) / 100.0;
            double ltpRupees = static_cast<double>(mwInfo.lastTradePrice) / 100.0;

            csvFile7201 << timestamp << ","
                        << "7201" << ","
                        << msg.numberOfRecords << ","
                        << i << ","
                        << record.token << ","
                        << j << ","
                        << mwInfo.mboMbpIndicator << ","
                        << mwInfo.buyVolume << ","
                        << std::fixed << std::setprecision(2) << buyPriceRupees << ","
                        << mwInfo.sellVolume << ","
                        << sellPriceRupees << ","
                        << ltpRupees << ","
                        << mwInfo.lastTradeTime << "\n";
        }
    }
    
    csvFile7201.flush();
}

// =============================================================================
// MESSAGE PROCESSING
// =============================================================================

void process7201Message(const uint8_t* data, int dataLen) {
    if (dataLen < 466) { // Must have full 466-byte structure
        std::cout << "⚠️  Message 7201 too short: " << dataLen << " bytes (expected 466)" << std::endl;
        return;
    }
    
    message7201Count++;
    int64_t currentCount = message7201Count.load();
    
    // Parse NumberOfRecords at offset 40 (after 40-byte BCAST_HEADER)
    uint16_t numRecords = readUint16BigEndian(data, 40);
    
    if (numRecords > 4) {
        std::cout << "⚠️  Invalid NumberOfRecords: " << numRecords << " (max 4)" << std::endl;
        return;
    }
    
    if (currentCount == 1) {
        std::cout << "\n✅ First Message 7201 (BCAST_MW_ROUND_ROBIN) received" << std::endl;
        std::cout << "📊 Number of Records: " << numRecords << "\n" << std::endl;
    }
    
    Message7201Data msg;
    msg.numberOfRecords = numRecords;
    
    // MARKETWATCHBROADCAST array starts at offset 42
    int offset = 42;
    int recordLen = 106; // Each MARKETWATCHBROADCAST is 106 bytes
    
    for (int i = 0; i < static_cast<int>(numRecords) && i < 4; i++) {
        if (offset + recordLen > dataLen) {
            std::cout << "⚠️  Record " << i << " would exceed data length" << std::endl;
            break;
        }
        
        // Parse Token (4 bytes)
        msg.records[i].token = readUint32BigEndian(data, offset);
        
        // Parse 3 MARKETWISEINFORMATION blocks (34 bytes each)
        for (int j = 0; j < 3; j++) {
            int mwOffset = offset + 4 + j * 34;
            if (mwOffset + 34 > dataLen) {
                continue;
            }
            
            msg.records[i].marketWiseInfo[j].mboMbpIndicator = readUint16BigEndian(data, mwOffset);
            msg.records[i].marketWiseInfo[j].buyVolume = readUint64BigEndian(data, mwOffset + 2);
            msg.records[i].marketWiseInfo[j].buyPrice = readUint32BigEndian(data, mwOffset + 10);
            msg.records[i].marketWiseInfo[j].sellVolume = readUint64BigEndian(data, mwOffset + 14);
            msg.records[i].marketWiseInfo[j].sellPrice = readUint32BigEndian(data, mwOffset + 22);
            msg.records[i].marketWiseInfo[j].lastTradePrice = readUint32BigEndian(data, mwOffset + 26);
            msg.records[i].marketWiseInfo[j].lastTradeTime = readUint32BigEndian(data, mwOffset + 30);
        }
        
        offset += recordLen;
    }
    
    // Export to CSV
    exportTo7201CSV(msg);
    message7201Saved++;
}

bool processUDPPacket7201(const uint8_t* data, int dataLen) {
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
    
    // Only process message 7201
    if (transactionCode != 7201) {
        return false;
    }
    
    process7201Message(processData.data(), processData.size());
    return true;
}

// =============================================================================
// STATISTICS
// =============================================================================

void printStats7201() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t compressed = compressedCount.load();
    int64_t msg7201 = message7201Count.load();
    int64_t saved = message7201Saved.load();
    
    if (seconds > 0) {
        std::string status = "❌ NOT FOUND";
        if (msg7201 > 0) {
            status = "✅ RECEIVING";
        }
        
        std::cout << "⏱️  " << std::fixed << std::setprecision(0) << seconds 
                  << "s | 📦 " << packets << " pkts (" 
                  << std::setprecision(0) << (packets / seconds) << "/s) | 🗜️  " 
                  << compressed << " compressed | 🎯 7201: " << status 
                  << " | " << msg7201 << " msgs, " << saved << " saved" << std::endl;
    }
}

std::string formatNumber7201(int64_t n) {
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

void printFinalStats7201() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t bytes = totalBytes.load();
    double totalMB = static_cast<double>(bytes) / (1024.0 * 1024.0);
    int64_t compressed = compressedCount.load();
    int64_t decompressed = decompressedCount.load();
    int64_t errors = decompressionErrors.load();
    int64_t msg7201 = message7201Count.load();
    int64_t saved = message7201Saved.load();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "FINAL STATISTICS - MESSAGE 7201 DECODER (BCAST_MW_ROUND_ROBIN)" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    std::cout << "📊 LISTENER PERFORMANCE" << std::endl;
    std::cout << "  Runtime:              " << std::fixed << std::setprecision(0) 
              << seconds << " seconds" << std::endl;
    std::cout << "  Total Packets:        " << formatNumber7201(packets) << std::endl;
    std::cout << "  Total Data:           " << std::setprecision(1) << totalMB << " MB" << std::endl;
    
    if (seconds > 0) {
        std::cout << "  Avg Packet Rate:      " << std::setprecision(2) 
                  << (packets / seconds) << " packets/sec" << std::endl;
        std::cout << "  Avg Data Rate:        " << std::setprecision(2) 
                  << (totalMB * 1024 / seconds) << " KB/sec" << std::endl;
    }
    
    std::cout << std::endl << "📦 DECOMPRESSION STATISTICS" << std::endl;
    std::cout << "  Compressed Packets:   " << formatNumber7201(compressed);
    if (packets > 0) {
        std::cout << " (" << std::setprecision(1) 
                  << (static_cast<double>(compressed) * 100.0 / packets) << "%)";
    }
    std::cout << std::endl;
    std::cout << "  Decompressed OK:      " << formatNumber7201(decompressed) << std::endl;
    std::cout << "  Decompression Errors: " << formatNumber7201(errors) << std::endl;
    if (compressed > 0) {
        std::cout << "  Success Rate:         " << std::setprecision(1) 
                  << (static_cast<double>(decompressed) * 100.0 / compressed) << "%" << std::endl;
    }
    
    std::cout << std::endl << "🎯 MESSAGE 7201 STATISTICS (BCAST_MW_ROUND_ROBIN)" << std::endl;
    std::cout << "  Total Messages:       " << formatNumber7201(msg7201) << std::endl;
    std::cout << "  Messages Saved:       " << formatNumber7201(saved) << std::endl;
    
    std::cout << std::endl << "📁 CSV FILE CREATED" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "  Location: csv_output/" << std::endl;
    std::cout << "  Messages: " << formatNumber7201(saved) << std::endl;
    std::cout << "  Format: Market watch round robin with 4 records × 3 market types" << std::endl;
    
    std::cout << std::endl << std::string(80, '=') << std::endl;
    if (msg7201 > 0) {
        std::cout << "✅ SUCCESS: Market Watch Round Robin Messages (7201) processing completed" << std::endl;
        std::cout << "📊 Captured " << saved << " market watch snapshots" << std::endl;
    } else {
        std::cout << "⚠️  WARNING: No Market Watch Round Robin Messages (7201) found during session" << std::endl;
        std::cout << "💡 Note: Market watch messages contain snapshots for multiple tokens" << std::endl;
    }
    std::cout << "✅ Check csv_output/ for message_7201_*.csv file" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

// =============================================================================
// MAIN RECEIVER FUNCTION
// =============================================================================

bool runMessage7201Receiver(const std::string& multicastIP, int port) {
    startTime = std::chrono::steady_clock::now();
    shutdownFlag = false;
    
    // Reset counters
    packetCount = 0;
    totalBytes = 0;
    compressedCount = 0;
    decompressedCount = 0;
    decompressionErrors = 0;
    message7201Count = 0;
    message7201Saved = 0;
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
    std::cout << "NSE CM UDP Receiver - Message 7201 (BCAST_MW_ROUND_ROBIN)" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Listening for message code 7201 (0x1C21 in hex)" << std::endl;
    std::cout << "Purpose: Market watch round robin snapshots" << std::endl;
    std::cout << "Structure: 4 records × 3 market types × buy/sell/LTP data" << std::endl;
    std::cout << "Multicast: " << multicastIP << ":" << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    // Initialize CSV
    if (!initialize7201CSV()) {
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
                printStats7201();
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
                processUDPPacket7201(reinterpret_cast<uint8_t*>(buffer), n);
            }
        }
    }
    
    // Cleanup
    shutdownFlag = true;
    statsThread.join();
    SOCKET_CLOSE(sockfd);
    csvFile7201.close();

#ifdef _WIN32
    WSACleanup();
#endif
    
    printFinalStats7201();
    return true;
}

void stopMessage7201Receiver() {
    shutdownFlag = true;
}
