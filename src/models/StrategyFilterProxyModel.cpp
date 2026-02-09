#include "models/StrategyFilterProxyModel.h"
#include "models/StrategyTableModel.h"

StrategyFilterProxyModel::StrategyFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void StrategyFilterProxyModel::setStrategyTypeFilter(const QString& strategyType)
{
    m_strategyTypeFilter = strategyType.trimmed();
    invalidateFilter();
}

void StrategyFilterProxyModel::setStatusFilter(StrategyState state)
{
    m_hasStatusFilter = true;
    m_statusFilter = state;
    invalidateFilter();
}

void StrategyFilterProxyModel::clearStatusFilter()
{
    m_hasStatusFilter = false;
    invalidateFilter();
}

bool StrategyFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex typeIndex = sourceModel()->index(sourceRow, StrategyTableModel::COL_STRATEGY_TYPE, sourceParent);
    QModelIndex stateIndex = sourceModel()->index(sourceRow, StrategyTableModel::COL_STATUS, sourceParent);

    QString typeValue = sourceModel()->data(typeIndex, Qt::DisplayRole).toString();
    QString statusValue = sourceModel()->data(stateIndex, Qt::DisplayRole).toString();

    if (!m_strategyTypeFilter.isEmpty() && m_strategyTypeFilter.toUpper() != "ALL") {
        if (typeValue.compare(m_strategyTypeFilter, Qt::CaseInsensitive) != 0) {
            return false;
        }
    }

    if (m_hasStatusFilter) {
        QString statusFilterValue = StrategyInstance::stateDisplay(m_statusFilter);
        if (statusValue.compare(statusFilterValue, Qt::CaseInsensitive) != 0) {
            return false;
        }
    }

    return true;
}
