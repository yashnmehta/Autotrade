# Quick Reference - Market Watch Features

## 🚀 Launch Application
```bash
cd /Users/yashmehta/Desktop/go_proj/trading_terminal_cpp/build
open TradingTerminal.app
```

## ✅ What Works Now

### 1. Sorting (Click Column Headers)
- Click any column header → Sort ascending
- Click again → Sort descending
- Works for: Symbol, LTP, Change, %Change, Volume, Bid, Ask, High, Low, Open, OI

### 2. Row Drag & Drop (Direct on Cells)
- Select rows
- Click and hold on **cell data** (not row header)
- Drag to new position
- Release
- **Note**: Disabled during sort (clear sort first)

### 3. Clipboard at Position
- **Ctrl+C** - Copy selected rows
- **Ctrl+X** - Cut selected rows
- **Ctrl+V** - Paste after current row (NOT at bottom!)
- Works between different Market Watch windows

## 🎯 Quick Test

1. **Create Market Watch**: Press `Ctrl+M`
2. **Test Sort**: Click "LTP" header twice
3. **Test Drag**: Select row, drag to new position
4. **Test Paste**: 
   - Select row 2
   - Cut row 4 (`Ctrl+X`)
   - Paste (`Ctrl+V`)
   - Row 4 appears after row 2 ✅

## ⌨️ Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **Ctrl+M** | Create Market Watch |
| **Ctrl+A** | Select All |
| **Ctrl+C** | Copy |
| **Ctrl+X** | Cut |
| **Ctrl+V** | Paste (at cursor!) |
| **Delete** | Delete selected rows |
| **F1** | Buy selected scrip |
| **F2** | Sell selected scrip |

## 🖱️ Mouse Actions

| Action | Result |
|--------|--------|
| Click column header | Sort by column |
| Right-click row | Context menu |
| Click + drag cell | Move row(s) |
| Double-click header edge | Auto-resize column |

## 📋 Context Menu

Right-click any row:
- Buy (F1)
- Sell (F2)
- Add Scrip
- Insert Blank Row Above/Below
- Delete (Del)
- Copy (Ctrl+C)
- Cut (Ctrl+X)
- Paste (Ctrl+V)

## ⚠️ Important Notes

### Drag & Drop
- ✅ **DO**: Click on cell data to drag
- ❌ **DON'T**: Click on row header
- ⚠️ **NOTE**: Must clear sort before dragging

### Paste Position
- Pastes **after selected row**
- If no row selected → pastes at bottom
- Duplicate scrips automatically skipped

### Sorting
- Numeric columns sort numerically (not alphabetically)
- String columns (Symbol) sort alphabetically
- Indicator shows sort direction (▲ ascending, ▼ descending)

## 🐛 Troubleshooting

### "Sorting not working"
- Make sure you're clicking the **header**, not a cell
- Check if data has values (0.00 sorts before other numbers)

### "Can't drag rows"
- Is sorting enabled? Click header to clear sort
- Are rows selected?
- Clicking cell data, not row header?

### "Paste goes to bottom"
- Select a row before pasting
- Paste inserts **after** selected row

### "Drag doesn't move rows"
- Try clicking and holding for 1 second before dragging
- Check console for error messages

## 📊 Debug Console

Run with console output to see debug messages:
```bash
cd /Users/yashmehta/Desktop/go_proj/trading_terminal_cpp/build
./TradingTerminal.app/Contents/MacOS/TradingTerminal
```

Look for:
```
[MarketWatchWindow] Moved 2 rows to position 1
[MarketWatchModel] Inserted scrip: RELIANCE Token: 2885 at position 3
[TokenAddressBook] Added token 2885 at row 3
```

## 📁 Documentation Files

- **Testing_Guide_Latest_Fixes.md** - Comprehensive testing instructions
- **Implementation_Complete_Summary.md** - Technical details and architecture
- **Row_DragDrop_Implementation_Guide.md** - Drag & drop deep dive
- **UI_Customization_Implementation_Guide.md** - Future customization features

## ✨ Features Summary

| Feature | Status | Notes |
|---------|--------|-------|
| Sorting | ✅ WORKING | Click headers |
| Row Drag & Drop | ✅ WORKING | Direct on cells |
| Paste at Position | ✅ WORKING | After current row |
| Multi-row Operations | ✅ WORKING | Select with Ctrl+Click |
| Duplicate Detection | ✅ WORKING | Auto-skip duplicates |
| Token Subscriptions | ✅ WORKING | Auto-managed |
| Inter-window Ops | ✅ WORKING | Cut/paste between windows |

## 🎉 Success!

All requested features are now working:
1. ✅ Sorting (not working) → **FIXED**
2. ✅ Row drag & drop (without row header) → **IMPLEMENTED**
3. ✅ Ctrl+V paste position (was bottom) → **FIXED**

**Build Status**: ✅ 0 errors, 0 warnings
**Ready for Production**: Yes! 🚀
