#include "../../lib/cpp_broacast_nsefo/include/nse_structures.h"
#include "../../lib/cpp_broacast_nsefo/include/config.h"
#include "../../lib/cpp_broacast_nsefo/include/multicast_receiver.h"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <map>
#include <iomanip>

using nsefo::Config;
using nsefo::MulticastReceiver;
using nsefo::UDPStats;

// Global flag for graceful shutdown
std::atomic<bool> keep_running{true};

// Message code names for display
std::map<uint16_t, std::string> message_names = {
    {6511, "6511 - Market Open"},
    {6521, "6521 - Market Close"},
    {6522, "6522 - Post Close"},
    {6531, "6531 - Pre/Post Day"},
    {6541, "6541 - Circuit Check"},
    {6571, "6571 - PreOpen Ended"},
    {7130, "7130 - Market Movement CM OI"},
    {7200, "7200 - MBO/MBP Update"},
    {7201, "7201 - Market Watch"},
    {7202, "7202 - Ticker & Trade"},
    {7203, "7203 - Industry Index"},
    {7206, "7206 - System Info"},
    {7207, "7207 - Indices"},
    {7208, "7208 - Touchline/MBP"},
    {7210, "7210 - Security Status PreOpen"},
    {7211, "7211 - Spread MBP Delta"},
    {7220, "7220 - Price Protection"},
    {7304, "7304 - LocalDB Data"},
    {7305, "7305 - Security Master Change"},
    {7306, "7306 - Participant Master Change"},
    {7307, "7307 - LocalDB Header"},
    {7308, "7308 - LocalDB Trailer"},
    {7309, "7309 - Spread Master Change"},
    {7320, "7320 - Security Status Change"},
    {7321, "7321 - Partial System Info"},
    {7324, "7324 - Instrument Master Change"},
    {7325, "7325 - Index Master Change"},
    {7326, "7326 - Index Map Table"},
    {7340, "7340 - Security Master Periodic"},
    {7341, "7341 - Spread Master Periodic"},
    {17130, "17130 - Enhanced Market Movement"},
    {17201, "17201 - Enhanced Market Watch"},
    {17202, "17202 - Enhanced Ticker & Trade"}
};

void signal_handler(int signal) {
    (void)signal;
    keep_running = false;
}

int main(int argc, char** argv) {
    std::cout << "\n=== NSE Broadcast Message Test ===\n";
    std::cout << "Monitoring messages: 7201, 7202, 17201, 17202\n";
    std::cout << "This test will display the raw message data as received.\n";
    std::cout << "Press Ctrl+C to stop...\n\n";
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Load configuration
    Config config;
    if (!config.loadFromFile("../../configs/config.ini")) {
        std::cerr << "Failed to load config. Using defaults.\n";
    }
    
    // Get multicast settings from config
    std::string multicast_ip = config.multicast_ip;
    int multicast_port = config.port;
    
    std::cout << "Connecting to: " << multicast_ip << ":" << multicast_port << "\n\n";
    
    // Create multicast receiver
    MulticastReceiver receiver(multicast_ip, multicast_port);
    
    if (!receiver.isValid()) {
        std::cerr << "Failed to initialize multicast receiver!\n";
        return 1;
    }
    
    // Start receiver in background thread
    std::thread receiver_thread([&receiver]() {
        receiver.start();
    });
    
    // Statistics reporting
    auto start_time = std::chrono::steady_clock::now();
    uint64_t last_packet_count = 0;
    
    while (keep_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        
        const UDPStats& stats = receiver.getStats();
        uint64_t packets_since_last = stats.totalPackets - last_packet_count;
        last_packet_count = stats.totalPackets;
        
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "STATISTICS (Elapsed: " << elapsed << "s)\n";
        std::cout << std::string(80, '=') << "\n";
        std::cout << "  Total Packets:                 " << stats.totalPackets << "\n";
        std::cout << "  Recent Packets (last 5s):      " << packets_since_last << "\n";
        std::cout << "  Packets/sec (avg):             " << (elapsed > 0 ? stats.totalPackets / elapsed : 0) << "\n";
        std::cout << "  Total Bytes:                   " << stats.totalBytes << "\n";
        std::cout << std::string(80, '-') << "\n";
        
        // Display message code statistics
        std::cout << "MESSAGE CODE BREAKDOWN:\n";
        std::cout << std::string(80, '-') << "\n";
        
        uint64_t total_messages = 0;
        for (const auto& stat : stats.messageStats) {
            total_messages += stat.second.count;
        }
        
        for (const auto& stat : stats.messageStats) {
            uint16_t code = stat.first;
            uint64_t count = stat.second.count;
            double percentage = total_messages > 0 ? (count * 100.0 / total_messages) : 0;
            
            std::string name = "Unknown";
            auto it = message_names.find(code);
            if (it != message_names.end()) {
                name = it->second;
            } else {
                name = std::to_string(code) + " - Unknown";
            }
            
            std::cout << "  " << std::left << std::setw(35) << name 
                      << std::right << std::setw(12) << count 
                      << "  (" << std::fixed << std::setprecision(1) << std::setw(5) << percentage << "%)\n";
        }
        
        std::cout << std::string(80, '-') << "\n";
        std::cout << "  Total Messages:                " << total_messages << "\n";
        std::cout << std::string(80, '=') << "\n";
    }
    
    // Cleanup
    std::cout << "\nStopping receiver...\n";
    receiver.stop();
    
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }
    
    // Final statistics
    const UDPStats& final_stats = receiver.getStats();
    
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "FINAL STATISTICS\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  Total Packets:                 " << final_stats.totalPackets << "\n";
    std::cout << "  Total Bytes:                   " << final_stats.totalBytes << "\n";
    std::cout << std::string(80, '-') << "\n";
    
    // Display final message code statistics
    std::cout << "MESSAGE CODE BREAKDOWN:\n";
    std::cout << std::string(80, '-') << "\n";
    
    uint64_t total_messages = 0;
    for (const auto& stat : final_stats.messageStats) {
        total_messages += stat.second.count;
    }
    
    for (const auto& stat : final_stats.messageStats) {
        uint16_t code = stat.first;
        uint64_t count = stat.second.count;
        double percentage = total_messages > 0 ? (count * 100.0 / total_messages) : 0;
        
        std::string name = "Unknown";
        auto it = message_names.find(code);
        if (it != message_names.end()) {
            name = it->second;
        } else {
            name = std::to_string(code) + " - Unknown";
        }
        
        std::cout << "  " << std::left << std::setw(35) << name 
                  << std::right << std::setw(12) << count 
                  << "  (" << std::fixed << std::setprecision(1) << std::setw(5) << percentage << "%)\n";
    }
    
    std::cout << std::string(80, '-') << "\n";
    std::cout << "  Total Messages:                " << total_messages << "\n";
    std::cout << std::string(80, '=') << "\n";
    
    return 0;
}
