#include "utils/parse_uncompressed_packet.h"
#include "protocol.h"
#include "constants.h"
#include "nse_parsers.h"
#include <iostream>
#include <iomanip>

namespace nsefo {

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
    if (txCode == 7207) {
    std::cout << "Transaction Code: " << txCode << std::endl;
    }
    // Cast to appropriate structure and parse
    switch (txCode) {
        case TxCodes::BC_OPEN_MSG:
            parse_market_open(reinterpret_cast<const MS_BC_OPEN_MSG*>(data));
            break;
        case TxCodes::BC_CLOSE_MSG:
            parse_market_close(reinterpret_cast<const MS_BC_CLOSE_MSG*>(data));
            break;
        case TxCodes::BC_CIRCUIT_CHECK:
            parse_circuit_check(reinterpret_cast<const MS_BC_CIRCUIT_CHECK*>(data));
            break;
        default:
            // std::cout << "    [Unknown TxCode] " << txCode << " (" << getTxCodeName(txCode) << ")" << std::endl;
            break;
    }
}

} // namespace nsefo
