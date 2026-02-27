#include "models/qt/PinnedRowProxyModel.h"
#include <QDebug>

PinnedRowProxyModel::PinnedRowProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

bool PinnedRowProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    // Get UserRole data to identify special rows
    QString leftType = sourceModel()->data(source_left, Qt::UserRole).toString();
    QString rightType = sourceModel()->data(source_right, Qt::UserRole).toString();

    bool isAscending = (sortOrder() == Qt::AscendingOrder);

    // 1. Filter Row Logic
    // Ascending: Filter < All (Returns true if left is filter)
    // Descending: Filter > All (Returns false if left is filter, true if right is filter)
    
    if (leftType == "FILTER_ROW") {
        return isAscending; 
    }
    if (rightType == "FILTER_ROW") {
        return !isAscending;
    }

    // 2. Summary Row Logic
    // Ascending: Summary > All (Returns false if left is summary)
    // Descending: Summary < All (Returns true if left is summary)
    
    if (leftType == "SUMMARY_ROW") {
        return !isAscending;
    }
    if (rightType == "SUMMARY_ROW") {
        return isAscending;
    }

    // 3. Normal Rows - use default sorting with tiebreaker
    bool result = QSortFilterProxyModel::lessThan(source_left, source_right);
    
    // Add tiebreaker logic for stable sort: if values are equal, use row index
    if (!result && !QSortFilterProxyModel::lessThan(source_right, source_left)) {
        // Values are equal - use row index as tiebreaker for consistent ordering
        return source_left.row() < source_right.row();
    }
    
    return result;
}
