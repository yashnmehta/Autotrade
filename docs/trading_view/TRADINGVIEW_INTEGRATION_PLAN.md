# TradingView Chart Integration - Implementation Plan

**Feature:** Embedded Real-time Charting with Custom Data  
**Date:** February 10, 2026  
**Priority:** HIGH  
**Estimated Effort:** 6-8 weeks

---

## Overview

Integrate professional charting capability into the trading terminal using Trade ViewLightweight Charts (open-source) to visualize:
- Real-time price feeds
- Custom indicators
- Strategy entry/exit points
- Position P&L tracking
- Multiple timeframes (1m, 5m, 15m, 1h, 1D)

---

## Architecture Design

### Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    MAIN APPLICATION (C++)                    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ UDP Broadcast│→ │  CandleAggr  │→ │ HistoricalDB │     │
│  │   Service    │  │   Service    │  │  (SQLite)    │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│           ↓                 ↓                 ↓             │
│  ┌────────────────────────────────────────────────────┐     │
│  │           ChartDataService (Singleton)            │     │
│  │  - Manages OHLCV data                             │     │
│  │  - Indicator calculations                          │     │
│  │  - Real-time updates                              │     │
│  └────────────────┬───────────────────────────────────┘     │
│                   │                                         │
│  ┌────────────────▼───────────────────────────────────┐     │
│  │         QWebEngineView (Chart Widget)             │     │
│  │  ┌─────────────────────────────────────────────┐  │     │
│  │  │       TradingView Lightweight Charts        │  │     │
│  │  │         (JavaScript/HTML/CSS)               │  │     │
│  │  └─────────────────────────────────────────────┘  │     │
│  └────────────────┬───────────────────────────────────┘     │
│                   │                                         │
│  ┌────────────────▼───────────────────────────────────┐     │
│  │          QWebChannel (C++ ↔ JS Bridge)            │     │
│  │  - sendCandle(ohlcv)                              │     │
│  │  - updateIndicator(name, values)                  │     │
│  │  - addMarker(order)                               │     │
│  │  - onChartClick(price, timestamp)                 │     │
│  └────────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Historical Data Storage

### Database Schema

```sql
-- OHLCV Candles Table
CREATE TABLE IF NOT EXISTS candles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL,
    segment INTEGER NOT NULL,
    timeframe TEXT NOT NULL,  -- '1m', '5m', '15m', '1h', '1D'
    timestamp INTEGER NOT NULL,  -- Unix epoch (seconds)
    open REAL NOT NULL,
    high REAL NOT NULL,
    low REAL NOT NULL,
    close REAL NOT NULL,
    volume INTEGER DEFAULT 0,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(symbol, segment, timeframe, timestamp)
);

CREATE INDEX idx_candles_lookup 
    ON candles(symbol, segment, timeframe, timestamp);

-- Indicator Values Table
CREATE TABLE IF NOT EXISTS indicator_values (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    candle_id INTEGER NOT NULL,
    indicator_name TEXT NOT NULL,
    value REAL NOT NULL,
    metadata TEXT,  -- JSON for multi-value indicators (e.g., Bollinger Bands)
    FOREIGN KEY(candle_id) REFERENCES candles(id) ON DELETE CASCADE
);

CREATE INDEX idx_indicator_lookup 
    ON indicator_values(candle_id, indicator_name);
```

### C++ Implementation

```cpp
// include/services/HistoricalDataStore.h
class HistoricalDataStore : public QObject {
    Q_OBJECT
public:
    static HistoricalDataStore& instance();
    
    // Write operations
    void saveCandle(const QString& symbol, int segment, 
                    const QString& timeframe, const Candle& candle);
    void saveCandleBatch(const QString& symbol, int segment,
                         const QString& timeframe, 
                         const QVector<Candle>& candles);
    
    // Read operations
    QVector<Candle> getCandles(const QString& symbol, int segment,
                               const QString& timeframe,
                               qint64 startTime, qint64 endTime);
    QVector<Candle> getRecentCandles(const QString& symbol, int segment,
                                     const QString& timeframe, int count);
    
    // Indicator storage
    void saveIndicatorValue(qint64 candleId, const QString& indicatorName,
                           double value, const QVariantMap& metadata = {});
    QVector<double> getIndicatorValues(const QString& symbol, int segment,
                                        const QString& timeframe,
                                        const QString& indicatorName,
                                        qint64 startTime, qint64 endTime);
    
signals:
    void candleSaved(const QString& symbol, const Candle& candle);
    
private:
    QSqlDatabase m_db;
    QMutex m_mutex;
};

// Data structures
struct Candle {
    qint64 timestamp;  // Unix epoch seconds
    double open;
    double high;
    double low;
    double close;
    qint64 volume;
    
    QJsonObject toJson() const {
        return {
            {"time", timestamp},
            {"open", open},
            {"high", high},
            {"low", low},
            {"close", close},
            {"volume", volume}
        };
    }
};
```

---

## Phase 2: Candle Aggregation Service

### Tick-to-Candle Conversion

```cpp
// include/services/CandleAggregator.h
class CandleAggregator : public QObject {
    Q_OBJECT
public:
    static CandleAggregator& instance();
    
    void subscribeTo(const QString& symbol, int segment, 
                    const QStringList& timeframes);
    void unsubscribeFrom(const QString& symbol, int segment);
    
    Candle getCurrentCandle(const QString& symbol, int segment,
                            const QString& timeframe) const;
    
signals:
    void candleComplete(const QString& symbol, int segment,
                       const QString& timeframe, const Candle& candle);
    void candleUpdate(const QString& symbol, int segment,
                     const QString& timeframe, const Candle& partialCandle);
    
private slots:
    void onTick(const UDP::MarketTick& tick);
    void onMinuteTick();  // Timer every minute
    
private:
    struct CandleBuilder {
        qint64 startTime;
        double open;
        double high;
        double low;
        double close;
        qint64 volume;
        bool firstTick;
        
        void update(double price, qint64 volQty) {
            if (firstTick) {
                open = high = low = close = price;
                volume = volQty;
                firstTick = false;
            } else {
                high = std::max(high, price);
                low = std::min(low, price);
                close = price;
                volume += volQty;
            }
        }
        
        Candle build() const {
            return {startTime, open, high, low, close, volume};
        }
    };
    
    // Key: "SYMBOL_SEGMENT_TIMEFRAME"
    QHash<QString, CandleBuilder> m_builders;
    QTimer m_minuteTimer;
};
```

### Implementation Logic

```cpp
void CandleAggregator::onTick(const UDP::MarketTick& tick) {
    QString symbol = getSymbolFromToken(tick.token);
    int segment = getSegmentFromToken(tick.token);
    
    // Update all active timeframes for this symbol
    for (const QString& tf : m_activeTimeframes[symbol]) {
        QString key = buildKey(symbol, segment, tf);
        
        // Get or create builder
        if (!m_builders.contains(key)) {
            m_builders[key] = CandleBuilder{
                .startTime = getTimeframeStart(QDateTime::currentSecsSinceEpoch(), tf),
                .firstTick = true
            };
        }
        
        m_builders[key].update(tick.ltp, tick.volume);
        
        // Emit partial update for real-time chart
        emit candleUpdate(symbol, segment, tf, m_builders[key].build());
    }
}

void CandleAggregator::onMinuteTick() {
    qint64 now = QDateTime::currentSecsSinceEpoch();
    
    // Check all builders for completion
    for (auto it = m_builders.begin(); it != m_builders.end(); ++it) {
        const QString& key = it.key();
        CandleBuilder& builder = it.value();
        
        // Parse key
        QStringList parts = key.split('_');
        QString symbol = parts[0];
        int segment = parts[1].toInt();
        QString timeframe = parts[2];
        
        // Check if timeframe period has ended
        qint64 periodEnd = builder.startTime + getTimeframeDuration(timeframe);
        if (now >= periodEnd) {
            // Candle complete
            Candle completedCandle = builder.build();
            emit candleComplete(symbol, segment, timeframe, completedCandle);
            
            // Save to database
            HistoricalDataStore::instance().saveCandle(
                symbol, segment, timeframe, completedCandle
            );
            
            // Start new candle
            builder = CandleBuilder{
                .startTime = periodEnd,
                .firstTick = true
            };
        }
    }
}

qint64 CandleAggregator::getTimeframeDuration(const QString& tf) const {
    if (tf == "1m") return 60;
    if (tf == "5m") return 300;
    if (tf == "15m") return 900;
    if (tf == "1h") return 3600;
    if (tf == "1D") return 86400;
    return 60;  // Default 1 minute
}
```

---

## Phase 3: Chart Widget Integration

### QWebEngineView Setup

```cpp
// include/ui/ChartWidget.h
class ChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChartWidget(QWidget* parent = nullptr);
    
    void setSymbol(const QString& symbol, int segment);
    void setTimeframe(const QString& timeframe);
    void addIndicator(const QString& indicatorName);
    void removeIndicator(const QString& indicatorName);
    void addOrderMarker(qint64 orderId, double price, const QString& side);
    
public slots:
    void onNewCandle(const Candle& candle);
    void onCandleUpdate(const Candle& partialCandle);
    void onIndicatorUpdate(const QString& name, double value);
    
signals:
    void chartClicked(double price, qint64 timestamp);
    void orderRequested(const QString& side, double price, int quantity);
    
private:
    void loadChartHTML();
    void setupWebChannel();
    
    QWebEngineView* m_webView;
    QWebChannel* m_channel;
    ChartBridge* m_bridge;
    QString m_currentSymbol;
    int m_currentSegment;
    QString m_currentTimeframe;
};

// Chart JavaScript bridge
class ChartBridge : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE void onChartClick(double price, qint64 timestamp);
    Q_INVOKABLE void onOrderRequest(const QString& side, double price, int qty);
    
public slots:
    void pushCandle(const QJsonObject& candleData);
    void pushIndicator(const QString& name, const QJsonArray& values);
    void pushMarker(const QJsonObject& markerData);
    
signals:
    void chartClicked(double price, qint64 timestamp);
    void orderRequested(const QString& side, double price, int quantity);
    
    // Signals to JavaScript
    void newCandleAvailable(const QJsonObject& candle);
    void indicatorUpdated(const QString& name, const QJsonArray& values);
    void markerAdded(const QJsonObject& marker);
};
```

### HTML/JavaScript Chart Implementation

```html
<!DOCTYPE html>
<html>
<head>
    <title>Trading Chart</title>
    <script src="qrc:///qtwebchannel/qwebchannel.js"></script>
    <script src="https://unpkg.com/lightweight-charts/dist/lightweight-charts.standalone.production.js"></script>
    <style>
        body {
            margin: 0;
            padding: 0;
            font-family: 'Segoe UI', Arial, sans-serif;
            background: #1e222d;
        }
        #chart-container {
            width: 100%;
            height: 100vh;
        }
        .legend {
            position: absolute;
            top: 10px;
            left: 10px;
            background: rgba(255, 255, 255, 0.1);
            padding: 8px 12px;
            border-radius: 4px;
            color: #d1d4dc;
            font-size: 12px;
        }
    </style>
</head>
<body>
    <div id="chart-container"></div>
    <div class="legend" id="legend"></div>
    
    <script>
        let chart, candleSeries, indicators = {};
        let bridge;
        
        // Initialize chart
        function initChart() {
            chart = LightweightCharts.createChart(
                document.getElementById('chart-container'),
                {
                    layout: {
                        background: { color: '#1e222d' },
                        textColor: '#d1d4dc',
                    },
                    grid: {
                        vertLines: { color: '#2b2b43' },
                        horzLines: { color: '#363c4e' },
                    },
                    crosshair: {
                        mode: LightweightCharts.CrosshairMode.Normal,
                    },
                    rightPriceScale: {
                        borderColor: '#2b2b43',
                    },
                    timeScale: {
                        borderColor: '#2b2b43',
                        timeVisible: true,
                        secondsVisible: false,
                    },
                }
            );
            
            // Create candlestick series
            candleSeries = chart.addCandlestickSeries({
                upColor: '#26a69a',
                downColor: '#ef5350',
                borderVisible: false,
                wickUpColor: '#26a69a',
                wickDownColor: '#ef5350',
            });
            
            // Click handler
            chart.subscribeCross hereMove(function(param) {
                if (!param.time || !param.point) return;
                
                const price = candleSeries.coordinateToPrice(param.point.y);
                updateLegend(param.time, price);
            });
            
            chart.subscribeClick(function(param) {
                if (!param.time || !param.point) return;
                
                const price = candleSeries.coordinateToPrice(param.point.y);
                bridge.onChartClick(price, param.time);
            });
        }
        
        // Connect to Qt
        new QWebChannel(qt.webChannelTransport, function(channel) {
            bridge = channel.objects.chartBridge;
            
            // Listen for new candles from C++
            bridge.newCandleAvailable.connect(function(candle) {
                candleSeries.update(candle);
            });
            
            // Listen for indicator updates
            bridge.indicatorUpdated.connect(function(name, values) {
                if (!indicators[name]) {
                    indicators[name] = chart.addLineSeries({
                        color: getIndicatorColor(name),
                        lineWidth: 2,
                    });
                }
                indicators[name].setData(values);
            });
            
            // Listen for order markers
            bridge.markerAdded.connect(function(marker) {
                candleSeries.setMarkers([...candleSeries.markers(), marker]);
            });
            
            console.log("Chart bridge connected");
        });
        
        function updateLegend(time, price) {
            document.getElementById('legend').innerHTML = `
                Time: ${new Date(time * 1000).toLocaleTimeString()}<br>
                Price: ${price.toFixed(2)}
            `;
        }
        
        function getIndicatorColor(name) {
            const colors = {
                'SMA_20': '#2196F3',
                'SMA_50': '#FF9800',
                'EMA_12': '#4CAF50',
                'EMA_26': '#F44336',
                'RSI': '#9C27B0',
                'MACD': '#00BCD4',
            };
            return colors[name] || '#FFFFFF';
        }
        
        // Initialize on load
        window.addEventListener('load', initChart);
    </script>
</body>
</html>
```

---

## Phase 4: Chart Data Service

### Unified API for Chart Data

```cpp
// include/services/ChartDataService.h
class ChartDataService : public QObject {
    Q_OBJECT
public:
    static ChartDataService& instance();
    
    void init();
    void subscribeChart(const QString& symbol, int segment, 
                       const QString& timeframe, ChartWidget* widget);
    void unsubscribeChart(ChartWidget* widget);
    
    QVector<Candle> getHistoricalCandles(const QString& symbol, int segment,
                                         const QString& timeframe, int count);
    
    void addIndicatorToChart(ChartWidget* widget, const QString& indicatorName,
                            const QVariantMap& params);
    
signals:
    void historicalDataLoaded(const QString& symbol, const QVector<Candle>& candles);
    void realtimeCandleUpdate(const QString& symbol, const Candle& candle);
    
private slots:
    void onCandleComplete(const QString& symbol, int segment,
                         const QString& timeframe, const Candle& candle);
    void onCandleUpdate(const QString& symbol, int segment,
                       const QString& timeframe, const Candle& partialCandle);
    
private:
    ChartDataService();
    
    struct ChartSubscription {
        ChartWidget* widget;
        QString symbol;
        int segment;
        QString timeframe;
        QSet<QString> indicators;
    };
    
    QVector<ChartSubscription> m_subscriptions;
    QMutex m_mutex;
};
```

### Implementation

```cpp
void ChartDataService::subscribeChart(const QString& symbol, int segment,
                                      const QString& timeframe, 
                                      ChartWidget* widget) {
    QMutexLocker locker(&m_mutex);
    
    // Add subscription
    ChartSubscription sub{widget, symbol, segment, timeframe, {}};
    m_subscriptions.append(sub);
    
    // Load historical data (last 500 candles)
    QVector<Candle> historical = 
        HistoricalDataStore::instance().getRecentCandles(
            symbol, segment, timeframe, 500
        );
    
    // Send to chart widget
    for (const Candle& candle : historical) {
        widget->onNewCandle(candle);
    }
    
    // Subscribe to aggregator for real-time updates
    connect(&CandleAggregator::instance(), &CandleAggregator::candleComplete,
            this, &ChartDataService::onCandleComplete);
    connect(&CandleAggregator::instance(), &CandleAggregator::candleUpdate,
            this, &ChartDataService::onCandleUpdate);
    
    // Start aggregation if not already running
    CandleAggregator::instance().subscribeTo(symbol, segment, {timeframe});
}

void ChartDataService::onCandleComplete(const QString& symbol, int segment,
                                        const QString& timeframe, 
                                        const Candle& candle) {
    QMutexLocker locker(&m_mutex);
    
    // Find subscribed widgets
    for (const auto& sub : m_subscriptions) {
        if (sub.symbol == symbol && 
            sub.segment == segment && 
            sub.timeframe == timeframe) {
            sub.widget->onNewCandle(candle);
        }
    }
    
    emit realtimeCandleUpdate(symbol, candle);
}
```

---

## Phase 5: Order Integration

### Chart-based Order Placement

```cpp
void ChartWidget::onChartClicked(double price, qint64 timestamp) {
    // Show order entry dialog
    OrderEntryDialog dialog(this);
    dialog.setSymbol(m_currentSymbol);
    dialog.setPrice(price);
    dialog.setQuantity(m_defaultQuantity);
    
    if (dialog.exec() == QDialog::Accepted) {
        QString side = dialog.side();  // "BUY" or "SELL"
        double orderPrice = dialog.price();
        int quantity = dialog.quantity();
        
        // Place order via OrderManager
        qint64 orderId = OrderManager::instance().placeOrder(
            m_currentSymbol, m_currentSegment, side, 
            quantity, orderPrice, "LIMIT"
        );
        
        // Add visual marker on chart
        addOrderMarker(orderId, orderPrice, side);
    }
}

void ChartWidget::addOrderMarker(qint64 orderId, double price, 
                                  const QString& side) {
    QJsonObject marker;
    marker["time"] = QDateTime::currentSecsSinceEpoch();
    marker["position"] = (side == "BUY") ? "belowBar" : "aboveBar";
    marker["color"] = (side == "BUY") ? "#26a69a" : "#ef5350";
    marker["shape"] = (side == "BUY") ? "arrowUp" : "arrowDown";
    marker["text"] = QString("%1 @%2").arg(side).arg(price, 0, 'f', 2);
    marker["size"] = 1.5;
    
    m_bridge->pushMarker(marker);
}
```

---

## Phase 6: Custom Indicator Integration

### Indicator Overlay on Chart

```cpp
void ChartWidget::addIndicator(const QString& indicatorName) {
    // Calculate indicator values from historical candles
    QVector<Candle> candles = ChartDataService::instance().getHistoricalCandles(
        m_currentSymbol, m_currentSegment, m_currentTimeframe, 500
    );
    
    QVector<double> values = IndicatorEngine::instance().calculate(
        indicatorName, candles
    );
    
    // Convert to JSON array
    QJsonArray jsonValues;
    for (int i = 0; i < values.size(); ++i) {
        QJsonObject point;
        point["time"] = candles[i].timestamp;
        point["value"] = values[i];
        jsonValues.append(point);
    }
    
    // Send to chart
    m_bridge->pushIndicator(indicatorName, jsonValues);
    
    // Subscribe to real-time updates
    connect(&IndicatorEngine::instance(), &IndicatorEngine::indicatorUpdated,
            this, &ChartWidget::onIndicatorUpdate);
}

void ChartWidget::onIndicatorUpdate(const QString& name, double value) {
    QJsonArray point;
    QJsonObject obj;
    obj["time"] = QDateTime::currentSecsSinceEpoch();
    obj["value"] = value;
    point.append(obj);
    
    m_bridge->pushIndicator(name, point);
}
```

---

## Testing Strategy

### Unit Tests

```cpp
// Test candle aggregation
TEST(CandleAggregator, OneMcompareCandle) {
    CandleAggregator aggr;
    QVector<Candle> completed;
    
    connect(&aggr, &CandleAggregator::candleComplete,
            [&](auto, auto, auto, const Candle& c) {
                completed.append(c);
            });
    
    // Simulate ticks over 1 minute
    for (int sec = 0; sec < 60; ++sec) {
        UDP::MarketTick tick;
        tick.ltp = 22000 + (sec % 10);
        tick.volume = 100;
        aggr.onTick(tick);
    }
    
    aggr.onMinuteTick();  // Force completion
    
    ASSERT_EQ(completed.size(), 1);
    EXPECT_NEAR(completed[0].open, 22000, 0.01);
    EXPECT_NEAR(completed[0].close, 22009, 0.01);
}
```

### Integration Tests

1. **Historical Data Load:** Verify 500 candles load correctly
2. **Real-time Updates:** Verify partial candle updates every tick
3. **Indicator Calculation:** Verify SMA/EMA values match expected
4. **Order Marker Display:** Verify markers appear at correct price/time
5. **Timeframe Switching:** Verify seamless timeframe changes

---

## Performance Optimization

### Considerations

1. **Database Batch Inserts:**
   ```cpp
   void saveCandleBatch(const QVector<Candle>& candles) {
       m_db.transaction();
       for (const Candle& c : candles) {
           // Insert without commit
       }
       m_db.commit();  // Single commit
   }
   ```

2. **WebChannel Message Batching:**
   - Don't send every tick to JavaScript
   - Batch updates at 100ms intervals
   - Only send changed candles

3. **Indicator Caching:**
   - Cache calculated indicators in memory
   - Only recalculate on new candle complete
   - Use incremental calculation where possible (e.g., EMA)

---

## Deployment Checklist

- [ ] QWebEngine module integrated in CMakeLists.txt
- [ ] TradingView Lightweight Charts library downloaded
- [ ] HistoricalDataStore with SQLite database
- [ ] CandleAggregator service running
- [ ] ChartWidget added to main window
- [ ] WebChannel bridge connected
- [ ] Historical data populated (at least 1 week)
- [ ] Real-time candle updates working
- [ ] Indicator overlays functional
- [ ] Order markers displaying correctly
- [ ] Multiple timeframes supported
- [ ] Performance acceptable (< 50ms chart updates)

---

## Estimated Costs

| Item | Cost | Notes |
|------|------|-------|
| **TradingView Lightweight Charts** | FREE | Open source (Apache 2.0) |
| **QWebEngine** | FREE | Part of Qt (LGPL) |
| **Storage (1 year OHLCV)** | ~500MB | SQLite, negligible cost |
| **Development Time** | 6-8 weeks | 1-2 developers |

**Total:** FREE (open source stack)

---

## Alternative: Paid TradingView Library

If budget allows, consider upgrading to full TradingView Charting Library:

**Pros:**
- More features (drawing tools, studies, alerts)
- Better performance
- Advanced order entry widgets
- Multi-symbol comparison

**Cons:**
- Cost: $3,500-$5,000 one-time
- Licensing restrictions
- Annual renewal fees

**Recommendation:** Start with Lightweight Charts, upgrade if needed.

---

## Next Steps

1. ✅ Approve this implementation plan
2. Begin Phase 1: Historical data storage (Week 1-2)
3. Implement Phase 2: Candle aggregation (Week 3-4)
4. Build Phase 3: Chart widget integration (Week 5-7)
5. Complete Phase 4-6: Data service, orders, indicators (Week 8)

---

**Files to Create:**
- HistoricalDataStore.h/cpp
- CandleAggregator.h/cpp
- ChartWidget.h/cpp
- ChartBridge.h/cpp
- ChartDataService.h/cpp
- chart.html (embedded resource)
- lightweight-charts.min.js (vendor library)

**Dependencies:**
- Qt5::WebEngineWidgets
- Qt5::WebChannel
- TA-Lib (optional for indicators)

---

**Status:** Ready for Implementation  
**Risk Level:** LOW (mature libraries)  
**Expected ROI:** HIGH (major UX improvement)
