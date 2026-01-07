// NSE Capital Market Receiver - Shared Utilities
//
// Common utility functions used across all message decoders
//
// Created: December 26, 2025

#include "utilities.h"
#include <iomanip>
#include <sstream>

#ifdef _WIN32
std::string getWinSockError() {
    int error = WSAGetLastError();
    std::stringstream ss;
    ss << "WSA Error " << error;
    return ss.str();
}
#endif

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string getFileTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

uint16_t readUint16BigEndian(const uint8_t* data, int offset) {
    return (static_cast<uint16_t>(data[offset]) << 8) | 
           static_cast<uint16_t>(data[offset + 1]);
}

uint32_t readUint32BigEndian(const uint8_t* data, int offset) {
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           static_cast<uint32_t>(data[offset + 3]);
}

uint64_t readUint64BigEndian(const uint8_t* data, int offset) {
    return (static_cast<uint64_t>(data[offset]) << 56) |
           (static_cast<uint64_t>(data[offset + 1]) << 48) |
           (static_cast<uint64_t>(data[offset + 2]) << 40) |
           (static_cast<uint64_t>(data[offset + 3]) << 32) |
           (static_cast<uint64_t>(data[offset + 4]) << 24) |
           (static_cast<uint64_t>(data[offset + 5]) << 16) |
           (static_cast<uint64_t>(data[offset + 6]) << 8) |
           static_cast<uint64_t>(data[offset + 7]);
}
