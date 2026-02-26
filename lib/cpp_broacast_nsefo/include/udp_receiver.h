#ifndef UDP_RECEIVER_H
#define UDP_RECEIVER_H

#include <string>
#include <atomic>
#include <map>
#include <chrono>
#include <iostream>

namespace nsefo {

struct MessageStats {
    uint16_t transactionCode;
    uint64_t count;
    uint64_t totalCompressedSize;
    uint64_t totalRawSize;
};

class UDPStats {
public:
    std::map<uint16_t, MessageStats> messageStats;
    uint64_t totalPackets{0};
    uint64_t totalBytes{0};
    uint64_t compressedPackets{0};
    uint64_t decompressedPackets{0};
    uint64_t decompressionFailures{0};
    // Sequence gap counters – incremented when a gap is detected in bcSeqNo
    uint64_t sequenceGaps{0};
    uint64_t droppedMessages{0}; // Estimated messages lost in gaps
    std::chrono::steady_clock::time_point startTime;

    UDPStats();
    void update(uint16_t code, int compressedSize, int rawSize, bool error);
    void recordPacket();  // Record a packet without detailed stats
    void recordSequenceGap(uint32_t expected, uint32_t actual); // Record gap
    void print();

    // Output operator for easy printing
    friend std::ostream& operator<<(std::ostream& os, const UDPStats& stats);
};

class UDPReceiver {
public:
    // DEPRECATED: Use MulticastReceiver instead.
    // This method has no shutdown mechanism, uses a hardcoded multicast IP,
    // and does not perform actual message parsing.
    [[deprecated("Use MulticastReceiver – startListener has no stop() support and hardcodes the multicast IP")]]
    static void startListener(int port, UDPStats& stats);
};

} // namespace nsefo

#endif // UDP_RECEIVER_H
