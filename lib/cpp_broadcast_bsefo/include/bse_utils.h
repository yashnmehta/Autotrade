#ifndef BSE_UTILS_H
#define BSE_UTILS_H

#include <cstdint>

namespace bse {

// Big Endian to Host
inline uint16_t be16toh_func(uint16_t x) {
    return (x << 8) | (x >> 8);
}

inline uint32_t be32toh_func(uint32_t x) {
    return ((x << 24) & 0xff000000 ) |
           ((x <<  8) & 0x00ff0000 ) |
           ((x >>  8) & 0x0000ff00 ) |
           ((x >> 24) & 0x000000ff );
}

// Little Endian to Host (Assuming Host is LE, so no-op)
// If Host is BE, these would need swapping. We assume LE (x86/ARM).
inline uint16_t le16toh_func(uint16_t x) {
    return x;
}

inline uint32_t le32toh_func(uint32_t x) {
    return x;
}

inline int32_t le32toh_signed(int32_t x) {
    return x;
}

} // namespace bse

#endif // BSE_UTILS_H
