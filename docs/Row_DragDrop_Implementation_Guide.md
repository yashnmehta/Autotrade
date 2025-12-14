# Row Drag & Drop Implementation Guide

## Overview
This guide explains how to implement row drag & drop for the MarketWatchWindow, allowing users to reorder scrips by dragging rows with the mouse.

## Current Status
- ✅ Column drag & drop (working)
- ⏳ Row drag & drop (needs implementation)

## Implementation Strategy

### Approach 1: Enable Built-in QTableView Drag & Drop (Recommended)

This approach uses Qt's built-in drag-drop support with minimal custom code.

#### Step 1: Update MarketWatchModel Header

Add these methods to `MarketWatchModel` class:

```cpp
// In include/ui/MarketWatchModel.h

public:
    // Drag & Drop support
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions() const override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent, int destinationChild) override;
```

#### Step 2: Implement Drag & Drop in MarketWatchModel

Add this to `src/ui/MarketWatchModel.cpp`:

```cpp
Qt::ItemFlags MarketWatchModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractTableModel::flags(index);
    
    if (index.isValid()) {
        return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
    } else {
        return Qt::ItemIsDropEnabled | defaultFlags;
    }
}

Qt::DropActions MarketWatchModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

bool MarketWatchModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                                 const QModelIndex &destinationParent, int destinationChild)
{
    // Validate input
    if (sourceRow < 0 || sourceRow + count - 1 >= m_scrips.count() ||
        destinationChild < 0 || destinationChild > m_scrips.count()) {
        return false;
    }
    
    // Same position - no move needed
    if (sourceRow == destinationChild || sourceRow == destinationChild - 1) {
        return false;
    }
    
    // Calculate destination row accounting for removal
    int destRow = destinationChild;
    if (destinationChild > sourceRow) {
        destRow -= count;
    }
    
    // Begin move operation
    if (!beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1,
                       destinationParent, destinationChild)) {
        return false;
    }
    
    // Move the rows
    QList<ScripData> movedScrips;
    for (int i = 0; i < count; ++i) {
        movedScrips.append(m_scrips.at(sourceRow));
        m_scrips.removeAt(sourceRow);
    }
    
    for (int i = 0; i < count; ++i) {
        m_scrips.insert(destRow + i, movedScrips.at(i));
    }
    
    endMoveRows();
    
    qDebug() << "[MarketWatchModel] Moved" << count << "row(s) from" << sourceRow << "to" << destRow;
    
    return true;
}
```

#### Step 3: Enable Drag & Drop in MarketWatchWindow

Update `setupUI()` in `MarketWatchWindow.cpp`:

```cpp
// Add after table view setup
m_tableView->setDragEnabled(true);
m_tableView->setAcceptDrops(true);
m_tableView->setDropIndicatorShown(true);
m_tableView->setDragDropMode(QAbstractItemView::InternalMove);
m_tableView->setDefaultDropAction(Qt::MoveAction);
```

#### Step 4: Update TokenAddressBook on Row Move

Connect to the model's rowsMoved signal in `MarketWatchWindow::setupConnections()`:

```cpp
// Connect to rowsMoved for address book updates
connect(m_model, &QAbstractItemModel::rowsMoved,
        this, [this](const QModelIndex &, int start, int end,
                     const QModelIndex &, int destination) {
    int count = end - start + 1;
    for (int i = 0; i < count; ++i) {
        int oldRow = start + i;
        int newRow = (destination > start) ? destination - count + i : destination + i;
        
        const ScripData &scrip = m_model->getScripAt(newRow);
        if (scrip.isValid()) {
            m_tokenAddressBook->onRowMoved(oldRow, newRow);
        }
    }
});
```

---

### Approach 2: Custom Drag & Drop with Visual Feedback (Advanced)

This approach provides more control over the drag-drop experience with custom visual indicators.

#### Step 1: Create Custom Drag Indicator Widget

```cpp
// In MarketWatchWindow.h
private:
    QWidget *m_dragIndicator;
    void showDragIndicator(int row);
    void hideDragIndicator();
```

#### Step 2: Override Mouse Events

```cpp
// In MarketWatchWindow.h
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    
private:
    QPoint m_dragStartPos;
    int m_dragSourceRow;
    bool m_isDragging;
```

#### Step 3: Implement Mouse Event Handlers

```cpp
// In MarketWatchWindow.cpp

void MarketWatchWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QModelIndex index = m_tableView->indexAt(m_tableView->viewport()->mapFromGlobal(event->globalPos()));
        if (index.isValid()) {
            m_dragStartPos = event->pos();
            m_dragSourceRow = mapToSource(index.row());
            m_isDragging = false;
        }
    }
    QWidget::mousePressEvent(event);
}

void MarketWatchWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    
    if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    
    if (!m_isDragging && m_dragSourceRow >= 0) {
        m_isDragging = true;
        // Create drag
        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData;
        
        // Store row data
        const ScripData &scrip = m_model->getScripAt(m_dragSourceRow);
        QString data = QString("%1\t%2\t%3").arg(scrip.symbol).arg(scrip.exchange).arg(scrip.token);
        mimeData->setText(data);
        
        drag->setMimeData(mimeData);
        drag->exec(Qt::MoveAction);
        
        m_isDragging = false;
    }
    
    QWidget::mouseMoveEvent(event);
}
```

---

## Testing Checklist

After implementing, test these scenarios:

### Basic Drag & Drop
- [ ] Drag single row up
- [ ] Drag single row down
- [ ] Drag row to top
- [ ] Drag row to bottom
- [ ] Drag multiple selected rows

### Edge Cases
- [ ] Drag row onto itself (should do nothing)
- [ ] Drag to invalid location
- [ ] Drag blank rows
- [ ] Drag after sorting (verify behavior with proxy model)

### Integration
- [ ] Verify TokenAddressBook updates correctly
- [ ] Verify price updates still work after reordering
- [ ] Verify Buy/Sell actions work on reordered rows
- [ ] Verify delete works after reordering

### Visual Feedback
- [ ] Drop indicator shows between rows
- [ ] Cursor changes during drag
- [ ] Dragged row highlights or shows feedback

---

## Known Limitations

1. **Drag & Drop with Sorting**: When table is sorted, dragging rows may not work intuitively since the visual order doesn't match the model order. Consider disabling drag-drop when sorting is active.

2. **Proxy Model Compatibility**: With QSortFilterProxyModel active, you may need to disable sorting temporarily during drag operations.

## Recommended Solution

**Use Approach 1** for simplicity and Qt best practices. It provides:
- ✅ Built-in visual indicators
- ✅ Proper event handling
- ✅ Minimal custom code
- ✅ Standard Qt behavior

Add this warning to users:
```cpp
// Disable sorting when user wants to manually reorder
if (m_tableView->isSortingEnabled()) {
    QMessageBox::information(this, "Manual Reordering",
        "To manually reorder rows, please disable sorting first.\n\n"
        "Click any column header to disable sorting.");
}
```

---

## Integration with Proxy Model

**Important**: The QSortFilterProxyModel complicates drag & drop. You have two options:

### Option A: Disable Drag-Drop When Sorted (Recommended)
```cpp
void MarketWatchWindow::setupUI()
{
    // ...
    
    // Disable drag-drop when sorting is enabled
    connect(m_tableView->horizontalHeader(), &QHeaderView::sectionClicked,
            this, [this]() {
        // Sorting is now active, disable drag
        m_tableView->setDragEnabled(false);
        m_tableView->setAcceptDrops(false);
    });
}
```

### Option B: Work with Proxy Model Indices
```cpp
// Map all drag-drop operations through proxy model
// This is complex and may lead to unexpected behavior
```

**Recommendation**: Use Option A and add a "Reset Order" button to clear sorting and re-enable manual reordering.

---

## Visual Enhancement: Custom Drop Indicator

For better UX, create a custom drop indicator line:

```cpp
// In MarketWatchWindow.h
private:
    QFrame *m_dropIndicatorLine;
    
// In MarketWatchWindow.cpp
void MarketWatchWindow::setupUI()
{
    // Create drop indicator line
    m_dropIndicatorLine = new QFrame(m_tableView->viewport());
    m_dropIndicatorLine->setFrameShape(QFrame::HLine);
    m_dropIndicatorLine->setLineWidth(2);
    m_dropIndicatorLine->setStyleSheet("background-color: #0078d4;");
    m_dropIndicatorLine->setFixedHeight(2);
    m_dropIndicatorLine->hide();
}

void MarketWatchWindow::showDropIndicator(int row)
{
    QRect rowRect = m_tableView->visualRect(m_proxyModel->index(row, 0));
    m_dropIndicatorLine->setGeometry(0, rowRect.top() - 1, 
                                     m_tableView->viewport()->width(), 2);
    m_dropIndicatorLine->show();
    m_dropIndicatorLine->raise();
}
```

---

## Complete Implementation Files

### 1. Update include/ui/MarketWatchModel.h

Add to public section:
```cpp
// Drag & Drop support
Qt::ItemFlags flags(const QModelIndex &index) const override;
Qt::DropActions supportedDropActions() const override;
bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
              const QModelIndex &destinationParent, int destinationChild) override;
```

### 2. Update src/ui/MarketWatchModel.cpp

Add the three methods shown in Approach 1.

### 3. Update src/ui/MarketWatchWindow.cpp

In `setupUI()`:
```cpp
// Enable row drag & drop (disabled when sorting is active)
m_tableView->setDragEnabled(true);
m_tableView->setAcceptDrops(true);
m_tableView->setDropIndicatorShown(true);
m_tableView->setDragDropMode(QAbstractItemView::InternalMove);
m_tableView->setDefaultDropAction(Qt::MoveAction);

// Disable drag-drop when sorting
connect(m_tableView->horizontalHeader(), &QHeaderView::sectionClicked,
        this, [this](int logicalIndex) {
    Q_UNUSED(logicalIndex);
    // Sorting is now active
    m_tableView->setDragEnabled(false);
    m_tableView->setAcceptDrops(false);
    
    qDebug() << "[MarketWatchWindow] Drag-drop disabled due to sorting."
             << "Right-click header and select 'Clear Sorting' to re-enable.";
});
```

---

## Next Steps

1. ✅ **Sorting fixed** - QSortFilterProxyModel implemented
2. ⏳ **Implement drag-drop** - Follow Approach 1 above
3. ⏳ **Test thoroughly** - Use testing checklist
4. ⏳ **Add UI settings** - See UI_Customization_Guide.md

---

## Summary

**Recommended Implementation Path**:
1. Add `flags()`, `supportedDropActions()`, `moveRows()` to MarketWatchModel
2. Enable drag-drop in MarketWatchWindow::setupUI()
3. Disable drag-drop when sorting is active
4. Update TokenAddressBook on rowsMoved signal
5. Test all scenarios

**Estimated Time**: 2-3 hours including testing

**Complexity**: Medium (Qt provides most of the infrastructure)

**User Experience**: Users can drag rows to reorder, but must disable sorting first for intuitive behavior.
