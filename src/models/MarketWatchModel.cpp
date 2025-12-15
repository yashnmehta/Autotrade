#include "models/MarketWatchModel.h"
#include <QColor>
#include <QFont>
#include <QDebug>

MarketWatchModel::MarketWatchModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    // Initialize column headers
    m_headers << "Symbol" << "LTP" << "Change" << "%Change" << "Volume"
              << "Bid" << "Ask" << "High" << "Low" << "Open" << "OI";
}

int MarketWatchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_scrips.count();
}

int MarketWatchModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : COL_COUNT;
}

QVariant MarketWatchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_scrips.count())
        return QVariant();

    const ScripData &scrip = m_scrips.at(index.row());
    
    // Special handling for blank rows
    if (scrip.isBlankRow) {
        if (role == Qt::DisplayRole && index.column() == COL_SYMBOL) {
            return scrip.symbol;  // Show separator
        }
        if (role == Qt::UserRole + 100) {
            return true;  // Mark as blank row for delegate
        }
        return QVariant();
    }

    // Display role - show formatted data
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case COL_SYMBOL:
                return scrip.symbol;
            case COL_LTP:
                return QString::number(scrip.ltp, 'f', 2);
            case COL_CHANGE:
                return QString::number(scrip.change, 'f', 2);
            case COL_CHANGE_PERCENT:
                return QString::number(scrip.changePercent, 'f', 2) + "%";
            case COL_VOLUME:
                return QString::number(scrip.volume);
            case COL_BID:
                return QString::number(scrip.bid, 'f', 2);
            case COL_ASK:
                return QString::number(scrip.ask, 'f', 2);
            case COL_HIGH:
                return QString::number(scrip.high, 'f', 2);
            case COL_LOW:
                return QString::number(scrip.low, 'f', 2);
            case COL_OPEN:
                return QString::number(scrip.open, 'f', 2);
            case COL_OPEN_INTEREST:
                return QString::number(scrip.openInterest);
            default:
                return QVariant();
        }
    }
    
    // Text alignment
    else if (role == Qt::TextAlignmentRole) {
        if (index.column() == COL_SYMBOL)
            return Qt::AlignLeft + Qt::AlignVCenter;
        return Qt::AlignRight + Qt::AlignVCenter;
    }
    
    // User role - return raw data for sorting (including string for symbol)
    else if (role == Qt::UserRole) {
        switch (index.column()) {
            case COL_SYMBOL: return scrip.symbol;  // Return string for alphabetic sorting
            case COL_LTP: return scrip.ltp;
            case COL_CHANGE: return scrip.change;
            case COL_CHANGE_PERCENT: return scrip.changePercent;
            case COL_VOLUME: return static_cast<qlonglong>(scrip.volume);
            case COL_BID: return scrip.bid;
            case COL_ASK: return scrip.ask;
            case COL_HIGH: return scrip.high;
            case COL_LOW: return scrip.low;
            case COL_OPEN: return scrip.open;
            case COL_OPEN_INTEREST: return static_cast<qlonglong>(scrip.openInterest);
            default: return QVariant();
        }
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
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section >= 0 && section < m_headers.count())
            return m_headers.at(section);
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
