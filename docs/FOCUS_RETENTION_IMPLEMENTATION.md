# Market Watch Focus Retention - Implementation Summary

## Overview
Implemented focus retention functionality for the Market Watch window to maintain user's last active scrip position when switching between windows or adding new scrips.

## Problem Statement
- When a user opens a window (Buy/Sell/SnapQuote) from Market Watch and closes it, the focus was lost
- When adding a new scrip, the focus didn't automatically move to the newly added scrip
- Arrow key navigation wasn't working properly because focus wasn't set correctly

## Root Cause Analysis
From the logs:
```
[MarketWatch] Stored focus on token: 59175 Symbol: "BANKNIFTY" Row: 0
[MarketWatch] Stored token 59175 not found in model
```

The issue was:
1. Focus restoration was triggered immediately when the window was activated
2. The model wasn't fully ready at that time, causing token lookup to fail
3. No delay mechanism to ensure model stability before restoring focus
4. Focus wasn't explicitly set on the view after selection

## Solution Implemented

### 1. **Added Focus Tracking** (`MarketWatchWindow.h`)
- Added member variable: `int m_lastFocusedToken = -1;`
- Added methods:
  - `storeFocusedRow()` - Store current focused scrip token
  - `restoreFocusedRow()` - Restore focus to stored token
  - `setFocusToToken(int token)` - Set focus to specific token

### 2. **Store Focus Before Opening Windows** (`MainWindow\Windows.cpp`)
Modified:
- `createBuyWindow()` - Stores focus before opening Buy window
- `createSellWindow()` - Stores focus before opening Sell window
- `createSnapQuoteWindow()` - Stores focus before opening SnapQuote window

Example:
```cpp
MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
if (activeMarketWatch) {
    activeMarketWatch->storeFocusedRow();  // ✓ Store before opening
}
```

### 3. **Delayed Focus Restoration** (`MarketWatchWindow.cpp`)

#### A. In `focusInEvent()`:
```cpp
void MarketWatchWindow::focusInEvent(QFocusEvent *event) {
  CustomMarketWatch::focusInEvent(event);
  
  if (m_lastFocusedToken > 0) {
    QTimer::singleShot(50, this, [this]() {  // ✓ 50ms delay
      if (m_lastFocusedToken > 0) {
        restoreFocusedRow();
      }
    });
  }
}
```

#### B. In `restoreFocusedRow()`:
```cpp
void MarketWatchWindow::restoreFocusedRow() {
    // ✓ Check model validity
    if (!m_model || m_model->rowCount() == 0) {
        return;
    }
    
    // ✓ Find token in model
    int row = findTokenRow(m_lastFocusedToken);
    if (row < 0) {
        m_lastFocusedToken = -1;  // ✓ Clear invalid token
        return;
    }
    
    // ✓ Map to proxy row (handles sorting)
    int proxyRow = mapToProxy(row);
    
    // ✓ Select and scroll
    selectRow(proxyRow);
    scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
    
    // ✓ Explicitly set focus
    setFocus(Qt::OtherFocusReason);
}
```

#### C. Window Activation Handler (`MainWindow\Windows.cpp`):
```cpp
connect(window, &CustomMDISubWindow::windowActivated, marketWatch, [marketWatch]() {
    QTimer::singleShot(100, marketWatch, [marketWatch]() {  // ✓ 100ms delay
      if (marketWatch->getModel() && marketWatch->getModel()->rowCount() > 0) {
        marketWatch->restoreFocusedRow();
      }
    });
});
```

### 4. **Auto-Focus on Newly Added Scrip** (`Actions.cpp`)
Modified `addScripFromContract()`:
```cpp
emit scripAdded(scrip.symbol, scrip.exchange, scrip.token);

// ✓ Set focus to newly added scrip
setFocusToToken(scrip.token);
```

## Key Improvements

### 1. **Delayed Execution**
- 50ms delay in `focusInEvent()` - ensures view is ready
- 100ms delay in window activation - ensures model is populated
- Prevents race conditions with model updates

### 2. **Model Validation**
- Check if model exists and has data before restoration
- Validates token exists in model before mapping
- Clears invalid tokens to prevent repeated failures

### 3. **Explicit Focus Setting**
- Calls `setFocus(Qt::OtherFocusReason)` after selection
- Ensures keyboard navigation works immediately
- Prevents "selected but not focused" state

### 4. **Proxy Model Handling**
- Uses `mapToProxy()` to handle sorted/filtered views
- Ensures correct row selection regardless of sorting

## Testing Checklist

✓ Test Case 1: Open Buy Window from Market Watch
- Select TCS in Market Watch
- Press F1 (Buy Window)
- Close Buy Window
- **Expected**: Focus returns to TCS

✓ Test Case 2: Open SnapQuote from Market Watch
- Select BANKNIFTY
- Press Ctrl+Q (SnapQuote)
- Close SnapQuote
- **Expected**: Focus returns to BANKNIFTY

✓ Test Case 3: Add New Scrip
- Select TCS
- Add MIDCAPNIFTY via ScripBar
- **Expected**: Focus moves to MIDCAPNIFTY

✓ Test Case 4: Arrow Key Navigation
- Focus restored row
- Press Up/Down arrows
- **Expected**: Navigation works immediately

## Log Messages

### Success Logs:
```
[MarketWatch] Stored focus on token: 59175 Symbol: "BANKNIFTY" Row: 0
[MainWindow] Market Watch window activated, scheduling focus restore
[MarketWatch] ✓ Restored focus to token: 59175 Source Row: 0 Proxy Row: 3
[MarketWatch] ✓ Set focus to token: 11536 Source Row: 5 Proxy Row: 6
```

### Handled Error Cases:
```
[MarketWatch] No stored focus token, skipping restore
[MarketWatch] Model is not ready or empty, skipping focus restore
[MarketWatch] Stored token 59175 not found in model (may have been removed)
```

## Files Modified

1. `include/views/MarketWatchWindow.h`
   - Added focus tracking methods and member variable

2. `src/views/MarketWatchWindow/MarketWatchWindow.cpp`
   - Implemented `storeFocusedRow()`, `restoreFocusedRow()`, `setFocusToToken()`
   - Added `focusInEvent()` with delayed restoration
   - Added `#include <QFocusEvent>`

3. `src/views/MarketWatchWindow/Actions.cpp`
   - Modified `addScripFromContract()` to set focus on new scrip

4. `src/app/MainWindow/Windows.cpp`
   - Modified `createBuyWindow()` to store focus
   - Modified `createSellWindow()` to store focus
   - Modified `createSnapQuoteWindow()` to store focus
   - Modified `createMarketWatch()` to add window activation handler

## Performance Impact
- Minimal: ~50-100ms delay for focus restoration
- No impact on normal operations
- Delays are imperceptible to users
- Prevents race conditions and improves reliability

## Future Enhancements
1. Add persistence - save last focused scrip between sessions
2. Add focus history (stack of last 10 focused scrips)
3. Add keyboard shortcut to cycle through focus history
4. Handle multi-selection focus restoration
