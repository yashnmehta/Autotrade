#ifndef GENERICTABLEFILTER_H
#define GENERICTABLEFILTER_H

#include <QWidget>
#include <QStringList>
#include <QPushButton>
#include <QAbstractItemModel>
#include <QTableView>

/**
 * @brief A unified filter widget for table columns.
 * Can be used in a "Filter Row" at the top of a table.
 */
class GenericTableFilter : public QWidget
{
    Q_OBJECT

public:
    explicit GenericTableFilter(int column, QAbstractItemModel* model, QTableView* tableView, QWidget* parent = nullptr);
    
    void clear();
    void updateButtonDisplay();
    void setFilterRowOffset(int offset) { m_filterRowOffset = offset; }
    void setSummaryRowOffset(int offset) { m_summaryRowOffset = offset; }

signals:
    void filterChanged(int column, const QStringList& selectedValues);

private slots:
    void showFilterPopup();

private:
    QStringList getUniqueValuesForColumn() const;
    
    int m_column;
    QAbstractItemModel* m_model;
    QTableView* m_tableView;
    QPushButton* m_filterButton;
    QStringList m_selectedValues;
    
    int m_filterRowOffset;  // Number of rows at start to skip (e.g. 1 if filter row is at index 0)
    int m_summaryRowOffset; // Number of rows at end to skip (e.g. 1 if summary row is at the end)
};

#endif // GENERICTABLEFILTER_H
