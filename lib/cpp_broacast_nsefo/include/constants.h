#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstdint>
#include <map>
#include <string>

// Broadcast Transaction Codes
namespace TxCodes {
    constexpr uint16_t BCAST_MBO_MBP_UPDATE = 7200;
    constexpr uint16_t BCAST_MW_ROUND_ROBIN = 7201;
    constexpr uint16_t BCAST_TICKER_AND_MKT_INDEX = 7202;
    constexpr uint16_t BCAST_INDUSTRY_INDEX_UPDATE = 7203;
    constexpr uint16_t BCAST_SYSTEM_INFORMATION_OUT = 7206;
    constexpr uint16_t BCAST_INDICES = 7207;
    constexpr uint16_t BCAST_ONLY_MBP = 7208;
    constexpr uint16_t BCAST_SECURITY_STATUS_CHG_PREOPEN = 7210;
    constexpr uint16_t BCAST_SPD_MBP_DELTA = 7211;
    constexpr uint16_t BCAST_LIMIT_PRICE_PROTECTION_RANGE = 7220;
    
    constexpr uint16_t UPDATE_LOCALDB_DATA = 7304;
    constexpr uint16_t BCAST_SECURITY_MSTR_CHG = 7305;
    constexpr uint16_t BCAST_PART_MSTR_CHG = 7306;
    constexpr uint16_t UPDATE_LOCALDB_HEADER = 7307;
    constexpr uint16_t UPDATE_LOCALDB_TRAILER = 7308;
    constexpr uint16_t BCAST_SPD_MSTR_CHG = 7309;
    constexpr uint16_t BCAST_SECURITY_STATUS_CHG = 7320;
    constexpr uint16_t PARTIAL_SYSTEM_INFORMATION = 7321;
    constexpr uint16_t BCAST_INSTR_MSTR_CHG = 7324;
    constexpr uint16_t BCAST_INDEX_MSTR_CHG = 7325;
    constexpr uint16_t BCAST_INDEX_MAP_TABLE = 7326;
    constexpr uint16_t BCAST_SEC_MSTR_CHNG_PERIODIC = 7340;
    constexpr uint16_t BCAST_SPD_MSTR_CHG_PERIODIC = 7341;
    
    constexpr uint16_t BC_OPEN_MSG = 6511;
    constexpr uint16_t BC_CLOSE_MSG = 6521;
    constexpr uint16_t BC_POSTCLOSE_MSG = 6522;
    constexpr uint16_t BC_PRE_OR_POST_DAY_MSG = 6531;
    constexpr uint16_t BC_CIRCUIT_CHECK = 6541;
    constexpr uint16_t BC_NORMAL_MKT_PREOPEN_ENDED = 6571;
    constexpr uint16_t BCAST_JRNL_VCT_MSG = 6501;
    
    constexpr uint16_t CTRL_MSG_TO_TRADER = 5295;
    constexpr uint16_t SECURITY_OPEN_PRICE = 6013;
    
    constexpr uint16_t BCAST_TURNOVER_EXCEEDED = 9010;
    constexpr uint16_t BROADCAST_BROKER_REACTIVATED = 9011;
    
    constexpr uint16_t MKT_MVMT_CM_OI_IN = 7130;
    constexpr uint16_t ENHNCD_MKT_MVMT_CM_OI_IN = 17130;
    constexpr uint16_t BCAST_ENHNCD_MW_ROUND_ROBIN = 17201;
    constexpr uint16_t BCAST_ENHNCD_TICKER_AND_MKT_INDEX = 17202;
    
    constexpr uint16_t RPRT_MARKET_STATS_OUT_RPT = 1833;
    constexpr uint16_t ENHNCD_RPRT_MARKET_STATS_OUT_RPT = 11833;
    constexpr uint16_t SPD_BC_JRNL_VCT_MSG = 1862;
    constexpr uint16_t GI_INDICES_ASSETS = 7732;
}

namespace CommonConfig {
    // Replaces magic number 8 for compressed header offset
    constexpr size_t COMPRESSED_HEADER_OFFSET = 8;
    // Replaces magic number 10 for transaction codes in broadcast header
    constexpr size_t BCAST_HEADER_TXCODE_OFFSET = 10;
}

// Helper to check compression
inline bool isCompressed(uint16_t code) {
    switch(code) {
        case TxCodes::BCAST_MBO_MBP_UPDATE:
        case TxCodes::BCAST_MW_ROUND_ROBIN:
        case TxCodes::BCAST_TICKER_AND_MKT_INDEX:
        case TxCodes::BCAST_ONLY_MBP:
        case TxCodes::BCAST_LIMIT_PRICE_PROTECTION_RANGE:
        case TxCodes::BCAST_ENHNCD_MW_ROUND_ROBIN:
        case TxCodes::BCAST_ENHNCD_TICKER_AND_MKT_INDEX:
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
        case TxCodes::BCAST_INDUSTRY_INDEX_UPDATE: return "BCAST_INDUSTRY_INDEX_UPDATE";
        case TxCodes::BCAST_SYSTEM_INFORMATION_OUT: return "BCAST_SYSTEM_INFORMATION_OUT";
        case TxCodes::BCAST_INDICES: return "BCAST_INDICES";
        case TxCodes::BCAST_ONLY_MBP: return "BCAST_ONLY_MBP";
        case TxCodes::BCAST_SECURITY_STATUS_CHG_PREOPEN: return "BCAST_SECURITY_STATUS_CHG_PREOPEN";
        case TxCodes::BCAST_SPD_MBP_DELTA: return "BCAST_SPD_MBP_DELTA";
        case TxCodes::BCAST_LIMIT_PRICE_PROTECTION_RANGE: return "BCAST_LIMIT_PRICE_PROTECTION_RANGE";
        
        case TxCodes::UPDATE_LOCALDB_DATA: return "UPDATE_LOCALDB_DATA";
        case TxCodes::BCAST_SECURITY_MSTR_CHG: return "BCAST_SECURITY_MSTR_CHG";
        case TxCodes::BCAST_PART_MSTR_CHG: return "BCAST_PART_MSTR_CHG";
        case TxCodes::UPDATE_LOCALDB_HEADER: return "UPDATE_LOCALDB_HEADER";
        case TxCodes::UPDATE_LOCALDB_TRAILER: return "UPDATE_LOCALDB_TRAILER";
        case TxCodes::BCAST_SPD_MSTR_CHG: return "BCAST_SPD_MSTR_CHG";
        case TxCodes::BCAST_SECURITY_STATUS_CHG: return "BCAST_SECURITY_STATUS_CHG";
        case TxCodes::PARTIAL_SYSTEM_INFORMATION: return "PARTIAL_SYSTEM_INFORMATION";
        case TxCodes::BCAST_INSTR_MSTR_CHG: return "BCAST_INSTR_MSTR_CHG";
        case TxCodes::BCAST_INDEX_MSTR_CHG: return "BCAST_INDEX_MSTR_CHG";
        case TxCodes::BCAST_INDEX_MAP_TABLE: return "BCAST_INDEX_MAP_TABLE";
        case TxCodes::BCAST_SEC_MSTR_CHNG_PERIODIC: return "BCAST_SEC_MSTR_CHNG_PERIODIC";
        case TxCodes::BCAST_SPD_MSTR_CHG_PERIODIC: return "BCAST_SPD_MSTR_CHG_PERIODIC";
        
        case TxCodes::BC_OPEN_MSG: return "BC_OPEN_MSG";
        case TxCodes::BC_CLOSE_MSG: return "BC_CLOSE_MSG";
        case TxCodes::BC_POSTCLOSE_MSG: return "BC_POSTCLOSE_MSG";
        case TxCodes::BC_PRE_OR_POST_DAY_MSG: return "BC_PRE_OR_POST_DAY_MSG";
        case TxCodes::BC_CIRCUIT_CHECK: return "BC_CIRCUIT_CHECK";
        case TxCodes::BC_NORMAL_MKT_PREOPEN_ENDED: return "BC_NORMAL_MKT_PREOPEN_ENDED";
        case TxCodes::BCAST_JRNL_VCT_MSG: return "BCAST_JRNL_VCT_MSG";
        
        case TxCodes::CTRL_MSG_TO_TRADER: return "CTRL_MSG_TO_TRADER";
        case TxCodes::SECURITY_OPEN_PRICE: return "SECURITY_OPEN_PRICE";
        
        case TxCodes::BCAST_TURNOVER_EXCEEDED: return "BCAST_TURNOVER_EXCEEDED";
        case TxCodes::BROADCAST_BROKER_REACTIVATED: return "BROADCAST_BROKER_REACTIVATED";
        
        case TxCodes::MKT_MVMT_CM_OI_IN: return "MKT_MVMT_CM_OI_IN";
        case TxCodes::ENHNCD_MKT_MVMT_CM_OI_IN: return "ENHNCD_MKT_MVMT_CM_OI_IN";
        case TxCodes::BCAST_ENHNCD_MW_ROUND_ROBIN: return "BCAST_ENHNCD_MW_ROUND_ROBIN";
        case TxCodes::BCAST_ENHNCD_TICKER_AND_MKT_INDEX: return "BCAST_ENHNCD_TICKER_AND_MKT_INDEX";
        
        case TxCodes::RPRT_MARKET_STATS_OUT_RPT: return "RPRT_MARKET_STATS_OUT_RPT";
        case TxCodes::ENHNCD_RPRT_MARKET_STATS_OUT_RPT: return "ENHNCD_RPRT_MARKET_STATS_OUT_RPT";
        case TxCodes::SPD_BC_JRNL_VCT_MSG: return "SPD_BC_JRNL_VCT_MSG";
        case TxCodes::GI_INDICES_ASSETS: return "GI_INDICES_ASSETS";
        
        default: return "UNKNOWN_" + std::to_string(code);
    }
}

#endif // CONSTANTS_H
