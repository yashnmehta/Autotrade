#include "models/MarketWatchColumnProfile.h"
#include <QDebug>

// Static member initialization
QMap<MarketWatchColumn, ColumnInfo> MarketWatchColumnProfile::s_columnMetadata;

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
    
    // Greeks (Options only)
    info = ColumnInfo();
    info.id = MarketWatchColumn::IMPLIED_VOLATILITY;
    info.name = "IV";
    info.shortName = "IV";
    info.description = "Implied Volatility %";
    info.defaultWidth = 65;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.1f";
    info.unit = "%";
    info.isNumeric = true;
    info.isFOSpecific = true;
    s_columnMetadata[MarketWatchColumn::IMPLIED_VOLATILITY] = info;

    info = ColumnInfo();
    info.id = MarketWatchColumn::BID_IV;
    info.name = "Bid IV";
    info.shortName = "BidIV";
    info.description = "Bid Implied Volatility %";
    info.defaultWidth = 65;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.1f";
    info.unit = "%";
    info.isNumeric = true;
    info.isFOSpecific = true;
    s_columnMetadata[MarketWatchColumn::BID_IV] = info;

    info = ColumnInfo();
    info.id = MarketWatchColumn::ASK_IV;
    info.name = "Ask IV";
    info.shortName = "AskIV";
    info.description = "Ask Implied Volatility %";
    info.defaultWidth = 65;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.1f";
    info.unit = "%";
    info.isNumeric = true;
    info.isFOSpecific = true;
    s_columnMetadata[MarketWatchColumn::ASK_IV] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::DELTA;
    info.name = "Delta";
    info.shortName = "Δ";
    info.description = "Option Delta";
    info.defaultWidth = 60;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.3f";
    info.isNumeric = true;
    info.isFOSpecific = true;
    s_columnMetadata[MarketWatchColumn::DELTA] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::GAMMA;
    info.name = "Gamma";
    info.shortName = "Γ";
    info.description = "Option Gamma";
    info.defaultWidth = 65;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.5f";
    info.isNumeric = true;
    info.isFOSpecific = true;
    s_columnMetadata[MarketWatchColumn::GAMMA] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::VEGA;
    info.name = "Vega";
    info.shortName = "ν";
    info.description = "Option Vega (per 1% IV)";
    info.defaultWidth = 65;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.2f";
    info.isNumeric = true;
    info.isFOSpecific = true;
    s_columnMetadata[MarketWatchColumn::VEGA] = info;
    
    info = ColumnInfo();
    info.id = MarketWatchColumn::THETA;
    info.name = "Theta";
    info.shortName = "Θ";
    info.description = "Option Theta (daily decay)";
    info.defaultWidth = 65;
    info.alignment = Qt::AlignRight | Qt::AlignVCenter;
    info.visibleByDefault = false;
    info.format = "%.2f";
    info.isNumeric = true;
    info.isFOSpecific = true;
    s_columnMetadata[MarketWatchColumn::THETA] = info;
    
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

// ── Bridge to generic profile system ────────────────────────────────────

QList<GenericColumnInfo> MarketWatchColumnProfile::buildGenericMetadata()
{
    initializeColumnMetadata();
    QList<GenericColumnInfo> meta;
    for (int i = 0; i < static_cast<int>(MarketWatchColumn::COLUMN_COUNT); ++i) {
        MarketWatchColumn col = static_cast<MarketWatchColumn>(i);
        const ColumnInfo &ci = s_columnMetadata[col];
        GenericColumnInfo gi;
        gi.id = i;
        gi.name = ci.name;
        gi.shortName = ci.shortName;
        gi.description = ci.description;
        gi.defaultWidth = ci.defaultWidth;
        gi.alignment = ci.alignment;
        gi.visibleByDefault = ci.visibleByDefault;
        gi.isNumeric = ci.isNumeric;
        meta.append(gi);
    }
    return meta;
}

// Preset Profiles (return GenericTableProfile)

GenericTableProfile MarketWatchColumnProfile::createDefaultProfile()
{
    auto p = GenericTableProfile::createDefault(buildGenericMetadata());
    p.setName("Default");
    p.setDescription("Standard market watch view");
    return p;
}

GenericTableProfile MarketWatchColumnProfile::createCompactProfile()
{
    auto p = createDefaultProfile();
    p.setName("Compact");
    p.setDescription("Minimal columns for compact display");

    // Hide all columns first
    for (int i = 0; i < static_cast<int>(MarketWatchColumn::COLUMN_COUNT); ++i)
        p.setColumnVisible(i, false);

    // Show only essentials
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::SYMBOL), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::LAST_TRADED_PRICE), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::NET_CHANGE_RS), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::PERCENT_CHANGE), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::VOLUME), true);
    return p;
}

GenericTableProfile MarketWatchColumnProfile::createDetailedProfile()
{
    auto p = createDefaultProfile();
    p.setName("Detailed");
    p.setDescription("All important columns");

    for (int i = 0; i < static_cast<int>(MarketWatchColumn::COLUMN_COUNT); ++i) {
        auto col = static_cast<MarketWatchColumn>(i);
        if (col != MarketWatchColumn::MARKET_TYPE &&
            col != MarketWatchColumn::ISIN_CODE &&
            col != MarketWatchColumn::TRADE_EXECUTION_RANGE) {
            p.setColumnVisible(i, true);
        }
    }
    return p;
}

GenericTableProfile MarketWatchColumnProfile::createFOProfile()
{
    auto p = createDefaultProfile();
    p.setName("F&O");
    p.setDescription("Futures & Options focused");

    p.setColumnVisible(static_cast<int>(MarketWatchColumn::STRIKE_PRICE), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::OPTION_TYPE), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::SERIES_EXPIRY), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::OPEN_INTEREST), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::OI_CHANGE_PERCENT), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::IMPLIED_VOLATILITY), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::BID_IV), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::ASK_IV), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::DELTA), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::THETA), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::GAMMA), false);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::VEGA), false);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::MARKET_CAP), false);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::WEEK_52_HIGH), false);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::WEEK_52_LOW), false);
    return p;
}

GenericTableProfile MarketWatchColumnProfile::createEquityProfile()
{
    auto p = createDefaultProfile();
    p.setName("Equity");
    p.setDescription("Equity/Cash market focused");

    p.setColumnVisible(static_cast<int>(MarketWatchColumn::MARKET_CAP), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::WEEK_52_HIGH), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::WEEK_52_LOW), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::STRIKE_PRICE), false);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::OPTION_TYPE), false);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::SERIES_EXPIRY), false);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::OPEN_INTEREST), false);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::OI_CHANGE_PERCENT), false);
    return p;
}

GenericTableProfile MarketWatchColumnProfile::createTradingProfile()
{
    auto p = createDefaultProfile();
    p.setName("Trading");
    p.setDescription("Active trading with depth");

    p.setColumnVisible(static_cast<int>(MarketWatchColumn::BUY_PRICE), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::BUY_QTY), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::SELL_PRICE), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::SELL_QTY), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::TOTAL_BUY_QTY), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::TOTAL_SELL_QTY), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::AVG_TRADED_PRICE), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::LAST_TRADED_QUANTITY), true);
    p.setColumnVisible(static_cast<int>(MarketWatchColumn::LAST_TRADED_TIME), true);
    return p;
}

void MarketWatchColumnProfile::registerPresets(GenericProfileManager &mgr)
{
    mgr.addPreset(createDefaultProfile());
    mgr.addPreset(createCompactProfile());
    mgr.addPreset(createDetailedProfile());
    mgr.addPreset(createFOProfile());
    mgr.addPreset(createEquityProfile());
    mgr.addPreset(createTradingProfile());
}


