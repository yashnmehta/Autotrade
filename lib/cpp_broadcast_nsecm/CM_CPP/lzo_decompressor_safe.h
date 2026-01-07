// LZO1Z Decompression Header File
//
// This is an ULTRA-OPTIMIZED version using direct pointer manipulation.
// WARNING: This trades safety for performance. Use only with validated inputs.
//
// USAGE: Include this header in your CM message decoder files
//
// Created: December 26, 2025
// Purpose: Header for shared LZO decompressor for all CM message decoders
// Converted from Go implementation

#ifndef LZO_DECOMPRESSOR_SAFE_H
#define LZO_DECOMPRESSOR_SAFE_H

#include <cstdint>
#include <stdexcept>
#include <string>

// LZO1Z constants
extern const int M2_MAX_OFFSET;

// LZO error types
class LZOException : public std::exception {
private:
    std::string message;
public:
    explicit LZOException(const std::string& msg) : message(msg) {}
    const char* what() const noexcept override {
        return message.c_str();
    }
};

class LZOInputOverrun : public LZOException {
public:
    LZOInputOverrun() : LZOException("LZO input overrun") {}
};

class LZOOutputOverrun : public LZOException {
public:
    LZOOutputOverrun() : LZOException("LZO output overrun") {}
};

class LZOCorrupted : public LZOException {
public:
    LZOCorrupted() : LZOException("LZO data corrupted") {}
};

// Main decompression function
// Parameters:
//   src: Input compressed data buffer
//   srcLen: Length of input data
//   dst: Output decompressed data buffer
//   dstLen: Maximum length of output buffer
// Returns: Actual number of bytes decompressed
// Throws: LZOException on error
int DecompressUltra(const uint8_t* src, int srcLen, uint8_t* dst, int dstLen);

// Internal helper function for optimized match copying
int copyMatchUltraFast(uint8_t* dst, int op, int mPos, int length);

#endif // LZO_DECOMPRESSOR_SAFE_H
