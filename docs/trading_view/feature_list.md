# TradingView Advanced Charts - Feature Implementation List

**Library Version:** TradingView Charting Library v27.006  
**Communication:** QWebChannel (Full-Duplex Bidirectional)  
**Status:** Implementation Roadmap

---

## 🔄 Core Data Integration

### 1. XTS Candle Event Support
**Description:** Real-time candle streaming from XTS WebSocket feed  
**Implementation:**
- Connect to XTS MarketData WebSocket for OHLC candles
- Subscribe to symbol-specific candle streams (1min, 5min, 15min, etc.)
- Parse XTS candle format and convert to TradingView Bar format
- Handle candle completion events (when new candle starts)
- Support multiple timeframes simultaneously

As far as i know xts ohlc event gives 1 min ohlc data over socket for the subscribed tokens, once a minute. it is not live ohlc data , so live price reflection in candle to be implemented using live ltp(udp).also as it is 1 min candle ohlc if user rending bigger candle candle data adjustments is to be done progametically by aggregateing the candle data




**Data Flow:**
```
XTS WebSocket → C++ CandleAggregator → QWebChannel → JavaScript → TradingView Chart
```

**Technical Details:**
- XTS Event: `{"eventCode": "1501", "data": {...}}`
- Convert to TradingView format: `{time, open, high, low, close, volume}`
- Send via QWebChannel signal: `emit newCandleAvailable(QJsonObject)`

---

### 2. OHLC API Integration
**Description:** Fetch historical OHLC data from backend API  
**Implementation:**
- REST API endpoint for historical candles
- Support date range queries (from timestamp, to timestamp)
- Load initial 500-1000 candles on chart startup
- Implement pagination for loading older data
- Cache historical data in SQLite for faster subsequent loads

**API Endpoints:**
- `GET /api/ohlc?symbol=NIFTY&segment=2&timeframe=5m&from=<timestamp>&to=<timestamp>&limit=500`
- Response format: Array of `{time, open, high, low, close, volume}` objects

**C++ Implementation:**
```cpp
QVector<Candle> HistoricalDataStore::getCandles(
    QString symbol, int segment, QString timeframe,
    qint64 fromTime, qint64 toTime, int limit = 500
);
```

---

### 3. Pagination Support
**Description:** Load-on-scroll historical data pagination  
**Implementation:**
- Detect when user scrolls to chart beginning (via TradingView's `onVisibleRangeChanged`)
- Automatically fetch next batch of historical candles (500-1000 bars)
- Smooth loading without chart disruption
- Show loading indicator during data fetch
- Handle "no more data" scenario gracefully

**JavaScript Integration:**
```javascript
// TradingView datafeed getBars() method
getBars: function(symbolInfo, resolution, periodParams, onHistoryCallback, onErrorCallback) {
    // Request from C++ via QWebChannel
    dataBridge.requestHistoricalData(
        symbolInfo.name,
        resolution,
        periodParams.from,
        periodParams.to,
        periodParams.countBack
    );
}
```

---

### 4. Real-time Candle Updates (UDP Broadcast)
**Description:** Update current candle tick-by-tick from UDP market data  
**Implementation:**
- Listen to UDP broadcast packets (NSE/BSE market data)
- Parse tick data: `{token, ltp, volume, timestamp}`
- Update current candle bar in real-time (not yet completed)
- When candle completes (timeframe boundary), start new candle
- Emit partial candle updates every 100-500ms to avoid overloading chart

**UDP → Candle Update Flow:**
```
UDP Tick → MarketDataService::onTick() 
         → CandleAggregator::updateCurrentCandle()
         → emit candleUpdate(symbol, partialCandle)
         → JavaScript → TradingView.update(candle)
```

**Optimization:**
- Batch multiple ticks before updating chart (reduce CPU usage)
- Only update visible symbols (don't process background charts)
- Throttle updates to 5-10 updates per second per chart

---

## 📊 Custom Indicators

### 5. Custom Indicator Support
**Description:** Add proprietary indicators with custom calculations  
**Implementation Strategies:**

#### **Strategy A: C++ Calculation + JavaScript Display**
**Pros:** Code protection, fast calculations, access to C++ data  
**Cons:** Requires QWebChannel bridge setup

```cpp
// C++ side - Indicator calculation
class IndicatorEngine {
public:
    QVector<double> calculateATMStrategy(
        const QVector<Candle>& candles,
        double strikePrice,
        int period
    ) {
        // Your proprietary algorithm (stays compiled/hidden)
        QVector<double> signals;
        for (const auto& candle : candles) {
            double signal = computeATMLogic(candle, strikePrice, period);
            signals.append(signal);
        }
        return signals;
    }
};

// Bridge to JavaScript
void TradingViewBridge::addIndicator(QString name, QString type, QVariantMap params) {
    auto values = indicatorEngine->calculate(type, params);
    emit indicatorCalculated(name, convertToJsonArray(values));
}
```

```javascript
// JavaScript side - Display only
dataBridge.indicatorCalculated.connect(function(name, dataArray) {
    const lineSeries = chart.addLineSeries({
        color: '#2196F3',
        lineWidth: 2,
        title: name
    });
    lineSeries.setData(dataArray);
});
```

#### **Strategy B: JavaScript Custom Indicators (TradingView Native)**
**Pros:** Full TradingView integration, easy to update  
**Cons:** Code visible in browser, less secure

```javascript
custom_indicators_getter: function(PineJS) {
    return Promise.resolve([{
        name: 'My Custom RSI',
        metainfo: {
            id: "CustomRSI@tv-1",
            description: "Custom RSI with ATM logic",
            inputs: [
                { id: "period", name: "Period", type: "integer", defval: 14 },
                { id: "overbought", name: "Overbought", type: "integer", defval: 70 }
            ],
            plots: [{ id: "plot_0", type: "line" }],
            defaults: {
                styles: {
                    plot_0: { color: "#2196F3", linewidth: 2 }
                }
            }
        },
        constructor: function() {
            this.main = function(context, inputCallback) {
                var period = inputCallback(0);
                var close = PineJS.Std.close(context);
                var rsi = PineJS.Std.rsi(close, period, context);
                // Add custom logic here
                return [rsi];
            };
        }
    }]);
}
```

**Supported Indicators:**
- ✅ Moving Averages (SMA, EMA, WMA)
- ✅ Oscillators (RSI, MACD, Stochastic)
- ✅ Volatility (ATR, Bollinger Bands, Standard Deviation)
- ✅ Volume indicators (VWAP, OBV, Volume Profile)
- ✅ Custom proprietary indicators (ATM strategies, Greeks-based signals)

---

## 💾 Study & Template Management

### 6. Save Studies (Indicator Presets)
**Description:** Save indicator configurations for later use  
**Implementation:**
- Use TradingView's `chart.createStudyTemplate()` API
- Store study templates in SQLite database or JSON files
- Link templates to user profiles
- Support quick-load from favorites

**Database Schema:**
```sql
CREATE TABLE study_templates (
    id INTEGER PRIMARY KEY,
    user_id INTEGER,
    name TEXT NOT NULL,
    description TEXT,
    template_data TEXT, -- JSON of indicator settings
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(user_id, name)
);
```

**JavaScript API:**
```javascript
// Save current indicators as template
const template = widget.activeChart().createStudyTemplate({
    saveInterval: true,
    saveSymbol: false
});

// Send to C++ for storage
dataBridge.saveStudyTemplate(templateName, JSON.stringify(template));
```

---

### 7. Template Save & Load
**Description:** Save entire chart layouts with multiple indicators  
**Implementation:**
- Chart templates include: indicators, drawings, timeframe, symbol
- Store in database with user-friendly names
- Quick-access dropdown in toolbar
- Auto-save feature (save on close)

**Features:**
- ✅ Save entire chart state (indicators + drawings + settings)
- ✅ Load template instantly
- ✅ Share templates between users (export/import)
- ✅ Default templates for common strategies (e.g., "ATM Strategy", "Scalping Setup")

**C++ Implementation:**
```cpp
class ChartTemplateManager {
public:
    void saveTemplate(int userId, QString name, QString templateJson);
    QString loadTemplate(int userId, QString name);
    QStringList listTemplates(int userId);
    void deleteTemplate(int userId, QString name);
    void exportTemplate(QString name, QString filePath); // Save as JSON file
    void importTemplate(QString filePath); // Load from JSON file
};
```

---

## 🎯 Trading Integration

### 8. Buy/Sell Icons Based on Indicators
**Description:** Visual markers on chart when indicator signals occur  
**Implementation:**
- Monitor indicator values (e.g., RSI crosses 30/70, MACD crossover)
- Automatically add markers to chart at signal points
- Different colors/shapes for buy vs sell
- Markers clickable to show signal details

**Signal Detection:**
```cpp
class SignalDetector : public QObject {
    Q_OBJECT
signals:
    void buySignal(QString symbol, double price, qint64 timestamp, QString reason);
    void sellSignal(QString symbol, double price, qint64 timestamp, QString reason);
    
    void processIndicatorUpdate(QString indicator, double value) {
        // Detect crossovers, thresholds, etc.
        if (indicator == "RSI" && value < 30 && previousRSI >= 30) {
            emit buySignal(currentSymbol, currentPrice, 
                          QDateTime::currentSecsSinceEpoch(), 
                          "RSI oversold (< 30)");
        }
    }
};
```

**Chart Marker Display:**
```javascript
dataBridge.buySignal.connect(function(symbol, price, timestamp, reason) {
    chart.addMarker({
        time: timestamp,
        position: 'belowBar',
        color: '#26a69a',
        shape: 'arrowUp',
        text: 'BUY: ' + reason,
        size: 2
    });
});

dataBridge.sellSignal.connect(function(symbol, price, timestamp, reason) {
    chart.addMarker({
        time: timestamp,
        position: 'aboveBar',
        color: '#ef5350',
        shape: 'arrowDown',
        text: 'SELL: ' + reason,
        size: 2
    });
});
```

---

### 9. Trigger Buy/Sell Orders Based on Indicators
**Description:** Automated order placement when indicator conditions are met  
**Implementation:**
- Define trigger conditions (candle crossover, indicator threshold, pattern recognition)
- Auto-generate order when condition triggers
- User confirmation dialog (optional - can be disabled for algo trading)
- Log all automated orders with trigger reason

**Trigger Types:**
1. **Candle Crossover:**
   - Price crosses moving average
   - Candle closes above/below resistance/support

2. **Indicator Threshold:**
   - RSI < 30 (oversold → BUY)
   - RSI > 70 (overbought → SELL)
   - MACD histogram crossover

3. **Pattern Recognition:**
   - Bullish engulfing → BUY
   - Bearish engulfing → SELL
   - Doji at support/resistance

4. **Multi-Indicator Confluence:**
   - RSI oversold + Price at support + MACD bullish crossover → Strong BUY

**Implementation:**
```cpp
class AutoTradingEngine : public QObject {
public:
    struct TriggerRule {
        QString name;
        QString symbol;
        QString condition; // "RSI < 30 AND MACD > SIGNAL"
        QString action;    // "BUY" or "SELL"
        int quantity;
        QString orderType; // "MARKET", "LIMIT"
        double limitPrice; // Optional
        bool enabled;
    };
    
    void addTriggerRule(const TriggerRule& rule);
    void evaluateTriggers(); // Called on every candle update
    
signals:
    void triggerFired(QString triggerName, QString action);
    void orderPlaced(qint64 orderId, QString symbol, QString side, int qty);
    
private:
    QVector<TriggerRule> m_triggers;
    
    void evaluateTriggers() {
        for (const auto& trigger : m_triggers) {
            if (!trigger.enabled) continue;
            
            bool conditionMet = evaluateCondition(trigger.condition);
            if (conditionMet) {
                emit triggerFired(trigger.name, trigger.action);
                
                // Place order via OrderManager
                qint64 orderId = OrderManager::instance().placeOrder(
                    trigger.symbol,
                    getSegmentForSymbol(trigger.symbol),
                    trigger.action, // BUY/SELL
                    trigger.quantity,
                    trigger.limitPrice,
                    trigger.orderType
                );
                
                emit orderPlaced(orderId, trigger.symbol, trigger.action, trigger.quantity);
                
                // Optionally disable trigger after firing (one-shot)
                if (trigger.oneShot) {
                    disableTrigger(trigger.name);
                }
            }
        }
    }
};
```

**UI for Trigger Management:**
- Add/Edit/Delete triggers
- Enable/Disable triggers
- View trigger history
- Test triggers without placing real orders (dry run mode)

---

## 🔗 Event Communication (QWebChannel)

### 10. Event Emission to C++
**Description:** Full bidirectional communication between JavaScript and C++  

#### **QWebChannel IS Full-Duplex (Both Ways)**

**JavaScript → C++ (Invoke C++ methods):**
```javascript
// Chart click event
chart.subscribeClick(function(param) {
    const price = candleSeries.coordinateToPrice(param.point.y);
    const time = param.time;
    
    // Call C++ method via QWebChannel
    dataBridge.onChartClick(price, time);
});

// User draws horizontal line
chart.onDrawingCreated(function(drawingId) {
    const drawingInfo = chart.getDrawing(drawingId);
    dataBridge.onDrawingCreated(drawingId, drawingInfo.price, drawingInfo.type);
});

// Timeframe changed
chart.onIntervalChanged(function(interval) {
    dataBridge.onTimeframeChanged(interval);
});
```

**C++ → JavaScript (Emit signals from C++):**
```cpp
class TradingViewBridge : public QObject {
    Q_OBJECT
    
public:
    // Methods callable from JavaScript (Q_INVOKABLE)
    Q_INVOKABLE void onChartClick(double price, qint64 timestamp) {
        qDebug() << "Chart clicked at price:" << price << "time:" << timestamp;
        // Show order entry dialog at this price
        showOrderDialog(price);
    }
    
    Q_INVOKABLE void onDrawingCreated(QString drawingId, double price, QString type) {
        // User drew resistance line - could create alert
        if (type == "horizontal_line") {
            createPriceAlert(price);
        }
    }
    
    Q_INVOKABLE void onTimeframeChanged(QString interval) {
        // User changed to 5min chart - reload data
        loadHistoricalData(currentSymbol, interval);
    }
    
signals:
    // Signals to JavaScript (emit from C++)
    void newCandleAvailable(QJsonObject candle);
    void indicatorCalculated(QString name, QJsonArray values);
    void orderExecuted(qint64 orderId, double price, QString status);
    void alertTriggered(QString message, QString severity);
    void symbolChanged(QString symbol, int segment);
};

// Emit from anywhere in C++ code
emit dataBridge->newCandleAvailable(candle.toJson());
```

**Supported Events (Both Directions):**

| Event Type | Direction | Description |
|------------|-----------|-------------|
| Chart Click | JS → C++ | User clicks on chart to place order |
| Drawing Created | JS → C++ | User draws line/shape on chart |
| Symbol Changed | JS → C++ | User changes active symbol |
| Timeframe Changed | JS → C++ | User changes chart interval |
| Crosshair Move | JS → C++ | Mouse moves over chart (for tooltips) |
| New Candle | C++ → JS | Send new OHLC bar to chart |
| Indicator Update | C++ → JS | Send calculated indicator values |
| Order Executed | C++ → JS | Show order execution marker |
| Alert Triggered | C++ → JS | Display notification on chart |
| Position Update | C++ → JS | Update P&L overlay |
| Market Depth | C++ → JS | Update order book display |

---

## 🎨 Advanced Features

### Custom Chart Interactions

#### **Order Placement from Chart:**
1. User clicks on chart at desired price
2. C++ receives click event with price/time
3. Show order entry dialog pre-filled with price
4. User confirms → Place order
5. Show order marker on chart

#### **Drag-to-Modify Orders:**
1. Display pending orders as draggable lines
2. User drags line to new price
3. JavaScript detects drag → Calls C++ modify order
4. Update order in backend
5. Confirm modification on chart

#### **Visual Stop-Loss / Target:**
1. After order execution, show position line
2. Allow drag handles for SL/Target price adjustment
3. Real-time P&L calculation as price moves
4. Color-coded: Green (profit), Red (loss)

---

## 🔧 Technical Implementation Notes

### QWebChannel Setup

**C++ Side:**
```cpp
// In TradingViewChartWidget constructor
m_webView = new QWebEngineView(this);
m_channel = new QWebChannel(this);
m_bridge = new TradingViewBridge(this);

m_channel->registerObject("dataBridge", m_bridge);
m_webView->page()->setWebChannel(m_channel);
m_webView->setUrl(QUrl("qrc:/html/tradingview_chart.html"));
```

**JavaScript Side:**
```javascript
new QWebChannel(qt.webChannelTransport, function(channel) {
    window.dataBridge = channel.objects.dataBridge;
    
    // Now you can use dataBridge for both directions:
    
    // Call C++ methods
    dataBridge.placeOrder("NIFTY", 50, 26000, "BUY");
    
    // Listen to C++ signals
    dataBridge.orderExecuted.connect(function(orderId, price, status) {
        showNotification("Order " + orderId + " executed at " + price);
    });
});
```

---

## 📋 Implementation Priority

**Phase 1 (Foundation):**
1. ✅ OHLC API integration
2. ✅ Real-time candle updates (UDP)
3. ✅ QWebChannel bidirectional setup

**Phase 2 (Core Features):**
4. Custom indicators (C++ calculation)
5. Study/template save & load
6. Buy/sell signal markers

**Phase 3 (Trading Integration):**
7. Chart-based order placement
8. Automated trading triggers
9. Visual order management (drag to modify)

**Phase 4 (Advanced):**
10. Pattern recognition
11. Multi-indicator confluence alerts
12. Strategy backtesting on chart

---

## ✅ Success Criteria

- [ ] Chart loads historical data in < 2 seconds
- [ ] Real-time updates with < 100ms latency
- [ ] Indicators calculate instantly (< 50ms)
- [ ] Orders placed from chart in < 1 second
- [ ] Templates save/load in < 500ms
- [ ] Automated triggers fire within 1 candle period
- [ ] Zero data loss during high-frequency UDP updates
- [ ] Chart remains smooth at 60 FPS during real-time updates

---

**Last Updated:** February 12, 2026  
**Library Version:** TradingView Charting Library v27.006  
**Integration Status:** Ready for Full Implementation



 place limit order or sl order on chart 
 
