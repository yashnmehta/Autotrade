# MarketWatch Deep Review — Architecture, Code Quality & Feature Roadmap

**Date**: 4 March 2026  
**Scope**: Full analysis of `CustomMarketWatch`, `MarketWatchWindow`, `MarketWatchModel`, `TokenAddressBook`, delegates, helpers, and data flow.

---

## 1. Architecture Overview (Current State)

```
┌──────────────────────────────────────────────────────────────────┐
│                        MarketWatchWindow                        │
│  (Views layer — owns model, address book, subscriptions, UI)    │
│  Files: MarketWatchWindow.cpp, Actions.cpp, UI.cpp, Data.cpp    │
├──────────────────────────────────────────────────────────────────┤
│                       CustomMarketWatch                          │
│  (Core layer — QTableView + proxy, drag-drop, styling, keys)    │
│  File: CustomMarketWatch.cpp                                    │
├──────────────────────────────────────────────────────────────────┤
│                       MarketWatchModel                           │
│  (Model layer — QAbstractTableModel, column profiles, data)     │
│  File: MarketWatchModel.cpp  (~1000 lines)                       │
├──────────────────────────────────────────────────────────────────┤
│  TokenAddressBook │ MarketWatchHelpers │ IMarketWatchViewCallback│
│  (Domain support)  (Parse/Format/IO)    (Ultra-low latency I/F) │
└──────────────────────────────────────────────────────────────────┘
```

**Strengths of this architecture**:
- Clean 3-layer separation (Core → View → Model).
- `IMarketWatchViewCallback` bypasses Qt signal overhead for ~200× faster viewport invalidation.
- Batch `updateFromUdpTick()` reduces 9 separate `dataChanged` emissions to 1 per row per tick.
- `TokenAddressBook` provides O(1) lookups for price routing.
- Column profile system is generic and reusable across other table views.
- Dynamic selection mode switching (Single ↔ Extended) is clever and avoids permanent ExtendedSelection footguns.

---

## 2. Bugs & Issues Found

### 2.1 🔴 CRITICAL — Tick Flash Never Resets

**Location**: `MarketWatchModel.cpp` (lines 420-428) + `ScripData` (lines 100-103)

```cpp
// updatePrice() sets tick direction:
if (ltp > scrip.ltp && scrip.ltp > 0) scrip.ltpTick = 1;   // UP
else if (ltp < scrip.ltp && scrip.ltp > 0) scrip.ltpTick = -1; // DOWN
```

**Problem**: `ltpTick`, `bidTick`, `askTick` are set to ±1 on change, but **never reset to 0**. The `MarketWatchDelegate::paint()` checks these values and paints a light-blue or light-red background. Since the tick value is never cleared, the cell stays colored **permanently** after the first tick. It only changes color on direction change — it never returns to the default background.

**Expected behavior**: Flash for 200-500ms, then return to normal background.

**Fix**: Add a `QTimer::singleShot(300, ...)` after setting tick direction to reset it to 0, OR use a timestamp-based approach: store `lastTickTime` and have the delegate check `(now - lastTickTime < 300ms)`.

### 2.2 🟡 MEDIUM — Dual Token→Row Mapping (Model vs AddressBook)

`MarketWatchModel` maintains `QHash<int,int> m_tokenToRow` internally.  
`TokenAddressBook` maintains its own `QMap<int64_t, QList<int>>` externally.

These are two independent maps that must be kept in sync but are updated in different places:
- Model's `m_tokenToRow` is updated inside `insertScrip()`, `removeScrip()`, `moveRow()`.
- `TokenAddressBook` is updated in `MarketWatchWindow::addScrip()`, `removeScrip()`, and via `rowsInserted`/`rowsRemoved` signals in `setupConnections()`.

**Risk**: If one map gets out of sync with the other, price updates will silently fail (model says row 5, address book says row 6). This has likely already caused subtle bugs during drag-and-drop row moves.

**Fix**: Consolidate to a single source of truth. Either:
1. Remove `m_tokenToRow` from the model and always go through `TokenAddressBook`, OR
2. Remove `TokenAddressBook` and extend the model's map to support composite keys.

### 2.3 🟡 MEDIUM — `addScrips()` Duplicate Check is O(N²)

```cpp
void MarketWatchModel::addScrips(const QList<ScripData> &scrips) {
    for (const auto &scrip : scrips) {
        // For each scrip, linear scan all existing scrips
        for (const ScripData &existing : m_scrips) {  // O(N)
            if (existing.token == scrip.token && ...)
```

For delta portfolio generation with 200+ scrips and 100 existing rows, this is O(N×M). Should use the `m_tokenToRow` hash for O(1) lookups instead.

### 2.4 🟡 MEDIUM — `onTickUpdate()` Legacy Path is Wasteful

`Data.cpp::onTickUpdate(XTS::Tick)` makes **up to 9 separate model update calls** per tick, each emitting its own `dataChanged`:
```cpp
m_model->updatePrice(row, ...);     // dataChanged
m_model->updateOHLC(row, ...);      // dataChanged
m_model->updateLastTradedQuantity(); // dataChanged
m_model->updateAveragePrice();       // dataChanged
m_model->updateVolume();             // dataChanged
m_model->updateBidAsk();             // dataChanged
m_model->updateBidAskQuantities();   // dataChanged
m_model->updateTotalBuySellQty();    // dataChanged
m_model->updateOpenInterest();       // dataChanged
```

The UDP path correctly uses batch `updateFromUdpTick()`. **If the legacy XTS path is still in use**, it's 9× slower per tick. If it's deprecated, it should be removed to avoid confusion.

### 2.5 🟢 LOW — `ScripData` Struct is Bloated (41 fields, ~400 bytes)

`ScripData` contains Greeks, 52-week highs/lows, market cap, ISIN code, etc. — many of which are rarely populated for most instruments. This creates memory overhead for every row (including blank rows).

**Impact**: For 500 rows × 400 bytes ≈ 200KB — negligible. But it makes the code harder to maintain and increases copy costs during drag-and-drop moves.

### 2.6 🟢 LOW — `addScrip()` in Model Duplicates Logic of `insertScrip()`

`addScrip()` is just `insertScrip(rowCount(), scrip)` but with duplicated duplicate-checking logic. One should call the other.

### 2.7 🟢 LOW — `bid`/`ask` vs `buyPrice`/`sellPrice` Duality

`ScripData` has both `bid`/`ask` AND `buyPrice`/`sellPrice` with comments saying they're aliases, but the model updates both independently:
```cpp
scrip.bid = bid;
scrip.ask = ask;
scrip.buyPrice = bid;  // Buy Price = Bid
scrip.sellPrice = ask; // Sell Price = Ask
```

This is error-prone. If someone updates `bid` without updating `buyPrice`, they'll diverge.

### 2.8 🟢 LOW — `notifyRowUpdated()` Always Emits Full Row Range

Most individual update methods (e.g., `updateVolume()`) notify `(row, 0, columnCount()-1)` even though only one column changed. Only `updateOpenInterestWithChange()` correctly uses `(row, COL_OPEN_INTEREST, COL_OPEN_INTEREST)`.

**Impact**: Viewport repaints slightly more than necessary per update. Minor performance loss.

---

## 3. Performance Assessment

### What's Already Excellent ✅

| Area | Implementation | Impact |
|------|---------------|--------|
| Native callback bypass | `IMarketWatchViewCallback` | ~200× faster than Qt signals |
| Batch UDP updates | `updateFromUdpTick()` | 9× fewer signals per tick |
| O(1) price routing | `TokenAddressBook::getRowsForIntKey()` | No linear scan per tick |
| Token→row hash | `m_tokenToRow` in model | O(1) `findScripByToken()` |
| Deferred construction | `QTimer::singleShot(0, ...)` for shortcuts/settings | Faster window creation |
| Cached preferences | `s_useZeroCopyPriceCache_Cached` | Avoids 50ms disk I/O |
| No dynamic sort | `setDynamicSortFilter(false)` | No re-sort on price ticks |
| Direct viewport update | `viewport()->update(updateRect)` in `onRowUpdated` | Minimal repaint area |

### Where Performance Can Be Improved 🔧

| Area | Current | Improvement | Effort |
|------|---------|-------------|--------|
| Tick flash reset | None (stays forever) | Timer-based or timestamp-based clear | Low |
| Full-row notify | All updates notify (0, colCount-1) | Notify only affected columns | Low |
| `addScrips()` dup check | O(N×M) linear scan | Use `m_tokenToRow` hash | Low |
| Legacy `onTickUpdate` | 9 separate updates | Remove if UDP path is standard | Low |
| `QList<ScripData>` storage | Copy-on-grow | Consider `QVector` or `std::vector` with `reserve()` | Low |
| Proxy model overhead | Every update goes through proxy | Consider if proxy is needed when not sorting | Medium |

---

## 4. Code Quality Review

### 4.1 Strengths ✅

1. **Clear file organization**: Actions.cpp / Data.cpp / UI.cpp / MarketWatchWindow.cpp is a good split by concern.
2. **Comprehensive documentation**: Doxygen comments on all public methods in the header.
3. **Clean RAII**: Destructor properly unsubscribes from all feeds.
4. **Debug logging**: Rich `qDebug()` output with `[PERF]` prefixes for timing.
5. **Selection management**: `storeFocusedRow()` / `restoreFocusedRow()` / `m_suppressFocusRestore` is robust.
6. **Portfolio save/load**: Full JSON serialization with column profile persistence.

### 4.2 Issues 🔧

1. **Verbose qDebug() in production**: Many debug prints are active. Should use a compile-time or runtime log-level filter.

2. **`static ScripData emptyData` in `getScripAt()`**: The non-const overload returns a reference to a mutable static — any caller that modifies it would silently corrupt it for all future callers.

3. **Comment `// Data Components` duplicated** on consecutive lines in MarketWatchWindow.h (line ~362).

4. **`exchangeToSegment()` is a static free function** in Actions.cpp. It's also conceptually used in Data.cpp (via the `exchangeToSegment` pattern). Should be a shared utility.

5. **No `const` correctness on some methods**: `hasDuplicate()`, `findTokenRow()`, `hasValidSelection()` are `const`, but `getSelectedContractContext()` does a non-const lookup on `RepositoryManager`.

---

## 5. Feature Roadmap — Scope for Improvements

### 5.1 🎯 HIGH PRIORITY — UX Improvements

| Feature | Description | Benefit |
|---------|-------------|---------|
| **Tick flash with auto-reset** | Flash cells for 300ms then revert to default bg | Essential visual cue for traders |
| **Change color coding** | Color Change/Change% column text: green for positive, red for negative | Industry-standard UX |
| **Row grouping / section headers** | Collapsible groups (e.g., "NIFTY Options", "Bank NIFTY") instead of blank rows | Better organization for large watchlists |
| **Freeze first column** | Keep Symbol column visible while scrolling horizontally | Usability with many columns |
| **Row numbering** | Show visual row numbers in a thin left column | Easier orientation in large lists |
| **Inline search/filter** | Ctrl+F to filter visible rows by symbol | Fast navigation in large lists |
| **Double-click to open Snap Quote** | Double-click a row to open detailed quote for that scrip | Industry-standard behavior |
| **Status bar** | Show total scrips, total value, P&L summary at bottom | Quick overview |

### 5.2 🔧 MEDIUM PRIORITY — Technical Improvements

| Feature | Description | Benefit |
|---------|-------------|---------|
| **Consolidate token maps** | Single source of truth for token→row | Eliminate sync bugs |
| **Eliminate `bid`/`ask` duality** | Remove `buyPrice`/`sellPrice` aliases | Cleaner data model |
| **Remove legacy `onTickUpdate`** | Single UDP tick path | Simpler code, no 9× overhead |
| **Targeted column notifications** | Notify only changed columns | ~30% less repaint work |
| **Virtual scrolling** | Only render visible rows for 1000+ scrip lists | Smoother scrolling |
| **Undo/Redo stack** | Undo delete, move, paste operations | Prevent accidental data loss |
| **Column auto-sizing** | Double-click column border to auto-fit content | Better column widths |
| **Remember sort state per window** | Persist which column was sorted and direction | Consistent UX across restarts |

### 5.3 🚀 LOW PRIORITY — Advanced Features

| Feature | Description | Benefit |
|---------|-------------|---------|
| **Calculated columns** | User-defined columns like `(Bid+Ask)/2`, `OI × LTP` | Power-user feature |
| **Alert/Alarm on price** | Set price alerts: "NIFTY > 22000" with sound/notification | Active trading support |
| **Heat map mode** | Color rows by change% intensity (gradient red → green) | Quick market overview |
| **Spread view** | Show 2-leg/3-leg spreads inline (CE-PE, calendar, etc.) | F&O strategy support |
| **Live P&L column** | If positions are linked, show unrealized P&L per scrip | Position tracking |
| **Export to Excel** | One-click export of current view to .xlsx with formatting | Reporting |
| **Multi-level depth** | Show full 5-level market depth inline or on hover | Depth analysis |
| **Sparkline charts** | Mini price charts in a cell (requires custom delegate) | Visual price history |
| **Custom color themes** | Let users pick from dark/light/custom color schemes | Personalization |

---

## 6. Specific Refactoring Recommendations

### 6.1 Extract `ScripSubscriptionManager` from `MarketWatchWindow`

Currently, `addScrip()` in Actions.cpp does 5 things:
1. Create `ScripData` from contract
2. Insert into model
3. Subscribe to `TokenSubscriptionManager`
4. Subscribe to `FeedHandler` UDP
5. Load initial snapshot from `PriceStoreGateway`
6. Update `TokenAddressBook`
7. Emit signal

Steps 3-6 are **subscription plumbing** that should be in a dedicated `ScripSubscriptionManager`:

```cpp
class ScripSubscriptionManager {
    void subscribe(MarketWatchModel*, TokenAddressBook*, const ScripData&, int row);
    void unsubscribe(const ScripData&, int row);
    void resubscribeAll(); // After model reset
};
```

This would eliminate the massive code duplication between `addScrip()`, `addScripFromContract()`, `addScrips()`, and `pasteFromClipboard()`, all of which repeat the same subscription pattern.

### 6.2 Replace `QList<ScripData>` with `std::vector<ScripData>`

`QList<T>` for types larger than a pointer (ScripData is ~400 bytes) stores items on the heap via pointers, adding indirection and poor cache locality. `std::vector` stores items contiguously, which is better for sequential access during rendering.

### 6.3 Column-Specific Notifications

Replace the blanket `notifyRowUpdated(row, 0, columnCount()-1)` with targeted column ranges. This requires a mapping from data field → visible column index, but would reduce viewport repaint by ~30-50%.

Example:
```cpp
void MarketWatchModel::updatePrice(int row, double ltp, ...) {
    // Only notify LTP, Change, Change% columns
    int ltpCol = m_columnProfile.visibleIndexOf(MarketWatchColumn::LAST_TRADED_PRICE);
    int chgCol = m_columnProfile.visibleIndexOf(MarketWatchColumn::PERCENT_CHANGE);
    if (ltpCol >= 0 && chgCol >= 0)
        notifyRowUpdated(row, ltpCol, chgCol);
}
```

---

## 7. Summary Scorecard

| Category | Score | Notes |
|----------|-------|-------|
| **Architecture** | 8.5/10 | Clean 3-layer separation, well-defined interfaces |
| **Performance** | 9/10 | Native callbacks, batch updates, O(1) lookups — excellent |
| **Code Quality** | 7/10 | Good structure but has duplication and the dual-map issue |
| **UX Completeness** | 6/10 | Functional but missing standard terminal features (tick flash, change colors) |
| **Maintainability** | 7/10 | Good file organization, but subscription logic is spread across 4 methods |
| **Test Coverage** | 5/10 | Model has unit tests but Window/View layer is untested |
| **Error Handling** | 6/10 | Bounds checks exist but no error reporting to user for failed subscriptions |

**Overall**: Solid professional implementation with strong performance foundations. The biggest wins now are UX polish (tick flash, change colors) and code consolidation (subscription plumbing, token maps).

---

## 8. Priority Fix Order

1. ✅ **Tick flash auto-reset** — Timestamp-based with QTimer coalescing (350ms)
2. ✅ **Change column color coding** — Green/red for LTP, Change, Change%
3. ✅ **O(1) duplicate check** — `addScrip()`/`addScrips()` now use `m_tokenToRow` hash
4. ✅ **Column-specific notifications** — All update methods now notify only affected columns
5. ✅ **Double-click → Snap Quote** — `mouseDoubleClickEvent` + context menu entry
6. ✅ **Open Chart with context** — Context menu "Open Chart" passes scrip to chart window
7. ✅ **Safe `getScripAt()` non-const** — Resets static to prevent stale data corruption
8. **Consolidate token maps** — Prevent sync bugs (TODO)
9. **Extract subscription logic** — Eliminate 4× code duplication (TODO)
10. **Remove legacy `onTickUpdate`** — Simplify data path (TODO)
11. **Inline search/filter** — Power-user feature (TODO)
