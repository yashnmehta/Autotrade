#include "lzo_decompress.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

// LZO1Z Constants
const int M2_MAX_OFFSET = 0x0700;

int LzoDecompressor::decompress(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst) {
    if (src.empty()) {
        throw std::runtime_error("LZO: Empty input");
    }
    
    if (dst.empty()) {
        throw std::runtime_error("LZO: Output buffer not allocated");
    }

    size_t ip = 0;
    size_t op = 0;
    size_t ipEnd = src.size();
    size_t opEnd = dst.size();
    
    size_t t = 0;
    size_t mPos = 0;
    size_t mLen = 0;
    size_t off = 0;
    size_t lastMOff = 0;

    const uint8_t* srcPtr = src.data();
    uint8_t* dstPtr = dst.data();

    // Helper macros
    #define CHECK_IP(x) if (ip + (x) > ipEnd) throw std::runtime_error("LZO input overrun")
    #define CHECK_OP(x) if (op + (x) > opEnd) throw std::runtime_error("LZO output overrun")
    
    // Read first instruction
    CHECK_IP(1);
    t = srcPtr[ip++];

    // Handle initial literal run
    if (t > 17) {
        t = t - 17;
        if (t < 4) {
            CHECK_OP(t);
            CHECK_IP(t);
            for (size_t i = 0; i < t; i++) {
                dstPtr[op + i] = srcPtr[ip + i];
            }
            op += t;
            ip += t;
            
            if (ip >= ipEnd) return op;
            
            CHECK_IP(1);
            t = srcPtr[ip++];
        } else {
            CHECK_OP(t);
            CHECK_IP(t);
            memcpy(dstPtr + op, srcPtr + ip, t);
            op += t;
            ip += t;
            
            if (ip >= ipEnd) return op;
            
            CHECK_IP(1);
            t = srcPtr[ip++];
            
            if (t < 16) {
                CHECK_IP(1);
                off = (1 + M2_MAX_OFFSET) + (t << 6) + (srcPtr[ip] >> 2);
                ip++;

                if (off > op) throw std::runtime_error("LZO corrupted data (lookbehind)");
                mPos = op - off;
                lastMOff = off;
                
                CHECK_OP(3);
                dstPtr[op] = dstPtr[mPos];
                dstPtr[op+1] = dstPtr[mPos+1];
                dstPtr[op+2] = dstPtr[mPos+2];
                op += 3;
                
                if (ip >= ipEnd) return op;
                
                t = srcPtr[ip-1] & 3;
                
                if (t != 0) {
                    CHECK_OP(t);
                    CHECK_IP(t);
                    for (size_t i = 0; i < t; i++) {
                        dstPtr[op + i] = srcPtr[ip + i];
                    }
                    op += t;
                    ip += t;
                    
                    if (ip >= ipEnd) return op;
                    
                    CHECK_IP(1);
                    t = srcPtr[ip++];
                } else {
                    CHECK_IP(1);
                    t = srcPtr[ip++];
                }
            }
        }
    }

    // Main loop
    for (;;) {
        if (t >= 16) goto match_handling;
        
        // Literal run
        if (t == 0) {
            CHECK_IP(1);
            while (srcPtr[ip] == 0) {
                t += 255;
                ip++;
                CHECK_IP(1);
            }
            t += 15 + srcPtr[ip++];
        }
        
        t += 3;
        CHECK_OP(t);
        CHECK_IP(t);
        memcpy(dstPtr + op, srcPtr + ip, t);
        op += t;
        ip += t;
        
        if (ip >= ipEnd) return op;
        
        CHECK_IP(1);
        t = srcPtr[ip++];
        
        if (t >= 16) goto match_handling;
        
        // M1 match
        CHECK_IP(1);
        off = 1 + (t << 6) + (srcPtr[ip] >> 2);
        ip++;
        
        if (off > op) throw std::runtime_error("LZO corrupted data (lookbehind)");
        mPos = op - off;
        lastMOff = off;
        
        CHECK_OP(2);
        dstPtr[op] = dstPtr[mPos];
        dstPtr[op+1] = dstPtr[mPos+1];
        op += 2;
        
        goto match_done;
        
    match_handling:
        if (t >= 64) {
            // M2 match
            off = t & 0x1f;
            if (off >= 0x1c) {
                if (lastMOff == 0) throw std::runtime_error("LZO corrupted data");
                off = lastMOff;
                mPos = op - off;
            } else {
                CHECK_IP(1);
                off = 1 + (off << 6) + (srcPtr[ip] >> 2);
                ip++;
                if (off > op) throw std::runtime_error("LZO corrupted data (lookbehind)");
                mPos = op - off;
                lastMOff = off;
            }
            mLen = (t >> 5) - 1;
        } else if (t >= 32) {
            // M3 match
            t &= 31;
            if (t == 0) {
                CHECK_IP(1);
                while (srcPtr[ip] == 0) {
                    t += 255;
                    ip++;
                    CHECK_IP(1);
                }
                t += 31 + srcPtr[ip++];
            }
            
            CHECK_IP(2);
            off = 1 + (srcPtr[ip] << 6) + (srcPtr[ip+1] >> 2);
            ip += 2;
            
            if (off > op) throw std::runtime_error("LZO corrupted data (lookbehind)");
            mPos = op - off;
            lastMOff = off;
            mLen = t;
        } else if (t >= 16) {
            // M4 match
            mPos = op;
            mPos -= (t & 8) << 11;
            
            t &= 7;
            if (t == 0) {
                CHECK_IP(1);
                while (srcPtr[ip] == 0) {
                    t += 255;
                    ip++;
                    CHECK_IP(1);
                }
                t += 7 + srcPtr[ip++];
            }
            
            CHECK_IP(2);
            mPos -= (srcPtr[ip] << 6) + (srcPtr[ip+1] >> 2);
            ip += 2;
            
            if (mPos == op) return op; // EOF marker
            
            mPos -= 0x4000;
            if (op < mPos) throw std::runtime_error("LZO corrupted data (lookbehind M4)");
            lastMOff = op - mPos;
            if (lastMOff == 0 || lastMOff > op) throw std::runtime_error("LZO corrupted data (invalid offset M4)");
            
            mLen = t;
        } else {
            // M1 match (inner)
            CHECK_IP(1);
            off = 1 + (t << 6) + (srcPtr[ip] >> 2);
            ip++;
            
            if (off > op) throw std::runtime_error("LZO corrupted data (lookbehind)");
            mPos = op - off;
            lastMOff = off;
            
            CHECK_OP(2);
            dstPtr[op] = dstPtr[mPos];
            dstPtr[op+1] = dstPtr[mPos+1];
            op += 2;
            
            goto match_done;
        }
        
        // Copy match
        {
            size_t totalLen = 2 + mLen;
            CHECK_OP(totalLen);
            
            // Byte-by-byte copy to handle overlapping regions
            for (size_t i = 0; i < totalLen; i++) {
                dstPtr[op + i] = dstPtr[mPos + i];
            }
            op += totalLen;
        }
        
    match_done:
        t = srcPtr[ip-1] & 3;
        
        if (ip >= ipEnd) return op;
        
        if (t != 0) {
            CHECK_OP(t);
            CHECK_IP(t);
            for (size_t i = 0; i < t; i++) {
                dstPtr[op + i] = srcPtr[ip + i];
            }
            op += t;
            ip += t;
            
            if (ip >= ipEnd) return op;
            
            CHECK_IP(1);
            t = srcPtr[ip++];
            goto match_handling;
        }
        
        if (ip >= ipEnd) return op;
        
        CHECK_IP(1);
        t = srcPtr[ip++];
    }
    
    #undef CHECK_IP
    #undef CHECK_OP
}
