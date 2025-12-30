#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstdint>
#include <map>
#include <string>

// Common Protocol Configuration
namespace CommonConfig {
    constexpr size_t COMPRESSED_HEADER_OFFSET = 8;
    constexpr size_t BCAST_HEADER_TXCODE_OFFSET = 10;
}

// Broadcast Transaction Codes
namespace TxCodes {
    constexpr uint16_t BCAST_MBO_MBP_UPDATE = 7200;
    constexpr uint16_t BCAST_MW_ROUND_ROBIN = 7201;
    constexpr uint16_t BCAST_IND_INDICES = 7203; // CM Industry Indices
    constexpr uint16_t BCAST_SYSTEM_INFORMATION_OUT = 7206;
    constexpr uint16_t BCAST_INDICES = 7207;
    constexpr uint16_t BCAST_ONLY_MBP = 7208;
    constexpr uint16_t BCAST_SECURITY_STATUS_CHG_PREOPEN = 7210;
    
    constexpr uint16_t BC_OPEN_MSG = 6511;
    constexpr uint16_t BC_CLOSE_MSG = 6521;
    constexpr uint16_t BC_POSTCLOSE_MSG = 6522;
    constexpr uint16_t BC_PRE_OR_POST_DAY_MSG = 6531;
    constexpr uint16_t BC_CIRCUIT_CHECK = 6541;
    constexpr uint16_t BC_NORMAL_MKT_PREOPEN_ENDED = 6571;
    constexpr uint16_t BCAST_JRNL_VCT_MSG = 6501;
    
    constexpr uint16_t CTRL_MSG_TO_TRADER = 5295;
    constexpr uint16_t SECURITY_OPEN_PRICE = 6013;
    
    constexpr uint16_t BCAST_TICKER_AND_MKT_INDEX = 18703; // CM Ticker
    constexpr uint16_t BCAST_BUY_BACK = 18708;
    
    constexpr uint16_t BCAST_TURNOVER_EXCEEDED = 9010;
    constexpr uint16_t BROADCAST_BROKER_REACTIVATED = 9011;
    
    constexpr uint16_t BC_SYMBOL_STATUS_CHANGE_ACTION = 7764;
}

// Helper to check compression
inline bool isCompressed(uint16_t code) {
    switch(code) {
        case TxCodes::BCAST_MBO_MBP_UPDATE:
        case TxCodes::BCAST_MW_ROUND_ROBIN:
        case TxCodes::BCAST_TICKER_AND_MKT_INDEX:
        case TxCodes::BCAST_ONLY_MBP:
        case TxCodes::BCAST_IND_INDICES:
        case TxCodes::BCAST_INDICES:
            return true;
        default:
            return false;
    }
}

// Helper to get name
inline std::string getTxCodeName(uint16_t code) {
    switch(code) {
        case TxCodes::BCAST_MBO_MBP_UPDATE: return "BCAST_MBO_MBP_UPDATE";
        case TxCodes::BCAST_MW_ROUND_ROBIN: return "BCAST_MW_ROUND_ROBIN";
        case TxCodes::BCAST_TICKER_AND_MKT_INDEX: return "BCAST_TICKER_AND_MKT_INDEX";
        case TxCodes::BCAST_IND_INDICES: return "BCAST_IND_INDICES";
        case TxCodes::BCAST_SYSTEM_INFORMATION_OUT: return "BCAST_SYSTEM_INFORMATION_OUT";
        case TxCodes::BCAST_INDICES: return "BCAST_INDICES";
        case TxCodes::BCAST_ONLY_MBP: return "BCAST_ONLY_MBP";
        case TxCodes::BCAST_SECURITY_STATUS_CHG_PREOPEN: return "BCAST_SECURITY_STATUS_CHG_PREOPEN";
        
        case TxCodes::BC_OPEN_MSG: return "BC_OPEN_MSG";
        case TxCodes::BC_CLOSE_MSG: return "BC_CLOSE_MSG";
        case TxCodes::BC_POSTCLOSE_MSG: return "BC_POSTCLOSE_MSG";
        case TxCodes::BC_PRE_OR_POST_DAY_MSG: return "BC_PRE_OR_POST_DAY_MSG";
        case TxCodes::BC_CIRCUIT_CHECK: return "BC_CIRCUIT_CHECK";
        case TxCodes::BC_NORMAL_MKT_PREOPEN_ENDED: return "BC_NORMAL_MKT_PREOPEN_ENDED";
        case TxCodes::BCAST_JRNL_VCT_MSG: return "BCAST_JRNL_VCT_MSG";
        
        case TxCodes::CTRL_MSG_TO_TRADER: return "CTRL_MSG_TO_TRADER";
        case TxCodes::SECURITY_OPEN_PRICE: return "SECURITY_OPEN_PRICE";
        case TxCodes::BCAST_BUY_BACK: return "BCAST_BUY_BACK";
        
        case TxCodes::BCAST_TURNOVER_EXCEEDED: return "BCAST_TURNOVER_EXCEEDED";
        case TxCodes::BROADCAST_BROKER_REACTIVATED: return "BROADCAST_BROKER_REACTIVATED";
        case TxCodes::BC_SYMBOL_STATUS_CHANGE_ACTION: return "BC_SYMBOL_STATUS_CHANGE_ACTION";
        
        default: return "UNKNOWN_" + std::to_string(code);
    }
}

#endif // CONSTANTS_H
