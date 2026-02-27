// DEPRECATED: The endianness helpers here duplicate be16toh_func/be32toh_func
// in protocol.h.  Include protocol.h instead for any new code.
//
// This file is kept for backward compatibility only.

#pragma once

#include "protocol.h"  // be16toh_func, be32toh_func, be64toh_func

// Legacy names – delegate to the canonical implementations in protocol.h
// so there is a single source of truth.
inline uint16_t be16toh(uint16_t x) { return be16toh_func(x); }
inline uint32_t be32toh(uint32_t x) { return be32toh_func(x); }

// Hex dump utility (non-endianness, kept as-is)
inline void print_hex_dump(const char* data, int len) {
    for (int i = 0; i < len; i++) {
        printf("%02x ", (unsigned char)data[i]);
    }
    printf("\n");
}