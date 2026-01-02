#include "core/widgets/CustomScripComboBox.h"
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QStandardItem>
#include <QTimer>
#include <QDebug>
#include <algorithm>

// Debug logging control
#define DEBUG_COMBOBOX 0

#if DEBUG_COMBOBOX
#define COMBO_DEBUG(msg) qDebug() << "[CustomScripComboBox]" << msg
#else
#define COMBO_DEBUG(msg)
#endif

CustomScripComboBox::CustomScripComboBox(QWidget *parent)
    : QComboBox(parent)
    , m_sortMode(AlphabeticalSort)
    , m_isPopupVisible(false)
    , m_isUpdating(false)
{
    COMBO_DEBUG("Constructor called");
    setupUI();
    COMBO_DEBUG("Constructor complete");
}

CustomScripComboBox::~CustomScripComboBox() = default;

void CustomScripComboBox::setupUI()
{
    // Make it editable to get line-edit feel
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
    
    // Get the line edit
    m_lineEdit = lineEdit();
    if (m_lineEdit) {
        m_lineEdit->setClearButtonEnabled(false);
        m_lineEdit->setPlaceholderText("Select...");
        
        // Install event filter for Tab key handling
        m_lineEdit->installEventFilter(this);
        
        // Connect text change for real-time filtering
        connect(m_lineEdit, &QLineEdit::textEdited, this, &CustomScripComboBox::onFilterTextChanged);
    }
    
    // Setup model
    m_sourceModel = new QStandardItemModel(this);
    
    // Setup proxy model for high-performance filtering
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_sourceModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(0);
    
    // Set the proxy model as the combobox model
    setModel(m_proxyModel);
    
    // Setup list view
    m_listView = qobject_cast<QListView*>(view());
    if (m_listView) {
        m_listView->setUniformItemSizes(true);
        m_listView->setLayoutMode(QListView::Batched);
    }
    
    // Connect item activation
    connect(this, QOverload<int>::of(&QComboBox::activated),
            this, &CustomScripComboBox::onItemActivated);
    
    // Set default max visible items
    setMaxVisibleItems(15);
    
    // Custom styling
    setStyleSheet(
        "QComboBox {"
        "    border: 1px solid #3f3f46;"
        "    border-radius: 3px;"
        "    padding: 2px 8px;"
        "    background: #1e1e1e;"
        "    color: #ffffff;"
        "    selection-background-color: #094771;"
        "}"
        "QComboBox:focus {"
        "    border: 1px solid #0e639c;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 0px;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background: #1e1e1e;"
        "    color: #ffffff;"
        "    selection-background-color: #094771;"
        "    selection-color: #ffffff;"
        "    border: 1px solid #3f3f46;"
        "    outline: none;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "    padding: 4px 8px;"
        "    min-height: 20px;"
        "}"
        "QComboBox QAbstractItemView::item:hover {"
        "    background: #2d2d30;"
        "}"
        "QComboBox QAbstractItemView::item:selected {"
        "    background: #094771;"
        "}"
        "QLineEdit {"
        "    border: none;"
        "    background: transparent;"
        "    color: #ffffff;"
        "    selection-background-color: #094771;"
        "}"
    );
}

void CustomScripComboBox::setSortMode(SortMode mode)
{
    m_sortMode = mode;
    sortItems();
}

void CustomScripComboBox::setMaxVisibleItems(int count)
{
    QComboBox::setMaxVisibleItems(count);
}

void CustomScripComboBox::addItem(const QString &text, const QVariant &userData)
{
    if (text.isEmpty() || m_allItems.contains(text)) {
        return;
    }
    
    m_allItems.append(text);
    
    QStandardItem *item = new QStandardItem(text);
    item->setData(userData, Qt::UserRole);
    m_sourceModel->appendRow(item);
    
    if (!m_isUpdating) {
        sortItems();
    }
}

void CustomScripComboBox::addItems(const QStringList &texts)
{
    if (texts.isEmpty()) return;
    
    m_isUpdating = true;
    m_sourceModel->blockSignals(true);
    
    // Use QSet for O(1) membership check
    QSet<QString> existingItems = m_allItems.toSet();
    bool changed = false;
    
    for (const QString &text : texts) {
        if (!text.isEmpty() && !existingItems.contains(text)) {
            m_allItems.append(text);
            existingItems.insert(text);
            changed = true;
        }
    }
    
    if (changed) {
        sortItems(); // This will rebuild the model with sorted items
    }
    
    m_sourceModel->blockSignals(false);
    m_isUpdating = false;
    
    // Refresh the view
    if (changed) {
        m_sourceModel->layoutChanged();
    }
}

void CustomScripComboBox::clearItems()
{
    m_allItems.clear();
    m_sourceModel->clear();
    if (m_lineEdit) {
        m_lineEdit->clear();
    }
}

QString CustomScripComboBox::currentText() const
{
    return m_lineEdit ? m_lineEdit->text() : QComboBox::currentText();
}

void CustomScripComboBox::selectAllText()
{
    if (m_lineEdit) {
        m_lineEdit->selectAll();
    }
}

void CustomScripComboBox::onFilterTextChanged(const QString &text)
{
    if (m_isUpdating) return;
    
    // Apply filter
    m_proxyModel->setFilterFixedString(text);
    
    if (text.isEmpty()) {
        hidePopup();
    } else if (m_proxyModel->rowCount() > 0) {
        if (!m_isPopupVisible) {
            showPopup();
        }
    } else {
        hidePopup();
    }
}

bool CustomScripComboBox::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_lineEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        
        // Handle Tab key - select all text
        if (keyEvent->key() == Qt::Key_Tab) {
            selectAllText();
            return true; // Event handled
        }
        
        // Handle Shift+Tab
        if (keyEvent->key() == Qt::Key_Backtab) {
            selectAllText();
            return false; // Let it propagate for focus change
        }
    }
    
    return QComboBox::eventFilter(obj, event);
}

void CustomScripComboBox::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Escape:
            // Close dropdown and clear selection
            hidePopup();
            if (m_lineEdit) {
                m_lineEdit->deselect();
            }
            event->accept();
            return;
            
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_isPopupVisible) {
                // Determine which item is selected in view
                QModelIndex currentIndex = view()->currentIndex();
                if (currentIndex.isValid()) {
                    QString selectedText = m_proxyModel->data(currentIndex).toString();
                    if (m_lineEdit) m_lineEdit->setText(selectedText);
                }
                
                hidePopup();
                if (m_lineEdit && !m_lineEdit->text().isEmpty()) {
                    emit itemSelected(m_lineEdit->text());
                }
                
                // Advance focus to next widget
                QWidget *nextWidget = nextInFocusChain();
                if (nextWidget) {
                    nextWidget->setFocus();
                }
                
                event->accept();
                return;
            } else {
                // Dropdown is closed: emit signal for parent to trigger action
                emit enterPressedWhenClosed();
                event->accept();
                return;
            }
            break;
            
        case Qt::Key_Down:
            // Show dropdown if not visible
            if (!m_isPopupVisible) {
                showPopup();
                return;
            }
            break;
            
        default:
            break;
    }
    
    QComboBox::keyPressEvent(event);
}

void CustomScripComboBox::focusInEvent(QFocusEvent *event)
{
    QComboBox::focusInEvent(event);
    
    // Auto-select text on focus for easy editing
    QTimer::singleShot(0, this, [this]() {
        if (m_lineEdit && !m_lineEdit->text().isEmpty()) {
            m_lineEdit->selectAll();
        }
    });
}

void CustomScripComboBox::showPopup()
{
    if (m_proxyModel->rowCount() == 0) return;
    
    m_isPopupVisible = true;
    QComboBox::showPopup();
    
    // Ensure the first item is selected in the view
    if (m_proxyModel->rowCount() > 0) {
        view()->setCurrentIndex(m_proxyModel->index(0, 0));
    }
}

void CustomScripComboBox::hidePopup()
{
    m_isPopupVisible = false;
    QComboBox::hidePopup();
}

void CustomScripComboBox::onItemActivated(int index)
{
    // index here is for the proxy model
    QModelIndex proxyIndex = m_proxyModel->index(index, 0);
    QString selectedText = m_proxyModel->data(proxyIndex).toString();
    
    if (m_lineEdit) {
        m_lineEdit->setText(selectedText);
        emit itemSelected(selectedText);
        
        // Select all text for easy next input
        QTimer::singleShot(0, this, &CustomScripComboBox::selectAllText);
    }
}

void CustomScripComboBox::sortItems()
{
    if (m_sortMode == NoSort) return;
    
    // Sort logic
    if (m_sortMode == AlphabeticalSort) {
        // Sort alphabetically (case-insensitive)
        std::sort(m_allItems.begin(), m_allItems.end(), 
                  [](const QString &a, const QString &b) {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
    } else if (m_sortMode == ChronologicalSort) {
        // Sort by date
        std::sort(m_allItems.begin(), m_allItems.end(),
                  [this](const QString &a, const QString &b) {
            QDateTime dateA = parseDate(a);
            QDateTime dateB = parseDate(b);
            if (dateA.isValid() && dateB.isValid()) return dateA < dateB;
            // Fallback to string comparison
            return a < b;
        });
    } else if (m_sortMode == NumericSort) {
        // Sort numerically (for strike prices, integers, floats)
        std::sort(m_allItems.begin(), m_allItems.end(),
                  [](const QString &a, const QString &b) {
            bool okA, okB;
            double numA = a.toDouble(&okA);
            double numB = b.toDouble(&okB);
            
            // If both are valid numbers, compare numerically
            if (okA && okB) return numA < numB;
            
            // If only one is a number, number comes first
            if (okA) return true;
            if (okB) return false;
            
            // If neither is a number, compare as strings
            return a < b;
        });
    }
    
    // Rebuild model in one go
    m_sourceModel->clear();
    QList<QStandardItem*> items;
    for (const QString &text : m_allItems) {
        items.append(new QStandardItem(text));
    }
    if (!items.isEmpty()) {
        m_sourceModel->appendColumn(items);
    }
}

QDateTime CustomScripComboBox::parseDate(const QString &dateStr) const
{
    // Try common date formats used in trading (most common first)
    QStringList formats = {
        "ddMMMMyyyy", "dd-MMM-yyyy", "ddMMMyyyy", "dd-MM-yyyy", "yyyy-MM-dd", "dd/MM/yyyy", "MMM-yyyy", "MMMMyyyy"
    };
    
    for (const QString &format : formats) {
        QDateTime dt = QDateTime::fromString(dateStr, format);
        if (dt.isValid()) return dt;
    }
    
    return QDateTime();
}
