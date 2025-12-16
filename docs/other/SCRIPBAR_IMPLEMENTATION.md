# ScripBar Implementation Guide

## Overview
ScripBar is the instrument selection widget that allows users to search and add trading instruments to their watchlist. It follows the existing CustomScripComboBox architecture for consistency with the rest of the application.

## Architecture

### Widget Structure
```
ScripBar (QWidget)
└── QHBoxLayout
    ├── Exchange (CustomScripComboBox) - "NSE", "BSE", "MCX"
    ├── Segment (CustomScripComboBox) - "CM", "FO", "CD"
    ├── Instrument (CustomScripComboBox) - "EQUITY", "FUTIDX", "OPTIDX", etc.
    ├── Symbol (CustomScripComboBox) - "RELIANCE", "NIFTY", etc.
    ├── Expiry (CustomScripComboBox) - Date selection
    ├── Strike (CustomScripComboBox) - Price levels
    ├── Option Type (CustomScripComboBox) - "CE", "PE"
    └── Add Button (QPushButton) - Emits addToWatchRequested signal
```

### Cascading Logic
Each combo box change triggers population of the next combo:
1. **Exchange** → Segments (NSE: CM/FO/CD, BSE: CM/FO, MCX: FO)
2. **Segment** → Instruments (CM: EQUITY, FO: FUTIDX/FUTSTK/OPTIDX/OPTSTK)
3. **Instrument** → Symbols (Currently demo data, future XTS API)
4. **Symbol** → Expiries (Demo dates, future XTS data)
5. **Expiry** → Strikes (Demo strikes, future XTS data)
6. **Strike** → Option Types (CE/PE)

### CustomScripComboBox Features
- **Editable**: Users can type to search
- **Sorting Modes**:
  - AlphabeticalSort: Exchange, Segment, Instrument, Symbol
  - ChronologicalSort: Expiry (date-based)
  - NumericSort: Strike (price-based)
  - NoSort: Option Type (preserve CE/PE order)
- **Keyboard Shortcuts**:
  - Ctrl+S: Focus Exchange combo
  - Enter: Advance to next combo or trigger Add button
  - Esc: Clear selection
  - Tab: Move to next combo
- **Select All**: Automatically selects all text on focus for easy editing

## Styling
Dark theme matching existing toolbar:
- Background: #2d2d30
- Combo boxes: #1e1e1e
- Button: #0e639c (blue accent)
- Text: #ffffff
- Selection: #094771

## XTS API Integration

### Current State (Demo Data)
- Hardcoded symbols for testing: RELIANCE, TCS, INFY, NIFTY, etc.
- Hardcoded expiries: December 2024 - February 2025
- Hardcoded strikes: 17000-25000 in 500 increments

### Future Enhancement (XTS API)
The implementation includes prepared hooks:

```cpp
void ScripBar::setXTSClient(XTSMarketDataClient *client);
void ScripBar::searchInstrumentsAsync(const QString &searchText);
void ScripBar::onInstrumentsReceived(const QVector<InstrumentData> &instruments);
int ScripBar::getCurrentExchangeSegmentCode() const; // Maps to XTS segment codes
```

**XTS Exchange Segment Mapping**:
- NSE_CM: 1 (NSECM)
- NSE_FO: 2 (NSEFO)
- NSE_CD: 13 (NSECD)
- BSE_CM: 11 (BSECM)
- BSE_FO: 12 (BSEFO)
- BSE_CD: 61 (BSECD)
- MCX_FO: 51 (MCXFO)

To enable real XTS search:
1. Uncomment `searchInstrumentsAsync()` call in `populateSymbols()`
2. Call `XTSMarketDataClient::searchInstruments()` with exchange segment code
3. Parse instrument data to extract symbol, expiry, strike, option type
4. Cache results for performance

## Usage Example

### In MainWindow
```cpp
// Create ScripBar
m_scripBar = new ScripBar(this);
m_scripBar->setXTSClient(m_xtsMarketDataClient);

// Connect signal
connect(m_scripBar, &ScripBar::addToWatchRequested,
        this, &MainWindow::onAddToWatchRequested);

// Add to layout
contentLayout->addWidget(m_scripBar);
```

### Signal Handler
```cpp
void MainWindow::onAddToWatchRequested(const QString &exchange,
                                       const QString &segment,
                                       const QString &instrument,
                                       const QString &symbol,
                                       const QString &expiry,
                                       const QString &strike,
                                       const QString &optionType)
{
    qDebug() << "Add to watch:" << symbol << expiry << strike << optionType;
    // Add to MarketWatchWindow
    // Subscribe to XTS feed
}
```

## Testing Workflow

### Manual Test Cases
1. **Cascading Selection**:
   - Select NSE → Should show CM/FO/CD
   - Select FO → Should show FUTIDX/OPTIDX
   - Select OPTIDX → Should show NIFTY/BANKNIFTY

2. **Keyboard Navigation**:
   - Press Ctrl+S → Exchange should focus
   - Type "BSE" → Should filter to BSE
   - Press Enter → Should advance to Segment

3. **Enter Workflow**:
   - Fill all combos
   - Press Enter in Option Type → Should trigger Add button

4. **Sorting**:
   - Expiry combo should show chronological order
   - Strike combo should show numeric order

### XTS Integration Testing
1. Login to XTS via LoginWindow
2. Open MainWindow
3. ScripBar should have XTS client set
4. Symbol search should call XTS API (when enabled)
5. Verify instrument data populates correctly

## File Structure

### Header
`include/app/ScripBar.h`
- Class definition
- Signal: `addToWatchRequested`
- Slots: Cascading onChange handlers
- Members: 7 CustomScripComboBox pointers + button + layout

### Implementation
`src/app/ScripBar.cpp`
- setupUI(): Creates widget layout
- populate*() methods: Cascading data population
- on*Changed() slots: Cascade triggers
- setupShortcuts(): Keyboard shortcuts
- XTS integration hooks

### Backup Files
- `src/app/ScripBar.cpp.old` - Original demo implementation
- `src/app/ScripBar.cpp.ui_backup` - Old UI-based approach

## Migration Notes

### Changes from Old Implementation
- **Removed**: UI file dependency (`ui_ScripBar.h`)
- **Kept**: CustomScripComboBox architecture (maintained)
- **Added**: XTS API hooks (searchInstrumentsAsync)
- **Enhanced**: Enter key workflow (all combos trigger Add)

### Backwards Compatibility
Signal signature matches old implementation:
```cpp
void addToWatchRequested(const QString &exchange, ...);
```

Any code connecting to this signal will continue to work.

## Future Enhancements

1. **Real-time Search**: Type in Symbol combo → Search XTS API
2. **Instrument Caching**: Cache frequently accessed instruments
3. **Smart Defaults**: Remember last used exchange/segment
4. **Quick Access**: Recent symbols dropdown
5. **Option Chain**: View full option chain for selected symbol
6. **Greeks Display**: Show implied volatility, delta, etc.
7. **Multi-leg Strategies**: Support spreads, straddles, strangles

## Performance Considerations

- **Lazy Loading**: Only load data when combo is focused
- **Debounce Search**: Wait for user to stop typing before searching
- **Cache Management**: Clear cache after session timeout
- **Async Operations**: All XTS calls are non-blocking

## Known Limitations

1. **Demo Data**: Currently uses hardcoded symbols/strikes
2. **No Validation**: Doesn't validate expiry/strike existence
3. **No Option Chain**: Manual entry required
4. **Single Symbol**: Can only add one at a time

These will be addressed in future iterations as XTS integration matures.
