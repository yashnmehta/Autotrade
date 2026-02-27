#include "utils/parse_compressed_message.h"
#include "udp_receiver.h"  // For UDPStats
#include "lzo_decompress.h"
#include "protocol.h"
#include "constants.h"
#include "nse_parsers.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <atomic>

namespace nsefo {

void parse_compressed_message(const char* data, int16_t length, UDPStats& stats) {
    // Decompression error counters – use atomics so this function is safe
    // even if called from multiple threads in a future multi-feed scenario.
    static std::atomic<int> total_messages{0};
    static std::atomic<int> successful_decompressions{0};
    static std::atomic<int> failed_decompressions{0};
    static std::atomic<int> lookbehind_errors{0};
    static std::atomic<int> other_errors{0};

    // Per-feed sequence tracking to detect dropped messages.
    // Key: bcSeqNo from BCAST_HEADER (offset 14 within the header).
    // We track the last seen sequence number and flag gaps.
    static std::atomic<uint32_t> lastSeqNo{0};
    static std::atomic<bool> seqNoInitialized{false};

    int msgCount = ++total_messages;
    
    // Decompress using official LZO library
    std::vector<uint8_t> input(data, data + length);
    std::vector<uint8_t> output(65535);  // Large output buffer for decompressed data
    
    int result;
    try {
        result = common::LzoDecompressor::decompressWithLibrary(input, output);
        ++successful_decompressions;
    } catch (const std::exception& e) {
        ++failed_decompressions;
        std::string error_msg = e.what();
        if (error_msg.find("lookbehind") != std::string::npos) {
            ++lookbehind_errors;
        } else {
            ++other_errors;
        }

        // Show first 10 errors for debugging
        static std::atomic<int> error_count{0};
        int ec = ++error_count;
        if (ec <= 10) {
            std::cout << "\n[Decompression Error #" << ec << "] " << error_msg << std::endl;
            std::cout << "Input length: " << length << " bytes" << std::endl;
            std::cout << "First 16 bytes: ";
            for (int i = 0; i < std::min((int)length, 16); i++) {
                printf("%02X ", (unsigned char)data[i]);
            }
            std::cout << std::endl;
        }

        // Print statistics every 100 messages
        if (msgCount % 1000 == 0) {
            int succ = successful_decompressions.load();
            int fail = failed_decompressions.load();
            int lb   = lookbehind_errors.load();
            double success_rate = (succ * 100.0) / msgCount;
            double lookbehind_rate = (lb * 100.0) / msgCount;
            std::cout << "\n=== Decompression Statistics (after " << msgCount << " messages) ===" << std::endl;
            std::cout << "Success: " << succ << " (" << std::fixed << std::setprecision(2) << success_rate << "%)" << std::endl;
            std::cout << "Failed: "  << fail << " (" << std::fixed << std::setprecision(2) << (100.0 - success_rate) << "%)" << std::endl;
            std::cout << "  - Lookbehind errors: " << lb << " (" << std::fixed << std::setprecision(2) << lookbehind_rate << "%)" << std::endl;
            std::cout << "  - Other errors: " << other_errors.load() << std::endl;
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

    // -------------------------------------------------------
    // DISABLED: Sequence gap detection via bcSeqNo (offset 14 of BCAST_HEADER).
    // 
    // NSE FO multicast sends MULTIPLE INDEPENDENT STREAMS (different instruments,
    // transaction codes, segments) each with their own sequence counter. Tracking
    // a single global lastSeqNo produces billions of false "dropped packets".
    // 
    // Example: Packet from NIFTY stream (seqNo=7146537) followed by packet from
    // BANKNIFTY stream (seqNo=1008990) falsely reports 6M+ dropped packets.
    // 
    // Proper fix would require tracking per-stream sequences (alphaChar+transCode
    // combinations), which adds complexity with minimal benefit for application use.
    // 
    // For now, gap detection is DISABLED. Market data processing is unaffected.
    // -------------------------------------------------------
    #if 0  // Disabled - see explanation above
    {
        // BCAST_HEADER layout: reserved1(2)+reserved2(2)+logTime(4)+alphaChar(2)+transCode(2)+errorCode(2)+bcSeqNo(4)
        // Offset of bcSeqNo = 2+2+4+2+2+2 = 14
        constexpr size_t BCAST_HEADER_SEQNO_OFFSET = 14;
        if (message_size >= BCAST_HEADER_SEQNO_OFFSET + sizeof(uint32_t)) {
            uint32_t currentSeqNo = be32toh_func(
                *reinterpret_cast<const uint32_t*>(message_data + BCAST_HEADER_SEQNO_OFFSET));

            if (!seqNoInitialized.exchange(true)) {
                lastSeqNo.store(currentSeqNo);
            } else {
                uint32_t expected = lastSeqNo.load() + 1;
                // Allow wrapping (uint32 overflow)
                if (currentSeqNo != expected && currentSeqNo != lastSeqNo.load()) {
                    stats.recordSequenceGap(expected, currentSeqNo);
                }
                lastSeqNo.store(currentSeqNo);
            }
        }
    }
    #endif

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
            
        case TxCodes::BCAST_INDUSTRY_INDEX_UPDATE:
            // std::cout << "  [Received] " << txCode << " (BCAST_INDUSTRY_INDEX_UPDATE)" << std::endl;
            if (message_size >= sizeof(MS_BCAST_INDUSTRY_INDICES)) {
                parse_bcast_industry_indices(reinterpret_cast<const MS_BCAST_INDUSTRY_INDICES*>(message_data));
            }
            break;
            
        default:
            break;
    }
}

} // namespace nsefo
