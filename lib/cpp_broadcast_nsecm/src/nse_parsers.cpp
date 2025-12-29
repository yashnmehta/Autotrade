#include "../include/nse_parsers.h"
#include "../include/protocol.h"
#include "../include/market_data_callback.h"
#include <iostream>
#include <string>

void parse_circuit_check(const MS_BC_CIRCUIT_CHECK* msg) {
    (void)msg;
    // std::cout << "[ADMIN] Circuit Check received" << std::endl;
}

void parse_vct_messages(const BCAST_VCT_MESSAGES* msg) {
    AdminMessage admin;
    admin.token = 0; // Token not present in BCAST_VCT_MESSAGES
    admin.timestamp = be32toh_func(msg->header.logTime);
    uint16_t msgLen = be16toh_func(msg->broadcastMessageLength);
    if (msgLen > 240) msgLen = 240;
    admin.message = std::string(msg->broadcastMessage, msgLen);
    admin.actionCode = "VCT";
    
    MarketDataCallbackRegistry::instance().dispatchAdmin(admin);
}

void parse_jrnl_vct_msg(const MS_BCAST_MESSAGE* msg) {
    AdminMessage admin;
    admin.token = 0;
    admin.timestamp = be32toh_func(msg->header.logTime);
    uint16_t msgLen = be16toh_func(msg->broadcastMessageLength);
    if (msgLen > 240) msgLen = 240;
    admin.message = std::string(msg->broadcastMessage, msgLen);
    admin.actionCode = std::string(msg->actionCode, 3);
    
    MarketDataCallbackRegistry::instance().dispatchAdmin(admin);
}

void parse_symbol_status_change(const BC_SYMBOL_STATUS_CHANGE_ACTION* msg) {
    AdminMessage admin;
    admin.token = 0;
    admin.timestamp = be32toh_func(msg->header.logTime);
    admin.message = "Symbol Status Change: Symbol=" + std::string(msg->secInfo.symbol, 10) + 
                    ", ActionCode=" + std::to_string(be16toh_func(msg->actionCode));
    admin.actionCode = "SSC";
    
    MarketDataCallbackRegistry::instance().dispatchAdmin(admin);
}
