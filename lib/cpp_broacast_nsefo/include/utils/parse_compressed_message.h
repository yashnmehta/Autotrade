#ifndef PARSE_COMPRESSED_MESSAGE_H
#define PARSE_COMPRESSED_MESSAGE_H

#include <cstdint>

namespace nsefo {

// Forward declaration
class UDPStats;

void parse_compressed_message(const char* data, int16_t length, UDPStats& stats);

} // namespace nsefo

#endif
