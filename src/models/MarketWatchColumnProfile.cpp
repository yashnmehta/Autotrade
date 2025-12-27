#include "models/MarketWatchColumnProfile.h"
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <QDir>

// Static member initialization
QMap<MarketWatchColumn, ColumnInfo> MarketWatchColumnProfile::s_columnMetadata;

MarketWatchColumnProfile::MarketWatchColumnProfile()
    : m_name("Unnamed Profile")
{
    initializeDefaults();
}

MarketWatchColumnProfile::MarketWatchColumnProfile(const QString &name)
    : m_name(name)
{
    initializeDefaults();
}

void MarketWatchColumnProfile::initializeColumnMetadata()
{
    if (!s_columnMetadata.isEmpty()) return;
    
    // Initialize all column metadata
    ColumnInfo info;
    
    // Identification columns
    info = ColumnInfo();
    info.id = MarketWatchColumn::CODE;
    info.name = "Code";
    info.shortName = "Code";
    info.description = "Security Code/Token";
    info.defaultWidth = 80;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::CODE] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::SYMBOL;
    info.name = "Symbol";
    info.shortName = "Sym";
    info.description = "Trading Symbol";
    info.defaultWidth = 100;
    info.alignment = Qt::AlignLeft | Qt::AlignVCenter;
    info.visibleByDefault = true;
    s_columnMetadata[MarketWatchColumn::SYMBOL] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::SCRIP_NAME;
    info.name = "Scrip Name";
    info.shortName = "Name";
    info.description = "Full Scrip Name";
    info.defaultWidth = 150;
    info.alignment = Qt::AlignLeft | Qt::AlignVCenter;
    info.visibleByDefault = false;
    s_columnMetadata[MarketWatchColumn::SCRIP_NAME] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::INSTRUMENT_NAME;
    info.name = "Instrument Name";
    info.shortName = "Inst";
    info.description = "Instrument Name";
    info.defaultWidth = 120;
    info.alignment = Qt::AlignLeft | Qt::AlignVCenter;
    info.visibleByDefault = false;
    s_columnMetadata[MarketWatchColumn::INSTRUMENT_NAME] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::INSTRUMENT_TYPE;
    info.name = "Instrument Type";
    info.shortName = "Type";
    info.description = "Instrument Type (FUTIDX, OPTSTK, etc.)";
    info.defaultWidth = 80;
    info.alignment = Qt::AlignCenter | Qt::AlignVCenter;
    info.visibleByDefault = true;
    s_columnMetadata[MarketWatchColumn::INSTRUMENT_TYPE] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::MARKET_TYPE;
    info.name = "Market Type";
    info.shortName = "Mkt";
    info.description = "Market Type (Normal/Auction)";
    info.defaultWidth = 70;
    info.alignment = Qt::AlignCenter | Qt::AlignVCenter;
    info.visibleByDefault = false;
    s_columnMetadata[MarketWatchColumn::MARKET_TYPE] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::EXCHANGE;
    info.name = "Exchange";
    info.shortName = "Exch";
    info.description = "Exchange (NSE/BSE/MCX)";
    info.defaultWidth = 60;
    info.alignment = Qt::AlignCenter | Qt::AlignVCenter;
    info.visibleByDefault = true;
    s_columnMetadata[MarketWatchColumn::EXCHANGE] = info;
    
    // F&O Specific columns
    info = ColumnInfo();
    info.id = MarketWatchColumn::STRIKE_PRICE;
    info.name = "Strike Price";
    info.shortName = "Strike";
    info.description = "Option Strike Price";
    info.defaultWidth = 80;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%.2f";
    info.isNumeric = true;
    info.isFOSpecific = true;
    s_columnMetadata[MarketWatchColumn::STRIKE_PRICE] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::OPTION_TYPE;
    info.name = "Option Type";
    info.shortName = "Opt";
    info.description = "Option Type (CE/PE)";
    info.defaultWidth = 50;
    info.alignment = Qt::AlignCenter | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.isFOSpecific = true;
    s_columnMetadata[MarketWatchColumn::OPTION_TYPE] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::SERIES_EXPIRY;
    info.name = "Ser/Exp";
    info.shortName = "Expiry";
    info.description = "Series/Expiry Date";
    info.defaultWidth = 90;
    info.alignment = Qt::AlignCenter | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.isFOSpecific = true;
    s_columnMetadata[MarketWatchColumn::SERIES_EXPIRY] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::ISIN_CODE;
    info.name = "ISIN Code";
    info.shortName = "ISIN";
    info.description = "ISIN Code";
    info.defaultWidth = 120;
    info.alignment = Qt::AlignLeft | Qt::AlignVCenter;
    info.visibleByDefault = false;
    s_columnMetadata[MarketWatchColumn::ISIN_CODE] = info;
    
    // Last Traded Information
    info = ColumnInfo();
    info.id = MarketWatchColumn::LAST_TRADED_PRICE;
    info.name = "LTP";
    info.shortName = "LTP";
    info.description = "Last Traded Price";
    info.defaultWidth = 80;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::LAST_TRADED_PRICE] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::LAST_TRADED_QUANTITY;
    info.name = "LTQ";
    info.shortName = "LTQ";
    info.description = "Last Traded Quantity";
    info.defaultWidth = 70;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%d";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::LAST_TRADED_QUANTITY] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::LAST_TRADED_TIME;
    info.name = "LTT";
    info.shortName = "LTT";
    info.description = "Last Traded Time";
    info.defaultWidth = 90;
    info.alignment = Qt::AlignCenter | Qt::AlignVCenter;
    info.visibleByDefault = false;
    s_columnMetadata[MarketWatchColumn::LAST_TRADED_TIME] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::LAST_UPDATE_TIME;
    info.name = "Last Update";
    info.shortName = "Updated";
    info.description = "Last Update Time";
    info.defaultWidth = 90;
    info.alignment = Qt::AlignCenter | Qt::AlignVCenter;
    info.visibleByDefault = false;
    s_columnMetadata[MarketWatchColumn::LAST_UPDATE_TIME] = info;
    
    // OHLC
    info = ColumnInfo();
    info.id = MarketWatchColumn::OPEN;
    info.name = "Open";
    info.shortName = "Open";
    info.description = "Opening Price";
    info.defaultWidth = 70;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::OPEN] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::HIGH;
    info.name = "High";
    info.shortName = "High";
    info.description = "Day High";
    info.defaultWidth = 70;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::HIGH] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::LOW;
    info.name = "Low";
    info.shortName = "Low";
    info.description = "Day Low";
    info.defaultWidth = 70;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::LOW] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::CLOSE;
    info.name = "Close";
    info.shortName = "Close";
    info.description = "Closing/Previous Close";
    info.defaultWidth = 70;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::CLOSE] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::DPR;
    info.name = "DPR";
    info.shortName = "DPR";
    info.description = "Daily Price Range/Price Band";
    info.defaultWidth = 100;
    info.alignment = Qt::AlignCenter | Qt::AlignVCenter;
    info.visibleByDefault = false;
    s_columnMetadata[MarketWatchColumn::DPR] = info;
    
    // Change Indicators
    info = ColumnInfo();
    info.id = MarketWatchColumn::NET_CHANGE_RS;
    info.name = "Change (Rs)";
    info.shortName = "Chg";
    info.description = "Net Change in Rupees";
    info.defaultWidth = 80;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::NET_CHANGE_RS] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::PERCENT_CHANGE;
    info.name = "% Change";
    info.shortName = "%Chg";
    info.description = "Percentage Change";
    info.defaultWidth = 70;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%.2f";
    info.unit = "%";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::PERCENT_CHANGE] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::TREND_INDICATOR;
    info.name = "Trend";
    info.shortName = "Trend";
    info.description = "Trend Indicator (↑/↓)";
    info.defaultWidth = 50;
    info.alignment = Qt::AlignCenter | Qt::AlignVCenter;
    info.visibleByDefault = true;
    s_columnMetadata[MarketWatchColumn::TREND_INDICATOR] = info;
    
    // Trading Activity
    info = ColumnInfo();
    info.id = MarketWatchColumn::AVG_TRADED_PRICE;
    info.name = "Avg Price";
    info.shortName = "AvgP";
    info.description = "Average Traded Price";
    info.defaultWidth = 80;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::AVG_TRADED_PRICE] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::VOLUME;
    info.name = "Volume";
    info.shortName = "Vol";
    info.description = "Volume (in 000s)";
    info.defaultWidth = 90;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%d";
    info.unit = "000s";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::VOLUME] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::VALUE;
    info.name = "Value";
    info.shortName = "Value";
    info.description = "Value (in lacs)";
    info.defaultWidth = 90;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.2f";
    info.unit = "lacs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::VALUE] = info;
    
    // Depth - Buy Side
    info = ColumnInfo();
    info.id = MarketWatchColumn::BUY_PRICE;
    info.name = "Buy Price";
    info.shortName = "Bid";
    info.description = "Best Buy Price";
    info.defaultWidth = 80;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::BUY_PRICE] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::BUY_QTY;
    info.name = "Buy Qty";
    info.shortName = "BidQ";
    info.description = "Buy Quantity";
    info.defaultWidth = 80;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%d";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::BUY_QTY] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::TOTAL_BUY_QTY;
    info.name = "Total Buy Qty";
    info.shortName = "TotBid";
    info.description = "Total Buy Quantity (all 5 levels)";
    info.defaultWidth = 90;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%d";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::TOTAL_BUY_QTY] = info;
    
    // Depth - Sell Side
    info = ColumnInfo();
    info.id = MarketWatchColumn::SELL_PRICE;
    info.name = "Sell Price";
    info.shortName = "Ask";
    info.description = "Best Sell Price";
    info.defaultWidth = 80;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::SELL_PRICE] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::SELL_QTY;
    info.name = "Sell Qty";
    info.shortName = "AskQ";
    info.description = "Sell Quantity";
    info.defaultWidth = 80;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%d";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::SELL_QTY] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::TOTAL_SELL_QTY;
    info.name = "Total Sell Qty";
    info.shortName = "TotAsk";
    info.description = "Total Sell Quantity (all 5 levels)";
    info.defaultWidth = 90;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%d";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::TOTAL_SELL_QTY] = info;
    
    // Open Interest
    info = ColumnInfo();
    info.id = MarketWatchColumn::OPEN_INTEREST;
    info.name = "OI";
    info.shortName = "OI";
    info.description = "Open Interest";
    info.defaultWidth = 90;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%d";
    info.isNumeric = true;
    info.isFOSpecific = true;
    s_columnMetadata[MarketWatchColumn::OPEN_INTEREST] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::OI_CHANGE_PERCENT;
    info.name = "%OI";
    info.shortName = "%OI";
    info.description = "OI Change %";
    info.defaultWidth = 70;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = true;
    info.format = "%.2f";
    info.unit = "%";
    info.isNumeric = true;
    info.isFOSpecific = true;
    s_columnMetadata[MarketWatchColumn::OI_CHANGE_PERCENT] = info;
    
    // Historical Range
    info = ColumnInfo();
    info.id = MarketWatchColumn::WEEK_52_HIGH;
    info.name = "52W High";
    info.shortName = "52WH";
    info.description = "52 Week High";
    info.defaultWidth = 80;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::WEEK_52_HIGH] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::WEEK_52_LOW;
    info.name = "52W Low";
    info.shortName = "52WL";
    info.description = "52 Week Low";
    info.defaultWidth = 80;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::WEEK_52_LOW] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::LIFETIME_HIGH;
    info.name = "Lifetime High";
    info.shortName = "LifeH";
    info.description = "Lifetime High";
    info.defaultWidth = 90;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::LIFETIME_HIGH] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::LIFETIME_LOW;
    info.name = "Lifetime Low";
    info.shortName = "LifeL";
    info.description = "Lifetime Low";
    info.defaultWidth = 90;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.2f";
    info.unit = "Rs";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::LIFETIME_LOW] = info;
    
    // Additional Metrics
    info = ColumnInfo();
    info.id = MarketWatchColumn::MARKET_CAP;
    info.name = "Market Cap";
    info.shortName = "MCap";
    info.description = "Market Capitalization";
    info.defaultWidth = 100;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.2f";
    info.unit = "Cr";
    info.isNumeric = true;
    s_columnMetadata[MarketWatchColumn::MARKET_CAP] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::TRADE_EXECUTION_RANGE;
    info.name = "Trade Range";
    info.shortName = "Range";
    info.description = "Trade Execution Range";
    info.defaultWidth = 100;
    info.alignment = Qt::AlignCenter | Qt::AlignVCenter;
    info.visibleByDefault = false;
    s_columnMetadata[MarketWatchColumn::TRADE_EXECUTION_RANGE] = info;
}

void MarketWatchColumnProfile::initializeDefaults()
{
    initializeColumnMetadata();
    
    // Set all columns to default visibility
    for (int i = 0; i < static_cast<int>(MarketWatchColumn::COLUMN_COUNT); ++i) {
        MarketWatchColumn col = static_cast<MarketWatchColumn>(i);
        ColumnInfo info = s_columnMetadata[col];
        m_visibility[col] = info.visibleByDefault;
        m_widths[col] = info.defaultWidth;
        m_columnOrder.append(col);
    }
}

void MarketWatchColumnProfile::setColumnVisible(MarketWatchColumn col, bool visible)
{
    m_visibility[col] = visible;
}

bool MarketWatchColumnProfile::isColumnVisible(MarketWatchColumn col) const
{
    return m_visibility.value(col, false);
}

QList<MarketWatchColumn> MarketWatchColumnProfile::visibleColumns() const
{
    QList<MarketWatchColumn> visible;
    for (MarketWatchColumn col : m_columnOrder) {
        if (isColumnVisible(col)) {
            visible.append(col);
        }
    }
    return visible;
}

int MarketWatchColumnProfile::visibleColumnCount() const
{
    return visibleColumns().count();
}

void MarketWatchColumnProfile::setColumnOrder(const QList<MarketWatchColumn> &order)
{
    m_columnOrder = order;
}

void MarketWatchColumnProfile::moveColumn(int fromIndex, int toIndex)
{
    if (fromIndex >= 0 && fromIndex < m_columnOrder.size() &&
        toIndex >= 0 && toIndex < m_columnOrder.size()) {
        m_columnOrder.move(fromIndex, toIndex);
    }
}

void MarketWatchColumnProfile::setColumnWidth(MarketWatchColumn col, int width)
{
    m_widths[col] = width;
}

int MarketWatchColumnProfile::columnWidth(MarketWatchColumn col) const
{
    return m_widths.value(col, 80);
}

ColumnInfo MarketWatchColumnProfile::getColumnInfo(MarketWatchColumn col)
{
    initializeColumnMetadata();
    return s_columnMetadata.value(col, ColumnInfo());
}

QString MarketWatchColumnProfile::getColumnName(MarketWatchColumn col)
{
    return getColumnInfo(col).name;
}

QString MarketWatchColumnProfile::getColumnShortName(MarketWatchColumn col)
{
    return getColumnInfo(col).shortName;
}

QList<MarketWatchColumn> MarketWatchColumnProfile::getAllColumns()
{
    QList<MarketWatchColumn> all;
    for (int i = 0; i < static_cast<int>(MarketWatchColumn::COLUMN_COUNT); ++i) {
        all.append(static_cast<MarketWatchColumn>(i));
    }
    return all;
}

// Preset Profiles
MarketWatchColumnProfile MarketWatchColumnProfile::createDefaultProfile()
{
    MarketWatchColumnProfile profile("Default");
    profile.setDescription("Standard market watch view");
    // Uses default visibility settings from metadata
    return profile;
}

MarketWatchColumnProfile MarketWatchColumnProfile::createCompactProfile()
{
    MarketWatchColumnProfile profile("Compact");
    profile.setDescription("Minimal columns for compact display");
    
    // Hide all columns first
    for (int i = 0; i < static_cast<int>(MarketWatchColumn::COLUMN_COUNT); ++i) {
        profile.setColumnVisible(static_cast<MarketWatchColumn>(i), false);
    }
    
    // Show only essential columns
    profile.setColumnVisible(MarketWatchColumn::SYMBOL, true);
    profile.setColumnVisible(MarketWatchColumn::LAST_TRADED_PRICE, true);
    profile.setColumnVisible(MarketWatchColumn::NET_CHANGE_RS, true);
    profile.setColumnVisible(MarketWatchColumn::PERCENT_CHANGE, true);
    profile.setColumnVisible(MarketWatchColumn::VOLUME, true);
    
    return profile;
}

MarketWatchColumnProfile MarketWatchColumnProfile::createDetailedProfile()
{
    MarketWatchColumnProfile profile("Detailed");
    profile.setDescription("All important columns");
    
    // Show most columns
    for (int i = 0; i < static_cast<int>(MarketWatchColumn::COLUMN_COUNT); ++i) {
        MarketWatchColumn col = static_cast<MarketWatchColumn>(i);
        // Skip very specialized columns
        if (col != MarketWatchColumn::MARKET_TYPE &&
            col != MarketWatchColumn::ISIN_CODE &&
            col != MarketWatchColumn::TRADE_EXECUTION_RANGE) {
            profile.setColumnVisible(col, true);
        }
    }
    
    return profile;
}

MarketWatchColumnProfile MarketWatchColumnProfile::createFOProfile()
{
    MarketWatchColumnProfile profile("F&O");
    profile.setDescription("Futures & Options focused");
    
    // Start with default
    profile = createDefaultProfile();
    profile.setName("F&O");
    
    // Add F&O specific columns
    profile.setColumnVisible(MarketWatchColumn::STRIKE_PRICE, true);
    profile.setColumnVisible(MarketWatchColumn::OPTION_TYPE, true);
    profile.setColumnVisible(MarketWatchColumn::SERIES_EXPIRY, true);
    profile.setColumnVisible(MarketWatchColumn::OPEN_INTEREST, true);
    profile.setColumnVisible(MarketWatchColumn::OI_CHANGE_PERCENT, true);
    
    // Hide equity-specific
    profile.setColumnVisible(MarketWatchColumn::MARKET_CAP, false);
    profile.setColumnVisible(MarketWatchColumn::WEEK_52_HIGH, false);
    profile.setColumnVisible(MarketWatchColumn::WEEK_52_LOW, false);
    
    return profile;
}

MarketWatchColumnProfile MarketWatchColumnProfile::createEquityProfile()
{
    MarketWatchColumnProfile profile("Equity");
    profile.setDescription("Equity/Cash market focused");
    
    // Start with default
    profile = createDefaultProfile();
    profile.setName("Equity");
    
    // Add equity specific
    profile.setColumnVisible(MarketWatchColumn::MARKET_CAP, true);
    profile.setColumnVisible(MarketWatchColumn::WEEK_52_HIGH, true);
    profile.setColumnVisible(MarketWatchColumn::WEEK_52_LOW, true);
    
    // Hide F&O columns
    profile.setColumnVisible(MarketWatchColumn::STRIKE_PRICE, false);
    profile.setColumnVisible(MarketWatchColumn::OPTION_TYPE, false);
    profile.setColumnVisible(MarketWatchColumn::SERIES_EXPIRY, false);
    profile.setColumnVisible(MarketWatchColumn::OPEN_INTEREST, false);
    profile.setColumnVisible(MarketWatchColumn::OI_CHANGE_PERCENT, false);
    
    return profile;
}

MarketWatchColumnProfile MarketWatchColumnProfile::createTradingProfile()
{
    MarketWatchColumnProfile profile("Trading");
    profile.setDescription("Active trading with depth");
    
    // Start with default
    profile = createDefaultProfile();
    profile.setName("Trading");
    
    // Add depth and trading columns
    profile.setColumnVisible(MarketWatchColumn::BUY_PRICE, true);
    profile.setColumnVisible(MarketWatchColumn::BUY_QTY, true);
    profile.setColumnVisible(MarketWatchColumn::SELL_PRICE, true);
    profile.setColumnVisible(MarketWatchColumn::SELL_QTY, true);
    profile.setColumnVisible(MarketWatchColumn::TOTAL_BUY_QTY, true);
    profile.setColumnVisible(MarketWatchColumn::TOTAL_SELL_QTY, true);
    profile.setColumnVisible(MarketWatchColumn::AVG_TRADED_PRICE, true);
    profile.setColumnVisible(MarketWatchColumn::LAST_TRADED_QUANTITY, true);
    profile.setColumnVisible(MarketWatchColumn::LAST_TRADED_TIME, true);
    
    return profile;
}

// Serialization
QJsonObject MarketWatchColumnProfile::toJson() const
{
    QJsonObject json;
    json["name"] = m_name;
    json["description"] = m_description;
    
    // Visibility map
    QJsonObject visibility;
    for (auto it = m_visibility.constBegin(); it != m_visibility.constEnd(); ++it) {
        visibility[QString::number(static_cast<int>(it.key()))] = it.value();
    }
    json["visibility"] = visibility;
    
    // Widths map
    QJsonObject widths;
    for (auto it = m_widths.constBegin(); it != m_widths.constEnd(); ++it) {
        widths[QString::number(static_cast<int>(it.key()))] = it.value();
    }
    json["widths"] = widths;
    
    // Column order
    QJsonArray order;
    for (MarketWatchColumn col : m_columnOrder) {
        order.append(static_cast<int>(col));
    }
    json["columnOrder"] = order;
    
    return json;
}

bool MarketWatchColumnProfile::fromJson(const QJsonObject &json)
{
    if (!json.contains("name")) return false;
    
    m_name = json["name"].toString();
    m_description = json.value("description").toString();
    
    // Visibility
    if (json.contains("visibility")) {
        QJsonObject visibility = json["visibility"].toObject();
        m_visibility.clear();
        for (auto it = visibility.constBegin(); it != visibility.constEnd(); ++it) {
            int colIndex = it.key().toInt();
            m_visibility[static_cast<MarketWatchColumn>(colIndex)] = it.value().toBool();
        }
    }
    
    // Widths
    if (json.contains("widths")) {
        QJsonObject widths = json["widths"].toObject();
        m_widths.clear();
        for (auto it = widths.constBegin(); it != widths.constEnd(); ++it) {
            int colIndex = it.key().toInt();
            m_widths[static_cast<MarketWatchColumn>(colIndex)] = it.value().toInt();
        }
    }
    
    // Column order
    if (json.contains("columnOrder")) {
        QJsonArray order = json["columnOrder"].toArray();
        m_columnOrder.clear();
        for (const QJsonValue &val : order) {
            m_columnOrder.append(static_cast<MarketWatchColumn>(val.toInt()));
        }
    }
    
    return true;
}

bool MarketWatchColumnProfile::saveToFile(const QString &filepath) const
{
    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to save profile to" << filepath;
        return false;
    }
    
    QJsonDocument doc(toJson());
    file.write(doc.toJson());
    file.close();
    return true;
}

bool MarketWatchColumnProfile::loadFromFile(const QString &filepath)
{
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to load profile from" << filepath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON in profile file" << filepath;
        return false;
    }
    
    return fromJson(doc.object());
}

// Profile Manager Implementation
MarketWatchProfileManager& MarketWatchProfileManager::instance()
{
    static MarketWatchProfileManager instance;
    return instance;
}

MarketWatchProfileManager::MarketWatchProfileManager()
    : m_currentProfileName("Default")
{
    loadDefaultProfiles();
    loadAllProfiles("profiles/marketwatch");
}

void MarketWatchProfileManager::addProfile(const MarketWatchColumnProfile &profile)
{
    m_profiles[profile.name()] = profile;
    saveAllProfiles("profiles/marketwatch");
}

bool MarketWatchProfileManager::removeProfile(const QString &name)
{
    if (!m_profiles.contains(name)) return false;
    
    // Delete file
    QDir dir("profiles/marketwatch");
    dir.remove(name + ".json");
    
    m_profiles.remove(name);
    if (m_currentProfileName == name) {
        m_currentProfileName = "Default";
    }
    return true;
}

MarketWatchColumnProfile MarketWatchProfileManager::getProfile(const QString &name) const
{
    return m_profiles.value(name, MarketWatchColumnProfile::createDefaultProfile());
}

QStringList MarketWatchProfileManager::profileNames() const
{
    return m_profiles.keys();
}

bool MarketWatchProfileManager::hasProfile(const QString &name) const
{
    return m_profiles.contains(name);
}

void MarketWatchProfileManager::setCurrentProfile(const QString &name)
{
    if (m_profiles.contains(name)) {
        m_currentProfileName = name;
    }
}

MarketWatchColumnProfile MarketWatchProfileManager::currentProfile() const
{
    return getProfile(m_currentProfileName);
}

bool MarketWatchProfileManager::saveAllProfiles(const QString &directory) const
{
    QDir dir(directory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    bool allSuccess = true;
    for (const MarketWatchColumnProfile &profile : m_profiles) {
        QString filename = dir.filePath(profile.name() + ".json");
        if (!profile.saveToFile(filename)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool MarketWatchProfileManager::loadAllProfiles(const QString &directory)
{
    QDir dir(directory);
    if (!dir.exists()) {
        qWarning() << "Profile directory does not exist:" << directory;
        return false;
    }
    
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo &fileInfo : files) {
        MarketWatchColumnProfile profile;
        if (profile.loadFromFile(fileInfo.absoluteFilePath())) {
            addProfile(profile);
        }
    }
    
    return !m_profiles.isEmpty();
}

void MarketWatchProfileManager::loadDefaultProfiles()
{
    addProfile(MarketWatchColumnProfile::createDefaultProfile());
    addProfile(MarketWatchColumnProfile::createCompactProfile());
    addProfile(MarketWatchColumnProfile::createDetailedProfile());
    addProfile(MarketWatchColumnProfile::createFOProfile());
    addProfile(MarketWatchColumnProfile::createEquityProfile());
    addProfile(MarketWatchColumnProfile::createTradingProfile());
}
