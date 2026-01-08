#ifndef BSE_UTILS_H
#define BSE_UTILS_H

#include <cstdint>
#include "socket_platform.h"

namespace bse {
namespace bse_utils {

// Endianness converters - cross-platform
#ifdef _WIN32
    // Windows: use winsock functions
    inline uint16_t be16toh_func(uint16_t val) { return ntohs(val); }
    inline uint32_t be32toh_func(uint32_t val) { return ntohl(val); }
#else
    // Unix/Linux/macOS: use standard functions
    inline uint16_t be16toh_func(uint16_t val) { return ntohs(val); }
    inline uint32_t be32toh_func(uint32_t val) { return ntohl(val); }
#endif

inline uint64_t be64toh_func(uint64_t val) {
    uint32_t high = ntohl(val >> 32);
    uint32_t low = ntohl(val & 0xFFFFFFFF);
    return ((uint64_t)low << 32) | high;
}

inline uint16_t le16toh_func(uint16_t val) {
    const uint8_t* p = (const uint8_t*)&val;
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

inline uint32_t le32toh_func(uint32_t val) {
    const uint8_t* p = (const uint8_t*)&val;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

inline uint64_t le64toh_func(uint64_t val) {
    const uint8_t* p = (const uint8_t*)&val;
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) | ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

// Host to Big Endian (Network)
inline uint32_t htobe32_func(uint32_t val) { return htonl(val); }

// Host to Little Endian (Identity on Windows/x86)
inline uint16_t htole16_func(uint16_t val) { return val; }
inline uint32_t htole32_func(uint32_t val) { return val; }
inline uint64_t htole64_func(uint64_t val) { return val; }

// Reads a compressed field (delta from base)
// Advances cursor automatically.
inline int32_t read_compressed(const uint8_t* buffer, size_t& cursor, int32_t base) {
    int16_t diff = (int16_t)be16toh_func(*(uint16_t*)(buffer + cursor));
    cursor += 2;
    
    // Check for overflow marker (0x7FFF = 32767)
    if (diff == 32767) {
        int32_t val = (int32_t)be32toh_func(*(uint32_t*)(buffer + cursor));
        cursor += 4;
        return val;
    }
    
    return base + diff;
}

} // namespace bse_utils
} // namespace bse

#endif // BSE_UTILS_H
