#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <cstring>

#pragma pack(push, 1)

// NSE Broadcast Header (40 bytes)
struct BroadcastHeader {
    uint8_t reserved1[2];
    uint8_t reserved2[2];
    uint32_t logTime;
    uint8_t alphaChar[2];
    uint16_t transactionCode;
    uint16_t errorCode;
    uint32_t bcSeqNo;
    uint8_t reserved3;
    uint8_t reserved4[3];
    uint8_t timeStamp2[8];
    uint8_t filler[8];
    uint16_t messageLength;
};

#pragma pack(pop)

// Endianness handling (Big Endian to Host)
inline uint16_t be16toh_func(uint16_t x) {
    return (x << 8) | (x >> 8);
}

inline uint32_t be32toh_func(uint32_t x) {
    return ((x << 24) & 0xff000000 ) |
           ((x <<  8) & 0x00ff0000 ) |
           ((x >>  8) & 0x0000ff00 ) |
           ((x >> 24) & 0x000000ff );
}

inline uint64_t be64toh_func(uint64_t x) {
    return ((x << 56) & 0xff00000000000000ULL) |
           ((x << 40) & 0x00ff000000000000ULL) |
           ((x << 24) & 0x0000ff0000000000ULL) |
           ((x <<  8) & 0x000000ff00000000ULL) |
           ((x >>  8) & 0x00000000ff000000ULL) |
           ((x >> 24) & 0x0000000000ff0000ULL) |
           ((x >> 40) & 0x000000000000ff00ULL) |
           ((x >> 56) & 0x00000000000000ffULL);
}

#endif // PROTOCOL_H
