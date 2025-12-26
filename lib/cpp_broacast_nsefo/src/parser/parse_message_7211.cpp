#include "nse_parsers.h"
#include "protocol.h"
#include <iostream>

void parse_message_7211(const MS_SPD_MKT_INFO* msg) {
    int32_t token1 = be32toh_func(msg->token1);
    int32_t token2 = be32toh_func(msg->token2);
    uint32_t volume = be32toh_func(msg->tradedVolume);
    
    // std::cout << "  [SPD_MBP_DELTA] Token1: " << token1
    //           << " | Token2: " << token2
    //           << " | Volume: " << volume
    //           << " | Value: " << msg->totalTradedValue << std::endl;
    
    // std::cout << "    Buys: " << be16toh_func(msg->mbpBuy) 
    //           << " | Sells: " << be16toh_func(msg->mbpSell) << std::endl;
}

void parse_spd_mbp_delta(const MS_SPD_MKT_INFO* msg) {
    parse_message_7211(msg);
}
