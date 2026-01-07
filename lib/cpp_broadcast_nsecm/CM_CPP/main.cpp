// NSE Capital Market Multicast UDP Receiver - Main Entry Point
//
// This main file allows you to specify:
// - Multicast IP address
// - Port number  
// - Message code to process
//
// USAGE: ./main <multicast_ip> <port> <message_code>
// Example: ./main 233.1.2.5 8222 6501
//
// Created: December 26, 2025
// Purpose: Unified entry point for all message decoders

#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <atomic>

// Include message decoders
#include "msg_codes/message_6501_live.h"
#include "msg_codes/message_6511_live.h"
#include "msg_codes/message_6521_live.h"
#include "msg_codes/message_6531_live.h"
#include "msg_codes/message_6541_live.h"
#include "msg_codes/message_6571_live.h"
#include "msg_codes/message_6581_live.h"
#include "msg_codes/message_6583_live.h"
#include "msg_codes/message_6584_live.h"
#include "msg_codes/message_7200_live.h"
#include "msg_codes/message_7201_live.h"
#include "msg_codes/message_7207_live.h"
#include "msg_codes/message_7208_live.h"
#include "msg_codes/message_7216_live.h"
#include "msg_codes/message_7306_live.h"

// Global flag for shutdown
std::atomic<bool> g_shutdownRequested{false};

void signalHandler(int sig) {
    std::cout << "\n\n⏹️  Shutdown signal received (Ctrl+C)..." << std::endl;
    g_shutdownRequested = true;
    
    // Call appropriate stop function based on active message code
    stopMessage6501Receiver();
    stopMessage6511Receiver();
    stopMessage6521Receiver();
    stopMessage6531Receiver();
    stopMessage6541Receiver();
    stopMessage6571Receiver();
    stopMessage6581Receiver();
    stopMessage6583Receiver();
    stopMessage6584Receiver();
    stopMessage7200Receiver();
    stopMessage7201Receiver();
    stopMessage7207Receiver();
    stopMessage7208Receiver();
    stopMessage7216Receiver();
    stopMessage7306Receiver();
    
    // Suppress unused parameter warning
    (void)sig;
}

void printUsage(const char* programName) {
    std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║ NSE Capital Market Multicast UDP Receiver - C++           ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    std::cout << "USAGE: " << programName << " <multicast_ip> <port> <message_code>" << std::endl;
    std::cout << std::endl;
    std::cout << "PARAMETERS:" << std::endl;
    std::cout << "  multicast_ip  : Multicast IP address to listen on" << std::endl;
    std::cout << "  port          : Port number to listen on" << std::endl;
    std::cout << "  message_code  : NSE message code to process" << std::endl;
    std::cout << std::endl;
    std::cout << "SUPPORTED MESSAGE CODES:" << std::endl;
    std::cout << "  6501 : BCAST_JRNL_VCT_MSG (Journal/VCT Messages)" << std::endl;
    std::cout << "  6511 : BC_OPEN_MESSAGE (Market Open Notifications)" << std::endl;
    std::cout << "  6521 : BC_CLOSE_MESSAGE (Market Close Notifications)" << std::endl;
    std::cout << "  6531 : BC_PREOPEN_SHUTDOWN_MSG (Pre-market/Shutdown)" << std::endl;
    std::cout << "  6541 : BC_CIRCUIT_CHECK (Heartbeat Pulse)" << std::endl;
    std::cout << "  6571 : BC_NORMAL_MKT_PREOPEN_ENDED (Preopen End)" << std::endl;
    std::cout << "  6581 : BC_AUCTION_INQUIRY (Auction Status Change)" << std::endl;
    std::cout << "  6583 : BC_CLOSING_START (Closing Session Start)" << std::endl;
    std::cout << "  6584 : BC_CLOSING_END (Closing Session End)" << std::endl;
    std::cout << "  7200 : BCAST_MBO_MBP_UPDATE (Market By Order/Price)" << std::endl;
    std::cout << "  7201 : BCAST_MW_ROUND_ROBIN (Market Watch Round Robin)" << std::endl;
    std::cout << "  7207 : BCAST_INDICES (Broadcast Indices)" << std::endl;
    std::cout << "  7208 : BCAST_ONLY_MBP (Market By Price Only)" << std::endl;
    std::cout << "  7216 : BCAST_INDICES_VIX (India VIX Index)" << std::endl;
    std::cout << "  7306 : BCAST_PART_MSTR_CHG (Participant Master Change)" << std::endl;
    std::cout << std::endl;
    std::cout << "EXAMPLES:" << std::endl;
    std::cout << "  Live Market Hours:" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 6501" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 6511" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 6521" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 6531" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 6541" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 6571" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 6581" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 6583" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 6584" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 7200" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 7201" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 7207" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 7208" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 7216" << std::endl;
    std::cout << "    " << programName << " 233.1.2.5 8222 7306" << std::endl;
    std::cout << std::endl;
    std::cout << "  After Market Hours:" << std::endl;
    std::cout << "    " << programName << " 231.31.31.4 18901 6501" << std::endl;
    std::cout << "    " << programName << " 231.31.31.4 18901 6511" << std::endl;
    std::cout << "    " << programName << " 231.31.31.4 18901 6521" << std::endl;
    std::cout << "    " << programName << " 231.31.31.4 18901 6531" << std::endl;
    std::cout << "    " << programName << " 231.31.31.4 18901 6541" << std::endl;
    std::cout << "    " << programName << " 231.31.31.4 18901 6571" << std::endl;
    std::cout << "    " << programName << " 231.31.31.4 18901 6581" << std::endl;
    std::cout << "    " << programName << " 231.31.31.4 18901 6583" << std::endl;
    std::cout << "    " << programName << " 231.31.31.4 18901 6584" << std::endl;
    std::cout << "    " << programName << " 231.31.31.4 18901 7200" << std::endl;
    std::cout << "    " << programName << " 231.31.31.4 18901 7201" << std::endl;
    std::cout << std::endl;
    std::cout << "CONTROLS:" << std::endl;
    std::cout << "  Press Ctrl+C to stop the receiver" << std::endl;
    std::cout << std::endl;
}

bool runReceiver(const std::string& multicastIP, int port, int messageCode) {
    std::cout << "🚀 Starting receiver for message code " << messageCode << "..." << std::endl;
    
    switch (messageCode) {
        case 6501:
            return runMessage6501Receiver(multicastIP, port);
            
        case 6511:
            return runMessage6511Receiver(multicastIP, port);
            
        case 6521:
            return runMessage6521Receiver(multicastIP, port);
            
        case 6531:
            return runMessage6531Receiver(multicastIP, port);
            
        case 6541:
            return runMessage6541Receiver(multicastIP, port);
            
        case 6571:
            return runMessage6571Receiver(multicastIP, port);
            
        case 6581:
            return runMessage6581Receiver(multicastIP, port);
            
        case 6583:
            return runMessage6583Receiver(multicastIP, port);
            
        case 6584:
            return runMessage6584Receiver(multicastIP, port);
            
        case 7200:
            return runMessage7200Receiver(multicastIP, port);
            
        case 7201:
            return runMessage7201Receiver(multicastIP, port);
            
        case 7207:
            return runMessage7207Receiver(multicastIP, port);
            
        case 7208:
            return runMessage7208Receiver(multicastIP, port);
            
        case 7216:
            return runMessage7216Receiver(multicastIP, port);
            
        case 7306:
            return runMessage7306Receiver(multicastIP, port);
            
        default:
            std::cerr << "❌ Unsupported message code: " << messageCode << std::endl;
            std::cerr << "   Supported codes: 6501, 6511, 6521, 6531, 6541, 6571, 6581, 6583, 6584, 7200, 7201, 7207, 7208, 7216, 7306" << std::endl;
            return false;
    }
}

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc != 4) {
        printUsage(argv[0]);
        return 1;
    }
    
    // Parse arguments
    std::string multicastIP = argv[1];
    int port;
    int messageCode;
    
    try {
        port = std::stoi(argv[2]);
        messageCode = std::stoi(argv[3]);
    } catch (const std::exception& e) {
        std::cerr << "❌ Error parsing arguments: " << e.what() << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Validate arguments
    if (port < 1 || port > 65535) {
        std::cerr << "❌ Invalid port number. Must be between 1 and 65535." << std::endl;
        return 1;
    }
    
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "📋 Configuration:" << std::endl;
    std::cout << "   Multicast IP  : " << multicastIP << std::endl;
    std::cout << "   Port          : " << port << std::endl;
    std::cout << "   Message Code  : " << messageCode << std::endl;
    std::cout << std::endl;
    std::cout << "⏹️  Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;
    
    // Run the appropriate receiver
    bool success = runReceiver(multicastIP, port, messageCode);
    
    if (!success) {
        std::cerr << "❌ Receiver failed to start or encountered an error." << std::endl;
        return 1;
    }
    
    std::cout << "✅ Receiver completed successfully." << std::endl;
    return 0;
}
