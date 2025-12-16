# Trading Terminal Widget Customizations

## Overview
This document details the custom Qt widgets developed for the Trading Terminal application. These widgets provide a polished, professional UI/UX specifically tailored for trading applications.

## Status: STABLE FRAMEWORK
⚠️ **These widgets are tested and production-ready. Modifications should be rare and carefully considered.**

## Custom Widgets

### 1. CustomMainWindow
**Location**: `src/core/widgets/CustomMainWindow.cpp`  
**Header**: `include/core/widgets/CustomMainWindow.h`

**Purpose**: Main application window with native macOS integration

**Key Features**:
- Native macOS menu bar integration
- MDI (Multiple Document Interface) area management
- Window state persistence
- Menu action handling

**Customizations**:
- Uses native macOS menu bar instead of in-window menu
- Seamless integration with macOS window management
- Custom toolbar with trading-specific actions

**Usage**:
```cpp
CustomMainWindow *mainWindow = new CustomMainWindow();
mainWindow->show();
```

---

### 2. CustomMDIArea
**Location**: `src/core/widgets/CustomMDIArea.cpp`  
**Header**: `include/core/widgets/CustomMDIArea.h`

**Purpose**: Custom MDI area for managing multiple child windows

**Key Features**:
- Tabbed interface support
- Window activation tracking
- Drag-and-drop window management
- Window list management

**Customizations**:
- Enhanced window activation logic
- Custom window ordering
- Debug logging for window lifecycle
- Support for multiple window types

**Signals**:
- `windowActivated(QString title)` - Emitted when a window becomes active
- `windowListChanged()` - Emitted when windows are added/removed

---

### 3. CustomMDISubWindow
**Location**: `src/core/widgets/CustomMDISubWindow.cpp`  
**Header**: `include/core/widgets/CustomMDISubWindow.h`

**Purpose**: Custom subwindow with custom title bar

**Key Features**:
- Custom title bar integration
- Resize handles (8 directions)
- Window controls (minimize, maximize, close)
- Native macOS-style appearance

**Customizations**:
- Frameless window with custom border
- Custom resize cursors
- Smooth resize animations
- Title bar drag support

**Usage**:
```cpp
CustomMDISubWindow *subWindow = new CustomMDISubWindow();
subWindow->setContentWidget(yourWidget);
subWindow->setWindowTitle("Your Window");
```

---

### 4. CustomMDIChild
**Location**: `src/core/widgets/CustomMDIChild.cpp`  
**Header**: `include/core/widgets/CustomMDIChild.h`

**Purpose**: Wrapper for MDI child windows

**Key Features**:
- Consistent child window behavior
- Title bar integration
- Focus management

---

### 5. CustomTitleBar
**Location**: `src/core/widgets/CustomTitleBar.cpp`  
**Header**: `include/core/widgets/CustomTitleBar.h`

**Purpose**: Custom title bar for MDI windows

**Key Features**:
- Native macOS-style buttons (red, yellow, green)
- Double-click to maximize
- Drag to move window
- Window title display

**Customizations**:
- SVG-based buttons for high-DPI support
- Hover effects
- Active/inactive states
- Custom spacing and padding

**Layout**:
```
[●][○][○]  Window Title
```

**Mouse Interactions**:
- Click red: Close window
- Click yellow: Minimize (if supported)
- Click green: Maximize/restore
- Double-click title: Maximize/restore
- Drag: Move window

---

### 6. CustomScripComboBox
**Location**: `src/core/widgets/CustomScripComboBox.cpp`  
**Header**: `include/core/widgets/CustomScripComboBox.h`

**Purpose**: Specialized combo box for scrip (security) selection

**Key Features**:
- Optimized for large datasets (10,000+ scrips)
- Search and filter capabilities
- Recent selections tracking
- Performance optimized

**Customizations**:
- Virtual item handling for large lists
- Custom popup styling
- Keyboard navigation
- Auto-complete support

**Debug Logging**:
- Tracks constructor calls
- Monitors item additions/removals
- Logs search operations

---

### 7. MDITaskBar
**Location**: `src/core/widgets/MDITaskBar.cpp`  
**Header**: `include/core/widgets/MDITaskBar.h`

**Purpose**: Task bar for quick window switching

**Key Features**:
- Button for each open window
- Active window highlighting
- Click to activate window
- Right-click context menu

**Customizations**:
- Custom button styling
- Icon support
- Active state indicators
- Smooth transitions

---

### 8. InfoBar
**Location**: `src/core/widgets/InfoBar.cpp`  
**Header**: `include/core/widgets/InfoBar.h`

**Purpose**: Information/status bar component

**Key Features**:
- Connection status display
- User information
- Market status indicators
- Real-time updates

**UI Definition**: `resources/forms/InfoBar.ui`

**Status Indicators**:
- 🟢 Connected
- 🔴 Disconnected
- 🟡 Connecting

---

### 9. CustomMarketWatch (Base Class)
**Location**: `src/core/widgets/CustomMarketWatch.cpp`  
**Header**: `include/core/widgets/CustomMarketWatch.h`

**Purpose**: Base class for market watch style table views

**Key Features**:
- Pre-configured table styling
- White background, no grid lines
- Custom header styling
- Context menu support
- Column management

**Subclass This For**:
- Market Watch windows
- Option Chain displays
- Any market data table

**Example Subclass**:
```cpp
class MarketWatchWindow : public CustomMarketWatch
{
    Q_OBJECT
public:
    explicit MarketWatchWindow(QWidget *parent = nullptr);
    
protected:
    QMenu* createContextMenu() override {
        QMenu *menu = CustomMarketWatch::createContextMenu();
        // Add custom menu items
        menu->addAction("Add to Watchlist");
        return menu;
    }
};
```

**Default Styling**:
- Background: White
- Selection: Light blue (#E3F2FD)
- No alternating row colors
- No grid lines
- Row height: 28px

---

### 10. CustomNetPosition (Base Class)
**Location**: `src/core/widgets/CustomNetPosition.cpp`  
**Header**: `include/core/widgets/CustomNetPosition.h`

**Purpose**: Base class for position/P&L style table views

**Key Features**:
- Pre-configured for financial data
- Summary row support
- Context menu with position actions
- Export functionality
- Custom formatting for P&L

**Subclass This For**:
- Position windows
- Order books
- Trade books
- P&L reports

**Example Subclass**:
```cpp
class PositionWindow : public CustomNetPosition
{
    Q_OBJECT
public:
    explicit PositionWindow(QWidget *parent = nullptr) {
        setSummaryRowEnabled(true);  // Enable summary row
    }
    
protected slots:
    void onClosePositionRequested() {
        // Handle close position
    }
    
    void onExportRequested() {
        // Handle CSV export
    }
};
```

**Default Styling**:
- Background: White
- Selection: Light blue (#E3F2FD)
- No alternating row colors
- No grid lines
- Row height: 28px
- Padding: 4px 8px per cell

---

## Common Styling Guidelines

### Color Palette
```css
Background (Main):      #FFFFFF (White)
Background (Header):    #F5F5F5 (Light Gray)
Border:                 #E0E0E0 (Medium Gray)
Selection:              #E3F2FD (Light Blue)
Text (Primary):         #000000 (Black)
Text (Secondary):       #666666 (Gray)
Accent (Buy/Long):      #4CAF50 (Green)
Accent (Sell/Short):    #F44336 (Red)
```

### Typography
- Default Font: System default (San Francisco on macOS)
- Header Font Weight: Bold
- Regular Text: 12-14px
- Small Text: 10-11px

### Spacing
- Padding (Cell): 4px 8px
- Margin (Widget): 8px
- Row Height: 28px (table rows)
- Header Height: 32px

### Animations
- Hover transitions: 200ms ease
- Window animations: 150ms ease-out
- Button press: 100ms

---

## Testing Guidelines

### Visual Testing
1. **macOS Native Feel**: Ensure widgets match macOS design language
2. **High DPI**: Test on Retina displays
3. **Dark Mode**: Verify appearance in system dark mode (future)
4. **Window Management**: Test minimize, maximize, close operations

### Performance Testing
1. **Large Datasets**: Test with 10,000+ items in combo boxes
2. **Multiple Windows**: Open 20+ MDI windows simultaneously
3. **Rapid Updates**: Stress test with market data updates (>100/sec)

### Interaction Testing
1. **Keyboard Navigation**: Tab, arrow keys, shortcuts
2. **Mouse Interactions**: Click, drag, double-click, right-click
3. **Touch**: Test on trackpad with gestures
4. **Accessibility**: VoiceOver compatibility

---

## Modification Guidelines

### When to Modify
- ✅ Bug fixes (visual or functional)
- ✅ Performance improvements
- ✅ New features explicitly requested
- ❌ "Nice to have" changes during feature development
- ❌ Experimental UI tweaks

### How to Modify
1. **Create Test Branch**: Never modify directly on main
2. **Document Changes**: Update this file with modifications
3. **Test Thoroughly**: Run full test suite
4. **Get Review**: Have changes reviewed before merging
5. **Update Screenshots**: If visual changes, update docs

### Modification Checklist
- [ ] Changes documented in this file
- [ ] All existing tests pass
- [ ] New tests added (if applicable)
- [ ] Visual regression tested on macOS
- [ ] No breaking changes to public API
- [ ] Debug logging added/updated
- [ ] Code reviewed

---

## Debugging

### Debug Logging
All custom widgets include comprehensive debug logging:

```cpp
qDebug() << "[WidgetName] Action description";
```

**Enable Debug Output**:
```bash
export QT_LOGGING_RULES="*.debug=true"
./TradingTerminal
```

**Common Debug Patterns**:
- Constructor/Destructor calls
- State changes
- Event handling
- Data updates

### Common Issues

#### Issue: Window doesn't show
**Check**:
- Window is added to MDI area: `mdiArea->addSubWindow(window)`
- Window is shown: `window->show()`
- Content widget is set: `subWindow->setContentWidget(widget)`

#### Issue: Sluggish performance
**Check**:
- Virtual scrolling enabled: `setVerticalScrollMode(ScrollPerPixel)`
- Model signals properly batched: Use `beginResetModel()/endResetModel()`
- Unnecessary repaints: Avoid `update()` in tight loops

#### Issue: Styling not applied
**Check**:
- Stylesheet syntax
- Widget hierarchy
- Qt style sheet precedence
- Platform-specific overrides

---

## Future Enhancements

### Planned
- [ ] Dark mode support
- [ ] Theme customization
- [ ] Window layout templates
- [ ] Keyboard shortcut customization

### Under Consideration
- [ ] Touch optimization
- [ ] Accessibility improvements
- [ ] Multi-monitor support enhancements
- [ ] Custom animations/transitions

---

## References

- [Qt Widgets Documentation](https://doc.qt.io/qt-5/qtwidgets-index.html)
- [Qt Style Sheets](https://doc.qt.io/qt-5/stylesheet.html)
- [macOS Human Interface Guidelines](https://developer.apple.com/design/human-interface-guidelines/macos)

---

**Document Version**: 1.0  
**Last Updated**: December 15, 2025  
**Maintained By**: Trading Terminal Development Team
