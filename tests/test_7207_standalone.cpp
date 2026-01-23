#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <iomanip>
#include <string>
#include <map>
#include <algorithm>

// Windows socket headers
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// Include necessary headers from the project
// Using relative paths as this is a standalone test without CMake integration
#include "../lib/cpp_broadcast_nsecm/include/protocol.h" 
#include "../lib/cpp_broadcast_nsecm/include/nse_common.h"
#include "../lib/cpp_broadcast_nsecm/include/nse_index_messages.h"
#include "../lib/cpp_broadcast_nsecm/CM_CPP/lzo_decompressor_safe.h"

// Broadcast message structure (Broadcast Header + Message Header + Data)
struct BroadcastMessage {
    BroadcastHeader bcHeader;
    MESSAGE_HEADER msgHeader;
    // Data follows
};

// Define a struct to hold decoded data (mocking what IndicesUpdate would do)
struct DecodedIndex {
    std::string name;
    double value;
    double high;
    double low;
    double open;
    double close;
    double percentChange;
    double yearlyHigh;
    double yearlyLow;
    int32_t upMoves;
    int32_t downMoves;
    double marketCap;
    char netChangeIndicator;
};

// Simple INI parser for config file
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
        // Remove leading/trailing whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start, end - start + 1);

        // Skip comments and empty lines
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        // Check for section
        if (line[0] == '[' && line[line.length()-1] == ']') {
            current_section = line.substr(1, line.length()-2);
            continue;
        }

        // Parse key-value pairs
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            
            // Trim whitespace
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

void log_indices(const std::vector<DecodedIndex>& indices, const std::string& filename) {
    std::ofstream outfile(filename, std::ios::app);
    if (!outfile.is_open()) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
        return;
    }

    outfile << "--- Decoded " << indices.size() << " Indices (TransCode 7207) ---\n";
    for (const auto& idx : indices) {
        outfile << "Name: " << idx.name << "\n";
        outfile << "  Value: " << std::fixed << std::setprecision(2) << idx.value << "\n";
        outfile << "  High: " << idx.high << " Low: " << idx.low << "\n";
        outfile << "  Open: " << idx.open << " Close: " << idx.close << "\n";
        outfile << "  % Change: " << idx.percentChange << "\n";
        outfile << "  Yearly High: " << idx.yearlyHigh << " Low: " << idx.yearlyLow << "\n";
        outfile << "  Up Moves: " << idx.upMoves << " Down Moves: " << idx.downMoves << "\n";
        outfile << "  Market Cap: " << idx.marketCap << "\n";
        outfile << "  Net Change Ind: " << idx.netChangeIndicator << "\n";
        outfile << "----------------------------------------\n";
    }
    outfile.close();
    std::cout << "Logged " << indices.size() << " indices to " << filename << std::endl;
}

// Logic extracted from parse_message_7207.cpp
void process_7207_message(const uint8_t* message_data, int message_size, const std::string& log_file) {
    // The message_data points to BCAST_HEADER (40 bytes) + data
    // Structure: BCAST_HEADER (40) + numberOfRecords (2) + MS_INDICES array
    
    if (message_size < 42) {
        std::cerr << "Message too small for 7207: " << message_size << " bytes" << std::endl;
        return;
    }
    
    // Skip the BCAST_HEADER (40 bytes) to get to numberOfRecords
    const uint8_t* data_ptr = message_data + 40;
    uint16_t numRecords = be16toh_func(*((uint16_t*)data_ptr));
    
    std::cout << "  Number of records: " << numRecords << std::endl;
    
    std::vector<DecodedIndex> decoded_indices;
    int limit = (numRecords < 6) ? numRecords : 6;
    
    if (numRecords == 0) {
        std::cout << "  Warning: numberOfRecords is 0!" << std::endl;
        // Dump first 64 bytes for debugging
        std::cout << "  First 64 bytes (hex): ";
        for (int i = 0; i < 64 && i < message_size; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)message_data[i] << " ";
            if ((i + 1) % 16 == 0) std::cout << "\n                        ";
        }
        std::cout << std::dec << std::endl;
    }

    // Point to start of indices array (after BCAST_HEADER + numberOfRecords)
    const MS_INDICES* indices_ptr = (const MS_INDICES*)(data_ptr + 2);

    for (int i = 0; i < limit; i++) {
        const auto& rec = indices_ptr[i];
        DecodedIndex data;

        // Copy name
        char nameBuf[22];
        std::memcpy(nameBuf, rec.indexName, 21);
        nameBuf[21] = '\0';
        data.name = std::string(nameBuf);

        // Parse values (Big Endian to Host)
        // Note: Division by 100.0 as per reference implementation
        data.value = (int32_t)be32toh_func(rec.indexValue) / 100.0;
        data.high = (int32_t)be32toh_func(rec.highIndexValue) / 100.0;
        data.low = (int32_t)be32toh_func(rec.lowIndexValue) / 100.0;
        data.open = (int32_t)be32toh_func(rec.openingIndex) / 100.0;
        data.close = (int32_t)be32toh_func(rec.closingIndex) / 100.0;
        data.percentChange = (int32_t)be32toh_func(rec.percentChange) / 100.0;
        data.yearlyHigh = (int32_t)be32toh_func(rec.yearlyHigh) / 100.0;
        data.yearlyLow = (int32_t)be32toh_func(rec.yearlyLow) / 100.0;
        data.upMoves = be32toh_func(rec.noOfUpmoves);
        data.downMoves = be32toh_func(rec.noOfDownmoves);

        // Market Capitalisation is a double. Reference implementation does byte swap on the raw double bytes.
        uint64_t rawMktCap;
        std::memcpy(&rawMktCap, &rec.marketCapitalisation, sizeof(double));
        rawMktCap = be64toh_func(rawMktCap);
        std::memcpy(&data.marketCap, &rawMktCap, sizeof(double));

        data.netChangeIndicator = rec.netChangeIndicator;

        decoded_indices.push_back(data);
    }

    log_indices(decoded_indices, log_file);
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

    // Allow multiple sockets to bind to the same port
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
        std::cerr << "Failed to set SO_REUSEADDR" << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return INVALID_SOCKET;
    }

    // Bind to the port
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

    // Join the multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_ip.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
        std::cerr << "Failed to join multicast group: " << WSAGetLastError() << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return INVALID_SOCKET;
    }

    // Set receive timeout (5 seconds)
    DWORD timeout = 5000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    std::cout << "Successfully joined multicast group " << multicast_ip << ":" << port << std::endl;
    return sockfd;
}

int main() {
    std::string log_file = "nse_7207_decoded.txt"; // Output file for decoded data
    std::string config_file = "configs/config.ini";

    // Set up exception handling
    try {
        // Parse config file
        std::cout << "Reading configuration from " << config_file << "..." << std::endl;
        auto config = parse_config(config_file);

        std::string multicast_ip = config["UDP.nse_cm_multicast_ip"];
        std::string port_str = config["UDP.nse_cm_port"];

        if (multicast_ip.empty() || port_str.empty()) {
            std::cerr << "Failed to read NSE CM UDP configuration from config file" << std::endl;
            std::cerr << "Expected: [UDP] section with nse_cm_multicast_ip and nse_cm_port" << std::endl;
            return 1;
        }

        int port = std::stoi(port_str);
        std::cout << "NSE CM Configuration: " << multicast_ip << ":" << port << std::endl;

        // Setup UDP socket
        SOCKET sockfd = setup_udp_socket(multicast_ip, port);
        if (sockfd == INVALID_SOCKET) {
            std::cerr << "Failed to setup UDP socket" << std::endl;
            return 1;
        }

        std::cout << "\n=== Listening for NSE CM Broadcast (TransCode 7207) ===" << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;
        std::cout << "Decoded data will be written to: " << log_file << std::endl << std::endl;

        // Clear the log file
        std::ofstream clearFile(log_file, std::ios::trunc);
        clearFile << "--- Session Started: Listening for 7207 ---\n";
        clearFile.close();

    char buffer[65536];
    uint8_t decompressed[65536];
    int msg_count = 0;
    int indices_7207_count = 0;
    int compressed_count = 0;
    int decompression_errors = 0;
    std::map<uint16_t, int> trans_code_stats;

    std::cout << "Debug: Logging first 10 transaction codes received..." << std::endl;
    std::cout << "Note: NSE broadcasts are LZO-compressed (alphaChar='YZ')\n" << std::endl;

    while (true) {
        int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        
        if (bytes_received < 0) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                std::cout << "Timeout waiting for data. Retrying..." << std::endl;
                continue;
            }
            std::cerr << "Receive error: " << error << std::endl;
            break;
        }

        // NSE broadcast messages have a 40-byte BroadcastHeader followed by the message
        if (bytes_received < (int)sizeof(BroadcastHeader)) {
            continue; // Too small to be a valid message
        }

        msg_count++;

        // Parse broadcast header first (40 bytes)
        BroadcastHeader* bcHeader = (BroadcastHeader*)buffer;
        uint16_t trans_code = be16toh_func(bcHeader->transactionCode);
        uint16_t msg_length = be16toh_func(bcHeader->messageLength);
        bool is_compressed = (bcHeader->alphaChar[0] == 'Y' && bcHeader->alphaChar[1] == 'Z');
        
        if (is_compressed) compressed_count++;
        
        // Debug: Log first 10 messages
        if (msg_count <= 10) {
            std::cout << "Message #" << msg_count << ":\n";
            std::cout << "  BC TransCode=" << trans_code 
                      << " (0x" << std::hex << std::setw(4) << std::setfill('0') << trans_code << std::dec << ")";
            std::cout << ", MsgLen=" << msg_length
                      << ", Compressed=" << (is_compressed ? "YES" : "NO")
                      << ", Size=" << bytes_received << " bytes\n" << std::endl;
        }

        // Decompress if needed
        uint8_t* message_data = nullptr;
        int message_size = 0;
        
        if (is_compressed) {
            try {
                // Decompress the data after the 40-byte broadcast header
                const uint8_t* compressed_data = (uint8_t*)(buffer + sizeof(BroadcastHeader));
                int compressed_size = bytes_received - sizeof(BroadcastHeader);
                
                if (msg_count <= 10) {
                    std::cout << "  Attempting LZO decompression: " << compressed_size << " bytes..." << std::endl;
                }
                
                int decompressed_size = -1;
                try {
                    decompressed_size = DecompressUltra(compressed_data, compressed_size, decompressed, sizeof(decompressed));
                } catch (...) {
                    decompression_errors++;
                    if (msg_count <= 20) {
                        std::cerr << "  LZO decompression threw exception" << std::endl;
                    }
                    continue;
                }
                
                if (decompressed_size <= 0) {
                    decompression_errors++;
                    if (msg_count <= 20) {
                        std::cerr << "  LZO decompression failed: returned " << decompressed_size << std::endl;
                    }
                    continue;
                }
                
                if (msg_count <= 10) {
                    std::cout << "  Decompressed: " << decompressed_size << " bytes" << std::endl;
                }
                
                if (decompressed_size > 8) {
                    // Skip first 8 bytes of decompressed data (protocol requirement)
                    message_data = decompressed + 8;
                    message_size = decompressed_size - 8;
                } else {
                    if (msg_count <= 10) {
                        std::cerr << "  Warning: Decompressed data too small: " << decompressed_size << std::endl;
                    }
                    continue;
                }
            } catch (const LZOException& e) {
                decompression_errors++;
                if (msg_count <= 20) {
                    std::cerr << "  LZO exception: " << e.what() << std::endl;
                }
                continue;
            } catch (const std::exception& e) {
                decompression_errors++;
                if (msg_count <= 20) {
                    std::cerr << "  Exception: " << e.what() << std::endl;
                }
                continue;
            } catch (...) {
                decompression_errors++;
                if (msg_count <= 20) {
                    std::cerr << "  Unknown error during decompression" << std::endl;
                }
                continue;
            }
        } else {
            // Uncompressed - data starts after broadcast header
            message_data = (uint8_t*)(buffer + sizeof(BroadcastHeader));
            message_size = bytes_received - sizeof(BroadcastHeader);
        }

        // Now parse the transaction code from the actual message header
        if (message_size >= sizeof(MESSAGE_HEADER)) {
            MESSAGE_HEADER* msgHeader = (MESSAGE_HEADER*)message_data;
            trans_code = be16toh_func(msgHeader->transactionCode);
            
            if (msg_count <= 10) {
                std::cout << "  Decoded TransCode: " << trans_code 
                          << " (0x" << std::hex << std::setw(4) << std::setfill('0') << trans_code << std::dec << ")" << std::endl;
            }
        } else {
            if (msg_count <= 10) {
                std::cerr << "  Warning: Message too small for header: " << message_size << " bytes" << std::endl;
            }
            continue;
        }
        
        // Track transaction code statistics
        trans_code_stats[trans_code]++;

        if (trans_code == 7207) {
            indices_7207_count++;
            std::cout << "\n*** FOUND 7207 message #" << indices_7207_count 
                      << " (total msgs: " << msg_count << ") ***" << std::endl;
            std::cout << "    Compressed: " << (is_compressed ? "YES" : "NO") << std::endl;
            std::cout << "    Message size: " << message_size << " bytes" << std::endl;

            // Parse the indices message
            process_7207_message(message_data, message_size, log_file);
        }

        // Status update every 5000 messages with transaction code distribution
        if (msg_count % 5000 == 0) {
            std::cout << "\n=== Stats at " << msg_count << " messages ===" << std::endl;
            std::cout << "Compressed: " << compressed_count << " (" 
                      << (compressed_count * 100.0 / msg_count) << "%)" << std::endl;
            std::cout << "Decompression errors: " << decompression_errors << std::endl;
            std::cout << "7207 (Indices): " << indices_7207_count << std::endl;
            std::cout << "Top 15 Transaction Codes:" << std::endl;
            
            std::vector<std::pair<uint16_t, int>> sorted_stats(trans_code_stats.begin(), trans_code_stats.end());
            std::sort(sorted_stats.begin(), sorted_stats.end(), 
                      [](const auto& a, const auto& b) { return a.second > b.second; });
            
            int count = 0;
            for (const auto& [code, cnt] : sorted_stats) {
                if (count++ >= 15) break;
                std::cout << "  TransCode " << code << " (0x" << std::hex << std::setw(4) 
                          << std::setfill('0') << code << std::dec << "): " << cnt << " messages" << std::endl;
            }
            std::cout << std::endl;
        }
    }

    closesocket(sockfd);
    WSACleanup();
    
    std::cout << "\nSession ended. Total messages: " << msg_count 
              << ", 7207 indices messages: " << indices_7207_count << std::endl;

    return 0;
}
