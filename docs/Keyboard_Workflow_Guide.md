# Keyboard Workflow Guide - ScripBar

## Overview
This guide documents the **keyboard-driven workflow** for the ScripBar in the Trading Terminal. The implementation is optimized for maximum speed, allowing users to select and add instruments to the active Market Watch window using only the keyboard.

**Important**: This workflow is **ScripBar-specific** and only active when focus is within the ScripBar fields. The shortcuts and Enter key behavior do not work globally across the application.

## Quick Reference

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| **Ctrl+S** (Win/Linux)<br>**Cmd+S** (macOS) | Focus Exchange field and start workflow |
| **Type** | Search and filter items in dropdown |
| **Enter** (dropdown open) | Select item, close dropdown, move to next field |
| **Enter** (dropdown closed) | Trigger "Add" button |
| **Tab** | Move to next field + select all text |
| **Shift+Tab** | Move to previous field + select all text |
| **Esc** | Close dropdown |
| **↓ ↑** | Navigate dropdown items |

## Workflow Example

### Adding NIFTY Option to Market Watch

```
Step 1: Press Ctrl+S (or Cmd+S on Mac)
   → ScripBar's Exchange field gets focus
   → Dropdown shows: NSE, BSE, MCX

Step 2: Type "n"
   → Filters to: NSE
   → Press Enter
   → Dropdown closes, focus moves to Segment

Step 3: Type "fo"
   → Filters to: FO
   → Press Enter
   → Dropdown closes, focus moves to Instrument

Step 4: Type "opt"
   → Filters to: OPTIDX
   → Press Enter
   → Dropdown closes, focus moves to Symbol

Step 5: Type "nifty"
   → Filters to: NIFTY, BANKNIFTY, FINNIFTY, MIDCPNIFTY
   → Use ↓ to select NIFTY if needed
   → Press Enter
   → Dropdown closes, focus moves to Expiry

Step 6: Type "19-dec"
   → Filters to expiry dates with "19-dec"
   → Press Enter
   → Dropdown closes, focus moves to Strike

Step 7: Type "2400"
   → Filters to: 24000, 24500
   → Press Enter
   → Dropdown closes, focus moves to Option Type

Step 8: Type "ce"
   → Filters to: CE
   → Press Enter
   → Dropdown closes, focus moves to Add button

Step 9: Press Enter
   → Add button clicked
   → Signal emitted: addToWatchRequested(exchange, segment, instrument, symbol, expiry, strike, optionType)
   → Instrument added to active Market Watch window!
```

**Total Time: ~15 seconds** (vs ~45 seconds with mouse)

**Note**: The actual addition to Market Watch window is handled by the parent component that receives the `addToWatchRequested` signal. The ScripBar only collects and validates the scrip details.

## Implementation Details

### Smart Enter Key Behavior

The `CustomScripComboBox` implements intelligent Enter key handling:

#### When Dropdown is Open:
```cpp
// File: src/ui/CustomScripComboBox.cpp
if (m_isPopupVisible) {
    hidePopup();                              // Close dropdown
    emit itemSelected(m_lineEdit->text());    // Emit selection signal
    QWidget *nextWidget = nextInFocusChain(); // Get next field
    nextWidget->setFocus();                   // Move focus forward
    event->accept();
    return;
}
```

#### When Dropdown is Closed:
```cpp
// File: src/ui/CustomScripComboBox.cpp
else {
    emit enterPressedWhenClosed();  // Signal parent to trigger action
    event->accept();
    return;
}
```

### Add Button Integration

In `ScripBar.cpp`, all combobox fields connect to the Add button:

```cpp
// Connect Enter key (when dropdown closed) to trigger Add button
connect(m_exchangeCombo, &CustomScripComboBox::enterPressedWhenClosed, 
        m_addToWatchButton, &QPushButton::click);
connect(m_segmentCombo, &CustomScripComboBox::enterPressedWhenClosed, 
        m_addToWatchButton, &QPushButton::click);
// ... all 7 fields connected
```

This means:
- **First Enter**: Closes dropdown + moves to next field
- **Second Enter** (after cycling through all fields): Triggers Add button

### Field Focus Chain

The tab order is automatically managed by Qt:

```
Exchange → Segment → Instrument → Symbol → Expiry → Strike → OptionType → Add Button
```

Press Tab to advance forward, Shift+Tab to go back.

## Context-Sensitive Behavior

**IMPORTANT**: All keyboard shortcuts and Enter key behavior are **ScripBar-specific** and only work when:
- Focus is on one of the ScripBar comboboxes (Exchange, Segment, Instrument, Symbol, Expiry, Strike, OptionType)
- Or focus is on the Add button within ScripBar

**When NOT active**:
- Focus is in Market Watch window
- Focus is in order entry panel
- Focus is in any other part of the application
- No ScripBar element has focus

This ensures the keyboard shortcuts don't interfere with other parts of the application.

## Features

### 1. **Zero Mouse Required**
- Entire ScripBar workflow can be completed without touching mouse
- Optimized for trading speed where milliseconds matter
- **Context-sensitive**: Only active when focus is within ScripBar fields

### 2. **Smart Filtering**
- Case-insensitive substring matching
- Debounced (150ms) for smooth performance
- Auto-sorts based on data type:
  - **Alphabetical**: Exchange, Segment, Instrument, Symbol, Option Type
  - **Chronological**: Expiry dates (earliest first)
  - **Numeric**: Strike prices (ascending order)

### 3. **Text Selection**
- Tab key automatically selects all text for easy replacement
- Focus-in events select text for quick editing
- Type to replace, don't need to clear first

### 4. **Visual Feedback**
- Gold highlighting (#FFD700) on matched text
- Custom QPainter rendering for performance
- Dropdown auto-shows when typing starts

### 5. **Debug Logging**
- Set `#define DEBUG_COMBOBOX 1` to enable logs
- Tracks Enter key behavior, dropdown state, focus changes
- Example logs:
  ```
  [CustomScripComboBox] Enter pressed with dropdown OPEN - closing and advancing focus
  [CustomScripComboBox] Enter pressed with dropdown CLOSED - emitting enterPressedWhenClosed signal
  ```

## Configuration

### Customizing Shortcuts

To change the activation shortcut, edit `ScripBar.cpp`:

```cpp
void ScripBar::setupShortcuts()
{
    // Change Ctrl+S to something else
    QShortcut *focusExchangeShortcut = new QShortcut(
        QKeySequence("Ctrl+E"),  // Changed from Ctrl+S
        this  // Important: 'this' makes it ScripBar-specific, not global
    );
    // ... rest remains same
}
```

**Note**: The shortcut parent is `this` (ScripBar widget), which means it only works when ScripBar or its child widgets have focus. This is **not a global shortcut**.

### Adjusting Filter Delay

To change the debounce delay (default 150ms):

```cpp
// In ScripBar::setupUI()
m_exchangeCombo->setFilterDebounceMs(200);  // Slower
m_symbolCombo->setFilterDebounceMs(100);    // Faster
```

### Minimum Characters for Dropdown

To require N characters before showing dropdown:

```cpp
m_symbolCombo->setMinCharsForDropdown(2);  // Require 2 chars
m_symbolCombo->setMinCharsForDropdown(0);  // No restriction (current)
```

## Troubleshooting

### Enter Key Not Working

**Symptom**: Pressing Enter doesn't advance focus or trigger Add button

**Causes**:
1. Event filter is consuming the event
2. Dropdown state not tracked correctly
3. Signal not connected

**Solution**:
```bash
# Enable debug logging
# Edit src/ui/CustomScripComboBox.cpp
#define DEBUG_COMBOBOX 1

# Rebuild and check logs
cd build
make
./TradingTerminal.app/Contents/MacOS/TradingTerminal
```

Look for these logs:
- `Enter pressed with dropdown OPEN` - should appear when Enter pressed in dropdown
- `Enter pressed with dropdown CLOSED` - should appear when pressing Enter in field

### Focus Not Advancing

**Symptom**: Enter closes dropdown but focus stays in same field

**Cause**: nextInFocusChain() returns nullptr or invalid widget

**Solution**: Check tab order is set correctly in ScripBar::setupUI()

### Add Button Not Triggered

**Symptom**: Double-Enter doesn't add to watchlist

**Cause**: enterPressedWhenClosed signal not connected

**Solution**: Verify connections in ScripBar.cpp around line 103-120

## Performance Tips

### 1. Use Keyboard Shortcuts
- **Ctrl+S**: Start workflow from anywhere
- **Tab/Shift+Tab**: Navigate fields quickly
- **Esc**: Quickly exit dropdown if needed

### 2. Partial Typing
- Type minimum characters to narrow down list
- Don't need to type full text
- Examples:
  - "n" → NSE
  - "fo" → FO
  - "opt" → OPTIDX
  - "nif" → NIFTY

### 3. Arrow Keys
- Use ↓↑ only when multiple matches exist
- Usually typing 2-3 chars is faster than arrows

### 4. Muscle Memory Workflow
Practice this sequence until it's automatic:
```
Ctrl+S → n → Enter → fo → Enter → opt → Enter → nifty → Enter 
→ 19 → Enter → 24000 → Enter → ce → Enter → Enter
```

## Technical Architecture

### Signal Flow Diagram

```
User Types
    ↓
QLineEdit::textEdited
    ↓
CustomScripComboBox::onTextEdited
    ↓
QTimer (150ms debounce)
    ↓
CustomScripComboBox::applyFiltering
    ↓
QSortFilterProxyModel updates
    ↓
Dropdown shows filtered results
    ↓
User presses Enter
    ↓
CustomScripComboBox::keyPressEvent
    ├─ If dropdown open:
    │    ├─ hidePopup()
    │    ├─ emit itemSelected()
    │    └─ nextInFocusChain()->setFocus()
    └─ If dropdown closed:
         └─ emit enterPressedWhenClosed()
              ↓
         ScripBar receives signal
              ↓
         m_addToWatchButton->click()
              ↓
         ScripBar::onAddToWatchClicked
              ↓
         emit addToWatchRequested(exchange, segment, instrument, symbol, expiry, strike, optionType)
              ↓
         Parent component receives signal
              ↓
         Adds instrument to active Market Watch window
```

### Key Classes

1. **CustomScripComboBox** (`include/ui/CustomScripComboBox.h`)
   - Manages keyboard input and filtering
   - Emits signals for selection and Enter key
   - Tracks dropdown visibility state

2. **ScripBar** (`include/ui/ScripBar.h`)
   - Contains 7 CustomScripComboBox instances
   - Connects enterPressedWhenClosed to Add button
   - Manages field population and dependencies

3. **HighlightDelegate** (inner class in `CustomScripComboBox.cpp`)
   - Custom QPainter rendering for text highlighting
   - Draws gold background for matched substrings

## Future Enhancements

### Planned Features

1. **Additional Shortcuts**
   - Ctrl+W: Jump directly to Symbol field (most common search)
   - Ctrl+Enter: Direct add without cycling through all fields
   - Ctrl+1-7: Jump to specific field by number

2. **Recent Items**
   - Remember last N selected items per field
   - Show recent items at top of dropdown
   - Clear recent items button

3. **Fuzzy Matching**
   - Match non-contiguous characters (e.g., "bknfty" → "BANKNIFTY")
   - Configurable fuzzy threshold

4. **Autocomplete**
   - Show suggestion inline as you type
   - Press → to accept suggestion
   - Similar to browser address bar

5. **Clipboard Integration**
   - Ctrl+V to paste symbol directly
   - Parse and auto-populate all fields from pasted text

## Testing

### Manual Test Cases

#### Test 1: Basic Workflow
1. Press Ctrl+S
2. Verify Exchange field has focus
3. Type "nse"
4. Press Enter
5. Verify Segment field has focus
6. Verify Exchange shows "NSE"

#### Test 2: Enter Key Behavior
1. Focus any field
2. Type to open dropdown
3. Press Enter
4. Verify dropdown closes
5. Verify focus moves to next field
6. Press Enter again (dropdown closed)
7. Verify Add button is triggered

#### Test 3: Tab Navigation
1. Press Ctrl+S
2. Press Tab repeatedly
3. Verify focus cycles through all fields in order
4. Press Shift+Tab
5. Verify focus goes backwards

#### Test 4: Filtering
1. Focus Symbol field
2. Type "rel"
3. Verify only "RELIANCE" appears (if in list)
4. Type "xyz"
5. Verify empty dropdown (no matches)

### Automated Testing
See `docs/CustomScripComboBox_Testing_Guide.md` for detailed test procedures.

## Version History

### v1.3 - Smart Enter Key (Current)
- Added enterPressedWhenClosed signal
- Implemented dual Enter behavior (close dropdown vs trigger Add)
- Focus automatically advances after selection
- Keyboard-only workflow complete

### v1.2 - Keyboard Shortcuts
- Added Ctrl+S/Cmd+S shortcut to focus Exchange
- setupShortcuts() method added to ScripBar
- Cross-platform shortcut handling

### v1.1 - All Fields Custom
- Converted all 7 ScripBar fields to CustomScripComboBox
- Consistent behavior across all input fields
- Comprehensive debug logging added

### v1.0 - Initial Implementation
- Basic CustomScripComboBox with filtering
- Alphabetical, chronological, and numeric sorting
- QPainter-based highlighting
- Tab select-all functionality

## Support

For issues, suggestions, or contributions:
- Check debug logs with `DEBUG_COMBOBOX` enabled
- Review `docs/CustomScripComboBox_Debug_Guide.md`
- Test with `docs/CustomScripComboBox_Testing_Guide.md`

---

**Last Updated**: December 2024  
**Author**: Trading Terminal Development Team  
**Status**: Production Ready ✅
