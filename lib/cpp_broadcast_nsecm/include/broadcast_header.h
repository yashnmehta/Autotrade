// broadcast header
struct BroadcastHeader {
    uint8_t reserved1[2];
    uint8_t reserved2[2];
    uint32_t logTime;
    uint8_t alphaChar[2];
    uint16_t transactionCode;
    uint16_t errorCode;
    uint32_t bcSeqNo;
    uint8_t reserved3;
    uint8_t reserved4[3];
    uint8_t timeStamp2[8];
    uint8_t filler[8];
    uint16_t messageLength;
};
