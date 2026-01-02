#include "utils/parse_compressed_message.h"
#include "udp_receiver.h"  // For UDPStats
#include "lzo_decompress.h"
#include "protocol.h"
#include "constants.h"
#include "nse_parsers.h"
#include <iostream>
#include <iomanip>
#include <vector>

namespace nsefo {

void parse_compressed_message(const char* data, int16_t length, UDPStats& stats) {
    // Statistics tracking
    static int total_messages = 0;
    static int successful_decompressions = 0;
    static int failed_decompressions = 0;
    static int lookbehind_errors = 0;
    static int other_errors = 0;
    
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
        std::string error_msg = e.what();
        if (error_msg.find("lookbehind") != std::string::npos) {
            lookbehind_errors++;
        } else {
            other_errors++;
        }
        
        // Show first 10 errors for debugging
        static int error_count = 0;
        error_count++;
        if (error_count <= 10) {
            std::cout << "\n[Decompression Error #" << error_count << "] " << error_msg << std::endl;
            std::cout << "Input length: " << length << " bytes" << std::endl;
            std::cout << "First 16 bytes: ";
            for (int i = 0; i < std::min((int)length, 16); i++) {
                printf("%02X ", (unsigned char)data[i]);
            }
            std::cout << std::endl;
        }
        
        // Print statistics every 100 messages
        if (total_messages % 100 == 0) {
            double success_rate = (successful_decompressions * 100.0) / total_messages;
            double lookbehind_rate = (lookbehind_errors * 100.0) / total_messages;
            std::cout << "\n=== Decompression Statistics (after " << total_messages << " messages) ===" << std::endl;
            std::cout << "Success: " << successful_decompressions << " (" << std::fixed << std::setprecision(2) << success_rate << "%)" << std::endl;
            std::cout << "Failed: " << failed_decompressions << " (" << std::fixed << std::setprecision(2) << (100.0 - success_rate) << "%)" << std::endl;
            std::cout << "  - Lookbehind errors: " << lookbehind_errors << " (" << std::fixed << std::setprecision(2) << lookbehind_rate << "%)" << std::endl;
            std::cout << "  - Other errors: " << other_errors << std::endl;
            std::cout << std::endl;
        }
        return;
    }
    
    if (result <= 0) {
        // std::cout << "  [Error] Decompression failed with result: " << result << std::endl;
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

    if (txCode != 7208 and txCode != 17202 and txCode != 7220 and txCode != 7211)
    {
    
    // std::cout << "  [Decompressed] TxCode at offset 10: " << txCode << std::endl;

    }
    // UPDATE STATISTICS: Track this message by transaction code
    // compressedSize = input length, rawSize = decompressed output size
    stats.update(txCode, length, output.size(), false);
    
    switch (txCode) {
        case TxCodes::BCAST_MBO_MBP_UPDATE:
            // std::cout << "  [Received] " << txCode << " (BCAST_MBO_MBP_UPDATE)" << std::endl;
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
            // std::cout << "  [Received] " << txCode << " (BCAST_TICKER_AND_MKT_INDEX)" << std::endl;
            if (message_size >= sizeof(MS_TICKER_TRADE_DATA)) {
                parse_ticker_trade_data(reinterpret_cast<const MS_TICKER_TRADE_DATA*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_ENHNCD_TICKER_AND_MKT_INDEX:
            if (message_size >= sizeof(MS_ENHNCD_TICKER_TRADE_DATA)) {
                parse_enhncd_ticker_trade_data(reinterpret_cast<const MS_ENHNCD_TICKER_TRADE_DATA*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_MW_ROUND_ROBIN:
            // std::cout << "  [Received] " << txCode << " (BCAST_MW_ROUND_ROBIN)" << std::endl;
            if (message_size >= sizeof(MS_BCAST_INQ_RESP_2)) {
                parse_market_watch(reinterpret_cast<const MS_BCAST_INQ_RESP_2*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_ENHNCD_MW_ROUND_ROBIN:
            if (message_size >= sizeof(MS_ENHNCD_BCAST_INQ_RESP_2)) {
                parse_enhncd_market_watch(reinterpret_cast<const MS_ENHNCD_BCAST_INQ_RESP_2*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_SPD_MBP_DELTA:
            if (message_size >= sizeof(MS_SPD_MKT_INFO)) {
                parse_spd_mbp_delta(reinterpret_cast<const MS_SPD_MKT_INFO*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_LIMIT_PRICE_PROTECTION_RANGE:
            if (message_size >= sizeof(MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE)) {
                parse_limit_price_protection(reinterpret_cast<const MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_INDICES:
            // std::cout << "  [Received] " << txCode << " (BCAST_INDICES)" << std::endl;
            if (message_size >= sizeof(MS_BCAST_INDICES)) {
                parse_bcast_indices(reinterpret_cast<const MS_BCAST_INDICES*>(message_data));
            }
            break;
            
        default:
            break;
    }
}

} // namespace nsefo
