# Scroll Synchronization Bug Analysis — ATMWatch & OptionChain

## Executive Summary

**Page Up / Page Down / Arrow Key scroll synchronization is broken** in both ATMWatch and OptionChain windows. Only the focused table scrolls; sibling tables remain stationary.

**Root Cause**: `QSignalBlocker` in the tri-directional sync lambdas blocks **ALL** receivers of `QScrollBar::valueChanged`, including Qt's **internal** `_q_vslc()` slot that calls `scrollContentsBy()`. The scrollbar *value* is updated but the *viewport* never scrolls.

---

## Architecture Overview

### ATMWatch (3 tables)
```
┌───────────┐  ┌──────────────┐  ┌───────────┐
│ callTable  │  │ symbolTable  │  │ putTable   │
│ ScrollOff  │  │ ScrollAsNeed │  │ ScrollOff  │
└─────┬──────┘  └──────┬───────┘  └─────┬──────┘
      │                │                │
      └──── scroll sync (tri-directional) ──┘
```

### OptionChain (3 tables)
```
┌───────────┐  ┌──────────────┐  ┌───────────┐
│ callTable  │  │ strikeTable  │  │ putTable   │
│ ScrollOff  │  │ ScrollAsNeed │  │ ScrollOff  │
└─────┬──────┘  └──────┬───────┘  └─────┬──────┘
      │                │                │
      └──── scroll sync (tri-directional) ──┘
```

---

## Bug #1: QSignalBlocker Kills Viewport Scrolling (CRITICAL)

### How Qt Scrollbar → Viewport Works

```
QScrollBar::setValue(v)
    → emits valueChanged(v)
        → QAbstractScrollArea::_q_vslc()  [INTERNAL Qt slot]
            → scrollContentsBy(dx, dy)
                → viewport actually moves
```

### What Our Code Does (BROKEN)

```cpp
// ATMWatch setupConnections() — current code
connect(m_callTable->verticalScrollBar(), &QScrollBar::valueChanged, this,
    [this](int value) {
        QSignalBlocker b1(m_symbolTable->verticalScrollBar());  // ← BLOCKS ALL SIGNALS
        QSignalBlocker b2(m_putTable->verticalScrollBar());     // ← BLOCKS ALL SIGNALS
        m_symbolTable->verticalScrollBar()->setValue(value);     // value set, NO scroll
        m_putTable->verticalScrollBar()->setValue(value);        // value set, NO scroll
    });
```

### What Happens Step-by-Step

1. User presses **Page Down** on `callTable`
2. Qt's `QTableView::keyPressEvent` → moves selection → calls `scrollTo()` → `callTable` scrollbar `setValue()`  
3. `callTable->verticalScrollBar()->valueChanged(v)` fires normally
4. Qt's internal `_q_vslc()` fires → `callTable` viewport scrolls ✓
5. Our lambda fires →  
   - `QSignalBlocker b1(symbolVB)` — **blocks ALL signals from symbolVB**  
   - `symbolVB->setValue(v)` — internal value updates, but `valueChanged` is **suppressed**  
   - Qt's `_q_vslc()` for symbolTable **never called** → viewport does NOT scroll ✗
6. Same for `putTable` → viewport does NOT scroll ✗

**Result**: Only `callTable` scrolls. `symbolTable` and `putTable` stay frozen.

### The Same Bug in OptionChain

Identical pattern — `QSignalBlocker` on `strikeVB`, `callVB`, `putVB` prevents sibling viewports from scrolling.

---

## Bug #2: Wheel Event Handler Inconsistency (OptionChain)

### Current OptionChain eventFilter
```cpp
// Sets all three manually — but sync lambda interferes
m_strikeTable->verticalScrollBar()->setValue(newValue);
m_callTable->verticalScrollBar()->setValue(newValue);   // ← often a no-op (already set by sync)
m_putTable->verticalScrollBar()->setValue(newValue);     // ← often a no-op
```

**Problem sequence**:
1. `strikeVB->setValue(newValue)` → fires `valueChanged` → sync lambda blocks call+put
2. `callVB->setValue(newValue)` → value already `newValue` → no signal → no viewport scroll
3. `putVB->setValue(newValue)` → same

Only `strikeTable` viewport actually scrolls from wheel events.

### ATMWatch eventFilter (same issue)
```cpp
m_symbolTable->verticalScrollBar()->setValue(newValue);
// Only symbolTable is set → sync lambda blocks call+put → they don't scroll
```

---

## Bug #3: Dead `synchronizeScrollBars()` Methods

Both files retain the old `synchronizeScrollBars(int value)` slot:
- **ATMWatch** (line 904): Uses `QSignalBlocker` — same broken pattern
- **OptionChain** (line 951): Doesn't use blocker but is unreferenced

These are disconnected dead code — the tri-directional lambdas replaced them. They should be cleaned up.

---

## Bug #4: OptionChain `showEvent` Race Condition

```cpp
void OptionChainWindow::showEvent(QShowEvent *event) {
    QTimer::singleShot(200, this, [this]() {
        m_callTable->setFocus();
        if (m_callModel->rowCount() > 0 && !m_callTable->currentIndex().isValid()) {
            m_callTable->selectRow(targetRow);  // scrolls callTable
        }
    });
}
```

The 200ms timer fires after `refreshData`'s 0ms ATM-scroll timer. But `selectRow()` on callTable triggers a viewport scroll. With the broken sync, only callTable moves. With the fixed sync, this works correctly.

---

## The Fix: Replace QSignalBlocker with Re-entrancy Guard

### Why a Flag Works

```cpp
bool m_syncingScroll = false;

connect(callVB, &QScrollBar::valueChanged, this, [this](int v) {
    if (m_syncingScroll) return;       // prevent infinite loop
    m_syncingScroll = true;
    strikeVB->setValue(v);             // signal fires → viewport scrolls ✓
                                       // our handler re-enters → early return (flag)
    putVB->setValue(v);                // signal fires → viewport scrolls ✓
    m_syncingScroll = false;
});
```

**Key difference**: Signals are NOT blocked. Qt's internal `_q_vslc()` slot receives `valueChanged` normally and scrolls the viewport. The flag only prevents our own handlers from re-entering.

### Signal Flow (Fixed)

```
User presses PageDown on callTable
  → callTable scrollbar setValue(42)
  → valueChanged(42) emits
  → [Qt internal] callTable viewport scrolls ✓
  → [Our handler] flag=true
      → strikeVB.setValue(42) → valueChanged(42) emits
          → [Qt internal] strikeTable viewport scrolls ✓
          → [Our handler] flag=true → return (skip)
      → putVB.setValue(42) → valueChanged(42) emits
          → [Qt internal] putTable viewport scrolls ✓
          → [Our handler] flag=true → return (skip)
      → flag=false
```

All three viewports scroll. No infinite loop.

---

## Additional Optimizations

### 1. Simplify eventFilter (both windows)
Only set the master table's scrollbar for wheel events. Let tri-directional sync propagate:
```cpp
// OptionChain: only set strikeTable, sync handles call+put
m_strikeTable->verticalScrollBar()->setValue(newValue);
return true;
```

### 2. Remove dead methods
Remove `synchronizeScrollBars()` from both `.h` and `.cpp` files.

### 3. Consistent row height
Ensure all three tables use identical row heights so scrollbar ranges match exactly:
```cpp
table->verticalHeader()->setDefaultSectionSize(ROW_HEIGHT);
```

---

## Files To Modify

| File | Change |
|------|--------|
| `include/ui/ATMWatchWindow.h` | Add `bool m_syncingScroll = false;`, remove `synchronizeScrollBars` |
| `src/ui/ATMWatchWindow.cpp` | Replace QSignalBlocker lambdas with flag, simplify eventFilter, remove dead method |
| `include/ui/OptionChainWindow.h` | Add `bool m_syncingScroll = false;`, remove `synchronizeScrollBars` |
| `src/ui/OptionChainWindow.cpp` | Replace QSignalBlocker lambdas with flag, simplify eventFilter, remove dead method |

---

## Test Matrix

| Action | Expected | Window |
|--------|----------|--------|
| Page Down on call table | All 3 tables scroll down one page | Both |
| Page Up on put table | All 3 tables scroll up one page | Both |
| Arrow Down on strike/symbol table | All 3 tables follow | Both |
| Mouse wheel on any table | All 3 tables scroll | Both |
| Ctrl+G (go to ATM) | All 3 tables center on ATM | OptionChain |
| Escape → focus ATM row | All 3 tables jump to ATM | OptionChain |
| showEvent auto-focus | All 3 tables at ATM row | OptionChain |
