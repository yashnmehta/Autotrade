#include "models/OrderModel.h"
#include <QColor>
#include <QBrush>
#include <QFont>
#include <QDateTime>
#include <QSet>
#include <QMap>
#include "repository/RepositoryManager.h"



OrderModel::OrderModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_filterRowVisible(false)
{
    m_headers = QStringList({
        "User", "Group", "Client Order No.", "Exchange Code", "MemberId", "TraderId",
        "Instrument Type", "Instrument Name", "Code", "Symbol/ScripId", "Ser/Exp/Group",
        "Strike Price", "Option Type", "Scrip Name", "Order Type", "B/S", "Pending Quantity",
        "Price", "Spread", "Total Quantity", "Pro/Cli", "Client", "Client Name", "Misc.",
        "Exch. Ord.No.", "Status", "Disclosed Quantity", "Settlor", "Validity",
        "Good Till Days/Date/Time", "MF/AON", "MF Quantity", "Trigger Price",
        "CP Broker Id", "Auction No.", "Last Modified Time", "Total Executed Quantity",
        "Avg. Price", "Reason", "User Remarks", "Part Type", "ProductType",
        "Order Unique ID", "Server Entry Time", "AMO Order ID", "OMSID", "Give Up Status", "EOMSID",
        "BookingRef", "Dealing Instruction", "Order Instruction", "Pending Lots",
        "Total Lots", "Total Executed Lots", "Alias Settlor", "Alias PartType",
        "Initiated From", "Modified From", "Strategy", "Mapping", "Initiated From User Id",
        "Modified From User Id", "SOR Id", "Maturity Date", "Yield", "COL", "SL Price",
        "SL Trigger Price", "SL Jump Price", "LTP Jump Price", "Profit Price",
        "Leg Indicator", "Bracket Order Status", "Mapping Order Type", "Algo Type",
        "Algo Description"
    });
}

int OrderModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_orders.size() + (m_filterRowVisible ? 1 : 0);
}

int OrderModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant OrderModel::data(const QModelIndex& index, int role) const
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
    if (dataRow < 0 || dataRow >= m_orders.size()) return QVariant();
    const XTS::Order& order = m_orders.at(dataRow);

    if (role == Qt::DisplayRole) {
        switch (col) {
            case User: return order.loginID;
            case Group: return "DEFAULT";
            case ClientOrderNo: return (qlonglong)order.appOrderID;
            case ExchangeCode: {
                bool isNumeric;
                int segId = order.exchangeSegment.toInt(&isNumeric);
                if (isNumeric) return RepositoryManager::getExchangeSegmentName(segId);
                return order.exchangeSegment;
            }
            case MemberId: return "1"; // Static for now or find in order
            case TraderId: return order.loginID;
            case InstrumentType: {
                bool isNumeric;
                int segId = order.exchangeSegment.toInt(&isNumeric);
                if (isNumeric) return RepositoryManager::getExchangeSegmentName(segId);
                return order.exchangeSegment;
            }

            case InstrumentName: return order.tradingSymbol;
            case Code: return (qlonglong)order.exchangeInstrumentID;
            case Symbol: return order.tradingSymbol;
            case StrikePrice: return (order.exchangeSegment.contains("FO")) ? QString::number(order.orderPrice, 'f', 2) : ""; // Placeholder logic
            case ScripName: return order.tradingSymbol;
            case OrderType: return order.orderType;
            case BuySell: return order.orderSide;
            case PendingQty: return order.orderQuantity - order.cumulativeQuantity;
            case Price: return QString::number(order.orderPrice, 'f', 2);
            case TotalQty: return order.orderQuantity;
            case ProCli: return "CLI";
            case Client: return order.clientID;
            case ExchOrdNo: return order.exchangeOrderID;
            case Status: return order.orderStatus;
            case DisclosedQty: return order.orderDisclosedQuantity;
            case TriggerPrice: return QString::number(order.orderStopPrice, 'f', 2);
            case TotalExecutedQty: return order.cumulativeQuantity;
            case AvgPrice: return QString::number(order.orderAverageTradedPrice, 'f', 2);
            case Reason: return order.cancelRejectReason;
            case UserRemarks: return order.orderUniqueIdentifier;
            case ProductType: return order.productType;
            case OrderUniqueID: return order.orderUniqueIdentifier;  // Dedicated column
            case ServerEntryTime: return order.orderTimestamp();
            default: return QString();
        }
    } else if (role == Qt::UserRole) {
        switch (col) {
            case Code: return (qlonglong)order.exchangeInstrumentID;
            case Price: return order.orderPrice;

            case TotalQty: return order.orderQuantity;
            case ExchOrdNo: return (qlonglong)order.exchangeOrderID.toLongLong();
            case ServerEntryTime: return QDateTime::fromString(order.orderTimestamp(), "dd-MM-yyyy HH:mm:ss");
            default: return QVariant();
        }
    } else if (role == Qt::BackgroundRole) {
        QString side = order.orderSide.trimmed().toUpper();
        QString status = order.orderStatus.trimmed().toUpper();
        
        QColor color;
        if (status.contains("REJECTED")) {
            color = QColor("#FFE0B2"); // Orange
        } else if (side == "BUY") {
            // Default to Light Blue (Executed/Filled)
            color = QColor("#E3F2FD"); 
            
            if (status.contains("OPEN") || status.contains("NEW") || status.contains("PENDING") || status.contains("PARTIALLY")) {
                color = QColor("#90CAF9"); // Dark Blue
            } else if (status.contains("CANCEL")) {
                color = QColor("#CFD8DC"); // Blueish Grey
            }
        } else if (side == "SELL") {
            // Default to Light Red (Executed/Filled)
            color = QColor("#FFEBEE");
            
            if (status.contains("OPEN") || status.contains("NEW") || status.contains("PENDING") || status.contains("PARTIALLY")) {
                color = QColor("#EF9A9A"); // Dark Red
            } else if (status.contains("CANCEL")) {
                color = QColor("#D7CCC8"); // Redish Grey
            }
        }
        
        if (color.isValid()) {
            return color;
        }
        return QVariant();
    } else if (role == Qt::ForegroundRole) {
        if (col == BuySell) return (order.orderSide.trimmed().toUpper() == "BUY") ? QColor("#0d47a1") : QColor("#b71c1c");
        return QColor(Qt::black);
    } else if (role == Qt::FontRole) {
        if (col == BuySell || col == Status) { QFont font; font.setBold(true); return font; }
    }
    return QVariant();
}

QVariant OrderModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < m_headers.size()) {
        return m_headers.at(section);
    }
    return QVariant();
}

void OrderModel::setOrders(const QVector<XTS::Order>& newOrders)
{
    // Index mapping for current orders
    QMap<int64_t, int> existingIndices;
    for (int i = 0; i < m_orders.size(); ++i) {
        existingIndices[m_orders[i].appOrderID] = i;
    }

    QVector<XTS::Order> finalOrders;
    QSet<int64_t> handledIDs;

    // 1. Update existing and find new
    for (const auto& newOrder : newOrders) {
        handledIDs.insert(newOrder.appOrderID);
        if (existingIndices.contains(newOrder.appOrderID)) {
            int oldIdx = existingIndices[newOrder.appOrderID];
            int modelRow = m_filterRowVisible ? oldIdx + 1 : oldIdx;
            
            // Check if anything actually changed before emitting (optional but good)
            m_orders[oldIdx] = newOrder;
            emit dataChanged(index(modelRow, 0), index(modelRow, ColumnCount - 1));
        } else {
            finalOrders.append(newOrder);
        }
    }

    // 2. Handle Insertions (usually new orders appear at the end or top, depends on sorting)
    if (!finalOrders.isEmpty()) {
        int startRow = m_orders.size();
        int modelStart = m_filterRowVisible ? startRow + 1 : startRow;
        
        beginInsertRows(QModelIndex(), modelStart, modelStart + finalOrders.size() - 1);
        m_orders.append(finalOrders);
        endInsertRows();
    }

    // 3. Handle Deletions
    for (int i = m_orders.size() - 1; i >= 0; --i) {
        if (!handledIDs.contains(m_orders[i].appOrderID)) {
            int modelRow = m_filterRowVisible ? i + 1 : i;
            beginRemoveRows(QModelIndex(), modelRow, modelRow);
            m_orders.removeAt(i);
            endRemoveRows();
        }
    }
}

void OrderModel::setFilterRowVisible(bool visible)
{
    if (m_filterRowVisible == visible) return;
    beginResetModel();
    m_filterRowVisible = visible;
    endResetModel();
}

const XTS::Order& OrderModel::getOrderAt(int row) const
{
    int dataRow = m_filterRowVisible ? row - 1 : row;
    return m_orders.at(dataRow);
}
