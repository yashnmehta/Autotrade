#ifndef MARKETWATCHCOLUMNPROFILE_H
#define MARKETWATCHCOLUMNPROFILE_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include "models/profiles/GenericTableProfile.h"
#include "models/profiles/GenericProfileManager.h"

/**
 * @brief Context tag for profile dialogs (kept for backward compat)
 */
enum class ProfileContext {
    MarketWatch,
    OrderBook,
    TradeBook,
    NetPosition,
    Holdings
};

/**
 * @brief Enumeration of all available market watch columns
 */
enum class MarketWatchColumn {
    // Identification (7 columns)
    CODE = 0,
    SYMBOL,
    SCRIP_NAME,
    INSTRUMENT_NAME,
    INSTRUMENT_TYPE,
    MARKET_TYPE,
    EXCHANGE,
    
    // F&O Specific (3 columns)
    STRIKE_PRICE,
    OPTION_TYPE,
    SERIES_EXPIRY,
    
    // Additional Identification
    ISIN_CODE,
    
    // Last Traded Information (4 columns)
    LAST_TRADED_PRICE,
    LAST_TRADED_QUANTITY,
    LAST_TRADED_TIME,
    LAST_UPDATE_TIME,
    
    // OHLC (5 columns)
    OPEN,
    HIGH,
    LOW,
    CLOSE,
    DPR,  // Price Band/Daily Price Range
    
    // Change Indicators (3 columns)
    NET_CHANGE_RS,
    PERCENT_CHANGE,
    TREND_INDICATOR,
    
    // Trading Activity (3 columns)
    AVG_TRADED_PRICE,
    VOLUME,
    VALUE,
    
    // Depth - Buy Side (3 columns)
    BUY_PRICE,
    BUY_QTY,
    TOTAL_BUY_QTY,
    
    // Depth - Sell Side (3 columns)
    SELL_PRICE,
    SELL_QTY,
    TOTAL_SELL_QTY,
    
    // Open Interest (2 columns)
    OPEN_INTEREST,
    OI_CHANGE_PERCENT,
    
    // Greeks (5 columns - Options only)
    IMPLIED_VOLATILITY,  // IV %
    BID_IV,              // New
    ASK_IV,              // New
    DELTA,
    GAMMA,
    VEGA,
    THETA,
    
    // Historical Range (4 columns)
    WEEK_52_HIGH,
    WEEK_52_LOW,
    LIFETIME_HIGH,
    LIFETIME_LOW,
    
    // Additional Metrics (2 columns)
    MARKET_CAP,
    TRADE_EXECUTION_RANGE,
    
    // Keep this last
    COLUMN_COUNT
};

/**
 * @brief Column metadata and configuration (enum → display info)
 */
struct ColumnInfo {
    MarketWatchColumn id;
    QString name;              // Display name
    QString shortName;         // Short name for compact view
    QString description;       // Tooltip description
    int defaultWidth;          // Default column width in pixels
    Qt::Alignment alignment;   // Text alignment
    bool visibleByDefault;     // Visible in default profile
    QString format;            // Data format (e.g., "%.2f", "%d")
    QString unit;              // Unit (e.g., "Rs", "%", "000s")
    bool isNumeric;            // Is numeric data
    bool isFOSpecific;         // Only for F&O instruments
    
    ColumnInfo()
        : id(MarketWatchColumn::CODE)
        , defaultWidth(80)
        , alignment(Qt::AlignLeft | Qt::AlignVCenter)
        , visibleByDefault(true)
        , isNumeric(false)
        , isFOSpecific(false)
    {}
};

/**
 * @brief Static helpers for MarketWatch column metadata.
 *
 * The profile storage itself is now handled by GenericTableProfile +
 * GenericProfileManager.  This class ONLY provides static column info
 * and preset profile factories.
 */
class MarketWatchColumnProfile
{
public:
    // Column information
    static ColumnInfo getColumnInfo(MarketWatchColumn col);
    static QString getColumnName(MarketWatchColumn col);
    static QString getColumnShortName(MarketWatchColumn col);
    static QList<MarketWatchColumn> getAllColumns();

    // ── Bridge to generic profile system ─────────────────────────────────
    static QList<GenericColumnInfo> buildGenericMetadata();

    // Preset GenericTableProfile factories
    static GenericTableProfile createDefaultProfile();
    static GenericTableProfile createCompactProfile();
    static GenericTableProfile createDetailedProfile();
    static GenericTableProfile createFOProfile();
    static GenericTableProfile createEquityProfile();
    static GenericTableProfile createTradingProfile();

    // Convenience: register all presets on a GenericProfileManager
    static void registerPresets(GenericProfileManager &mgr);

private:
    static void initializeColumnMetadata();
    static QMap<MarketWatchColumn, ColumnInfo> s_columnMetadata;
};

#endif // MARKETWATCHCOLUMNPROFILE_H
