#ifndef SELECTIONHELPERS_H
#define SELECTIONHELPERS_H

#include <QList>
#include <QModelIndex>
#include <QModelIndexList>

class QItemSelectionModel;
class QSortFilterProxyModel;

/**
 * @brief Generic selection utilities for views with proxy models
 * 
 * Provides helper functions for mapping selections between proxy and source models,
 * useful when working with sorted/filtered table views.
 */
class SelectionHelpers
{
public:
    /**
     * @brief Get source model row numbers from proxy selection
     * @param selectionModel Item selection model (from view)
     * @param proxyModel Proxy model used by the view
     * @return List of source model row indices (unsorted)
     */
    static QList<int> getSourceRows(QItemSelectionModel *selectionModel,
                                     QSortFilterProxyModel *proxyModel);
    
    /**
     * @brief Get source model row numbers (sorted in descending order)
     * @param selectionModel Item selection model (from view)
     * @param proxyModel Proxy model used by the view
     * @return List of source model row indices (descending order)
     * 
     * Useful for removing rows without index shifting issues.
     */
    static QList<int> getSourceRowsDescending(QItemSelectionModel *selectionModel,
                                               QSortFilterProxyModel *proxyModel);
    
    /**
     * @brief Map proxy indices to source indices
     * @param proxyIndices List of proxy model indices
     * @param proxyModel Proxy model
     * @return List of source model indices
     */
    static QModelIndexList mapToSource(const QModelIndexList &proxyIndices,
                                       QSortFilterProxyModel *proxyModel);
    
    /**
     * @brief Map source indices to proxy indices
     * @param sourceIndices List of source model indices
     * @param proxyModel Proxy model
     * @return List of proxy model indices
     */
    static QModelIndexList mapToProxy(const QModelIndexList &sourceIndices,
                                      QSortFilterProxyModel *proxyModel);
};

#endif // SELECTIONHELPERS_H
