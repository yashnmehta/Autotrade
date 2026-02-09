#include "models/StrategyTableModel.h"
#include <QColor>
#include <QDateTime>

StrategyTableModel::StrategyTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int StrategyTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_instances.size();
}

int StrategyTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : COL_COUNT;
}

QVariant StrategyTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_instances.size()) {
        return QVariant();
    }

    const StrategyInstance& instance = m_instances.at(index.row());
    Column column = static_cast<Column>(index.column());

    if (role == Qt::DisplayRole) {
        return displayForColumn(instance, column);
    }

    if (role == Qt::TextAlignmentRole) {
        switch (column) {
        case COL_INSTANCE_NAME:
        case COL_SYMBOL:
        case COL_STRATEGY_TYPE:
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft);
        case COL_STATUS:
        case COL_DURATION:
        case COL_CREATED_AT:
        case COL_LAST_UPDATED:
            return static_cast<int>(Qt::AlignCenter);
        default:
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);
        }
    }

    if (role == Qt::ForegroundRole) {
        if (column == COL_MTM) {
            if (instance.mtm > 0.0) {
                return QColor("#00C853");
            }
            if (instance.mtm < 0.0) {
                return QColor("#FF5252");
            }
        }
    }

    if (role == Qt::UserRole) {
        return sortValueForColumn(instance, column);
    }

    if (role == Qt::UserRole + 1) {
        return static_cast<qlonglong>(instance.instanceId);
    }

    return QVariant();
}

QVariant StrategyTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (static_cast<Column>(section)) {
    case COL_INSTANCE_NAME:
        return "Instance";
    case COL_STATUS:
        return "Status";
    case COL_MTM:
        return "MTM";
    case COL_STOP_LOSS:
        return "StopLoss";
    case COL_TARGET:
        return "Target";
    case COL_ENTRY_PRICE:
        return "Entry";
    case COL_QUANTITY:
        return "Qty";
    case COL_POSITIONS:
        return "Positions";
    case COL_ORDERS:
        return "Orders";
    case COL_DURATION:
        return "Duration";
    case COL_SYMBOL:
        return "Symbol";
    case COL_STRATEGY_TYPE:
        return "Type";
    case COL_CREATED_AT:
        return "Created";
    case COL_LAST_UPDATED:
        return "Updated";
    default:
        return QVariant();
    }
}

Qt::ItemFlags StrategyTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags baseFlags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    Column column = static_cast<Column>(index.column());
    if (column == COL_STOP_LOSS || column == COL_TARGET) {
        return baseFlags | Qt::ItemIsEditable;
    }

    return baseFlags;
}

bool StrategyTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole) {
        return false;
    }

    int row = index.row();
    if (row < 0 || row >= m_instances.size()) {
        return false;
    }

    StrategyInstance& instance = m_instances[row];
    Column column = static_cast<Column>(index.column());

    if (column == COL_STOP_LOSS) {
        instance.stopLoss = value.toDouble();
    } else if (column == COL_TARGET) {
        instance.target = value.toDouble();
    } else {
        return false;
    }

    instance.lastUpdated = QDateTime::currentDateTime();

    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    emit instanceEdited(instance);
    return true;
}

void StrategyTableModel::setInstances(const QVector<StrategyInstance>& instances)
{
    beginResetModel();
    m_instances = instances;
    endResetModel();
}

void StrategyTableModel::addInstance(const StrategyInstance& instance)
{
    int row = m_instances.size();
    beginInsertRows(QModelIndex(), row, row);
    m_instances.append(instance);
    endInsertRows();
}

void StrategyTableModel::updateInstance(const StrategyInstance& instance)
{
    int row = findRowById(instance.instanceId);
    if (row < 0) {
        return;
    }

    m_instances[row] = instance;
    QModelIndex topLeft = index(row, 0);
    QModelIndex bottomRight = index(row, COL_COUNT - 1);
    emit dataChanged(topLeft, bottomRight, {Qt::DisplayRole, Qt::UserRole});
}

void StrategyTableModel::removeInstance(qint64 instanceId)
{
    int row = findRowById(instanceId);
    if (row < 0) {
        return;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_instances.removeAt(row);
    endRemoveRows();
}

StrategyInstance StrategyTableModel::instanceAt(int row) const
{
    if (row < 0 || row >= m_instances.size()) {
        return StrategyInstance();
    }
    return m_instances.at(row);
}

int StrategyTableModel::findRowById(qint64 instanceId) const
{
    for (int i = 0; i < m_instances.size(); ++i) {
        if (m_instances[i].instanceId == instanceId) {
            return i;
        }
    }
    return -1;
}

QVariant StrategyTableModel::displayForColumn(const StrategyInstance& instance, Column column) const
{
    switch (column) {
    case COL_INSTANCE_NAME:
        return instance.instanceName;
    case COL_STATUS:
        return StrategyInstance::stateDisplay(instance.state);
    case COL_MTM:
        return QString::number(instance.mtm, 'f', 2);
    case COL_STOP_LOSS:
        return QString::number(instance.stopLoss, 'f', 2);
    case COL_TARGET:
        return QString::number(instance.target, 'f', 2);
    case COL_ENTRY_PRICE:
        return QString::number(instance.entryPrice, 'f', 2);
    case COL_QUANTITY:
        return instance.quantity;
    case COL_POSITIONS:
        return instance.activePositions;
    case COL_ORDERS:
        return instance.pendingOrders;
    case COL_DURATION:
        return formatDuration(instance);
    case COL_SYMBOL:
        return instance.symbol;
    case COL_STRATEGY_TYPE:
        return instance.strategyType;
    case COL_CREATED_AT:
        return instance.createdAt.isValid() ? instance.createdAt.toString("yyyy-MM-dd HH:mm:ss") : "";
    case COL_LAST_UPDATED:
        return instance.lastUpdated.isValid() ? instance.lastUpdated.toString("yyyy-MM-dd HH:mm:ss") : "";
    default:
        return QVariant();
    }
}

QVariant StrategyTableModel::sortValueForColumn(const StrategyInstance& instance, Column column) const
{
    switch (column) {
    case COL_MTM:
        return instance.mtm;
    case COL_STOP_LOSS:
        return instance.stopLoss;
    case COL_TARGET:
        return instance.target;
    case COL_ENTRY_PRICE:
        return instance.entryPrice;
    case COL_QUANTITY:
        return instance.quantity;
    case COL_POSITIONS:
        return instance.activePositions;
    case COL_ORDERS:
        return instance.pendingOrders;
    case COL_DURATION:
        return instance.startTime.isValid() ? instance.startTime.secsTo(QDateTime::currentDateTime()) : 0;
    case COL_CREATED_AT:
        return instance.createdAt.isValid() ? instance.createdAt.toMSecsSinceEpoch() : 0;
    case COL_LAST_UPDATED:
        return instance.lastUpdated.isValid() ? instance.lastUpdated.toMSecsSinceEpoch() : 0;
    default:
        return displayForColumn(instance, column);
    }
}

QString StrategyTableModel::formatDuration(const StrategyInstance& instance) const
{
    if (!instance.startTime.isValid()) {
        return "00:00:00";
    }

    qint64 seconds = instance.startTime.secsTo(QDateTime::currentDateTime());
    if (seconds < 0) {
        seconds = 0;
    }

    qint64 hours = seconds / 3600;
    qint64 minutes = (seconds % 3600) / 60;
    qint64 secs = seconds % 60;

    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}
