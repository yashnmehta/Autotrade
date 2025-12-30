---
description: MDI Sub-Window Management Improvements
---

# MDI Sub-Window Management Enhancement Plan

## Issues to Address

### 1. **Smooth Window Resizing**
- Problem: Initial resize requires extra effort
- Solution: Improve resize handles and grip areas
- Implementation: Custom resize borders with larger hit areas

### 2. **Window Grouping**
- Problem: No way to group related windows
- Solution: Implement tabbed window groups
- Implementation: QTabWidget integration for grouped windows

### 3. **Window Switching (Ctrl+Tab)**
- Problem: No keyboard shortcut for window switching
- Solution: Implement Ctrl+Tab cycling
- Implementation: QShortcut + window activation logic

### 4. **Visible Thin Borders**
- Problem: Window borders are hard to see
- Solution: Add prominent, styled borders
- Implementation: Custom QMdiSubWindow stylesheet

### 5. **Focus Policy**
- Problem: Unclear which window has focus
- Solution: Visual focus indicators
- Implementation: Active window highlighting

### 6. **Save/Load Market Watch**
- Problem: No persistence for watchlists
- Solution: Save to JSON/INI file
- Implementation: Serialize symbol lists

### 7. **Save/Load Portfolio**
- Problem: No persistence for portfolio views
- Solution: Save window layouts and data
- Implementation: QSettings for layout persistence

---

## Implementation Tasks

### Task 1: Improve Resize Handles
**File**: `src/app/MainWindow/Windows.cpp`
**Changes**:
- Increase resize grip area from 4px to 8px
- Add visual resize cursors
- Implement smooth resize with minimum size constraints

### Task 2: Add Window Borders
**File**: `src/app/MainWindow/UI.cpp` or new stylesheet
**Changes**:
- Create custom QMdiSubWindow stylesheet
- Add 2px solid border for inactive windows
- Add 3px highlighted border for active window
- Use contrasting colors (e.g., #3498db for active)

### Task 3: Implement Ctrl+Tab Switching
**File**: `src/app/MainWindow/MainWindow.cpp`
**Changes**:
- Add QShortcut for Ctrl+Tab
- Implement window cycling logic
- Show window switcher overlay (optional)

### Task 4: Window Grouping
**File**: New `src/app/WindowGroup.cpp`
**Changes**:
- Create WindowGroup class
- Implement tab-based grouping
- Add context menu: "Group with..." option

### Task 5: Focus Policy
**File**: `src/app/MainWindow/MainWindow.cpp`
**Changes**:
- Connect to subWindowActivated signal
- Update active window styling
- Add focus indicator (title bar highlight)

### Task 6: Save/Load Market Watch
**File**: `src/app/windows/MarketWatchWindow.cpp`
**Changes**:
- Implement `saveToFile()` method
- Implement `loadFromFile()` method
- Use JSON format for symbol lists
- Add "Save Watchlist" / "Load Watchlist" menu items

### Task 7: Save/Load Portfolio
**File**: `src/app/MainWindow/MainWindow.cpp`
**Changes**:
- Implement `saveLayout()` method using QSettings
- Implement `restoreLayout()` method
- Save: window positions, sizes, states
- Auto-save on close, auto-restore on startup

---

## Priority Order

1. **High Priority** (UX Critical):
   - ✅ Visible thin borders
   - ✅ Smooth resizing
   - ✅ Focus policy

2. **Medium Priority** (Productivity):
   - ✅ Ctrl+Tab switching
   - ✅ Save/Load Market Watch
   - ✅ Save/Load Portfolio

3. **Low Priority** (Nice to Have):
   - ✅ Window grouping

---

## Testing Checklist

- [ ] Resize windows smoothly without effort
- [ ] Borders are clearly visible
- [ ] Active window is visually distinct
- [ ] Ctrl+Tab cycles through windows
- [ ] Market watch saves and loads correctly
- [ ] Portfolio layout persists across sessions
- [ ] Window grouping works as expected

---

## Files to Modify

1. `src/app/MainWindow/MainWindow.cpp` - Main window logic
2. `src/app/MainWindow/UI.cpp` - UI styling
3. `src/app/MainWindow/Windows.cpp` - Window management
4. `src/app/windows/MarketWatchWindow.cpp` - Market watch persistence
5. `resources/styles/mdi_windows.qss` - New stylesheet (create)

---

## Estimated Time

- Borders & Resizing: 1 hour
- Focus Policy: 30 minutes  
- Ctrl+Tab: 1 hour
- Save/Load: 2 hours
- Window Grouping: 3 hours
- **Total**: ~7.5 hours
