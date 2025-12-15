#ifndef CLIPBOARDHELPERS_H
#define CLIPBOARDHELPERS_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QModelIndex>

class QAbstractItemModel;

/**
 * @brief Generic clipboard utilities for TSV (Tab-Separated Values) operations
 * 
 * Provides reusable functions for parsing and formatting clipboard data
 * in TSV format, commonly used in spreadsheet-style applications.
 */
class ClipboardHelpers
{
public:
    /**
     * @brief Parse TSV text into rows of fields
     * @param text Raw clipboard text in TSV format
     * @return List of rows, each row is a list of fields
     */
    static QList<QStringList> parseTSV(const QString &text);
    
    /**
     * @brief Format rows of fields into TSV text
     * @param rows List of rows, each row is a list of fields
     * @return TSV formatted string
     */
    static QString formatToTSV(const QList<QStringList> &rows);
    
    /**
     * @brief Convert model rows to TSV format
     * @param indices Selected model indices (should be complete rows)
     * @param model Source model containing the data
     * @param columnCount Number of columns to export (default: all)
     * @return TSV formatted string
     */
    static QString modelRowsToTSV(const QModelIndexList &indices, 
                                   QAbstractItemModel *model,
                                   int columnCount = -1);
    
    /**
     * @brief Check if text appears to be valid TSV format
     * @param text Text to validate
     * @param minColumns Minimum expected columns per row (0 = no check)
     * @return true if text has valid TSV structure
     */
    static bool isValidTSV(const QString &text, int minColumns = 0);
};

#endif // CLIPBOARDHELPERS_H
