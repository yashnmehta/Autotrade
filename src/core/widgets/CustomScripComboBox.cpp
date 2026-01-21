#include "core/widgets/CustomScripComboBox.h"
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QStandardItem>
#include <QTimer>
#include <QDebug>
#include <algorithm>

CustomScripComboBox::CustomScripComboBox(QWidget *parent)
    : QComboBox(parent)
    , m_sortMode(AlphabeticalSort)
    , m_isPopupVisible(false)
    , m_isUpdating(false)
{
    setupUI();
}

CustomScripComboBox::~CustomScripComboBox() = default;

void CustomScripComboBox::setupUI()
{
    setInsertPolicy(QComboBox::NoInsert);
    
    m_sourceModel = new QStandardItemModel(this);
    setModel(m_sourceModel);
    
    m_listView = qobject_cast<QListView*>(view());
    if (m_listView) {
        m_listView->setUniformItemSizes(true);
        
        // Forced "Snap to Top" for the Viewport View
        // This catches jumps during typing/searching while the popup is open
        connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged,
                [this](const QModelIndex &current, const QModelIndex &) {
            if (m_isPopupVisible && current.isValid()) {
                QTimer::singleShot(0, this, [this, current]() {
                    if (m_listView && m_isPopupVisible) {
                        m_listView->scrollTo(current, QAbstractItemView::PositionAtTop);
                    }
                });
            }
        });
    }
    
    connect(this, QOverload<int>::of(&QComboBox::activated),
            this, &CustomScripComboBox::onItemActivated);
    
    setMaxVisibleItems(15);
    
    // Default to SearchMode as previously implemented
    setMode(SearchMode);
}

void CustomScripComboBox::setMode(Mode mode)
{
    bool isSearch = (mode == SearchMode);
    setEditable(isSearch);
    
    if (isSearch) {
        if (auto* le = lineEdit()) {
            le->setPlaceholderText("Select...");
            le->installEventFilter(this);
        }
    }

    QString arrowWidth = "18px";
    QString arrowPadding = "4px";


}

void CustomScripComboBox::setSortMode(SortMode mode)
{
    m_sortMode = mode;
    sortItems();
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
    
    // Optimized merge and unique
    m_allItems.append(texts);
    m_allItems.removeDuplicates();
    m_allItems.removeAll("");
    
    // Sort and refill the model
    sortItems();
    
    m_isUpdating = false;
    m_sourceModel->layoutChanged();
}

void CustomScripComboBox::clearItems()
{
    m_allItems.clear();
    m_sourceModel->clear();
    if (auto* le = lineEdit()) {
        le->clear();
    }
}


void CustomScripComboBox::selectAllText()
{
    if (auto* le = lineEdit()) {
        le->selectAll();
    }
}


bool CustomScripComboBox::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == lineEdit() && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            selectAllText();
            return false; 
        }
    }
    return QComboBox::eventFilter(obj, event);
}

void CustomScripComboBox::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Escape:
            hidePopup();
            if (auto* le = lineEdit()) {
                le->deselect();
            }
            emit escapePressed();
            event->accept();
            return;
            
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_isPopupVisible) {
                QModelIndex currentIndex = view()->currentIndex();
                if (currentIndex.isValid()) {
                    // Fix: Use data from currentIndex directly (it's from the proxy model)
                    QString selectedText = currentIndex.data().toString();
                    if (auto* le = lineEdit()) le->setText(selectedText);
                }
                hidePopup();
                if (auto* le = lineEdit()) {
                    if (!le->text().isEmpty()) {
                        emit itemSelected(le->text());
                    }
                }
                event->accept();
                return;
            } else {
                emit enterPressedWhenClosed();
                event->accept();
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
    QTimer::singleShot(0, this, [this]() {
        if (auto* le = lineEdit()) {
            if (!le->text().isEmpty()) {
                le->selectAll();
            }
        }
    });
}

void CustomScripComboBox::focusOutEvent(QFocusEvent *event)
{
    QComboBox::focusOutEvent(event);
    if (auto* le = lineEdit()) {
        le->deselect();
    }
}

void CustomScripComboBox::showPopup()
{
    if (m_sourceModel->rowCount() == 0) return;
    m_isPopupVisible = true;
    QComboBox::showPopup();
    
    // Ensure the current item is scrolled to top immediately on show
    int current = currentIndex();
    if (current >= 0 && m_listView) {
        QTimer::singleShot(0, this, [this, current]() {
            if (m_listView && m_isPopupVisible) {
                // Determine the correct index for scroll (handle proxy if necessary)
                // If filtering is inactive on show, m_sourceModel indices match.
                // If tricky, rely on view's current index if valid.
                QModelIndex idx = view()->currentIndex();
                if(idx.isValid()) {
                    m_listView->scrollTo(idx, QAbstractItemView::PositionAtTop);
                } else {
                     m_listView->scrollTo(m_sourceModel->index(current, 0), QAbstractItemView::PositionAtTop);
                }
            }
        });
    }
}

void CustomScripComboBox::hidePopup()
{
    m_isPopupVisible = false;
    QComboBox::hidePopup();
}

void CustomScripComboBox::onItemActivated(int index)
{
    // Fix: 'index' corresponds to the view's model (which might be a proxy)
    // Use the model associated with the combo box at this moment (or view)
    QString selectedText;
    if (view() && view()->model()) {
        QModelIndex idx = view()->model()->index(index, 0);
        if (idx.isValid()) {
             selectedText = idx.data().toString();
        }
    }
    
    // Fallback if view model access fails (unlikely)
    if (selectedText.isEmpty()) {
         selectedText = itemText(index);
    }

    // Update the line edit if we are in SearchMode (editable)
    if (auto* le = lineEdit()) {
        le->setText(selectedText);
        QTimer::singleShot(0, this, &CustomScripComboBox::selectAllText);
    }
    
    // Always emit the signal so the ScripBar knows to update the next combo box
    emit itemSelected(selectedText);
}

void CustomScripComboBox::sortItems()
{
    // Apply sorting based on mode (skip sorting for NoSort, but still populate model)
    if (m_sortMode == AlphabeticalSort) {
        m_allItems.sort(Qt::CaseInsensitive); // Faster than std::sort for QStringList
    } else if (m_sortMode == ChronologicalSort) {
        std::sort(m_allItems.begin(), m_allItems.end(),
                  [this](const QString &a, const QString &b) {
            QDateTime dateA = parseDate(a);
            QDateTime dateB = parseDate(b);
            if (dateA.isValid() && dateB.isValid()) return dateA < dateB;
            return a < b;
        });
    } else if (m_sortMode == NumericSort) {
        std::sort(m_allItems.begin(), m_allItems.end(),
                  [](const QString &a, const QString &b) {
            bool okA, okB;
            double numA = a.toDouble(&okA);
            double numB = b.toDouble(&okB);
            if (okA && okB) return numA < numB;
            if (okA) return true;
            if (okB) return false;
            return a < b;
        });
    }
    // NoSort: skip sorting, but still populate model below
    
    // Always populate the model (even for NoSort)
    m_sourceModel->clear();
    QComboBox::addItems(m_allItems);
}


QDateTime CustomScripComboBox::parseDate(const QString &dateStr) const
{
    QStringList formats = {
        "ddMMMMyyyy", "dd-MMM-yyyy", "ddMMMyyyy", "dd-MM-yyyy", "yyyy-MM-dd", "dd/MM/yyyy", "MMM-yyyy", "MMMMyyyy"
    };
    for (const QString &format : formats) {
        QDateTime dt = QDateTime::fromString(dateStr, format);
        if (dt.isValid()) return dt;
    }
    return QDateTime();
}
