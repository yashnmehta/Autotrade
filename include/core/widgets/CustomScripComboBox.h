#ifndef CUSTOMSCRIPCOMBOBOX_H
#define CUSTOMSCRIPCOMBOBOX_H

#include <QComboBox>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QDateTime>
#include <QListView>
#include <QSortFilterProxyModel>

/**
 * @brief Custom ComboBox for scrip/symbol selection with enhanced UX
 * 
 * Features:
 * - Line-edit feel with editable input
 * - High-performance filtering for large lists (8000+ items)
 * - Automatic sorting (alphabetical for symbols, chronological for dates, numeric for strikes)
 * - Select-all text on Tab key
 * - Keyboard shortcuts (Esc, Enter, Tab)
 * - Smart Enter key behavior (advance focus or trigger action)
 * - Custom styling support
 */
class CustomScripComboBox : public QComboBox
{
    Q_OBJECT

public:
    enum SortMode {
        AlphabeticalSort,    // For symbols/text (A-Z)
        ChronologicalSort,   // For dates (earliest first)
        NumericSort,         // For numbers (ascending: 18000, 18500, 19000)
        NoSort               // Keep original order
    };

    explicit CustomScripComboBox(QWidget *parent = nullptr);
    ~CustomScripComboBox() override;

    // Configuration
    void setSortMode(SortMode mode);
    void setMaxVisibleItems(int count);
    
    // Data management
    void addItem(const QString &text, const QVariant &userData = QVariant());
    void addItems(const QStringList &texts);
    void clearItems();
    QString currentText() const;
    
    // Utility
    void selectAllText();

signals:
    void textChanged(const QString &text);
    void itemSelected(const QString &text);
    void enterPressedWhenClosed();  // Emitted when Enter pressed with dropdown closed

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void showPopup() override;
    void hidePopup() override;

private slots:
    void onItemActivated(int index);
    void onFilterTextChanged(const QString &text);

private:
    void setupUI();
    void sortItems();
    QDateTime parseDate(const QString &dateStr) const;

    // UI Components
    QLineEdit *m_lineEdit;
    QStandardItemModel *m_sourceModel;
    QSortFilterProxyModel *m_proxyModel;
    QListView *m_listView;
    
    // Configuration
    SortMode m_sortMode;
    
    // State
    QStringList m_allItems;
    bool m_isPopupVisible;
    bool m_isUpdating;
};

#endif // CUSTOMSCRIPCOMBOBOX_H
