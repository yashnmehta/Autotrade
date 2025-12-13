# CustomScripComboBox Documentation

## Overview
`CustomScripComboBox` is a sophisticated, custom Qt combobox widget specifically designed for scrip/symbol search in trading applications. It provides an enhanced user experience with intelligent filtering, sorting, and keyboard navigation.

---

## Features Implemented

### ✅ Your Requirements

1. **Line-Edit Feel**
   - Fully editable combobox with integrated line edit
   - Clean, minimalist design matching your trading terminal theme
   - Clear button for quick reset

2. **Alphabetical Sorting for Symbols**
   - Automatic ascending order (A-Z) for symbol lists
   - Case-insensitive sorting

3. **Chronological Sorting for Expiries**
   - Date-wise sorting (earliest to latest)
   - Supports multiple date formats: `dd-MMM-yyyy`, `dd-MM-yyyy`, `yyyy-MM-dd`, etc.

4. **Auto-Dropdown After 2 Characters**
   - Dropdown opens automatically when typing 2+ characters
   - Configurable threshold via `setMinCharsForDropdown()`
   - Shows only filtered results

5. **Select All on Tab**
   - Text automatically selected when pressing Tab
   - Also selects all on focus for easy editing

---

## Additional Enhancements

### 🚀 Advanced Filtering

- **Fuzzy Matching**: Finds items even with non-consecutive characters
  - Example: Typing "bnk" will match "BANKNIFTY"
  
- **Substring Matching**: Case-insensitive substring search
  - Example: "bank" matches "HDFCBANK", "ICICIBANK", etc.

- **Smart Scoring System**: Results ranked by relevance
  - Exact prefix matches appear first
  - Consecutive character matches score higher
  - Shorter matching items rank higher

### ⌨️ Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `Tab` | Select all text |
| `Esc` | Close dropdown and clear selection |
| `Enter` | Accept current selection and close dropdown |
| `Up/Down` | Navigate dropdown (auto-opens if closed) |

### 🎨 User Experience

- **Recent Items Highlighting**: Recently selected items appear in teal and bold
- **Debounced Filtering**: 150ms delay prevents excessive filtering during fast typing
- **Auto-Select on Focus**: Text automatically selected when field receives focus
- **Dark Theme Styling**: Pre-styled to match your trading terminal theme

### ⚡ Performance Optimizations

- **Batched List Layout**: Efficient rendering for large lists
- **Uniform Item Sizes**: Faster list view performance
- **Debounced Filtering**: Reduces unnecessary computations
- **Smart Refiltering**: Only refilters when input actually changes

---

## Usage Examples

### Basic Usage (Symbol Search)

```cpp
// Create symbol combobox
CustomScripComboBox *symbolCombo = new CustomScripComboBox(this);
symbolCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);
symbolCombo->setMinCharsForDropdown(2);

// Add symbols
QStringList symbols = {"RELIANCE", "TCS", "INFY", "HDFCBANK", "ICICIBANK"};
symbolCombo->addItems(symbols);

// Connect to selection signal
connect(symbolCombo, &CustomScripComboBox::itemSelected, 
        this, [](const QString &symbol) {
    qDebug() << "Selected symbol:" << symbol;
});
```

### Expiry Date Search

```cpp
// Create expiry combobox
CustomScripComboBox *expiryCombo = new CustomScripComboBox(this);
expiryCombo->setSortMode(CustomScripComboBox::ChronologicalSort);
expiryCombo->setMinCharsForDropdown(2);

// Add expiry dates (will be sorted chronologically)
QStringList expiries = {
    "30-Jan-2025", "26-Dec-2024", "02-Jan-2025", "19-Dec-2024"
};
expiryCombo->addItems(expiries);
// Result order: 19-Dec-2024, 26-Dec-2024, 02-Jan-2025, 30-Jan-2025
```

### Advanced Configuration

```cpp
CustomScripComboBox *combo = new CustomScripComboBox(this);

// Configure behavior
combo->setSortMode(CustomScripComboBox::AlphabeticalSort);
combo->setMinCharsForDropdown(3);           // Require 3 chars before dropdown
combo->setFilterDebounceMs(200);            // 200ms debounce delay
combo->setMaxVisibleItems(15);              // Show up to 15 items
combo->setShowRecentItems(true);            // Highlight recent selections

// Connect signals
connect(combo, &CustomScripComboBox::textChanged, 
        this, &MyClass::onSearchTextChanged);
connect(combo, &CustomScripComboBox::itemSelected, 
        this, &MyClass::onItemSelected);
```

---

## API Reference

### Configuration Methods

#### `setSortMode(SortMode mode)`
Sets the sorting mode for items.
- **AlphabeticalSort**: A-Z sorting for symbols/text
- **ChronologicalSort**: Date-based sorting for expiries
- **NoSort**: Keep original insertion order

#### `setMinCharsForDropdown(int chars)`
Sets minimum characters required before dropdown opens (default: 2)

#### `setFilterDebounceMs(int ms)`
Sets debounce delay for filtering in milliseconds (default: 150)

#### `setMaxVisibleItems(int count)`
Sets maximum number of visible items in dropdown

#### `setShowRecentItems(bool show)`
Enable/disable recent items highlighting (default: true)

### Data Management Methods

#### `addItem(const QString &text, const QVariant &userData = QVariant())`
Adds a single item to the combobox

#### `addItems(const QStringList &texts)`
Adds multiple items at once

#### `clearItems()`
Clears all items from the combobox

#### `currentText() const`
Returns the current text in the line edit

#### `selectAllText()`
Programmatically select all text

#### `recentItems() const`
Returns list of recently selected items

### Signals

#### `textChanged(const QString &text)`
Emitted when the text is edited by the user

#### `itemSelected(const QString &text)`
Emitted when an item is selected from the dropdown

---

## Supported Date Formats

The chronological sorting recognizes these date formats:

- `dd-MMM-yyyy` → 26-Dec-2024
- `dd-MM-yyyy` → 26-12-2024
- `yyyy-MM-dd` → 2024-12-26
- `dd/MM/yyyy` → 26/12/2024
- `MMM-yyyy` → Dec-2024
- `MMMMyyyy` → DEC2024

---

## Fuzzy Matching Examples

| User Input | Matches |
|------------|---------|
| "rel" | **REL**IANCE, BA**REL** |
| "bnk" | **B**A**NK**NIFTY, HDFC**B**A**NK** |
| "hdf" | **HDF**CBANK, **HDF**C |
| "dec" | 26-**Dec**-2024, 30-**Dec**-2024 |

---

## Styling Customization

The combobox comes pre-styled for dark themes. You can customize further:

```cpp
combo->setStyleSheet(
    "QComboBox {"
    "    border: 1px solid #your-color;"
    "    background: #your-bg;"
    "    color: #your-text;"
    "}"
    // ... more styles
);
```

---

## Integration in ScripBar

The `CustomScripComboBox` is now integrated into `ScripBar`:

- **Symbol field**: Uses alphabetical sorting
- **Expiry field**: Uses chronological sorting
- Both fields auto-open dropdown after 2 characters
- Tab key selects all text for quick editing

---

## Best Practices

### 1. **Choose Appropriate Sort Mode**
```cpp
// For symbols/text
combo->setSortMode(CustomScripComboBox::AlphabeticalSort);

// For dates
combo->setSortMode(CustomScripComboBox::ChronologicalSort);

// For order-sensitive data
combo->setSortMode(CustomScripComboBox::NoSort);
```

### 2. **Optimize for Large Lists**
```cpp
// Pre-allocate items
QStringList largeList;
largeList.reserve(1000);
// ... populate list ...
combo->addItems(largeList); // Single call better than many addItem()
```

### 3. **Handle User Input**
```cpp
connect(combo, &CustomScripComboBox::itemSelected,
        this, [this](const QString &item) {
    // Validate selection
    if (!item.isEmpty()) {
        processSelection(item);
    }
});
```

### 4. **Clear When Context Changes**
```cpp
void onInstrumentChanged() {
    symbolCombo->clearItems();
    symbolCombo->addItems(getSymbolsForInstrument());
}
```

---

## Performance Characteristics

- **Filtering**: O(n) where n = number of items
- **Fuzzy Matching**: O(m*k) where m = filter length, k = item length
- **Sorting**: O(n log n)
- **Memory**: O(n) for storing items

For very large lists (>10,000 items), consider:
1. Virtual scrolling
2. Async filtering
3. Server-side filtering

---

## Future Enhancement Ideas

Here are additional features that could be implemented:

### 1. **Multi-Column Support**
```cpp
// Show symbol + exchange + sector in dropdown
combo->addColumns({"Symbol", "Exchange", "Sector"});
```

### 2. **Icon Support**
```cpp
// Add icons for different instrument types
combo->addItem("NIFTY", icon, userData);
```

### 3. **Custom Delegates**
```cpp
// Rich HTML rendering in dropdown
combo->setItemDelegate(new CustomStyledItemDelegate);
```

### 4. **Grouped Items**
```cpp
// Group by categories
combo->addGroup("Indices");
combo->addItem("NIFTY");
combo->addItem("BANKNIFTY");
combo->addGroup("Stocks");
```

### 5. **API Integration**
```cpp
// Fetch results from server
connect(combo, &CustomScripComboBox::textChanged, [](const QString &text) {
    if (text.length() >= 3) {
        fetchFromAPI(text);
    }
});
```

### 6. **Favorites/Pinned Items**
```cpp
// Pin frequently used items to top
combo->addPinnedItem("NIFTY");
```

### 7. **Search History**
```cpp
// Save and restore search history
combo->setSearchHistory(loadHistory());
```

### 8. **Keyboard Quick Search**
```cpp
// Jump to items by typing first letter
// "N" -> NIFTY, "NN" -> NIFTY, "NNN" -> Next N item
```

### 9. **Voice Input**
```cpp
// Speech-to-text for hands-free trading
combo->enableVoiceInput(true);
```

### 10. **Context Menu**
```cpp
// Right-click options
combo->addContextMenuAction("Add to Watchlist");
combo->addContextMenuAction("View Chart");
```

---

## Troubleshooting

### Dropdown not opening after 2 characters?
Check: `combo->setMinCharsForDropdown(2);`

### Items not sorting?
Ensure: `combo->setSortMode(CustomScripComboBox::AlphabeticalSort);`

### Dates sorting incorrectly?
- Use `ChronologicalSort` mode
- Verify date format matches supported formats
- Check date string parsing with `parseDate()`

### Text not selecting on Tab?
- Ensure the combobox is editable
- Event filter is automatically installed in constructor

### Performance issues with many items?
- Increase debounce time: `combo->setFilterDebounceMs(300);`
- Limit visible items: `combo->setMaxVisibleItems(20);`
- Consider lazy loading or server-side filtering

---

## Comparison with Standard QComboBox

| Feature | QComboBox | CustomScripComboBox |
|---------|-----------|---------------------|
| Editable | ✓ | ✓ |
| Basic filtering | ✓ | ✓ |
| Fuzzy matching | ✗ | ✓ |
| Smart scoring | ✗ | ✓ |
| Auto-dropdown | ✗ | ✓ |
| Tab select-all | ✗ | ✓ |
| Recent items | ✗ | ✓ |
| Date sorting | ✗ | ✓ |
| Debounced filtering | ✗ | ✓ |
| Custom styling | ✓ | ✓ (Pre-styled) |

---

## Testing Checklist

- [ ] Type 2 characters - dropdown opens
- [ ] Type 1 character - dropdown stays closed
- [ ] Press Tab - text selected
- [ ] Press Esc - dropdown closes
- [ ] Press Enter - selection confirmed
- [ ] Fuzzy search works (e.g., "bnk" → "BANKNIFTY")
- [ ] Symbols sorted A-Z
- [ ] Dates sorted chronologically
- [ ] Recent items highlighted
- [ ] Clear button works
- [ ] Focus select-all works
- [ ] Keyboard navigation works

---

## License & Credits

Developed for Trading Terminal C++ project
Compatible with Qt5
Author: Trading Terminal Development Team
