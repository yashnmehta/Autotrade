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
#include "../lib/cpp_broadcast_nsecm/include/protocol.h" 
#include "../lib/cpp_broadcast_nsecm/include/nse_common.h"
#include "../lib/cpp_broadcast_nsecm/include/nse_index_messages.h"
#include "../lib/cpp_broadcast_nsecm/CM_CPP/lzo_decompressor_safe.h"

// Define a struct to hold decoded data
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
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Section header
        if (line[0] == '[' && line[line.length() - 1] == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }

        // Key-value pair
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            
            // Trim key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // Store with section prefix
            std::string full_key = current_section.empty() ? key : current_section + "." + key;
            config[full_key] = value;
        }
    }

    file.close();
    return config;
}

// Log decoded indices to file
void log_indices(const std::vector<DecodedIndex>& indices, const std::string& filename) {
    std::ofstream log_file(filename, std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
        return;
    }

    time_t now = time(nullptr);
    log_file << "\n=== Indices Update: " << ctime(&now);
    log_file << "Number of Indices: " << indices.size() << "\n";

    for (const auto& idx : indices) {
        log_file << "\n Index: " << idx.name << "\n";
        log_file << "  Value: " << idx.value << "\n";
        log_file << "  High: " << idx.high << " | Low: " << idx.low << "\n";
        log_file << "  Open: " << idx.open << " | Close: " << idx.close << "\n";
        log_file << "  Percent Change: " << idx.percentChange << "% | Net Change: " << idx.netChangeIndicator << "\n";
        log_file << "  Yearly High: " << idx.yearlyHigh << " | Yearly Low: " << idx.yearlyLow << "\n";
        log_file << "  Up Moves: " << idx.upMoves << " | Down Moves: " << idx.downMoves << "\n";
        log_file << "  Market Cap: " << std::fixed << std::setprecision(2) << idx.marketCap << "\n";
    }

    log_file.close();
    std::cout << "Logged " << indices.size() << " indices to " << filename << std::endl;
}

// Process a 7207 message (indices data)
void process_7207_message(const unsigned char* message_data, size_t message_size, const std::string& log_file) {
    std::cout << "  Processing 7207 message (" << message_size << " bytes)..." << std::endl;

    // The message structure after decompression and skipping headers:
    // BCAST_HEADER (40 bytes) + numberOfRecords (2 bytes) + MS_INDICES[numberOfRecords]
    
    // Skip the BCAST_HEADER (40 bytes)
    if (message_size < 42) {
        std::cerr << "    ERROR: Message too small: " << message_size << " bytes (need at least 42)" << std::endl;
        return;
    }

    const unsigned char* data_ptr = message_data + 40; // Skip BCAST_HEADER
    
    // Read numberOfRecords (2 bytes, big-endian)
    uint16_t numberOfRecords = be16toh_func(*(uint16_t*)data_ptr);
    data_ptr += 2;

    std::cout << "    Number of records: " << numberOfRecords << std::endl;

    // Safety check
    if (numberOfRecords == 0 || numberOfRecords > 100) {
        std::cerr << "    WARNING: Suspicious numberOfRecords = " << numberOfRecords << std::endl;
        std::cout << "    First 64 bytes (hex): ";
        for (size_t i = 0; i < std::min(size_t(64), message_size); i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)message_data[i] << " ";
            if ((i + 1) % 16 == 0) std::cout << "\n                          ";
        }
        std::cout << std::dec << std::endl;
        return;
    }

    size_t expected_size = 42 + numberOfRecords * sizeof(MS_INDICES);
    if (message_size < expected_size) {
        std::cerr << "    ERROR: Message too small for " << numberOfRecords << " records" << std::endl;
        std::cerr << "           Expected: " << expected_size << " bytes, Got: " << message_size << " bytes" << std::endl;
        return;
    }

    std::vector<DecodedIndex> decoded_indices;

    for (uint16_t i = 0; i < numberOfRecords; i++) {
        const MS_INDICES* rec = reinterpret_cast<const MS_INDICES*>(data_ptr);
        data_ptr += sizeof(MS_INDICES);

        DecodedIndex data;
        // Copy index name (21 bytes)
        std::memcpy(&data.name[0], rec->indexName, sizeof(rec->indexName));
        data.name[sizeof(rec->indexName)] = '\0';

        // Decode fields (big-endian conversion)
        // Note: MS_INDICES uses int32_t for most fields, not int64_t
        data.value = be32toh_func(rec->indexValue) / 100.0;
        data.high = be32toh_func(rec->highIndexValue) / 100.0;
        data.low = be32toh_func(rec->lowIndexValue) / 100.0;
        data.open = be32toh_func(rec->openingIndex) / 100.0;
        data.close = be32toh_func(rec->closingIndex) / 100.0;
        data.percentChange = be32toh_func(rec->percentChange) / 100.0;
        data.yearlyHigh = be32toh_func(rec->yearlyHigh) / 100.0;
        data.yearlyLow = be32toh_func(rec->yearlyLow) / 100.0;
        data.upMoves = be32toh_func(rec->noOfUpmoves);
        data.downMoves = be32toh_func(rec->noOfDownmoves);
        data.marketCap = be64toh_func(rec->marketCapitalisation) / 100.0;
        data.netChangeIndicator = rec->netChangeIndicator;

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
    std::string log_file = "nse_7207_decoded.txt";
    std::string config_file = "configs/config.ini";

    std::cout << "=== NSE CM 7207 (Indices) Broadcast Receiver ===" << std::endl;
    std::cout << "Reading configuration..." << std::endl;

    auto config = parse_config(config_file);

    std::string multicast_ip = config["UDP.nse_cm_multicast_ip"];
    std::string port_str = config["UDP.nse_cm_port"];

    if (multicast_ip.empty() || port_str.empty()) {
        std::cerr << "Failed to read configuration. Check configs/config.ini" << std::endl;
        return 1;
    }

    int port = std::stoi(port_str);
    std::cout << "NSE CM Multicast: " << multicast_ip << ":" << port << std::endl;

    SOCKET sockfd = setup_udp_socket(multicast_ip, port);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Failed to setup UDP socket" << std::endl;
        return 1;
    }

    // Clear log file
    std::ofstream clearLog(log_file, std::ios::trunc);
    clearLog << "=== Session Started ===" << std::endl;
    clearLog.close();

    std::cout << "\nListening for 7207 messages... (Press Ctrl+C to stop)" << std::endl;
    std::cout << "Output will be logged to: " << log_file << std::endl << std::endl;

    // Statistics
    int message_count = 0;
    int compressed_count = 0;
    int decompression_errors = 0;
    int found_7207_count = 0;
    std::map<uint16_t, int> trans_code_stats;

    // Buffers
    unsigned char buffer[8192];
    unsigned char decompressed[8192];

    while (true) {
        int recv_len = recvfrom(sockfd, (char*)buffer, sizeof(buffer), 0, NULL, NULL);
        
        if (recv_len < 0) {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                continue; // Normal timeout
            }
            std::cerr << "Receive error: " << err << std::endl;
            break;
        }

        message_count++;

        // Check minimum size for BroadcastHeader
        if (recv_len < (int)sizeof(BroadcastHeader)) {
            continue;
        }

        // Parse BroadcastHeader (40 bytes at start of UDP packet)
        const BroadcastHeader* header = reinterpret_cast<const BroadcastHeader*>(buffer);
        uint16_t trans_code = be16toh_func(header->transactionCode);
        bool is_compressed = (header->alphaChar[0] == 'Y' && header->alphaChar[1] == 'Z');

        if (is_compressed) compressed_count++;

        // Track transaction codes
        trans_code_stats[trans_code]++;

        // Print stats every 10000 messages
        if (message_count % 10000 == 0) {
            std::cout << "\n[Stats after " << message_count << " messages]" << std::endl;
            std::cout << "  Compressed: " << compressed_count << " (" 
                      << (100.0 * compressed_count / message_count) << "%)" << std::endl;
            std::cout << "  Decompression errors: " << decompression_errors << std::endl;
            std::cout << "  Found 7207: " << found_7207_count << std::endl;
            std::cout << "  Transaction codes seen: " << trans_code_stats.size() << std::endl;
        }

        // Process 7207 messages
        if (trans_code == 7207) {
            found_7207_count++;
            std::cout << "\n*** FOUND 7207 message #" << found_7207_count 
                      << " (total msg #" << message_count << ")" << std::endl;
            std::cout << "    Compressed: " << (is_compressed ? "YES" : "NO")
                      << " | Size: " << recv_len << " bytes" << std::endl;

            try {
                const unsigned char* message_data = nullptr;
                size_t message_size = 0;

                if (is_compressed) {
                    // Decompress: skip 40-byte BroadcastHeader
                    const unsigned char* compressed_data = buffer + sizeof(BroadcastHeader);
                    size_t compressed_size = recv_len - sizeof(BroadcastHeader);

                    std::cout << "    Decompressing " << compressed_size << " bytes..." << std::flush;

                    int decompressed_size = 0;
                    try {
                        // Add bounds checking
                        if (compressed_size == 0 || compressed_size > 8000) {
                            std::cerr << " INVALID SIZE" << std::endl;
                            decompression_errors++;
                            continue;
                        }

                        memset(decompressed, 0, sizeof(decompressed));
                        decompressed_size = DecompressUltra(compressed_data, compressed_size, 
                                                          decompressed, sizeof(decompressed));
                        
                        if (decompressed_size <= 0 || decompressed_size > 8000) {
                            std::cerr << " FAILED (size=" << decompressed_size << ")" << std::endl;
                            decompression_errors++;
                            continue;
                        }

                        std::cout << " OK (" << decompressed_size << " bytes)" << std::endl;

                    } catch (const LZOException& e) {
                        std::cerr << " LZO ERROR: " << e.what() << std::endl;
                        decompression_errors++;
                        continue;
                    } catch (...) {
                        std::cerr << " UNKNOWN ERROR" << std::endl;
                        decompression_errors++;
                        continue;
                    }

                    // Skip first 8 bytes of decompressed data
                    if (decompressed_size <= 8) {
                        std::cerr << "    Decompressed data too small" << std::endl;
                        continue;
                    }

                    message_data = decompressed + 8;
                    message_size = decompressed_size - 8;
                } else {
                    // Uncompressed: skip BroadcastHeader + 8 bytes
                    if (recv_len <= (int)(sizeof(BroadcastHeader) + 8)) {
                        std::cerr << "    Uncompressed message too small" << std::endl;
                        continue;
                    }
                    message_data = buffer + sizeof(BroadcastHeader) + 8;
                    message_size = recv_len - sizeof(BroadcastHeader) - 8;
                }

                // Process the message
                process_7207_message(message_data, message_size, log_file);

            } catch (const std::exception& e) {
                std::cerr << "    Exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "    Unknown exception" << std::endl;
            }
        }
    }

    closesocket(sockfd);
    WSACleanup();
    return 0;
}
