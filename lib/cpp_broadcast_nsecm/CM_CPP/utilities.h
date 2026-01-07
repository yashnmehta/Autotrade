// NSE Capital Market Receiver - Shared Utilities Header
//
// Common utility functions used across all message decoders
//
// Created: December 26, 2025

#ifndef UTILITIES_H
#define UTILITIES_H

#include <string>
#include <chrono>
#include <cstdint>

// Platform-specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define SOCKET_CLOSE closesocket
    #define SOCKET_ERROR_CHECK(x) ((x) == SOCKET_ERROR)
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/select.h>
    #define SOCKET_CLOSE close
    #define SOCKET_ERROR_CHECK(x) ((x) < 0)
    typedef int SOCKET;
    #define INVALID_SOCKET -1
#endif

// Utility function declarations
#ifdef _WIN32
std::string getWinSockError();
#endif

std::string getCurrentTimestamp();
std::string getFileTimestamp();
uint16_t readUint16BigEndian(const uint8_t* data, int offset);
uint32_t readUint32BigEndian(const uint8_t* data, int offset);
uint64_t readUint64BigEndian(const uint8_t* data, int offset);

#endif // UTILITIES_H
