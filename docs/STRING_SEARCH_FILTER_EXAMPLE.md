# String Search Filter Implementation Example

## Overview

This example shows how to implement a global search filter across multiple columns in Position/Trade/Order books (as shown in the reference image).

---

## 1. UI Component: Search Bar Widget

### Header: `include/ui/SearchFilterWidget.h`

```cpp
#ifndef SEARCHFILTERWIDGET_H
#define SEARCHFILTERWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>

class SearchFilterWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit SearchFilterWidget(QWidget* parent = nullptr);
    
    QString getSearchText() const;
    void clearSearch();
    bool isCaseSensitive() const;
    
signals:
    void searchTextChanged(const QString& text);
    void searchCleared();
    
private slots:
    void onTextChanged();
    void onClearClicked();
    
private:
    QLineEdit* m_searchEdit;
    QPushButton* m_clearButton;
    QComboBox* m_searchModeCombo;
    QCheckBox* m_caseSensitiveCheck;
};

#endif
```

### Implementation: `src/ui/SearchFilterWidget.cpp`

```cpp
#include "ui/SearchFilterWidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>

SearchFilterWidget::SearchFilterWidget(QWidget* parent)
    : QWidget(parent)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);
    
    // Search icon/label
    QLabel* searchIcon = new QLabel("🔍");
    searchIcon->setStyleSheet("font-size: 16px;");
    layout->addWidget(searchIcon);
    
    // Search text input
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search across all columns (e.g., ADANI, NRML, A0068)...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(300);
    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "   background-color: white;"
        "   border: 2px solid #3b82f6;"
        "   border-radius: 4px;"
        "   padding: 6px 10px;"
        "   font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "   border-color: #2563eb;"
        "   background-color: #eff6ff;"
        "}"
    );
    layout->addWidget(m_searchEdit);
    
    // Search mode dropdown
    QLabel* modeLabel = new QLabel("Mode:");
    layout->addWidget(modeLabel);
    
    m_searchModeCombo = new QComboBox(this);
    m_searchModeCombo->addItem("Contains", "contains");
    m_searchModeCombo->addItem("Starts With", "startswith");
    m_searchModeCombo->addItem("Exact Match", "exact");
    m_searchModeCombo->setCurrentIndex(0);
    layout->addWidget(m_searchModeCombo);
    
    // Case sensitive checkbox
    m_caseSensitiveCheck = new QCheckBox("Case Sensitive", this);
    m_caseSensitiveCheck->setChecked(false);
    layout->addWidget(m_caseSensitiveCheck);
    
    // Results label
    QLabel* resultsLabel = new QLabel("", this);
    resultsLabel->setObjectName("resultsLabel");
    resultsLabel->setStyleSheet("color: #6b7280; font-size: 12px;");
    layout->addWidget(resultsLabel);
    
    layout->addStretch();
    
    // Connections
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SearchFilterWidget::onTextChanged);
    connect(m_searchModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SearchFilterWidget::onTextChanged);
    connect(m_caseSensitiveCheck, &QCheckBox::stateChanged,
            this, &SearchFilterWidget::onTextChanged);
}

QString SearchFilterWidget::getSearchText() const {
    return m_searchEdit->text();
}

void SearchFilterWidget::clearSearch() {
    m_searchEdit->clear();
}

bool SearchFilterWidget::isCaseSensitive() const {
    return m_caseSensitiveCheck->isChecked();
}

void SearchFilterWidget::onTextChanged() {
    emit searchTextChanged(m_searchEdit->text());
}

void SearchFilterWidget::onClearClicked() {
    clearSearch();
    emit searchCleared();
}
```

---

## 2. Enhanced Proxy Model with Search

### Add to `SmartFilterProxyModel.h`:

```cpp
class SmartFilterProxyModel : public PinnedRowProxyModel {
    Q_OBJECT
    
public:
    // ... existing methods ...
    
    // Global search functionality
    void setSearchText(const QString& text, const QList<int>& searchColumns = {});
    void setSearchMode(SearchMode mode);
    void setCaseSensitive(bool sensitive);
    QString getSearchText() const { return m_searchText; }
    
    enum SearchMode {
        Contains,
        StartsWith,
        ExactMatch
    };
    
signals:
    void searchResultsChanged(int totalRows, int visibleRows, int filteredRows);
    
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    
private:
    QString m_searchText;
    QList<int> m_searchColumns;  // Empty = search all columns
    SearchMode m_searchMode = Contains;
    bool m_caseSensitive = false;
    
    bool matchesSearchText(int sourceRow) const;
};
```

### Add to `SmartFilterProxyModel.cpp`:

```cpp
void SmartFilterProxyModel::setSearchText(const QString& text, const QList<int>& searchColumns) {
    if (m_searchText != text || m_searchColumns != searchColumns) {
        m_searchText = text.trimmed();
        m_searchColumns = searchColumns;
        invalidateFilter();
        
        // Emit statistics
        int total = sourceModel() ? sourceModel()->rowCount() : 0;
        int visible = rowCount();
        emit searchResultsChanged(total, visible, total - visible);
    }
}

void SmartFilterProxyModel::setSearchMode(SearchMode mode) {
    if (m_searchMode != mode) {
        m_searchMode = mode;
        if (!m_searchText.isEmpty()) {
            invalidateFilter();
        }
    }
}

void SmartFilterProxyModel::setCaseSensitive(bool sensitive) {
    if (m_caseSensitive != sensitive) {
        m_caseSensitive = sensitive;
        if (!m_searchText.isEmpty()) {
            invalidateFilter();
        }
    }
}

bool SmartFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    // Always show filter and summary rows
    QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    QString rowType = sourceModel()->data(idx, Qt::UserRole).toString();
    if (rowType == "FILTER_ROW" || rowType == "SUMMARY_ROW") {
        return true;
    }
    
    // Check search text first (fastest rejection)
    if (!m_searchText.isEmpty() && !matchesSearchText(sourceRow)) {
        return false;
    }
    
    // Check other filters (column filters, numeric ranges, etc.)
    // ... existing filter logic ...
    
    return true;
}

bool SmartFilterProxyModel::matchesSearchText(int sourceRow) const {
    if (m_searchText.isEmpty()) {
        return true;
    }
    
    // Determine which columns to search
    QList<int> cols = m_searchColumns;
    if (cols.isEmpty()) {
        // Search all columns
        for (int i = 0; i < sourceModel()->columnCount(); ++i) {
            cols.append(i);
        }
    }
    
    // Search logic based on mode
    Qt::CaseSensitivity cs = m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    
    for (int col : cols) {
        QModelIndex idx = sourceModel()->index(sourceRow, col);
        QString value = sourceModel()->data(idx, Qt::DisplayRole).toString();
        
        bool match = false;
        
        switch (m_searchMode) {
            case Contains:
                match = value.contains(m_searchText, cs);
                break;
                
            case StartsWith:
                match = value.startsWith(m_searchText, cs);
                break;
                
            case ExactMatch:
                match = (value.compare(m_searchText, cs) == 0);
                break;
        }
        
        if (match) {
            return true;  // Found in at least one column
        }
    }
    
    return false;  // Not found in any column
}
```

---

## 3. Integration into PositionWindow

### Update `PositionWindow.cpp`:

```cpp
#include "ui/SearchFilterWidget.h"

void PositionWindow::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Top filter bar
    mainLayout->addWidget(createFilterWidget());
    
    // Search bar (NEW!)
    m_searchWidget = new SearchFilterWidget(this);
    mainLayout->addWidget(m_searchWidget);
    
    // Table
    setupTable();
    mainLayout->addWidget(m_tableView, 1);
    
    // Status bar with search results
    m_statusBar = new QWidget(this);
    QHBoxLayout* statusLayout = new QHBoxLayout(m_statusBar);
    m_statusLabel = new QLabel(this);
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    mainLayout->addWidget(m_statusBar);
}

void PositionWindow::setupConnections() {
    // ... existing connections ...
    
    // Connect search widget
    connect(m_searchWidget, &SearchFilterWidget::searchTextChanged,
            this, &PositionWindow::onSearchTextChanged);
    
    connect(m_searchWidget, &SearchFilterWidget::searchCleared,
            this, &PositionWindow::onSearchCleared);
    
    // Connect to proxy model for search results
    SmartFilterProxyModel* proxy = qobject_cast<SmartFilterProxyModel*>(m_proxyModel);
    if (proxy) {
        connect(proxy, &SmartFilterProxyModel::searchResultsChanged,
                this, &PositionWindow::onSearchResultsChanged);
    }
}

void PositionWindow::onSearchTextChanged(const QString& text) {
    SmartFilterProxyModel* proxy = qobject_cast<SmartFilterProxyModel*>(m_proxyModel);
    if (!proxy) return;
    
    // Define which columns to search (or empty for all columns)
    QList<int> searchColumns = {
        PositionModel::Symbol,           // Trading Symbol (e.g., "ADANIENT 27JAN2026")
        PositionModel::Exchange,         // Exchange (e.g., "NSEFO")
        PositionModel::User,            // User Id (e.g., "A0068")
        PositionModel::Client,          // Account Id (e.g., "A0068")
        PositionModel::ProductType,     // Product (e.g., "NRML")
        PositionModel::InstrumentType,  // Instrument Type
        PositionModel::ScripName        // Scrip Name
    };
    
    proxy->setSearchText(text, searchColumns);
    proxy->setCaseSensitive(m_searchWidget->isCaseSensitive());
}

void PositionWindow::onSearchCleared() {
    SmartFilterProxyModel* proxy = qobject_cast<SmartFilterProxyModel*>(m_proxyModel);
    if (proxy) {
        proxy->setSearchText("");
    }
}

void PositionWindow::onSearchResultsChanged(int total, int visible, int filtered) {
    QString statusText;
    
    if (filtered > 0) {
        statusText = QString("Showing %1 of %2 positions (%3 filtered by search)")
            .arg(visible)
            .arg(total)
            .arg(filtered);
    } else {
        statusText = QString("Showing %1 positions").arg(visible);
    }
    
    m_statusLabel->setText(statusText);
    
    // Optional: Highlight search results
    if (!m_searchWidget->getSearchText().isEmpty()) {
        m_statusLabel->setStyleSheet("color: #2563eb; font-weight: bold;");
    } else {
        m_statusLabel->setStyleSheet("color: #6b7280;");
    }
}
```

---

## 4. Advanced Features

### A. Highlight Search Matches in Table

```cpp
class SearchHighlightDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit SearchHighlightDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}
    
    void setSearchText(const QString& text) {
        m_searchText = text;
    }
    
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        
        QString text = index.data(Qt::DisplayRole).toString();
        
        if (!m_searchText.isEmpty() && text.contains(m_searchText, Qt::CaseInsensitive)) {
            // Highlight matching text
            QStyleOptionViewItem opt = option;
            opt.palette.setColor(QPalette::Highlight, QColor("#fef3c7")); // Light yellow
            opt.palette.setColor(QPalette::HighlightedText, QColor("#92400e")); // Dark brown
            
            painter->save();
            painter->fillRect(option.rect, QColor("#fef3c7"));
            
            // Draw text with highlight
            QStyledItemDelegate::paint(painter, opt, index);
            
            painter->restore();
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
    
private:
    QString m_searchText;
};

// Usage in PositionWindow:
void PositionWindow::setupTable() {
    // ... existing setup ...
    
    m_searchDelegate = new SearchHighlightDelegate(this);
    
    // Apply to all columns
    for (int i = 0; i < m_model->columnCount(); ++i) {
        m_tableView->setItemDelegateForColumn(i, m_searchDelegate);
    }
}

void PositionWindow::onSearchTextChanged(const QString& text) {
    // ... existing code ...
    
    // Update highlight delegate
    if (m_searchDelegate) {
        m_searchDelegate->setSearchText(text);
        m_tableView->viewport()->update();
    }
}
```

### B. Search History Dropdown

```cpp
class SearchFilterWidget : public QWidget {
    // ... existing code ...
    
private:
    void loadSearchHistory();
    void saveSearchHistory(const QString& text);
    
    QStringList m_searchHistory;
    QCompleter* m_completer;
};

void SearchFilterWidget::loadSearchHistory() {
    QSettings settings("YourCompany", "TradingTerminal");
    m_searchHistory = settings.value("search/history").toStringList();
    
    // Limit to last 10 searches
    if (m_searchHistory.size() > 10) {
        m_searchHistory = m_searchHistory.mid(0, 10);
    }
    
    // Setup autocomplete
    m_completer = new QCompleter(m_searchHistory, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_searchEdit->setCompleter(m_completer);
}

void SearchFilterWidget::saveSearchHistory(const QString& text) {
    if (text.isEmpty() || text.length() < 2) return;
    
    // Remove if already exists (move to top)
    m_searchHistory.removeAll(text);
    
    // Add to front
    m_searchHistory.prepend(text);
    
    // Limit size
    if (m_searchHistory.size() > 10) {
        m_searchHistory.removeLast();
    }
    
    // Save to settings
    QSettings settings("YourCompany", "TradingTerminal");
    settings.setValue("search/history", m_searchHistory);
    
    // Update completer
    m_completer->setModel(new QStringListModel(m_searchHistory, m_completer));
}
```

### C. Search with Keyboard Shortcuts

```cpp
void PositionWindow::setupUI() {
    // ... existing code ...
    
    // Ctrl+F for quick search focus
    QShortcut* searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        m_searchWidget->findChild<QLineEdit*>()->setFocus();
        m_searchWidget->findChild<QLineEdit*>()->selectAll();
    });
    
    // Escape to clear search
    QShortcut* clearShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(clearShortcut, &QShortcut::activated, this, [this]() {
        if (!m_searchWidget->getSearchText().isEmpty()) {
            m_searchWidget->clearSearch();
        }
    });
}
```

---

## 5. Real-World Usage Examples

### Example 1: Search for Trading Symbol
```
User types: "ADANI"
Result: Shows all rows containing "ADANI" (ADANIENT, ADANIGREEN, etc.)
```

### Example 2: Search for Product Type
```
User types: "NRML"
Result: Shows only NRML product positions
```

### Example 3: Search for Account
```
User types: "A0068"
Result: Shows all positions for account A0068
```

### Example 4: Combined with Column Filters
```
User has Exchange filter = "NSEFO"
User searches: "ADANI"
Result: Shows only NSEFO positions containing "ADANI"
```

---

## 6. Performance Optimization

### Debounce Search Input

```cpp
class SearchFilterWidget : public QWidget {
    // ... existing code ...
    
private:
    QTimer* m_debounceTimer;
};

SearchFilterWidget::SearchFilterWidget(QWidget* parent)
    : QWidget(parent)
{
    // ... existing setup ...
    
    // Debounce timer to avoid filtering on every keystroke
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(300); // 300ms delay
    
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() {
        m_debounceTimer->start();
    });
    
    connect(m_debounceTimer, &QTimer::timeout, this, [this]() {
        emit searchTextChanged(m_searchEdit->text());
    });
}
```

### Cache Search Results

```cpp
class SmartFilterProxyModel : public PinnedRowProxyModel {
private:
    mutable QMap<QString, QSet<int>> m_searchCache; // searchText -> matching rows
    
    bool matchesSearchText(int sourceRow) const {
        // Check cache first
        if (m_searchCache.contains(m_searchText)) {
            return m_searchCache[m_searchText].contains(sourceRow);
        }
        
        // Perform search and cache result
        bool matches = performSearch(sourceRow);
        
        if (!m_searchCache.contains(m_searchText)) {
            m_searchCache[m_searchText] = QSet<int>();
        }
        
        if (matches) {
            m_searchCache[m_searchText].insert(sourceRow);
        }
        
        return matches;
    }
    
    void invalidateSearchCache() {
        m_searchCache.clear();
    }
};
```

---

## 7. Testing

### Manual Test Checklist

- [ ] Search with empty string (shows all rows)
- [ ] Search with single character
- [ ] Search with multiple words
- [ ] Search with special characters
- [ ] Case sensitive ON/OFF
- [ ] Different search modes (Contains, Starts With, Exact)
- [ ] Search + column filters combined
- [ ] Search + sorting
- [ ] Performance with 1000+ rows
- [ ] Clear search button
- [ ] Keyboard shortcuts (Ctrl+F, Escape)
- [ ] Search history autocomplete

### Unit Test Example

```cpp
void TestSmartFilterProxyModel::testSearchFilter() {
    // Setup
    PositionModel sourceModel;
    SmartFilterProxyModel proxyModel;
    proxyModel.setSourceModel(&sourceModel);
    
    // Add test data
    PositionData pos1;
    pos1.symbol = "ADANIENT 27JAN2026";
    pos1.exchange = "NSEFO";
    sourceModel.addPosition(pos1);
    
    PositionData pos2;
    pos2.symbol = "RELIANCE 27JAN2026";
    pos2.exchange = "NSEFO";
    sourceModel.addPosition(pos2);
    
    // Test 1: Empty search shows all
    QCOMPARE(proxyModel.rowCount(), 2);
    
    // Test 2: Search for "ADANI" shows 1 row
    proxyModel.setSearchText("ADANI");
    QCOMPARE(proxyModel.rowCount(), 1);
    
    // Test 3: Search for "NSEFO" shows both
    proxyModel.setSearchText("NSEFO");
    QCOMPARE(proxyModel.rowCount(), 2);
    
    // Test 4: Clear search
    proxyModel.setSearchText("");
    QCOMPARE(proxyModel.rowCount(), 2);
}
```

---

## Complete Implementation Checklist

- [ ] Create `SearchFilterWidget.h` and `.cpp`
- [ ] Add search methods to `SmartFilterProxyModel`
- [ ] Integrate search widget in `PositionWindow`
- [ ] Add keyboard shortcuts
- [ ] Implement search highlighting (optional)
- [ ] Add search history (optional)
- [ ] Add debounce for performance
- [ ] Test with large datasets
- [ ] Update documentation
- [ ] Deploy to production

---

## Summary

This implementation provides:
✅ Real-time search across multiple columns  
✅ Multiple search modes (Contains, Starts With, Exact)  
✅ Case sensitivity toggle  
✅ Search result statistics  
✅ Keyboard shortcuts  
✅ Performance optimization with debouncing  
✅ Integration with existing filter system  
✅ Optional search highlighting  
✅ Optional search history  

**Time to implement**: 4-6 hours  
**Difficulty**: Medium  
**Dependencies**: Qt 5.15+, existing proxy model architecture
