#include "utils/ClipboardHelpers.h"
#include <QAbstractItemModel>
#include <QDebug>

QList<QStringList> ClipboardHelpers::parseTSV(const QString &text)
{
    QList<QStringList> result;
    
    if (text.isEmpty()) {
        return result;
    }
    
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }
        
        QStringList fields = line.split('\t');
        result.append(fields);
    }
    
    qDebug() << "[ClipboardHelpers] Parsed" << result.size() << "rows from TSV";
    return result;
}

QString ClipboardHelpers::formatToTSV(const QList<QStringList> &rows)
{
    QString result;
    
    for (const QStringList &row : rows) {
        result += row.join('\t');
        result += '\n';
    }
    
    qDebug() << "[ClipboardHelpers] Formatted" << rows.size() << "rows to TSV";
    return result;
}

QString ClipboardHelpers::modelRowsToTSV(const QModelIndexList &indices, 
                                         QAbstractItemModel *model,
                                         int columnCount)
{
    if (!model || indices.isEmpty()) {
        return QString();
    }
    
    // Determine column count
    if (columnCount < 0) {
        columnCount = model->columnCount();
    }
    
    QList<QStringList> rows;
    
    // Extract unique rows
    QSet<int> processedRows;
    for (const QModelIndex &index : indices) {
        int row = index.row();
        
        if (processedRows.contains(row)) {
            continue;
        }
        processedRows.insert(row);
        
        QStringList rowData;
        for (int col = 0; col < columnCount; ++col) {
            QModelIndex cellIndex = model->index(row, col);
            QString value = model->data(cellIndex, Qt::DisplayRole).toString();
            rowData.append(value);
        }
        
        rows.append(rowData);
    }
    
    return formatToTSV(rows);
}

bool ClipboardHelpers::isValidTSV(const QString &text, int minColumns)
{
    if (text.isEmpty()) {
        return false;
    }
    
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    
    if (lines.isEmpty()) {
        return false;
    }
    
    // Check if all lines have at least minColumns fields
    if (minColumns > 0) {
        for (const QString &line : lines) {
            QStringList fields = line.split('\t');
            if (fields.size() < minColumns) {
                return false;
            }
        }
    }
    
    return true;
}
