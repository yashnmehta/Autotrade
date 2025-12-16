# Debug Guide - Drag & Drop Issue

## Current Status
✅ Build successful with debug logging added
🔍 Debug logs now active - ready to investigate

## How to Run with Debug Output

### Option 1: Terminal (Recommended)
```bash
cd /Users/yashmehta/Desktop/go_proj/trading_terminal_cpp/build
/Users/yashmehta/Desktop/go_proj/trading_terminal_cpp/build/TradingTerminal.app/Contents/MacOS/TradingTerminal
```

### Option 2: Open Console.app First
1. Open **Console.app** (Applications > Utilities > Console)
2. In the search bar, type: `TradingTerminal`
3. Then launch: `open TradingTerminal.app`
4. Watch Console.app for debug output

## What to Test

### Test 1: Initial State Check
1. Launch app
2. Press **Ctrl+M** (create Market Watch)
3. **WITHOUT CLICKING ANY HEADERS**
4. Try to drag a row

**Expected Debug Output**:
```
[DEBUG] === Drag Start Sort Check ===
[DEBUG] isSortIndicatorShown(): false  ← Should be FALSE
[DEBUG] sortIndicatorSection(): -1     ← Should be -1
[DEBUG] sortIndicatorOrder(): [Ascending/Descending]
[DEBUG] isSortingEnabled(): true       ← This will be true (that's OK)
[DEBUG] proxyModel->sortColumn(): -1   ← Should be -1
[DEBUG] proxyModel->sortOrder(): [Ascending/Descending]
[DEBUG] ALLOWING: No active sort detected, proceeding with drag
```

**If you see "BLOCKING" instead**: This tells us the root cause!

### Test 2: After Clicking Header
1. Click **"LTP"** column header
2. Try to drag a row

**Expected Debug Output**:
```
[DEBUG] === Sort Indicator Changed ===
[DEBUG] Column: 1
[DEBUG] Order: Ascending
[DEBUG] isSortIndicatorShown(): true

[DEBUG] === Drag Start Sort Check ===
[DEBUG] isSortIndicatorShown(): true   ← Should be TRUE
[DEBUG] sortIndicatorSection(): 1      ← Should be 1 (LTP column)
[DEBUG] BLOCKING: Sort is active on column 1
```

### Test 3: After Clearing Sort
1. Click **"LTP"** header again to clear
2. Try to drag a row

**Expected Debug Output**:
```
[DEBUG] === Sort Indicator Changed ===
[DEBUG] Column: -1  ← or different state
[DEBUG] isSortIndicatorShown(): false

[DEBUG] === Drag Start Sort Check ===
[DEBUG] isSortIndicatorShown(): false
[DEBUG] sortIndicatorSection(): -1
[DEBUG] ALLOWING: No active sort detected
```

## What the Logs Tell Us

### Scenario A: Always Shows "BLOCKING"
**If you see**:
```
[DEBUG] isSortIndicatorShown(): true   ← Always true!
```

**Root Cause**: Sort indicator is somehow always shown, even at startup.

**Fix**: We need to call `header->setSortIndicatorShown(false)` initially.

### Scenario B: sortIndicatorSection() Never -1
**If you see**:
```
[DEBUG] sortIndicatorSection(): 0  ← Never -1, even without sorting!
```

**Root Cause**: Header remembers last sort column.

**Fix**: We need to check both indicator shown AND column >= 0.

### Scenario C: Proxy Model Always Sorted
**If you see**:
```
[DEBUG] proxyModel->sortColumn(): 0  ← Always >= 0!
```

**Root Cause**: Proxy model is initialized with a default sort.

**Fix**: We need to call `m_proxyModel->sort(-1)` to clear it.

## Quick Diagnostic Questions

After running the test, answer these:

1. **What does isSortIndicatorShown() show at startup?**
   - [ ] true (BUG: Should be false)
   - [ ] false (CORRECT)

2. **What does sortIndicatorSection() show at startup?**
   - [ ] >= 0 (BUG: Should be -1)
   - [ ] -1 (CORRECT)

3. **What does proxyModel->sortColumn() show at startup?**
   - [ ] >= 0 (BUG: Should be -1)
   - [ ] -1 (CORRECT)

4. **Does clicking a header change these values?**
   - [ ] Yes (CORRECT)
   - [ ] No (BUG)

## Copy & Paste Test Commands

### Quick Test Script
```bash
# Terminal 1: Run app with output
cd /Users/yashmehta/Desktop/go_proj/trading_terminal_cpp/build
/Users/yashmehta/Desktop/go_proj/trading_terminal_cpp/build/TradingTerminal.app/Contents/MacOS/TradingTerminal > /tmp/trading_debug.log 2>&1 &

# Terminal 2: Watch the log
tail -f /tmp/trading_debug.log | grep -E "DEBUG|BLOCKING|ALLOWING"
```

Then:
1. Press Ctrl+M
2. Try to drag a row
3. Check Terminal 2 for output

## Expected vs Actual

### CORRECT Behavior
```
Startup → isSortIndicatorShown() = false → Drag ALLOWED ✅
Click header → isSortIndicatorShown() = true → Drag BLOCKED ✅
Click again → isSortIndicatorShown() = false → Drag ALLOWED ✅
```

### If BROKEN
```
Startup → isSortIndicatorShown() = true → Drag BLOCKED ❌
[Issue: Indicator shown at startup]
```

OR

```
Startup → sortIndicatorSection() = 0 → Drag BLOCKED ❌
[Issue: Section not -1 at startup]
```

## Next Steps Based on Logs

### If isSortIndicatorShown() is TRUE at startup:
Add to `setupUI()`:
```cpp
m_tableView->horizontalHeader()->setSortIndicatorShown(false);
```

### If sortIndicatorSection() is >= 0 at startup:
Add to `setupUI()`:
```cpp
m_tableView->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
```

### If proxyModel->sortColumn() is >= 0 at startup:
Add to `setupUI()`:
```cpp
m_proxyModel->sort(-1);  // Clear any default sort
```

## How to Share Debug Output

### Copy from Terminal
```bash
# Save last 100 lines
tail -100 /tmp/trading_debug.log > ~/Desktop/debug_output.txt
```

### Or Screenshot
1. Run app in terminal
2. Try to drag
3. Take screenshot of debug output
4. Share the screenshot

## Summary

The debug logs will tell us EXACTLY which check is failing:
- `isSortIndicatorShown()` = true when it should be false?
- `sortIndicatorSection()` = 0 when it should be -1?
- `proxyModel->sortColumn()` = 0 when it should be -1?

Once we see the actual values, we'll know exactly what to fix!

---

**Ready to debug!** Please run the tests above and share the debug output from the console. 🔍
