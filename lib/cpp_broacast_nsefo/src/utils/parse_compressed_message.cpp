#include "utils/parse_compressed_message.h"
#include "lzo_decompress.h"
#include "protocol.h"
#include "constants.h"
#include "nse_parsers.h"
#include <iostream>
#include <iomanip>
#include <vector>

void parse_compressed_message(const char* data, int16_t length) {
    // Statistics tracking
    static int total_messages = 0;
    static int successful_decompressions = 0;
    static int failed_decompressions = 0;
    static int lookbehind_errors = 0;
    static int other_errors = 0;
    
    total_messages++;
    
    /*
    if (msg_count <= 5) {
        std::cout << "\n=== Compressed Message #" << msg_count << " ===" << std::endl;
        std::cout << "Input length: " << length << " bytes" << std::endl;
        std::cout << "First 32 bytes of compressed data:" << std::endl;
        for (int i = 0; i < std::min((int)length, 32); i++) {
            printf("%02X ", (unsigned char)data[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        std::cout << std::endl;
    }
    */
    
    // Decompress using official LZO library
    std::vector<uint8_t> input(data, data + length);
    std::vector<uint8_t> output(65535);  // Large output buffer for decompressed data
    
    int result;
    try {
        result = LzoDecompressor::decompressWithLibrary(input, output);
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
    
    /*
    // Show hex dump for first 5 packets
    static int hex_count = 0;
    hex_count++;
    if (hex_count <= 5) {
        std::cout << "\n========== Decompressed Packet #" << hex_count << " (" << result << " bytes) ==========" << std::endl;
        std::cout << "BEFORE skipping 8 bytes - First 64 bytes:" << std::endl;
        for (int i = 0; i < std::min(result, 64); i++) {
            printf("%02X ", output[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        std::cout << std::endl;
    }
    */
    
    // Resize output to actual decompressed size
    output.resize(result);

    // Skip first 8 bytes of the decompressed data
    // Original offset 10 (TxCode) becomes offset 2 after this skip
    output.erase(output.begin(), output.begin() + 8);

    // std::cout << "  [Decompressed] " << output.size() << " bytes" << std::endl;
    
    if (output.size() < 20) {
        std::cout << "  [Error] Decompressed data too small" << std::endl;
        return;
    }
    
    // Extract Transaction Code from offset 10 (even after skipping 8 bytes)
    // Looking at hex: offset 10-11 contains the TxCode (e.g., 0x1C28 = 7208)
    uint16_t txCode = be16toh_func(*((uint16_t*)(output.data() + 10)));
    
    // Debug: Show transaction code for first few messages
    static int debug_count = 0;
    debug_count++;
    if (debug_count <= 0) {
        std::cout << "\nAFTER skipping 8 bytes - TxCode at offset 10: " << txCode << " (" << getTxCodeName(txCode) << ")" << std::endl;
        std::cout << "First 64 bytes AFTER skip:" << std::endl;
        for (int i = 0; i < std::min((int)output.size(), 64); i++) {
            printf("%02X ", output[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        std::cout << std::endl;
    }
    
    // Debug: Show raw bytes at key offsets for first message
    static int struct_debug = 0;
    struct_debug++;
    if (struct_debug <= 0 && txCode == 7208) {
        std::cout << "\n=== Structure Debug for BCAST_ONLY_MBP ===" << std::endl;
        std::cout << "Bytes at offset 10-11 (TxCode): " << std::hex 
                  << (int)output[10] << " " << (int)output[11] << std::dec 
                  << " = " << be16toh_func(*((uint16_t*)(output.data() + 10))) << std::endl;
        std::cout << "Bytes at offset 40-41 (NoOfRecords): " << std::hex 
                  << (int)output[40] << " " << (int)output[41] << std::dec 
                  << " = " << be16toh_func(*((uint16_t*)(output.data() + 40))) << std::endl;
        std::cout << "Bytes at offset 42-45 (First Token): ";
        for (int i = 42; i < 46; i++) printf("%02X ", output[i]);
        std::cout << " = " << be32toh_func(*((uint32_t*)(output.data() + 42))) << std::endl;
        std::cout << std::endl;
    }
    // std::cout << "  [TxCode] " << txCode << " (" << getTxCodeName(txCode) << ")" << std::endl;
    
    // Message data starts at offset 0 (the entire 470-byte buffer IS the message structure)
    // The transaction code at offset 10-11 is PART of the message structure (inside BCAST_HEADER)
    const uint8_t* message_data = output.data();
    
    // Debug: Show pointer and actual bytes for first BCAST_ONLY_MBP message
    static int ptr_debug = 0;
    if (txCode == 7208 && ptr_debug < 1) {
        ptr_debug++;
        std::cout << "\n=== Pointer Debug ===" << std::endl;
        std::cout << "output.size(): " << output.size() << std::endl;
        std::cout << "message_data pointer: " << (void*)message_data << std::endl;
        std::cout << "Bytes at message_data[40-41]: " << std::hex 
                  << (int)message_data[40] << " " << (int)message_data[41] << std::dec << std::endl;
        std::cout << "Bytes at message_data[42-45]: ";
        for (int i = 42; i < 46; i++) printf("%02X ", message_data[i]);
        std::cout << std::endl;
        
        // Cast and show what structure reads
        const MS_BCAST_ONLY_MBP* test_msg = reinterpret_cast<const MS_BCAST_ONLY_MBP*>(message_data);
        std::cout << "Structure reads noOfRecords as: 0x" << std::hex << test_msg->noOfRecords << std::dec << std::endl;
        std::cout << "Structure reads data[0].token as: 0x" << std::hex << test_msg->data[0].token << std::dec << std::endl;
        std::cout << std::endl;
    }
    
    switch (txCode) {
        case TxCodes::BCAST_MBO_MBP_UPDATE:
            std::cout << "  [Received] " << txCode << " (BCAST_MBO_MBP_UPDATE)" << std::endl;
            if (output.size() >= sizeof(MS_BCAST_MBO_MBP)) {
                parse_bcast_mbo_mbp(reinterpret_cast<const MS_BCAST_MBO_MBP*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_ONLY_MBP:
            // Suppressed as requested
            if (output.size() >= sizeof(MS_BCAST_ONLY_MBP)) {
                parse_bcast_only_mbp(reinterpret_cast<const MS_BCAST_ONLY_MBP*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_TICKER_AND_MKT_INDEX:
            std::cout << "  [Received] " << txCode << " (BCAST_TICKER_AND_MKT_INDEX)" << std::endl;
            if (output.size() >= sizeof(MS_TICKER_TRADE_DATA)) {
                parse_ticker_trade_data(reinterpret_cast<const MS_TICKER_TRADE_DATA*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_ENHNCD_TICKER_AND_MKT_INDEX:
            std::cout << "  [Received] " << txCode << " (BCAST_ENHNCD_TICKER_AND_MKT_INDEX)" << std::endl;
            if (output.size() >= sizeof(MS_ENHNCD_TICKER_TRADE_DATA)) {
                parse_enhncd_ticker_trade_data(reinterpret_cast<const MS_ENHNCD_TICKER_TRADE_DATA*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_MW_ROUND_ROBIN:
            std::cout << "  [Received] " << txCode << " (BCAST_MW_ROUND_ROBIN)" << std::endl;
            if (output.size() >= sizeof(MS_BCAST_INQ_RESP_2)) {
                parse_market_watch(reinterpret_cast<const MS_BCAST_INQ_RESP_2*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_ENHNCD_MW_ROUND_ROBIN:
            std::cout << "  [Received] " << txCode << " (BCAST_ENHNCD_MW_ROUND_ROBIN)" << std::endl;
            if (output.size() >= sizeof(MS_ENHNCD_BCAST_INQ_RESP_2)) {
                parse_enhncd_market_watch(reinterpret_cast<const MS_ENHNCD_BCAST_INQ_RESP_2*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_SPD_MBP_DELTA:
            std::cout << "  [Received] " << txCode << " (BCAST_SPD_MBP_DELTA)" << std::endl;
            if (output.size() >= sizeof(MS_SPD_MKT_INFO)) {
                parse_spd_mbp_delta(reinterpret_cast<const MS_SPD_MKT_INFO*>(message_data));
            }
            break;
            
        case TxCodes::BCAST_LIMIT_PRICE_PROTECTION_RANGE:
            std::cout << "  [Received] " << txCode << " (BCAST_LIMIT_PRICE_PROTECTION_RANGE)" << std::endl;
            if (output.size() >= sizeof(MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE)) {
                parse_limit_price_protection(reinterpret_cast<const MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE*>(message_data));
            }
            break;
            
        default:
            std::cout << "  [Unknown TxCode] " << txCode << " (" << getTxCodeName(txCode) << ")" << std::endl;
            break;
    }
}
