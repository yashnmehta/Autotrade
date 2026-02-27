#ifndef PINNEDROWPROXYMODEL_H
#define PINNEDROWPROXYMODEL_H

#include <QSortFilterProxyModel>

class PinnedRowProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit PinnedRowProxyModel(QObject *parent = nullptr);

protected:
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;
};

#endif // PINNEDROWPROXYMODEL_H
