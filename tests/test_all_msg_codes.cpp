#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <string>

// Windows socket headers
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// Include protocol for BroadcastHeader and endianness functions
#include "../lib/cpp_broadcast_nsecm/include/protocol.h"

// Simple INI parser
std::map<std::string, std::string> parse_config(const std::string& filename) {
    std::map<std::string, std::string> config;
    std::ifstream file(filename);
    std::string line;
    std::string current_section;

    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return config;
    }

    while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start, end - start + 1);

        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line[0] == '[' && line[line.length()-1] == ']') {
            current_section = line.substr(1, line.length()-2);
            continue;
        }

        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            std::string full_key = current_section + "." + key;
            config[full_key] = value;
        }
    }

    file.close();
    return config;
}

// Setup UDP multicast socket
SOCKET setup_udp_socket(const std::string& multicast_ip, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return INVALID_SOCKET;
    }

    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return INVALID_SOCKET;
    }

    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
        std::cerr << "Failed to set SO_REUSEADDR" << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return INVALID_SOCKET;
    }

    sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return INVALID_SOCKET;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_ip.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
        std::cerr << "Failed to join multicast group: " << WSAGetLastError() << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return INVALID_SOCKET;
    }

    DWORD timeout = 5000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    std::cout << "Successfully joined multicast group " << multicast_ip << ":" << port << std::endl;
    return sockfd;
}

void print_statistics(int total, const std::map<uint16_t, int>& stats) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Total Messages Received: " << total << std::endl;
    std::cout << "Unique Transaction Codes: " << stats.size() << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::vector<std::pair<uint16_t, int>> sorted_stats(stats.begin(), stats.end());
    std::sort(sorted_stats.begin(), sorted_stats.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::cout << "\nAll Transaction Codes (sorted by frequency):" << std::endl;
    std::cout << std::left << std::setw(12) << "TransCode" 
              << std::setw(10) << "Hex" 
              << std::setw(10) << "Count" 
              << std::setw(10) << "%" << std::endl;
    std::cout << std::string(42, '-') << std::endl;
    
    for (const auto& [code, count] : sorted_stats) {
        double percent = (count * 100.0) / total;
        std::cout << std::left << std::setw(12) << code
                  << "0x" << std::hex << std::setw(8) << std::setfill('0') << code << std::dec
                  << std::setw(10) << count
                  << std::fixed << std::setprecision(2) << percent << "%" << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    std::string config_file = "configs/config.ini";
    std::string output_file = "nse_all_msg_codes.log";

    std::cout << "=== NSE CM Broadcast Message Code Scanner ===" << std::endl;
    std::cout << "This tool shows ALL transaction codes being broadcast\n" << std::endl;

    // Parse config
    auto config = parse_config(config_file);
    std::string multicast_ip = config["UDP.nse_cm_multicast_ip"];
    std::string port_str = config["UDP.nse_cm_port"];

    if (multicast_ip.empty() || port_str.empty()) {
        std::cerr << "Failed to read NSE CM UDP configuration" << std::endl;
        return 1;
    }

    int port = std::stoi(port_str);
    std::cout << "Configuration: " << multicast_ip << ":" << port << std::endl;

    SOCKET sockfd = setup_udp_socket(multicast_ip, port);
    if (sockfd == INVALID_SOCKET) {
        return 1;
    }

    std::cout << "\nListening for broadcast messages..." << std::endl;
    std::cout << "Press Ctrl+C to stop and see final statistics\n" << std::endl;

    char buffer[65536];
    int msg_count = 0;
    std::map<uint16_t, int> trans_code_stats;
    std::map<uint16_t, int> first_seen;

    // Open output file
    std::ofstream logfile(output_file, std::ios::trunc);
    logfile << "Message#,TransCode,Hex,MsgLength,PacketSize" << std::endl;

    while (true) {
        int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        
        if (bytes_received < 0) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                std::cout << "Timeout. Retrying..." << std::endl;
                continue;
            }
            std::cerr << "Receive error: " << error << std::endl;
            break;
        }

        if (bytes_received < (int)sizeof(BroadcastHeader)) {
            continue;
        }

        msg_count++;

        BroadcastHeader* bcHeader = (BroadcastHeader*)buffer;
        uint16_t trans_code = be16toh_func(bcHeader->transactionCode);
        uint16_t msg_length = be16toh_func(bcHeader->messageLength);

        // Track statistics
        trans_code_stats[trans_code]++;
        
        // Record first occurrence
        if (first_seen.find(trans_code) == first_seen.end()) {
            first_seen[trans_code] = msg_count;
            std::cout << "*** NEW TransCode " << trans_code 
                      << " (0x" << std::hex << std::setw(4) << std::setfill('0') << trans_code << std::dec 
                      << ") first seen at message #" << msg_count << " ***" << std::endl;
        }

        // Log to file (first 10000 messages or all if checking for 7207)
        if (msg_count <= 10000 || trans_code == 7207) {
            logfile << msg_count << "," << trans_code << ",0x" << std::hex << trans_code << std::dec 
                    << "," << msg_length << "," << bytes_received << std::endl;
        }

        // Print detailed info for first 20 messages
        if (msg_count <= 20) {
            std::cout << "Msg #" << msg_count << ": TransCode=" << trans_code 
                      << " (0x" << std::hex << std::setw(4) << std::setfill('0') << trans_code << std::dec 
                      << "), MsgLen=" << msg_length 
                      << ", Size=" << bytes_received << " bytes" << std::endl;
        }

        // Highlight 7207 if found
        if (trans_code == 7207) {
            std::cout << "\n!!! FOUND 7207 (INDICES) at message #" << msg_count << " !!!" << std::endl;
            std::cout << "    Message Length: " << msg_length << " bytes" << std::endl;
            std::cout << "    Packet Size: " << bytes_received << " bytes\n" << std::endl;
        }

        // Periodic statistics
        if (msg_count % 2000 == 0) {
            print_statistics(msg_count, trans_code_stats);
        }
    }

    logfile.close();
    
    // Final statistics
    std::cout << "\n\n=== FINAL STATISTICS ===" << std::endl;
    print_statistics(msg_count, trans_code_stats);
    
    std::cout << "Log saved to: " << output_file << std::endl;

    closesocket(sockfd);
    WSACleanup();
    
    return 0;
}
