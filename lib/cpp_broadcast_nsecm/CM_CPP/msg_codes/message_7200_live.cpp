// NSE Capital Market Multicast UDP Receiver - Message 7200 Only
// 
// FOCUS: Only process message code 7200 (BCAST_MBO_MBP_UPDATE)
// OUTPUT: csv_output/message_7200_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Page 113-117
// Structure: BCAST_MBO_MBP_UPDATE (482 bytes)
// Contains: Order book depth with MBO (10 levels) + MBP (10 levels) + market data
//
// Converted from Go to C++

#include "message_7200_live.h"
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

namespace Message7200Receiver {
    // Basic counters
    std::atomic<int64_t> packetCount{0};
    std::atomic<int64_t> totalBytes{0};
    std::atomic<int64_t> compressedCount{0};
    std::atomic<int64_t> decompressedCount{0};
    std::atomic<int64_t> decompressionErrors{0};
    
    // 7200 specific counters
    std::atomic<int64_t> message7200Count{0};
    std::atomic<int64_t> message7200Saved{0};
    
    // CSV file for 7200
    std::ofstream csvFile7200;
    
    // Control variables
    std::chrono::steady_clock::time_point startTime;
    std::atomic<bool> shutdownFlag{false};
    
    // Track message codes for statistics
    std::map<uint16_t, int64_t> messageCodeCounts;
    std::mutex messageCodeMutex;
}

using namespace Message7200Receiver;

// =============================================================================
// CSV FUNCTIONS
// =============================================================================

bool initialize7200CSV() {
    // Create output directory
#ifdef _WIN32
    system("mkdir csv_output 2>nul");  // Suppress error if directory exists
#else
    system("mkdir -p csv_output");
#endif
    
    std::string timestamp = getFileTimestamp();
    std::string filename = "csv_output/message_7200_" + timestamp + ".csv";
    
    csvFile7200.open(filename);
    if (!csvFile7200.is_open()) {
        std::cerr << "❌ Failed to create CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write headers
    csvFile7200 << "Timestamp,TransactionCode,Token,BookType,TradingStatus,VolumeTradedToday,"
                << "LastTradedPrice,NetChangeIndicator,NetPriceChange,LastTradeQuantity,"
                << "LastTradeTime,AverageTradePrice,TotalBuyQuantity,TotalSellQuantity,"
                << "ClosingPrice,OpenPrice,HighPrice,LowPrice,BestBuyPrice,BestSellPrice,"
                << "BestBuyQty,BestSellQty\n";
    csvFile7200.flush();
    
    std::cout << "📁 Created CSV file for Message 7200: " << filename << std::endl;
    return true;
}

void exportTo7200CSV(const Message7200Data& msg) {
    if (!csvFile7200.is_open()) {
        return;
    }
    
    // Convert prices from paise to rupees
    double ltpRupees = static_cast<double>(msg.lastTradedPrice) / 100.0;
    double avgPriceRupees = static_cast<double>(msg.averageTradePrice) / 100.0;
    double closePriceRupees = static_cast<double>(msg.closingPrice) / 100.0;
    double openPriceRupees = static_cast<double>(msg.openPrice) / 100.0;
    double highPriceRupees = static_cast<double>(msg.highPrice) / 100.0;
    double lowPriceRupees = static_cast<double>(msg.lowPrice) / 100.0;
    double bestBuyPriceRupees = static_cast<double>(msg.bestBuyPrice) / 100.0;
    double bestSellPriceRupees = static_cast<double>(msg.bestSellPrice) / 100.0;
    double netChangeRupees = static_cast<double>(msg.netPriceChange) / 100.0;
    
    csvFile7200 << getCurrentTimestamp() << ","
                << "7200" << ","
                << msg.token << ","
                << msg.bookType << ","
                << msg.tradingStatus << ","
                << msg.volumeTradedToday << ","
                << std::fixed << std::setprecision(2) << ltpRupees << ","
                << static_cast<char>(msg.netChangeIndicator) << ","
                << netChangeRupees << ","
                << msg.lastTradeQuantity << ","
                << msg.lastTradeTime << ","
                << avgPriceRupees << ","
                << msg.totalBuyQuantity << ","
                << msg.totalSellQuantity << ","
                << closePriceRupees << ","
                << openPriceRupees << ","
                << highPriceRupees << ","
                << lowPriceRupees << ","
                << bestBuyPriceRupees << ","
                << bestSellPriceRupees << ","
                << msg.bestBuyQty << ","
                << msg.bestSellQty << "\n";
    csvFile7200.flush();
}

// =============================================================================
// MESSAGE PROCESSING
// =============================================================================

void process7200Message(const uint8_t* data, int dataLen) {
    if (dataLen < 482) { // Must have full 482-byte structure
        return;
    }
    
    message7200Count++;
    int64_t currentCount = message7200Count.load();
    
    // Parse message structure (BigEndian encoding)
    Message7200Data msg;
    
    // INTERACTIVE MBO DATA starts at offset 40
    int offset = 40;
    
    msg.token = readUint32BigEndian(data, offset);
    msg.bookType = readUint16BigEndian(data, offset + 4);
    msg.tradingStatus = readUint16BigEndian(data, offset + 6);
    msg.volumeTradedToday = readUint64BigEndian(data, offset + 8);
    msg.lastTradedPrice = readUint32BigEndian(data, offset + 16);
    msg.netChangeIndicator = data[offset + 20];
    // Skip 1 byte reserved field at offset 21
    msg.netPriceChange = readUint32BigEndian(data, offset + 22);
    msg.lastTradeQuantity = readUint32BigEndian(data, offset + 26);
    msg.lastTradeTime = readUint32BigEndian(data, offset + 30);
    msg.averageTradePrice = readUint32BigEndian(data, offset + 34);
    
    // MBPBuffer starts at offset 280 (first best buy/sell prices)
    int mbpOffset = 280;
    if (dataLen >= mbpOffset + 16) {
        msg.bestBuyQty = readUint64BigEndian(data, mbpOffset);
        msg.bestBuyPrice = readUint32BigEndian(data, mbpOffset + 8);
    }
    if (dataLen >= mbpOffset + 96) { // 80 bytes later for sell side
        msg.bestSellQty = readUint64BigEndian(data, mbpOffset + 80);
        msg.bestSellPrice = readUint32BigEndian(data, mbpOffset + 88);
    }
    
    // Total quantities at offset 444
    if (dataLen >= 460) {
        msg.totalBuyQuantity = readUint64BigEndian(data, 444);
        msg.totalSellQuantity = readUint64BigEndian(data, 452);
    }
    
    // OHLC prices
    if (dataLen >= 478) {
        msg.closingPrice = readUint32BigEndian(data, 462);
        msg.openPrice = readUint32BigEndian(data, 466);
        msg.highPrice = readUint32BigEndian(data, 470);
        msg.lowPrice = readUint32BigEndian(data, 474);
    }
    
    if (currentCount == 1) {
        std::cout << "\n✅ First Message 7200 (BCAST_MBO_MBP_UPDATE) received\n" << std::endl;
    }
    
    // Export to CSV
    exportTo7200CSV(msg);
    message7200Saved++;
}

bool processUDPPacket7200(const uint8_t* data, int dataLen) {
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
    
    // Only process message 7200
    if (transactionCode != 7200) {
        return false;
    }
    
    process7200Message(processData.data(), processData.size());
    return true;
}

// =============================================================================
// STATISTICS
// =============================================================================

void printStats7200() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t compressed = compressedCount.load();
    int64_t msg7200 = message7200Count.load();
    int64_t saved = message7200Saved.load();
    
    if (seconds > 0) {
        std::string status = "❌ NOT FOUND";
        if (msg7200 > 0) {
            status = "✅ RECEIVING";
        }
        
        std::cout << "⏱️  " << std::fixed << std::setprecision(0) << seconds 
                  << "s | 📦 " << packets << " pkts (" 
                  << std::setprecision(0) << (packets / seconds) << "/s) | 🗜️  " 
                  << compressed << " compressed | 🎯 7200: " << status 
                  << " | " << msg7200 << " msgs, " << saved << " saved" << std::endl;
    }
}

std::string formatNumber7200(int64_t n) {
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

void printFinalStats7200() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t bytes = totalBytes.load();
    double totalMB = static_cast<double>(bytes) / (1024.0 * 1024.0);
    int64_t compressed = compressedCount.load();
    int64_t decompressed = decompressedCount.load();
    int64_t errors = decompressionErrors.load();
    int64_t msg7200 = message7200Count.load();
    int64_t saved = message7200Saved.load();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "FINAL STATISTICS - MESSAGE 7200 DECODER (BCAST_MBO_MBP_UPDATE)" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    std::cout << "📊 LISTENER PERFORMANCE" << std::endl;
    std::cout << "  Runtime:              " << std::fixed << std::setprecision(0) 
              << seconds << " seconds" << std::endl;
    std::cout << "  Total Packets:        " << formatNumber7200(packets) << std::endl;
    std::cout << "  Total Data:           " << std::setprecision(1) << totalMB << " MB" << std::endl;
    
    if (seconds > 0) {
        std::cout << "  Avg Packet Rate:      " << std::setprecision(2) 
                  << (packets / seconds) << " packets/sec" << std::endl;
        std::cout << "  Avg Data Rate:        " << std::setprecision(2) 
                  << (totalMB * 1024 / seconds) << " KB/sec" << std::endl;
    }
    
    std::cout << std::endl << "📦 DECOMPRESSION STATISTICS" << std::endl;
    std::cout << "  Compressed Packets:   " << formatNumber7200(compressed);
    if (packets > 0) {
        std::cout << " (" << std::setprecision(1) 
                  << (static_cast<double>(compressed) * 100.0 / packets) << "%)";
    }
    std::cout << std::endl;
    std::cout << "  Decompressed OK:      " << formatNumber7200(decompressed) << std::endl;
    std::cout << "  Decompression Errors: " << formatNumber7200(errors) << std::endl;
    if (compressed > 0) {
        std::cout << "  Success Rate:         " << std::setprecision(1) 
                  << (static_cast<double>(decompressed) * 100.0 / compressed) << "%" << std::endl;
    }
    
    std::cout << std::endl << "🎯 MESSAGE 7200 STATISTICS (BCAST_MBO_MBP_UPDATE)" << std::endl;
    std::cout << "  Total Messages:       " << formatNumber7200(msg7200) << std::endl;
    std::cout << "  Messages Saved:       " << formatNumber7200(saved) << std::endl;
    
    std::cout << std::endl << "📁 CSV FILE CREATED" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "  Location: csv_output/" << std::endl;
    std::cout << "  Messages: " << formatNumber7200(saved) << std::endl;
    std::cout << "  Format: Market By Order/Price updates with order book depth" << std::endl;
    
    std::cout << std::endl << std::string(80, '=') << std::endl;
    if (msg7200 > 0) {
        std::cout << "✅ SUCCESS: Market By Order/Price Messages (7200) processing completed" << std::endl;
        std::cout << "📊 Captured " << saved << " MBO/MBP order book updates" << std::endl;
    } else {
        std::cout << "⚠️  WARNING: No Market By Order/Price Messages (7200) found during session" << std::endl;
        std::cout << "💡 Note: MBO/MBP messages contain real-time order book depth data" << std::endl;
    }
    std::cout << "✅ Check csv_output/ for message_7200_*.csv file" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

// =============================================================================
// MAIN RECEIVER FUNCTION
// =============================================================================

bool runMessage7200Receiver(const std::string& multicastIP, int port) {
    startTime = std::chrono::steady_clock::now();
    shutdownFlag = false;
    
    // Reset counters
    packetCount = 0;
    totalBytes = 0;
    compressedCount = 0;
    decompressedCount = 0;
    decompressionErrors = 0;
    message7200Count = 0;
    message7200Saved = 0;
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
    std::cout << "NSE CM UDP Receiver - Message 7200 (BCAST_MBO_MBP_UPDATE)" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Listening for message code 7200 (0x1C20 in hex)" << std::endl;
    std::cout << "Purpose: Market By Order/Price updates with order book depth" << std::endl;
    std::cout << "Structure: 482 bytes with MBO (10 levels) + MBP (10 levels)" << std::endl;
    std::cout << "Multicast: " << multicastIP << ":" << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    // Initialize CSV
    if (!initialize7200CSV()) {
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
                printStats7200();
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
                processUDPPacket7200(reinterpret_cast<uint8_t*>(buffer), n);
            }
        }
    }
    
    // Cleanup
    shutdownFlag = true;
    statsThread.join();
    SOCKET_CLOSE(sockfd);
    csvFile7200.close();

#ifdef _WIN32
    WSACleanup();
#endif
    
    printFinalStats7200();
    return true;
}

void stopMessage7200Receiver() {
    shutdownFlag = true;
}
