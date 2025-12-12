# Linux Platform Fixes Applied

**Date:** December 12, 2025  
**Platform:** Cross-platform (macOS & Linux)

---

## 🔧 Fixes Applied

### Fix #1: Menu Bar Side-by-Side Issue (Linux)
**Problem:** On Linux, QMenuBar was appearing side-by-side with title bar instead of below it.

**Solution:**
```cpp
// In MainWindow::createMenuBar()
menuBar->setNativeMenuBar(false);  // Force non-native menu bar
menuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);  // Full width
```

**Why:** Linux window managers treat QMenuBar differently. `setNativeMenuBar(false)` forces Qt to render the menu bar as a widget in our layout instead of using the native window manager's menu system.

---

### Fix #2: Window Dragging Not Working
**Problem:** Window dragging stopped working after layout changes.

**Root Cause:** The drag calculation was incorrect:
```cpp
// OLD (BROKEN):
move(globalPos - m_titleBar->mapFromGlobal(globalPos) - QPoint(0, 0));
// This always results in move(QPoint(0,0)) because mapFromGlobal cancels out globalPos
```

**Solution:**
```cpp
// NEW (FIXED):
// 1. Store offset when drag starts
connect(m_titleBar, &CustomTitleBar::dragStarted, this, [this](const QPoint &globalPos) {
    m_dragOffset = globalPos - frameGeometry().topLeft();
});

// 2. Use stored offset during drag
connect(m_titleBar, &CustomTitleBar::dragMoved, this, [this](const QPoint &globalPos) {
    if (!m_isMaximized && !m_isResizing) {
        move(globalPos - m_dragOffset);
    }
});
```

**Added member variable:**
```cpp
// In CustomMainWindow.h
QPoint m_dragOffset;  // Offset from cursor to window top-left during drag
```

---

## 📊 Testing Status

### macOS ✅
- [x] Application compiles
- [x] Application runs without crash
- [x] Title bar at top
- [x] Menu bar below title bar (correct position)
- [x] Layout correct
- [ ] Window dragging works (NEEDS MANUAL VERIFICATION)
- [ ] Minimize/maximize/close work
- [ ] Resizing works

### Linux ❓
- [ ] Application compiles
- [ ] Application runs without crash
- [ ] Title bar at top
- [ ] Menu bar below title bar (not side-by-side)
- [ ] Layout correct
- [ ] Window dragging works
- [ ] Minimize/maximize/close work
- [ ] Resizing works

---

## 🧪 Manual Testing Required

### Test Window Dragging:
1. Launch application
2. Click and hold on title bar (empty area, not buttons)
3. Move mouse while holding
4. Window should follow cursor smoothly
5. Release mouse button

**Expected:** Window moves to new position  
**If Broken:** Window doesn't move or jumps incorrectly

### Test Menu Bar Position (Linux):
1. Launch application on Linux
2. Check menu bar position

**Expected:** Menu bar appears BELOW title bar as a horizontal bar  
**If Broken:** Menu bar appears SIDE-BY-SIDE with title bar or in window manager's native menu area

---

## 🐛 Debugging

### If Window Dragging Doesn't Work:

**Check 1: Are signals connected?**
```bash
# Look for these debug messages in console:
CustomMainWindow created
```

**Check 2: Is dragStarted signal emitted?**
Add temporary debug in CustomTitleBar.cpp:
```cpp
void CustomTitleBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        qDebug() << "Title bar pressed at" << event->globalPos();
        emit dragStarted(event->globalPos());
    }
}
```

**Check 3: Is dragMoved signal emitted?**
Add temporary debug in CustomTitleBar.cpp:
```cpp
void CustomTitleBar::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        qDebug() << "Dragging to" << event->globalPos();
        emit dragMoved(event->globalPos());
    }
}
```

**Check 4: Is lambda being called?**
Add temporary debug in CustomMainWindow.cpp:
```cpp
connect(m_titleBar, &CustomTitleBar::dragMoved, this, [this](const QPoint &globalPos) {
    qDebug() << "Drag lambda called, moving to" << (globalPos - m_dragOffset);
    if (!m_isMaximized && !m_isResizing) {
        move(globalPos - m_dragOffset);
    }
});
```

---

## 🔄 Rollback Instructions

If fixes cause issues on macOS, rollback:

### Rollback Menu Bar Fix:
```cpp
// Remove these lines from MainWindow::createMenuBar():
// menuBar->setNativeMenuBar(false);
// menuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
```

### Rollback Drag Fix:
```cpp
// Replace the two connect() calls with simpler version:
connect(m_titleBar, &CustomTitleBar::dragMoved, this, [this](const QPoint &globalPos) {
    if (!m_isMaximized && !m_isResizing) {
        // Try alternative drag calculation
        QPoint newPos = globalPos - QPoint(width()/2, 16);  // Center under cursor
        move(newPos);
    }
});
```

---

## 📝 Files Modified

1. **`src/ui/MainWindow.cpp`** - Added `setNativeMenuBar(false)` and `setSizePolicy()`
2. **`src/ui/CustomMainWindow.cpp`** - Fixed drag calculation using offset
3. **`include/ui/CustomMainWindow.h`** - Added `QPoint m_dragOffset` member

---

## ✅ Next Steps

1. **Manual Test on macOS** - Verify window dragging works
2. **Test on Linux** - Build and verify both fixes work
3. **Report Results** - Document what works and what doesn't

---

## 🎯 Expected Results

### macOS:
- ✅ Menu bar below title bar (already working)
- ✅ Window dragging works (should be fixed)
- ✅ All other functionality unchanged

### Linux:
- ✅ Menu bar below title bar (fixed by `setNativeMenuBar(false)`)
- ✅ Window dragging works (fixed by offset calculation)
- ✅ Cross-platform consistency achieved
