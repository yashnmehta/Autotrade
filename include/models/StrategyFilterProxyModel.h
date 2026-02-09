#ifndef STRATEGY_FILTER_PROXY_MODEL_H
#define STRATEGY_FILTER_PROXY_MODEL_H

#include <QSortFilterProxyModel>
#include "models/StrategyInstance.h"

class StrategyFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit StrategyFilterProxyModel(QObject *parent = nullptr);

    void setStrategyTypeFilter(const QString& strategyType);
    void setStatusFilter(StrategyState state);
    void clearStatusFilter();

    QString strategyTypeFilter() const { return m_strategyTypeFilter; }
    bool hasStatusFilter() const { return m_hasStatusFilter; }
    StrategyState statusFilter() const { return m_statusFilter; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_strategyTypeFilter;
    bool m_hasStatusFilter = false;
    StrategyState m_statusFilter = StrategyState::Created;
};

#endif // STRATEGY_FILTER_PROXY_MODEL_H
