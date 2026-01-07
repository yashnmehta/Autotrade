#include "../../include/utils/parse_compressed_message.h"
#include "../../include/nsecm_udp_receiver.h"  // For UDPStats
#include "lzo_decompress.h"  // Common LZO decompressor
#include "../../include/protocol.h"
#include "../../include/constants.h"
#include "../../include/nse_parsers.h"
#include <iostream>
#include <iomanip>
#include <vector>

namespace nsecm {

void parse_compressed_message(const char* data, int16_t length, UDPStats& stats) {
    // Statistics tracking
    static int total_messages = 0;
    static int successful_decompressions = 0;
    static int failed_decompressions = 0;
    
    total_messages++;
    
    // Decompress using official LZO library
    std::vector<uint8_t> input(data, data + length);
    std::vector<uint8_t> output(65535);  // Large output buffer for decompressed data
    
    int result;
    try {
        result = common::LzoDecompressor::decompressWithLibrary(input, output);
        successful_decompressions++;
    } catch (const std::exception& e) {
        failed_decompressions++;
        return;
    }
    
    if (result <= 0) {
        return;
    }
    
    // Resize output to actual decompressed size
    output.resize(result);

    // Skip first 8 bytes of the decompressed data (magic offset)
    size_t header_offset = CommonConfig::COMPRESSED_HEADER_OFFSET;
    
    if (result < header_offset + sizeof(BCAST_HEADER)) { 
        return;
    }

    // Pointer to start of BCAST_HEADER
    const uint8_t* message_data = output.data() + header_offset;
    size_t message_size = result - header_offset;
    
    // Extract Transaction Code from offset 10 of the BCAST_HEADER
    uint16_t txCode = be16toh_func(*((uint16_t*)(message_data + CommonConfig::BCAST_HEADER_TXCODE_OFFSET)));

    // UPDATE STATISTICS: Track this message by transaction code
    stats.update(txCode, length, output.size(), false);
    
    switch (txCode) {
        case TxCodes::BCAST_MBO_MBP_UPDATE:
            if (message_size >= sizeof(MS_BCAST_MBO_MBP)) {
                parse_bcast_mbo_mbp(reinterpret_cast<const MS_BCAST_MBO_MBP*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_ONLY_MBP:
            if (message_size >= sizeof(MS_BCAST_ONLY_MBP)) {
                parse_bcast_only_mbp(reinterpret_cast<const MS_BCAST_ONLY_MBP*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_TICKER_AND_MKT_INDEX:
            if (message_size >= sizeof(MS_TICKER_TRADE_DATA)) {
                parse_ticker_trade_data(reinterpret_cast<const MS_TICKER_TRADE_DATA*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_BUY_BACK:
            if (message_size >= sizeof(MS_BCAST_BUY_BACK)) {
                parse_bcast_buy_back(reinterpret_cast<const MS_BCAST_BUY_BACK*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_MW_ROUND_ROBIN:
            if (message_size >= sizeof(MS_BCAST_INQ_RESP_2)) {
                parse_market_watch(reinterpret_cast<const MS_BCAST_INQ_RESP_2*>(message_data));
            }
            break;

        case TxCodes::BCAST_INDICES:
            if (message_size >= sizeof(MS_BCAST_INDICES)) {
                parse_bcast_indices(reinterpret_cast<const MS_BCAST_INDICES*>(message_data));
            }
            break;

        case TxCodes::BCAST_IND_INDICES:
            if (message_size >= sizeof(MS_BCAST_INDUSTRY_INDICES)) {
                parse_bcast_industry_indices(reinterpret_cast<const MS_BCAST_INDUSTRY_INDICES*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_SYSTEM_INFORMATION_OUT:
            if (message_size >= sizeof(MS_BCAST_SYSTEM_INFORMATION)) {
                parse_system_information(reinterpret_cast<const MS_BCAST_SYSTEM_INFORMATION*>(message_data));
            } else {
                std::cerr << "[7206] Message too small: " << message_size 
                          << " (expected " << sizeof(MS_BCAST_SYSTEM_INFORMATION) << ")" 
                          << std::endl;
            }
            break;
            
        case TxCodes::BCAST_SECURITY_STATUS_CHG_PREOPEN:
            if (message_size >= sizeof(MS_BCAST_CALL_AUCTION_ORD_CXL)) {
                parse_call_auction_order_cxl(reinterpret_cast<const MS_BCAST_CALL_AUCTION_ORD_CXL*>(message_data));
            } else {
                std::cerr << "[7210] Message too small: " << message_size 
                          << " (expected " << sizeof(MS_BCAST_CALL_AUCTION_ORD_CXL) << ")" 
                          << std::endl;
            }
            break;
            
        default:
            break;
    }
}

} // namespace nsecm
