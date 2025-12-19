#include <iostream>
#include <stddef.h>
#include "nse_common.h"
#include "nse_market_data.h"

int main() {
    std::cout << "BCAST_HEADER field offsets:" << std::endl;
    std::cout << "  reserved1: " << offsetof(BCAST_HEADER, reserved1) << std::endl;
    std::cout << "  reserved2: " << offsetof(BCAST_HEADER, reserved2) << std::endl;
    std::cout << "  logTime: " << offsetof(BCAST_HEADER, logTime) << std::endl;
    std::cout << "  alphaChar: " << offsetof(BCAST_HEADER, alphaChar) << std::endl;
    std::cout << "  transactionCode: " << offsetof(BCAST_HEADER, transactionCode) << std::endl;
    std::cout << "  errorCode: " << offsetof(BCAST_HEADER, errorCode) << std::endl;
    std::cout << "  bcSeqNo: " << offsetof(BCAST_HEADER, bcSeqNo) << std::endl;
    std::cout << "  reserved3: " << offsetof(BCAST_HEADER, reserved3) << std::endl;
    std::cout << "  reserved4: " << offsetof(BCAST_HEADER, reserved4) << std::endl;
    std::cout << "  timeStamp2: " << offsetof(BCAST_HEADER, timeStamp2) << std::endl;
    std::cout << "  filler2: " << offsetof(BCAST_HEADER, filler2) << std::endl;
    std::cout << "  messageLength: " << offsetof(BCAST_HEADER, messageLength) << std::endl;
    std::cout << "  Total size: " << sizeof(BCAST_HEADER) << std::endl;
    
    std::cout << "\nMS_BCAST_ONLY_MBP field offsets:" << std::endl;
    std::cout << "  header: " << offsetof(MS_BCAST_ONLY_MBP, header) << std::endl;
    std::cout << "  noOfRecords: " << offsetof(MS_BCAST_ONLY_MBP, noOfRecords) << std::endl;
    std::cout << "  data: " << offsetof(MS_BCAST_ONLY_MBP, data) << std::endl;
    std::cout << "  Total size: " << sizeof(MS_BCAST_ONLY_MBP) << std::endl;
    
    return 0;
}
