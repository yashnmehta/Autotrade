# Tab Focus Containment - Root Cause Analysis & Solution

**Date**: December 14, 2024  
**Issue**: Tab key navigation escaping from Buy Window to other MDI windows (Market Watch)  
**Status**: ✅ RESOLVED

---

## Root Cause Analysis

### Problem Identification

When pressing Tab in the Buy Window, focus would escape to other MDI child windows (like Market Watch) instead of cycling within the Buy Window itself.

### Architecture Analysis

1. **MDI Structure**:
   ```
   MainWindow
   └── CustomMDIArea (QWidget-based)
       ├── CustomMDISubWindow ("Buy Order")
       │   └── BuyWindow (QWidget)
       │       └── m_formWidget (loaded from .ui file)
       │           └── 21 focusable widgets
       └── CustomMDISubWindow ("Market Watch 1")
           └── MarketWatchWindow
   ```

2. **Qt Focus Navigation**:
   - Qt's default `QWidget::focusNextPrevChild()` traverses the entire widget hierarchy
   - When Tab is pressed on the last widget, Qt searches parent hierarchy for next focusable widget
   - Since CustomMDISubWindow and all child windows are siblings in CustomMDIArea, focus can jump between them

3. **Why `setFocusPolicy()` Doesn't Work**:
   - `setFocusPolicy(Qt::StrongFocus)` only controls **how** a widget receives focus (tab, click, wheel)
   - It does NOT control **where** focus goes when Tab is pressed
   - Focus traversal is controlled by `focusNextPrevChild()` method

### Investigation Evidence

**Initial Attempt** (FAILED):
```cpp
// In BuyWindow constructor
setFocusPolicy(Qt::StrongFocus);
m_formWidget->setFocusPolicy(Qt::StrongFocus);
```
**Result**: Focus still escaped to Market Watch

**Root Cause**: Qt's `QWidget::focusNextPrevChild()` was not overridden, so default behavior continued to search entire parent hierarchy.

---

## Solution Implementation

### Strategy

Override `focusNextPrevChild(bool next)` in BuyWindow to:
1. Build a list of all focusable widgets in tab order
2. Find current focused widget
3. Calculate next/previous widget with **wraparound**
4. Set focus manually
5. Return `true` to prevent Qt from searching further

### Code Changes

#### 1. BuyWindow.h
```cpp
protected:
    // Override to keep focus within the window (prevent tab escape)
    bool focusNextPrevChild(bool next) override;
```

#### 2. BuyWindow.cpp
```cpp
#include <QApplication>  // Added for QApplication::focusWidget()

bool BuyWindow::focusNextPrevChild(bool next)
{
    // Build list of all focusable widgets in our tab order
    QList<QWidget*> focusableWidgets;
    
    if (m_cbEx && m_cbEx->isEnabled()) focusableWidgets.append(m_cbEx);
    if (m_cbInstrName && m_cbInstrName->isEnabled()) focusableWidgets.append(m_cbInstrName);
    if (m_cbOrdType && m_cbOrdType->isEnabled()) focusableWidgets.append(m_cbOrdType);
    if (m_leToken && m_leToken->isEnabled()) focusableWidgets.append(m_leToken);
    if (m_leInsType && m_leInsType->isEnabled()) focusableWidgets.append(m_leInsType);
    if (m_leSymbol && m_leSymbol->isEnabled()) focusableWidgets.append(m_leSymbol);
    if (m_cbExp && m_cbExp->isEnabled()) focusableWidgets.append(m_cbExp);
    if (m_cbStrike && m_cbStrike->isEnabled()) focusableWidgets.append(m_cbStrike);
    if (m_cbOptType && m_cbOptType->isEnabled()) focusableWidgets.append(m_cbOptType);
    if (m_leQty && m_leQty->isEnabled()) focusableWidgets.append(m_leQty);
    if (m_leDiscloseQty && m_leDiscloseQty->isEnabled()) focusableWidgets.append(m_leDiscloseQty);
    if (m_leRate && m_leRate->isEnabled()) focusableWidgets.append(m_leRate);
    if (m_leSetflor && m_leSetflor->isEnabled()) focusableWidgets.append(m_leSetflor);
    if (m_leTrigPrice && m_leTrigPrice->isEnabled()) focusableWidgets.append(m_leTrigPrice);
    if (m_cbMFAON && m_cbMFAON->isEnabled()) focusableWidgets.append(m_cbMFAON);
    if (m_leMFQty && m_leMFQty->isEnabled()) focusableWidgets.append(m_leMFQty);
    if (m_cbProduct && m_cbProduct->isEnabled()) focusableWidgets.append(m_cbProduct);
    if (m_cbValidity && m_cbValidity->isEnabled()) focusableWidgets.append(m_cbValidity);
    if (m_leRemarks && m_leRemarks->isEnabled()) focusableWidgets.append(m_leRemarks);
    if (m_pbSubmit && m_pbSubmit->isEnabled()) focusableWidgets.append(m_pbSubmit);
    if (m_pbClear && m_pbClear->isEnabled()) focusableWidgets.append(m_pbClear);
    
    if (focusableWidgets.isEmpty()) {
        return false;  // No focusable widgets
    }
    
    // Find current focused widget
    QWidget *currentFocus = QApplication::focusWidget();
    int currentIndex = focusableWidgets.indexOf(currentFocus);
    
    // If no widget has focus, focus the first one
    if (currentIndex == -1) {
        focusableWidgets.first()->setFocus();
        return true;
    }
    
    // Calculate next index with WRAPAROUND
    int nextIndex;
    if (next) {
        nextIndex = (currentIndex + 1) % focusableWidgets.size();  // Forward wrap
    } else {
        nextIndex = (currentIndex - 1 + focusableWidgets.size()) % focusableWidgets.size();  // Backward wrap
    }
    
    // Set focus to next/previous widget
    focusableWidgets[nextIndex]->setFocus();
    
    qDebug() << "[BuyWindow] Tab navigation:" << (next ? "forward" : "backward") 
             << "from index" << currentIndex << "to" << nextIndex;
    
    return true;  // We handled the focus change - stop Qt from searching further
}
```

---

## Verification

### Debug Output
```
[BuyWindow] Tab navigation: forward from index 18 to 19
[BuyWindow] Tab navigation: forward from index 19 to 20
[BuyWindow] Tab navigation: forward from index 20 to 0   ✅ WRAPS AROUND!
[BuyWindow] Tab navigation: forward from index 0 to 1
[BuyWindow] Tab navigation: forward from index 1 to 2
```

### Test Cases

| Test | Input | Expected | Result |
|------|-------|----------|--------|
| Forward Tab on Last Widget | Tab on pbClear (index 20) | Focus wraps to cbEx (index 0) | ✅ PASS |
| Forward Tab mid-form | Tab on leQty (index 9) | Focus moves to leDiscloseQty (index 10) | ✅ PASS |
| Backward Tab on First Widget | Shift+Tab on cbEx (index 0) | Focus wraps to pbClear (index 20) | ✅ PASS |
| Tab with Disabled Widgets | Tab skips disabled widgets | Only enabled widgets receive focus | ✅ PASS |
| Focus Never Escapes | Tab through entire cycle | Focus never leaves Buy Window | ✅ PASS |

---

## Key Insights

### Why This Solution Works

1. **Complete Control**: By overriding `focusNextPrevChild()`, we intercept Qt's focus traversal **before** it searches parent hierarchy

2. **Explicit List**: We explicitly define which widgets are focusable in which order, giving us complete control

3. **Wraparound Logic**: Modulo arithmetic ensures focus cycles indefinitely within the window

4. **Return True**: Returning `true` tells Qt "we handled it" so Qt doesn't continue searching

### Alternative Approaches Considered

| Approach | Why It Failed |
|----------|---------------|
| `setFocusPolicy()` | Doesn't control focus traversal, only how focus is received |
| `setTabOrder()` | Only affects order within a window, doesn't prevent escape |
| Focus Proxy | Too complex, doesn't solve root cause |
| Event Filter on Tab Key | Misses Shift+Tab, complex event handling |
| Custom `event()` override | More invasive than `focusNextPrevChild()` |

### Best Practices

1. **Check Widget Enabled State**: Use `isEnabled()` to respect disabled widgets
2. **Handle Empty List**: Return false if no widgets are focusable
3. **Debug Logging**: Log navigation for troubleshooting
4. **Match Tab Order**: List order should match `.ui` file `<tabstops>` section
5. **Return True**: Always return true if you handled the focus change

---

## Application to Other Windows

### SellWindow Implementation

The same fix should be applied to SellWindow:

```cpp
// SellWindow.h
protected:
    bool focusNextPrevChild(bool next) override;

// SellWindow.cpp
bool SellWindow::focusNextPrevChild(bool next)
{
    // Same logic as BuyWindow with SellWindow's widget list
    QList<QWidget*> focusableWidgets;
    // ... add all SellWindow widgets ...
    
    // Same wraparound logic
    // ...
}
```

### MarketWatchWindow

MarketWatch has a table widget which handles its own tab navigation, but if focus escape is needed:

```cpp
// For table-based windows, override to keep focus on table
bool MarketWatchWindow::focusNextPrevChild(bool next)
{
    if (m_tableView) {
        m_tableView->setFocus();
        return true;  // Keep focus on table
    }
    return QWidget::focusNextPrevChild(next);
}
```

---

## Performance Considerations

- **Execution Time**: O(n) where n = number of widgets (~21), negligible
- **Memory**: Single QList allocation on each tab press, automatically freed
- **CPU**: Only runs on Tab/Shift+Tab keypresses, not continuous
- **Impact**: Zero performance impact on trading operations

---

## References

- Qt Documentation: [QWidget::focusNextPrevChild()](https://doc.qt.io/qt-5/qwidget.html#focusNextPrevChild)
- Qt Documentation: [Focus Policies](https://doc.qt.io/qt-5/qwidget.html#focusPolicy-prop)
- Qt Source: `qwidget.cpp` - Focus chain implementation
- Stack Overflow: "Containing focus within a QWidget"

---

## Summary

**Problem**: Tab key navigation escaped from Buy Window to other MDI windows

**Root Cause**: Qt's default `focusNextPrevChild()` searches entire parent hierarchy

**Solution**: Override `focusNextPrevChild()` to manually manage focus with wraparound

**Result**: Focus now cycles indefinitely within Buy Window, never escaping to other windows

**Status**: ✅ **RESOLVED** - Verified working in application testing
