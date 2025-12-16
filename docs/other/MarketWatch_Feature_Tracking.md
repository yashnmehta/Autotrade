# Custom Market Watch Window - Feature Specification & Tracking

## Overview
A professional-grade Market Watch window using QTableView with real-time market data display, extensive customization, and trader-friendly features.

## Status Legend
- ⏳ **Pending** - Not started
- 🚧 **In Progress** - Currently being implemented
- ✅ **Complete** - Implemented and tested
- 🔄 **Testing** - Implementation done, testing in progress
- ❌ **Blocked** - Blocked by dependencies

---

## Core Features

### 1. Data Display (QTableView)
| Feature | Priority | Status | Notes |
|---------|----------|--------|-------|
| QTableView base implementation | High | ⏳ | Custom model with QAbstractItemModel |
| Custom delegate for cell rendering | High | ⏳ | Color-coded price changes, custom formatting |
| Multiple columns (Symbol, LTP, Change, %Change, Volume, etc.) | High | ⏳ | Configurable column set |
| Real-time data updates | High | ⏳ | Efficient update mechanism |
| Flash animation on value change | Medium | ⏳ | Green flash (up), Red flash (down) |
| Thousands separator formatting | Medium | ⏳ | 1,00,000 vs 100000 |
| Decimal precision control | Medium | ⏳ | Per-column precision settings |

### 2. Sorting & Filtering
| Feature | Priority | Status | Notes |
|---------|----------|--------|-------|
| Column header click to sort | High | ⏳ | Ascending/Descending toggle |
| Multi-column sorting | Medium | ⏳ | Shift+Click for secondary sort |
| Sort indicator in header | High | ⏳ | Arrow showing sort direction |
| Persist sort preferences | Low | ⏳ | Remember last sort column |
| Quick filter by text | Medium | ⏳ | Filter scrips by symbol name |

### 3. Column Management
| Feature | Priority | Status | Notes |
|---------|----------|--------|-------|
| Column drag & drop reordering | High | ⏳ | Visual feedback during drag |
| Column resize | High | ⏳ | Manual resize with double-click auto-fit |
| Column visibility toggle | High | ⏳ | Show/hide columns from menu |
| Save column configuration | High | ⏳ | Per-market watch settings |
| Restore default layout | Medium | ⏳ | Reset to factory columns |
| Column width auto-fit | Medium | ⏳ | Fit to content |
| Freeze first column (Symbol) | Low | ⏳ | Keep symbol visible while scrolling |

### 4. Row Management
| Feature | Priority | Status | Notes |
|---------|----------|--------|-------|
| Add scrip from ScripBar | High | ⏳ | Integration with ScripBar |
| Delete selected rows | High | ⏳ | Delete key or context menu |
| Row drag & drop reordering | High | ⏳ | Custom drag indicator |
| Duplicate scrip prevention | High | ⏳ | Check before adding |
| Row height customization | Medium | ⏳ | Compact/Normal/Large modes |
| Insert row at position | Medium | ⏳ | Right-click → Insert |
| Clear all rows | Medium | ⏳ | Confirmation dialog |
| Row limit warning | Low | ⏳ | Warn if >50 scrips (performance) |

### 5. Selection & Clipboard
| Feature | Priority | Status | Notes |
|---------|----------|--------|-------|
| Multi-row selection (Ctrl+Click) | High | ⏳ | QAbstractItemView::ExtendedSelection |
| Range selection (Shift+Click) | High | ⏳ | Select continuous rows |
| Select All (Ctrl+A) | High | ⏳ | Select all rows |
| Copy (Ctrl+C) | High | ⏳ | Copy to clipboard (TSV format) |
| Cut (Ctrl+X) | High | ⏳ | Copy + delete from current watch |
| Paste (Ctrl+V) | High | ⏳ | Paste from clipboard or other watch |
| Drag & drop between Market Watches | High | ⏳ | Inter-window drag support |
| Selection highlight color | Medium | ⏳ | Customizable selection color |
| Copy with headers | Medium | ⏳ | Include column names |

### 6. Context Menu
| Feature | Priority | Status | Notes |
|---------|----------|--------|-------|
| Right-click context menu | High | ⏳ | Context-aware actions |
| Add Scrip | High | ⏳ | Open ScripBar with focus |
| Delete Scrip(s) | High | ⏳ | Delete selected rows |
| Buy/Sell shortcuts | High | ⏳ | Quick order entry |
| Copy/Cut/Paste | High | ⏳ | Clipboard operations |
| Market Depth | Medium | ⏳ | Show Level 2 data |
| Chart | Medium | ⏳ | Open chart for scrip |
| Scrip Info | Medium | ⏳ | Show detailed info dialog |
| Add to Another Watch | Medium | ⏳ | Submenu with other watches |
| Column Settings | Medium | ⏳ | Open column config |
| Refresh | Low | ⏳ | Force refresh data |

### 7. Visual Customization (UI Settings)
| Feature | Priority | Status | Notes |
|---------|----------|--------|-------|
| Show/Hide grid lines | High | ⏳ | Toggle grid visibility |
| Alternate row colors | High | ⏳ | Zebra striping for readability |
| Custom font selection | High | ⏳ | Font family, size, weight |
| Background color | Medium | ⏳ | Custom BG color |
| Text color (default) | Medium | ⏳ | Custom text color |
| Positive change color | High | ⏳ | Default: Green (#00C853) |
| Negative change color | High | ⏳ | Default: Red (#FF1744) |
| Selection color | Medium | ⏳ | Highlight color |
| Header style | Medium | ⏳ | Bold, background color |
| Row height | Medium | ⏳ | Compact/Normal/Large |
| Padding | Low | ⏳ | Cell padding adjustment |
| Border style | Low | ⏳ | Border width, color |

### 8. Data Columns (Configurable)
| Column Name | Type | Description | Default Visible |
|-------------|------|-------------|-----------------|
| Symbol | String | Trading symbol | ✅ |
| LTP (Last Traded Price) | Decimal | Current price | ✅ |
| Change | Decimal | Absolute change | ✅ |
| %Change | Decimal | Percentage change | ✅ |
| Volume | Integer | Total volume traded | ✅ |
| Bid Price | Decimal | Best bid price | ❌ |
| Bid Qty | Integer | Best bid quantity | ❌ |
| Ask Price | Decimal | Best ask price | ❌ |
| Ask Qty | Integer | Best ask quantity | ❌ |
| Open | Decimal | Opening price | ❌ |
| High | Decimal | Day high | ✅ |
| Low | Decimal | Day low | ✅ |
| Close | Decimal | Previous close | ❌ |
| Total Buy Qty | Integer | Total buy quantity | ❌ |
| Total Sell Qty | Integer | Total sell quantity | ❌ |
| ATP (Average Traded Price) | Decimal | Average price | ❌ |
| OI (Open Interest) | Integer | For F&O contracts | ❌ |
| OI Change | Integer | Change in OI | ❌ |
| 52W High | Decimal | 52-week high | ❌ |
| 52W Low | Decimal | 52-week low | ❌ |

### 9. Keyboard Shortcuts
| Shortcut | Action | Priority | Status |
|----------|--------|----------|--------|
| **Ctrl+A** | Select All | High | ⏳ |
| **Ctrl+C** | Copy | High | ⏳ |
| **Ctrl+X** | Cut | High | ⏳ |
| **Ctrl+V** | Paste | High | ⏳ |
| **Delete** | Delete selected rows | High | ⏳ |
| **Ctrl+F** | Find/Filter scrip | Medium | ⏳ |
| **F5** | Refresh data | Medium | ⏳ |
| **Ctrl+N** | Add new scrip (focus ScripBar) | Medium | ⏳ |
| **Ctrl+D** | Duplicate scrip | Low | ⏳ |
| **Ctrl+↑/↓** | Move row up/down | Low | ⏳ |
| **F1** | Buy selected scrip | Medium | ⏳ |
| **F2** | Sell selected scrip | Medium | ⏳ |
| **F3** | Market Depth | Low | ⏳ |
| **Space** | Toggle selection | Low | ⏳ |

### 10. Persistence & State
| Feature | Priority | Status | Notes |
|---------|----------|--------|-------|
| Save scrip list to file | High | ⏳ | JSON or CSV format |
| Load scrip list from file | High | ⏳ | Restore saved watch |
| Auto-save on change | High | ⏳ | Background save |
| Save column configuration | High | ⏳ | Width, order, visibility |
| Save visual preferences | Medium | ⏳ | Colors, fonts, grid |
| Multiple watch templates | Medium | ⏳ | Predefined watch lists |
| Export to CSV | Medium | ⏳ | Export current data |
| Import from CSV | Medium | ⏳ | Bulk scrip import |

### 11. Performance Optimization
| Feature | Priority | Status | Notes |
|---------|----------|--------|-------|
| Lazy loading for large lists | High | ⏳ | Load visible rows only |
| Efficient data updates | High | ⏳ | Update changed cells only |
| Debounced updates | High | ⏳ | Batch rapid updates |
| Viewport-based rendering | Medium | ⏳ | Render visible area only |
| Thread-safe data updates | High | ⏳ | Avoid UI freezing |
| Memory profiling | Low | ⏳ | Monitor memory usage |

### 12. Advanced Features
| Feature | Priority | Status | Notes |
|---------|----------|--------|-------|
| Alerts on price conditions | Medium | ⏳ | Popup/sound on price target |
| Scrip grouping | Low | ⏳ | Group by sector/category |
| Color rules (conditional formatting) | Medium | ⏳ | Custom color based on conditions |
| Calculated columns | Low | ⏳ | Custom formulas |
| Historical comparison | Low | ⏳ | Compare with previous day |
| Watch list sharing | Low | ⏳ | Export/import watch configs |
| Scrip notes | Low | ⏳ | Add notes per scrip |
| Star/favorite scrips | Low | ⏳ | Mark important scrips |
| Search highlighting | Medium | ⏳ | Highlight matching scrips |

---

## Technical Architecture

### Class Structure

```cpp
// Main Market Watch Window
class MarketWatchWindow : public QWidget
{
    - QTableView *m_tableView
    - MarketWatchModel *m_model
    - MarketWatchDelegate *m_delegate
    - QMenu *m_contextMenu
    - MarketWatchSettings *m_settings
    
    + addScrip(scripInfo)
    + removeScrip(symbol)
    + updateScrip(symbol, data)
    + saveWatchList()
    + loadWatchList()
}

// Custom Model
class MarketWatchModel : public QAbstractTableModel
{
    - QList<ScripData> m_scrips
    - QStringList m_columns
    
    + rowCount()
    + columnCount()
    + data()
    + setData()
    + headerData()
    + updateScripData()
}

// Custom Delegate
class MarketWatchDelegate : public QStyledItemDelegate
{
    - QColor m_positiveColor
    - QColor m_negativeColor
    
    + paint()
    + createEditor()
    + flashCell()
}

// Data Structure
struct ScripData
{
    QString symbol;
    QString exchange;
    double ltp;
    double change;
    double changePercent;
    qint64 volume;
    double open, high, low, close;
    // ... more fields
}

// Settings Manager
class MarketWatchSettings
{
    - QSettings *m_settings
    
    + saveColumnConfig()
    + loadColumnConfig()
    + saveVisualSettings()
    + loadVisualSettings()
}
```

### File Structure

```
include/ui/
├── MarketWatchWindow.h
├── MarketWatchModel.h
├── MarketWatchDelegate.h
├── MarketWatchSettings.h
└── MarketWatchSettingsDialog.h

src/ui/
├── MarketWatchWindow.cpp
├── MarketWatchModel.cpp
├── MarketWatchDelegate.cpp
├── MarketWatchSettings.cpp
└── MarketWatchSettingsDialog.cpp

resources/forms/
└── MarketWatchSettings.ui (Qt Designer form)

docs/
├── MarketWatch_Implementation_Plan.md (this file)
├── MarketWatch_Technical_Guide.md
└── MarketWatch_User_Guide.md
```

---

## Implementation Phases

### Phase 1: Foundation (Week 1)
- [ ] Create MarketWatchWindow base class
- [ ] Implement MarketWatchModel with basic columns
- [ ] Setup QTableView with model
- [ ] Basic add/remove scrip functionality
- [ ] Simple data display (no updates yet)

### Phase 2: Core Features (Week 1-2)
- [ ] Column sorting (click to sort)
- [ ] Column drag & drop reordering
- [ ] Column resize
- [ ] Row selection (multi-select)
- [ ] Delete selected rows
- [ ] Basic context menu

### Phase 3: Clipboard & Drag-Drop (Week 2)
- [ ] Copy (Ctrl+C) implementation
- [ ] Cut (Ctrl+X) implementation
- [ ] Paste (Ctrl+V) implementation
- [ ] Row drag & drop reordering (intra-watch)
- [ ] Drag & drop between watches (inter-watch)

### Phase 4: Visual Customization (Week 2-3)
- [ ] MarketWatchSettings class
- [ ] MarketWatchSettingsDialog (UI form)
- [ ] Grid toggle
- [ ] Alternate row colors
- [ ] Font customization
- [ ] Color customization
- [ ] Settings persistence

### Phase 5: Advanced Features (Week 3)
- [ ] Custom delegate with color coding
- [ ] Flash animation on value change
- [ ] Column visibility toggle
- [ ] Save/Load watch list
- [ ] Export to CSV
- [ ] Import from CSV

### Phase 6: Polish & Optimization (Week 4)
- [ ] Performance optimization
- [ ] Keyboard shortcuts
- [ ] Comprehensive testing
- [ ] Documentation
- [ ] Bug fixes

---

## Additional Features to Consider

### 1. **Multi-Market Watch Tabs**
- Tabbed interface with multiple watch lists
- Quick switch between watches
- Drag scrips between tabs

### 2. **Scrip Depth (Level 2 Data)**
- Show bid/ask depth in expandable row
- 5 levels of buy/sell orders
- Toggle inline or popup

### 3. **Watch List Templates**
- Predefined lists (NIFTY 50, Bank Nifty, F&O, etc.)
- One-click load templates
- Create custom templates

### 4. **Smart Alerts**
- Price alerts (above/below target)
- Volume alerts (unusual activity)
- %Change alerts
- Visual + audio notifications

### 5. **Quick Chart Preview**
- Hover over scrip → show mini chart
- Click for full chart window
- Intraday price sparkline in row

### 6. **Scrip Details Panel**
- Split view: Watch list + details
- Show company info, fundamentals
- Recent news feed

### 7. **Batch Operations**
- Add multiple scrips at once
- Bulk delete by criteria
- Apply alerts to multiple scrips

### 8. **Market Summary Bar**
- Show index values at top
- Advance/decline ratio
- Market sentiment indicator

### 9. **Colorblind Mode**
- Alternative color schemes
- Icon indicators (▲▼) in addition to colors
- High contrast mode

### 10. **Watch List Snapshots**
- Save current state as snapshot
- Compare with previous snapshots
- Track portfolio performance

### 11. **Option Chain Integration**
- Quick access to option chain for underlying
- Right-click → View Option Chain
- Add strikes to watch from chain

### 12. **Greeks Display (For Options)**
- Delta, Gamma, Theta, Vega columns
- IV (Implied Volatility)
- Greeks change indicators

### 13. **Scrip Linking**
- Link scrips (e.g., futures + options)
- Synchronized selection
- Related scrips panel

### 14. **Keyboard-Only Mode**
- Complete navigation without mouse
- Vim-like shortcuts (optional)
- Quick command palette (Ctrl+P)

### 15. **Themes**
- Dark theme (default)
- Light theme
- High contrast
- Custom theme creator

---

## Data Update Strategy

### Real-time Updates
```cpp
// Pseudo-code for data update flow
void MarketWatchWindow::onMarketDataReceived(const ScripUpdate &update)
{
    // 1. Find scrip in model
    int row = m_model->findScripRow(update.symbol);
    if (row < 0) return;
    
    // 2. Update model data
    m_model->updateScripData(row, update);
    
    // 3. Trigger flash animation
    m_delegate->flashCell(row, LTP_COLUMN);
    
    // 4. Check alerts
    checkAlerts(update);
}
```

### Efficient Updates
- Use `dataChanged()` signal for specific cells only
- Batch updates if receiving rapid ticks (>100/sec)
- Update visible rows first (viewport priority)
- Use separate thread for data processing

---

## Testing Checklist

### Functional Tests
- [ ] Add scrip from ScripBar
- [ ] Remove scrip (Delete key, context menu)
- [ ] Sort by each column (ascending/descending)
- [ ] Drag column to reorder
- [ ] Drag row to reorder
- [ ] Resize columns
- [ ] Multi-select (Ctrl+Click, Shift+Click, Ctrl+A)
- [ ] Copy/Cut/Paste (Ctrl+C/X/V)
- [ ] Drag between two Market Watch windows
- [ ] Context menu actions
- [ ] Toggle grid lines
- [ ] Toggle alternate colors
- [ ] Change font
- [ ] Change colors
- [ ] Save and load watch list
- [ ] Export to CSV
- [ ] Import from CSV

### Performance Tests
- [ ] 10 scrips - smooth updates
- [ ] 50 scrips - acceptable performance
- [ ] 100 scrips - performance degradation acceptable
- [ ] Rapid updates (100 ticks/sec) - no lag
- [ ] Memory usage reasonable

### Edge Cases
- [ ] Add duplicate scrip - prevention
- [ ] Paste invalid data - error handling
- [ ] Delete all rows - empty state
- [ ] Load corrupted file - graceful fallback
- [ ] Resize window very small - usability
- [ ] Very long symbol names - truncation

---

## Dependencies

### Qt Modules Required
- `Qt5::Widgets` (QTableView, QStyledItemDelegate)
- `Qt5::Core` (QAbstractTableModel, QSettings)
- `Qt5::Gui` (QPainter, QColor)

### Internal Dependencies
- ScripBar (for adding scrips)
- ScripData structure (shared data format)
- Settings Manager (application-wide settings)
- Market Data Feed (for real-time updates)

---

## Configuration File Format

### Watch List JSON Format
```json
{
  "name": "My Watch 1",
  "columns": [
    {"name": "Symbol", "visible": true, "width": 100, "order": 0},
    {"name": "LTP", "visible": true, "width": 80, "order": 1},
    {"name": "Change", "visible": true, "width": 70, "order": 2}
  ],
  "scrips": [
    {
      "symbol": "NIFTY",
      "exchange": "NSE",
      "segment": "FO",
      "instrument": "FUTIDX"
    }
  ],
  "settings": {
    "showGrid": true,
    "alternateColors": true,
    "font": "Arial,10,-1,5,50,0,0,0,0,0",
    "positiveColor": "#00C853",
    "negativeColor": "#FF1744"
  }
}
```

---

## Progress Tracking

### Overall Progress
- **Total Features**: 150+
- **Completed**: 0
- **In Progress**: 0
- **Pending**: 150+
- **Progress**: 0%

### Phase Status
| Phase | Status | Start Date | End Date | Progress |
|-------|--------|------------|----------|----------|
| Phase 1: Foundation | ⏳ Pending | - | - | 0% |
| Phase 2: Core Features | ⏳ Pending | - | - | 0% |
| Phase 3: Clipboard & Drag-Drop | ⏳ Pending | - | - | 0% |
| Phase 4: Visual Customization | ⏳ Pending | - | - | 0% |
| Phase 5: Advanced Features | ⏳ Pending | - | - | 0% |
| Phase 6: Polish & Optimization | ⏳ Pending | - | - | 0% |

---

## Notes & Decisions

### Design Decisions
1. **QTableView vs QTreeView**: Using QTableView for flat data structure (no hierarchy)
2. **Model Architecture**: Custom QAbstractTableModel for full control
3. **Delegate**: Custom QStyledItemDelegate for color coding and flash animations
4. **Persistence**: JSON format for watch lists (human-readable, easy to edit)
5. **Threading**: Market data updates in separate thread, UI updates on main thread

### Open Questions
1. ❓ Should we support grouping scrips by sector/category?
2. ❓ Maximum number of scrips per watch list? (Performance consideration)
3. ❓ Should watch lists be saved per user or per workspace?
4. ❓ Implement undo/redo for scrip operations?
5. ❓ Support for multiple data sources (NSE, BSE, MCX simultaneously)?

---

## Resources & References

### Qt Documentation
- [QTableView](https://doc.qt.io/qt-5/qtableview.html)
- [QAbstractTableModel](https://doc.qt.io/qt-5/qabstracttablemodel.html)
- [QStyledItemDelegate](https://doc.qt.io/qt-5/qstyleditemdelegate.html)
- [Model/View Programming](https://doc.qt.io/qt-5/model-view-programming.html)

### Similar Implementations
- MetaTrader 5 Market Watch
- Zerodha Kite Market Watch
- TradingView Watchlist
- Interactive Brokers TWS

---

**Last Updated**: 13 December 2025
**Document Version**: 1.0
**Status**: Planning Phase
