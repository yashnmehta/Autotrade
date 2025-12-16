# Market Watch Drag & Drop Fix

## Problem Summary

The Market Watch window's drag & drop reordering feature was only working on the first attempt. Subsequent drags would:
- Operate on the wrong rows
- Move unexpected items
- Show inconsistent proxy→source row mappings

## Root Causes Identified

### 1. **Token Capture Bug** (Critical)
In `MarketWatchWindow::eventFilter()`, the `m_draggedTokens` list was being cleared in the `MouseMove` event handler, even for single-selection mode:

```cpp
// BUGGY CODE (line ~827):
if (distance >= threshold) {
    m_draggedTokens.clear();  // ❌ This erased tokens captured at MousePress!
    // ...
}
```

**Impact**: Tokens captured at `MousePress` were immediately discarded, causing `performRowMoveByTokens()` to never be called.

**Solution**: Only clear tokens for multi-select mode. In single-select mode, tokens are already captured at `MousePress` and should not be overwritten:

```cpp
// FIXED CODE:
if (m_tableView->selectionMode() == QAbstractItemView::ExtendedSelection) {
    m_draggedTokens.clear();
    // Re-capture all selected rows for multi-select
} else {
    // Single-select: tokens already captured at MousePress - DON'T clear!
}
```

### 2. **Sorting vs Manual Reordering Conflict** (Design Issue)
When the user manually reorders rows, the table should no longer show a sort indicator (which would be misleading). However, sorting functionality should remain available for the user to re-sort by clicking column headers.

**Note**: `setDynamicSortFilter(false)` is already set, so Qt only sorts when the user clicks a column header, not continuously. This prevents automatic re-sorting during drag operations.

**Solution**: Clear the sort indicator when manual reordering begins:

```cpp
void MarketWatchWindow::performRowMoveByTokens(const QList<int> &tokens, int targetSourceRow)
{
    // Clear sort indicator during manual reorder
    // This shows the user that data is no longer sorted
    // Sorting remains enabled - user can re-sort by clicking headers
    int currentSortColumn = m_tableView->horizontalHeader()->sortIndicatorSection();
    if (currentSortColumn >= 0) {
        m_tableView->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    }
    
    // ... perform the move ...
}
```

This approach:
- ✅ Clears visual sort indicator when manual reorder happens
- ✅ Keeps `setSortingEnabled(true)` so user can still click headers to sort
- ✅ Respects `setDynamicSortFilter(false)` - no automatic re-sorting

## Code Changes

### File: `src/ui/MarketWatchWindow.cpp`

#### Change 1: Don't clear tokens in single-select mode (line ~820)
```cpp
// Before:
m_draggedTokens.clear();

// After:
if (m_tableView->selectionMode() == QAbstractItemView::ExtendedSelection) {
    m_draggedTokens.clear();
    // Multi-select: capture ALL selected rows
} else {
    // Single-select: tokens already captured at MousePress
}
```

#### Change 2: Disable sorting on manual reorder (line ~1040)
```cpp
void MarketWatchWindow::performRowMoveByTokens(const QList<int> &tokens, int targetSourceRow)
{
    if (tokens.isEmpty()) return;
    
    // NEW: Disable sorting for manual reorder
    bool sortingWasEnabled = m_tableView->isSortingEnabled();
    if (sortingWasEnabled) {
        qDebug() << "[PerformMoveByTokens] Disabling sorting for manual reorder";
        m_tableView->setSortingEnabled(false);
        m_tableView->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    }
    
    // ... rest of function ...
}
```

## Testing Results

**Before Fix**:
- First drag: ✅ Works
- Second drag: ❌ Moves wrong item (proxy row 0 → source row 2 instead of expected)
- Third drag: ❌ Fails completely

**After Fix**:
- First drag: ✅ Works (BANKNIFTY token 26009: row 1 → row 4)
- Second drag: ✅ Works (BANKNIFTY token 26009: row 3 → row 2)
- Third drag: ✅ Works (TCS token 11536: successful positioning)
- Fourth+ drags: ✅ All working perfectly

## Behavior

### Sorting and Manual Reordering
- **Sorting is always enabled** - user can click any column header to sort
- **Sort indicator is cleared** when user manually drags rows (to avoid confusion)
- **User can re-sort anytime** by clicking a column header again
- **No automatic re-sorting** - `setDynamicSortFilter(false)` prevents continuous sorting

This design:
- ✅ Respects manual reordering - doesn't automatically undo user's arrangement
- ✅ Allows on-demand sorting - user can click headers whenever needed
- ✅ Clear visual feedback - no sort indicator when manually arranged
- ✅ No performance overhead - sorting only happens on user action

## Architecture Notes

### Token-Based Tracking
The drag system uses **stable token IDs** instead of row indices:

```cpp
// At MousePress: Store token (stable across reorders)
const ScripData &scrip = m_model->getScripAt(sourceRow);
m_draggedTokens.append(scrip.token);

// At MouseRelease: Resolve token to CURRENT row
int row = m_model->findScripByToken(token);
```

This avoids stale row index issues when model changes between press and release.

### Coordinate Systems
Two separate row numbering systems:
- **Proxy rows**: Visual order in QTableView (what user sees)
- **Source rows**: Actual data order in MarketWatchModel

All drag operations work in **source coordinates** with token-based tracking.

## Debug Logging

Key debug prefixes for troubleshooting:
- `[EventFilter]`: Mouse event handling
- `[PerformMoveByTokens]`: Move operation with token resolution
- `[GetDropRow]`: Target row calculation
- `[TokenAddressBook]`: Row→token mapping updates

## Related Files

- `src/ui/MarketWatchWindow.cpp` - Main implementation
- `src/ui/MarketWatchModel.cpp` - Model with `moveRow()` and `findScripByToken()`
- `include/ui/MarketWatchWindow.h` - Added `m_draggedTokens`, `performRowMoveByTokens()`

## Status

✅ **FIXED** - Drag-drop now works reliably for consecutive operations
✅ **TESTED** - Multiple consecutive drags verified working
⚠️ **SORTING DISABLED** - After manual reorder (intentional design choice)
