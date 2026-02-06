# Window Manager Focus Fix - Cached Windows Integration

## Problem Identified

From the logs, we discovered that the WindowManager integration wasn't working properly with cached windows:

```
[2026-02-05 17:01:15.502] [DEBUG] [MDISubWindow] closeEvent for "Snap Quote 1"
[2026-02-05 17:01:15.518] [DEBUG] [WindowCacheManager] SnapQuote window 1 marked as closed
```

When the user manually clicked on Market Watch after closing SnapQuote:
```
[2026-02-05 17:00:41.388] [DEBUG] [MarketWatch] No stored focus token, skipping restore
```

### Root Cause

The issue was that cached windows (Buy, Sell, SnapQuote) were calling `WindowManager::unregisterWindow()` in their `closeEvent()` methods. However, cached windows are **not actually closed** - they're just moved off-screen for fast reuse. This caused the following problems:

1. **Premature Unregistration**: `closeEvent()` was firing BEFORE the window was moved off-screen, triggering `unregisterWindow()` too early
2. **Double Activation**: The WindowManager would try to activate the previous window while the cached window was still being hidden
3. **Lost Focus State**: Market Watch wasn't storing focus because the window activation happened at the wrong time

## Solution Implemented

### 1. **Moved unregisterWindow() to WindowCacheManager**

Instead of calling `unregisterWindow()` in the window's `closeEvent()`, we now call it in the WindowCacheManager's `mark*WindowClosed()` methods:

#### `WindowCacheManager.cpp`:
```cpp
void WindowCacheManager::markSnapQuoteWindowClosed(int windowIndex) {
  if (windowIndex >= 0 && windowIndex < m_snapQuoteWindows.size()) {
    auto &entry = m_snapQuoteWindows[windowIndex];
    entry.isVisible = false;
    entry.needsReset = true;
    entry.lastToken = -1;

    qDebug() << "[WindowCacheManager] SnapQuote window" << (windowIndex + 1)
             << "marked as closed (available for reuse)";
    
    // ✓ Unregister from WindowManager to trigger focus restoration
    if (entry.window) {
      WindowManager::instance().unregisterWindow(entry.window);
    }
  }
}

void WindowCacheManager::markBuyWindowClosed() {
  m_buyWindowNeedsReset = true;
  m_lastBuyToken = -1;
  
  qDebug() << "[WindowCacheManager] Buy window marked as closed";
  
  // ✓ Unregister from WindowManager to trigger focus restoration
  if (m_buyWindow) {
    WindowManager::instance().unregisterWindow(m_buyWindow);
  }
}

void WindowCacheManager::markSellWindowClosed() {
  m_sellWindowNeedsReset = true;
  m_lastSellToken = -1;
  
  qDebug() << "[WindowCacheManager] Sell window marked as closed";
  
  // ✓ Unregister from WindowManager to trigger focus restoration
  if (m_sellWindow) {
    WindowManager::instance().unregisterWindow(m_sellWindow);
  }
}
```

### 2. **Removed unregisterWindow() from Window closeEvent Methods**

#### `SnapQuoteWindow.cpp`:
```cpp
void SnapQuoteWindow::closeEvent(QCloseEvent *event)
{
    // Don't unregister from WindowManager here for cached windows
    // The WindowCacheManager will handle the window lifecycle
    // Only unregister when actually being destroyed
    
    QWidget::closeEvent(event);
}
```

#### `BuyWindow.cpp`:
```cpp
void BuyWindow::closeEvent(QCloseEvent *event) {
    WindowSettingsHelper::saveWindowSettings(this, "BuyWindow");
    
    // Don't unregister from WindowManager here for cached windows
    // The WindowCacheManager will handle the window lifecycle
    
    BaseOrderWindow::closeEvent(event);
}
```

#### `SellWindow.cpp`:
```cpp
void SellWindow::closeEvent(QCloseEvent *event) {
    WindowSettingsHelper::saveWindowSettings(this, "SellWindow");
    
    // Don't unregister from WindowManager here for cached windows
    // The WindowCacheManager will handle the window lifecycle
    
    BaseOrderWindow::closeEvent(event);
}
```

## How It Works Now

### Sequence When Closing a Cached Window:

1. **User closes SnapQuote** (presses ESC)
   ```
   [MDISubWindow] Escape pressed - closing "Snap Quote 1"
   [MDISubWindow] closeEvent for "Snap Quote 1"
   ```

2. **CustomMDISubWindow detects it's a cached window**
   ```
   [MDISubWindow] Cached window - moving off-screen instead of closing
   ```

3. **WindowCacheManager is notified**
   ```cpp
   WindowCacheManager::instance().markSnapQuoteWindowClosed(windowIndex);
   ```

4. **WindowCacheManager marks window as closed and unregisters from WindowManager**
   ```
   [WindowCacheManager] SnapQuote window 1 marked as closed (available for reuse)
   ```
   
5. **WindowManager automatically activates the previous window**
   ```cpp
   WindowManager::instance().unregisterWindow(entry.window);
   // This triggers activation of the last window in the stack
   ```

6. **Market Watch regains focus**
   ```
   [WindowManager] ✓ Activated previous window: "Market Watch 4"
   [MainWindow] Market Watch window activated, scheduling focus restore
   [MarketWatch] ✓ Restored focus to token: 59176
   ```

## Expected Logs After Fix

When you close SnapQuote and Market Watch regains focus:

```
[2026-02-05 17:01:15.502] [DEBUG] [MDISubWindow] closeEvent for "Snap Quote 1"
[2026-02-05 17:01:15.502] [DEBUG] [MDISubWindow] Cached window - moving off-screen
[2026-02-05 17:01:15.518] [DEBUG] [WindowCacheManager] SnapQuote window 1 marked as closed
[2026-02-05 17:01:15.520] [DEBUG] [WindowManager] Unregistered window: "Window_1903738010272"
[2026-02-05 17:01:15.572] [DEBUG] [WindowManager] ✓ Activated previous window: "Window_1903742062784"
[2026-02-05 17:01:15.620] [DEBUG] [MainWindow] Market Watch window activated
[2026-02-05 17:01:15.670] [DEBUG] [MarketWatch] Focus gained, scheduling delayed focus restoration
[2026-02-05 17:01:15.720] [DEBUG] [MarketWatch] ✓ Restored focus to token: 59176 Source Row: 4
```

## Files Modified

1. **`src/core/WindowCacheManager.cpp`**
   - Added `#include "utils/WindowManager.h"`
   - Updated `markSnapQuoteWindowClosed()` to call `unregisterWindow()`
   - Moved `markBuyWindowClosed()` and `markSellWindowClosed()` from header to cpp
   - Added `unregisterWindow()` calls in both methods

2. **`include/core/WindowCacheManager.h`**
   - Changed `markBuyWindowClosed()` and `markSellWindowClosed()` from inline to declared methods

3. **`src/views/SnapQuoteWindow.cpp`**
   - Removed `unregisterWindow()` call from `closeEvent()`

4. **`src/views/BuyWindow.cpp`**
   - Removed `unregisterWindow()` call from `closeEvent()`

5. **`src/views/SellWindow.cpp`**
   - Removed `unregisterWindow()` call from `closeEvent()`

## Benefits

✅ **Proper Timing**: `unregisterWindow()` is called AFTER the window is marked as closed, not during the close event
✅ **Automatic Focus**: Market Watch automatically regains focus when SnapQuote/Buy/Sell windows are closed
✅ **Keyboard Navigation**: Arrow keys work immediately without manual clicking
✅ **Clean Architecture**: WindowCacheManager controls the lifecycle, not individual windows
✅ **No Double Activation**: No race conditions or premature activations

## Testing

To verify the fix:

1. **Open Market Watch** and select a scrip (e.g., FINNIFTY)
2. **Press Ctrl+Q** to open SnapQuote
3. **Press ESC** to close SnapQuote
4. **Expected**: Market Watch automatically activates and FINNIFTY is selected
5. **Press arrow keys** - they should work immediately without clicking

The logs should show:
```
[WindowManager] ✓ Activated previous window: "Market Watch 4"
[MarketWatch] ✓ Restored focus to token: 59176
```

## Conclusion

By moving the `unregisterWindow()` call from the window's `closeEvent()` to the WindowCacheManager's `mark*WindowClosed()` methods, we ensure that:
- The window is properly marked as closed in the cache
- The WindowManager is notified at the correct time
- The previous window is activated automatically
- Focus is restored correctly

This provides a seamless user experience where closing any window automatically returns focus to the previously active window.
