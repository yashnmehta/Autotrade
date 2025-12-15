#include "utils/SelectionHelpers.h"
#include <QItemSelectionModel>
#include <QSortFilterProxyModel>
#include <algorithm>
#include <QDebug>

QList<int> SelectionHelpers::getSourceRows(QItemSelectionModel *selectionModel,
                                            QSortFilterProxyModel *proxyModel)
{
    QList<int> sourceRows;
    
    if (!selectionModel || !proxyModel) {
        return sourceRows;
    }
    
    QModelIndexList selected = selectionModel->selectedRows();
    
    for (const QModelIndex &proxyIndex : selected) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (sourceIndex.isValid()) {
            sourceRows.append(sourceIndex.row());
        }
    }
    
    qDebug() << "[SelectionHelpers] Mapped" << selected.size() << "proxy rows to" << sourceRows.size() << "source rows";
    return sourceRows;
}

QList<int> SelectionHelpers::getSourceRowsDescending(QItemSelectionModel *selectionModel,
                                                      QSortFilterProxyModel *proxyModel)
{
    QList<int> sourceRows = getSourceRows(selectionModel, proxyModel);
    
    // Sort in descending order (useful for row deletion)
    std::sort(sourceRows.begin(), sourceRows.end(), std::greater<int>());
    
    return sourceRows;
}

QModelIndexList SelectionHelpers::mapToSource(const QModelIndexList &proxyIndices,
                                               QSortFilterProxyModel *proxyModel)
{
    QModelIndexList sourceIndices;
    
    if (!proxyModel) {
        return sourceIndices;
    }
    
    for (const QModelIndex &proxyIndex : proxyIndices) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (sourceIndex.isValid()) {
            sourceIndices.append(sourceIndex);
        }
    }
    
    return sourceIndices;
}

QModelIndexList SelectionHelpers::mapToProxy(const QModelIndexList &sourceIndices,
                                             QSortFilterProxyModel *proxyModel)
{
    QModelIndexList proxyIndices;
    
    if (!proxyModel) {
        return proxyIndices;
    }
    
    for (const QModelIndex &sourceIndex : sourceIndices) {
        QModelIndex proxyIndex = proxyModel->mapFromSource(sourceIndex);
        if (proxyIndex.isValid()) {
            proxyIndices.append(proxyIndex);
        }
    }
    
    return proxyIndices;
}
