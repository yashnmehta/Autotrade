# Strategy Manager - Current Working Analysis

**Date:** February 10, 2026  
**Version:** 1.0  
**Status:** Production Analysis

---

## Executive Summary

This document provides a comprehensive analysis of the current Strategy Manager implementation, identifying its strengths, limitations, and opportunities for enhancement toward advanced features like TradingView integration, custom indicators, strategy builder, and plugin architecture.

---

## Current Architecture Analysis

### 1. Strategy Manager Core

**Components:**
```
StrategyService (Singleton)
    ├── Instance Management (QHash<qint64, StrategyInstance>)
    ├── Lifecycle Control (Create/Start/Pause/Resume/Stop/Delete)
    ├── 500ms Update Timer (Metric batching)
    ├── State Machine Enforcement (6 states)
    └── Active Strategy Map (QHash<qint64, StrategyBase*>)

StrategyRepository (SQLite)
    ├── CRUD Operations
    ├── Soft Delete Support
    └── JSON Parameter Storage

StrategyTableModel (Qt Model)
    ├── 15 Column Display
    ├── Delta Updates
    └── Signal-based Refresh

StrategyFilterProxyModel
    ├── Type Filtering
    └── Status Filtering
```

**Strengths:**
- ✅ Clean separation of concerns
- ✅ Event-driven architecture
- ✅ Persistent state management
- ✅ Thread-safe instance access
- ✅ Deterministic lifecycle control

**Limitations:**
- ❌ Hardcoded strategy types (JodiATM, TSpecial, etc.)
- ❌ No runtime strategy modification
- ❌ Limited parameter validation
- ❌ No visual backtesting
- ❌ No indicator integration
- ❌ Monolithic strategy implementation

---

### 2. Strategy Implementation Pattern

**Current Flow:**
```cpp
class JodiATMStrategy : public StrategyBase {
    // Hard-coded logic
    void init(const StrategyInstance& instance) override {
        m_offset = getParameter<double>("offset", 0.0);
        // ... parameter extraction
    }
    
    void start() override {
        // Subscribe to feeds
        FeedHandler::instance().subscribe(segment, token, this, &onTick);
    }
    
    void onTick(const UDP::MarketTick& tick) override {
        // Decision logic
        if (tick.ltp > m_blDP) {
            // Execute trade
        }
    }
};
```

**Analysis:**

| Aspect | Current State | Ideal State |
|--------|---------------|-------------|
| **Logic Location** | Hard-coded in C++ | Configurable/Scriptable |
| **Parameter Schema** | Freeform QVariantMap | Validated JSON Schema |
| **Backtesting** | None | Visual + Historical |
| **Indicators** | Manual calculations | Library-based |
| **Order Execution** | Direct FeedHandler | Abstracted Order API |
| **Error Handling** | Basic logs | Comprehensive error states |

---

### 3. Data Flow Analysis

**Current Data Pipeline:**

```
UDP Broadcast (NSE/BSE)
    ↓
UdpBroadcastService (Background Thread)
    ↓
FeedHandler::onUDPPacket()
    ↓
PriceStore::updatePrice() [Zero-copy cache]
    ↓
emit tickReceived(token, tick)
    ↓
Strategy::onTick() [Direct subscription]
    ↓
Order Decision Logic
    ↓
OrderManager::placeOrder() [Currently stubbed]
    ↓
TradingDataService (Position tracking)
    ↓
StrategyService::onUpdateTick() [500ms batched]
    ↓
emit instanceUpdated()
    ↓
UI Table Update (Delta repaint)
```

**Bottlenecks Identified:**
1. **No Indicator Layer:** Strategies recalculate indicators per tick
2. **No Historical Data:** Cannot backtest or replay
3. **No Chart Visualization:** Blind trading without visual feedback
4. **No Order Simulation:** Cannot paper trade effectively

---

### 4. JodiATM Strategy Deep Dive

**Current Implementation:**

```cpp
State Variables:
- Trend (Nutral/Bullish/Bearish)
- currentLeg (0-4)
- currentATM
- Decision Points (blDP, brDP, reversalP)
- Reset Boundaries (blRCP, brRCP)
- Tokens (cashToken, ceToken, peToken)

Logic Flow:
1. ATM Reset → Calculate thresholds
2. Subscribe to cash/futures feed
3. onTick() → Check price vs decision points
4. Leg progression: Neutral → Leg 1 → Leg 2 → Leg 3 → Leg 4
5. Reversal detection: Fall back to previous leg
6. Boundary breach: Reset ATM, start over

Parameters:
- offset: 10.0 (distance from ATM)
- threshold: 15.0 (buffer before trigger)
- adj_pts: 0.0-5.0 (fine-tuning)
- diff_points: 50/100 (strike interval)
- is_trailing: bool (trailing SL)
```

**Complexity Analysis:**
- **Lines of Code:** ~220 (JodiATMStrategy.cpp)
- **Decision Points:** 8 (blDP, brDP, reversalP, blRCP, brRCP + trend checks)
- **Conditional Branches:** 15+ (nested if-else)
- **State Transitions:** 9 (Neutral↔Bullish↔Bearish, Leg 0-4)

**Reusability Score:** 30%
- Tightly coupled to straddle logic
- Hard to extract common patterns
- Difficult to modify without deep understanding

---

## Gap Analysis for Advanced Features

### Feature 1: TradingView Chart Integration

**Current State:** ❌ No charting capability

**Requirements for Integration:**
1. **Web View Component:**
   - QWebEngineView widget in Qt
   - Bridge between C++ and JavaScript
   - Bidirectional communication (C++ ↔ JS)

2. **Data Feed:**
   - Convert UDP ticks to TradingView format
   - Historical OHLCV data storage
   - Real-time tick-to-candle aggregation

3. **Custom Indicators:**
   - JavaScript indicator API
   - C++ indicator calculation engine
   - Indicator value streaming to chart

4. **Trading Integration:**
   - Order entry from chart
   - Position visualization on chart
   - Alert/notification system

**Missing Components:**
- ❌ QWebEngineView integration
- ❌ Historical data storage (TimescaleDB/SQLite)
- ❌ OHLCV aggregator
- ❌ WebSocket server for chart updates
- ❌ TradingView widget license (paid)

---

### Feature 2: Custom Indicator System

**Current State:** ❌ Manual calculations in strategies

**Proposed Architecture:**
```cpp
class IndicatorBase {
public:
    virtual void update(double price, double volume, QDateTime timestamp) = 0;
    virtual QVariant value() const = 0;
    virtual QString name() const = 0;
};

class IndicatorLibrary {
public:
    // Built-in indicators
    Indicator* createSMA(int period);
    Indicator* createEMA(int period);
    Indicator* createRSI(int period);
    Indicator* createBollingerBands(int period, double stddev);
    Indicator* createMACD(int fast, int slow, int signal);
    Indicator* createATR(int period);
    
    // Custom indicators (user-defined)
    Indicator* createCustom(const QString& script);
};

class IndicatorEngine {
public:
    void registerIndicator(const QString& symbol, Indicator* indicator);
    void updateIndicators(const UDP::MarketTick& tick);
    QVariant getIndicatorValue(const QString& symbol, const QString& indicatorName);
};
```

**Benefits:**
- ✅ Reusable across strategies
- ✅ Centralized calculation
- ✅ Cacheable results
- ✅ Testable independently

**Missing Components:**
- ❌ Indicator calculation library (TA-Lib or custom)
- ❌ Indicator registry system
- ❌ Historical data buffer (ring buffer)
- ❌ Indicator script interpreter (ChaiScript/Lua)

---

### Feature 3: Strategy Builder (Visual/Scriptable)

**Current State:** ❌ Hard-coded C++ strategies only

**Vision:**
```
Strategy Builder UI
    ├── Visual Flow Editor (Nodes + Connections)
    ├── Condition Builder (If-Then-Else)
    ├── Action Builder (Buy/Sell/Modify)
    ├── Indicator Palette (Drag & Drop)
    ├── Backtesting Panel (Historical replay)
    └── Export to Script/C++
```

**Proposed Strategy Skeleton:**
```cpp
class StrategyTemplate {
protected:
    // Entry conditions
    virtual bool shouldEnter() = 0;
    
    // Exit conditions
    virtual bool shouldExit() = 0;
    
    // Position sizing
    virtual int calculateQuantity() = 0;
    
    // Risk management
    virtual double calculateStopLoss() = 0;
    virtual double calculateTarget() = 0;
    
    // Lifecycle hooks
    virtual void onEntry() {}
    virtual void onExit() {}
    virtual void onTick(const MarketData& data) {}
    virtual void onOrder(const OrderUpdate& update) {}
};

// Example: User-defined strategy
class MyCustomStrategy : public StrategyTemplate {
    bool shouldEnter() override {
        return getIndicator("RSI") < 30 
            && getIndicator("MACD") > getIndicator("MACD_SIGNAL")
            && getCurrentPrice() > getIndicator("SMA_200");
    }
    
    bool shouldExit() override {
        return getIndicator("RSI") > 70 
            || getCurrentMTM() > m_targetProfit;
    }
    
    int calculateQuantity() override {
        return static_cast<int>(m_capitalPerTrade / getCurrentPrice());
    }
};
```

**Missing Components:**
- ❌ Visual node editor (QGraphicsView or web-based)
- ❌ Strategy scripting language (Python/Lua/ChaiScript)
- ❌ Script → C++ compiler/interpreter
- ❌ Sandbox execution environment
- ❌ Strategy validation framework

---

### Feature 4: Client DLL Plugin System

**Current State:** ❌ Monolithic binary

**Proposed Architecture:**

**DLL Interface (Exported from main app):**
```cpp
// StrategyPluginAPI.h (exported header)
#define STRATEGY_API __declspec(dllexport)

class STRATEGY_API IMarketData {
public:
    virtual double getPrice(const QString& symbol) = 0;
    virtual double getIndicator(const QString& name) = 0;
    virtual QDateTime getTimestamp() = 0;
};

class STRATEGY_API IOrderAPI {
public:
    virtual qint64 placeOrder(const QString& symbol, int qty, double price) = 0;
    virtual bool cancelOrder(qint64 orderId) = 0;
    virtual bool modifyOrder(qint64 orderId, int newQty, double newPrice) = 0;
};

class STRATEGY_API IStrategyContext {
public:
    virtual IMarketData* marketData() = 0;
    virtual IOrderAPI* orderAPI() = 0;
    virtual void log(const QString& message) = 0;
    virtual QVariant getParameter(const QString& key) = 0;
};

// Client implementations implement this
class STRATEGY_API IStrategy {
public:
    virtual ~IStrategy() = default;
    virtual void init(IStrategyContext* context) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void onTick() = 0;
    virtual QString name() const = 0;
    virtual QString version() const = 0;
};

// DLL export function
extern "C" STRATEGY_API IStrategy* createStrategy();
```

**Client DLL Implementation:**
```cpp
// MyCustomStrategy.dll (client's code)
#include "StrategyPluginAPI.h"

class MyPrivateStrategy : public IStrategy {
private:
    IStrategyContext* m_context;
    double m_threshold;
    
public:
    void init(IStrategyContext* context) override {
        m_context = context;
        m_threshold = context->getParameter("threshold").toDouble();
        m_context->log("MyPrivateStrategy initialized");
    }
    
    void onTick() override {
        double price = m_context->marketData()->getPrice("NIFTY");
        double rsi = m_context->marketData()->getIndicator("RSI_14");
        
        if (rsi < 30 && price > m_threshold) {
            m_context->orderAPI()->placeOrder("NIFTY", 50, price);
        }
    }
    
    QString name() const override { return "MyPrivateStrategy"; }
    QString version() const override { return "1.0.0"; }
};

extern "C" __declspec(dllexport) IStrategy* createStrategy() {
    return new MyPrivateStrategy();
}
```

**Main App Loading:**
```cpp
class PluginLoader {
public:
    IStrategy* loadStrategy(const QString& dllPath) {
        QLibrary library(dllPath);
        
        if (!library.load()) {
            qWarning() << "Failed to load DLL:" << library.errorString();
            return nullptr;
        }
        
        typedef IStrategy* (*CreateStrategyFunc)();
        CreateStrategyFunc createFunc = 
            (CreateStrategyFunc)library.resolve("createStrategy");
        
        if (!createFunc) {
            qWarning() << "DLL missing createStrategy() function";
            return nullptr;
        }
        
        return createFunc();
    }
};
```

**Benefits:**
- ✅ Client code remains private (in DLL)
- ✅ No source code exposure
- ✅ Hot-reload capability
- ✅ Version control per DLL
- ✅ Sandboxed execution
- ✅ Easy distribution (single DLL file)

**Missing Components:**
- ❌ Stable C API definition
- ❌ Plugin manager system
- ❌ DLL signature verification (security)
- ❌ Sandbox/permission system
- ❌ DLL versioning compatibility checks
- ❌ Memory isolation (crash containment)

---

## Integration Complexity Matrix

| Feature | Complexity | Effort (Person-weeks) | Dependencies |
|---------|------------|----------------------|--------------|
| **TradingView Charts** | High | 6-8 weeks | QWebEngine, WebSocket, Historical DB |
| **Custom Indicators** | Medium | 4-6 weeks | TA-Lib, Ring Buffer, Indicator Registry |
| **Strategy Builder** | Very High | 10-14 weeks | Script Engine, Visual Editor, Compiler |
| **DLL Plugin System** | Medium | 4-6 weeks | Stable API, Plugin Manager, Security |

---

## Recommended Implementation Phases

### Phase 1: Foundation (Weeks 1-6)
**Goals:**
- Implement Historical Data Storage (TimescaleDB or SQLite with OHLCV tables)
- Create OHLCV Aggregator (tick → 1m/5m/15m candles)
- Build Indicator Engine with TA-Lib integration
- Define Stable Strategy API for plugins

**Deliverables:**
- ✅ 1-year historical OHLCV data per symbol
- ✅ Real-time candle aggregation
- ✅ 15+ common indicators (SMA, EMA, RSI, MACD, Bollinger Bands)
- ✅ StrategyPluginAPI.h header file

---

### Phase 2: Visualization (Weeks 7-14)
**Goals:**
- Integrate QWebEngineView in main window
- Embed TradingView Charting Library (or Lightweight Charts)
- Stream real-time candles to chart
- Display indicators on chart
- Add order execution from chart

**Deliverables:**
- ✅ Live updating charts (1m/5m/15m timeframes)
- ✅ Indicator overlays
- ✅ Order markers on chart
- ✅ Position P&L visualization

---

### Phase 3: Strategy Builder (Weeks 15-28)
**Goals:**
- Design Strategy Skeleton/Template system
- Implement Visual Flow Editor (optional: use web-based)
- Integrate scripting language (Python via PyBind11 or Lua)
- Build strategy validation & backtesting engine
- Create strategy marketplace/library UI

**Deliverables:**
- ✅ Drag-drop strategy builder
- ✅ Script-based strategy creation
- ✅ Backtest on historical data
- ✅ Strategy performance metrics

---

### Phase 4: Plugin System (Weeks 29-34)
**Goals:**
- Finalize DLL API specification
- Implement PluginManager with hot-reload
- Add DLL signature verification
- Build plugin installer/updater
- Create developer documentation & SDK

**Deliverables:**
- ✅ Stable Plugin API v1.0
- ✅ Plugin Manager UI
- ✅ SDK with examples
- ✅ Developer documentation

---

## Technical Recommendations

### For TradingView Integration

**Option 1: TradingView Charting Library (Paid)**
- **Cost:** $3,500-$5,000 one-time
- **Pros:** Full-featured, professional, customizable
- **Cons:** Expensive, licensing restrictions

**Option 2: TradingView Lightweight Charts (Free)**
- **Cost:** Free (Open Source - Apache 2.0)
- **Pros:** Free, lightweight, sufficient for basic charts
- **Cons:** Limited features compared to paid version
- **Recommendation:** ✅ Start with this

**Option 3: ChartJS with Custom Extensions (Free)**
- **Cost:** Free
- **Pros:** Fully customizable, good for custom indicators
- **Cons:** More development work

**Implementation:**
```cpp
// In Qt application
QWebEngineView* chartView = new QWebEngineView(this);
chartView->setHtml(generateChartHTML());

// JavaScript bridge
QWebChannel* channel = new QWebChannel(this);
channel->registerObject("chartBridge", new ChartBridge(this));
chartView->page()->setWebChannel(channel);

// Stream data
void ChartBridge::updateCandle(double open, double high, double low, double close, qint64 timestamp) {
    QJsonObject candle;
    candle["time"] = timestamp / 1000; // TradingView uses seconds
    candle["open"] = open;
    candle["high"] = high;
    candle["low"] = low;
    candle["close"] = close;
    
    emit newCandle(candle);
}
```

---

### For Custom Indicators

**Recommended Library: TA-Lib**
```cpp
#include <ta-lib/ta_libc.h>

class TALibIndicator : public IndicatorBase {
public:
    QVector<double> calculateSMA(const QVector<double>& prices, int period) {
        int outBegin, outNbElement;
        QVector<double> output(prices.size());
        
        TA_MA(
            0, prices.size() - 1,
            prices.constData(),
            period,
            TA_MAType_SMA,
            &outBegin,
            &outNbElement,
            output.data()
        );
        
        return output;
    }
};
```

**Alternative: Custom Implementation**
- Pros: No external dependency, full control
- Cons: More development time, potential bugs
- Use for: Custom proprietary indicators

---

### For Strategy Builder

**Scripting Language Comparison:**

| Language | Integration | Performance | Ease of Use | Sandboxing |
|----------|-------------|-------------|-------------|------------|
|**Python (PyBind11)**| Excellent | Medium | Excellent | Medium |
| **Lua** | Excellent | High | Good | Excellent |
| **ChaiScript** | Good | Medium | Good | Excellent |
| **JavaScript (V8)** | Good | High | Excellent | Good |

**Recommendation:** ✅ **ChaiScript** for initial implementation
- Header-only library
- C++-like syntax
- Easy sandboxing
- Good performance for strategies

**Example:**
```cpp
#include <chaiscript/chaiscript.hpp>

chaiscript::ChaiScript chai;

// Expose API to scripts
chai.add(chaiscript::fun(&IStrategyContext::getPrice), "getPrice");
chai.add(chaiscript::fun(&IStrategyContext::getIndicator), "getIndicator");
chai.add(chaiscript::fun(&IStrategyContext::placeOrder), "placeOrder");

// Load user script
chai.eval_file("user_strategy.chai");

// Call user-defined function
try {
    auto shouldEnter = chai.eval<std::function<bool()>>("shouldEnter");
    if (shouldEnter()) {
        // Execute entry logic
    }
} catch (const chaiscript::exception::eval_error& e) {
    qWarning() << "Script error:" << e.what();
}
```

---

## Security Considerations

### For DLL Plugin System

**Threats:**
1. Malicious DLL injection
2. Memory corruption
3. Data exfiltration
4. Resource exhaustion

**Mitigations:**
1. **DLL Signing:**
   ```cpp
   bool verifyDLLSignature(const QString& dllPath) {
       // Use Windows Authenticode verification
       // Or custom RSA signature scheme
   }
   ```

2. **Sandboxing:**
   - Run plugins in separate process
   - Use IPC for communication
   - Limit file system access

3. **Resource Limits:**
   - CPU time per tick (e.g., 50ms max)
   - Memory allocation limit (e.g., 100MB per plugin)
   - Order rate limiting

4. **API Permissions:**
   - Read-only data access by default
   - Require explicit permission for order placement
   - Audit log all plugin actions

---

## Current Codebase Strengths

✅ **Clean Architecture:** Well-separated concerns  
✅ **State Management:** Robust state machine  
✅ **Data Pipeline:** Efficient UDP → Strategy flow  
✅ **Thread Safety:** QMutex protection  
✅ **Persistence:** SQLite with soft deletes  
✅ **UI Responsiveness:** Delta updates, 500ms batching  

---

## Current Codebase Weaknesses

❌ **Hardcoded Strategies:** Difficult to add new logic  
❌ **No Historical Data:** Cannot backtest or visualize  
❌ **Limited Indicators:** Manual calculations only  
❌ **No Visual Feedback:** Blind trading  
❌ **Monolithic Build:** Cannot extend without recompiling  
❌ **Limited Parameter Validation:** Freeform QVariantMap  

---

## Conclusion

The current Strategy Manager provides a solid foundation for lifecycle management and real-time execution. However, to support advanced features like:

- TradingView chart integration
- Custom indicator system
- Visual strategy builder
- Client DLL plugins

Significant architectural enhancements are required across:
1. **Data Layer:** Historical storage, OHLCV aggregation
2. **Computation Layer:** Indicator engine, backtest engine
3. **Presentation Layer:** QWebEngineView, chart streaming
4. **Extensibility Layer:** Plugin API, script interpreter

**Estimated Total Effort:** 24-34 weeks for complete implementation

**Recommended Approach:** Incremental phases with working deliverables at each stage.

---

**Next Steps:**
1. Review and approve phased implementation plan
2. Select charting library (Lightweight Charts recommended)
3. Choose scripting language (ChaiScript recommended)
4. Define DLL API specification
5. Begin Phase 1 (Foundation)

---

**References:**
- [STRATEGY_MANAGER_FLOW_ANALYSIS.md](STRATEGY_MANAGER_FLOW_ANALYSIS.md)
- [JODIATM_STRATEGY_ANALYSIS.md](JODIATM_STRATEGY_ANALYSIS.md)
- [TradingView Lightweight Charts](https://github.com/tradingview/lightweight-charts)
- [TA-Lib Documentation](https://ta-lib.org/function.html)
- [ChaiScript](http://chaiscript.com/)
