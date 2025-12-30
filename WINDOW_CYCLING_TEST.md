# Window Cycling - Testing Guide
## Ctrl+Tab / Ctrl+Shift+Tab

---

## ✅ Build Status
**Status**: Compiled successfully
**Date**: 2025-12-30 02:17 IST

---

## 🎯 How to Test

### 1. Run the Application
```bash
cd /Users/yashmehta/Desktop/go_proj/trading_terminal_cpp/build
./TradingTerminal.app/Contents/MacOS/TradingTerminal
```

### 2. Create Multiple Windows
Open several windows using these shortcuts:
- **F4**: Market Watch
- **F3**: Order Book  
- **F8**: Trade Book
- **F1**: Buy Order
- **F2**: Sell Order

### 3. Test Window Cycling

**Forward Cycling (Ctrl+Tab)**:
1. Press `Ctrl+Tab`
2. Window should switch to the next window
3. Press again to continue cycling
4. Should wrap around (last → first)

**Backward Cycling (Ctrl+Shift+Tab)**:
1. Press `Ctrl+Shift+Tab`
2. Window should switch to the previous window
3. Press again to continue cycling backward
4. Should wrap around (first → last)

---

## 🔍 Expected Log Output

When you press Ctrl+Tab, you should see:
```
[GlobalShortcuts] Window cycling shortcuts registered (Ctrl+Tab / Ctrl+Shift+Tab)
[MainWindow] ⌨️ Ctrl+Tab pressed - cycling windows forward
[MainWindow] Total windows: 3
[MainWindow] ✅ Cycled forward: "Market Watch 1" → "Order Book"
```

When you press Ctrl+Shift+Tab, you should see:
```
[MainWindow] ⌨️ Ctrl+Shift+Tab pressed - cycling windows backward
[MainWindow] Total windows: 3
[MainWindow] ✅ Cycled backward: "Order Book" → "Market Watch 1"
```

---

## 🐛 Troubleshooting

### If shortcuts don't work:

1. **Check logs** for the registration message:
   ```
   [GlobalShortcuts] Window cycling shortcuts registered
   ```

2. **macOS System Shortcuts Conflict**:
   - Go to System Preferences → Keyboard → Shortcuts
   - Check if Ctrl+Tab is used by system
   - Disable conflicting shortcuts

3. **Qt Focus Issues**:
   - Make sure a window has focus
   - Click on a window first, then try Ctrl+Tab

4. **Alternative Test**:
   - Try with just 2 windows first
   - Check if debug messages appear in logs

---

## 📊 Test Checklist

- [ ] Shortcut registration message appears in logs
- [ ] Ctrl+Tab cycles forward through windows
- [ ] Ctrl+Shift+Tab cycles backward through windows
- [ ] Cycling wraps around correctly
- [ ] Active window border changes (blue highlight)
- [ ] Works with 1 window (no crash)
- [ ] Works with 5+ windows
- [ ] Debug messages show correct window titles

---

## 🔧 Implementation Details

**Files Modified**:
- `src/core/GlobalShortcuts.cpp` - Shortcut registration
- `src/app/MainWindow/WindowCycling.cpp` - Cycling logic
- `include/app/MainWindow.h` - Method declarations

**Key Features**:
- Uses `Qt::ApplicationShortcut` context
- Explicit key codes (`Qt::CTRL | Qt::Key_Tab`)
- SLOT() macro for private slots
- Verbose debug logging
- Wrap-around support

---

## 📝 Notes

- If Ctrl+Tab doesn't work, it may be captured by macOS
- Check System Preferences → Keyboard → Shortcuts
- Alternative: Use F11/F12 if needed (can be configured)
- Logs are saved to `logs/trading_terminal_*.log`

---

*Ready to test! 🚀*
