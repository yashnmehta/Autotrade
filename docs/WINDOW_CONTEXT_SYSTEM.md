# Window Context & Preferences System

## Overview

A comprehensive system for smart window initialization based on context (where the window was opened from) and user preferences. When you open a Buy/Sell window from Market Watch, it automatically fills in the contract details, prices, quantities, and order preferences.

## Architecture

### 1. WindowContext (Data Carrier)
**File**: `include/models/WindowContext.h`

Carries contract and market data when opening child windows:

```cpp
WindowContext context;
context.sourceWindow = "MarketWatch";
context.exchange = "NSEFO";
context.token = 49508;
context.symbol = "BANKNIFTY";
context.ltp = 59146.80;
context.ask = 59184.00;
context.bid = 59183.00;
context.lotSize = 15;
context.expiry = "30DEC2025";
context.strikePrice = 51000;
context.optionType = "CE";
```

### 2. PreferencesManager (User Settings)
**Files**: `include/utils/PreferencesManager.h`, `src/utils/PreferencesManager.cpp`

Singleton managing trading preferences stored in `~/.config/TradingCompany/TradingTerminal.conf`:

- Default order type (LIMIT/MARKET/SL/SL-M)
- Default product (MIS/NRML/CO/BO)
- Default validity (DAY/IOC)
- Default quantities per segment
- Auto-fill behavior
- Price calculation offsets

### 3. Enhanced Windows
**Files**: `include/views/BuyWindow.h`, `src/views/BuyWindow.cpp` (similar for SellWindow, SnapQuoteWindow)

Windows now accept context and apply preferences:

```cpp
// Constructor with context
BuyWindow(const WindowContext &context, QWidget *parent = nullptr);

// Load context method
void loadFromContext(const WindowContext &context);

// Load user preferences
void loadPreferences();

// Calculate prices based on market data
void calculateDefaultPrice(const WindowContext &context);
```

## Implementation Guide

### Step 1: MarketWatch Integration

Update `MarketWatchWindow` to create context when opening windows:

```cpp
void MarketWatchWindow::onBuyAction()
{
    QModelIndex proxyIndex = this->currentIndex();
    if (!proxyIndex.isValid()) return;
    
    int sourceRow = mapToSource(proxyIndex.row());
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    
    if (!scrip.isValid()) return;
    
    // Create context from scrip data
    WindowContext context;
    context.sourceWindow = "MarketWatch";
    context.sourceRow = sourceRow;
    context.exchange = scrip.exchange;
    context.token = scrip.token;
    context.symbol = scrip.symbol;
    context.displayName = scrip.symbol;  // Or use description if available
    context.series = scrip.series;
    
    // Market data
    context.ltp = scrip.ltp;
    context.bid = scrip.bid;
    context.ask = scrip.ask;
    context.high = scrip.high;
    context.low = scrip.low;
    context.close = scrip.close;
    context.volume = scrip.volume;
    
    // Contract specs (need to fetch from repository)
    RepositoryManager *repo = RepositoryManager::getInstance();
    if (context.exchange == "NSEFO") {
        const ContractData *contract = repo->getNSEFO()->getContract(scrip.token);
        if (contract) {
            context.lotSize = contract->lotSize;
            context.tickSize = contract->tickSize;
            context.freezeQty = contract->freezeQty;
            context.expiry = contract->expiryDate;
            context.strikePrice = contract->strikePrice;
            context.optionType = contract->optionType;
            context.instrumentType = contract->series;  // FUTIDX, OPTIDX, etc.
            context.segment = "F";  // FnO segment
        }
    }
    // Similar for NSECM, BSEFO, BSECM
    
    // Emit signal with context
    emit buyRequestedWithContext(context);
}
```

### Step 2: MainWindow Integration

Update `MainWindow` to create windows with context:

```cpp
void MainWindow::createBuyWindow()
{
    // Check if there's an active Market Watch
    MarketWatchWindow *activeWatch = getActiveMarketWatch();
    
    if (activeWatch && activeWatch->hasSelection()) {
        // Get context from active Market Watch
        WindowContext context = activeWatch->getSelectedContractContext();
        
        // Create Buy Window with context
        BuyWindow *buyWindow = new BuyWindow(context, this);
        
        CustomMDISubWindow *subWindow = new CustomMDISubWindow();
        subWindow->setWidget(buyWindow);
        subWindow->setWindowTitle("Buy Order");
        
        m_mdiArea->addWindow(subWindow);
        subWindow->show();
    } else {
        // No selection - create empty Buy Window
        BuyWindow *buyWindow = new BuyWindow(this);
        
        CustomMDISubWindow *subWindow = new CustomMDISubWindow();
        subWindow->setWidget(buyWindow);
        subWindow->setWindowTitle("Buy Order");
        
        m_mdiArea->addWindow(subWindow);
        subWindow->show();
    }
    
    qDebug() << "[MainWindow] Buy Window created";
}

// Helper to get active Market Watch with selection
MarketWatchWindow* MainWindow::getActiveMarketWatch()
{
    CustomMDISubWindow *activeSubWindow = m_mdiArea->activeSubWindow();
    if (!activeSubWindow) return nullptr;
    
    return qobject_cast<MarketWatchWindow*>(activeSubWindow->widget());
}
```

### Step 3: Connect Signals

In `MainWindow::setupContent()`:

```cpp
// When Market Watch window is created
connect(marketWatch, &MarketWatchWindow::buyRequestedWithContext,
        this, &MainWindow::onBuyRequestedWithContext);
        
connect(marketWatch, &MarketWatchWindow::sellRequestedWithContext,
        this, &MainWindow::onSellRequestedWithContext);
```

And add the slots:

```cpp
void MainWindow::onBuyRequestedWithContext(const WindowContext &context)
{
    BuyWindow *buyWindow = new BuyWindow(context, this);
    
    CustomMDISubWindow *subWindow = new CustomMDISubWindow();
    subWindow->setWidget(buyWindow);
    subWindow->setWindowTitle(QString("Buy - %1").arg(context.symbol));
    
    m_mdiArea->addWindow(subWindow);
    subWindow->show();
    
    qDebug() << "[MainWindow] Buy Window created with context:" << context.toString();
}
```

## User Preferences Usage

### Setting Defaults

Users can set default preferences programmatically (later add UI):

```cpp
PreferencesManager &prefs = PreferencesManager::instance();

// Trading defaults
prefs.setDefaultOrderType("LIMIT");
prefs.setDefaultProduct("MIS");  // Intraday
prefs.setDefaultValidity("DAY");

// Quantity defaults per segment
prefs.setDefaultQuantity("E", 1);      // Equity: 1 share
prefs.setDefaultQuantity("F", 15);     // FnO: 1 lot (BANKNIFTY = 15)
prefs.setDefaultQuantity("O", 15);     // Options: 1 lot

// Price calculation
prefs.setBuyPriceOffset(0.05);   // Buy at ask + ₹0.05
prefs.setSellPriceOffset(-0.05); // Sell at bid - ₹0.05

// Auto-fill behavior
prefs.setAutoFillQuantity(true);
prefs.setAutoFillPrice(true);
prefs.setAutoCalculatePrice(true);
```

### Quick Presets

Save and load quick presets:

```cpp
// Save preset
PreferencesManager::TradingPreset scalping;
scalping.orderType = "MARKET";
scalping.product = "MIS";
scalping.validity = "IOC";
scalping.quantity = 50;

prefs.savePreset("Scalping", scalping);

// Load preset
auto preset = prefs.getPreset("Scalping");
// Apply to UI...
```

## Window Behavior

### Buy Window
- **Price**: Uses `ask` price + offset for limit orders
- **Quantity**: Auto-fills based on segment (lot size for F&O)
- **Order Type**: Uses preference (default: LIMIT)
- **Product**: Uses preference (default: MIS)

### Sell Window
- **Price**: Uses `bid` price - offset for limit orders
- **Quantity**: Same as Buy Window
- **Order Type**: Uses preference
- **Product**: Uses preference

### SnapQuote Window
- Shows complete contract details
- Real-time market depth
- No trading-specific preferences needed

## Multi-Selection Handling

When multiple rows are selected in Market Watch:

```cpp
void MarketWatchWindow::onBuyAction()
{
    QModelIndexList selection = getSelectedRows();
    
    if (selection.isEmpty()) return;
    
    // Use LAST selected row (most recent)
    QModelIndex lastIndex = selection.last();
    int sourceRow = mapToSource(lastIndex.row());
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    
    // Create context from last selection
    WindowContext context = createContext(scrip);
    emit buyRequestedWithContext(context);
}
```

## Configuration File Location

Preferences are automatically saved to:
- **Linux**: `~/.config/TradingCompany/TradingTerminal.conf`
- **Windows**: `C:\Users\<User>\AppData\Roaming\TradingCompany\TradingTerminal.ini`
- **macOS**: `~/Library/Preferences/com.tradingcompany.tradingterminal.plist`

## Next Steps

1. **Add MarketWatch integration**: Create `getSelectedContractContext()` method
2. **Update signal/slot connections**: Connect context-aware signals
3. **Implement SellWindow**: Mirror BuyWindow implementation
4. **Implement SnapQuoteWindow**: Use context for initialization
5. **Add Preferences UI**: Create dialog for managing preferences
6. **Add quick preset buttons**: Toolbar buttons for common presets
7. **Position Window integration**: Open Buy/Sell with existing position details
8. **OrderBook integration**: Modify existing orders with context

## Example: Complete Flow

```
User Action: Right-click on "BANKNIFTY 51000 CE" in Market Watch → Buy

1. MarketWatch creates WindowContext:
   - Fetches scrip data from model
   - Fetches contract specs from repository
   - Fetches current market prices
   - Packages into WindowContext

2. MainWindow receives signal with context

3. BuyWindow created with context:
   - loadPreferences() sets order type, product, validity
   - loadFromContext() fills exchange, token, symbol, expiry, strike
   - calculateDefaultPrice() sets price = ask (59184) + offset (0.05) = 59184.05
   - Auto-fills quantity = 1 lot = 15 contracts

4. User sees pre-filled Buy Window ready to submit
   - Only needs to review and click Submit
   - Can modify any field if needed
```

## Benefits

✅ **Speed**: Reduces 10+ clicks to 2 clicks  
✅ **Accuracy**: No manual typing errors  
✅ **Consistency**: Always uses preferred settings  
✅ **Flexibility**: Can override any auto-filled value  
✅ **Intelligence**: Price calculation based on market depth  
✅ **Personalization**: Each user has their own defaults  

## Status

- ✅ WindowContext struct created
- ✅ PreferencesManager implemented  
- ✅ BuyWindow enhanced with context loading
- ⏳ MarketWatch integration pending
- ⏳ MainWindow signal/slot connections pending
- ⏳ SellWindow implementation pending
- ⏳ SnapQuoteWindow implementation pending
- ⏳ Preferences UI dialog pending
