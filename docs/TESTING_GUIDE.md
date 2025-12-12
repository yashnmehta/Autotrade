# Testing Guide - Trading Terminal C++

## 🎯 Purpose
Systematic testing of all features after critical crash fixes to ensure stability and functionality.

---

## ✅ Phase 1: Basic Window Functionality (COMPLETE)

### 1.1 Application Launch
- [x] Application starts without crash
- [x] No segmentation faults
- [x] Window appears on screen
- [x] Title bar visible
- [x] Menu bar visible

### 1.2 Visual Layout Verification
**Expected Order (Top to Bottom):**
1. Custom Title Bar (with min/max/close buttons)
2. Menu Bar (File, Edit, View, Window, Help)
3. Main Toolbar
4. Connection Toolbar
5. Scrip Toolbar
6. MDI Area (main content)
7. Status Bar

**Test:**
```bash
cd /Users/yashmehta/Desktop/go_proj/trading_terminal_cpp/build
open TradingTerminal.app
```

**Visual Checklist:**
- [ ] Title bar at TOP
- [ ] Menu bar BELOW title bar
- [ ] All toolbars BELOW menu bar
- [ ] MDI area takes up most space
- [ ] Status bar at BOTTOM
- [ ] No overlapping widgets
- [ ] No gaps or spacing issues

---

## 🪟 Phase 2: Window Management

### 2.1 Window Dragging
**Test:**
1. Click and hold on title bar (empty space, not buttons)
2. Move mouse while holding
3. Window should follow cursor
4. Release to drop window

**Expected:**
- [ ] Window moves smoothly
- [ ] No jumping or stuttering
- [ ] Works from any part of title bar
- [ ] Doesn't trigger when dragging from toolbars/content

### 2.2 Window Resizing
**Test all 8 directions:**

1. **Corners (4 directions):**
   - [ ] Top-Left: Cursor changes to ↖, resize works
   - [ ] Top-Right: Cursor changes to ↗, resize works
   - [ ] Bottom-Left: Cursor changes to ↙, resize works
   - [ ] Bottom-Right: Cursor changes to ↘, resize works

2. **Edges (4 directions):**
   - [ ] Top: Cursor changes to ↕, resize works
   - [ ] Bottom: Cursor changes to ↕, resize works
   - [ ] Left: Cursor changes to ↔, resize works
   - [ ] Right: Cursor changes to ↔, resize works

3. **Constraints:**
   - [ ] Cannot resize smaller than minimum size (800x600)
   - [ ] Cannot resize larger than screen
   - [ ] Aspect ratio not locked (can make any shape)

### 2.3 Minimize Button
**Test:**
1. Click minimize button (- on title bar)

**Expected:**
- [ ] Window minimizes to dock
- [ ] Click dock icon to restore
- [ ] Window returns to previous size/position

### 2.4 Maximize Button
**Test:**
1. Click maximize button (□ on title bar)
2. Click again to restore

**Expected:**
- [ ] Window fills available screen space
- [ ] Title bar remains visible
- [ ] All content visible and usable
- [ ] Click again restores to previous size
- [ ] Double-click title bar also maximizes/restores

### 2.5 Close Button
**Test:**
1. Click close button (× on title bar)

**Expected:**
- [ ] Application closes gracefully
- [ ] No crash or error messages
- [ ] Settings saved (check on next launch)

**Settings to verify persist:**
- Info bar visibility
- Status bar visibility

---

## 📋 Phase 3: Menu Bar Functionality

### 3.1 File Menu
**Test:**
- [ ] Click "File" opens menu
- [ ] "Exit" closes application
- [ ] Keyboard shortcut works (if applicable)

### 3.2 Edit Menu
**Test:**
- [ ] Click "Edit" opens menu
- [ ] "Preferences" item exists (may not be implemented)

### 3.3 View Menu
**Test:**
- [ ] Click "View" opens menu
- [ ] "Toolbar" toggle exists
- [ ] "Status Bar" toggle exists
  - [ ] Check it off → Status bar disappears
  - [ ] Check it on → Status bar reappears
- [ ] "Info Bar" toggle exists
  - [ ] Check it off → Info bar disappears (if implemented)
  - [ ] Check it on → Info bar reappears (if implemented)
- [ ] "Fullscreen" option exists
- [ ] "Reset Layout" works

### 3.4 Window Menu
**Test:**
- [ ] Click "Window" opens menu
- [ ] "New Market Watch" option exists
  - [ ] Click creates new Market Watch window
  - [ ] Keyboard shortcut Ctrl+M works
- [ ] "New Buy Window" option exists
  - [ ] Click creates new Buy window
  - [ ] Keyboard shortcut Ctrl+B works
- [ ] "Cascade" arranges windows in cascade
- [ ] "Tile" arranges windows in grid
- [ ] "Tile Horizontally" arranges horizontally
- [ ] "Tile Vertically" arranges vertically

---

## 🛠️ Phase 4: Toolbar Functionality

### 4.1 Main Toolbar
**Visual Test:**
- [ ] Toolbar visible
- [ ] Buttons/icons visible (if any)
- [ ] Height appropriate (25px on macOS)
- [ ] Dark theme applied

**Functionality Test:**
- [ ] Click toolbar buttons (test each one)
- [ ] Tooltips appear on hover (if implemented)

### 4.2 Connection Toolbar
**Visual Test:**
- [ ] Connection bar visible
- [ ] Status indicators visible
- [ ] Height 25px on macOS

**Functionality Test:**
- [ ] Connection status updates
- [ ] Clicking elements works (if applicable)

### 4.3 Scrip Toolbar (Search Bar)
**Visual Test:**
- [ ] Scrip bar visible
- [ ] Search field visible
- [ ] Dropdown/combos visible
- [ ] Height 25px on macOS

**Functionality Test:**
- [ ] Can type in search field
- [ ] Dropdowns open/close
- [ ] Search/filter works
- [ ] "Add to Watch" triggers correctly
- [ ] Signal connects to MainWindow handler

---

## 📦 Phase 5: MDI Area & Child Windows

### 5.1 Create Market Watch Window
**Test:**
1. Window → New Market Watch (or Ctrl+M)

**Expected:**
- [ ] New child window appears in MDI area
- [ ] Window has title "Market Watch"
- [ ] Window can be moved within MDI area
- [ ] Window can be resized
- [ ] Window has minimize/maximize/close buttons
- [ ] Multiple Market Watch windows can coexist

### 5.2 Create Buy Window
**Test:**
1. Window → New Buy Window (or Ctrl+B)

**Expected:**
- [ ] New child window appears
- [ ] Window has title "Buy Window"
- [ ] Window is functional
- [ ] Can create multiple Buy windows

### 5.3 Window Arrangement
**Test:**
1. Create 3-4 child windows
2. Try each arrangement:

**Cascade:**
- [ ] Windows overlap in staggered pattern
- [ ] Title bars all visible
- [ ] Can click to bring to front

**Tile:**
- [ ] Windows arranged in grid
- [ ] No overlaps
- [ ] Each window gets equal space
- [ ] All windows fully visible

**Tile Horizontally:**
- [ ] Windows stacked vertically
- [ ] Full width for each
- [ ] Equal height distribution

**Tile Vertically:**
- [ ] Windows side-by-side
- [ ] Full height for each
- [ ] Equal width distribution

### 5.4 Child Window Interactions
**Test:**
- [ ] Click to activate/focus window
- [ ] Drag window around MDI area
- [ ] Resize child window
- [ ] Minimize child window
- [ ] Maximize child window (fills MDI area)
- [ ] Close child window
- [ ] All actions work smoothly

---

## 📊 Phase 6: Status Bar

### 6.1 Visual
**Test:**
- [ ] Status bar visible at bottom
- [ ] Dark theme applied
- [ ] Proper height/padding

### 6.2 Functionality
**Test:**
- [ ] Status messages appear (if implemented)
- [ ] Toggle via View → Status Bar works
- [ ] Preference persists on restart

---

## 🎨 Phase 7: Visual & Theme

### 7.1 Color Scheme
**Check all elements:**
- [ ] Title bar: Dark background
- [ ] Menu bar: Dark with proper contrast
- [ ] Toolbars: Consistent dark theme
- [ ] MDI area: Dark background
- [ ] Status bar: Dark theme
- [ ] Child windows: Proper styling

### 7.2 Hover Effects
**Test:**
- [ ] Menu items highlight on hover
- [ ] Toolbar buttons highlight on hover
- [ ] Title bar buttons change color on hover
- [ ] Close button turns red on hover

### 7.3 Focus Indicators
**Test:**
- [ ] Active child window has highlight/indicator
- [ ] Menu selection visible
- [ ] Focus follows interactions

---

## 🔄 Phase 8: State Persistence

### 8.1 Save on Close
**Test:**
1. Launch app
2. Change settings:
   - Toggle status bar off
   - Toggle info bar off
3. Close app
4. Relaunch app

**Expected:**
- [ ] Status bar remains off
- [ ] Info bar remains off
- [ ] Settings persisted correctly

### 8.2 Window Geometry
**Test:**
1. Resize window to custom size
2. Move window to different position
3. Close and relaunch

**Expected (if implemented):**
- [ ] Window reopens at same position
- [ ] Window reopens at same size

---

## 🚨 Phase 9: Error Handling

### 9.1 Stress Testing
**Test:**
- [ ] Create 10+ child windows rapidly
- [ ] Resize window to very small size
- [ ] Maximize/restore repeatedly (10x fast)
- [ ] Open all menus rapidly
- [ ] Click all buttons rapidly

**Expected:**
- No crashes
- No hangs/freezes
- No memory leaks (check Activity Monitor)

### 9.2 Edge Cases
**Test:**
- [ ] Close app while child window is maximized
- [ ] Close app while dragging window
- [ ] Close app while menu is open
- [ ] Minimize then close from dock

---

## 🖥️ Phase 10: Cross-Platform Testing

### 10.1 macOS (Current)
**Platform:** macOS 13+ (Intel & Apple Silicon)

**Test:**
- [x] Runs on Apple Silicon
- [ ] Runs on Intel Mac
- [ ] Native look and feel
- [ ] Keyboard shortcuts work
- [ ] Cmd+M, Cmd+B shortcuts
- [ ] Window controls work
- [ ] Retina display support

### 10.2 Linux
**Platform:** Ubuntu 22.04, Fedora 38+

**Build:**
```bash
cd trading_terminal_cpp
mkdir build && cd build
cmake ..
make -j4
./TradingTerminal
```

**Test:**
- [ ] Compiles successfully
- [ ] Runs without crash
- [ ] Frameless window works with X11/Wayland
- [ ] Title bar renders correctly
- [ ] Window resize works
- [ ] All menus functional
- [ ] Keyboard shortcuts (Ctrl+M, Ctrl+B)

### 10.3 Windows
**Platform:** Windows 10, Windows 11

**Build:**
```cmd
cd trading_terminal_cpp
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
TradingTerminal.exe
```

**Test:**
- [ ] Compiles with MinGW
- [ ] Compiles with MSVC
- [ ] Runs without crash
- [ ] Frameless window works
- [ ] Window controls work
- [ ] Aero snap works (Win+Arrow keys)
- [ ] All functionality works

---

## 📝 Test Execution Log

### Test Session 1 - December 12, 2025

**Tester:** Initial validation  
**Platform:** macOS (Apple Silicon)  
**Build:** Fresh compilation  

**Results:**
- ✅ Phase 1.1: Application launches successfully
- ✅ Phase 1.2: No crashes on startup
- ✅ Title bar visible at correct position
- ✅ Menu bar visible below title bar
- ✅ Application running stable for 3+ minutes

**Issues Found:**
- None (in initial basic test)

**Next Steps:**
- Complete Phase 2 (Window Management)
- Test all menu items
- Test child window creation

---

## 🐛 Known Issues Tracking

### Issue #1: [Title]
**Severity:** [Critical/High/Medium/Low]  
**Phase:** [Which test phase]  
**Description:**  
**Steps to Reproduce:**  
**Expected:**  
**Actual:**  
**Status:** [Open/In Progress/Fixed]

---

## 📋 Testing Checklist Summary

**Total Test Items:** ~100+  
**Completed:** ~10  
**Remaining:** ~90  
**Critical Failures:** 0  

**Recommendation:** Continue systematic testing through all phases. Priority on Phase 2-4 for core functionality verification.
