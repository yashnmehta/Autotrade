#ifndef PACKET_H
#define PACKET_H

#include <cstdint>

namespace nsecm {

#pragma pack(push, 1)

struct MessageData {
    int16_t iCompLen;
    char data[0]; // Flexible array member
};

struct Packet {
    char cNetID[2];
    int16_t iNoOfMsgs;
    // The structure actually continues with variable length data
    // We can use a pointer or just access the buffer
    char cPackData[0];
};

#pragma pack(pop)

} // namespace nsecm

#endif // PACKET_H
