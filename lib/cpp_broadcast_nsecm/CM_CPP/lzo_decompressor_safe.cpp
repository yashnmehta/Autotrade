// LZO1Z Decompression Implementation in C++
//
// This is an ULTRA-OPTIMIZED version using direct pointer manipulation.
// WARNING: This trades safety for performance. Use only with validated inputs.
//
// USAGE: This file should be compiled together with CM message decoder files
//
// Created: December 26, 2025
// Purpose: Shared LZO decompressor for all CM message decoders (DRY principle)
// Converted from Go implementation

#include <cstdint>
#include <cstring>
#include <stdexcept>

// LZO1Z constants
const int M2_MAX_OFFSET = 0x0700;

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

// Forward declaration
int copyMatchUltraFast(uint8_t* dst, int op, int mPos, int length);

// DecompressUltra is an ultra-optimized version using direct pointer access
// Optimizations:
// - Uses direct pointer arithmetic to eliminate bounds checking
// - Manually inlined copyMatchFast for common cases
// - Unrolled small copy loops
// - Branchless optimizations where possible
// - 8-byte bulk copies using uint64_t
//
// Target: 270-300 MB/s (35-50% faster than standard implementation)
int DecompressUltra(const uint8_t* src, int srcLen, uint8_t* dst, int dstLen) {
    if (srcLen < 1 || dstLen < 1) {
        throw LZOInputOverrun();
    }

    int ip = 0;
    int op = 0;
    int lastMOff = 0;
    int t;
    int mPos;
    int mLen;
    int off;
    int totalLen;

    int ipEnd = srcLen;
    int opEnd = dstLen;

    // Helper macros for fast access (no bounds checking)
    #define GET_U8(ptr, offset) (ptr[offset])
    #define SET_U8(ptr, offset, val) (ptr[offset] = val)

    // Read first instruction
    t = GET_U8(src, ip);
    ip++;

    // Handle initial literal run
    if (t > 17) {
        t = t - 17;
        if (t < 4) {
            // Small literal: copy then continue as match_next
            if (op + t > opEnd || ip + t > ipEnd) {
                throw LZOOutputOverrun();
            }
            // Inline small copy (up to 3 bytes)
            for (int i = 0; i < t; i++) {
                SET_U8(dst, op + i, GET_U8(src, ip + i));
            }
            op += t;
            ip += t;

            if (ip >= ipEnd) {
                return op;
            }
            t = GET_U8(src, ip);
            ip++;
        } else {
            // Large literal: copy then goto first_literal_run
            if (op + t > opEnd || ip + t > ipEnd) {
                throw LZOOutputOverrun();
            }
            // Use bulk copy for larger literals
            memcpy(dst + op, src + ip, t);
            op += t;
            ip += t;

            if (ip >= ipEnd) {
                return op;
            }

            t = GET_U8(src, ip);
            ip++;

            if (t >= 16) {
                // t >= 16: it's a match, will be handled in main loop
            } else {
                // M2 match after initial literal run
                if (ip >= ipEnd) {
                    throw LZOInputOverrun();
                }

                off = (1 + M2_MAX_OFFSET) + (t << 6) + (GET_U8(src, ip) >> 2);
                ip++;

                mPos = op - off;
                lastMOff = off;

                if (op + 3 > opEnd) {
                    throw LZOOutputOverrun();
                }

                // Inline 3-byte match copy
                if (mPos + 3 > op) {
                    // Overlapping - byte by byte
                    SET_U8(dst, op, GET_U8(dst, mPos));
                    SET_U8(dst, op + 1, GET_U8(dst, mPos + 1));
                    SET_U8(dst, op + 2, GET_U8(dst, mPos + 2));
                } else {
                    // Non-overlapping - can use direct copy
                    SET_U8(dst, op, GET_U8(dst, mPos));
                    SET_U8(dst, op + 1, GET_U8(dst, mPos + 1));
                    SET_U8(dst, op + 2, GET_U8(dst, mPos + 2));
                }
                op += 3;

                if (ip >= ipEnd) {
                    return op;
                }

                t = GET_U8(src, ip - 1) & 3;

                if (t != 0) {
                    // Copy few literals
                    if (op + t > opEnd || ip + t > ipEnd) {
                        throw LZOOutputOverrun();
                    }
                    for (int i = 0; i < t; i++) {
                        SET_U8(dst, op + i, GET_U8(src, ip + i));
                    }
                    op += t;
                    ip += t;

                    if (ip >= ipEnd) {
                        return op;
                    }

                    t = GET_U8(src, ip);
                    ip++;
                } else {
                    if (ip + 3 > ipEnd) {
                        throw LZOInputOverrun();
                    }
                    t = GET_U8(src, ip);
                    ip++;
                }
            }
        }
    }

    // Main decompression loop
    while (true) {
        if (t >= 16) {
            goto matchHandling;
        }

        // Literal run (t < 16)
        if (t == 0) {
            // Extended literal length - unrolled for common case
            while (ip < ipEnd && GET_U8(src, ip) == 0) {
                t += 255;
                ip++;
            }
            if (ip >= ipEnd) {
                throw LZOInputOverrun();
            }
            t += 15 + GET_U8(src, ip);
            ip++;
        }

        // Copy literals (minimum 3)
        t += 3;
        if (op + t > opEnd || ip + t > ipEnd) {
            throw LZOOutputOverrun();
        }
        memcpy(dst + op, src + ip, t);
        op += t;
        ip += t;

        if (ip >= ipEnd) {
            return op;
        }

        t = GET_U8(src, ip);
        ip++;

        if (t >= 16) {
            goto matchHandling;
        }

        // M1 match after literal run
        if (ip >= ipEnd) {
            throw LZOInputOverrun();
        }

        off = 1 + (t << 6) + (GET_U8(src, ip) >> 2);
        ip++;
        mPos = op - off;
        lastMOff = off;

        if (op + 3 > opEnd) {
            throw LZOOutputOverrun();
        }

        // Inline 3-byte match copy
        if (mPos + 3 > op) {
            SET_U8(dst, op, GET_U8(dst, mPos));
            SET_U8(dst, op + 1, GET_U8(dst, mPos + 1));
            SET_U8(dst, op + 2, GET_U8(dst, mPos + 2));
        } else {
            SET_U8(dst, op, GET_U8(dst, mPos));
            SET_U8(dst, op + 1, GET_U8(dst, mPos + 1));
            SET_U8(dst, op + 2, GET_U8(dst, mPos + 2));
        }
        op += 3;

        goto matchDone;

    matchHandling:
        // Match handling (M2/M3/M4/M1 in match loop)
        if (t >= 64) {
            // M2 match (64-255)
            off = t & 0x1f;

            if (off >= 0x1c) {
                // Use cached offset
                if (lastMOff == 0) {
                    throw LZOCorrupted();
                }
                mPos = op - lastMOff;
            } else {
                // New offset
                if (ip >= ipEnd) {
                    throw LZOInputOverrun();
                }
                off = 1 + (off << 6) + (GET_U8(src, ip) >> 2);
                ip++;
                mPos = op - off;
                lastMOff = off;
            }

            mLen = (t >> 5) - 1;

        } else if (t >= 32) {
            // M3 match (32-63)
            t &= 31;

            if (t == 0) {
                // Extended length
                while (ip < ipEnd && GET_U8(src, ip) == 0) {
                    t += 255;
                    ip++;
                }
                if (ip >= ipEnd) {
                    throw LZOInputOverrun();
                }
                t += 31 + GET_U8(src, ip);
                ip++;
            }

            if (ip + 2 > ipEnd) {
                throw LZOInputOverrun();
            }

            off = 1 + (GET_U8(src, ip) << 6) + (GET_U8(src, ip + 1) >> 2);
            ip += 2;

            mPos = op - off;
            lastMOff = off;
            mLen = t;

        } else if (t >= 16) {
            // M4 match (16-31)
            mPos = op;
            mPos -= (t & 8) << 11;

            t &= 7;

            if (t == 0) {
                // Extended length
                while (ip < ipEnd && GET_U8(src, ip) == 0) {
                    t += 255;
                    ip++;
                }
                if (ip >= ipEnd) {
                    throw LZOInputOverrun();
                }
                t += 7 + GET_U8(src, ip);
                ip++;
            }
            if (ip + 2 > ipEnd) {
                throw LZOInputOverrun();
            }

            mPos -= (GET_U8(src, ip) << 6) + (GET_U8(src, ip + 1) >> 2);
            ip += 2;
            if (mPos == op) {
                return op;
            }

            mPos -= 0x4000;
            lastMOff = op - mPos;
            mLen = t;
        } else {
            // M1 match (0-15) in the inner match loop
            if (ip >= ipEnd) {
                throw LZOInputOverrun();
            }

            off = 1 + (t << 6) + (GET_U8(src, ip) >> 2);
            ip++;

            mPos = op - off;
            lastMOff = off;

            if (op + 2 > opEnd) {
                throw LZOOutputOverrun();
            }

            // Inline 2-byte copy
            SET_U8(dst, op, GET_U8(dst, mPos));
            SET_U8(dst, op + 1, GET_U8(dst, mPos + 1));
            op += 2;

            goto matchDone;
        }

        // For M2/M3/M4: Copy match (2 + mLen bytes total)
        totalLen = 2 + mLen;
        if (op + totalLen > opEnd) {
            throw LZOOutputOverrun();
        }

        // Optimized match copy
        op = copyMatchUltraFast(dst, op, mPos, totalLen);

        goto matchDone;

    matchDone:
        t = GET_U8(src, ip - 1) & 3;

        if (ip >= ipEnd) {
            return op;
        }

        if (t != 0) {
            // Copy few literals
            if (op + t > opEnd || ip + t > ipEnd) {
                throw LZOOutputOverrun();
            }

            // Unrolled for common cases (1-3 bytes)
            switch (t) {
                case 1:
                    SET_U8(dst, op, GET_U8(src, ip));
                    break;
                case 2:
                    SET_U8(dst, op, GET_U8(src, ip));
                    SET_U8(dst, op + 1, GET_U8(src, ip + 1));
                    break;
                case 3:
                    SET_U8(dst, op, GET_U8(src, ip));
                    SET_U8(dst, op + 1, GET_U8(src, ip + 1));
                    SET_U8(dst, op + 2, GET_U8(src, ip + 2));
                    break;
                default:
                    memcpy(dst + op, src + ip, t);
                    break;
            }
            op += t;
            ip += t;

            if (ip >= ipEnd) {
                return op;
            }

            t = GET_U8(src, ip);
            ip++;
            goto matchHandling;
        }

        if (ip >= ipEnd) {
            return op;
        }

        if (ip + 3 > ipEnd) {
            throw LZOInputOverrun();
        }

        t = GET_U8(src, ip);
        ip++;
        continue;
    }

    #undef GET_U8
    #undef SET_U8
}

// copyMatchUltraFast uses direct pointer access and optimized copying for maximum speed
int copyMatchUltraFast(uint8_t* dst, int op, int mPos, int length) {
    // For very small lengths, use unrolled byte copies
    if (length <= 8) {
        // Check for overlapping copy (pattern repeat)
        if (mPos + length > op) {
            // Overlapping - must copy byte by byte
            for (int i = 0; i < length; i++) {
                dst[op + i] = dst[mPos + i];
            }
            return op + length;
        }

        // Non-overlapping - unrolled copy using larger types where possible
        uint8_t* dstPtr = dst + op;
        uint8_t* srcPtr = dst + mPos;

        switch (length) {
            case 1:
                *dstPtr = *srcPtr;
                break;
            case 2:
                *reinterpret_cast<uint16_t*>(dstPtr) = *reinterpret_cast<uint16_t*>(srcPtr);
                break;
            case 3:
                *reinterpret_cast<uint16_t*>(dstPtr) = *reinterpret_cast<uint16_t*>(srcPtr);
                dstPtr[2] = srcPtr[2];
                break;
            case 4:
                *reinterpret_cast<uint32_t*>(dstPtr) = *reinterpret_cast<uint32_t*>(srcPtr);
                break;
            case 5:
                *reinterpret_cast<uint32_t*>(dstPtr) = *reinterpret_cast<uint32_t*>(srcPtr);
                dstPtr[4] = srcPtr[4];
                break;
            case 6:
                *reinterpret_cast<uint32_t*>(dstPtr) = *reinterpret_cast<uint32_t*>(srcPtr);
                *reinterpret_cast<uint16_t*>(dstPtr + 4) = *reinterpret_cast<uint16_t*>(srcPtr + 4);
                break;
            case 7:
                *reinterpret_cast<uint32_t*>(dstPtr) = *reinterpret_cast<uint32_t*>(srcPtr);
                *reinterpret_cast<uint16_t*>(dstPtr + 4) = *reinterpret_cast<uint16_t*>(srcPtr + 4);
                dstPtr[6] = srcPtr[6];
                break;
            case 8:
                *reinterpret_cast<uint64_t*>(dstPtr) = *reinterpret_cast<uint64_t*>(srcPtr);
                break;
        }
        return op + length;
    }

    // For larger lengths, check overlap
    if (mPos + length > op) {
        // Overlapping copy - byte by byte
        for (int i = 0; i < length; i++) {
            dst[op + i] = dst[mPos + i];
        }
        return op + length;
    }

    // Non-overlapping large copy - use 8-byte chunks
    uint8_t* dstPtr = dst + op;
    uint8_t* srcPtr = dst + mPos;

    // Copy 8 bytes at a time for maximum throughput
    while (length >= 8) {
        *reinterpret_cast<uint64_t*>(dstPtr) = *reinterpret_cast<uint64_t*>(srcPtr);
        dstPtr += 8;
        srcPtr += 8;
        length -= 8;
    }

    // Handle remainder (0-7 bytes)
    if (length >= 4) {
        *reinterpret_cast<uint32_t*>(dstPtr) = *reinterpret_cast<uint32_t*>(srcPtr);
        dstPtr += 4;
        srcPtr += 4;
        length -= 4;
    }
    if (length >= 2) {
        *reinterpret_cast<uint16_t*>(dstPtr) = *reinterpret_cast<uint16_t*>(srcPtr);
        dstPtr += 2;
        srcPtr += 2;
        length -= 2;
    }
    if (length == 1) {
        *dstPtr = *srcPtr;
    }

    return op + (dstPtr - (dst + op));
}
