#include "models/TradeModel.h"
#include <QColor>
#include <QBrush>
#include <QFont>
#include <QDateTime>
#include <QSet>
#include <QMap>
#include "repository/RepositoryManager.h"



TradeModel::TradeModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_filterRowVisible(false)
{
    m_headers = QStringList({
        "User", "Group", "Exchange Code", "MemberId", "TraderId", "Instrument Type",
        "Instrument Name", "Code", "Symbol/ScripId", "Spread Symbol", "Ser/Exp/Group",
        "Strike Price", "Option Type", "Order Type", "B/S", "Quantity", "Price",
        "Time", "Spread Price", "Spread", "Pro/Cli", "Client", "Client Name",
        "Exch. Order No.", "Trade No.", "Settlor", "User Remarks", "New Client",
        "Part Type", "Product Type", "Order Entry Time", "Client Order No.",
        "Order Initiated From", "Order Modified From", "Misc.", "Strategy",
        "Mapping", "OMSID", "GiveUp Status", "EOMSID", "Booking Ref.",
        "Dealing Instruction", "Order Instruction", "Lots", "Alias Settlor",
        "Alias PartType", "New Participant Code", "Initiated From User Id",
        "Modified From User Id", "SOR Id", "New Settlor", "Maturity Date",
        "Yield", "Mapping Order Type", "Algo Type", "Algo Description", "Scrip Name"
    });
}

int TradeModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_trades.size() + (m_filterRowVisible ? 1 : 0);
}

int TradeModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant TradeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return QVariant();
    int row = index.row();
    int col = index.column();

    if (m_filterRowVisible && row == 0) {
        if (role == Qt::BackgroundRole) return QColor(240, 248, 255);
        if (role == Qt::UserRole) return "FILTER_ROW";
        return QVariant();
    }

    int dataRow = m_filterRowVisible ? row - 1 : row;
    if (dataRow < 0 || dataRow >= m_trades.size()) return QVariant();
    const XTS::Trade& trade = m_trades.at(dataRow);

    if (role == Qt::DisplayRole) {
        switch (col) {
            case User: return trade.loginID;
            case Group: return "DEFAULT";
            case ExchangeCode: {
                bool isNumeric;
                int segId = trade.exchangeSegment.toInt(&isNumeric);
                if (isNumeric) return RepositoryManager::getExchangeSegmentName(segId);
                return trade.exchangeSegment;
            }
            case MemberId: return "1";
            case TraderId: return trade.loginID;
            case InstrumentType: {
                bool isNumeric;
                int segId = trade.exchangeSegment.toInt(&isNumeric);
                if (isNumeric) return RepositoryManager::getExchangeSegmentName(segId);
                return trade.exchangeSegment;
            }

            case InstrumentName: return trade.tradingSymbol;
            case Code: return QString::number(trade.exchangeInstrumentID);
            case Symbol: return trade.tradingSymbol;
            case StrikePrice: return (trade.exchangeSegment.contains("FO")) ? QString::number(trade.orderPrice, 'f', 2) : ""; // Placeholder
            case OrderType: return trade.orderType;
            case BuySell: return trade.orderSide;
            case Quantity: return QString::number(trade.lastTradedQuantity);
            case Price: return QString::number(trade.lastTradedPrice, 'f', 2);
            case Time: return trade.lastExecutionTransactTime;
            case ProCli: return "CLI";
            case Client: return trade.clientID.isEmpty() ? "PRO7" : trade.clientID;
            case ExchOrdNo: return trade.exchangeOrderID;
            case TradeNo: return trade.executionID;
            case UserRemarks: return trade.orderUniqueIdentifier;
            case ProductType: return trade.productType;
            case OrderEntryTime: return trade.orderGeneratedDateTime;
            case ClientOrderNo: return QString::number(trade.appOrderID);
            case ScripName: return trade.tradingSymbol;
            default: return QString();
        }
    } else if (role == Qt::UserRole) {
        switch (col) {
            case Code: return (qlonglong)trade.exchangeInstrumentID;
            case Quantity: return trade.lastTradedQuantity;
            case Price: return trade.lastTradedPrice;
            case Time: return QDateTime::fromString(trade.lastExecutionTransactTime, "dd-MM-yyyy HH:mm:ss");
            case ClientOrderNo: return (qlonglong)trade.appOrderID;
            default: return QVariant();
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (col == Quantity || col == Price) return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    } else if (role == Qt::BackgroundRole) {
        QString side = trade.orderSide.trimmed().toUpper();
        if (side == "BUY") return QColor("#88a9c1ff"); // Light Blue
        if (side == "SELL") return QColor("#eeb9c1ff"); // Light Red
        return QVariant();
    } else if (role == Qt::ForegroundRole) {
        if (col == BuySell) return (trade.orderSide.trimmed().toUpper() == "BUY") ? QColor("#0d47a1") : QColor("#b71c1c");
        return QColor(Qt::black);
    } else if (role == Qt::FontRole) {
        if (col == BuySell) { QFont font; font.setBold(true); return font; }
    }
    return QVariant();
}

QVariant TradeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < m_headers.size()) {
        return m_headers.at(section);
    }
    return QVariant();
}

void TradeModel::setTrades(const QVector<XTS::Trade>& newTrades)
{
    // Index mapping for current trades
    QMap<QString, int> existingIndices;
    for (int i = 0; i < m_trades.size(); ++i) {
        existingIndices[m_trades[i].executionID] = i;
    }

    QVector<XTS::Trade> finalTrades;
    QSet<QString> handledIDs;

    // 1. Update existing and find new
    for (const auto& newTrade : newTrades) {
        handledIDs.insert(newTrade.executionID);
        if (existingIndices.contains(newTrade.executionID)) {
            int oldIdx = existingIndices[newTrade.executionID];
            int modelRow = m_filterRowVisible ? oldIdx + 1 : oldIdx;
            
            m_trades[oldIdx] = newTrade;
            emit dataChanged(index(modelRow, 0), index(modelRow, ColumnCount - 1));
        } else {
            finalTrades.append(newTrade);
        }
    }

    // 2. Handle Insertions
    if (!finalTrades.isEmpty()) {
        int startRow = m_trades.size();
        int modelStart = m_filterRowVisible ? startRow + 1 : startRow;
        
        beginInsertRows(QModelIndex(), modelStart, modelStart + finalTrades.size() - 1);
        m_trades.append(finalTrades);
        endInsertRows();
    }

    // 3. Handle Deletions
    for (int i = m_trades.size() - 1; i >= 0; --i) {
        if (!handledIDs.contains(m_trades[i].executionID)) {
            int modelRow = m_filterRowVisible ? i + 1 : i;
            beginRemoveRows(QModelIndex(), modelRow, modelRow);
            m_trades.removeAt(i);
            endRemoveRows();
        }
    }
}

void TradeModel::setFilterRowVisible(bool visible)
{
    if (m_filterRowVisible == visible) return;
    beginResetModel();
    m_filterRowVisible = visible;
    endResetModel();
}
