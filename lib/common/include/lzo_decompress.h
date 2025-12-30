#ifndef COMMON_LZO_DECOMPRESS_H
#define COMMON_LZO_DECOMPRESS_H

#include <vector>
#include <cstdint>
#include <stdexcept>

namespace common {

// Error codes
enum class LzoError {
    OK,
    INPUT_OVERRUN,
    OUTPUT_OVERRUN,
    CORRUPTED_DATA
};

class LzoDecompressor {
public:
    // Custom LZO1Z decompression implementation (ported from Go)
    // Returns number of bytes written to dst, or throws runtime_error on error
    static int decompress(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst);
    
    // Library-based LZO1Z decompression using official liblzo2
    // Returns number of bytes written to dst, or throws runtime_error on error
    static int decompressWithLibrary(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst);
};

} // namespace common

#endif // COMMON_LZO_DECOMPRESS_H
