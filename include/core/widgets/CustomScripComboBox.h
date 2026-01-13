#ifndef CUSTOMSCRIPCOMBOBOX_H
#define CUSTOMSCRIPCOMBOBOX_H

#include <QComboBox>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QDateTime>
#include <QListView>

/**
 * @brief Simplified Custom ComboBox for scrip/symbol selection
 */
class CustomScripComboBox : public QComboBox
{
    Q_OBJECT

public:
    enum SortMode {
        AlphabeticalSort,
        ChronologicalSort,
        NumericSort,
        NoSort
    };

    enum Mode {
        SelectorMode, // For Exchange/Segment (Visible arrow, Non-editable)
        SearchMode    // For Symbol/Strike (Hidden arrow, Editable)
    };

    explicit CustomScripComboBox(QWidget *parent = nullptr);
    ~CustomScripComboBox() override;

    void setMode(Mode mode);
    void setSortMode(SortMode mode);
    
    // Data management
    void addItem(const QString &text, const QVariant &userData = QVariant());
    void addItems(const QStringList &texts);
    void clearItems();


signals:
    void escapePressed();
    void itemSelected(const QString &text);
    void enterPressedWhenClosed();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void showPopup() override;
    void hidePopup() override;

private slots:
    void onItemActivated(int index);

private:
    void setupUI();
    void sortItems();
    void selectAllText();
    QDateTime parseDate(const QString &dateStr) const;


    QStandardItemModel *m_sourceModel;
    QListView *m_listView;
    
    SortMode m_sortMode;
    QStringList m_allItems;
    bool m_isPopupVisible;
    bool m_isUpdating;
};

#endif // CUSTOMSCRIPCOMBOBOX_H
