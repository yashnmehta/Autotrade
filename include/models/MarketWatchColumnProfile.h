#ifndef MARKETWATCHCOLUMNPROFILE_H
#define MARKETWATCHCOLUMNPROFILE_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief Enumeration of all available market watch columns
 * 
 * Comprehensive list of 40+ columns for market watch display
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
 * @brief Column metadata and configuration
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
 * @brief Column profile for market watch display configuration
 * 
 * Manages which columns are visible, their order, and widths
 */
class MarketWatchColumnProfile
{
public:
    MarketWatchColumnProfile();
    explicit MarketWatchColumnProfile(const QString &name, ProfileContext context = ProfileContext::MarketWatch);
    
    // Context
    ProfileContext context() const { return m_context; }
    void setContext(ProfileContext context) { m_context = context; }

    // Profile Management
    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }
    
    QString description() const { return m_description; }
    void setDescription(const QString &desc) { m_description = desc; }
    
    // Column Visibility
    void setColumnVisible(MarketWatchColumn col, bool visible);
    bool isColumnVisible(MarketWatchColumn col) const;
    QList<MarketWatchColumn> visibleColumns() const;
    int visibleColumnCount() const;
    
    // Column Order
    void setColumnOrder(const QList<MarketWatchColumn> &order);
    QList<MarketWatchColumn> columnOrder() const { return m_columnOrder; }
    void moveColumn(int fromIndex, int toIndex);
    
    // Column Width
    void setColumnWidth(MarketWatchColumn col, int width);
    int columnWidth(MarketWatchColumn col) const;
    
    // Preset Profiles
    static MarketWatchColumnProfile createDefaultProfile();
    static MarketWatchColumnProfile createCompactProfile();
    static MarketWatchColumnProfile createDetailedProfile();
    static MarketWatchColumnProfile createFOProfile();        // F&O focused
    static MarketWatchColumnProfile createEquityProfile();    // Equity focused
    static MarketWatchColumnProfile createTradingProfile();   // Active trading
    
    // Serialization
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json);
    
    // Save/Load from file
    bool saveToFile(const QString &filepath) const;
    bool loadFromFile(const QString &filepath);
    
    // Column Information
    static ColumnInfo getColumnInfo(MarketWatchColumn col);
    static QString getColumnName(MarketWatchColumn col);
    static QString getColumnShortName(MarketWatchColumn col);
    static QList<MarketWatchColumn> getAllColumns();
    
private:
    QString m_name;
    ProfileContext m_context;
    QString m_description;
    QMap<MarketWatchColumn, bool> m_visibility;
    QMap<MarketWatchColumn, int> m_widths;
    QList<MarketWatchColumn> m_columnOrder;
    
    void initializeDefaults();
    static void initializeColumnMetadata();
    static QMap<MarketWatchColumn, ColumnInfo> s_columnMetadata;
};

/**
 * @brief Profile manager for loading/saving multiple profiles
 */
class MarketWatchProfileManager
{
public:
    static MarketWatchProfileManager& instance();
    
    // Profile CRUD operations
    void addProfile(const MarketWatchColumnProfile &profile);
    bool removeProfile(const QString &name);
    MarketWatchColumnProfile getProfile(const QString &name) const;
    QStringList profileNames() const;
    bool hasProfile(const QString &name) const;
    
    // Current profile management
    void setCurrentProfile(const QString &name);
    QString currentProfileName() const { return m_currentProfileName; }
    MarketWatchColumnProfile currentProfile() const;
    
    // Persistence
    bool saveAllProfiles(const QString &directory) const;
    bool loadAllProfiles(const QString &directory);
    
    // Default profiles
    void loadDefaultProfiles();
    
private:
    MarketWatchProfileManager();
    ~MarketWatchProfileManager() = default;
    
    QMap<QString, MarketWatchColumnProfile> m_profiles;
    QString m_currentProfileName;
};

#endif // MARKETWATCHCOLUMNPROFILE_H
