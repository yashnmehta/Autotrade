# Market Watch - Advanced Features Design

## Overview
This document covers the implementation of advanced Market Watch features:
1. **Column Profile Management** - Save/load column configurations
2. **Blank Row Insertion** - Visual separators for organizing scrips
3. **Token Subscription Manager** - Exchange-wise subscription tracking
4. **Token Address Book** - Fast lookups for price updates
5. **Duplicate Prevention** - One token per watch list

---

## 1. Column Profile System

### Purpose
Allow users to save/load different column arrangements (order, width, visibility) for different trading strategies.

### Design

#### ColumnProfile Class
```cpp
// include/ui/ColumnProfile.h
#ifndef COLUMNPROFILE_H
#define COLUMNPROFILE_H

#include <QString>
#include <QMap>
#include <QVariant>

class ColumnProfile
{
public:
    ColumnProfile();
    explicit ColumnProfile(const QString &name);
    
    // Profile metadata
    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }
    
    // Column configuration
    QList<int> columnOrder() const { return m_columnOrder; }
    void setColumnOrder(const QList<int> &order) { m_columnOrder = order; }
    
    QMap<int, int> columnWidths() const { return m_columnWidths; }
    void setColumnWidths(const QMap<int, int> &widths) { m_columnWidths = widths; }
    
    QList<int> hiddenColumns() const { return m_hiddenColumns; }
    void setHiddenColumns(const QList<int> &hidden) { m_hiddenColumns = hidden; }
    
    // Serialization
    QVariantMap toVariantMap() const;
    void fromVariantMap(const QVariantMap &map);
    
    // Persistence
    void save(const QString &watchName) const;
    static ColumnProfile load(const QString &watchName, const QString &profileName);
    static QStringList availableProfiles(const QString &watchName);
    static void deleteProfile(const QString &watchName, const QString &profileName);

private:
    QString m_name;
    QList<int> m_columnOrder;        // Visual order of columns
    QMap<int, int> m_columnWidths;   // Column index -> width in pixels
    QList<int> m_hiddenColumns;      // List of hidden column indices
};

#endif // COLUMNPROFILE_H
```

#### Implementation Details
```cpp
// src/ui/ColumnProfile.cpp
#include "ui/ColumnProfile.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

ColumnProfile::ColumnProfile()
    : m_name("Default")
{
}

ColumnProfile::ColumnProfile(const QString &name)
    : m_name(name)
{
}

QVariantMap ColumnProfile::toVariantMap() const
{
    QVariantMap map;
    map["name"] = m_name;
    
    // Save column order
    QVariantList orderList;
    for (int col : m_columnOrder) {
        orderList.append(col);
    }
    map["columnOrder"] = orderList;
    
    // Save column widths
    QVariantMap widthsMap;
    for (auto it = m_columnWidths.constBegin(); it != m_columnWidths.constEnd(); ++it) {
        widthsMap[QString::number(it.key())] = it.value();
    }
    map["columnWidths"] = widthsMap;
    
    // Save hidden columns
    QVariantList hiddenList;
    for (int col : m_hiddenColumns) {
        hiddenList.append(col);
    }
    map["hiddenColumns"] = hiddenList;
    
    return map;
}

void ColumnProfile::fromVariantMap(const QVariantMap &map)
{
    m_name = map["name"].toString();
    
    // Load column order
    m_columnOrder.clear();
    QVariantList orderList = map["columnOrder"].toList();
    for (const QVariant &v : orderList) {
        m_columnOrder.append(v.toInt());
    }
    
    // Load column widths
    m_columnWidths.clear();
    QVariantMap widthsMap = map["columnWidths"].toMap();
    for (auto it = widthsMap.constBegin(); it != widthsMap.constEnd(); ++it) {
        m_columnWidths[it.key().toInt()] = it.value().toInt();
    }
    
    // Load hidden columns
    m_hiddenColumns.clear();
    QVariantList hiddenList = map["hiddenColumns"].toList();
    for (const QVariant &v : hiddenList) {
        m_hiddenColumns.append(v.toInt());
    }
}

void ColumnProfile::save(const QString &watchName) const
{
    QSettings settings("TradingTerminal", "MarketWatch");
    QString key = QString("ColumnProfiles/%1/%2").arg(watchName).arg(m_name);
    
    QVariantMap map = toVariantMap();
    settings.setValue(key, map);
}

ColumnProfile ColumnProfile::load(const QString &watchName, const QString &profileName)
{
    QSettings settings("TradingTerminal", "MarketWatch");
    QString key = QString("ColumnProfiles/%1/%2").arg(watchName).arg(profileName);
    
    QVariantMap map = settings.value(key).toMap();
    
    ColumnProfile profile;
    if (!map.isEmpty()) {
        profile.fromVariantMap(map);
    } else {
        profile.setName(profileName);
    }
    
    return profile;
}

QStringList ColumnProfile::availableProfiles(const QString &watchName)
{
    QSettings settings("TradingTerminal", "MarketWatch");
    QString groupKey = QString("ColumnProfiles/%1").arg(watchName);
    
    settings.beginGroup(groupKey);
    QStringList profiles = settings.childKeys();
    settings.endGroup();
    
    return profiles;
}

void ColumnProfile::deleteProfile(const QString &watchName, const QString &profileName)
{
    QSettings settings("TradingTerminal", "MarketWatch");
    QString key = QString("ColumnProfiles/%1/%2").arg(watchName).arg(profileName);
    settings.remove(key);
}
```

#### MarketWatchWindow Integration
```cpp
// Add to MarketWatchWindow class

private:
    ColumnProfile m_currentProfile;
    
public slots:
    void saveColumnProfile(const QString &profileName);
    void loadColumnProfile(const QString &profileName);
    void deleteColumnProfile(const QString &profileName);
    ColumnProfile captureCurrentProfile() const;
    void applyProfile(const ColumnProfile &profile);
    
private:
    void createProfileMenu();  // Add to toolbar/menu
```

```cpp
// Implementation

ColumnProfile MarketWatchWindow::captureCurrentProfile() const
{
    ColumnProfile profile;
    
    // Capture column order
    QList<int> order;
    QHeaderView *header = m_tableView->horizontalHeader();
    for (int visual = 0; visual < header->count(); ++visual) {
        int logical = header->logicalIndex(visual);
        order.append(logical);
    }
    profile.setColumnOrder(order);
    
    // Capture column widths
    QMap<int, int> widths;
    for (int col = 0; col < m_model->columnCount(); ++col) {
        widths[col] = m_tableView->columnWidth(col);
    }
    profile.setColumnWidths(widths);
    
    // Capture hidden columns
    QList<int> hidden;
    for (int col = 0; col < m_model->columnCount(); ++col) {
        if (m_tableView->isColumnHidden(col)) {
            hidden.append(col);
        }
    }
    profile.setHiddenColumns(hidden);
    
    return profile;
}

void MarketWatchWindow::applyProfile(const ColumnProfile &profile)
{
    QHeaderView *header = m_tableView->horizontalHeader();
    
    // Apply column order
    QList<int> order = profile.columnOrder();
    for (int visual = 0; visual < order.count(); ++visual) {
        int logical = order.at(visual);
        int currentVisual = header->visualIndex(logical);
        header->moveSection(currentVisual, visual);
    }
    
    // Apply column widths
    QMap<int, int> widths = profile.columnWidths();
    for (auto it = widths.constBegin(); it != widths.constEnd(); ++it) {
        m_tableView->setColumnWidth(it.key(), it.value());
    }
    
    // Apply hidden columns
    QList<int> hidden = profile.hiddenColumns();
    for (int col = 0; col < m_model->columnCount(); ++col) {
        m_tableView->setColumnHidden(col, hidden.contains(col));
    }
    
    m_currentProfile = profile;
}

void MarketWatchWindow::saveColumnProfile(const QString &profileName)
{
    ColumnProfile profile = captureCurrentProfile();
    profile.setName(profileName);
    profile.save(windowTitle());
    
    QMessageBox::information(this, "Profile Saved", 
                             QString("Column profile '%1' saved successfully.").arg(profileName));
}
```

---

## 2. Blank Row Insertion

### Purpose
Allow users to insert blank separator rows to organize scrips by strategy, sector, or any custom grouping.

### Design

#### Update ScripData Structure
```cpp
struct ScripData
{
    QString symbol;
    QString exchange;
    int token = 0;           // NEW: Unique token ID
    bool isBlankRow = false; // NEW: Mark separator rows
    
    // Price data
    double ltp = 0.0;
    double change = 0.0;
    double changePercent = 0.0;
    qint64 volume = 0;
    double bid = 0.0;
    double ask = 0.0;
    double high = 0.0;
    double low = 0.0;
    double open = 0.0;
    qint64 openInterest = 0;
    
    // Constructor for blank row
    static ScripData createBlankRow() {
        ScripData blank;
        blank.isBlankRow = true;
        blank.symbol = "───────────────";  // Visual separator
        return blank;
    }
};
```

#### MarketWatchModel Methods
```cpp
// Add to MarketWatchModel class

public:
    void insertBlankRow(int position);
    bool isBlankRow(int row) const;
    
// Implementation
void MarketWatchModel::insertBlankRow(int position)
{
    if (position < 0 || position > m_scrips.count()) {
        position = m_scrips.count();  // Append at end
    }
    
    beginInsertRows(QModelIndex(), position, position);
    m_scrips.insert(position, ScripData::createBlankRow());
    endInsertRows();
}

bool MarketWatchModel::isBlankRow(int row) const
{
    if (row >= 0 && row < m_scrips.count()) {
        return m_scrips.at(row).isBlankRow;
    }
    return false;
}
```

#### Context Menu Integration
```cpp
void MarketWatchWindow::showContextMenu(const QPoint &pos)
{
    QModelIndex index = m_tableView->indexAt(pos);
    
    QMenu menu(this);
    
    // ... existing menu items ...
    
    menu.addSeparator();
    
    // Insert blank row options
    if (index.isValid()) {
        menu.addAction("Insert Blank Row Above", this, [this, index]() {
            m_model->insertBlankRow(index.row());
        });
        menu.addAction("Insert Blank Row Below", this, [this, index]() {
            m_model->insertBlankRow(index.row() + 1);
        });
    } else {
        menu.addAction("Insert Blank Row", this, [this]() {
            m_model->insertBlankRow(m_model->rowCount());
        });
    }
    
    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}
```

#### Custom Rendering for Blank Rows
```cpp
// In MarketWatchDelegate::paint()

void MarketWatchDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    // Check if blank row
    bool isBlank = index.data(Qt::UserRole + 100).toBool();  // Custom role
    
    if (isBlank) {
        // Paint as separator
        painter->fillRect(option.rect, QColor("#2d2d30"));
        
        if (index.column() == 0) {
            painter->setPen(QColor("#606060"));
            painter->drawLine(option.rect.left(), option.rect.center().y(),
                            option.rect.right(), option.rect.center().y());
        }
        return;
    }
    
    // Normal cell painting
    QStyledItemDelegate::paint(painter, option, index);
}
```

---

## 3. Token Subscription Manager

### Purpose
Track which tokens need real-time price updates, organized by exchange for efficient API calls.

### Design

```cpp
// include/api/TokenSubscriptionManager.h
#ifndef TOKENSUBSCRIPTIONMANAGER_H
#define TOKENSUBSCRIPTIONMANAGER_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QString>

class TokenSubscriptionManager : public QObject
{
    Q_OBJECT

public:
    static TokenSubscriptionManager* instance();
    
    // Subscription management
    void subscribe(const QString &exchange, int token);
    void unsubscribe(const QString &exchange, int token);
    void unsubscribeAll(const QString &exchange);
    void clearAll();
    
    // Query subscriptions
    QSet<int> getSubscribedTokens(const QString &exchange) const;
    QList<QString> getSubscribedExchanges() const;
    bool isSubscribed(const QString &exchange, int token) const;
    int totalSubscriptions() const;
    
    // Batch operations
    void subscribeBatch(const QString &exchange, const QList<int> &tokens);
    void unsubscribeBatch(const QString &exchange, const QList<int> &tokens);
    
signals:
    void tokenSubscribed(const QString &exchange, int token);
    void tokenUnsubscribed(const QString &exchange, int token);
    void exchangeSubscriptionsChanged(const QString &exchange);

private:
    explicit TokenSubscriptionManager(QObject *parent = nullptr);
    static TokenSubscriptionManager *s_instance;
    
    // Exchange -> Set of token IDs
    QMap<QString, QSet<int>> m_subscriptions;
};

#endif // TOKENSUBSCRIPTIONMANAGER_H
```

#### Implementation
```cpp
// src/api/TokenSubscriptionManager.cpp
#include "api/TokenSubscriptionManager.h"
#include <QDebug>

TokenSubscriptionManager* TokenSubscriptionManager::s_instance = nullptr;

TokenSubscriptionManager* TokenSubscriptionManager::instance()
{
    if (!s_instance) {
        s_instance = new TokenSubscriptionManager(nullptr);
    }
    return s_instance;
}

TokenSubscriptionManager::TokenSubscriptionManager(QObject *parent)
    : QObject(parent)
{
}

void TokenSubscriptionManager::subscribe(const QString &exchange, int token)
{
    if (!m_subscriptions.contains(exchange)) {
        m_subscriptions[exchange] = QSet<int>();
    }
    
    if (!m_subscriptions[exchange].contains(token)) {
        m_subscriptions[exchange].insert(token);
        emit tokenSubscribed(exchange, token);
        emit exchangeSubscriptionsChanged(exchange);
        
        qDebug() << "[TokenSubscription] Subscribed:" << exchange << "Token:" << token
                 << "Total:" << m_subscriptions[exchange].size();
    }
}

void TokenSubscriptionManager::unsubscribe(const QString &exchange, int token)
{
    if (m_subscriptions.contains(exchange)) {
        if (m_subscriptions[exchange].remove(token)) {
            emit tokenUnsubscribed(exchange, token);
            emit exchangeSubscriptionsChanged(exchange);
            
            qDebug() << "[TokenSubscription] Unsubscribed:" << exchange << "Token:" << token
                     << "Remaining:" << m_subscriptions[exchange].size();
        }
        
        // Clean up empty exchange entries
        if (m_subscriptions[exchange].isEmpty()) {
            m_subscriptions.remove(exchange);
        }
    }
}

QSet<int> TokenSubscriptionManager::getSubscribedTokens(const QString &exchange) const
{
    return m_subscriptions.value(exchange, QSet<int>());
}

QList<QString> TokenSubscriptionManager::getSubscribedExchanges() const
{
    return m_subscriptions.keys();
}

bool TokenSubscriptionManager::isSubscribed(const QString &exchange, int token) const
{
    return m_subscriptions.value(exchange, QSet<int>()).contains(token);
}

int TokenSubscriptionManager::totalSubscriptions() const
{
    int total = 0;
    for (const QSet<int> &tokens : m_subscriptions) {
        total += tokens.size();
    }
    return total;
}

void TokenSubscriptionManager::subscribeBatch(const QString &exchange, const QList<int> &tokens)
{
    for (int token : tokens) {
        subscribe(exchange, token);
    }
}

void TokenSubscriptionManager::unsubscribeBatch(const QString &exchange, const QList<int> &tokens)
{
    for (int token : tokens) {
        unsubscribe(exchange, token);
    }
}

void TokenSubscriptionManager::unsubscribeAll(const QString &exchange)
{
    if (m_subscriptions.contains(exchange)) {
        m_subscriptions.remove(exchange);
        emit exchangeSubscriptionsChanged(exchange);
        qDebug() << "[TokenSubscription] Cleared all subscriptions for" << exchange;
    }
}

void TokenSubscriptionManager::clearAll()
{
    m_subscriptions.clear();
    qDebug() << "[TokenSubscription] Cleared all subscriptions";
}
```

#### Integration with MarketWatch
```cpp
// In MarketWatchWindow::addScrip()

void MarketWatchWindow::addScrip(const QString &symbol, const QString &exchange, int token)
{
    // Check for duplicates (see section 5)
    if (hasDuplicate(token)) {
        return;
    }
    
    ScripData scrip;
    scrip.symbol = symbol;
    scrip.exchange = exchange;
    scrip.token = token;
    
    m_model->addScrip(scrip);
    
    // Subscribe to price updates
    TokenSubscriptionManager::instance()->subscribe(exchange, token);
    
    // Update address book (see section 4)
    m_tokenAddressBook->addToken(token, m_model->rowCount() - 1);
}

// In MarketWatchWindow::removeScrip()

void MarketWatchWindow::removeScrip(int row)
{
    const ScripData &scrip = m_model->getScripAt(row);
    
    if (!scrip.isBlankRow) {
        // Unsubscribe from price updates
        TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange, scrip.token);
        
        // Update address book
        m_tokenAddressBook->removeToken(scrip.token, row);
    }
    
    m_model->removeScrip(row);
}
```

---

## 4. Token Address Book

### Purpose
Maintain bidirectional mapping between tokens and row indices for fast O(1) lookups during price updates.

### Design

```cpp
// include/ui/TokenAddressBook.h
#ifndef TOKENADDRESSBOOK_H
#define TOKENADDRESSBOOK_H

#include <QObject>
#include <QMap>
#include <QList>

class TokenAddressBook : public QObject
{
    Q_OBJECT

public:
    explicit TokenAddressBook(QObject *parent = nullptr);
    
    // Add/remove mappings
    void addToken(int token, int row);
    void removeToken(int token, int row);
    void clear();
    
    // Update on row operations
    void onRowMoved(int fromRow, int toRow);
    void onRowsInserted(int firstRow, int count);
    void onRowsRemoved(int firstRow, int count);
    
    // Lookups
    QList<int> getRowsForToken(int token) const;
    int getTokenForRow(int row) const;
    bool hasToken(int token) const;
    
    // Statistics
    int tokenCount() const { return m_tokenToRows.size(); }
    int rowCount() const { return m_rowToToken.size(); }
    
    // Debug
    void dump() const;

private:
    void updateRowIndices(int startRow, int delta);
    
    QMap<int, QList<int>> m_tokenToRows;  // Token -> List of row indices
    QMap<int, int> m_rowToToken;          // Row -> Token
};

#endif // TOKENADDRESSBOOK_H
```

#### Implementation
```cpp
// src/ui/TokenAddressBook.cpp
#include "ui/TokenAddressBook.h"
#include <QDebug>

TokenAddressBook::TokenAddressBook(QObject *parent)
    : QObject(parent)
{
}

void TokenAddressBook::addToken(int token, int row)
{
    // Add to token -> rows mapping
    if (!m_tokenToRows.contains(token)) {
        m_tokenToRows[token] = QList<int>();
    }
    
    if (!m_tokenToRows[token].contains(row)) {
        m_tokenToRows[token].append(row);
    }
    
    // Add to row -> token mapping
    m_rowToToken[row] = token;
    
    qDebug() << "[TokenAddressBook] Added token" << token << "at row" << row;
}

void TokenAddressBook::removeToken(int token, int row)
{
    // Remove from token -> rows mapping
    if (m_tokenToRows.contains(token)) {
        m_tokenToRows[token].removeAll(row);
        
        if (m_tokenToRows[token].isEmpty()) {
            m_tokenToRows.remove(token);
        }
    }
    
    // Remove from row -> token mapping
    m_rowToToken.remove(row);
    
    qDebug() << "[TokenAddressBook] Removed token" << token << "from row" << row;
}

void TokenAddressBook::clear()
{
    m_tokenToRows.clear();
    m_rowToToken.clear();
    qDebug() << "[TokenAddressBook] Cleared all mappings";
}

QList<int> TokenAddressBook::getRowsForToken(int token) const
{
    return m_tokenToRows.value(token, QList<int>());
}

int TokenAddressBook::getTokenForRow(int row) const
{
    return m_rowToToken.value(row, -1);
}

bool TokenAddressBook::hasToken(int token) const
{
    return m_tokenToRows.contains(token);
}

void TokenAddressBook::onRowMoved(int fromRow, int toRow)
{
    int token = m_rowToToken.value(fromRow, -1);
    if (token == -1) return;
    
    // Update row -> token mapping
    m_rowToToken.remove(fromRow);
    m_rowToToken[toRow] = token;
    
    // Update token -> rows mapping
    if (m_tokenToRows.contains(token)) {
        int idx = m_tokenToRows[token].indexOf(fromRow);
        if (idx != -1) {
            m_tokenToRows[token][idx] = toRow;
        }
    }
    
    qDebug() << "[TokenAddressBook] Moved token" << token << "from row" << fromRow << "to" << toRow;
}

void TokenAddressBook::onRowsInserted(int firstRow, int count)
{
    // Shift all row indices >= firstRow by count
    updateRowIndices(firstRow, count);
}

void TokenAddressBook::onRowsRemoved(int firstRow, int count)
{
    // Remove tokens for deleted rows
    for (int row = firstRow; row < firstRow + count; ++row) {
        int token = m_rowToToken.value(row, -1);
        if (token != -1) {
            removeToken(token, row);
        }
    }
    
    // Shift all row indices > firstRow by -count
    updateRowIndices(firstRow + count, -count);
}

void TokenAddressBook::updateRowIndices(int startRow, int delta)
{
    // Create new mappings
    QMap<int, int> newRowToToken;
    QMap<int, QList<int>> newTokenToRows;
    
    // Update row -> token mapping
    for (auto it = m_rowToToken.constBegin(); it != m_rowToToken.constEnd(); ++it) {
        int row = it.key();
        int token = it.value();
        
        if (row >= startRow) {
            newRowToToken[row + delta] = token;
        } else {
            newRowToToken[row] = token;
        }
    }
    
    // Update token -> rows mapping
    for (auto it = m_tokenToRows.constBegin(); it != m_tokenToRows.constEnd(); ++it) {
        int token = it.key();
        QList<int> rows = it.value();
        
        QList<int> newRows;
        for (int row : rows) {
            if (row >= startRow) {
                newRows.append(row + delta);
            } else {
                newRows.append(row);
            }
        }
        
        newTokenToRows[token] = newRows;
    }
    
    m_rowToToken = newRowToToken;
    m_tokenToRows = newTokenToRows;
}

void TokenAddressBook::dump() const
{
    qDebug() << "=== TokenAddressBook Dump ===";
    qDebug() << "Token -> Rows:";
    for (auto it = m_tokenToRows.constBegin(); it != m_tokenToRows.constEnd(); ++it) {
        qDebug() << "  Token" << it.key() << "-> Rows" << it.value();
    }
    qDebug() << "Row -> Token:";
    for (auto it = m_rowToToken.constBegin(); it != m_rowToToken.constEnd(); ++it) {
        qDebug() << "  Row" << it.key() << "-> Token" << it.value();
    }
    qDebug() << "=============================";
}
```

#### Integration with MarketWatch
```cpp
// Add to MarketWatchWindow class

private:
    TokenAddressBook *m_tokenAddressBook;

// In constructor
MarketWatchWindow::MarketWatchWindow(QWidget *parent)
    : QWidget(parent)
{
    m_tokenAddressBook = new TokenAddressBook(this);
    
    // Connect model signals
    connect(m_model, &MarketWatchModel::rowsInserted,
            this, [this](const QModelIndex &, int first, int last) {
        m_tokenAddressBook->onRowsInserted(first, last - first + 1);
    });
    
    connect(m_model, &MarketWatchModel::rowsRemoved,
            this, [this](const QModelIndex &, int first, int last) {
        m_tokenAddressBook->onRowsRemoved(first, last - first + 1);
    });
}

// Fast price update using address book
void MarketWatchWindow::onPriceUpdate(int token, double ltp, double change)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    
    for (int row : rows) {
        m_model->updatePrice(row, ltp, change);
    }
}
```

---

## 5. Duplicate Token Prevention

### Purpose
Ensure each token appears only once in a Market Watch to avoid confusion and redundant subscriptions.

### Design

#### Duplicate Check Method
```cpp
// Add to MarketWatchWindow class

public:
    bool hasDuplicate(int token) const;
    int findTokenRow(int token) const;

private:
    void highlightRow(int row);

// Implementation
bool MarketWatchWindow::hasDuplicate(int token) const
{
    return m_tokenAddressBook->hasToken(token);
}

int MarketWatchWindow::findTokenRow(int token) const
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    return rows.isEmpty() ? -1 : rows.first();
}

void MarketWatchWindow::highlightRow(int row)
{
    // Select and scroll to the existing row
    m_tableView->selectRow(row);
    m_tableView->scrollTo(m_model->index(row, 0));
    
    // Flash effect (optional)
    QTimer::singleShot(100, this, [this, row]() {
        m_tableView->clearSelection();
        QTimer::singleShot(100, this, [this, row]() {
            m_tableView->selectRow(row);
        });
    });
}
```

#### Updated addScrip() with Validation
```cpp
void MarketWatchWindow::addScrip(const QString &symbol, const QString &exchange, int token)
{
    // Validate token
    if (token <= 0) {
        QMessageBox::warning(this, "Invalid Token", 
                           QString("Cannot add scrip '%1': Invalid token ID.").arg(symbol));
        return;
    }
    
    // Check for duplicate
    if (hasDuplicate(token)) {
        int existingRow = findTokenRow(token);
        
        QMessageBox::information(this, "Duplicate Scrip",
                                QString("Scrip '%1' (Token: %2) is already in this watch list at row %3.")
                                .arg(symbol).arg(token).arg(existingRow + 1));
        
        // Highlight the existing row
        highlightRow(existingRow);
        return;
    }
    
    // Add scrip
    ScripData scrip;
    scrip.symbol = symbol;
    scrip.exchange = exchange;
    scrip.token = token;
    scrip.isBlankRow = false;
    
    int newRow = m_model->rowCount();
    m_model->addScrip(scrip);
    
    // Subscribe and register
    TokenSubscriptionManager::instance()->subscribe(exchange, token);
    m_tokenAddressBook->addToken(token, newRow);
    
    qDebug() << "[MarketWatch] Added scrip" << symbol << "with token" << token << "at row" << newRow;
}
```

#### Visual Feedback Options
```cpp
// Option 1: Show warning dialog with "Go to existing" button
void MarketWatchWindow::showDuplicateDialog(const QString &symbol, int token, int existingRow)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Duplicate Scrip");
    msgBox.setText(QString("Scrip '%1' is already in this watch list.").arg(symbol));
    msgBox.setInformativeText(QString("Token: %1\nRow: %2").arg(token).arg(existingRow + 1));
    msgBox.setIcon(QMessageBox::Information);
    
    QPushButton *goToBtn = msgBox.addButton("Go to Existing", QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Ok);
    
    msgBox.exec();
    
    if (msgBox.clickedButton() == goToBtn) {
        highlightRow(existingRow);
    }
}

// Option 2: Status bar notification (less intrusive)
void MarketWatchWindow::showDuplicateNotification(const QString &symbol, int existingRow)
{
    QString msg = QString("'%1' already exists at row %2").arg(symbol).arg(existingRow + 1);
    
    // Show in status bar if available
    if (QMainWindow *mainWin = qobject_cast<QMainWindow*>(window())) {
        mainWin->statusBar()->showMessage(msg, 3000);
    }
    
    highlightRow(existingRow);
}
```

---

## 6. Usage Examples

### Example 1: Save and Load Column Profile
```cpp
// User customizes columns and saves profile
m_marketWatch->saveColumnProfile("Intraday Trading");

// Later, load the profile
m_marketWatch->loadColumnProfile("Intraday Trading");

// Delete a profile
m_marketWatch->deleteColumnProfile("Old Profile");
```

### Example 2: Organize with Blank Rows
```cpp
// User adds scrips
m_marketWatch->addScrip("NIFTY 50", "NSE", 26000);
m_marketWatch->addScrip("BANKNIFTY", "NSE", 26009);

// Insert separator
m_marketWatch->insertBlankRow(2);

// Add more scrips
m_marketWatch->addScrip("RELIANCE", "NSE", 2885);
m_marketWatch->addScrip("TCS", "NSE", 11536);
```

### Example 3: Token Subscription
```cpp
// Automatically managed when adding/removing scrips
TokenSubscriptionManager *subMgr = TokenSubscriptionManager::instance();

// Check subscriptions
QSet<int> nseTokens = subMgr->getSubscribedTokens("NSE");
qDebug() << "NSE subscriptions:" << nseTokens.size();

// Get all exchanges with subscriptions
QList<QString> exchanges = subMgr->getSubscribedExchanges();
```

### Example 4: Fast Price Updates
```cpp
// When price update received from API
void onMarketDataUpdate(int token, double ltp, double change, double volume)
{
    // O(1) lookup using address book
    QList<int> rows = m_marketWatch->getTokenAddressBook()->getRowsForToken(token);
    
    for (int row : rows) {
        m_model->updatePrice(row, ltp, change, volume);
    }
}
```

---

## 7. Testing Checklist

### Column Profiles
- [ ] Save profile with custom column order
- [ ] Load profile restores exact layout
- [ ] Hidden columns are preserved
- [ ] Column widths are restored
- [ ] Multiple profiles can coexist
- [ ] Profile deletion works
- [ ] Profile survives app restart

### Blank Rows
- [ ] Insert blank row above
- [ ] Insert blank row below
- [ ] Blank rows render correctly
- [ ] Blank rows don't affect price updates
- [ ] Can delete blank rows
- [ ] Drag-drop works with blank rows
- [ ] Copy/paste handles blank rows

### Token Subscription
- [ ] Subscribe on add scrip
- [ ] Unsubscribe on remove scrip
- [ ] Multiple exchanges tracked separately
- [ ] No duplicate subscriptions
- [ ] Batch operations work
- [ ] Clear all subscriptions
- [ ] Signals emitted correctly

### Token Address Book
- [ ] Token-to-row mapping accurate
- [ ] Row-to-token mapping accurate
- [ ] Updates on row insert
- [ ] Updates on row remove
- [ ] Updates on row move
- [ ] Fast O(1) lookups
- [ ] Handles multiple Market Watch windows

### Duplicate Prevention
- [ ] Duplicate token rejected
- [ ] Warning dialog shown
- [ ] Existing row highlighted
- [ ] Works across add methods (manual, paste, load)
- [ ] Blank rows don't trigger duplicate check
- [ ] Token 0 or -1 handled gracefully

---

## 8. Performance Considerations

### Memory Usage
- **ColumnProfile**: ~1KB per profile (negligible)
- **TokenAddressBook**: ~16 bytes per token-row mapping
  - 1000 scrips ≈ 16KB (very efficient)
- **TokenSubscriptionManager**: ~8 bytes per subscription
  - 1000 tokens ≈ 8KB (minimal)

### CPU Performance
- **Token Lookup**: O(1) hash map lookup
- **Duplicate Check**: O(1) with address book
- **Price Updates**: O(1) row lookup, O(log n) model update
- **Profile Save/Load**: O(n) where n = column count (fast)

### Optimization Tips
1. Use `beginResetModel/endResetModel` for bulk operations
2. Batch subscribe/unsubscribe for better performance
3. Lazy-load column profiles (don't load all at startup)
4. Use move semantics for ScripData copies

---

## 9. Future Enhancements

### Column Profiles
- [ ] Export/import profiles to JSON
- [ ] Share profiles between users
- [ ] Auto-apply profile based on market hours
- [ ] Profile preview before applying

### Blank Rows
- [ ] Editable separator text
- [ ] Colored separators
- [ ] Collapsible sections (fold/unfold groups)

### Token Management
- [ ] Token conflict resolution across watches
- [ ] Smart subscription (subscribe only to visible rows)
- [ ] Token validity checking

---

**Ready to implement? Let me know which feature to start with!** 🚀

Recommended order:
1. Update ScripData with token field (affects everything)
2. Token Address Book (foundation for other features)
3. Duplicate Prevention (quality of life)
4. Token Subscription Manager (API integration)
5. Blank Rows (UX enhancement)
6. Column Profiles (power user feature)
