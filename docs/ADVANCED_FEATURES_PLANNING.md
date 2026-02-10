# Advanced Features Planning - Trading Terminal C++

**Date:** February 10, 2026  
**Status:** Planning & Architecture Phase  
**Complexity:** Advanced

---

## Table of Contents

1. [Current Architecture Analysis](#current-architecture-analysis)
2. [TradingView Advanced Chart Integration](#tradingview-advanced-chart-integration)
3. [Custom Strategy Builder Framework](#custom-strategy-builder-framework)
4. [Plugin System (DLL-based Strategy Extensions)](#plugin-system-dll-based-strategy-extensions)
5. [Implementation Roadmap](#implementation-roadmap)
6. [Technical Challenges & Solutions](#technical-challenges--solutions)

---

## Current Architecture Analysis

### Strategy Manager Overview

#### Architecture Pattern
```
┌─────────────────────────────────────────────────────────┐
│                   Strategy Manager UI                    │
│             (StrategyManagerWindow.cpp)                  │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│               StrategyService (Singleton)                │
│  - Lifecycle management (start/pause/resume/stop)       │
│  - Instance storage (QHash<id, StrategyInstance>)       │
│  - 500ms update timer for metric refresh                │
│  - Signal/slot event distribution                       │
└────────────────────┬────────────────────────────────────┘
                     │
        ┌────────────┴────────────┐
        ▼                         ▼
┌──────────────────┐    ┌──────────────────┐
│ StrategyFactory  │    │ StrategyRepository│
│  (Factory)       │    │  (SQLite DB)      │
└────────┬─────────┘    └──────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────┐
│              StrategyBase (Abstract Base)                │
│  - init(), start(), stop(), pause(), resume()           │
│  - onTick() - pure virtual for strategy logic           │
│  - Market data subscription management                  │
│  - Parameter handling (QVariantMap)                     │
└────────────────────┬────────────────────────────────────┘
                     │
        ┌────────────┴────────────┬───────────────────┐
        ▼                         ▼                   ▼
┌──────────────┐    ┌──────────────────┐  ┌──────────────────┐
│ JodiATM      │    │ Momentum         │  │ RangeBreakout    │
│ Strategy     │    │ Strategy         │  │ Strategy         │
└──────────────┘    └──────────────────┘  └──────────────────┘
```

### Current Strategy Implementation (JodiATM Example)

**Key Features:**
- **Dynamic ATM Management**: Auto-adjusts strike prices as market moves
- **Leg-based Position Shifting**: 4-leg system with 25% quantity per leg
- **Trend Detection**: Bullish/Bearish/Neutral with reversal logic
- **ATM Reset Boundaries**: RCP (Reset Constant Points) for major market moves
- **Market Data Integration**: Real-time UDP feed subscription via FeedHandler
- **ATM Watch Integration**: Connected to ATMWatchManager for strike updates

**State Management:**
```cpp
enum class Trend { Nutral, Bullish, Bearish };

// Strategy Parameters
double m_offset;        // Distance from ATM to monitor
double m_threshold;     // Trigger buffer
double m_adjPts;        // Adjustment points
double m_strikeDiff;    // Strike interval (50 for NIFTY)
int m_currentLeg;       // 0-4 leg progression
double m_currentATM;    // Current ATM strike
```

**Execution Flow:**
1. **Init**: Load parameters from StrategyInstance
2. **Start**: Get ATM info, subscribe to underlying feed
3. **OnTick**: Receive market ticks → checkTrade()
4. **checkTrade()**: Decision logic based on price vs thresholds
5. **calculateThresholds()**: Recompute decision points per leg
6. **resetATM()**: Major market move handling

### Strengths of Current Design

✅ **Clean Separation**: Service layer decoupled from strategies  
✅ **Factory Pattern**: Easy to add new strategy types  
✅ **Parameter Flexibility**: QVariantMap allows dynamic configs  
✅ **State Machine**: Well-defined lifecycle states  
✅ **Real-time Integration**: Direct UDP feed subscription  
✅ **Persistence**: SQLite repository for recovery  
✅ **Thread-safe**: QMutex protection on shared data  

### Limitations & Extension Points

⚠️ **Hard-coded Strategy Types**: Factory requires code changes  
⚠️ **No Visual Strategy Builder**: Parameters are form-based only  
⚠️ **No Indicator Support**: No charting/technical analysis hooks  
⚠️ **No Plugin System**: Cannot load external strategies  
⚠️ **Limited Backtesting**: No historical simulation framework  

---

## TradingView Advanced Chart Integration

### Overview

Integrate TradingView Advanced Charts into the application to provide:
- Professional charting with indicators
- Custom indicator development
- Visual strategy backtesting
- Trade signal visualization
- Multi-timeframe analysis

### Architecture Design

#### Option 1: Qt WebEngineView (Recommended)

**Tech Stack:**
- Qt WebEngine (Chromium-based web browser widget)
- TradingView Charting Library (paid license required)
- JavaScript ↔ C++ bridge (QWebChannel)

**Pros:**
- Full TradingView functionality
- Real-time data injection from C++
- Custom indicators via Pine Script or JS
- Professional UI with minimal development
- Cross-platform

**Cons:**
- Requires Qt WebEngine dependency (~100MB)
- TradingView license cost (~$3-5k/year)
- JavaScript bridge complexity
- Memory overhead

#### Option 2: Lightweight Charting Library

**Tech Stack:**
- Qt WebEngineView with Lightweight Charts (free, TradingView's open-source alternative)
- Custom C++ ↔ JS bridge
- Custom indicator engine in C++

**Pros:**
- Free and open-source
- Lower memory footprint
- Full control over features
- No licensing restrictions

**Cons:**
- Fewer built-in indicators
- More development work
- Less polished UI

#### Recommended Architecture (TradingView + WebEngine)

```
┌─────────────────────────────────────────────────────────┐
│             ChartWindow (QMainWindow/QWidget)            │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │        QWebEngineView (Chromium Browser)           │ │
│  │                                                    │ │
│  │  ┌──────────────────────────────────────────────┐ │ │
│  │  │      TradingView Chart Widget (HTML/JS)      │ │ │
│  │  │  - Candlestick/Bar/Line charts              │ │ │
│  │  │  - Built-in indicators (SMA, EMA, RSI, etc) │ │ │
│  │  │  - Drawing tools                            │ │ │
│  │  │  - Custom Pine Script indicators           │ │ │
│  │  └──────────────────────────────────────────────┘ │ │
│  │                                                    │ │
│  └───────────────────┬────────────────────────────────┘ │
│                      │ QWebChannel                      │
│  ┌───────────────────▼────────────────────────────────┐ │
│  │        ChartBridge (C++ ↔ JS Interface)           │ │
│  │  - updateCandles(data)                            │ │
│  │  - addIndicator(name, params)                     │ │
│  │  - addTrade(signal)                               │ │
│  │  - onUserDrawing(event)                           │ │
│  └───────────────────┬────────────────────────────────┘ │
└────────────────────────┬────────────────────────────────┘
                         │
         ┌───────────────┴─────────────┐
         ▼                             ▼
┌──────────────────┐        ┌──────────────────┐
│  FeedHandler     │        │  Strategy        │
│  (Market Data)   │        │  (Trade Signals) │
└──────────────────┘        └──────────────────┘
```

### Implementation Steps

#### Step 1: Add Qt WebEngine Dependency

**CMakeLists.txt:**
```cmake
# Find Qt5 WebEngine
find_package(Qt5 COMPONENTS WebEngineWidgets REQUIRED)

# Add to target
target_link_libraries(TradingTerminal
    Qt5::Widgets
    Qt5::WebEngineWidgets
    # ... existing libs
)
```

**Install on Windows:**
```powershell
# Qt WebEngine is included in Qt 5.15.2 installation
# Verify installation:
ls "C:\Qt\5.15.2\mingw81_64\lib\*WebEngine*"
```

#### Step 2: Create ChartWindow Class

**include/ui/ChartWindow.h:**
```cpp
#ifndef CHART_WINDOW_H
#define CHART_WINDOW_H

#include <QWidget>
#include <QWebEngineView>
#include <QWebChannel>
#include "ChartBridge.h"

namespace Ui {
class ChartWindow;
}

class ChartWindow : public QWidget {
    Q_OBJECT

public:
    explicit ChartWindow(const QString& symbol, QWidget* parent = nullptr);
    ~ChartWindow();

    // Data injection
    void updateCandles(const QVector<OHLCV>& candles);
    void addTrade(const TradeSignal& trade);
    void updateIndicator(const QString& name, const QVariantMap& values);

    // Indicator management
    void addIndicator(const QString& name, const QVariantMap& config);
    void removeIndicator(const QString& name);

signals:
    void userDrawingAdded(const Drawing& drawing);
    void timeframeChanged(const QString& timeframe);
    void symbolChanged(const QString& symbol);

private:
    void setupUI();
    void loadChart();

    Ui::ChartWindow* ui;
    QWebEngineView* m_webView;
    QWebChannel* m_channel;
    ChartBridge* m_bridge;
    QString m_symbol;
};

#endif // CHART_WINDOW_H
```

#### Step 3: Create JavaScript Bridge

**include/ui/ChartBridge.h:**
```cpp
#ifndef CHART_BRIDGE_H
#define CHART_BRIDGE_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>

struct OHLCV {
    qint64 timestamp;
    double open;
    double high;
    double low;
    double close;
    double volume;
};

struct TradeSignal {
    qint64 timestamp;
    QString type;  // "buy", "sell"
    double price;
    QString label;
};

class ChartBridge : public QObject {
    Q_OBJECT

public:
    explicit ChartBridge(QObject* parent = nullptr);

public slots:
    // Called from JavaScript
    void onUserDrawing(const QVariantMap& drawing);
    void onTimeframeChange(const QString& timeframe);
    void onChartReady();

signals:
    // Called from C++ → exposed to JavaScript
    void updateCandles(const QVariantList& candles);
    void addTrade(const QVariantMap& trade);
    void addIndicatorData(const QString& name, const QVariantList& values);
    void clearChart();

    // User interactions (C++ receives)
    void drawingAdded(const QVariantMap& drawing);
    void timeframeChanged(const QString& timeframe);
    void chartReady();

private:
    bool m_isReady = false;
};

#endif // CHART_BRIDGE_H
```

#### Step 4: Create TradingView HTML Template

**resources/chart/tradingview.html:**
```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>TradingView Chart</title>
    <script src="qrc:///qtwebchannel/qwebchannel.js"></script>
    <script src="https://unpkg.com/lightweight-charts/dist/lightweight-charts.standalone.production.js"></script>
    <!-- OR TradingView library if licensed -->
    <style>
        body {
            margin: 0;
            padding: 0;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        }
        #chart-container {
            width: 100%;
            height: 100vh;
        }
    </style>
</head>
<body>
    <div id="chart-container"></div>

    <script>
        // Initialize WebChannel bridge
        let bridge = null;
        new QWebChannel(qt.webChannelTransport, function(channel) {
            bridge = channel.objects.bridge;

            // Notify C++ that chart is ready
            bridge.chartReady();

            // Listen to C++ signals
            bridge.updateCandles.connect(function(candles) {
                updateChartData(candles);
            });

            bridge.addTrade.connect(function(trade) {
                addTradeMarker(trade);
            });

            bridge.addIndicatorData.connect(function(name, values) {
                updateIndicator(name, values);
            });
        });

        // Create chart instance (Lightweight Charts example)
        const chart = LightweightCharts.createChart(document.getElementById('chart-container'), {
            width: window.innerWidth,
            height: window.innerHeight,
            layout: {
                backgroundColor: '#1e222d',
                textColor: '#d1d4dc',
            },
            grid: {
                vertLines: { color: '#2b2b43' },
                horzLines: { color: '#2b2b43' },
            },
            crosshair: {
                mode: LightweightCharts.CrosshairMode.Normal,
            },
            rightPriceScale: {
                borderColor: '#485c7b',
            },
            timeScale: {
                borderColor: '#485c7b',
                timeVisible: true,
                secondsVisible: false,
            },
        });

        const candlestickSeries = chart.addCandlestickSeries({
            upColor: '#26a69a',
            downColor: '#ef5350',
            borderVisible: false,
            wickUpColor: '#26a69a',
            wickDownColor: '#ef5350',
        });

        // Update chart with new candle data
        function updateChartData(candles) {
            const formatted = candles.map(c => ({
                time: c.timestamp / 1000, // Convert ms to seconds
                open: c.open,
                high: c.high,
                low: c.low,
                close: c.close,
            }));
            candlestickSeries.setData(formatted);
        }

        // Add trade markers
        const markers = [];
        function addTradeMarker(trade) {
            markers.push({
                time: trade.timestamp / 1000,
                position: trade.type === 'buy' ? 'belowBar' : 'aboveBar',
                color: trade.type === 'buy' ? '#26a69a' : '#ef5350',
                shape: trade.type === 'buy' ? 'arrowUp' : 'arrowDown',
                text: trade.label,
            });
            candlestickSeries.setMarkers(markers);
        }

        // Indicator series storage
        const indicators = {};
        function updateIndicator(name, values) {
            if (!indicators[name]) {
                indicators[name] = chart.addLineSeries({
                    color: getIndicatorColor(name),
                    lineWidth: 2,
                });
            }
            const formatted = values.map(v => ({
                time: v.timestamp / 1000,
                value: v.value,
            }));
            indicators[name].setData(formatted);
        }

        function getIndicatorColor(name) {
            const colors = {
                'SMA': '#2196F3',
                'EMA': '#4CAF50',
                'RSI': '#FF9800',
                'MACD': '#9C27B0',
            };
            return colors[name] || '#FFC107';
        }

        // Resize handler
        window.addEventListener('resize', () => {
            chart.applyOptions({
                width: window.innerWidth,
                height: window.innerHeight,
            });
        });

        // User drawing event (notify C++)
        // Note: Lightweight Charts doesn't have built-in drawing tools
        // You'd need to add a custom drawing layer or use TradingView library
    </script>
</body>
</html>
```

#### Step 5: Integrate with Strategy Manager

**Modify StrategyManagerWindow to open chart:**
```cpp
void StrategyManagerWindow::onShowChartClicked() {
    bool ok = false;
    StrategyInstance instance = selectedInstance(&ok);
    if (!ok) return;

    // Create or focus existing chart window
    QString key = instance.symbol;
    if (m_chartWindows.contains(key)) {
        m_chartWindows[key]->raise();
        m_chartWindows[key]->activateWindow();
        return;
    }

    ChartWindow* chartWin = new ChartWindow(instance.symbol, this);
    m_chartWindows[key] = chartWin;
    chartWin->show();

    // Feed historical data
    QVector<OHLCV> candles = HistoricalDataService::instance()
        .getCandles(instance.symbol, "1m", QDateTime::currentDateTime().addDays(-7));
    chartWin->updateCandles(candles);

    // Connect strategy signals
    connect(&StrategyService::instance(), &StrategyService::tradeSignal,
            chartWin, &ChartWindow::addTrade);
}
```

### Custom Indicator Development

#### Approach 1: Pine Script (TradingView)

If using TradingView library:
- Users write Pine Script indicators
- Store in database or files
- Inject into chart via JavaScript

#### Approach 2: C++ Indicator Engine

**Create indicator base class:**

**include/indicators/IndicatorBase.h:**
```cpp
#ifndef INDICATOR_BASE_H
#define INDICATOR_BASE_H

#include <QVector>
#include <QVariantMap>

struct Candle {
    qint64 timestamp;
    double open, high, low, close, volume;
};

struct IndicatorValue {
    qint64 timestamp;
    double value;
    QString label; // Optional for multi-line indicators
};

class IndicatorBase {
public:
    virtual ~IndicatorBase() = default;

    // Calculate indicator for candle data
    virtual QVector<IndicatorValue> calculate(
        const QVector<Candle>& candles,
        const QVariantMap& params) = 0;

    virtual QString name() const = 0;
    virtual QStringList parameterNames() const = 0;
};

// Example: Simple Moving Average
class SMAIndicator : public IndicatorBase {
public:
    QVector<IndicatorValue> calculate(
        const QVector<Candle>& candles,
        const QVariantMap& params) override {
        
        int period = params.value("period", 14).toInt();
        QVector<IndicatorValue> result;

        for (int i = period - 1; i < candles.size(); ++i) {
            double sum = 0.0;
            for (int j = 0; j < period; ++j) {
                sum += candles[i - j].close;
            }
            result.append({candles[i].timestamp, sum / period, ""});
        }

        return result;
    }

    QString name() const override { return "SMA"; }
    QStringList parameterNames() const override { return {"period"}; }
};

#endif // INDICATOR_BASE_H
```

**Indicator Factory:**
```cpp
class IndicatorFactory {
public:
    static IndicatorBase* create(const QString& name) {
        if (name == "SMA") return new SMAIndicator();
        if (name == "EMA") return new EMAIndicator();
        if (name == "RSI") return new RSIIndicator();
        if (name == "MACD") return new MACDIndicator();
        // Add custom indicators here
        return nullptr;
    }
};
```

### Data Feed Integration

**Connect real-time UDP feed to chart:**

```cpp
// In ChartWindow constructor
connect(&FeedHandler::instance(), &FeedHandler::tickReceived,
        this, &ChartWindow::onMarketTick);

void ChartWindow::onMarketTick(int segment, uint32_t token, const UDP::MarketTick& tick) {
    if (token != m_token) return;

    // Aggregate ticks into candles (1sec, 1min, 5min, etc.)
    m_candleBuilder.addTick(tick);

    if (m_candleBuilder.isComplete()) {
        OHLCV candle = m_candleBuilder.build();
        
        // Update chart via bridge
        QVariantList candles;
        candles.append(candleToVariant(candle));
        m_bridge->updateCandles(candles);
    }
}
```

---

## Custom Strategy Builder Framework

### Goal

Provide a flexible framework where users can create custom strategies without writing C++ code, using:
1. **Visual Strategy Builder UI** (drag-drop blocks)
2. **Strategy Skeleton/Template System**
3. **Script-based Strategy Definition** (Python/Lua/JSON DSL)

### Architecture Design

```
┌─────────────────────────────────────────────────────────┐
│          Strategy Builder UI (Visual Editor)             │
│  ┌────────────────────────────────────────────────────┐ │
│  │  Condition Blocks    │   Action Blocks             │ │
│  │  ┌────────────┐     │   ┌────────────┐           │ │
│  │  │ Price >    │     │   │ Buy Order  │           │ │
│  │  │   SMA(20)  │────────▶│  Quantity  │           │ │
│  │  └────────────┘     │   └────────────┘           │ │
│  │                     │                             │ │
│  │  ┌────────────┐     │   ┌────────────┐           │ │
│  │  │ RSI < 30   │     │   │ Sell Order │           │ │
│  │  └────────────┘     │   └────────────┘           │ │
│  └────────────────────────────────────────────────────┘ │
│               ▼ Generates Strategy Config ▼             │
│  ┌────────────────────────────────────────────────────┐ │
│  │         Strategy JSON Definition                   │ │
│  │  {                                                 │ │
│  │    "name": "My RSI Strategy",                     │ │
│  │    "conditions": [...],                           │ │
│  │    "actions": [...],                              │ │
│  │    "parameters": {...}                            │ │
│  │  }                                                 │ │
│  └────────────────────────────────────────────────────┘ │
└───────────────────────────┬─────────────────────────────┘
                            ▼
┌─────────────────────────────────────────────────────────┐
│        StrategyInterpreter / StrategyScriptEngine        │
│  - Parse JSON/Script definition                         │
│  - Compile to executable logic                          │
│  - Execute on market ticks                              │
└───────────────────────────┬─────────────────────────────┘
                            ▼
┌─────────────────────────────────────────────────────────┐
│            CustomStrategy : StrategyBase                 │
│  - Wraps interpreter/script engine                      │
│  - Implements onTick() by delegating to script          │
└─────────────────────────────────────────────────────────┘
```

### Implementation Approach

#### Option 1: JSON-Based Strategy Definition (Recommended for MVP)

**Strategy Schema:**
```json
{
  "strategyName": "RSI Reversal",
  "version": "1.0",
  "parameters": {
    "rsi_period": 14,
    "overbought": 70,
    "oversold": 30,
    "quantity": 50
  },
  "indicators": [
    {
      "name": "RSI",
      "type": "RSI",
      "params": { "period": "{{rsi_period}}" }
    },
    {
      "name": "SMA_20",
      "type": "SMA",
      "params": { "period": 20 }
    }
  ],
  "entryConditions": {
    "long": {
      "operator": "AND",
      "conditions": [
        { "indicator": "RSI", "operator": "<", "value": "{{oversold}}" },
        { "field": "close", "operator": ">", "indicator": "SMA_20" }
      ]
    },
    "short": {
      "operator": "AND",
      "conditions": [
        { "indicator": "RSI", "operator": ">", "value": "{{overbought}}" },
        { "field": "close", "operator": "<", "indicator": "SMA_20" }
      ]
    }
  },
  "exitConditions": {
    "target": { "type": "percentage", "value": 2.0 },
    "stopLoss": { "type": "percentage", "value": 1.0 }
  },
  "actions": {
    "onLongEntry": [
      { "type": "placeOrder", "side": "BUY", "quantity": "{{quantity}}" }
    ],
    "onShortEntry": [
      { "type": "placeOrder", "side": "SELL", "quantity": "{{quantity}}" }
    ]
  }
}
```

**Strategy Interpreter:**

**include/strategies/CustomStrategy.h:**
```cpp
#ifndef CUSTOM_STRATEGY_H
#define CUSTOM_STRATEGY_H

#include "strategies/StrategyBase.h"
#include "indicators/IndicatorBase.h"
#include <QJsonDocument>
#include <QMap>

class CustomStrategy : public StrategyBase {
    Q_OBJECT

public:
    explicit CustomStrategy(QObject* parent = nullptr);

    void init(const StrategyInstance& instance) override;
    void start() override;
    void stop() override;

protected slots:
    void onTick(const UDP::MarketTick& tick) override;

private:
    struct Condition {
        QString field;          // "close", "open", "high", "low"
        QString indicator;      // "RSI", "SMA_20"
        QString op;             // ">", "<", ">=", "<=", "==", "!="
        double value;
        bool isIndicator;
    };

    struct ConditionGroup {
        QString logicalOp;      // "AND", "OR"
        QVector<Condition> conditions;
    };

    bool loadStrategyDefinition(const QString& jsonPath);
    bool evaluateConditions(const ConditionGroup& group);
    void executeActions(const QJsonArray& actions);
    void updateIndicators(const UDP::MarketTick& tick);

    QJsonDocument m_strategyDef;
    QMap<QString, IndicatorBase*> m_indicators;
    QVector<Candle> m_candles;

    ConditionGroup m_longEntry;
    ConditionGroup m_shortEntry;
    
    bool m_inPosition = false;
    QString m_positionSide; // "LONG" or "SHORT"
    double m_entryPrice = 0.0;
};

#endif // CUSTOM_STRATEGY_H
```

**Implementation:**
```cpp
bool CustomStrategy::evaluateConditions(const ConditionGroup& group) {
    if (group.logicalOp == "AND") {
        for (const Condition& cond : group.conditions) {
            if (!evaluateSingleCondition(cond)) {
                return false;
            }
        }
        return true;
    } else if (group.logicalOp == "OR") {
        for (const Condition& cond : group.conditions) {
            if (evaluateSingleCondition(cond)) {
                return true;
            }
        }
        return false;
    }
    return false;
}

bool CustomStrategy::evaluateSingleCondition(const Condition& cond) {
    double leftValue = 0.0;
    double rightValue = cond.value;

    // Get left-hand side value
    if (cond.isIndicator && m_indicators.contains(cond.indicator)) {
        QVector<IndicatorValue> values = m_indicators[cond.indicator]->getValues();
        if (!values.isEmpty()) {
            leftValue = values.last().value;
        }
    } else {
        // Field value (close, open, etc.)
        if (m_candles.isEmpty()) return false;
        if (cond.field == "close") leftValue = m_candles.last().close;
        else if (cond.field == "open") leftValue = m_candles.last().open;
        else if (cond.field == "high") leftValue = m_candles.last().high;
        else if (cond.field == "low") leftValue = m_candles.last().low;
    }

    // Compare
    if (cond.op == ">") return leftValue > rightValue;
    if (cond.op == "<") return leftValue < rightValue;
    if (cond.op == ">=") return leftValue >= rightValue;
    if (cond.op == "<=") return leftValue <= rightValue;
    if (cond.op == "==") return qFuzzyCompare(leftValue, rightValue);
    if (cond.op == "!=") return !qFuzzyCompare(leftValue, rightValue);

    return false;
}

void CustomStrategy::onTick(const UDP::MarketTick& tick) {
    if (!m_isRunning) return;

    // Update candle data
    updateCandles(tick);
    
    // Update all indicators
    updateIndicators(tick);

    // Check entry conditions
    if (!m_inPosition) {
        if (evaluateConditions(m_longEntry)) {
            log("Long entry condition met");
            executeActions(m_strategyDef["actions"]["onLongEntry"].toArray());
            m_inPosition = true;
            m_positionSide = "LONG";
            m_entryPrice = tick.ltp;
        } else if (evaluateConditions(m_shortEntry)) {
            log("Short entry condition met");
            executeActions(m_strategyDef["actions"]["onShortEntry"].toArray());
            m_inPosition = true;
            m_positionSide = "SHORT";
            m_entryPrice = tick.ltp;
        }
    } else {
        // Check exit conditions (SL/Target)
        checkExitConditions(tick);
    }
}
```

#### Option 2: Visual Block-based Builder UI

**Component:**
- **BlockEditor**: Drag-drop interface (similar to Scratch/Blockly)
- **Block Types**: Condition blocks, operator blocks, action blocks
- **Data Flow**: Visual connections between blocks
- **Export**: Generate JSON strategy definition

**UI Mockup:**
```
┌────────────────────────────────────────────────────────────┐
│  Strategy Builder - Visual Editor                           │
├────────────────────────────────────────────────────────────┤
│  Blocks Palette       │   Canvas                           │
│  ┌──────────────┐    │   ┌──────────────────────────────┐ │
│  │ Conditions   │    │   │  ┌────────────────────────┐  │ │
│  │  • Price     │    │   │  │ When (Entry)           │  │ │
│  │  • Indicator │    │   │  │  ┌──────────────────┐  │  │ │
│  │  • Time      │    │   │  │  │ RSI < 30         │  │  │ │
│  └──────────────┘    │   │  │  └──────────────────┘  │  │ │
│                      │   │  │        AND              │  │ │
│  ┌──────────────┐    │   │  │  ┌──────────────────┐  │  │ │
│  │ Actions      │    │   │  │  │ Close > SMA(20)  │  │  │ │
│  │  • Buy/Sell  │    │   │  │  └──────────────────┘  │  │ │
│  │  • Modify    │    │   │  └───────▼────────────────┘  │ │
│  │  • Cancel    │    │   │  ┌────────────────────────┐  │ │
│  └──────────────┘    │   │  │ Then (Action)          │  │ │
│                      │   │  │  ┌──────────────────┐  │  │ │
│  ┌──────────────┐    │   │  │  │ Buy 50 lots      │  │  │ │
│  │ Operators    │    │   │  │  │ @ Market         │  │  │ │
│  │  • AND/OR    │    │   │  │  └──────────────────┘  │  │ │
│  │  • >/<       │    │   │  └────────────────────────┘  │ │
│  └──────────────┘    │   └──────────────────────────────┘ │
└────────────────────────────────────────────────────────────┘
```

**Implementation with Qt:**
- Use `QGraphicsScene` for canvas
- Custom `QGraphicsItem` subclasses for blocks
- `QGraphicsLineItem` for connections
- Serialize to JSON on save

#### Option 3: Python Scripting Engine (Advanced)

**Embed Python interpreter:**

```cpp
// Use pybind11 or QtPython
class PythonStrategy : public StrategyBase {
    void onTick(const UDP::MarketTick& tick) override {
        // Call Python function
        py::object result = m_pythonModule.attr("on_tick")(
            tick.ltp, tick.ltq, tick.bidPrice, tick.askPrice
        );
        
        if (result.cast<std::string>() == "BUY") {
            placeOrder(...);
        }
    }

private:
    py::object m_pythonModule;
};
```

**User Python script:**
```python
# user_strategy.py
class MyStrategy:
    def __init__(self, params):
        self.rsi_period = params['rsi_period']
        self.rsi = RSI(self.rsi_period)
    
    def on_tick(self, ltp, ltq, bid, ask):
        self.rsi.update(ltp)
        
        if self.rsi.value < 30:
            return "BUY"
        elif self.rsi.value > 70:
            return "SELL"
        
        return "HOLD"
```

---

## Plugin System (DLL-based Strategy Extensions)

### Goal

Allow clients to develop their own strategies as DLL plugins without accessing your source code.

### Architecture Design

```
┌─────────────────────────────────────────────────────────┐
│              Trading Terminal Application                │
│  ┌────────────────────────────────────────────────────┐ │
│  │            StrategyPluginManager                   │ │
│  │  - Load .dll/.so files from plugins/ directory    │ │
│  │  - Validate plugin signature/version              │ │
│  │  - Create strategy instances via factory          │ │
│  └────────────────────┬───────────────────────────────┘ │
│                       │                                  │
│       ┌───────────────┴────────────────┐                │
│       ▼                                ▼                │
│  ┌─────────────┐                 ┌─────────────┐       │
│  │ Built-in    │                 │ Plugin      │       │
│  │ Strategies  │                 │ Strategies  │       │
│  └─────────────┘                 └─────────────┘       │
└──────────────────────────────────────┬──────────────────┘
                                       ▼
                      ┌───────────────────────────────────┐
                      │  External Strategy DLL/SO         │
                      │  (Client-developed)               │
                      │                                   │
                      │  class MyStrategy : IStrategy {   │
                      │    on_tick() { ... }              │
                      │    on_start() { ... }             │
                      │  }                                │
                      └───────────────────────────────────┘
```

### Public API Design

**Provide a minimal C API header for plugin development:**

**sdk/IStrategyPlugin.h:**
```cpp
#ifndef ISTRATEGY_PLUGIN_H
#define ISTRATEGY_PLUGIN_H

#include <cstdint>

// C ABI for cross-compiler compatibility
#ifdef _WIN32
    #ifdef STRATEGY_PLUGIN_EXPORTS
        #define STRATEGY_API __declspec(dllexport)
    #else
        #define STRATEGY_API __declspec(dllimport)
    #endif
#else
    #define STRATEGY_API
#endif

extern "C" {

// Plugin metadata
struct StrategyPluginInfo {
    const char* name;
    const char* version;
    const char* author;
    const char* description;
    uint32_t apiVersion; // Must match host app version
};

// Market data tick
struct MarketTick {
    uint32_t token;
    double ltp;
    double ltq;
    double totalBuyQty;
    double totalSellQty;
    double bidPrice;
    double askPrice;
    uint64_t timestamp;
};

// Order request
struct OrderRequest {
    const char* symbol;
    const char* side;      // "BUY" or "SELL"
    int quantity;
    double price;
    const char* orderType; // "LIMIT", "MARKET", "SL", "SL-M"
};

// Strategy callbacks
typedef void (*OnTickCallback)(void* context, const MarketTick* tick);
typedef void (*OnOrderUpdateCallback)(void* context, const char* orderId, const char* status);
typedef void (*OnLogCallback)(void* context, const char* message);

// Host functions (provided by trading terminal)
struct HostFunctions {
    void (*placeOrder)(void* context, const OrderRequest* order);
    void (*cancelOrder)(void* context, const char* orderId);
    void (*modifyOrder)(void* context, const char* orderId, double newPrice, int newQty);
    void (*subscribeMarketData)(void* context, uint32_t token);
    void (*log)(void* context, const char* message);
    double (*getParameter)(void* context, const char* key);
};

// Strategy interface (implemented by plugin)
struct IStrategyPlugin {
    void* context; // Opaque strategy instance pointer

    // Lifecycle
    void (*init)(void* context, const HostFunctions* host);
    void (*start)(void* context);
    void (*stop)(void* context);
    void (*destroy)(void* context);

    // Callbacks
    OnTickCallback onTick;
    OnOrderUpdateCallback onOrderUpdate;
};

// Required exports from plugin DLL
STRATEGY_API const StrategyPluginInfo* GetPluginInfo();
STRATEGY_API IStrategyPlugin* CreateStrategy();
STRATEGY_API void DestroyStrategy(IStrategyPlugin* strategy);

} // extern "C"

#endif // ISTRATEGY_PLUGIN_H
```

### Plugin Loading System

**include/strategies/StrategyPluginManager.h:**
```cpp
#ifndef STRATEGY_PLUGIN_MANAGER_H
#define STRATEGY_PLUGIN_MANAGER_H

#include <QObject>
#include <QHash>
#include <QLibrary>
#include "IStrategyPlugin.h"

class StrategyPluginManager : public QObject {
    Q_OBJECT

public:
    static StrategyPluginManager& instance();

    // Load all plugins from directory
    bool loadPluginsFromDirectory(const QString& path);
    
    // Load single plugin
    bool loadPlugin(const QString& pluginPath);
    
    // Get available plugin names
    QStringList availablePlugins() const;
    
    // Create strategy instance from plugin
    IStrategyPlugin* createPluginStrategy(const QString& pluginName);

signals:
    void pluginLoaded(const QString& name);
    void pluginLoadFailed(const QString& path, const QString& error);

private:
    struct PluginEntry {
        QLibrary* library;
        StrategyPluginInfo info;
        IStrategyPlugin* (*createFunc)();
        void (*destroyFunc)(IStrategyPlugin*);
    };

    QHash<QString, PluginEntry> m_plugins;
};

#endif // STRATEGY_PLUGIN_MANAGER_H
```

**Implementation:**
```cpp
bool StrategyPluginManager::loadPlugin(const QString& pluginPath) {
    QLibrary* lib = new QLibrary(pluginPath);
    
    if (!lib->load()) {
        qWarning() << "Failed to load plugin:" << pluginPath << lib->errorString();
        delete lib;
        return false;
    }

    // Resolve required functions
    auto getInfoFunc = (const StrategyPluginInfo* (*)())lib->resolve("GetPluginInfo");
    auto createFunc = (IStrategyPlugin* (*)())lib->resolve("CreateStrategy");
    auto destroyFunc = (void (*)(IStrategyPlugin*))lib->resolve("DestroyStrategy");

    if (!getInfoFunc || !createFunc || !destroyFunc) {
        qWarning() << "Plugin missing required exports:" << pluginPath;
        lib->unload();
        delete lib;
        return false;
    }

    // Validate API version
    const StrategyPluginInfo* info = getInfoFunc();
    if (info->apiVersion != STRATEGY_API_VERSION) {
        qWarning() << "Plugin API version mismatch:"
                   << info->apiVersion << "!=" << STRATEGY_API_VERSION;
        lib->unload();
        delete lib;
        return false;
    }

    // Register plugin
    PluginEntry entry;
    entry.library = lib;
    entry.info = *info;
    entry.createFunc = createFunc;
    entry.destroyFunc = destroyFunc;

    m_plugins.insert(info->name, entry);
    emit pluginLoaded(info->name);

    qInfo() << "Loaded plugin:" << info->name << "v" << info->version;
    return true;
}
```

### Wrapper Strategy for Plugins

**include/strategies/PluginStrategyWrapper.h:**
```cpp
class PluginStrategyWrapper : public StrategyBase {
    Q_OBJECT

public:
    explicit PluginStrategyWrapper(IStrategyPlugin* plugin, QObject* parent = nullptr);
    ~PluginStrategyWrapper();

    void init(const StrategyInstance& instance) override;
    void start() override;
    void stop() override;

protected slots:
    void onTick(const UDP::MarketTick& tick) override;

private:
    static void hostPlaceOrder(void* context, const OrderRequest* order);
    static void hostCancelOrder(void* context, const char* orderId);
    static void hostLog(void* context, const char* message);
    static double hostGetParameter(void* context, const char* key);

    IStrategyPlugin* m_plugin;
    HostFunctions m_hostFunctions;
};
```

### Example Plugin Implementation (Client-side)

**Client develops this DLL using the SDK:**

**MyCustomStrategy.cpp:**
```cpp
#include "IStrategyPlugin.h"
#include <cstring>
#include <cstdio>

struct MyStrategyContext {
    const HostFunctions* host;
    double entryPrice;
    bool inPosition;
    double rsiValue;
};

void onTickImpl(void* context, const MarketTick* tick) {
    MyStrategyContext* ctx = (MyStrategyContext*)context;
    
    // Simple RSI-based logic (simplified)
    // In real implementation, calculate RSI properly
    
    if (!ctx->inPosition && tick->ltp < ctx->entryPrice * 0.98) {
        // Entry signal
        OrderRequest order;
        order.symbol = "NIFTY";
        order.side = "BUY";
        order.quantity = 50;
        order.price = tick->ltp;
        order.orderType = "MARKET";
        
        ctx->host->placeOrder(context, &order);
        ctx->inPosition = true;
        ctx->host->log(context, "Entered BUY position");
    } else if (ctx->inPosition && tick->ltp > ctx->entryPrice * 1.02) {
        // Exit signal
        ctx->host->log(context, "Exit target hit");
        // Place sell order...
    }
}

void initImpl(void* context, const HostFunctions* host) {
    MyStrategyContext* ctx = (MyStrategyContext*)context;
    ctx->host = host;
    ctx->entryPrice = host->getParameter(context, "entry_price");
    ctx->inPosition = false;
    host->log(context, "MyCustomStrategy initialized");
}

void startImpl(void* context) {
    MyStrategyContext* ctx = (MyStrategyContext*)context;
    ctx->host->subscribeMarketData(context, 256265); // NIFTY token
    ctx->host->log(context, "MyCustomStrategy started");
}

void stopImpl(void* context) {
    MyStrategyContext* ctx = (MyStrategyContext*)context;
    ctx->host->log(context, "MyCustomStrategy stopped");
}

void destroyImpl(void* context) {
    MyStrategyContext* ctx = (MyStrategyContext*)context;
    delete ctx;
}

extern "C" {

STRATEGY_API const StrategyPluginInfo* GetPluginInfo() {
    static StrategyPluginInfo info = {
        "MyCustomStrategy",
        "1.0.0",
        "Client Name",
        "Custom RSI-based strategy",
        1 // API version
    };
    return &info;
}

STRATEGY_API IStrategyPlugin* CreateStrategy() {
    IStrategyPlugin* plugin = new IStrategyPlugin();
    MyStrategyContext* ctx = new MyStrategyContext();
    
    plugin->context = ctx;
    plugin->init = initImpl;
    plugin->start = startImpl;
    plugin->stop = stopImpl;
    plugin->destroy = destroyImpl;
    plugin->onTick = onTickImpl;
    plugin->onOrderUpdate = nullptr;
    
    return plugin;
}

STRATEGY_API void DestroyStrategy(IStrategyPlugin* strategy) {
    if (strategy) {
        if (strategy->destroy) {
            strategy->destroy(strategy->context);
        }
        delete strategy;
    }
}

} // extern "C"
```

### SDK Distribution

**Provide clients with:**

1. **SDK Package:**
   - `IStrategyPlugin.h` (C API header)
   - `build_plugin.bat` / `build_plugin.sh` (build script)
   - Example plugin source code
   - Documentation PDF

2. **Build Script (build_plugin.bat):**
```batch
@echo off
REM Build strategy plugin for Trading Terminal

set GCC_PATH=C:\mingw64\bin
set OUTPUT_NAME=MyCustomStrategy.dll

%GCC_PATH%\g++ -shared -o %OUTPUT_NAME% ^
    -DSTRATEGY_PLUGIN_EXPORTS ^
    -std=c++17 ^
    MyCustomStrategy.cpp

echo Plugin built: %OUTPUT_NAME%
pause
```

3. **Documentation:**
   - API reference
   - Callback sequence diagrams
   - Parameter access guide
   - Order placement examples
   - Debugging tips

### Security & Validation

**Plugin validation checklist:**
- ✅ Check API version compatibility
- ✅ Verify plugin signature (optional: code signing)
- ✅ Sandbox plugin execution (if possible)
- ✅ Limit plugin permissions (no file system access, etc.)
- ✅ Timeout protection on callbacks
- ✅ Exception handling around plugin calls

---

## Implementation Roadmap

### Phase 1: TradingView Chart Integration (4-6 weeks)

**Week 1-2: Setup & Basic Integration**
- [ ] Add Qt WebEngine dependency to CMakeLists
- [ ] Create ChartWindow class with QWebEngineView
- [ ] Implement QWebChannel bridge (ChartBridge)
- [ ] Create basic HTML/JS chart template (Lightweight Charts)
- [ ] Test C++ ↔ JS communication

**Week 3-4: Data Integration**
- [ ] Implement OHLCV candle builder from UDP ticks
- [ ] Connect FeedHandler to ChartWindow
- [ ] Historical data fetching service
- [ ] Real-time candle updates
- [ ] Test with live market data

**Week 5-6: Indicators & Enhancements**
- [ ] Implement IndicatorBase framework
- [ ] Add SMA, EMA, RSI, MACD indicators
- [ ] Indicator calculation on candle close
- [ ] Trade marker visualization on chart
- [ ] Multi-timeframe support (1min, 5min, 15min)

### Phase 2: Custom Strategy Builder (6-8 weeks)

**Week 1-2: JSON Strategy Schema**
- [ ] Design comprehensive JSON schema
- [ ] Implement StrategyInterpreter class
- [ ] Condition evaluation engine
- [ ] Action execution framework
- [ ] Parameter templating system

**Week 3-4: CustomStrategy Implementation**
- [ ] CustomStrategy class (inherits StrategyBase)
- [ ] JSON parser integration
- [ ] Indicator management in custom strategies
- [ ] Entry/exit condition evaluation
- [ ] Order placement via actions

**Week 5-6: Visual Builder UI (Optional - can defer)**
- [ ] QGraphicsScene-based canvas
- [ ] Block palette (conditions, actions, operators)
- [ ] Drag-drop block system
- [ ] Connection/data flow visualization
- [ ] JSON export from visual blocks

**Week 7-8: Testing & Documentation**
- [ ] Unit tests for condition evaluation
- [ ] Integration tests with live data
- [ ] Example strategy templates
- [ ] User documentation
- [ ] Video tutorial

### Phase 3: Plugin System (4-5 weeks)

**Week 1: API Design**
- [ ] Finalize IStrategyPlugin.h C API
- [ ] Design HostFunctions interface
- [ ] Create SDK documentation
- [ ] Example plugin source code

**Week 2-3: Plugin Manager Implementation**
- [ ] StrategyPluginManager class
- [ ] DLL loading (QLibrary)
- [ ] Plugin validation & versioning
- [ ] PluginStrategyWrapper class
- [ ] Exception handling & safety

**Week 4-5: SDK & Testing**
- [ ] Build SDK package for clients
- [ ] Create sample plugin project
- [ ] Test plugin loading/unloading
- [ ] Performance testing
- [ ] Security audit

---

## Technical Challenges & Solutions

### Challenge 1: Qt WebEngine Size & Dependencies

**Problem:** Qt WebEngine adds ~100MB to application size.

**Solutions:**
1. **Use Lightweight Charts (TradingView's open-source lib)**
   - Much smaller footprint
   - Fewer features but adequate for basic charting
   
2. **Dynamic loading of WebEngine**
   - Only load WebEngine DLL when user opens chart
   - Check if installed before loading

3. **Alternative: Native Qt Charts (QChart)**
   - Pure Qt solution, no web engine
   - More limited, but lightweight
   - Custom indicator rendering more complex

**Recommendation:** Start with Lightweight Charts, offer TradingView as premium option.

### Challenge 2: Real-time Chart Performance

**Problem:** High-frequency UDP ticks (1000+ per second) overwhelming chart updates.

**Solutions:**
1. **Candle Aggregation:**
   - Aggregate ticks into 1-second candles
   - Only update chart on candle close
   - Use QTimer for throttling

2. **WebSocket Batching:**
   - Batch multiple candle updates
   - Send at fixed interval (e.g., 250ms)

3. **Chart Decimation:**
   - Show only visible timeframe data
   - Lazy-load historical data on zoom

### Challenge 3: Custom Strategy Performance

**Problem:** JSON parsing and condition evaluation overhead on every tick.

**Solutions:**
1. **Strategy Compilation:**
   - Parse JSON once on load
   - Compile to internal bytecode/structure
   - Cache indicator calculations

2. **Event-driven Updates:**
   - Only evaluate conditions on candle close
   - Skip evaluation if no trading hours

3. **Native Strategy Fallback:**
   - For high-frequency strategies, suggest native C++ implementation

### Challenge 4: Plugin Security

**Problem:** Malicious or buggy plugins crashing app.

**Solutions:**
1. **API Version Enforcement:**
   - Reject plugins with mismatched API versions
   - Use semantic versioning

2. **Callback Timeout:**
   ```cpp
   QTimer::singleShot(5000, [&]() {
       if (tickInProgress) {
           qWarning() << "Plugin tick callback timeout!";
           unloadPlugin();
       }
   });
   ```

3. **Exception Handling:**
   ```cpp
   try {
       plugin->onTick(ctx, &tick);
   } catch (...) {
       qCritical() << "Plugin threw exception, unloading";
       markPluginAsBad();
   }
   ```

4. **Resource Limits:**
   - Monitor plugin memory usage
   - Limit max execution time per callback

---

## Conclusion

This roadmap provides three major feature additions:

1. **TradingView Charts:** Professional charting with indicators for visual analysis
2. **Custom Strategy Builder:** Empowers non-programmers to create strategies
3. **Plugin System:** Allows clients to extend without source access

**Recommended Priority:**
1. **Start with Charts (Phase 1):** High visual impact, immediate value
2. **Then Plugin System (Phase 3):** Enables client extensibility early
3. **Finally Strategy Builder (Phase 2):** Complex but powerful long-term feature

**Estimated Total Timeline:** 14-19 weeks (~3-5 months)

**Next Steps:**
1. Review and approve architecture
2. Decide on TradingView vs Lightweight Charts
3. Setup dev environment with Qt WebEngine
4. Begin Phase 1 implementation

---

**Document Version:** 1.0  
**Last Updated:** February 10, 2026  
**Status:** Draft - Awaiting Review
