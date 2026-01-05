#ifndef GENERICTABLEFILTER_H
#define GENERICTABLEFILTER_H

#include <QWidget>
#include <QStringList>
#include <QPushButton>
#include <QLineEdit>
#include <QAbstractItemModel>
#include <QTableView>

/**
 * @brief A unified filter widget for table columns.
 * Supports three modes:
 * - PopupMode: Click button to show checkbox filter popup
 * - InlineMode: Text input for live string search
 * - CombinedMode: Text input + dropdown button (Excel-style)
 */
class GenericTableFilter : public QWidget
{
    Q_OBJECT

public:
    enum FilterMode {
        PopupMode,    ///< Button that opens checkbox popup
        InlineMode,   ///< Text input for live string search
        CombinedMode  ///< Both text input and dropdown button (Excel-like)
    };

    explicit GenericTableFilter(int column, QAbstractItemModel* model, QTableView* tableView, 
                                FilterMode mode = PopupMode, QWidget* parent = nullptr);
    
    void clear();
    void updateButtonDisplay();
    void setFilterRowOffset(int offset) { m_filterRowOffset = offset; }
    void setSummaryRowOffset(int offset) { m_summaryRowOffset = offset; }
    
    FilterMode filterMode() const { return m_filterMode; }
    QString filterText() const;  ///< Get current filter text (inline/combined mode)

signals:
    void filterChanged(int column, const QStringList& selectedValues);
    void textFilterChanged(int column, const QString& filterText);  ///< For inline/combined mode
    void sortRequested(int column, Qt::SortOrder order);

private slots:
    void showFilterPopup();
    void onTextChanged(const QString& text);

private:
    QStringList getUniqueValuesForColumn() const;
    
    int m_column;
    QAbstractItemModel* m_model;
    QTableView* m_tableView;
    FilterMode m_filterMode;
    
    // Widgets (may be null depending on mode)
    QPushButton* m_filterButton;
    QLineEdit* m_filterLineEdit;
    QStringList m_selectedValues;
    
    int m_filterRowOffset;  // Number of rows at start to skip (e.g. 1 if filter row is at index 0)
    int m_summaryRowOffset; // Number of rows at end to skip (e.g. 1 if summary row is at the end)
};

#endif // GENERICTABLEFILTER_H


