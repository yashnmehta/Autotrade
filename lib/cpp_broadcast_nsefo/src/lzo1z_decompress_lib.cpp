#include "lzo_decompress.h"
#include <lzo/lzo1z.h>
#include <stdexcept>
#include <cstring>

// LZO Library-based decompression
// Uses official lzo1z_decompress_safe function from liblzo2

int LzoDecompressor::decompressWithLibrary(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst) {
    if (src.empty()) {
        throw std::runtime_error("LZO: Empty input");
    }
    
    if (dst.empty()) {
        throw std::runtime_error("LZO: Output buffer not allocated");
    }

    // Initialize LZO library (only needs to be done once, but safe to call multiple times)
    if (lzo_init() != LZO_E_OK) {
        throw std::runtime_error("LZO: Library initialization failed");
    }

    // Prepare for decompression
    lzo_uint out_len = dst.size();
    
    // Call the official LZO1Z decompression function
    int result = lzo1z_decompress_safe(
        src.data(),           // Input compressed data
        src.size(),           // Input size
        dst.data(),           // Output buffer
        &out_len,             // Output size (will be updated with actual decompressed size)
        nullptr               // No working memory needed for decompression
    );
    
    // Check result
    switch (result) {
        case LZO_E_OK:
            // Success - return the actual decompressed size
            return static_cast<int>(out_len);
            
        case LZO_E_INPUT_OVERRUN:
            throw std::runtime_error("LZO: Input overrun - compressed data is corrupted");
            
        case LZO_E_OUTPUT_OVERRUN:
            throw std::runtime_error("LZO: Output overrun - output buffer too small");
            
        case LZO_E_LOOKBEHIND_OVERRUN:
            throw std::runtime_error("LZO: Lookbehind overrun - compressed data is corrupted");
            
        case LZO_E_EOF_NOT_FOUND:
            throw std::runtime_error("LZO: EOF marker not found");
            
        case LZO_E_INPUT_NOT_CONSUMED:
            throw std::runtime_error("LZO: Input not fully consumed");
            
        case LZO_E_ERROR:
        default:
            throw std::runtime_error("LZO: Decompression failed with error code " + std::to_string(result));
    }
}
