#ifndef NSE_PROTOCOL_H
#define NSE_PROTOCOL_H

#include <cstdint>

#pragma pack(push, 1)

struct Stream_Header {
    int16_t message_length;
    int16_t stream_id;
    int64_t sequence_number;
};

struct Order_Message {
    Stream_Header stream_header;
    char message_type; // 'N', 'M', 'X'
    int64_t timestamp;
    double order_id; // Using double as per reference parser
    int32_t token;
    char order_type; // 'B', 'S'
    int32_t price;
    int32_t quantity;
};

struct Trade_Message {
    Stream_Header stream_header;
    char message_type; // 'T'
    int64_t timestamp;
    double buy_order_id;
    double sell_order_id;
    int32_t token;
    int32_t trade_price;
    int32_t trade_quantity;
};

// Generic Message for type checking
struct Generic_Message {
    Stream_Header stream_header;
    char message_type;
};

#pragma pack(pop)

#endif // NSE_PROTOCOL_H
