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

// 5-level market depth structure
struct DepthLevel {
    double price = 0.0;
    int64_t quantity = 0;
    int orders = 0;
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
    double averagePrice;         // Average Traded Price
    int64_t openInterest;        // Open Interest
    
    // === 5-Level Market Depth ===
    DepthLevel bidDepth[5];      // 5 levels of bid depth
    DepthLevel askDepth[5];      // 5 levels of ask depth
    
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
             averagePrice(0.0), openInterest(0),
             bidDepth{}, askDepth{},
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
    QString instrumentType;
    QString symbol;
    QString expiryDate;
    double strikePrice;
    QString optionType;
};

// Position data structure
struct Position {
    QString accountID;
    double actualBuyAmount;
    double actualBuyAveragePrice;
    double actualSellAmount;
    double actualSellAveragePrice;
    double bep;
    double buyAmount;
    double buyAveragePrice;
    int64_t exchangeInstrumentID;
    QString exchangeSegment;
    QString loginID;
    double mtm;
    int marketLot;
    double multiplier;
    double netAmount;
    int openBuyQuantity;
    int openSellQuantity;
    QString productType;
    int quantity;
    double realizedMTM;
    double sellAmount;
    double sellAveragePrice;
    QString tradingSymbol;
    double unrealizedMTM;
    
    // Legacy mapping helpers
    double realizedProfit() const { return realizedMTM; }
    double unrealizedProfit() const { return unrealizedMTM; }
};

// Order data structure
struct Order {
    int64_t appOrderID;
    QString exchangeOrderID;
    QString clientID;
    QString loginID;
    QString exchangeSegment;
    int64_t exchangeInstrumentID;
    QString tradingSymbol;
    QString orderSide;
    QString orderType;
    double orderPrice;
    double orderStopPrice;
    int orderQuantity;
    int cumulativeQuantity;
    int leavesQuantity;
    QString orderStatus;
    double orderAverageTradedPrice;
    QString productType;
    QString timeInForce;
    QString orderGeneratedDateTime;
    QString exchangeTransactTime;
    QString lastUpdateDateTime;
    QString orderUniqueIdentifier;
    QString orderReferenceID;
    QString cancelRejectReason;
    QString orderCategoryType;
    QString orderLegStatus;
    int orderDisclosedQuantity;
    QString orderExpiryDate;
    
    // Legacy mapping helpers
    QString appOrderIDStr() const { return QString::number(appOrderID); }
    int filledQuantity() const { return cumulativeQuantity; }
    int pendingQuantity() const { return leavesQuantity; }
    QString orderTimestamp() const { return orderGeneratedDateTime; }
};

// Trade data structure
struct Trade {
    QString executionID;
    int64_t appOrderID;
    QString exchangeOrderID;
    QString clientID;
    QString loginID;
    QString exchangeSegment;
    int64_t exchangeInstrumentID;
    QString tradingSymbol;
    QString orderSide;
    QString orderType;
    double lastTradedPrice;
    int lastTradedQuantity;
    QString lastExecutionTransactTime;
    QString orderGeneratedDateTime;
    QString exchangeTransactTime;
    double orderAverageTradedPrice;
    int cumulativeQuantity;
    int leavesQuantity;
    QString orderStatus;
    QString productType;
    QString orderUniqueIdentifier;
    double orderPrice;
    int orderQuantity;

    // Legacy mapping helpers
    QString tradeID() const { return executionID; }
    QString orderIDStr() const { return QString::number(appOrderID); }
    QString tradeTimestamp() const { return lastExecutionTransactTime; }
    double tradePrice() const { return lastTradedPrice; }
    int tradeQuantity() const { return lastTradedQuantity; }
    QString tradeSide() const { return orderSide; }
};

// Order placement parameters
struct OrderParams {
    QString exchangeSegment;
    int64_t exchangeInstrumentID;
    QString productType;
    QString orderType;
    QString orderSide;
    QString timeInForce;
    int orderQuantity;
    int disclosedQuantity;
    double limitPrice;
    double stopPrice;
    QString orderUniqueIdentifier;
    QString clientID; // Optional override
};

// Order modification parameters
struct ModifyOrderParams {
    int64_t appOrderID;                 // Original order ID to modify
    int64_t exchangeInstrumentID;       // Instrument token
    QString exchangeSegment;            // Exchange segment (NSEFO, NSECM, etc.)
    QString productType;                // Product type (MIS, NRML, CNC) - REQUIRED by API
    QString orderType;                  // Limit, Market, StopLimit, StopMarket
    int modifiedOrderQuantity;          // New total quantity (must be >= filled qty)
    int modifiedDisclosedQuantity;      // New disclosed quantity
    double modifiedLimitPrice;          // New limit price
    double modifiedStopPrice;           // New trigger price (for SL orders)
    QString modifiedTimeInForce;        // DAY, IOC, GTD
    QString orderUniqueIdentifier;      // Tracking ID
    QString clientID;                   // Optional client ID override
};

} // namespace XTS

// Register XTS types with Qt's meta-type system for cross-thread signal/slot connections
// This is REQUIRED for passing these types via queued connections (different threads)
Q_DECLARE_METATYPE(XTS::Order)
Q_DECLARE_METATYPE(XTS::Trade)
Q_DECLARE_METATYPE(XTS::Position)
Q_DECLARE_METATYPE(XTS::Tick)
Q_DECLARE_METATYPE(QVector<XTS::Order>)
Q_DECLARE_METATYPE(QVector<XTS::Trade>)
Q_DECLARE_METATYPE(QVector<XTS::Position>)

#endif // XTSTYPES_H
