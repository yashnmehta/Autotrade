# Window Manager - Global Focus Management Implementation

## Overview
Implemented a global `WindowManager` singleton to track and manage window focus across the entire application. This ensures that when a window is closed, the previously active window automatically regains focus, providing a seamless user experience.

## Problem Statement
- When closing a window (SnapQuote, Buy, Sell), the Market Watch window didn't automatically regain focus
- Users had to manually click on Market Watch to activate it and enable keyboard navigation
- No centralized system to track the window activation history

## Solution: Global WindowManager

### Architecture

#### **WindowManager Class** (`utils/WindowManager.h`)
A singleton class that maintains a stack of active windows:

```cpp
class WindowManager : public QObject {
    Q_OBJECT
public:
    static WindowManager& instance();
    
    void registerWindow(QWidget* window, const QString& windowName);
    void unregisterWindow(QWidget* window);
    void bringToTop(QWidget* window);
    QWidget* getActiveWindow() const;
    
private:
    struct WindowEntry {
        QWidget* window;
        QString name;
        qint64 timestamp;
    };
    
    QList<WindowEntry> m_windowStack;
};
```

### Key Features

1. **Stack-Based Window Tracking**
   - Windows are stored in a stack (most recent at the top)
   - When a window is activated, it's moved to the top
   - When a window is closed, the next window in the stack is activated

2. **Automatic Focus Restoration**
   - When `unregisterWindow()` is called, the manager automatically activates the previous window
   - Uses a 50ms delay to ensure proper focus handling
   - Only activates visible windows

3. **Timestamp Tracking**
   - Each window entry includes a timestamp for debugging and ordering
   - Helps track the exact order of window activations

## Integration Points

### 1. **MarketWatchWindow**
```cpp
// Constructor
MarketWatchWindow::MarketWatchWindow(QWidget *parent) {
    // ...setup code...
    
    // Register with WindowManager
    WindowManager::instance().registerWindow(this, windowTitle());
}

// Destructor
MarketWatchWindow::~MarketWatchWindow() {
    // Unregister from WindowManager
    WindowManager::instance().unregisterWindow(this);
}

// Focus events
void MarketWatchWindow::focusInEvent(QFocusEvent *event) {
    CustomMarketWatch::focusInEvent(event);
    
    // Notify WindowManager that this window has gained focus
    WindowManager::instance().bringToTop(this);
    
    // ...existing focus restoration logic...
}

// Close events
void MarketWatchWindow::closeEvent(QCloseEvent *event) {
    WindowSettingsHelper::saveWindowSettings(this, "MarketWatch");
    
    // Unregister from WindowManager before closing
    WindowManager::instance().unregisterWindow(this);
    
    QWidget::closeEvent(event);
}
```

### 2. **SnapQuoteWindow**
```cpp
// Constructor
SnapQuoteWindow::SnapQuoteWindow(QWidget *parent) {
    initUI();
    
    // Register with WindowManager
    WindowManager::instance().registerWindow(this, windowTitle());
}

// Destructor
SnapQuoteWindow::~SnapQuoteWindow() {
    // Unregister from WindowManager
    WindowManager::instance().unregisterWindow(this);
}

// Show events
void SnapQuoteWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    
    // Notify WindowManager that this window is now active
    WindowManager::instance().bringToTop(this);
}

// Focus events
void SnapQuoteWindow::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    
    // Notify WindowManager that this window has gained focus
    WindowManager::instance().bringToTop(this);
}

// Close events
void SnapQuoteWindow::closeEvent(QCloseEvent *event) {
    // Unregister from WindowManager before closing
    WindowManager::instance().unregisterWindow(this);
    
    QWidget::closeEvent(event);
}
```

### 3. **BuyWindow**
```cpp
// Constructor
BuyWindow::BuyWindow(QWidget *parent) {
    setInstance(this);
    WindowSettingsHelper::loadAndApplyWindowSettings(this, "BuyWindow");
    
    // Register with WindowManager
    WindowManager::instance().registerWindow(this, "Buy Window");
}

// Destructor
BuyWindow::~BuyWindow() {
    // Unregister from WindowManager
    WindowManager::instance().unregisterWindow(this);
    
    if (s_instance == this) s_instance = nullptr;
}

// Focus events
void BuyWindow::focusInEvent(QFocusEvent *event) {
    BaseOrderWindow::focusInEvent(event);
    
    // Notify WindowManager that this window has gained focus
    WindowManager::instance().bringToTop(this);
}

// Close events
void BuyWindow::closeEvent(QCloseEvent *event) {
    WindowSettingsHelper::saveWindowSettings(this, "BuyWindow");
    
    // Unregister from WindowManager before closing
    WindowManager::instance().unregisterWindow(this);
    
    BaseOrderWindow::closeEvent(event);
}
```

## How It Works

### Scenario 1: Opening and Closing Windows
```
1. User opens Market Watch
   → WindowManager registers "Market Watch" (stack: [MW])

2. User opens SnapQuote from Market Watch
   → WindowManager registers "Snap Quote 1" (stack: [SQ, MW])

3. User closes SnapQuote
   → WindowManager unregisters "Snap Quote 1"
   → Automatically activates Market Watch (stack: [MW])
   → Market Watch becomes active and receives focus
```

### Scenario 2: Multiple Window Sequence
```
1. Market Watch opened (stack: [MW])
2. SnapQuote opened (stack: [SQ, MW])
3. Buy Window opened (stack: [BW, SQ, MW])
4. User closes Buy Window
   → Activates SnapQuote (stack: [SQ, MW])
5. User closes SnapQuote
   → Activates Market Watch (stack: [MW])
```

### Scenario 3: Window Activation
```
1. Stack: [BW, SQ, MW]
2. User clicks on Market Watch
   → focusInEvent() called
   → bringToTop(MW) called
   → Stack reordered: [MW, BW, SQ]
3. User closes Market Watch
   → Buy Window becomes active (stack: [BW, SQ])
```

## Key Methods

### `registerWindow(QWidget* window, const QString& windowName)`
- Adds a window to the top of the stack
- If window already exists, moves it to the top
- Updates timestamp for tracking

### `unregisterWindow(QWidget* window)`
- Removes window from the stack
- **Automatically activates the next window** using:
  - `activateWindow()` - Makes window active
  - `raise()` - Brings window to front
  - `setFocus()` - Gives keyboard focus
- Uses 50ms delay for proper focus handling

### `bringToTop(QWidget* window)`
- Moves an existing window to the top of the stack
- Called when a window gains focus
- Ensures the stack reflects the actual focus order

## Debug Logging

The WindowManager provides detailed logging for debugging:

```
[WindowManager] Initialized
[WindowManager] Registered new window: Market Watch Stack size: 1
[WindowManager] Registered new window: Snap Quote 1 Stack size: 2
[WindowManager] Unregistered window: Snap Quote 1 Remaining windows: 1
[WindowManager] ✓ Activated previous window: Market Watch
[WindowManager] Brought window to top: Market Watch Previous position: 1
```

## Performance Impact
- **Minimal overhead**: O(n) operations where n = number of open windows (typically < 10)
- **Memory**: ~40 bytes per window entry (negligible)
- **Activation delay**: 50ms (imperceptible to users, ensures proper focus)

## Benefits

1. **Seamless User Experience**
   - No manual clicking required to restore focus
   - Keyboard navigation works immediately after closing windows

2. **Centralized Management**
   - Single source of truth for window focus order
   - Easy to debug focus issues
   - Extensible for future features

3. **Automatic Behavior**
   - No need for manual focus management in individual windows
   - Consistent behavior across all windows

4. **Robust Error Handling**
   - Validates window visibility before activation
   - Handles edge cases (null windows, empty stack)
   - Provides clear debug logs

## Files Created

1. **`include/utils/WindowManager.h`**
   - WindowManager class definition
   - WindowEntry structure
   - Public API methods

2. **`src/utils/WindowManager.cpp`**
   - WindowManager implementation
   - Stack management logic
   - Automatic focus restoration

## Files Modified

1. **`src/views/MarketWatchWindow/MarketWatchWindow.cpp`**
   - Added `#include "utils/WindowManager.h"`
   - Register/unregister in constructor/destructor
   - Call `bringToTop()` in `focusInEvent()`
   - Call `unregisterWindow()` in `closeEvent()`

2. **`src/views/SnapQuoteWindow/SnapQuoteWindow.cpp`**
   - Added `#include "utils/WindowManager.h"`
   - Register/unregister in constructors/destructor
   - Call `bringToTop()` in `showEvent()` and `focusInEvent()`
   - Call `unregisterWindow()` in `closeEvent()`

3. **`include/views/SnapQuoteWindow.h`**
   - Added `focusInEvent()` and `closeEvent()` method declarations

4. **`src/views/BuyWindow.cpp`**
   - Added `#include "utils/WindowManager.h"`
   - Register/unregister in constructor/destructor
   - Call `bringToTop()` in `focusInEvent()`
   - Call `unregisterWindow()` in `closeEvent()`

5. **`CMakeLists.txt`**
   - Added `src/utils/WindowManager.cpp` to source files

## Future Enhancements

1. **Window History**
   - Add ability to cycle through recently active windows (Alt+Tab style)
   - Store last N activated windows for quick switching

2. **Window Groups**
   - Group related windows together (e.g., all Market Watch windows)
   - Activate within groups for better organization

3. **Persistence**
   - Save window stack order between sessions
   - Restore last active window on application restart

4. **Focus Policies**
   - Add configurable focus policies (e.g., always focus Market Watch)
   - Allow users to customize activation behavior

5. **Integration with Window Caching**
   - Coordinate with `WindowCacheManager` for cached windows
   - Ensure proper focus restoration for reused windows

## Testing Checklist

✅ **Test 1: Basic Window Sequence**
- Open Market Watch → Open SnapQuote → Close SnapQuote
- **Expected**: Market Watch automatically gains focus

✅ **Test 2: Multiple Windows**
- Open MW → Open SQ → Open BW → Close BW
- **Expected**: SnapQuote gains focus
- Close SQ → **Expected**: Market Watch gains focus

✅ **Test 3: Manual Window Activation**
- Open MW, SQ, BW (all visible)
- Click on Market Watch manually
- Close Market Watch
- **Expected**: Buy Window gains focus (was second in stack)

✅ **Test 4: Rapid Window Opening/Closing**
- Rapidly open and close multiple windows
- **Expected**: No crashes, focus always returns to previous window

✅ **Test 5: Keyboard Navigation**
- Open MW → Select scrip → Open SQ → Close SQ
- **Expected**: MW has focus, arrow keys work immediately

## Conclusion

The `WindowManager` provides a robust, centralized solution for managing window focus across the application. It eliminates the need for manual clicking to restore focus and ensures a seamless user experience when navigating between windows. The implementation is lightweight, extensible, and integrates cleanly with existing code.
