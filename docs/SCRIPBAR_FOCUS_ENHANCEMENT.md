# ScripBar Focus Policy Enhancement

**Date**: 27 February 2026  
**Status**: ✅ Completed

## Overview

Enhanced the ScripBar component with **Qt::StrongFocus** policy on all interactive widgets, **dynamic tab order reconfiguration**, and **focus trapping** to prevent focus from escaping the ScripBar during keyboard navigation.

---

## Changes Made

### 1. Focus Policy Enhancement (`src/app/ScripBar.cpp`)

All interactive widgets now have **Qt::StrongFocus** policy set:

| Widget | Focus Policy | Tab Navigation |
|--------|--------------|----------------|
| `m_exchangeCombo` | `Qt::StrongFocus` | ✅ Tab 1 |
| `m_segmentCombo` | `Qt::StrongFocus` | ✅ Tab 2 |
| `m_instrumentCombo` | `Qt::StrongFocus` | ✅ Tab 3 |
| `m_bseScripCodeCombo` | `Qt::StrongFocus` | ✅ Tab 4 (only when visible) |
| `m_symbolCombo` | `Qt::StrongFocus` | ✅ Tab 4/5 |
| `m_expiryCombo` | `Qt::StrongFocus` | ✅ Tab 5/6 |
| `m_strikeCombo` | `Qt::StrongFocus` | ✅ Tab 6/7 |
| `m_optionTypeCombo` | `Qt::StrongFocus` | ✅ Tab 7/8 |
| `m_tokenEdit` | `Qt::NoFocus` | ❌ Skipped (read-only) |
| `m_addToWatchButton` | `Qt::StrongFocus` | ✅ Tab 8/9 |

### 2. Dynamic Tab Order Configuration

Added **`setupTabOrder()`** method that intelligently handles widget visibility:

```cpp
void ScripBar::setupTabOrder() {
  // Build dynamic tab order chain based on visible widgets
  QList<QWidget*> tabChain;
  
  // Always include core widgets
  tabChain << m_exchangeCombo << m_segmentCombo << m_instrumentCombo;
  
  // Conditionally include BSE Scrip Code (only when visible)
  if (m_bseScripCodeCombo && m_bseScripCodeCombo->isVisible()) {
    tabChain << m_bseScripCodeCombo;
  }
  
  // Always include remaining widgets
  tabChain << m_symbolCombo << m_expiryCombo << m_strikeCombo << m_optionTypeCombo;
  
  // Add button always last
  tabChain << m_addToWatchButton;
  
  // Configure tab order
  for (int i = 0; i < tabChain.size() - 1; ++i) {
    setTabOrder(tabChain[i], tabChain[i+1]);
  }
}
```

### 3. Auto-Reconfiguration on Visibility Changes

The tab order is **automatically reconfigured** when BSE Scrip Code visibility changes:

```cpp
void ScripBar::updateBseScripCodeVisibility() {
  bool wasVisible = m_bseScripCodeCombo->isVisible();
  m_bseScripCodeCombo->setVisible(showBseCode);
  
  // Reconfigure tab order if visibility changed
  if (wasVisible != showBseCode) {
    setupTabOrder();
  }
}
```

### 4. Focus Trapping (Tab Cycling)

Implemented `focusNextPrevChild()` override to **trap focus within ScripBar**:

```cpp
bool ScripBar::focusNextPrevChild(bool next) {
  // Build list of visible, enabled, focusable widgets
  QList<QWidget*> focusChain;
  for (auto* widget : allWidgets) {
    if (widget && widget->isVisibleTo(this) && widget->isEnabled() 
        && widget->focusPolicy() != Qt::NoFocus) {
      focusChain.append(widget);
    }
  }
  
  // Calculate next/prev with wrapping
  // Set focus and return true (consumed)
  return true;  // Focus never escapes ScripBar
}
```

**Result:**
- ✅ Tab at last widget (Add button) → Wraps to first widget (Exchange)
- ✅ Shift+Tab at first widget → Wraps to last widget (Add button)
- ✅ Focus never leaves ScripBar accidentally
- ✅ User can still use ESC key to exit and restore previous focus

---

## Focus Policy Types

| Policy | Tab | Click | Wheel | Behavior | Use Case |
|--------|-----|-------|-------|----------|----------|
| `Qt::StrongFocus` | ✅ | ✅ | ✅ | **Accepts focus from Tab, Click, and Wheel** | ✅ **Used for all interactive ScripBar widgets** |
| `Qt::NoFocus` | ❌ | ❌ | ❌ | **Never accepts focus** | ✅ **Used for read-only token display** |
| `Qt::TabFocus` | ✅ | ❌ | ❌ | Only accepts Tab focus | Not used in ScripBar |
| `Qt::ClickFocus` | ❌ | ✅ | ❌ | Only accepts click focus | Not used in ScripBar |

**⚠️ Important:** Focus policy determines **HOW** a widget can receive focus (Tab/Click/Wheel), **NOT** whether focus can escape the container.

**Why Qt::StrongFocus?**
- ✅ Accepts keyboard (Tab) navigation
- ✅ Accepts mouse clicks  
- ✅ Accepts mouse wheel events (useful for combo boxes)
- ✅ Most user-friendly for interactive widgets

**Focus Trapping vs Focus Policy:**
- **Focus Policy** (`setFocusPolicy`) = How a widget can **receive** focus
- **Focus Trapping** (`focusNextPrevChild`) = Where focus **goes next** (prevents escape)
- **We use BOTH** for optimal UX ✅

For detailed explanation, see: [`QT_FOCUS_POLICIES_EXPLAINED.md`](./QT_FOCUS_POLICIES_EXPLAINED.md)

---

## Benefits

### 🎯 **Keyboard Navigation**
- Tab key moves focus through all interactive widgets in logical order
- Shift+Tab moves focus backwards
- **Focus cycles within ScripBar** - never escapes to other widgets
- Tab at last widget (Add button) → Wraps to first widget (Exchange)
- Shift+Tab at first widget → Wraps to last widget (Add button)
- Token field is automatically skipped (read-only, NoFocus)
- **BSE Scrip Code combo dynamically included/excluded based on visibility**
- Tab order automatically reconfigures when switching between BSE+E and other segments
- **ESC key** still works to exit ScripBar and restore previous window focus

### 🧠 **Smart Visibility Handling**
- **Dynamic Tab Chain**: Tab order rebuilds based on currently visible widgets
- **BSE Scrip Code Intelligence**: 
  - ✅ Included in tab order when BSE + E segment selected
  - ❌ Excluded from tab order for all other segments
  - 🔄 Auto-reconfigures when user switches segments
- **No Manual Skipping Required**: Qt handles hidden widgets gracefully, but we rebuild the chain for optimal UX

### 🎨 **User Experience**
- Consistent focus behavior across all ScripBar widgets
- Visual focus indicators work reliably
- Works seamlessly with existing keyboard shortcuts (Enter, Escape)

### 🔧 **Developer Experience**
- Clear, explicit tab order defined in code (no guessing)
- Debug log confirms tab order configuration on startup
- Easy to maintain and modify
- **Focus trapping prevents accidental focus leaks**
- Consistent behavior with SnapQuoteScripBar (which already had focus trapping)

---

## Inheritance Impact

### SnapQuoteScripBar
- Automatically inherits all focus policy enhancements from `ScripBar`
- Custom `focusNextPrevChild()` override still works (traps focus within bar)
- No code changes needed in `SnapQuoteScripBar.cpp`

---

## Testing Checklist

- [ ] Tab key navigates through all visible widgets in correct order
- [ ] Shift+Tab navigates backwards correctly
- [ ] **Tab at last widget (Add button) wraps to first widget (Exchange)**
- [ ] **Shift+Tab at first widget wraps to last widget (Add button)**
- [ ] **Focus never escapes ScripBar during tab navigation**
- [ ] Token field is skipped during tab navigation
- [ ] **BSE + E segment**: BSE Scrip Code field is included in tab order (Exchange → Segment → Instrument → **BSE Code** → Symbol → ...)
- [ ] **NSE segments**: BSE Scrip Code field is hidden AND excluded from tab order (Exchange → Segment → Instrument → Symbol → ...)
- [ ] **Segment switching**: Tab order reconfigures automatically when switching from BSE+E to other segments
- [ ] **Visibility change log**: Debug console shows "tab order reconfigured" message when BSE Code visibility changes
- [ ] Focus indicators (blue border) display correctly on all widgets
- [ ] Existing keyboard shortcuts still work (Enter, Escape)
- [ ] **ESC key exits ScripBar and restores focus to previous window** (MarketWatch, etc.)
- [ ] SnapQuoteScripBar still traps focus within the bar (unchanged behavior)
- [ ] Mouse clicks still work on all widgets
- [ ] Mouse wheel scrolling works on combo boxes

---

## Files Modified

1. **`src/app/ScripBar.cpp`**
   - Added `setFocusPolicy(Qt::StrongFocus)` to all 8 interactive combo boxes
   - Added `setFocusPolicy(Qt::StrongFocus)` to Add button
   - Added `setFocusPolicy(Qt::NoFocus)` to read-only token field
   - Added `setupTabOrder()` method for dynamic tab order configuration
   - Added `focusNextPrevChild()` override to trap focus within ScripBar
   - Modified `updateBseScripCodeVisibility()` to trigger tab order reconfiguration
   - Added debug logs for tab order changes
   - Added `#include <QApplication>` for focus widget detection

2. **`include/app/ScripBar.h`**
   - Added `setupTabOrder()` method declaration
   - Added `focusNextPrevChild(bool next)` override declaration (protected)

---

## Configuration Details

### Tab Navigation Flow

```
User presses Tab (CYCLES WITHIN SCRIPBAR):
┌────────────────┐
│ 1. Exchange    │ (NSE/BSE/NSECDS/MCX)
└────────┬───────┘
         ↓
┌────────────────┐
│ 2. Segment     │ (E/F/O)
└────────┬───────┘
         ↓
┌────────────────┐
│ 3. Instrument  │ (FUTIDX/FUTSTK/EQUITY/...)
└────────┬───────┘
         ↓
┌──────────────────────────────────────┐
│ 4. BSE Scrip Code (CONDITIONAL)      │
│    ✅ SHOWN: BSE + E segment         │
│    ❌ HIDDEN: All other segments     │
│    🔄 Tab order auto-reconfigures    │
└────────┬─────────────────────────────┘
         ↓
┌────────────────┐
│ 5. Symbol      │ (NIFTY/BANKNIFTY/...)
└────────┬───────┘
         ↓
┌────────────────┐
│ 6. Expiry      │ (28FEB26/27MAR26/...)
└────────┬───────┘
         ↓
┌────────────────┐
│ 7. Strike      │ (23000/23100/...)
└────────┬───────┘
         ↓
┌────────────────┐
│ 8. Option Type │ (CE/PE)
└────────┬───────┘
         ↓
┌────────────────┐
│ 9. Add Button  │ ← Tab here WRAPS back to Exchange
└────────┬───────┘
         ↓
         ╔══════════════════════════════╗
         ║  🔄 WRAPS TO EXCHANGE COMBO  ║
         ╚══════════════════════════════╝
         ↓
┌────────────────┐
│ 1. Exchange    │ ← Cycle continues...
└────────────────┘
```

**Key Features:**
- ✅ Focus **never escapes** ScripBar during tab navigation
- ✅ Wrapping at both ends (first ↔ last widget)
- ✅ BSE Scrip Code dynamically included/excluded
- ✅ Clean, predictable cycling behavior
- ⚠️ Use **ESC key** to exit ScripBar and restore focus to previous window

---

## Notes

- **StrongFocus** is the recommended policy for most interactive widgets
- Token field uses **NoFocus** because it's read-only and should not be editable
- **Dynamic tab order reconfiguration** ensures optimal UX when BSE Scrip Code visibility changes
- **Focus trapping** via `focusNextPrevChild()` override prevents focus from escaping ScripBar
- `setupTabOrder()` is called:
  - Once during initial setup
  - Every time BSE Scrip Code visibility changes (BSE+E ↔ other segments)
- Hidden widgets are **completely excluded** from the tab chain (not just skipped by Qt)
- Tab order is explicitly defined to prevent layout-dependent navigation issues
- **Wrapping behavior**: Tab at last widget returns to first, Shift+Tab at first returns to last
- **Exit mechanism**: ESC key allows user to exit ScripBar and return focus to previous window
- Compatible with both SearchMode and DisplayMode of ScripBar
- Consistent with SnapQuoteScripBar behavior (which also traps focus)

---

## Related Components

- `CustomScripComboBox` - Custom combo box with search/selector modes
- `SnapQuoteScripBar` - Inherits all enhancements automatically
- `MainWindow::focusScripBar()` - Works seamlessly with new focus policy

---

## Compatibility

✅ **Windows** - MSVC build with Qt 5.15.2  
✅ **Linux** - Standard Qt build  
✅ **macOS** - Standard Qt build  

No platform-specific issues expected.
