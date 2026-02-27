#include "multicast_receiver.h"
#include "config.h"
#include "logger.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

// Global pointer for signal handler
MulticastReceiver* g_receiver = nullptr;

void signal_handler(int signum) {
    Logger::info("Caught signal ", signum, ", shutting down gracefully...");
    if (g_receiver) {
        g_receiver->stop();
    }
}

int main(int argc, char* argv[]) {
    // Load configuration
    Config config;
    
    // Try to load from config file (command line arg or default)
    std::string config_file = (argc > 1) ? argv[1] : "config.ini";
    config.loadFromFile(config_file);
    
    // Override with environment variables
    config.loadFromEnv();
    
    // Initialize logger
    LogLevel log_level = LogLevel::INFO;
    if (config.log_level == "DEBUG") log_level = LogLevel::DEBUG;
    else if (config.log_level == "WARN") log_level = LogLevel::WARN;
    else if (config.log_level == "ERROR") log_level = LogLevel::ERROR;
    
    Logger::init(log_level, config.log_file, config.log_to_console);
    
    // Print configuration
    config.print();
    
    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        Logger::info("Starting NSE UDP Reader...");
        
        // Create receiver (will throw on error)
        MulticastReceiver receiver(config.multicast_ip, config.port);
        g_receiver = &receiver;
        
        // Check if initialization succeeded
        if (!receiver.isValid()) {
            Logger::error("Receiver initialization failed");
            return 1;
        }
        
        Logger::info("Receiver initialized successfully");
        
        // Start statistics reporting thread if enabled
        std::atomic<bool> stats_running(false);
        std::thread stats_thread;
        
        if (config.enable_stats) {
            stats_running = true;
            stats_thread = std::thread([&receiver, &stats_running, &config]() {
                while (stats_running) {
                    std::this_thread::sleep_for(std::chrono::seconds(config.stats_interval_sec));
                    if (stats_running) {
                        std::cout << receiver.getStats() << std::endl;
                    }
                }
            });
            Logger::info("Statistics reporting enabled (interval: ", config.stats_interval_sec, "s)");
        }
        
        // Start receiving (blocks until stop() is called)
        Logger::info("Starting packet reception...");
        receiver.start();
        
        // Stop stats thread
        if (config.enable_stats) {
            stats_running = false;
            if (stats_thread.joinable()) {
                stats_thread.join();
            }
        }
        
        // Print final statistics
        Logger::info("Printing final statistics...");
        std::cout << "\n=== Final Statistics ===" << std::endl;
        std::cout << receiver.getStats() << std::endl;
        
        g_receiver = nullptr;
        
    } catch (const std::exception& e) {
        Logger::error("Fatal error: ", e.what());
        Logger::close();
        return 1;
    }
    
    Logger::info("Shutdown complete");
    Logger::close();
    return 0;
}
