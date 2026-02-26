# ATM Watch & Option Chain — Complete Code Audit & Suggestions

## Table of Contents
1. [Active Bugs](#1-active-bugs)
2. [Performance Issues](#2-performance-issues)
3. [Robustness / Crash Risks](#3-robustness--crash-risks)
4. [Architecture Debt](#4-architecture-debt)
5. [UX / Keyboard Navigation Gaps](#5-ux--keyboard-navigation-gaps)
6. [Feature Suggestions](#6-feature-suggestions)

---

## 1. Active Bugs

### BUG-01: ATM Watch eventFilter Only Routes Through symbolTable (MEDIUM)

**File**: `ATMWatchWindow.cpp` line ~925  
**Code**:
```cpp
bool ATMWatchWindow::eventFilter(QObject *obj, QEvent *event) {
    // ...
    int currentValue = m_symbolTable->verticalScrollBar()->value();
    int step = m_symbolTable->verticalScrollBar()->singleStep();
    int newValue = currentValue - (delta > 0 ? step : -step);
    m_symbolTable->verticalScrollBar()->setValue(newValue);
    return true;
}
```

**Problem**: When the wheel event happens over `callTable` or `putTable` viewport, it reads `symbolTable`'s scrollbar step and sets `symbolTable`'s value. This works because tri-directional sync propagates from symbolTable → others. BUT if symbolTable has a different `singleStep()` than call/put (e.g. due to different row heights or column configs), the scroll distance will be wrong.

**Fix**: Use the originating table's scrollbar for step calculation, then set the master table:
```cpp
QScrollBar *originBar = static_cast<QTableView*>(
    qobject_cast<QWidget*>(obj)->parentWidget())->verticalScrollBar();
int step = originBar->singleStep();
```

---

### BUG-02: OptionChain `m_strikes` Duplicated Assignment (LOW)

**File**: `OptionChainWindow.cpp` line ~1067-1068 (in `refreshData()`)  
**Code**:
```cpp
m_strikes = sortedStrikes;
m_strikes = sortedStrikes;  // ← DUPLICATE LINE
```

**Problem**: Harmless but wasteful and indicates copy-paste artifact.

---

### BUG-03: OptionChain Destructor Does NOT Unsubscribe From Feed (HIGH)

**File**: `OptionChainWindow.cpp` line ~124  
**Code**:
```cpp
OptionChainWindow::~OptionChainWindow() {}
```

**Compare with ATMWatch**:
```cpp
ATMWatchWindow::~ATMWatchWindow() {
    FeedHandler::instance().unsubscribeAll(this);
}
```

**Problem**: When an OptionChain window is closed, all feed subscriptions (potentially 100+ tokens for NIFTY chain) remain active and deliver ticks to a dangling `this` pointer → **use-after-free crash**.

**Fix**:
```cpp
OptionChainWindow::~OptionChainWindow() {
    FeedHandler::instance().unsubscribeAll(this);
}
```

---

### BUG-04: OptionChain `updateStrikeData()` Dereferences Nullable Items (HIGH)

**File**: `OptionChainWindow.cpp` lines ~750-850  
**Code**:
```cpp
m_callModel->item(row, CALL_OI)->setText(QString::number(data.callOI));
```

**Problem**: `QStandardItemModel::item(row, col)` can return `nullptr` if the row was never populated at that column. If `refreshData()` hasn't been called yet, or if a tick comes in between `clearData()` and model repopulation, this will crash with a null-pointer dereference.

**Fix**: Add null-check or use the safer `setData(index, value)` pattern:
```cpp
if (auto *item = m_callModel->item(row, CALL_OI))
    item->setText(QString::number(data.callOI));
```

---

### BUG-05: OptionChain `onTickUpdate` Writes to `m_strikeData` Reference After Clear (MEDIUM)

**File**: `OptionChainWindow.cpp` line ~1400  
**Code**:
```cpp
OptionStrikeData &data = m_strikeData[strike];
```

**Problem**: During `refreshData()`, `clearData()` clears `m_strikeData`. If a tick fires between `clearData()` and the model rebuild, `m_tokenToStrike` may still have the old token→strike mapping. The `operator[]` on `m_strikeData` would silently insert a default `OptionStrikeData` and then `updateStrikeData()` would crash on nullptr items.

**Fix**: Check `m_strikeData.contains(strike)` first (already partially done but not consistently applied).

---

### BUG-06: ATM Watch `refreshData()` Calls `onTickUpdate()` Synchronously During Setup (LOW)

**File**: `ATMWatchWindow.cpp` line ~530  
**Code**:
```cpp
onTickUpdate(tick);  // called inside refreshData's loop
```

**Problem**: `onTickUpdate` calls `updateItemWithColor()` which requires `model->item(row, col)` to exist. But `insertRow()` only creates the row — it does NOT create `QStandardItem` objects for each cell. `model->item(row, col)` returns `nullptr` until data is set via `setData()` or `setItem()`. The `updateItemWithColor` function handles this with a null-check and creates items on-demand, so it works. But it's fragile — any future caller that doesn't do the null-check will crash.

---

### BUG-07: F1/F2 in OptionChain Both Emit Same `tradeRequested` Signal (MEDIUM)

**File**: `OptionChainWindow.cpp` lines ~680-705

**Code**: F1 (Buy) and F2 (Sell) handlers emit identical signals:
```cpp
// F1:
emit tradeRequested(m_currentSymbol, m_currentExpiry,
                    getStrikeAtRow(m_selectedCallRow), "CE");
// F2:
emit tradeRequested(m_currentSymbol, m_currentExpiry,
                    getStrikeAtRow(m_selectedCallRow), "CE");
```

**Problem**: F2 should be "Sell" but emits the same signal as F1 "Buy". There is no Buy/Sell distinction in the signal — the receiver cannot distinguish intent.

**Fix**: Either add a `side` parameter to `tradeRequested` signal, or emit separate `buyRequested`/`sellRequested` signals.

---

### BUG-08: OptionChain `setStrikeRange()` Has Floating-Point Accumulation Error (LOW)

**File**: `OptionChainWindow.cpp` line ~860  
```cpp
for (double strike = minStrike; strike <= maxStrike; strike += interval)
```

**Problem**: Accumulating floating-point additions causes drift (e.g., 100.0 + 50.0 * 40 iterations might produce 2099.999999 instead of 2100.0). Use integer loop counter and multiply:
```cpp
int count = (int)((maxStrike - minStrike) / interval) + 1;
for (int i = 0; i < count; ++i)
    m_strikes.append(minStrike + i * interval);
```

---

## 2. Performance Issues

### PERF-01: OptionChain `m_strikes.indexOf(strike)` is O(N) on Every Tick (HIGH)

**File**: `OptionChainWindow.cpp` in `setupConnections()` Greeks handler and `updateStrikeData()`

**Problem**: `QList::indexOf()` does a linear scan. For NIFTY with ~200 strikes, every tick triggers O(200) search. With 200 tokens × 10 ticks/sec = 2000 linear scans/sec.

**Fix**: Add `QMap<double, int> m_strikeToRow` populated once during `refreshData()`:
```cpp
for (int i = 0; i < m_strikes.size(); ++i)
    m_strikeToRow[m_strikes[i]] = i;
```
Then replace `m_strikes.indexOf(strike)` with `m_strikeToRow.value(strike, -1)` — O(log N).

---

### PERF-02: ATM Watch `updateDataIncrementally()` Never Removes Deleted Rows (MEDIUM)

**File**: `ATMWatchWindow.cpp` lines ~600-700  

**Problem**: The code detects symbols to remove (Step 1 has a `continue` for missing symbols) but never actually removes the model rows. Stale rows accumulate. The `m_symbolToRow` map retains dangling row indices.

**Fix**: Collect removed symbols, delete rows in reverse order, then compact `m_symbolToRow`.

---

### PERF-03: OptionChain `refreshData()` Does `QList::append` in Loop Instead of Batch (LOW)

**Problem**: `for (row : callRows) m_callModel->appendRow(row);` — each `appendRow` triggers a `layoutChanged` internally even with `setUpdatesEnabled(false)`, because the model's internal structure still changes per-row.

**Fix**: Build a flat `QList<QStandardItem*>` and set all items at once, or use `m_callModel->setRowCount(sortedStrikes.size())` first, then `setItem()` individually.

---

### PERF-04: No UI Update Throttling on Tick Updates (MEDIUM)

Both windows process every tick immediately into the model. With 50+ active symbols each producing 10+ ticks/sec, the UI thread could be overloaded.

**Fix**: Coalesce updates with a 50-100ms timer:
```cpp
m_dirtyRows.insert(row);
if (!m_updateTimer->isActive())
    m_updateTimer->start(50);
```

---

### PERF-05: ATM Watch `getCurrentContext()` Fetches All ATM Data on Every Key Press (LOW)

**File**: `ATMWatchWindow.cpp` line ~1450  
```cpp
auto atmList = ATMWatchManager::getInstance().getATMWatchArray();
```

**Problem**: `getATMWatchArray()` likely copies the entire vector. This runs on F1/F2/F6/Delete. If the list has 500 symbols, this is unnecessary overhead.

**Fix**: Add `ATMWatchManager::getATMInfoForSymbol(const QString&)` for single-symbol lookup.

---

## 3. Robustness / Crash Risks

### CRASH-01: OptionChain Greeks Handler Dereferences Model Items Without Null Check

**File**: `OptionChainWindow.cpp` line ~450+  
```cpp
m_callModel->item(row, CALL_IV)->setText(...);
```

If `item(row, CALL_IV)` returns `nullptr` (row not yet populated), this crashes.

---

### CRASH-02: ATM Watch `loadAllSymbols()` Captures `this` in QtConcurrent Lambda

**File**: `ATMWatchWindow.cpp` line ~985  
```cpp
QtConcurrent::run([this, repo]() { ... });
```

**Problem**: If the window is destroyed while the lambda is running on a background thread, `this` becomes dangling. The `QMetaObject::invokeMethod` calls at the end would then access freed memory.

**Fix**: Use a weak pointer guard:
```cpp
QPointer<ATMWatchWindow> self = this;
QtConcurrent::run([self, repo]() {
    if (!self) return;
    // ... work ...
    QMetaObject::invokeMethod(self.data(), ...);
});
```

---

### CRASH-03: OptionChain `getSelectedContext()` Uses Wrong Token Lookup

**File**: `OptionChainWindow.cpp` line ~1310  
```cpp
contract = RepositoryManager::getInstance()->getContractByToken("NSEFO", token);
```

**Problem**: `getContractByToken` takes (exchange, segment, token) but this passes "NSEFO" as exchange. The actual exchange should be "NSE" with segment "FO", or the segment value `m_exchangeSegment`. This could return `nullptr` and context would be incomplete.

---

## 4. Architecture Debt

### ARCH-01: Delegate Classes Defined in Header Files

`ATMWatchDelegate` is entirely implemented in [ATMWatchWindow.h](include/ui/ATMWatchWindow.h) (90 lines). `OptionChainDelegate` is implemented in [OptionChainWindow.cpp](src/ui/OptionChainWindow.cpp). Both approaches are inconsistent and the header inline is heavy.

**Suggestion**: Move both delegates to their own files or at least consistently into `.cpp`.

---

### ARCH-02: Massive God Methods

- `ATMWatchWindow::updateDataIncrementally()` — ~200 lines
- `ATMWatchWindow::sortATMList()` — ~60 lines
- `ATMWatchWindow::getCurrentContext()` — ~100 lines  
- `OptionChainWindow::refreshData()` — ~200 lines

**Suggestion**: Extract data preparation into service/helper classes. The view should only call `model->setData()`.

---

### ARCH-03: Duplicate Code for Greeks Updates

Both windows have nearly identical lambda blocks for `GreeksCalculationService::greeksCalculated`:
- ATMWatch: ~30 lines setting call/put model data
- OptionChain: ~40 lines setting call/put model data

**Suggestion**: Create a shared utility or use a signal→slot pattern in the model layer.

---

### ARCH-04: OptionStrikeData Struct is Huge and Passed by Value

`OptionStrikeData` has 30+ fields and is passed by value in `updateStrikeData(double, const OptionStrikeData&)` — the const-ref is fine, but in `onTickUpdate` the data is stored by value in `m_strikeData[strike] = data;`.

**Suggestion**: Store pointers or use `std::shared_ptr<OptionStrikeData>` to avoid large copies.

---

### ARCH-05: No Model/View Separation

Both windows directly manipulate `QStandardItemModel` from UI code. Data logic (subscriptions, price updates, Greeks) is mixed with view logic (colors, highlighting, scrolling).

**Suggestion**: Introduce proper `QAbstractTableModel` subclasses (`ATMWatchModel`, `OptionChainCallModel`, etc.) that encapsulate data and emit `dataChanged()`. The window then only handles view concerns.

---

## 5. UX / Keyboard Navigation Gaps

### UX-01: No Row Highlight Sync Between Tables (ATM Watch)

When callTable row 3 is selected, symbolTable and putTable clear their selection (exclusive selection logic). This means the user loses visual context of which symbol corresponds to the selected call.

**Suggestion**: Instead of clearing selection, highlight (but don't select) the corresponding row in sibling tables using a custom highlight delegate or `QItemDelegate::State_MouseOver`-style painting.

---

### UX-02: No Current-Row Indicator When Switching Tables

When pressing Ctrl+Right to move from Call → Symbol → Put, `focusTable()` calls `selectRow()`. But if selection is exclusive, this clears the previous table's selection. The user can't see "where they were" in the previous table.

**Suggestion**: Track a `m_currentRow` variable per window, always paint a subtle horizontal line across all 3 tables at that row.

---

### UX-03: No Status Bar or Row Count Indicator (OptionChain)

ATMWatch has `m_statusLabel` showing "Loaded 50 symbols". OptionChain has no equivalent — user doesn't know if data loaded, how many strikes are available, or if loading is in progress.

**Suggestion**: Add a status bar showing: `"NIFTY | 26DEC2025 | 180 strikes | ATM: 24500 | Last update: 10:30:15"`

---

### UX-04: Ctrl+G in ATM Watch Goes to Row 0, Not ATM

**File**: `ATMWatchWindow.cpp` line ~118  
```cpp
m_symbolTable->scrollToTop();
m_symbolTable->selectRow(0);
```

**Problem**: ATM Watch doesn't have a concept of "ATM row" — row 0 is just the first symbol alphabetically (or by sort order). The label says "Jumped to ATM row" which is misleading.

**Fix**: Either rename to "Go to Top" or actually find and scroll to the current symbol's ATM strike.

---

### UX-05: No Visual Feedback for Active Shortcuts

User has no way to discover keyboard shortcuts without reading source code.

**Suggestion**: Add a shortcut help overlay triggered by `F10` or `?`:
```
Ctrl+Right/Left  — Cycle tables
Ctrl+E           — Open Exchange/Expiry
Ctrl+G           — Jump to ATM
F1/F2            — Buy/Sell
F5               — Refresh
Escape           — Return to main table
```

---

### UX-06: OptionChain ExtendedSelection Mode Causes Confusion

**File**: `OptionChainWindow.cpp` — all 3 tables use `ExtendedSelection`  
ATMWatch uses `SingleSelection`.

**Problem**: `ExtendedSelection` allows Ctrl+Click and Shift+Click multi-row selection. For an option chain where you typically act on one strike at a time, this can lead to confusing states.

**Suggestion**: Switch to `SingleSelection` unless multi-select has a specific purpose (e.g., building spreads).

---

## 6. Feature Suggestions

### FEAT-01: Spread Builder from OptionChain Checkboxes

The OptionChain already has checkbox columns for both Call and Put. These appear unused.

**Suggestion**: When 2+ checkboxes are selected, show a floating panel:
- Strategy name (auto-detect: Bull Call Spread, Iron Condor, etc.)
- Net premium, max profit, max loss
- Payoff chart
- "Execute Spread" button

---

### FEAT-02: ITM/OTM Color Gradient

Currently only ATM row is highlighted. Professional platforms color ITM strikes with a gradient (deeper color = deeper ITM).

**Suggestion**:
```
Deep ITM Call  → dark green background, fading to transparent at ATM
Deep OTM Call  → subtle grey
Deep ITM Put   → dark red background, fading to transparent at ATM
```

---

### FEAT-03: PCR (Put-Call Ratio) Per Strike

Add a PCR column or overlay showing `putOI / callOI` per strike. This is one of the most-viewed indicators by options traders.

---

### FEAT-04: OI Change Heatmap

Color the "Chng in OI" column with a heat gradient — large positive changes in bright green, large negative in red. This instantly shows where money is flowing.

---

### FEAT-05: Auto-Refresh Timer with Configurable Interval

ATMWatch has `m_basePriceTimer` at 1s for LTP. OptionChain has no auto-refresh.

**Suggestion**: Add a configurable refresh interval (1s, 5s, 30s) for the full option chain data, not just tick-by-tick fields.

---

### FEAT-06: Quick Straddle/Strangle View

Add a keyboard shortcut (e.g., `Ctrl+T`) that shows a mini popup at the current strike:
```
NIFTY 24500 Straddle:
  CE LTP: 245.50  |  PE LTP: 198.30
  Combined: 443.80
  Breakeven: 24056 / 24944
```

---

### FEAT-07: Cross-Window ATMWatch→OptionChain Deep Link

Currently Enter on ATMWatch opens OptionChain with the symbol+expiry. Enhance this to also:
- Auto-scroll to the ATM strike shown in ATMWatch
- Pre-select the Call side at that exact strike
- Maintain a "return to ATMWatch" breadcrumb

---

### FEAT-08: Column Visibility Toggle

Both windows show ALL columns by default. Many traders only care about LTP, IV, OI, Delta.

**Suggestion**: Right-click on header → checkbox list of columns to show/hide. Persist choice in `config.ini`.

---

### FEAT-09: Max Pain Indicator on OptionChain

Calculate and display Max Pain strike (the price at which the maximum number of options expire worthless). Show as a horizontal marker line or highlighted strike row.

---

### FEAT-10: Volatility Smile Chart

Add a toggle (`Ctrl+V`?) that shows a mini IV vs Strike chart overlaid on the OptionChain, using the IV data already available.

---

## Summary Priority Matrix

| ID | Severity | Effort | Description |
|----|----------|--------|-------------|
| BUG-03 | **CRITICAL** | 1 line | OptionChain destructor missing unsubscribeAll → crash |
| BUG-04 | **HIGH** | 30 min | Null item dereference in updateStrikeData |
| CRASH-01 | **HIGH** | 30 min | Null item dereference in Greeks handler |
| CRASH-02 | **HIGH** | 10 min | Dangling `this` in QtConcurrent lambda |
| BUG-07 | **MEDIUM** | 15 min | F1/F2 emit identical trade signals |
| BUG-05 | **MEDIUM** | 10 min | Tick writes to cleared strikeData |
| PERF-01 | **HIGH** | 20 min | O(N) indexOf on every tick → O(log N) map |
| PERF-04 | **MEDIUM** | 1 hour | Tick update throttling |
| PERF-02 | **MEDIUM** | 1 hour | Stale rows never removed |
| UX-03 | **LOW** | 15 min | Add status bar to OptionChain |
| UX-05 | **LOW** | 30 min | Shortcut help overlay |
| FEAT-01 | **HIGH VALUE** | 2 days | Spread builder from checkboxes |
| FEAT-02 | **MEDIUM** | 2 hours | ITM/OTM color gradient |
| FEAT-08 | **MEDIUM** | 3 hours | Column visibility toggle |
