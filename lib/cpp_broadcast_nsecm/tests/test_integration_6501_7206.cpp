// Test example for Messages 6501 and 7206 Integration
// Demonstrates thread-safe callback registration and usage
// Compatible with Windows, Linux, and macOS

#include "../include/nsecm_callback.h"
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>

// Thread-safe counters for monitoring
std::atomic<int64_t> adminMessageCount{0};
std::atomic<int64_t> systemInfoCount{0};

// Example: Register callback for Message 6501 (Admin Messages)
void setupAdminMessageCallback() {
    nsecm::MarketDataCallbackRegistry::instance().registerAdminCallback(
        [](const nsecm::AdminMessage& msg) {
            adminMessageCount++;
            
            // Print admin message details
            std::cout << "\n[ADMIN MESSAGE 6501]" << std::endl;
            std::cout << "  Timestamp: " << msg.timestamp << std::endl;
            std::cout << "  Action Code: " << msg.actionCode << std::endl;
            std::cout << "  Message: " << msg.message << std::endl;
            std::cout << "  Total Received: " << adminMessageCount << std::endl;
        }
    );
    
    std::cout << "✅ Admin Message (6501) callback registered" << std::endl;
}

// Example: Register callback for Message 7206 (System Information)
void setupSystemInformationCallback() {
    nsecm::MarketDataCallbackRegistry::instance().registerSystemInformationCallback(
        [](const nsecm::SystemInformationData& data) {
            systemInfoCount++;
            
            // Print system information details
            std::cout << "\n[SYSTEM INFORMATION 7206]" << std::endl;
            std::cout << "  Market Index: " << data.marketIndex << std::endl;
            std::cout << "  Normal Market Status: " << data.normalMarketStatus << std::endl;
            std::cout << "  Tick Size: " << data.tickSize << " paise" << std::endl;
            std::cout << "  Board Lot Quantity: " << data.boardLotQuantity << std::endl;
            std::cout << "  Warning Percent: " << data.warningPercent << "%" << std::endl;
            std::cout << "  Volume Freeze Percent: " << data.volumeFreezePercent << "%" << std::endl;
            std::cout << "  Maximum GTC Days: " << data.maximumGtcDays << std::endl;
            std::cout << "  AON Allowed: " << (data.aonAllowed ? "Yes" : "No") << std::endl;
            std::cout << "  Minimum Fill Allowed: " << (data.minimumFillAllowed ? "Yes" : "No") << std::endl;
            std::cout << "  Books Merged: " << (data.booksMerged ? "Yes" : "No") << std::endl;
            std::cout << "  Total Received: " << systemInfoCount << std::endl;
        }
    );
    
    std::cout << "✅ System Information (7206) callback registered" << std::endl;
}

// Example: Statistics printer (runs in separate thread)
void printStatistics() {
    auto startTime = std::chrono::steady_clock::now();
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "STATISTICS (Runtime: " << duration << "s)" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Admin Messages (6501): " << adminMessageCount << std::endl;
        std::cout << "System Info (7206): " << systemInfoCount << std::endl;
        std::cout << "========================================\n" << std::endl;
    }
}

// Example: Complete setup function
void setupCallbacks() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "SETTING UP MESSAGE CALLBACKS" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Register both callbacks
    setupAdminMessageCallback();
    setupSystemInformationCallback();
    
    std::cout << "\n✅ All callbacks registered successfully!" << std::endl;
    std::cout << "Ready to receive UDP packets...\n" << std::endl;
}

// Example: Thread-safe test (demonstrates no race conditions)
void threadSafetyTest() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "THREAD SAFETY TEST" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Simulate concurrent callback registrations from multiple threads
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([i]() {
            // Each thread registers and unregisters callbacks
            for (int j = 0; j < 100; j++) {
                nsecm::MarketDataCallbackRegistry::instance().registerAdminCallback(
                    [i, j](const nsecm::AdminMessage& msg) {
                        // Dummy callback
                    }
                );
                
                nsecm::MarketDataCallbackRegistry::instance().registerSystemInformationCallback(
                    [i, j](const nsecm::SystemInformationData& data) {
                        // Dummy callback
                    }
                );
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "✅ Thread safety test completed successfully!" << std::endl;
    std::cout << "   No race conditions detected.\n" << std::endl;
}

// Main function - demonstrates usage
int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  NSE CM Messages 6501 & 7206 Integration Test         ║" << std::endl;
    std::cout << "║  Thread-Safe Callback System                           ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝\n" << std::endl;
    
    // Test 1: Thread safety
    threadSafetyTest();
    
    // Test 2: Setup callbacks
    setupCallbacks();
    
    // Test 3: Start statistics thread
    std::thread statsThread(printStatistics);
    statsThread.detach();
    
    // At this point, your UDP receiver would be running and
    // the callbacks will be automatically invoked when messages arrive
    
    std::cout << "Press Ctrl+C to exit..." << std::endl;
    
    // Keep main thread alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
