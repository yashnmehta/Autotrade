#include "multicast_receiver.h"
#include "market_data_callback.h"
#include "constants.h"
#include <iostream>
#include <map>
#include <atomic>
#include <chrono>
#include <thread>
#include <signal.h>
#include <iomanip>

// Message counters (not used anymore - using UDPStats instead)
// std::map<std::string, std::atomic<uint64_t>> messageCounters;
std::atomic<bool> running(true);

void signalHandler(int signum) {
    std::cout << "\n[SIGNAL] Interrupt received, stopping..." << std::endl;
    running = false;
}

// Removed printStatistics() - using built-in UDPStats instead

int main() {
    std::cout << "[TEST] NSE Broadcast Message Statistics Test" << std::endl;
    std::cout << "[TEST] Will collect data for 5 minutes..." << std::endl;
    std::cout << "[TEST] Press Ctrl+C to stop early" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Setup signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Register minimal callbacks (statistics tracked at UDP receiver level)
    MarketDataCallbackRegistry::instance().registerTouchlineCallback(
        [](const TouchlineData& data) {}
    );
    
    MarketDataCallbackRegistry::instance().registerMarketDepthCallback(
        [](const MarketDepthData& data) {}
    );
    
    MarketDataCallbackRegistry::instance().registerMarketWatchCallback(
        [](const MarketWatchData& data) {}
    );
    
    MarketDataCallbackRegistry::instance().registerTickerCallback(
        [](const TickerData& data) {}
    );
    
    // Start UDP receiver
    std::string multicastIP = "233.1.2.5";  // NSE F&O
    int port = 34331;
    
    std::cout << "[UDP] Starting receiver on " << multicastIP << ":" << port << std::endl;
    
    MulticastReceiver receiver(multicastIP, port);
    
    if (!receiver.isValid()) {
        std::cerr << "[ERROR] Failed to initialize UDP receiver!" << std::endl;
        return 1;
    }
    
    receiver.start();
    std::cout << "[UDP] Receiver started successfully" << std::endl;
    std::cout << "[UDP] Collecting statistics for 5 minutes..." << std::endl;
    
    // Run for 5 minutes
    auto startTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::minutes(5);
    
    int secondsElapsed = 0;
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        secondsElapsed += 10;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        
        if (elapsed >= 300) {  // 5 minutes
            // std::cout << "\n[TEST] 5 minutes completed!" << std::endl;
            break;
        }
        
        // std::cout << "[PROGRESS] " << elapsed << " seconds elapsed..." << std::endl;
    }
    
    // Stop receiver
    std::cout << "\n[UDP] Stopping receiver..." << std::endl;
    receiver.stop();
    
    // Print final statistics
    const UDPStats& stats = receiver.getStats();
    std::cout << "\n========================================" << std::endl;
    std::cout << "UDP RECEIVER STATISTICS" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Total Packets: " << stats.totalPackets << std::endl;
    std::cout << "Total Bytes: " << stats.totalBytes << std::endl;
    std::cout << "Compressed Packets: " << stats.compressedPackets << std::endl;
    std::cout << "Decompressed Packets: " << stats.decompressedPackets << std::endl;
    std::cout << "Decompression Failures: " << stats.decompressionFailures << std::endl;
    
    // Print detailed message type statistics
    std::cout << "\n========================================" << std::endl;
    std::cout << "MESSAGE TYPE BREAKDOWN (All Messages)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::left << std::setw(8) << "TxCode" 
              << std::setw(35) << "Message Name" 
              << std::right << std::setw(12) << "Count"
              << std::setw(15) << "Comp(KB)"
              << std::setw(15) << "Raw(KB)" << std::endl;
    std::cout << "--------------------------------------------------------------------------------" << std::endl;
    
    uint64_t totalMessages = 0;
    for (const auto& entry : stats.messageStats) {
        std::string name = getTxCodeName(entry.first);
        std::cout << std::left << std::setw(8) << entry.first
                  << std::setw(35) << name
                  << std::right << std::setw(12) << entry.second.count
                  << std::setw(15) << std::fixed << std::setprecision(2) 
                  << entry.second.totalCompressedSize / 1024.0
                  << std::setw(15) << entry.second.totalRawSize / 1024.0 << std::endl;
        totalMessages += entry.second.count;
    }
    
    std::cout << "--------------------------------------------------------------------------------" << std::endl;
    std::cout << std::left << std::setw(43) << "TOTAL MESSAGES" 
              << std::right << std::setw(12) << totalMessages << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
