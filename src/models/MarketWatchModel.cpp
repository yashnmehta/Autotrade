#include "models/MarketWatchModel.h"
#include "models/MarketWatchColumnProfile.h"
#include <QColor>
#include <QFont>
#include <QDebug>

MarketWatchModel::MarketWatchModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    // Initialize with default column profile
    m_columnProfile = MarketWatchColumnProfile::createDefaultProfile();
}

int MarketWatchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_scrips.count();
}

int MarketWatchModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_columnProfile.visibleColumnCount();
}

QVariant MarketWatchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_scrips.count())
        return QVariant();

    const ScripData &scrip = m_scrips.at(index.row());
    
    // Special handling for blank rows
    if (scrip.isBlankRow) {
        if (role == Qt::DisplayRole && index.column() == 0) {
            return "───────────────";  // Show separator
        }
        if (role == Qt::UserRole + 100) {
            return true;  // Mark as blank row for delegate
        }
        return QVariant();
    }

    // Get the column enum for this index
    QList<MarketWatchColumn> visibleCols = m_columnProfile.visibleColumns();
    if (index.column() < 0 || index.column() >= visibleCols.size())
        return QVariant();
    
    MarketWatchColumn column = visibleCols.at(index.column());

    // Display role - show formatted data
    if (role == Qt::DisplayRole) {
        return formatColumnData(scrip, column);
    }
    
    // Text alignment
    else if (role == Qt::TextAlignmentRole) {
        ColumnInfo info = MarketWatchColumnProfile::getColumnInfo(column);
        return static_cast<int>(info.alignment);
    }
    
    // User role - return raw data for sorting
    else if (role == Qt::UserRole) {
        return getColumnData(scrip, column);
    }
    
    // User role + 1 - return token for lookups
    else if (role == Qt::UserRole + 1) {
        return scrip.token;
    }
    
    // User role + 2 - return exchange
    else if (role == Qt::UserRole + 2) {
        return scrip.exchange;
    }

    return QVariant();
}

QVariant MarketWatchModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        QList<MarketWatchColumn> visibleCols = m_columnProfile.visibleColumns();
        if (section < 0 || section >= visibleCols.size())
            return QVariant();
        
        MarketWatchColumn col = visibleCols.at(section);
        ColumnInfo info = MarketWatchColumnProfile::getColumnInfo(col);
        
        if (role == Qt::DisplayRole) {
            return info.name;
        }
        else if (role == Qt::TextAlignmentRole) {
            return static_cast<int>(info.alignment);
        }
        else if (role == Qt::ToolTipRole) {
            return info.description;
        }
    }
    else if (orientation == Qt::Vertical && role == Qt::DisplayRole) {
        return section + 1;  // Row numbers
    }
    
    return QVariant();
}

void MarketWatchModel::addScrip(const ScripData &scrip)
{
    int row = m_scrips.count();
    beginInsertRows(QModelIndex(), row, row);
    m_scrips.append(scrip);
    endInsertRows();
    
    emit scripAdded(row, scrip);
    
    qDebug() << "[MarketWatchModel] Added scrip:" << scrip.symbol
             << "Token:" << scrip.token << "at row" << row;
}

void MarketWatchModel::insertScrip(int position, const ScripData &scrip)
{
    // Validate position
    if (position < 0) position = 0;
    if (position > m_scrips.count()) position = m_scrips.count();
    
    beginInsertRows(QModelIndex(), position, position);
    m_scrips.insert(position, scrip);
    endInsertRows();
    
    emit scripAdded(position, scrip);
    
    qDebug() << "[MarketWatchModel] Inserted scrip:" << scrip.symbol
             << "Token:" << scrip.token << "at position" << position;
}

void MarketWatchModel::removeScrip(int row)
{
    if (row >= 0 && row < m_scrips.count()) {
        beginRemoveRows(QModelIndex(), row, row);
        
        const ScripData &scrip = m_scrips.at(row);
        qDebug() << "[MarketWatchModel] Removing scrip:" << scrip.symbol
                 << "Token:" << scrip.token << "from row" << row;
        
        m_scrips.removeAt(row);
        endRemoveRows();
        
        emit scripRemoved(row);
    }
}

void MarketWatchModel::moveRow(int sourceRow, int targetRow)
{
    if (sourceRow < 0 || sourceRow >= m_scrips.count()) return;
    if (targetRow < 0 || targetRow > m_scrips.count()) return;
    if (sourceRow == targetRow) return;
    
    // Qt's moveRows expects destination to be the position AFTER the move
    // If moving down, targetRow is already correct
    // If moving up, targetRow should be the position before the item
    int destRow = targetRow;
    if (sourceRow < targetRow) {
        destRow = targetRow;
    }
    
    // Use Qt's beginMoveRows to notify views
    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow, QModelIndex(), destRow)) {
        qDebug() << "[MarketWatchModel] moveRow failed: invalid move from" << sourceRow << "to" << targetRow;
        return;
    }
    
    // Perform the actual move
    ScripData scrip = m_scrips.takeAt(sourceRow);
    int insertPos = targetRow;
    if (sourceRow < targetRow) {
        insertPos = targetRow - 1;
    }
    m_scrips.insert(insertPos, scrip);
    
    endMoveRows();
    
    qDebug() << "[MarketWatchModel] Moved row from" << sourceRow << "to" << insertPos;
}

void MarketWatchModel::clearAll()
{
    beginResetModel();
    m_scrips.clear();
    endResetModel();
    
    qDebug() << "[MarketWatchModel] Cleared all scrips";
}

int MarketWatchModel::findScrip(const QString &symbol) const
{
    for (int i = 0; i < m_scrips.count(); ++i) {
        if (m_scrips.at(i).symbol == symbol && !m_scrips.at(i).isBlankRow)
            return i;
    }
    return -1;
}

int MarketWatchModel::findScripByToken(int token) const
{
    if (token <= 0) return -1;
    
    for (int i = 0; i < m_scrips.count(); ++i) {
        if (m_scrips.at(i).token == token && !m_scrips.at(i).isBlankRow)
            return i;
    }
    return -1;
}

void MarketWatchModel::insertBlankRow(int position)
{
    if (position < 0 || position > m_scrips.count()) {
        position = m_scrips.count();  // Append at end
    }
    
    beginInsertRows(QModelIndex(), position, position);
    m_scrips.insert(position, ScripData::createBlankRow());
    endInsertRows();
    
    qDebug() << "[MarketWatchModel] Inserted blank row at position" << position;
}

bool MarketWatchModel::isBlankRow(int row) const
{
    if (row >= 0 && row < m_scrips.count()) {
        return m_scrips.at(row).isBlankRow;
    }
    return false;
}

const ScripData& MarketWatchModel::getScripAt(int row) const
{
    static const ScripData emptyData;
    if (row >= 0 && row < m_scrips.count()) {
        return m_scrips.at(row);
    }
    return emptyData;
}

ScripData& MarketWatchModel::getScripAt(int row)
{
    static ScripData emptyData;
    if (row >= 0 && row < m_scrips.count()) {
        return m_scrips[row];
    }
    return emptyData;
}

void MarketWatchModel::updatePrice(int row, double ltp, double change, double changePercent)
{
    if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
        ScripData &scrip = m_scrips[row];
        scrip.ltp = ltp;
        scrip.change = change;
        scrip.changePercent = changePercent;
        
        // Emit data changed for affected columns
        emit dataChanged(index(row, COL_LTP), index(row, COL_CHANGE_PERCENT));
        emit priceUpdated(row, ltp, change);
    }
}

void MarketWatchModel::updateVolume(int row, qint64 volume)
{
    if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
        m_scrips[row].volume = volume;
        emitCellChanged(row, COL_VOLUME);
    }
}

void MarketWatchModel::updateBidAsk(int row, double bid, double ask)
{
    if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
        m_scrips[row].bid = bid;
        m_scrips[row].ask = ask;
        emit dataChanged(index(row, COL_BID), index(row, COL_ASK));
    }
}

void MarketWatchModel::updateHighLow(int row, double high, double low)
{
    if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
        m_scrips[row].high = high;
        m_scrips[row].low = low;
        emit dataChanged(index(row, COL_HIGH), index(row, COL_LOW));
    }
}

void MarketWatchModel::updateOpenInterest(int row, qint64 oi)
{
    if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
        m_scrips[row].openInterest = oi;
        emitCellChanged(row, COL_OPEN_INTEREST);
    }
}

void MarketWatchModel::updateScripData(int row, const ScripData &scrip)
{
    if (row >= 0 && row < m_scrips.count()) {
        m_scrips[row] = scrip;
        emit dataChanged(index(row, 0), index(row, COL_COUNT - 1));
    }
}

int MarketWatchModel::scripCount() const
{
    int count = 0;
    for (const ScripData &scrip : m_scrips) {
        if (!scrip.isBlankRow) {
            ++count;
        }
    }
    return count;
}

void MarketWatchModel::emitCellChanged(int row, int column)
{
    QModelIndex idx = index(row, column);
    emit dataChanged(idx, idx);
}

// ============================================================================
// Column Profile Management
// ============================================================================

void MarketWatchModel::setColumnProfile(const MarketWatchColumnProfile& profile)
{
    beginResetModel();
    m_columnProfile = profile;
    endResetModel();
    qDebug() << "[MarketWatchModel] Column profile updated:" << profile.name();
}

void MarketWatchModel::loadProfile(const QString& profileName)
{
    MarketWatchProfileManager& manager = MarketWatchProfileManager::instance();
    
    if (manager.hasProfile(profileName)) {
        setColumnProfile(manager.getProfile(profileName));
    } else {
        qDebug() << "[MarketWatchModel] Profile not found:" << profileName;
    }
}

void MarketWatchModel::saveProfile(const QString& profileName)
{
    MarketWatchProfileManager& manager = MarketWatchProfileManager::instance();
    
    MarketWatchColumnProfile profile = m_columnProfile;
    profile.setName(profileName);
    
    manager.addProfile(profile);
    qDebug() << "[MarketWatchModel] Profile saved:" << profileName;
}

QStringList MarketWatchModel::getAvailableProfiles() const
{
    return MarketWatchProfileManager::instance().profileNames();
}

// ============================================================================
// Column Data Helpers
// ============================================================================

QVariant MarketWatchModel::getColumnData(const ScripData& scrip, MarketWatchColumn column) const
{
    switch (column) {
        case MarketWatchColumn::CODE: return scrip.code;
        case MarketWatchColumn::SYMBOL: return scrip.symbol;
        case MarketWatchColumn::SCRIP_NAME: return scrip.scripName;
        case MarketWatchColumn::INSTRUMENT_NAME: return scrip.instrumentName;
        case MarketWatchColumn::INSTRUMENT_TYPE: return scrip.instrumentType;
        case MarketWatchColumn::MARKET_TYPE: return scrip.marketType;
        case MarketWatchColumn::EXCHANGE: return scrip.exchange;
        case MarketWatchColumn::STRIKE_PRICE: return scrip.strikePrice;
        case MarketWatchColumn::OPTION_TYPE: return scrip.optionType;
        case MarketWatchColumn::SERIES_EXPIRY: return scrip.seriesExpiry;
        case MarketWatchColumn::ISIN_CODE: return scrip.isinCode;
        case MarketWatchColumn::LAST_TRADED_PRICE: return scrip.ltp;
        case MarketWatchColumn::LAST_TRADED_QUANTITY: return static_cast<qlonglong>(scrip.ltq);
        case MarketWatchColumn::LAST_TRADED_TIME: return scrip.ltpTime;
        case MarketWatchColumn::LAST_UPDATE_TIME: return scrip.lastUpdateTime;
        case MarketWatchColumn::OPEN: return scrip.open;
        case MarketWatchColumn::HIGH: return scrip.high;
        case MarketWatchColumn::LOW: return scrip.low;
        case MarketWatchColumn::CLOSE: return scrip.close;
        case MarketWatchColumn::DPR: return scrip.dpr;
        case MarketWatchColumn::NET_CHANGE_RS: return scrip.change;
        case MarketWatchColumn::PERCENT_CHANGE: return scrip.changePercent;
        case MarketWatchColumn::TREND_INDICATOR: return scrip.trendIndicator;
        case MarketWatchColumn::AVG_TRADED_PRICE: return scrip.avgTradedPrice;
        case MarketWatchColumn::VOLUME: return static_cast<qlonglong>(scrip.volume);
        case MarketWatchColumn::VALUE: return scrip.value;
        case MarketWatchColumn::BUY_PRICE: return scrip.buyPrice;
        case MarketWatchColumn::BUY_QTY: return static_cast<qlonglong>(scrip.buyQty);
        case MarketWatchColumn::TOTAL_BUY_QTY: return static_cast<qlonglong>(scrip.totalBuyQty);
        case MarketWatchColumn::SELL_PRICE: return scrip.sellPrice;
        case MarketWatchColumn::SELL_QTY: return static_cast<qlonglong>(scrip.sellQty);
        case MarketWatchColumn::TOTAL_SELL_QTY: return static_cast<qlonglong>(scrip.totalSellQty);
        case MarketWatchColumn::OPEN_INTEREST: return static_cast<qlonglong>(scrip.openInterest);
        case MarketWatchColumn::OI_CHANGE_PERCENT: return scrip.oiChangePercent;
        case MarketWatchColumn::WEEK_52_HIGH: return scrip.week52High;
        case MarketWatchColumn::WEEK_52_LOW: return scrip.week52Low;
        case MarketWatchColumn::LIFETIME_HIGH: return scrip.lifetimeHigh;
        case MarketWatchColumn::LIFETIME_LOW: return scrip.lifetimeLow;
        case MarketWatchColumn::MARKET_CAP: return scrip.marketCap;
        case MarketWatchColumn::TRADE_EXECUTION_RANGE: return scrip.tradeExecutionRange;
        default: return QVariant();
    }
}

QString MarketWatchModel::formatColumnData(const ScripData& scrip, MarketWatchColumn column) const
{
    ColumnInfo info = MarketWatchColumnProfile::getColumnInfo(column);
    QString format = info.format;
    QString units = info.unit;
    
    switch (column) {
        case MarketWatchColumn::CODE:
            return QString::number(scrip.code);
            
        case MarketWatchColumn::SYMBOL:
        case MarketWatchColumn::SCRIP_NAME:
        case MarketWatchColumn::INSTRUMENT_NAME:
        case MarketWatchColumn::INSTRUMENT_TYPE:
        case MarketWatchColumn::MARKET_TYPE:
        case MarketWatchColumn::EXCHANGE:
        case MarketWatchColumn::OPTION_TYPE:
        case MarketWatchColumn::SERIES_EXPIRY:
        case MarketWatchColumn::ISIN_CODE:
        case MarketWatchColumn::DPR:
        case MarketWatchColumn::TREND_INDICATOR:
        case MarketWatchColumn::TRADE_EXECUTION_RANGE:
            return getColumnData(scrip, column).toString();
            
        case MarketWatchColumn::STRIKE_PRICE:
        case MarketWatchColumn::LAST_TRADED_PRICE:
        case MarketWatchColumn::OPEN:
        case MarketWatchColumn::HIGH:
        case MarketWatchColumn::LOW:
        case MarketWatchColumn::CLOSE:
        case MarketWatchColumn::NET_CHANGE_RS:
        case MarketWatchColumn::AVG_TRADED_PRICE:
        case MarketWatchColumn::BUY_PRICE:
        case MarketWatchColumn::SELL_PRICE:
        case MarketWatchColumn::WEEK_52_HIGH:
        case MarketWatchColumn::WEEK_52_LOW:
        case MarketWatchColumn::LIFETIME_HIGH:
        case MarketWatchColumn::LIFETIME_LOW: {
            double value = getColumnData(scrip, column).toDouble();
            if (value == 0.0) return "-";
            return QString::number(value, 'f', 2) + (units.isEmpty() ? "" : " " + units);
        }
        
        case MarketWatchColumn::PERCENT_CHANGE:
        case MarketWatchColumn::OI_CHANGE_PERCENT: {
            double value = getColumnData(scrip, column).toDouble();
            if (value == 0.0) return "-";
            QString sign = (value > 0) ? "+" : "";
            return sign + QString::number(value, 'f', 2) + "%";
        }
        
        case MarketWatchColumn::VOLUME: {
            qint64 vol = getColumnData(scrip, column).toLongLong();
            if (vol == 0) return "-";
            return QString::number(vol / 1000.0, 'f', 2) + " K";  // in 000s
        }
        
        case MarketWatchColumn::VALUE: {
            double val = getColumnData(scrip, column).toDouble();
            if (val == 0.0) return "-";
            return QString::number(val / 100000.0, 'f', 2) + " L";  // in lacs
        }
        
        case MarketWatchColumn::MARKET_CAP: {
            double cap = getColumnData(scrip, column).toDouble();
            if (cap == 0.0) return "-";
            if (cap >= 10000000.0) {
                return QString::number(cap / 10000000.0, 'f', 2) + " Cr";
            }
            return QString::number(cap / 100000.0, 'f', 2) + " L";
        }
        
        case MarketWatchColumn::LAST_TRADED_QUANTITY:
        case MarketWatchColumn::BUY_QTY:
        case MarketWatchColumn::SELL_QTY:
        case MarketWatchColumn::TOTAL_BUY_QTY:
        case MarketWatchColumn::TOTAL_SELL_QTY:
        case MarketWatchColumn::OPEN_INTEREST: {
            qint64 value = getColumnData(scrip, column).toLongLong();
            if (value == 0) return "-";
            return QString::number(value) + (units.isEmpty() ? "" : " " + units);
        }
        
        case MarketWatchColumn::LAST_TRADED_TIME:
        case MarketWatchColumn::LAST_UPDATE_TIME:
            return getColumnData(scrip, column).toString();
            
        default:
            return "-";
    }
}
