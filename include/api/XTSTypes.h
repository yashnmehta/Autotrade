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
