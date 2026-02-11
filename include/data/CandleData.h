#ifndef CANDLEDATA_H
#define CANDLEDATA_H

#include <QJsonObject>
#include <QMetaType>
#include <cstdint>

namespace ChartData {

/**
 * @brief OHLCV Candle structure for charting
 */
struct Candle {
    qint64 timestamp;     // Unix epoch in seconds
    double open;
    double high;
    double low;
    double close;
    qint64 volume;
    qint64 openInterest;  // For futures/options
    
    Candle() 
        : timestamp(0), open(0), high(0), low(0), close(0), 
          volume(0), openInterest(0) {}
    
    Candle(qint64 ts, double o, double h, double l, double c, qint64 v = 0, qint64 oi = 0)
        : timestamp(ts), open(o), high(h), low(l), close(c), 
          volume(v), openInterest(oi) {}
    
    bool isValid() const {
        return timestamp > 0 && open > 0 && high > 0 && low > 0 && close > 0;
    }
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["time"] = timestamp;
        obj["open"] = open;
        obj["high"] = high;
        obj["low"] = low;
        obj["close"] = close;
        if (volume > 0) obj["volume"] = static_cast<double>(volume);
        if (openInterest > 0) obj["openInterest"] = static_cast<double>(openInterest);
        return obj;
    }
    
    static Candle fromJson(const QJsonObject& obj) {
        return Candle(
            obj["time"].toVariant().toLongLong(),
            obj["open"].toDouble(),
            obj["high"].toDouble(),
            obj["low"].toDouble(),
            obj["close"].toDouble(),
            obj["volume"].toVariant().toLongLong(),
            obj["openInterest"].toVariant().toLongLong()
        );
    }
};

/**
 * @brief Timeframe enumeration for candle aggregation
 */
enum class Timeframe {
    OneMinute,
    FiveMinutes,
    FifteenMinutes,
    ThirtyMinutes,
    OneHour,
    FourHours,
    OneDay,
    OneWeek
};

/**
 * @brief Convert timeframe to string
 */
inline QString timeframeToString(Timeframe tf) {
    switch(tf) {
        case Timeframe::OneMinute: return "1m";
        case Timeframe::FiveMinutes: return "5m";
        case Timeframe::FifteenMinutes: return "15m";
        case Timeframe::ThirtyMinutes: return "30m";
        case Timeframe::OneHour: return "1h";
        case Timeframe::FourHours: return "4h";
        case Timeframe::OneDay: return "1D";
        case Timeframe::OneWeek: return "1W";
    }
    return "1m";
}

/**
 * @brief Convert string to timeframe
 */
inline Timeframe stringToTimeframe(const QString& str) {
    if (str == "1m") return Timeframe::OneMinute;
    if (str == "5m") return Timeframe::FiveMinutes;
    if (str == "15m") return Timeframe::FifteenMinutes;
    if (str == "30m") return Timeframe::ThirtyMinutes;
    if (str == "1h") return Timeframe::OneHour;
    if (str == "4h") return Timeframe::FourHours;
    if (str == "1D") return Timeframe::OneDay;
    if (str == "1W") return Timeframe::OneWeek;
    return Timeframe::OneMinute;
}

/**
 * @brief Get timeframe duration in seconds
 */
inline qint64 timeframeDuration(Timeframe tf) {
    switch(tf) {
        case Timeframe::OneMinute: return 60;
        case Timeframe::FiveMinutes: return 300;
        case Timeframe::FifteenMinutes: return 900;
        case Timeframe::ThirtyMinutes: return 1800;
        case Timeframe::OneHour: return 3600;
        case Timeframe::FourHours: return 14400;
        case Timeframe::OneDay: return 86400;
        case Timeframe::OneWeek: return 604800;
    }
    return 60;
}

/**
 * @brief Get candle start time for a given timestamp and timeframe
 */
inline qint64 getCandleStartTime(qint64 timestamp, Timeframe tf) {
    qint64 duration = timeframeDuration(tf);
    return (timestamp / duration) * duration;
}

} // namespace ChartData

// Register metatypes for Qt signals/slots
Q_DECLARE_METATYPE(ChartData::Candle)
Q_DECLARE_METATYPE(ChartData::Timeframe)

#endif // CANDLEDATA_H
