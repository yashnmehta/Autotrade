# Custom Market Watch - Implementation Roadmap

## Overview
This document provides a step-by-step guide to implementing the Custom Market Watch window with all planned features.

---

## Quick Start Summary

### What We're Building
A **professional Market Watch window** for the trading terminal with:
- **QTableView** for data display
- **Custom Model** for efficient data management
- **Custom Delegate** for color-coded rendering
- **Drag & Drop** for columns and rows
- **Context Menu** with trading actions
- **Clipboard Operations** (Ctrl+C/X/V)
- **Visual Customization** (fonts, colors, grid)
- **Persistence** (save/load watch lists)

### Key Technologies
- **Qt Model/View Framework**: QAbstractTableModel + QTableView
- **Custom Painting**: QStyledItemDelegate with QPainter
- **Drag & Drop**: Qt's drag-drop API
- **Settings**: QSettings for persistence
- **Signals/Slots**: For real-time updates

---

## Phase 1: Foundation Setup

### Step 1.1: Create Basic Classes

**Files to Create**:
```
include/ui/MarketWatchWindow.h
src/ui/MarketWatchWindow.cpp
include/ui/MarketWatchModel.h
src/ui/MarketWatchModel.cpp
```

**MarketWatchWindow.h** (Simplified Starter):
```cpp
#ifndef MARKETWATCHWINDOW_H
#define MARKETWATCHWINDOW_H

#include <QWidget>
#include <QTableView>
#include <QVBoxLayout>

class MarketWatchModel;

class MarketWatchWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MarketWatchWindow(QWidget *parent = nullptr);
    ~MarketWatchWindow();

    // Core operations
    void addScrip(const QString &symbol, const QString &exchange);
    void removeScrip(const QString &symbol);
    void clearAll();

private:
    void setupUI();
    void setupConnections();

    QTableView *m_tableView;
    MarketWatchModel *m_model;
    QVBoxLayout *m_layout;
};

#endif // MARKETWATCHWINDOW_H
```

**MarketWatchModel.h** (Simplified Starter):
```cpp
#ifndef MARKETWATCHMODEL_H
#define MARKETWATCHMODEL_H

#include <QAbstractTableModel>
#include <QList>

struct ScripData
{
    QString symbol;
    QString exchange;
    double ltp = 0.0;
    double change = 0.0;
    double changePercent = 0.0;
    qint64 volume = 0;
    double high = 0.0;
    double low = 0.0;
};

class MarketWatchModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        COL_SYMBOL = 0,
        COL_LTP,
        COL_CHANGE,
        COL_CHANGE_PERCENT,
        COL_VOLUME,
        COL_HIGH,
        COL_LOW,
        COL_COUNT  // Always last
    };

    explicit MarketWatchModel(QObject *parent = nullptr);

    // Required overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Data management
    void addScrip(const ScripData &scrip);
    void removeScrip(int row);
    void clearAll();
    int findScrip(const QString &symbol) const;

private:
    QList<ScripData> m_scrips;
    QStringList m_headers;
};

#endif // MARKETWATCHMODEL_H
```

### Step 1.2: Implement Basic Display

**MarketWatchWindow.cpp**:
```cpp
#include "ui/MarketWatchWindow.h"
#include "ui/MarketWatchModel.h"

MarketWatchWindow::MarketWatchWindow(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    setupConnections();
}

MarketWatchWindow::~MarketWatchWindow() = default;

void MarketWatchWindow::setupUI()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    // Create model
    m_model = new MarketWatchModel(this);

    // Create table view
    m_tableView = new QTableView(this);
    m_tableView->setModel(m_model);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSortingEnabled(true);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->setShowGrid(true);

    m_layout->addWidget(m_tableView);

    // Dark theme styling
    setStyleSheet(
        "QTableView {"
        "    background-color: #1e1e1e;"
        "    color: #ffffff;"
        "    gridline-color: #3f3f46;"
        "    selection-background-color: #094771;"
        "}"
        "QHeaderView::section {"
        "    background-color: #2d2d30;"
        "    color: #ffffff;"
        "    padding: 4px;"
        "    border: 1px solid #3f3f46;"
        "    font-weight: bold;"
        "}"
    );
}

void MarketWatchWindow::setupConnections()
{
    // Future: Connect signals
}

void MarketWatchWindow::addScrip(const QString &symbol, const QString &exchange)
{
    ScripData scrip;
    scrip.symbol = symbol;
    scrip.exchange = exchange;
    // Mock data for now
    scrip.ltp = 18500.50;
    scrip.change = 125.25;
    scrip.changePercent = 0.68;
    scrip.volume = 1250000;
    scrip.high = 18650.00;
    scrip.low = 18450.00;

    m_model->addScrip(scrip);
}

void MarketWatchWindow::removeScrip(const QString &symbol)
{
    int row = m_model->findScrip(symbol);
    if (row >= 0) {
        m_model->removeScrip(row);
    }
}

void MarketWatchWindow::clearAll()
{
    m_model->clearAll();
}
```

**MarketWatchModel.cpp**:
```cpp
#include "ui/MarketWatchModel.h"

MarketWatchModel::MarketWatchModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_headers << "Symbol" << "LTP" << "Change" << "%Change" << "Volume" << "High" << "Low";
}

int MarketWatchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_scrips.count();
}

int MarketWatchModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : COL_COUNT;
}

QVariant MarketWatchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_scrips.count())
        return QVariant();

    const ScripData &scrip = m_scrips.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case COL_SYMBOL: return scrip.symbol;
            case COL_LTP: return QString::number(scrip.ltp, 'f', 2);
            case COL_CHANGE: return QString::number(scrip.change, 'f', 2);
            case COL_CHANGE_PERCENT: return QString::number(scrip.changePercent, 'f', 2) + "%";
            case COL_VOLUME: return QString::number(scrip.volume);
            case COL_HIGH: return QString::number(scrip.high, 'f', 2);
            case COL_LOW: return QString::number(scrip.low, 'f', 2);
        }
    }
    else if (role == Qt::TextAlignmentRole) {
        if (index.column() == COL_SYMBOL)
            return Qt::AlignLeft + Qt::AlignVCenter;
        return Qt::AlignRight + Qt::AlignVCenter;
    }

    return QVariant();
}

QVariant MarketWatchModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section < m_headers.count())
            return m_headers.at(section);
    }
    return QVariant();
}

void MarketWatchModel::addScrip(const ScripData &scrip)
{
    beginInsertRows(QModelIndex(), m_scrips.count(), m_scrips.count());
    m_scrips.append(scrip);
    endInsertRows();
}

void MarketWatchModel::removeScrip(int row)
{
    if (row >= 0 && row < m_scrips.count()) {
        beginRemoveRows(QModelIndex(), row, row);
        m_scrips.removeAt(row);
        endRemoveRows();
    }
}

void MarketWatchModel::clearAll()
{
    beginResetModel();
    m_scrips.clear();
    endResetModel();
}

int MarketWatchModel::findScrip(const QString &symbol) const
{
    for (int i = 0; i < m_scrips.count(); ++i) {
        if (m_scrips.at(i).symbol == symbol)
            return i;
    }
    return -1;
}
```

### Step 1.3: Integrate with MainWindow

**In MainWindow.cpp**, add:
```cpp
#include "ui/MarketWatchWindow.h"

// In setupUI() or similar
MarketWatchWindow *mw = new MarketWatchWindow(this);
mw->setWindowTitle("Market Watch 1");
// Add to MDI area or layout
```

### Step 1.4: Connect ScripBar

**In ScripBar signal connection**:
```cpp
connect(m_scripBar, &ScripBar::addToWatchRequested,
        m_marketWatch, [this](const QString &exchange, const QString &segment,
                               const QString &instrument, const QString &symbol,
                               const QString &expiry, const QString &strike,
                               const QString &optionType) {
    m_marketWatch->addScrip(symbol, exchange);
});
```

### Step 1.5: Test Phase 1

**Expected Result**:
- ✅ Market Watch window displays
- ✅ Add scrip from ScripBar appears in table
- ✅ Columns show with headers
- ✅ Dark theme styling applied
- ✅ Can select rows
- ✅ Can sort by clicking headers

---

## Phase 2: Core Features

### Step 2.1: Enable Drag & Drop for Columns

**In MarketWatchWindow::setupUI()**:
```cpp
// Enable column reordering
m_tableView->horizontalHeader()->setSectionsMovable(true);
m_tableView->horizontalHeader()->setDragEnabled(true);
m_tableView->horizontalHeader()->setDragDropMode(QAbstractItemView::InternalMove);
```

### Step 2.2: Enable Drag & Drop for Rows

**Create custom row drag implementation** (complex - see detailed guide in separate doc).

Key steps:
1. Override `mousePressEvent()`, `mouseMoveEvent()`, `mouseReleaseEvent()`
2. Use `QMimeData` to store row data
3. Visual indicator during drag
4. Update model on drop

### Step 2.3: Delete Selected Rows

**In MarketWatchWindow**:
```cpp
void MarketWatchWindow::deleteSelectedRows()
{
    QModelIndexList selected = m_tableView->selectionModel()->selectedRows();
    
    // Sort in descending order to avoid index shifting
    QList<int> rows;
    for (const QModelIndex &index : selected) {
        rows.append(index.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    
    for (int row : rows) {
        m_model->removeScrip(row);
    }
}

// Add keyboard shortcut
void MarketWatchWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        deleteSelectedRows();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
```

### Step 2.4: Context Menu

**Add context menu**:
```cpp
void MarketWatchWindow::setupUI()
{
    // ... existing code ...
    
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested,
            this, &MarketWatchWindow::showContextMenu);
}

void MarketWatchWindow::showContextMenu(const QPoint &pos)
{
    QModelIndex index = m_tableView->indexAt(pos);
    
    QMenu menu(this);
    
    if (index.isValid()) {
        menu.addAction("Buy", this, &MarketWatchWindow::onBuyAction);
        menu.addAction("Sell", this, &MarketWatchWindow::onSellAction);
        menu.addSeparator();
    }
    
    menu.addAction("Add Scrip", this, &MarketWatchWindow::onAddScripAction);
    
    if (index.isValid()) {
        menu.addAction("Delete", this, &MarketWatchWindow::deleteSelectedRows);
        menu.addSeparator();
        menu.addAction("Copy", this, &MarketWatchWindow::copyToClipboard);
        menu.addAction("Cut", this, &MarketWatchWindow::cutToClipboard);
    }
    
    menu.addAction("Paste", this, &MarketWatchWindow::pasteFromClipboard);
    
    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}
```

---

## Phase 3: Clipboard Operations

### Step 3.1: Copy (Ctrl+C)

```cpp
void MarketWatchWindow::copyToClipboard()
{
    QModelIndexList selected = m_tableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;
    
    QString text;
    for (const QModelIndex &index : selected) {
        for (int col = 0; col < m_model->columnCount(); ++col) {
            QModelIndex cellIndex = m_model->index(index.row(), col);
            text += m_model->data(cellIndex, Qt::DisplayRole).toString();
            if (col < m_model->columnCount() - 1)
                text += "\t";  // Tab-separated
        }
        text += "\n";
    }
    
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text);
}
```

### Step 3.2: Cut (Ctrl+X)

```cpp
void MarketWatchWindow::cutToClipboard()
{
    copyToClipboard();
    deleteSelectedRows();
}
```

### Step 3.3: Paste (Ctrl+V)

```cpp
void MarketWatchWindow::pasteFromClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString text = clipboard->text();
    
    // Parse TSV format and add scrips
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QStringList fields = line.split('\t');
        if (!fields.isEmpty()) {
            QString symbol = fields.at(0);
            QString exchange = fields.size() > 1 ? fields.at(1) : "NSE";
            addScrip(symbol, exchange);
        }
    }
}
```

### Step 3.4: Keyboard Shortcuts

```cpp
void MarketWatchWindow::setupUI()
{
    // ... existing code ...
    
    // Copy shortcut
    QShortcut *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, &MarketWatchWindow::copyToClipboard);
    
    // Cut shortcut
    QShortcut *cutShortcut = new QShortcut(QKeySequence::Cut, this);
    connect(cutShortcut, &QShortcut::activated, this, &MarketWatchWindow::cutToClipboard);
    
    // Paste shortcut
    QShortcut *pasteShortcut = new QShortcut(QKeySequence::Paste, this);
    connect(pasteShortcut, &QShortcut::activated, this, &MarketWatchWindow::pasteFromClipboard);
    
    // Select All
    QShortcut *selectAllShortcut = new QShortcut(QKeySequence::SelectAll, this);
    connect(selectAllShortcut, &QShortcut::activated, m_tableView, &QTableView::selectAll);
}
```

---

## Phase 4: Visual Customization

### Step 4.1: Settings Dialog UI

Create `resources/forms/MarketWatchSettings.ui` using Qt Designer with:
- Checkbox: Show Grid
- Checkbox: Alternate Row Colors
- Font Selector: QPushButton to open QFontDialog
- Color Pickers: QPushButton to open QColorDialog for positive/negative colors

### Step 4.2: Settings Class

```cpp
class MarketWatchSettings
{
public:
    bool showGrid = true;
    bool alternateColors = true;
    QFont font = QFont("Arial", 10);
    QColor positiveColor = QColor("#00C853");
    QColor negativeColor = QColor("#FF1744");
    
    void save(const QString &watchName);
    void load(const QString &watchName);
};
```

### Step 4.3: Apply Settings

```cpp
void MarketWatchWindow::applySettings(const MarketWatchSettings &settings)
{
    m_tableView->setShowGrid(settings.showGrid);
    m_tableView->setAlternatingRowColors(settings.alternateColors);
    m_tableView->setFont(settings.font);
    
    // Apply colors through delegate (Phase 5)
}
```

---

## Phase 5: Custom Delegate (Color Coding)

### Step 5.1: Create Delegate Class

```cpp
class MarketWatchDelegate : public QStyledItemDelegate
{
public:
    explicit MarketWatchDelegate(QObject *parent = nullptr);
    
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
               
    void setPositiveColor(const QColor &color);
    void setNegativeColor(const QColor &color);
    
private:
    QColor m_positiveColor;
    QColor m_negativeColor;
};
```

### Step 5.2: Implement Color-Coded Painting

```cpp
void MarketWatchDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    // Get data
    QVariant value = index.data(Qt::DisplayRole);
    
    // Determine color based on column
    QColor textColor = option.palette.text().color();
    
    if (index.column() == MarketWatchModel::COL_CHANGE ||
        index.column() == MarketWatchModel::COL_CHANGE_PERCENT) {
        double changeValue = index.data(Qt::UserRole).toDouble();
        if (changeValue > 0)
            textColor = m_positiveColor;
        else if (changeValue < 0)
            textColor = m_negativeColor;
    }
    
    // Paint background
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    } else {
        painter->fillRect(option.rect, option.palette.base());
    }
    
    // Paint text
    painter->setPen(textColor);
    painter->drawText(option.rect, Qt::AlignRight | Qt::AlignVCenter, value.toString());
}
```

---

## Phase 6: Save & Load

### Step 6.1: Save Watch List

```cpp
void MarketWatchWindow::saveWatchList(const QString &filePath)
{
    QJsonObject json;
    json["name"] = windowTitle();
    
    QJsonArray scripsArray;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        // Get scrip data and add to array
    }
    json["scrips"] = scripsArray;
    
    // Write to file
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(json);
        file.write(doc.toJson());
    }
}
```

### Step 6.2: Load Watch List

```cpp
void MarketWatchWindow::loadWatchList(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject json = doc.object();
    
    setWindowTitle(json["name"].toString());
    
    m_model->clearAll();
    
    QJsonArray scripsArray = json["scrips"].toArray();
    for (const QJsonValue &value : scripsArray) {
        // Parse and add scrip
    }
}
```

---

## Summary & Next Steps

### What You'll Have After All Phases
1. ✅ Fully functional Market Watch with QTableView
2. ✅ Sortable columns (click headers)
3. ✅ Drag & drop columns and rows
4. ✅ Context menu with actions
5. ✅ Copy/Cut/Paste with Ctrl+C/X/V
6. ✅ Multi-row selection
7. ✅ Visual customization (fonts, colors, grid)
8. ✅ Save/load watch lists
9. ✅ Color-coded positive/negative values
10. ✅ Dark theme styling

### Recommended Implementation Order
1. **Week 1**: Phase 1 (Foundation) - Get basic table working
2. **Week 2**: Phases 2-3 (Core + Clipboard) - Add interactions
3. **Week 3**: Phases 4-5 (Customization + Delegate) - Polish UI
4. **Week 4**: Phase 6 + Testing - Persistence and bug fixes

### Resources for Learning
- **Qt Model/View Tutorial**: https://doc.qt.io/qt-5/modelview.html
- **QTableView Examples**: Qt Creator → Examples → Item Views
- **Drag & Drop Guide**: https://doc.qt.io/qt-5/dnd.html

---

**Ready to start? Let me know which phase to begin with!** 🚀

I recommend starting with **Phase 1** to get the foundation working, then we can iterate from there.
