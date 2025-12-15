#include "core/widgets/CustomScripComboBox.h"
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QStandardItem>
#include <QTimer>
#include <QDebug>
#include <algorithm>

// Debug logging control
#define DEBUG_COMBOBOX 1

#if DEBUG_COMBOBOX
#define COMBO_DEBUG(msg) qDebug() << "[CustomScripComboBox]" << msg
#else
#define COMBO_DEBUG(msg)
#endif

CustomScripComboBox::CustomScripComboBox(QWidget *parent)
    : QComboBox(parent)
    , m_sortMode(AlphabeticalSort)
    , m_isPopupVisible(false)
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
    }
    
    // Setup model
    m_sourceModel = new QStandardItemModel(this);
    setModel(m_sourceModel);
    
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
    setMaxVisibleItems(10);
    
    // Custom styling
    setStyleSheet(
        "QComboBox {"
        "    border: 1px solid #3f3f46;"
        "    border-radius: 3px;"
        "    padding: 4px 8px;"
        "    background: #1e1e1e;"
        "    color: #ffffff;"
        "    selection-background-color: #094771;"
        "}"
        "QComboBox:focus {"
        "    border: 1px solid #0e639c;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 20px;"
        "}"
        "QComboBox::down-arrow {"
        "    width: 0;"
        "    height: 0;"
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
        COMBO_DEBUG("addItem skipped:" << text << "(empty or duplicate)");
        return;
    }
    
    COMBO_DEBUG("addItem:" << text);
    m_allItems.append(text);
    
    QStandardItem *item = new QStandardItem(text);
    item->setData(userData, Qt::UserRole);
    m_sourceModel->appendRow(item);
    
    sortItems();
}

void CustomScripComboBox::addItems(const QStringList &texts)
{
    COMBO_DEBUG("addItems: count =" << texts.count());
    for (const QString &text : texts) {
        addItem(text);
    }
}

void CustomScripComboBox::clearItems()
{
    COMBO_DEBUG("clearItems called");
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
                // Dropdown is open: close it, emit selection, and advance focus
                COMBO_DEBUG("Enter pressed with dropdown OPEN - closing and advancing focus");
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
                COMBO_DEBUG("Enter pressed with dropdown CLOSED - emitting enterPressedWhenClosed signal");
                emit enterPressedWhenClosed();
                event->accept();
                return;
            }
            break;
            
        case Qt::Key_Down:
        case Qt::Key_Up:
            // Show dropdown if not visible
            if (!m_isPopupVisible) {
                showPopup();
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
    COMBO_DEBUG("showPopup called");
    m_isPopupVisible = true;
    QComboBox::showPopup();
}

void CustomScripComboBox::hidePopup()
{
    COMBO_DEBUG("hidePopup called");
    m_isPopupVisible = false;
    QComboBox::hidePopup();
}

void CustomScripComboBox::onItemActivated(int index)
{
    Q_UNUSED(index)
    
    if (m_lineEdit) {
        QString selectedText = m_lineEdit->text();
        if (!selectedText.isEmpty()) {
            emit itemSelected(selectedText);
            
            // Select all text for easy next input
            QTimer::singleShot(0, this, &CustomScripComboBox::selectAllText);
        }
    }
}

void CustomScripComboBox::sortItems()
{
    if (m_sortMode == NoSort) {
        return;
    }
    
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
            if (dateA.isValid() && dateB.isValid()) {
                return dateA < dateB;
            }
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
            if (okA && okB) {
                return numA < numB;
            }
            
            // If only one is a number, number comes first
            if (okA) return true;
            if (okB) return false;
            
            // If neither is a number, compare as strings
            return a < b;
        });
    }
    
    // Rebuild model with sorted items
    m_sourceModel->clear();
    for (const QString &item : m_allItems) {
        m_sourceModel->appendRow(new QStandardItem(item));
    }
}

QDateTime CustomScripComboBox::parseDate(const QString &dateStr) const
{
    // Try common date formats used in trading
    QStringList formats = {
        "dd-MMM-yyyy",  // 26-Dec-2024
        "dd-MM-yyyy",   // 26-12-2024
        "yyyy-MM-dd",   // 2024-12-26
        "dd/MM/yyyy",   // 26/12/2024
        "MMM-yyyy",     // Dec-2024
        "MMMMyyyy",     // DEC2024
    };
    
    for (const QString &format : formats) {
        QDateTime dt = QDateTime::fromString(dateStr, format);
        if (dt.isValid()) {
            return dt;
        }
    }
    
    return QDateTime();
}
