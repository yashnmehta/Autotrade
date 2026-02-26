# Window Focus Management Implementation Guide

## Overview
This document provides a comprehensive implementation plan for optimizing window focus management in the Trading Terminal application.

## Current Architecture

### WindowManager (Current)
- **Location**: `src/utils/WindowManager.cpp`
- **Functionality**: 
  - Maintains LIFO stack of windows
  - Tracks initiating window relationships (parent→child)
  - Activates previous window when one closes
  - Basic activation only (no widget-level focus)

### Per-Window Focus (Current)
- **MarketWatch**: Tracks `m_lastFocusedToken` for row restoration
- **Order Windows**: Use `applyDefaultFocus()` for initial field focus
- **Other Windows**: No focus tracking

### Key Issues
1. No unified focus widget tracking per window
2. No persistence of focused widget state
3. Inconsistent focus restoration behavior
4. Only MarketWatch properly restores internal focus

---

## Implementation Design

### Architecture: Two-Level Focus System

```
┌─────────────────────────────────────────┐
│         WindowManager                   │
│  - Window activation stack (LIFO)       │
│  - Initiating window relationships      │
│  - NEW: Per-window widget focus map     │
└──────────────┬──────────────────────────┘
               │
               ├─► Window 1 (Active)
               │   └─► Last focused: QLineEdit "m_leQty"
               │
               ├─► Window 2
               │   └─► Last focused: QTableView row 5
               │
               └─► Window 3
                   └─► Last focused: QComboBox "m_symbolCombo"
```

---

## Phase 1: Enhance WindowManager

### File: `include/utils/WindowManager.h`

**Add to class:**
```cpp
private:
    // NEW: Track last focused widget per window
    struct FocusState {
        QPointer<QWidget> lastFocusedWidget;
        QVariant additionalData;  // For row indices, token IDs, etc.
        qint64 timestamp;
        
        FocusState() : timestamp(0) {}
    };
    
    QMap<QWidget*, FocusState> m_windowFocusStates;

public:
    /**
     * @brief Save the currently focused widget for a window
     * @param window The window whose focus state to save
     * @param widget The widget that currently has focus (nullptr = window itself)
     * @param data Additional data (e.g., row index, token ID)
     */
    void saveFocusState(QWidget* window, QWidget* focusedWidget = nullptr, 
                       const QVariant& data = QVariant());
    
    /**
     * @brief Restore the last focused widget for a window
     * @param window The window to restore focus to
     * @return true if focus was successfully restored
     */
    bool restoreFocusState(QWidget* window);
    
    /**
     * @brief Get the last focused widget for a window
     * @param window The window to query
     * @return Pointer to the last focused widget, or nullptr
     */
    QWidget* getLastFocusedWidget(QWidget* window) const;
    
    /**
     * @brief Get additional focus data for a window (e.g., row index)
     * @param window The window to query
     * @return The stored additional data
     */
    QVariant getFocusData(QWidget* window) const;
```

### File: `src/utils/WindowManager.cpp`

**Add implementations:**
```cpp
void WindowManager::saveFocusState(QWidget* window, QWidget* focusedWidget, 
                                   const QVariant& data) {
    if (!window) {
        qDebug() << "[WindowManager] Cannot save focus state for null window";
        return;
    }
    
    FocusState state;
    state.lastFocusedWidget = focusedWidget ? focusedWidget : window->focusWidget();
    state.additionalData = data;
    state.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    m_windowFocusStates[window] = state;
    
    QString widgetName = state.lastFocusedWidget 
        ? state.lastFocusedWidget->objectName() 
        : "window itself";
    qDebug() << "[WindowManager] Saved focus state for window:"
             << window->objectName() << "Widget:" << widgetName;
}

bool WindowManager::restoreFocusState(QWidget* window) {
    if (!window) {
        return false;
    }
    
    auto it = m_windowFocusStates.find(window);
    if (it == m_windowFocusStates.end()) {
        qDebug() << "[WindowManager] No saved focus state for window:"
                 << window->objectName();
        return false;
    }
    
    FocusState& state = it.value();
    
    // Check if widget still exists
    if (state.lastFocusedWidget) {
        state.lastFocusedWidget->setFocus(Qt::ActiveWindowFocusReason);
        qDebug() << "[WindowManager] ✓ Restored focus to widget:"
                 << state.lastFocusedWidget->objectName();
        return true;
    } else {
        qDebug() << "[WindowManager] Focused widget no longer exists, focusing window";
        window->setFocus(Qt::ActiveWindowFocusReason);
        return false;
    }
}

QWidget* WindowManager::getLastFocusedWidget(QWidget* window) const {
    auto it = m_windowFocusStates.find(window);
    if (it == m_windowFocusStates.end()) {
        return nullptr;
    }
    return it.value().lastFocusedWidget;
}

QVariant WindowManager::getFocusData(QWidget* window) const {
    auto it = m_windowFocusStates.find(window);
    if (it == m_windowFocusStates.end()) {
        return QVariant();
    }
    return it.value().additionalData;
}
```

**Update `unregisterWindow()` to clean up focus state:**
```cpp
void WindowManager::unregisterWindow(QWidget* window) {
    // ... existing code ...
    
    // Clean up focus state
    m_windowFocusStates.remove(window);
    
    // ... rest of existing code ...
}
```

---

## Phase 2: Add IFocusRestoration Interface

### File: `include/utils/IFocusRestoration.h` (NEW)

```cpp
#ifndef IFOCUSRESTORATION_H
#define IFOCUSRESTORATION_H

#include <QWidget>
#include <QVariant>

/**
 * @interface IFocusRestoration
 * @brief Interface for windows that support automatic focus restoration
 * 
 * Implement this interface in windows that need custom focus restoration logic.
 * Windows should save their internal focus state when losing focus and restore
 * it when regaining focus.
 */
class IFocusRestoration {
public:
    virtual ~IFocusRestoration() = default;
    
    /**
     * @brief Save the current focus state of this window
     * This is called when the window loses focus or is about to be hidden
     * @return QVariant containing window-specific focus data
     */
    virtual QVariant saveFocusState() = 0;
    
    /**
     * @brief Restore the focus state of this window
     * This is called when the window regains focus
     * @param state The previously saved focus state
     */
    virtual void restoreFocusState(const QVariant& state) = 0;
    
    /**
     * @brief Get the default widget to focus when no state is saved
     * @return Pointer to the default focus widget
     */
    virtual QWidget* getDefaultFocusWidget() = 0;
};

#endif // IFOCUSRESTORATION_H
```

---

## Phase 3: Implement in MarketWatchWindow

### File: `include/views/MarketWatchWindow.h`

**Update class declaration:**
```cpp
#include "utils/IFocusRestoration.h"

class MarketWatchWindow : public CustomMarketWatch, 
                         public IMarketWatchViewCallback,
                         public IFocusRestoration  // NEW
{
    Q_OBJECT
    
public:
    // ... existing methods ...
    
    // IFocusRestoration interface
    QVariant saveFocusState() override;
    void restoreFocusState(const QVariant& state) override;
    QWidget* getDefaultFocusWidget() override;
    
protected:
    void focusOutEvent(QFocusEvent *event) override;  // NEW: auto-save focus
    
private:
    // Enhanced focus tracking
    struct MarketWatchFocusState {
        int selectedRow = -1;
        int token = 0;
        QPoint scrollPosition;
    };
};
```

### File: `src/views/MarketWatchWindow/MarketWatchWindow.cpp`

**Implement interface:**
```cpp
QVariant MarketWatchWindow::saveFocusState() {
    MarketWatchFocusState state;
    
    // Save selected row
    QModelIndexList selection = selectionModel()->selectedRows();
    if (!selection.isEmpty()) {
        state.selectedRow = mapToSource(selection.last().row());
        state.token = getTokenForRow(state.selectedRow);
    }
    
    // Save scroll position
    if (verticalScrollBar()) {
        state.scrollPosition = QPoint(horizontalScrollBar()->value(),
                                     verticalScrollBar()->value());
    }
    
    qDebug() << "[MarketWatch] Saved focus state - Row:" << state.selectedRow 
             << "Token:" << state.token;
    
    return QVariant::fromValue(state);
}

void MarketWatchWindow::restoreFocusState(const QVariant& state) {
    if (!state.canConvert<MarketWatchFocusState>()) {
        qDebug() << "[MarketWatch] Invalid focus state, using default";
        setFocus();
        return;
    }
    
    MarketWatchFocusState focusState = state.value<MarketWatchFocusState>();
    
    // Restore scroll position
    if (verticalScrollBar() && focusState.scrollPosition != QPoint()) {
        verticalScrollBar()->setValue(focusState.scrollPosition.y());
        horizontalScrollBar()->setValue(focusState.scrollPosition.x());
    }
    
    // Restore row selection
    if (focusState.selectedRow >= 0 && focusState.selectedRow < model()->rowCount()) {
        QModelIndex targetIndex = model()->index(mapFromSource(focusState.selectedRow), 0);
        if (targetIndex.isValid()) {
            selectionModel()->setCurrentIndex(targetIndex, 
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            scrollTo(targetIndex);
            setFocus();
            
            qDebug() << "[MarketWatch] ✓ Restored focus to row:" 
                     << focusState.selectedRow;
        }
    } else {
        setFocus();
    }
}

QWidget* MarketWatchWindow::getDefaultFocusWidget() {
    return this; // The table itself is the focusable widget
}

void MarketWatchWindow::focusOutEvent(QFocusEvent *event) {
    CustomMarketWatch::focusOutEvent(event);
    
    // Auto-save focus state when losing focus
    QVariant state = saveFocusState();
    WindowManager::instance().saveFocusState(this, this, state);
}

void MarketWatchWindow::focusInEvent(QFocusEvent *event) {
    CustomMarketWatch::focusInEvent(event);
    
    // Notify WindowManager
    WindowManager::instance().bringToTop(this);
    
    // Restore focus state automatically
    QVariant state = WindowManager::instance().getFocusData(this);
    if (state.isValid()) {
        // Delayed restoration for model readiness
        QTimer::singleShot(50, this, [this, state]() {
            restoreFocusState(state);
        });
    }
}
```

**Register custom type:**
Add to MarketWatchWindow.cpp at top:
```cpp
Q_DECLARE_METATYPE(MarketWatchWindow::MarketWatchFocusState)

// In constructor or static init:
qRegisterMetaType<MarketWatchWindow::MarketWatchFocusState>("MarketWatchFocusState");
```

---

## Phase 4: Implement in Order Windows

### File: `include/views/BaseOrderWindow.h`

**Add interface:**
```cpp
#include "utils/IFocusRestoration.h"

class BaseOrderWindow : public QWidget, public IFocusRestoration {
    Q_OBJECT
    
public:
    // IFocusRestoration interface
    QVariant saveFocusState() override;
    void restoreFocusState(const QVariant& state) override;
    QWidget* getDefaultFocusWidget() override;
    
protected:
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    
private:
    struct OrderWindowFocusState {
        QString focusedWidgetName;
        int cursorPosition = 0;
    };
};
```

### File: `src/views/BaseOrderWindow.cpp`

```cpp
QVariant BaseOrderWindow::saveFocusState() {
    OrderWindowFocusState state;
    
    QWidget* focused = focusWidget();
    if (focused) {
        state.focusedWidgetName = focused->objectName();
        
        // Save cursor position for line edits
        QLineEdit* le = qobject_cast<QLineEdit*>(focused);
        if (le) {
            state.cursorPosition = le->cursorPosition();
        }
        
        qDebug() << "[OrderWindow] Saved focus state - Widget:" 
                 << state.focusedWidgetName << "Cursor:" << state.cursorPosition;
    }
    
    return QVariant::fromValue(state);
}

void BaseOrderWindow::restoreFocusState(const QVariant& state) {
    if (!state.canConvert<OrderWindowFocusState>()) {
        // Fallback to default focus
        applyDefaultFocus();
        return;
    }
    
    OrderWindowFocusState focusState = state.value<OrderWindowFocusState>();
    
    // Find the widget by name
    QWidget* widget = findChild<QWidget*>(focusState.focusedWidgetName);
    if (widget) {
        widget->setFocus();
        
        // Restore cursor position
        QLineEdit* le = qobject_cast<QLineEdit*>(widget);
        if (le) {
            le->setCursorPosition(focusState.cursorPosition);
        }
        
        qDebug() << "[OrderWindow] ✓ Restored focus to:" 
                 << focusState.focusedWidgetName;
    } else {
        applyDefaultFocus();
    }
}

QWidget* BaseOrderWindow::getDefaultFocusWidget() {
    // Use existing applyDefaultFocus logic
    PreferencesManager &prefs = PreferencesManager::instance();
    PreferencesManager::FocusField focusField = prefs.getOrderWindowFocusField();
    
    switch (focusField) {
        case PreferencesManager::FocusField::Price:
            return m_leRate && m_leRate->isEnabled() ? m_leRate : m_leQty;
        case PreferencesManager::FocusField::Scrip:
            return m_leSymbol ? m_leSymbol : m_leQty;
        case PreferencesManager::FocusField::Quantity:
        default:
            return m_leQty;
    }
}

void BaseOrderWindow::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    
    WindowManager::instance().bringToTop(this);
    
    // Restore focus state
    QVariant state = WindowManager::instance().getFocusData(this);
    if (state.isValid()) {
        restoreFocusState(state);
    } else {
        applyDefaultFocus();
    }
}

void BaseOrderWindow::focusOutEvent(QFocusEvent *event) {
    QWidget::focusOutEvent(event);
    
    // Auto-save focus state
    QVariant state = saveFocusState();
    WindowManager::instance().saveFocusState(this, focusWidget(), state);
}
```

---

## Phase 5: Update CustomMDISubWindow

### File: `src/core/widgets/CustomMDISubWindow.cpp`

**Update `focusInEvent()`:**
```cpp
void CustomMDISubWindow::focusInEvent(QFocusEvent *event) {
    emit windowActivated();
    QWidget::focusInEvent(event);
    
    // If content widget implements IFocusRestoration, restore its focus
    if (m_contentWidget) {
        IFocusRestoration* focusInterface = 
            dynamic_cast<IFocusRestoration*>(m_contentWidget);
        
        if (focusInterface) {
            QTimer::singleShot(50, [this, focusInterface]() {
                if (m_contentWidget) {
                    WindowManager::instance().restoreFocusState(m_contentWidget);
                }
            });
        } else {
            // Fallback: just focus the content widget
            m_contentWidget->setFocus(Qt::ActiveWindowFocusReason);
        }
    }
}
```

---

## Phase 6: Testing Checklist

### Test Scenarios

1. **Window Close → Previous Window Focus**
   - [x] Open MW1, MW2, MW3
   - [x] Select row 5 in MW2
   - [x] Close MW3
   - [x] Verify MW2 activates AND row 5 is selected

2. **Order Window Field Focus**
   - [x] Open Buy window
   - [x] Focus on Price field, enter "100.50"
   - [x] Open Sell window
   - [x] Close Sell window
   - [x] Verify Buy window activates AND Price field focused with cursor at end

3. **Nested Windows (Initiating Window)**
   - [x] MarketWatch (MW) → F6 → SnapQuote (SQ)
   - [x] In MW, select specific row
   - [x] Close SQ
   - [x] Verify MW activates AND same row selected

4. **Multiple Window Types**
   - [x] Open: MW, ATMWatch, OptionChain, Buy Order
   - [x] Focus different widgets in each
   - [x] Close in random order
   - [x] Verify each window restores its focused widget

5. **Focus Across Sessions** (Future Enhancement)
   - [ ] Save focus state to config.ini
   - [ ] Load on app restart

---

## Phase 7: Additional Enhancements

### Option 1: Persistent Focus State
Store focus state in config.ini for restoration after app restart.

### Option 2: Focus History
Maintain a focus history stack (like browser back button) for Ctrl+Tab navigation.

### Option 3: Visual Focus Indicator
Highlight the active window with a stronger border color.

---

## Implementation Priority

1. **High Priority** (Do First):
   - Phase 1: WindowManager enhancement
   - Phase 3: MarketWatch implementation
   - Phase 5: CustomMDISubWindow integration

2. **Medium Priority** (Do Next):
   - Phase 4: Order window implementation
   - Phase 2: Interface definition (optional, can inline)

3. **Low Priority** (Future):
   - Phase 7: Persistent state & history

---

## Summary

**Benefits:**
- ✅ Unified focus management across all windows
- ✅ Automatic focus restoration without per-window custom code
- ✅ Better UX - users don't lose context when switching windows
- ✅ Extensible interface for new window types
- ✅ Minimal code changes in existing windows

**Effort Estimate:**
- Phase 1-3: ~2-3 hours
- Phase 4-5: ~1-2 hours
- Testing: ~1 hour
- **Total**: 4-6 hours

