// NSE Capital Market Multicast UDP Receiver - Message 7208 Only
// 
// FOCUS: Only process message code 7208 (BCAST_ONLY_MBP - Market By Price Only)
// OUTPUT: csv_output/message_7208_TIMESTAMP.csv
//
// Protocol Reference: NSE CM NNF Protocol v6.3, Pages 118-123
// Structure: BROADCAST ONLY MBP (262 bytes per record)
// Contains: Market By Price data (10 levels without order count)
//
// Converted from Go to C++

#include "message_7208_live.h"
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

namespace Message7208Receiver {
    // Basic counters
    std::atomic<int64_t> packetCount{0};
    std::atomic<int64_t> totalBytes{0};
    std::atomic<int64_t> compressedCount{0};
    std::atomic<int64_t> decompressedCount{0};
    std::atomic<int64_t> decompressionErrors{0};
    
    // 7208 specific counters
    std::atomic<int64_t> message7208Count{0};
    std::atomic<int64_t> message7208Saved{0};
    
    // CSV file for 7208
    std::ofstream csvFile7208;
    
    // Control variables
    std::chrono::steady_clock::time_point startTime;
    std::atomic<bool> shutdownFlag{false};
    
    // Track message codes for statistics
    std::map<uint16_t, int64_t> messageCodeCounts;
    std::mutex messageCodeMutex;
}

using namespace Message7208Receiver;

// =============================================================================
// CSV FUNCTIONS
// =============================================================================

bool initialize7208CSV() {
    // Create output directory
#ifdef _WIN32
    system("mkdir csv_output 2>nul");  // Suppress error if directory exists
#else
    system("mkdir -p csv_output");
#endif
    
    std::string timestamp = getFileTimestamp();
    std::string filename = "csv_output/message_7208_" + timestamp + ".csv";
    
    csvFile7208.open(filename);
    if (!csvFile7208.is_open()) {
        std::cerr << "❌ Failed to create CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write headers
    csvFile7208 << "Timestamp,TransactionCode,Token,BookType,TradingStatus,"
                << "VolumeTradedToday,LastTradedPrice,NetChangeIndicator,NetPriceChange,"
                << "LastTradeQuantity,LastTradeTime,AverageTradePrice,TotalBuyQuantity,"
                << "TotalSellQuantity,ClosingPrice,OpenPrice,HighPrice,LowPrice,"
                << "IndicativeClosePrice,BestBuyPrice_1,BestBuyQty_1,BestSellPrice_1,"
                << "BestSellQty_1,BestBuyPrice_2,BestBuyQty_2,BestSellPrice_2,BestSellQty_2,"
                << "BestBuyPrice_3,BestBuyQty_3,BestSellPrice_3,BestSellQty_3,"
                << "BestBuyPrice_4,BestBuyQty_4,BestSellPrice_4,BestSellQty_4,"
                << "BestBuyPrice_5,BestBuyQty_5,BestSellPrice_5,BestSellQty_5\n";
    csvFile7208.flush();
    
    std::cout << "📁 Created CSV file for Message 7208: " << filename << std::endl;
    return true;
}

void exportTo7208CSV(const Message7208Data& msg) {
    if (!csvFile7208.is_open()) {
        return;
    }
    
    // Convert prices from paise to rupees
    double ltpRupees = static_cast<double>(msg.lastTradedPrice) / 100.0;
    double avgPriceRupees = static_cast<double>(msg.averageTradePrice) / 100.0;
    double closePriceRupees = static_cast<double>(msg.closingPrice) / 100.0;
    double openPriceRupees = static_cast<double>(msg.openPrice) / 100.0;
    double highPriceRupees = static_cast<double>(msg.highPrice) / 100.0;
    double lowPriceRupees = static_cast<double>(msg.lowPrice) / 100.0;
    double indicativeClosePriceRupees = static_cast<double>(msg.indicativeClosePrice) / 100.0;
    double netChangeRupees = static_cast<double>(msg.netPriceChangeFromClosingPrice) / 100.0;
    
    // Extract buy and sell prices (first 5 levels of each)
    double buyPrices[5] = {0.0};
    int64_t buyQtys[5] = {0};
    double sellPrices[5] = {0.0};
    int64_t sellQtys[5] = {0};
    
    int buyIdx = 0;
    int sellIdx = 0;
    
    for (int i = 0; i < 10; i++) {
        if (msg.mbpData[i].bbBuySellFlag == 0 && buyIdx < 5) { // Buy order
            buyPrices[buyIdx] = static_cast<double>(msg.mbpData[i].price) / 100.0;
            buyQtys[buyIdx] = msg.mbpData[i].quantity;
            buyIdx++;
        } else if (msg.mbpData[i].bbBuySellFlag == 1 && sellIdx < 5) { // Sell order
            sellPrices[sellIdx] = static_cast<double>(msg.mbpData[i].price) / 100.0;
            sellQtys[sellIdx] = msg.mbpData[i].quantity;
            sellIdx++;
        }
    }
    
    csvFile7208 << getCurrentTimestamp() << ","
                << "7208,"
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
                << indicativeClosePriceRupees << ",";
    
    // Add buy/sell prices and quantities
    for (int i = 0; i < 5; i++) {
        csvFile7208 << buyPrices[i] << "," << buyQtys[i] << ","
                    << sellPrices[i] << "," << sellQtys[i];
        if (i < 4) csvFile7208 << ",";
    }
    csvFile7208 << "\n";
    csvFile7208.flush();
    
    message7208Saved++;
}

// =============================================================================
// MESSAGE PROCESSING
// =============================================================================

void process7208Message(const uint8_t* data, int dataLen) {
    if (dataLen < 42) { // 40-byte header + at least 2 bytes for NoOfRecords
        return;
    }
    
    message7208Count++;
    int64_t currentCount = message7208Count.load();
    
    // Parse BCAST_HEADER (40 bytes)
    // Per NSE Protocol documentation:
    // Offset 10-12: TransactionCode (7208)
    // After BCAST_HEADER: NoOfRecords (2 bytes)
    
    uint16_t transactionCode = readUint16BigEndian(data, 10);
    
    // NoOfRecords is right after the 40-byte BCAST_HEADER
    uint16_t noOfRecords = readUint16BigEndian(data, 40);
    
    // Debug for first few messages
    if (currentCount <= 3) {
        std::cout << "🔍 Message 7208 #" << currentCount << ": " << noOfRecords 
                  << " records, data length = " << dataLen << std::endl;
    }
    
    if (noOfRecords == 0) {
        if (currentCount <= 3) {
            std::cout << "⚠️ Message 7208 #" << currentCount << ": No records to process" << std::endl;
        }
        return;
    }
    
    // Process each record (262 bytes per record)
    // Records start at offset 42 (after 40-byte BCAST_HEADER + 2-byte NoOfRecords)
    int offset = 42;
    int recordSize = 262;
    int recordsProcessed = 0;
    
    for (int i = 0; i < static_cast<int>(noOfRecords) && i < 2; i++) { // Max 2 records
        if (offset + recordSize > dataLen) {
            if (currentCount <= 3) {
                std::cout << "❌ Message 7208 #" << currentCount << ": Record " << (i+1) 
                          << " would exceed data length (offset " << offset << " + 262 > " 
                          << dataLen << ")" << std::endl;
            }
            break;
        }
        
        Message7208Data msg;
        memset(&msg, 0, sizeof(msg));
        
        // Parse BROADCAST ONLY MBP DATA structure
        // Use BIG ENDIAN - NSE CM standard protocol
        msg.token = readUint32BigEndian(data, offset);
        msg.bookType = readUint16BigEndian(data, offset + 4);
        msg.tradingStatus = readUint16BigEndian(data, offset + 6);
        msg.volumeTradedToday = static_cast<int64_t>(readUint64BigEndian(data, offset + 8));
        msg.lastTradedPrice = readUint32BigEndian(data, offset + 16);
        msg.netChangeIndicator = data[offset + 20];
        // Skip reserved byte at offset + 21
        msg.netPriceChangeFromClosingPrice = readUint32BigEndian(data, offset + 22);
        msg.lastTradeQuantity = readUint32BigEndian(data, offset + 26);
        msg.lastTradeTime = readUint32BigEndian(data, offset + 30);
        msg.averageTradePrice = readUint32BigEndian(data, offset + 34);
        msg.auctionNumber = readUint16BigEndian(data, offset + 38);
        msg.auctionStatus = readUint16BigEndian(data, offset + 40);
        msg.initiatorType = readUint16BigEndian(data, offset + 42);
        msg.initiatorPrice = readUint32BigEndian(data, offset + 44);
        msg.initiatorQuantity = readUint32BigEndian(data, offset + 48);
        msg.auctionPrice = readUint32BigEndian(data, offset + 52);
        msg.auctionQuantity = readUint32BigEndian(data, offset + 56);
        
        // Parse MBP INFORMATION (10 levels × 16 bytes = 160 bytes)
        int mbpOffset = offset + 60;
        for (int j = 0; j < 10; j++) {
            int mbpStart = mbpOffset + (j * 16);
            if (mbpStart + 16 <= offset + recordSize) {
                msg.mbpData[j].quantity = static_cast<int64_t>(readUint64BigEndian(data, mbpStart));
                msg.mbpData[j].price = readUint32BigEndian(data, mbpStart + 8);
                msg.mbpData[j].numberOfOrders = readUint16BigEndian(data, mbpStart + 12);
                msg.mbpData[j].bbBuySellFlag = readUint16BigEndian(data, mbpStart + 14);
            }
        }
        
        // Parse remaining fields
        msg.bbTotalBuyFlag = readUint16BigEndian(data, offset + 220);
        msg.bbTotalSellFlag = readUint16BigEndian(data, offset + 222);
        msg.totalBuyQuantity = static_cast<int64_t>(readUint64BigEndian(data, offset + 224));
        msg.totalSellQuantity = static_cast<int64_t>(readUint64BigEndian(data, offset + 232));
        // Skip 2 reserved bytes at offset + 240
        msg.closingPrice = readUint32BigEndian(data, offset + 242);
        msg.openPrice = readUint32BigEndian(data, offset + 246);
        msg.highPrice = readUint32BigEndian(data, offset + 250);
        msg.lowPrice = readUint32BigEndian(data, offset + 254);
        msg.indicativeClosePrice = readUint32BigEndian(data, offset + 258);
        
        // Export to CSV
        exportTo7208CSV(msg);
        recordsProcessed++;
        
        offset += recordSize;
    }
    
    if (currentCount <= 3) {
        std::cout << "✅ Message 7208 #" << currentCount << ": Successfully processed " 
                  << recordsProcessed << " records" << std::endl;
    }
}

bool processUDPPacket7208(const uint8_t* data, int dataLen) {
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
    
    // Debug: Print first occurrence of each message code
    if (messageCodeCounts[transactionCode] == 1) {
        std::cout << "📊 Found message code: " << transactionCode << " (hex: 0x" 
                  << std::hex << std::uppercase << transactionCode << std::dec 
                  << ") - first occurrence" << std::endl;
    }
    
    // Only process message 7208
    if (transactionCode != 7208) {
        return false;
    }
    
    if (message7208Count.load() <= 3) {
        std::cout << "🎯 Processing message 7208, finalData length: " << processData.size() << std::endl;
    }
    
    process7208Message(processData.data(), processData.size());
    return true;
}

// =============================================================================
// STATISTICS
// =============================================================================

void printStats7208() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t compressed = compressedCount.load();
    int64_t msg7208 = message7208Count.load();
    int64_t saved = message7208Saved.load();
    
    if (seconds > 0) {
        std::string status = "❌ NOT FOUND";
        if (msg7208 > 0) {
            status = "✅ RECEIVING";
        }
        
        std::cout << "⏱️  " << std::fixed << std::setprecision(0) << seconds 
                  << "s | 📦 " << packets << " pkts (" 
                  << std::setprecision(0) << (packets / seconds) << "/s) | 🗜️  " 
                  << compressed << " compressed | 🎯 7208: " << status 
                  << " | " << msg7208 << " msgs, " << saved << " records" << std::endl;
    }
}

std::string formatNumber7208(int64_t n) {
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

void printFinalStats7208() {
    auto duration = std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(duration).count();
    
    int64_t packets = packetCount.load();
    int64_t bytes = totalBytes.load();
    double totalMB = static_cast<double>(bytes) / (1024.0 * 1024.0);
    int64_t compressed = compressedCount.load();
    int64_t decompressed = decompressedCount.load();
    int64_t errors = decompressionErrors.load();
    int64_t msg7208 = message7208Count.load();
    int64_t saved = message7208Saved.load();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "FINAL STATISTICS - MESSAGE 7208 DECODER (BCAST_ONLY_MBP)" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    std::cout << "📊 LISTENER PERFORMANCE" << std::endl;
    std::cout << "  Runtime:              " << std::fixed << std::setprecision(0) 
              << seconds << " seconds" << std::endl;
    std::cout << "  Total Packets:        " << formatNumber7208(packets) << std::endl;
    std::cout << "  Total Data:           " << std::setprecision(1) << totalMB << " MB" << std::endl;
    
    if (seconds > 0) {
        std::cout << "  Avg Packet Rate:      " << std::setprecision(2) 
                  << (packets / seconds) << " packets/sec" << std::endl;
        std::cout << "  Avg Data Rate:        " << std::setprecision(2) 
                  << (totalMB * 1024 / seconds) << " KB/sec" << std::endl;
    }
    
    std::cout << std::endl << "📦 DECOMPRESSION STATISTICS" << std::endl;
    std::cout << "  Compressed Packets:   " << formatNumber7208(compressed);
    if (packets > 0) {
        std::cout << " (" << std::setprecision(1) 
                  << (static_cast<double>(compressed) * 100.0 / packets) << "%)";
    }
    std::cout << std::endl;
    std::cout << "  Decompressed OK:      " << formatNumber7208(decompressed) << std::endl;
    std::cout << "  Decompression Errors: " << formatNumber7208(errors) << std::endl;
    if (compressed > 0) {
        std::cout << "  Success Rate:         " << std::setprecision(1) 
                  << (static_cast<double>(decompressed) * 100.0 / compressed) << "%" << std::endl;
    }
    
    std::cout << std::endl << "🎯 MESSAGE 7208 STATISTICS (BCAST_ONLY_MBP)" << std::endl;
    std::cout << "  Total Messages:       " << formatNumber7208(msg7208) << std::endl;
    std::cout << "  Records Saved:        " << formatNumber7208(saved) << std::endl;
    if (msg7208 > 0) {
        std::cout << "  Avg Records/Message:  " << std::setprecision(2) 
                  << (static_cast<double>(saved) / msg7208) << std::endl;
    }
    
    std::cout << std::endl << "📁 CSV FILE CREATED" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "  Location: csv_output/" << std::endl;
    std::cout << "  Records: " << formatNumber7208(saved) << std::endl;
    std::cout << "  Format: Market By Price data with 5 best bid/ask levels" << std::endl;
    
    // Show all message codes found
    if (!messageCodeCounts.empty()) {
        std::cout << std::endl << "📋 ALL MESSAGE CODES DETECTED:" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        for (const auto& pair : messageCodeCounts) {
            double percentage = static_cast<double>(pair.second) / packets * 100;
            if (pair.first == 7208) {
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
    if (msg7208 > 0) {
        std::cout << "✅ SUCCESS: Market By Price Only Messages (7208) processing completed" << std::endl;
        std::cout << "📊 Captured " << saved << " MBP records" << std::endl;
    } else {
        std::cout << "⚠️  WARNING: No Market By Price Only Messages (7208) found during session" << std::endl;
        std::cout << "💡 Note: MBP messages contain 10-level order book without order count" << std::endl;
    }
    std::cout << "✅ Check csv_output/ for message_7208_*.csv file" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

// =============================================================================
// MAIN RECEIVER FUNCTION
// =============================================================================

bool runMessage7208Receiver(const std::string& multicastIP, int port) {
    startTime = std::chrono::steady_clock::now();
    shutdownFlag = false;
    
    // Reset counters
    packetCount = 0;
    totalBytes = 0;
    compressedCount = 0;
    decompressedCount = 0;
    decompressionErrors = 0;
    message7208Count = 0;
    message7208Saved = 0;
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
    std::cout << "NSE CM UDP Receiver - Message 7208 (BCAST_ONLY_MBP)" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Listening for message code 7208 (0x1C28 in hex)" << std::endl;
    std::cout << "Purpose: Market By Price data (10 levels without order count)" << std::endl;
    std::cout << "Structure: Up to 2 records per message (262 bytes each)" << std::endl;
    std::cout << "Contains: Best 5 bid/ask levels, OHLC, volumes" << std::endl;
    std::cout << "Multicast: " << multicastIP << ":" << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    // Initialize CSV
    if (!initialize7208CSV()) {
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
                printStats7208();
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
                processUDPPacket7208(reinterpret_cast<uint8_t*>(buffer), n);
            }
        }
    }
    
    // Cleanup
    shutdownFlag = true;
    statsThread.join();
    SOCKET_CLOSE(sockfd);
    csvFile7208.close();

#ifdef _WIN32
    WSACleanup();
#endif
    
    printFinalStats7208();
    return true;
}

void stopMessage7208Receiver() {
    shutdownFlag = true;
}
