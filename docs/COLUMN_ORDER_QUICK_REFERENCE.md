# Column Order Fix - Quick Reference

**Quick lookup for developers maintaining this code**

---

## 🔑 Key Changes Summary

### 1. **Enable Column Reordering**
**File:** `src/views/MarketWatchWindow/UI.cpp:62`
```cpp
horizontalHeader()->setSectionsMovable(true);
```
**Why:** Allows users to drag-and-drop columns

---

### 2. **Capture Visual Order from View**
**File:** `src/views/MarketWatchWindow/Actions.cpp:469-527`

**Key Concept:** Iterate through **visualIndex** (user's displayed order), not logical index

```cpp
for (int visualIdx = 0; visualIdx < header->count(); ++visualIdx) {
    int logicalIdx = header->logicalIndex(visualIdx);  // Visual → Logical
    // Get column at this visual position
    MarketWatchColumn col = logicalToColumn[logicalIdx];
    newVisibleOrder.append(col);  // Preserve visual order
}
```

**Why:** QHeaderView tracks visual order separately from logical order after drag-and-drop

---

### 3. **Apply Order to View**
**File:** `src/views/MarketWatchWindow/Actions.cpp:507-548`

**Key Concept:** Use `header->moveSection()` to reorder view columns

```cpp
for (int targetVisualIdx = 0; targetVisualIdx < visibleCols.size(); ++targetVisualIdx) {
    MarketWatchColumn col = visibleCols[targetVisualIdx];
    int logicalIdx = columnToLogicalIndex[col];
    int currentVisualIdx = header->visualIndex(logicalIdx);
    
    if (currentVisualIdx != targetVisualIdx) {
        header->moveSection(currentVisualIdx, targetVisualIdx);
    }
}
```

**Why:** View order must be explicitly set; model order alone isn't enough

---

### 4. **Don't Reset Model During Save**
**File:** `src/views/MarketWatchWindow/Actions.cpp:358-361`

**Before:**
```cpp
captureProfileFromView(currentProfile);
m_model->setColumnProfile(currentProfile);  // ❌ Triggers reset!
```

**After:**
```cpp
captureProfileFromView(currentProfile);
// Profile captured, ready to save - don't update model!
```

**Why:** `setColumnProfile()` triggers model reset, destroying view state

---

### 5. **Validate JSON Input**
**File:** `src/models/MarketWatchColumnProfile.cpp:785-828`

**Added checks:**
- Range validation: `colId >= 0 && colId < COLUMN_COUNT`
- Duplicate detection: `seen.contains(colId)`
- Completeness: Add missing columns automatically

**Why:** Prevents crashes from corrupted profile files

---

### 6. **Add Profile Versioning**
**File:** `src/models/MarketWatchColumnProfile.cpp:727-729`

```cpp
json["version"] = 2;
json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
```

**Why:** Future-proofs profile format, aids debugging

---

### 7. **Clear Before Init**
**File:** `src/models/MarketWatchColumnProfile.cpp:520-522`

```cpp
m_columnOrder.clear();
m_visibility.clear();
m_widths.clear();
```

**Why:** Prevents duplicates if initialization called multiple times

---

### 8. **Apply Profile on Restore**
**File:** `src/views/MarketWatchWindow/Actions.cpp:446-451`

**Before:**
```cpp
m_model->setColumnProfile(profile);
m_model->layoutChanged(); // ❌ Wrong signal
```

**After:**
```cpp
m_model->setColumnProfile(profile);
applyProfileToView(profile);  // ✅ Apply widths + order
```

**Why:** Consistent behavior between load portfolio and restore workspace

---

## 🧠 Mental Model

### QHeaderView Column Management

```
Logical Index:  0    1    2    3    4
                ↓    ↓    ↓    ↓    ↓
Columns:      [Code][Sym][LTP][Bid][Ask]
                
After user drags Symbol to end:

Logical Index:  0    1    2    3    4
                ↓    ↓    ↓    ↓    ↓
Columns:      [Code][Sym][LTP][Bid][Ask]
                ↓           ↓    ↓    ↓
Visual Index:   0    2    3    4    1
Display:      [Code][LTP][Bid][Ask][Sym]
```

**Key Functions:**
- `header->visualIndex(logicalIdx)` → Where is this logical column displayed?
- `header->logicalIndex(visualIdx)` → What logical column is at this position?
- `header->moveSection(from, to)` → Move visual from→to

---

## 🎯 Common Pitfalls

### ❌ DON'T:
```cpp
// This captures model order, not user's visual arrangement!
for (int i = 0; i < header->count(); ++i) {
    MarketWatchColumn col = modelColumns[i];  // Wrong!
}
```

### ✅ DO:
```cpp
// This captures actual visual order
for (int visualIdx = 0; visualIdx < header->count(); ++visualIdx) {
    int logicalIdx = header->logicalIndex(visualIdx);
    MarketWatchColumn col = logicalToColumn[logicalIdx];  // Correct!
}
```

---

### ❌ DON'T:
```cpp
// This only sets widths, ignores order!
for (int i = 0; i < visibleCols.size(); ++i) {
    header->resizeSection(i, width);  // Wrong!
}
```

### ✅ DO:
```cpp
// Move section to target position, then set width
header->moveSection(currentVisualIdx, targetVisualIdx);
header->resizeSection(logicalIdx, width);  // Correct!
```

---

## 📚 Related Qt Documentation

- [`QHeaderView::moveSection()`](https://doc.qt.io/qt-6/qheaderview.html#moveSection)
- [`QHeaderView::setSectionsMovable()`](https://doc.qt.io/qt-6/qheaderview.html#setSectionsMovable)
- [`QHeaderView::logicalIndex()`](https://doc.qt.io/qt-6/qheaderview.html#logicalIndex)
- [`QHeaderView::visualIndex()`](https://doc.qt.io/qt-6/qheaderview.html#visualIndex)
- [`QAbstractItemModel::layoutChanged()`](https://doc.qt.io/qt-6/qabstractitemmodel.html#layoutChanged)

---

## 🔍 Debugging Tips

### View Column Order Issues:
```cpp
// Log current visual order
qDebug() << "Visual order:";
for (int visualIdx = 0; visualIdx < header->count(); ++visualIdx) {
    int logicalIdx = header->logicalIndex(visualIdx);
    qDebug() << "  Visual" << visualIdx << "→ Logical" << logicalIdx;
}
```

### Profile Order Issues:
```cpp
// Log profile column order
QList<MarketWatchColumn> order = profile.columnOrder();
qDebug() << "Profile order:" << order.size() << "columns";
for (int i = 0; i < order.size(); ++i) {
    qDebug() << "  " << i << ":" 
             << MarketWatchColumnProfile::getColumnName(order[i]);
}
```

### Save/Load Issues:
```cpp
// Enable detailed logging in capture/apply functions
qDebug() << "[captureProfileFromView] Captured" << newVisibleOrder.size() 
         << "visible columns in visual order";
         
qDebug() << "[applyProfileToView] Applied column order and widths for" 
         << visibleCols.size() << "columns";
```

---

## 🚨 Important Notes

1. **Always use visualIndex for display order**
   - `visualIndex` = where user sees column
   - `logicalIndex` = model's internal index

2. **Model order != View order**
   - Model provides logical columns 0..N
   - View can display them in any visual order
   - Both must be synchronized

3. **Profile stores full order**
   - `columnOrder` includes ALL columns (visible + hidden)
   - `visibleColumns()` filters by visibility flag
   - Hidden columns preserve position in full order

4. **Don't trigger model reset unnecessarily**
   - Reset destroys view state
   - Use `layoutChanged()` or `headerDataChanged()` when possible
   - Only reset when column count changes

---

**Last Updated:** January 14, 2026  
**Maintainer:** Development Team
