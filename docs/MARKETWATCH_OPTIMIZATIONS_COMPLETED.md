# MarketWatch Optimizations — Completed

**Date**: 2 March 2026  
**Status**: ✅ All Phase 1-3 optimizations complete  
**Build Status**: ✅ Clean (0 errors)  
**Test Status**: ✅ All 83 tests passing  

---

## Summary

Implemented **11 fixes and optimizations** to the MarketWatch implementation, focusing on:
1. **Bug fixes** (light theme, out-of-range columns, context menu)
2. **Performance** (9× reduction in Qt signals, O(1) token lookup, batch updates)
3. **Code quality** (removed duplication, dead code, verbose logging)

**Net result:** MarketWatch now emits **9× fewer `dataChanged` signals per tick**, uses **O(1) token lookup** for Greeks updates, and has **cleaner, more maintainable code**.

---

## Phase 1: Bug Fixes (5 fixes)

### 1.1 Fixed Out-of-Range `dataChanged` with Compact Profile

**File:** `src/models/MarketWatchModel.cpp`  
**Lines:** 432, 443, 480  

**Problem:**  
Three methods used `COL_COUNT - 1` (enum max = 12) instead of `columnCount() - 1` (profile-aware, e.g. 6 for Compact). When using Compact profile, `dataChanged` was emitted with invalid column indices.

**Fix:**
```cpp
// Before (3 locations):
notifyRowUpdated(row, 0, COL_COUNT - 1);

// After:
notifyRowUpdated(row, 0, columnCount() - 1);
```

**Impact:** Eliminates unnecessary proxy model work mapping invalid indices.

---

### 1.2 Fixed Tick Flash Colors for Light Theme

**File:** `src/models/MarketWatchModel.cpp`  
**Line:** 113-114  

**Problem:**  
Tick flash backgrounds were dark colors (`#1a3a6b`, `#4a1212`) designed for black backgrounds. On the light-themed app, these were barely visible.

**Fix:**
```cpp
// Before:
if (tick > 0) return QColor("#1a3a6b"); // Dark blue for black bg
if (tick < 0) return QColor("#4a1212"); // Dark crimson for black bg

// After:
if (tick > 0) return QColor("#dbeafe"); // Light blue (light theme palette)
if (tick < 0) return QColor("#fee2e2"); // Light red (light theme palette)
```

**Impact:** Tick flash colors are now visible and consistent with light theme.

---

### 1.3 Fixed Context Menu for Light Theme

**File:** `src/views/MarketWatchWindow/UI.cpp`  
**Line:** 88-106  

**Problem:**  
Context menu used dark background (`#0d1117`) with light text, clashing with the light-themed app.

**Fix:**
```cpp
// Before:
"QMenu { background-color: #0d1117; color: #e2e8f0; }"

// After:
"QMenu { background-color: #ffffff; color: #1e293b; }"
"QMenu::item:selected { background-color: #dbeafe; color: #1e40af; }"
```

**Impact:** Context menu now matches light theme palette.

---

### 1.4 Fixed Delegate Text Color for Light Tick Flash

**File:** `src/views/MarketWatchWindow/UI.cpp`  
**Line:** 28-35  

**Problem:**  
Delegate painted white text on tick flash backgrounds. With the new light backgrounds, white text was invisible.

**Fix:**
```cpp
// Before:
opt.palette.setColor(QPalette::Text, Qt::white);

// After:
QColor textColor = QColor("#1e293b");
opt.palette.setColor(QPalette::Text, textColor);
```

**Impact:** Text is now readable on light tick flash backgrounds.

---

### 1.5 Removed "Export Cache Debug" Stub

**File:** `src/views/MarketWatchWindow/UI.cpp`  
**Line:** 160-161  

**Problem:**  
Context menu had a stub "Export Cache Debug" action that showed "being refactored" dialog.

**Fix:**  
Removed menu entry and separator.

**Impact:** Cleaner context menu without non-functional options.

---

## Phase 2: Performance Optimizations (5 improvements)

### 2.1 Batch Update Method — **9× Fewer Signals**

**Files:**  
- `include/models/qt/MarketWatchModel.h` (added declaration)
- `src/models/MarketWatchModel.cpp` (added implementation)

**Problem:**  
Every UDP tick triggered **9 separate `updateXxx()` calls**, each emitting its own `dataChanged` signal:
```cpp
m_model->updatePrice(row, ltp, change, changePercent);      // → dataChanged #1
m_model->updateOHLC(row, open, high, low, prevClose);        // → dataChanged #2
m_model->updateLastTradedQuantity(row, ltq);                 // → dataChanged #3
m_model->updateAveragePrice(row, atp);                       // → dataChanged #4
m_model->updateVolume(row, volume);                          // → dataChanged #5
m_model->updateBidAsk(row, bid, ask);                        // → dataChanged #6
m_model->updateBidAskQuantities(row, bidQty, askQty);        // → dataChanged #7
m_model->updateTotalBuySellQty(row, totalBuy, totalSell);    // → dataChanged #8
m_model->updateOpenInterestWithChange(row, oi, oiChange);    // → dataChanged #9
```

With 100 scrips receiving ticks at 10 Hz, that's **9,000 `dataChanged` signals per second** — massive Qt overhead.

**Fix:**  
Added `updateFromUdpTick(row, tick, change, changePercent)` that updates all fields in one batch:
```cpp
void MarketWatchModel::updateFromUdpTick(int row, const UDP::MarketTick& tick, 
                                         double change, double changePercent) {
    if (row < 0 || row >= m_scrips.count() || m_scrips.at(row).isBlankRow)
        return;
    
    ScripData& s = m_scrips[row];
    
    // Update LTP + tick direction
    if (tick.ltp > 0) {
        if (tick.ltp > s.ltp && s.ltp > 0) s.ltpTick = 1;
        else if (tick.ltp < s.ltp && s.ltp > 0) s.ltpTick = -1;
        s.ltp = tick.ltp;
        s.change = change;
        s.changePercent = changePercent;
    }
    
    // Update OHLC
    if (tick.open > 0) s.open = tick.open;
    if (tick.high > 0) s.high = tick.high;
    // ... (all 9 field groups updated)
    
    // ONE dataChanged signal for entire row
    notifyRowUpdated(row, 0, columnCount() - 1);
}
```

**Impact:**  
- **9× fewer `dataChanged` signals** → 9× less Qt signal overhead
- **9× fewer proxy model remappings** → 9× less QSortFilterProxyModel work
- **9× fewer view repaint schedules** → smoother UI updates

---

### 2.2 Rewrote `onUdpTickUpdate()` — 140 → 35 Lines

**File:** `src/views/MarketWatchWindow/Data.cpp`  
**Lines:** 186-353 → 186-220  

**Problem:**  
Original code was 140 lines with:
- Verbose perf logging stubs (50+ lines of commented code)
- 9 separate field update blocks
- Redundant BSE debug logging

**Fix:**  
Rewrote to use batch method:
```cpp
void MarketWatchWindow::onUdpTickUpdate(const UDP::MarketTick &tick) {
  int token = tick.token;
  int64_t timestampModelStart = LatencyTracker::now();

  QList<int> rows = m_tokenAddressBook->getRowsForIntKey(
      static_cast<int>(tick.exchangeSegment), token);

  if (rows.isEmpty())
    return;

  // Pre-calculate change once (shared across all rows)
  double change = 0, changePercent = 0;
  if (tick.ltp > 0) {
    double closePrice = tick.prevClose > 0 ? tick.prevClose 
                                            : m_model->getScripAt(rows.first()).close;
    if (closePrice > 0) {
      change = tick.ltp - closePrice;
      changePercent = (change / closePrice) * 100.0;
    }
  }

  // Batch update: ONE dataChanged per row
  for (int row : rows) {
    m_model->updateFromUdpTick(row, tick, change, changePercent);
  }

  int64_t timestampModelEnd = LatencyTracker::now();
  if (tick.refNo > 0 && tick.timestampUdpRecv > 0) {
    LatencyTracker::recordLatency(tick.timestampUdpRecv, tick.timestampParsed,
                                  tick.timestampEmitted, 0, tick.timestampFeedHandler,
                                  timestampModelStart, timestampModelEnd);
  }
}
```

**Impact:**  
- **75% code reduction** (140 → 35 lines)
- Removed all verbose perf logging
- Cleaner, more maintainable code

---

### 2.3 O(1) Token Lookup — `QHash<int,int> m_tokenToRow`

**Files:**  
- `include/models/qt/MarketWatchModel.h` (added member)
- `src/models/MarketWatchModel.cpp` (maintained in 6 methods)

**Problem:**  
`findScripByToken()` was O(N) linear scan, called frequently:
- Greeks updates: 1×/second per scrip → with 200 scrips = 40,000 comparisons/sec
```cpp
int MarketWatchModel::findScripByToken(int token) const {
    for (int i = 0; i < m_scrips.count(); ++i) {  // O(N) scan
        if (m_scrips.at(i).token == token && !m_scrips.at(i).isBlankRow)
            return i;
    }
    return -1;
}
```

**Fix:**  
Added `QHash<int, int> m_tokenToRow` maintained on add/remove/move/clear/insertBlankRow:
```cpp
// In addScrip():
if (!scrip.isBlankRow && scrip.token > 0) {
    m_tokenToRow[scrip.token] = row;
}

// In removeScrip():
if (!scrip.isBlankRow && scrip.token > 0) {
    m_tokenToRow.remove(scrip.token);
}
// Re-index all rows after removed row
for (int i = row; i < m_scrips.count(); ++i) {
    if (!m_scrips[i].isBlankRow && m_scrips[i].token > 0) {
        m_tokenToRow[m_scrips[i].token] = i;
    }
}

// findScripByToken() now O(1):
auto it = m_tokenToRow.find(token);
if (it != m_tokenToRow.end()) {
    return it.value();  // O(1) hash lookup
}
```

**Impact:**  
- **Greeks updates:** O(N) → O(1) per scrip
- **With 200 scrips:** 40,000 comparisons/sec → 200 hash lookups/sec

---

### 2.4 Extracted `exchangeToSegment()` Helper

**File:** `src/views/MarketWatchWindow/Actions.cpp`  
**Lines:** 8-15 (new helper), 4 call sites updated  

**Problem:**  
Exchange→segment mapping was copy-pasted 4 times:
```cpp
// 4× duplicated in addScrip(), addScripFromContract(), removeScrip(), pasteFromClipboard()
UDP::ExchangeSegment segment = UDP::ExchangeSegment::NSECM;
if (exchange == "NSEFO") segment = UDP::ExchangeSegment::NSEFO;
else if (exchange == "NSECM") segment = UDP::ExchangeSegment::NSECM;
else if (exchange == "BSEFO") segment = UDP::ExchangeSegment::BSEFO;
else if (exchange == "BSECM") segment = UDP::ExchangeSegment::BSECM;
```

**Fix:**  
Extracted static helper:
```cpp
static UDP::ExchangeSegment exchangeToSegment(const QString &exchange) {
  if (exchange == "NSEFO") return UDP::ExchangeSegment::NSEFO;
  if (exchange == "NSECM") return UDP::ExchangeSegment::NSECM;
  if (exchange == "BSEFO") return UDP::ExchangeSegment::BSEFO;
  if (exchange == "BSECM") return UDP::ExchangeSegment::BSECM;
  return UDP::ExchangeSegment::NSECM; // default
}

// All 4 call sites now:
UDP::ExchangeSegment segment = exchangeToSegment(exchange);
```

**Impact:**  
- **DRY principle** — single source of truth
- **20 lines of duplication eliminated**

---

### 2.5 Removed Verbose Perf Logging

**File:** `src/views/MarketWatchWindow/Actions.cpp`  
**Lines:** 126-240 (entire `addScripFromContract` function)  

**Problem:**  
`addScripFromContract()` logged **~15 `qDebug()` lines per call**:
```cpp
qDebug() << "[PERF] [MW_ADD_SCRIP] #" << addScripCounter 
         << "START - Token:" << contractData.token;
// ... 13 more lines with timing breakdowns
```

With portfolio load of 100 scrips, that's **1,500 debug lines** polluting the log.

**Fix:**  
Removed all perf logging (can be re-added behind `#ifdef QT_DEBUG` if needed for profiling).

**Impact:**  
- **Cleaner debug output**
- **Faster scrip addition** (no debug string formatting overhead)

---

## Phase 3: Bug Fix (1 fix)

### 3.1 Fixed Multi-Row Move Losing Subscriptions

**File:** `src/views/MarketWatchWindow/Actions.cpp`  
**Line:** 432-448  

**Problem:**  
When dragging 2+ rows to a new position, `performRowMoveByTokens()` called:
```cpp
for (int sourceRow : sortedRows) {
    removeScrip(sourceRow);  // ← UNSUBSCRIBES from FeedHandler + TSM
}
// ...
for (int i = 0; i < scrips.size(); ++i) {
    m_model->insertScrip(adjustedTarget + i, scrips[i]);
    m_tokenAddressBook->addCompositeToken(...);
    // ❌ NO re-subscribe — scrips stop receiving ticks!
}
```

**Fix:**  
Added re-subscription after multi-row re-insert:
```cpp
for (int i = 0; i < scrips.size(); ++i) {
    m_model->insertScrip(adjustedTarget + i, scrips[i]);
    m_tokenAddressBook->addCompositeToken(...);
    m_tokenAddressBook->addIntKeyToken(...);

    // Re-subscribe after multi-row move
    if (scrips[i].isValid()) {
        TokenSubscriptionManager::instance()->subscribe(scrips[i].exchange, scrips[i].token);
        UDP::ExchangeSegment udpSeg = exchangeToSegment(scrips[i].exchange);
        FeedHandler::instance().subscribeUDP(udpSeg, scrips[i].token, this,
                                             &MarketWatchWindow::onUdpTickUpdate);
    }
}
```

**Impact:**  
- **Multi-row drag now preserves live data subscriptions**
- Previously: silent data loss — scrips stopped updating after move
- Now: scrips continue receiving ticks after move

---

## Test Results

```
✅ test_market_watch_model:        45 tests passed
✅ test_service_registry:           18 tests passed  
✅ test_trading_data_service:       20 tests passed
───────────────────────────────────────────────────
   TOTAL:                           83 tests passed, 0 failed
```

---

## Performance Impact Summary

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| **`dataChanged` per tick per row** | 9 | 1 | **9× reduction** |
| **`findScripByToken()` complexity** | O(N) | O(1) | **200× faster with 200 scrips** |
| **Exchange→segment mapping** | 4× copy-paste | 1 helper | **20 lines eliminated** |
| **`addScripFromContract` debug output** | ~15 lines | 0 | **Cleaner logs** |
| **`onUdpTickUpdate` code size** | 140 lines | 35 lines | **75% reduction** |

---

## Remaining Optimization Opportunities

From the deep analysis, these remain **not yet implemented**:

### High Priority
1. **Column-specific `dataChanged` ranges** — currently all updates invalidate entire row
2. **Fix address book row indices after multi-delete** — currently go stale

### Medium Priority
3. **Only update OHLC when changed** — currently rewrites `close` on every tick
4. **Multi-watchlist tabs** — only one watchlist supported
5. **5-level market depth popup** — data arrives but not displayed

### Low Priority
6. **Sort by column UX** — proxy model in place but no header click handler
7. **Price alerts** — no alert mechanism
8. **Inline scrip search** — redirects to ScripBar instead

See `MARKETWATCH_DEEP_ANALYSIS.md` for full list.

---

## Files Modified

| File | Lines Changed | Type |
|------|--------------|------|
| `include/models/qt/MarketWatchModel.h` | +18 | Feature |
| `src/models/MarketWatchModel.cpp` | +95, -20 | Optimization |
| `src/views/MarketWatchWindow/Data.cpp` | +35, -140 | Optimization |
| `src/views/MarketWatchWindow/Actions.cpp` | +30, -50 | Bug fix + refactor |
| `src/views/MarketWatchWindow/UI.cpp` | +15, -20 | Bug fix |

**Total:** ~153 lines added, ~230 lines removed = **77 net lines removed** with better functionality.

---

## Conclusion

All **Phase 1-3 optimizations complete**. MarketWatch now has:
- ✅ Light-theme visual consistency
- ✅ 9× fewer Qt signals per tick
- ✅ O(1) token lookup for Greeks
- ✅ Multi-row move preserves subscriptions
- ✅ Cleaner, more maintainable code

**Next steps:**  
- Monitor production performance with real UDP feed
- Consider implementing Phase 4: column-specific `dataChanged` ranges
- Plan for multi-watchlist tabs feature

---

*Document created: 2 March 2026*  
*All code verified with 83 passing unit tests*
