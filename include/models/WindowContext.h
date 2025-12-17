#ifndef WINDOWCONTEXT_H
#define WINDOWCONTEXT_H

#include <QString>
#include <QVariant>
#include <QHash>

/**
 * @brief Context data passed when opening windows from other windows
 * 
 * Carries contract/scrip details and metadata about the source window
 * to enable intelligent initialization of target windows.
 */
struct WindowContext {
    // Source window identification
    QString sourceWindow;  // "MarketWatch", "PositionWindow", "OrderBook", etc.
    int sourceRow = -1;    // Row number in source (if applicable)
    
    // Contract details
    QString exchange;      // NSEFO, NSECM, BSEFO, BSECM
    QString segment;       // E, D, F (Equity, Derivative, FnO)
    int64_t token = 0;     // Instrument token
    QString symbol;        // Trading symbol
    QString displayName;   // Display name for UI
    QString series;        // EQ, BE, etc.
    QString instrumentType; // OPTIDX, FUTSTK, etc.
    
    // Option details (if applicable)
    QString expiry;        // Expiry date (DDMMMYYYY format)
    double strikePrice = 0.0;
    QString optionType;    // CE, PE, or XX (futures)
    
    // Market data (current prices)
    double ltp = 0.0;      // Last traded price
    double bid = 0.0;      // Best bid
    double ask = 0.0;      // Best ask
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;    // Previous close
    int64_t volume = 0;
    
    // Contract specifications
    int lotSize = 1;
    double tickSize = 0.05;
    int freezeQty = 0;
    
    // Additional metadata (extensible)
    QHash<QString, QVariant> metadata;
    
    // Constructors
    WindowContext() = default;
    
    WindowContext(const QString &src, const QString &ex, int64_t tok, const QString &sym)
        : sourceWindow(src), exchange(ex), token(tok), symbol(sym) {}
    
    // Validation
    bool isValid() const {
        return !exchange.isEmpty() && token > 0 && !symbol.isEmpty();
    }
    
    // Helpers
    void addMetadata(const QString &key, const QVariant &value) {
        metadata[key] = value;
    }
    
    QVariant getMetadata(const QString &key, const QVariant &defaultValue = QVariant()) const {
        return metadata.value(key, defaultValue);
    }
    
    // Debugging
    QString toString() const {
        return QString("WindowContext(source=%1, exchange=%2, token=%3, symbol=%4, ltp=%5)")
            .arg(sourceWindow).arg(exchange).arg(token).arg(symbol).arg(ltp);
    }
};

#endif // WINDOWCONTEXT_H
