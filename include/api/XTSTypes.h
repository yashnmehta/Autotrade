#ifndef XTSTYPES_H
#define XTSTYPES_H

#include <QString>
#include <QVector>
#include <QJsonObject>

namespace XTS {

// Exchange Segment Constants
enum class ExchangeSegment {
    NSECM = 1,  // NSE Cash
    NSEFO = 2,  // NSE F&O
    BSECM = 11, // BSE Cash
    BSEFO = 12, // BSE F&O
    NSECD = 13, // NSE Currency
    BSECD = 61, // BSE Currency
    MCXFO = 51  // MCX Commodity
};

// Tick data structure
struct Tick {
    int exchangeSegment;
    int64_t exchangeInstrumentID;
    double lastTradedPrice;
    int lastTradedQuantity;
    int totalBuyQuantity;
    int totalSellQuantity;
    int64_t volume;
    double open;
    double high;
    double low;
    double close;
    int64_t lastUpdateTime;
    double bidPrice;
    int bidQuantity;
    double askPrice;
    int askQuantity;
    
    // === Latency Tracking Fields ===
    // Used to measure end-to-end latency from UDP → Screen
    uint64_t refNo;              // Unique reference number from UDP packet
    int64_t timestampUdpRecv;    // µs: When UDP packet received
    int64_t timestampParsed;     // µs: When packet parsed
    int64_t timestampQueued;     // µs: When enqueued to UI thread
    int64_t timestampDequeued;   // µs: When dequeued by UI thread
    int64_t timestampFeedHandler;// µs: When FeedHandler processes
    int64_t timestampModelUpdate;// µs: When model updated
    int64_t timestampViewUpdate; // µs: When view updated (screen)
    
    // Helper to initialize all timestamps to 0
    Tick() : exchangeSegment(0), exchangeInstrumentID(0), 
             lastTradedPrice(0.0), lastTradedQuantity(0),
             totalBuyQuantity(0), totalSellQuantity(0),
             volume(0), open(0.0), high(0.0), low(0.0), close(0.0),
             lastUpdateTime(0), bidPrice(0.0), bidQuantity(0),
             askPrice(0.0), askQuantity(0),
             refNo(0), timestampUdpRecv(0), timestampParsed(0),
             timestampQueued(0), timestampDequeued(0),
             timestampFeedHandler(0), timestampModelUpdate(0),
             timestampViewUpdate(0) {}
};

// Instrument data structure
struct Instrument {
    int exchangeSegment;
    int64_t exchangeInstrumentID;
    QString instrumentName;
    QString series;
    QString nameWithSeries;
    int64_t instrumentID;
    double priceBandHigh;
    double priceBandLow;
    int freezeQty;
    double tickSize;
    int lotSize;
};

// Position data structure
struct Position {
    QString exchangeSegment;
    int64_t exchangeInstrumentID;
    QString productType;
    int quantity;
    double buyAveragePrice;
    double sellAveragePrice;
    int netQuantity;
    double realizedProfit;
    double unrealizedProfit;
    double mtm;
};

// Order data structure
struct Order {
    QString appOrderID;
    QString orderID;
    QString exchangeOrderID;
    int64_t exchangeInstrumentID;
    QString orderSide;  // BUY/SELL
    QString orderType;  // MARKET/LIMIT/SL
    QString productType;
    QString timeInForce;
    double orderPrice;
    int orderQuantity;
    int filledQuantity;
    QString orderStatus;
    QString orderTimestamp;
};

// Trade data structure
struct Trade {
    QString tradeID;
    QString orderID;
    int64_t exchangeInstrumentID;
    QString tradeSide;
    double tradePrice;
    int tradeQuantity;
    QString tradeTimestamp;
};

} // namespace XTS

#endif // XTSTYPES_H
