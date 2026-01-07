#include "../../include/utils/parse_uncompressed_packet.h"
#include "../../include/protocol.h"
#include "../../include/constants.h"
#include "../../include/nse_parsers.h"
#include <iostream>
#include <iomanip>

namespace nsecm {

void parse_uncompressed_message(const char* data, int16_t length) {
    // std::cout << "    [Uncompressed] " << length << " bytes" << std::endl;
    
    if (length < 20) {
        std::cout << "    [Error] Message too small" << std::endl;
        return;
    }
    
    // data points to BCAST_HEADER (starts at packet offset 14)
    // TransCode is at offset 10 of BCAST_HEADER
    uint16_t txCode = be16toh_func(*((uint16_t*)(data + 10)));
    // std::cout << "    [TxCode] " << txCode << " (" << getTxCodeName(txCode) << ")" << std::endl;
    
    // Cast to appropriate structure and parse
    switch (txCode) {
        case TxCodes::CTRL_MSG_TO_TRADER:
            if (length >= sizeof(MS_TRADER_INT_MSG)) {
                parse_message_5295(reinterpret_cast<const MS_TRADER_INT_MSG*>(data));
            }
            break;

        case TxCodes::SECURITY_OPEN_PRICE:
            if (length >= sizeof(MS_SEC_OPEN_MSGS)) {
                parse_message_6013(reinterpret_cast<const MS_SEC_OPEN_MSGS*>(data));
            }
            break;

        case TxCodes::BCAST_JRNL_VCT_MSG:
            if (length >= sizeof(MS_BCAST_MESSAGE)) {
                parse_message_6501(reinterpret_cast<const MS_BCAST_MESSAGE*>(data));
            }
            break;

        case TxCodes::BC_OPEN_MSG:
            if (length >= sizeof(BCAST_VCT_MESSAGES)) {
                parse_message_6511(reinterpret_cast<const BCAST_VCT_MESSAGES*>(data));
            }
            break;

        case TxCodes::BC_CLOSE_MSG:
            if (length >= sizeof(BCAST_VCT_MESSAGES)) {
                parse_message_6521(reinterpret_cast<const BCAST_VCT_MESSAGES*>(data));
            }
            break;

        case TxCodes::BC_POSTCLOSE_MSG:
            if (length >= sizeof(BCAST_VCT_MESSAGES)) {
                parse_message_6522(reinterpret_cast<const BCAST_VCT_MESSAGES*>(data));
            }
            break;

        case TxCodes::BC_PRE_OR_POST_DAY_MSG:
            if (length >= sizeof(BCAST_VCT_MESSAGES)) {
                parse_message_6531(reinterpret_cast<const BCAST_VCT_MESSAGES*>(data));
            }
            break;

        case TxCodes::BC_CIRCUIT_CHECK:
            if (length >= sizeof(MS_BC_CIRCUIT_CHECK)) {
                parse_message_6541(reinterpret_cast<const MS_BC_CIRCUIT_CHECK*>(data));
            }
            break;

        case TxCodes::BC_NORMAL_MKT_PREOPEN_ENDED:
            if (length >= sizeof(BCAST_VCT_MESSAGES)) {
                parse_message_6571(reinterpret_cast<const BCAST_VCT_MESSAGES*>(data));
            }
            break;

        case TxCodes::BC_SYMBOL_STATUS_CHANGE_ACTION:
            if (length >= sizeof(BC_SYMBOL_STATUS_CHANGE_ACTION)) {
                parse_symbol_status_change(reinterpret_cast<const BC_SYMBOL_STATUS_CHANGE_ACTION*>(data));
            }
            break;

        default:
            break;
    }
}

} // namespace nsecm
