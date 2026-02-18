# TA-Lib Integration with QChart - Complete Guide

**Date:** February 12, 2026  
**Status:** ✅ Implementation Complete - Ready for Testing

---

## 📦 Installation & Setup

### Step 1: Install TA-Lib

#### Windows (MSVC)
```powershell
# Download TA-Lib from https://ta-lib.org/
# Extract to C:\ta-lib (or preferred location)

# Verify structure:
# C:\ta-lib\
#   ├── include\
#   │   └── ta-lib\
#   │       └── ta_libc.h
#   └── lib\
#       └── ta_lib.lib (or ta_libc_cdr.lib for Release)
```

#### Linux
```bash
# Download source
wget https://sourceforge.net/projects/ta-lib/files/ta-lib/0.4.0/ta-lib-0.4.0-src.tar.gz
tar -xzf ta-lib-0.4.0-src.tar.gz
cd ta-lib/

# Build and install
./configure --prefix=/usr
make
sudo make install
```

### Step 2: Update CMakeLists.txt

Add these lines to your `CMakeLists.txt`:

```cmake
# ============================================================================
# TA-Lib Integration
# ============================================================================

option(ENABLE_TALIB "Enable TA-Lib technical indicators" ON)

if(ENABLE_TALIB)
    # Set TA-Lib root directory (adjust path as needed)
    if(WIN32)
        set(TALIB_ROOT "C:/ta-lib" CACHE PATH "TA-Lib installation directory")
    else()
        set(TALIB_ROOT "/usr" CACHE PATH "TA-Lib installation directory")
    endif()
    
    set(TALIB_INCLUDE_DIR "${TALIB_ROOT}/include")
    
    if(WIN32)
        if(CMAKE_BUILD_TYPE MATCHES Debug)
            set(TALIB_LIBRARY "${TALIB_ROOT}/lib/ta_libc_cmd.lib")
        else()
            set(TALIB_LIBRARY "${TALIB_ROOT}/lib/ta_libc_cdr.lib")
        endif()
    else()
        set(TALIB_LIBRARY "${TALIB_ROOT}/lib/libta_lib.so")
    endif()
    
    # Check if TA-Lib was found
    if(EXISTS "${TALIB_INCLUDE_DIR}/ta-lib/ta_libc.h")
        message(STATUS "TA-Lib found: ${TALIB_ROOT}")
        add_definitions(-DHAVE_TALIB)
        include_directories(${TALIB_INCLUDE_DIR})
    else()
        message(WARNING "TA-Lib not found at ${TALIB_ROOT}. Indicators will be disabled.")
        set(ENABLE_TALIB OFF)
    endif()
endif()

# ============================================================================
# Source Files - Add Indicators
# ============================================================================

# Indicator sources
set(INDICATOR_SOURCES
    src/indicators/TALibIndicators.cpp
)

set(INDICATOR_HEADERS
    include/indicators/TALibIndicators.h
)

# UI sources - Add IndicatorChartWidget
list(APPEND UI_SOURCES src/ui/IndicatorChartWidget.cpp)
list(APPEND UI_HEADERS include/ui/IndicatorChartWidget.h)

# Add to executable
add_executable(TradingTerminal
    # ... existing sources ...
    ${INDICATOR_SOURCES}
    ${INDICATOR_HEADERS}
)

# Link TA-Lib
if(ENABLE_TALIB)
    target_link_libraries(TradingTerminal ${TALIB_LIBRARY})
endif()
```

### Step 3: Initialize TA-Lib in main.cpp

```cpp
#include "indicators/TALibIndicators.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Initialize TA-Lib
    if (!TALibIndicators::initialize()) {
        qWarning() << "TA-Lib initialization failed. Indicators will not be available.";
    } else {
        qDebug() << "TA-Lib initialized:" << TALibIndicators::getVersion();
    }
    
    // ... rest of application setup ...
    
    // Shutdown TA-Lib on exit
    QObject::connect(&app, &QApplication::aboutToQuit, []() {
        TALibIndicators::shutdown();
    });
    
    return app.exec();
}
```

---

## 🎨 Using IndicatorChartWidget

### Basic Usage

```cpp
#include "ui/IndicatorChartWidget.h"

// Create chart widget
IndicatorChartWidget* chartWidget = new IndicatorChartWidget(parentWindow);

// Load symbol
chartWidget->loadSymbol("NIFTY", 2, 26000);  // segment=2 (NSE FO), token=26000

// Set candle data (e.g., from CandleAggregator)
QVector<ChartData::Candle> candles;
// ... populate candles from HistoricalDataStore or CandleAggregator ...
chartWidget->setCandleData(candles);

// Add indicators
QVariantMap smaParams;
smaParams["period"] = 20;
chartWidget->addOverlayIndicator("SMA 20", "SMA", smaParams);

QVariantMap rsiParams;
rsiParams["period"] = 14;
chartWidget->addPanelIndicator("RSI 14", "RSI", rsiParams);
```

### Real-Time Updates

```cpp
// Connect to CandleAggregator for real-time updates
connect(candleAggregator, &CandleAggregator::candleComplete,
        chartWidget, [chartWidget](const QString& symbol, int segment,
                                   const QString& timeframe,
                                   const ChartData::Candle& candle) {
    chartWidget->appendCandle(candle);
});

// Update current candle (partial updates)
connect(candleAggregator, &CandleAggregator::candleUpdate,
        chartWidget, [chartWidget](const QString& symbol, int segment,
                                   const QString& timeframe,
                                   const ChartData::Candle& candle) {
    chartWidget->updateCurrentCandle(candle);
});
```

### Adding Custom Indicators

```cpp
// Simple Moving Average (overlay on price chart)
QVariantMap params;
params["period"] = 50;
chartWidget->addOverlayIndicator("SMA 50", "SMA", params);

// Exponential Moving Average
params["period"] = 12;
chartWidget->addOverlayIndicator("EMA 12", "EMA", params);

// Bollinger Bands
params["period"] = 20;
params["nbDevUp"] = 2.0;
params["nbDevDn"] = 2.0;
chartWidget->addOverlayIndicator("BB 20", "BB", params);

// RSI (separate panel below price chart)
params.clear();
params["period"] = 14;
chartWidget->addPanelIndicator("RSI 14", "RSI", params);

// MACD (separate panel)
params.clear();
params["fastPeriod"] = 12;
params["slowPeriod"] = 26;
params["signalPeriod"] = 9;
chartWidget->addPanelIndicator("MACD", "MACD", params);

// Stochastic Oscillator
chartWidget->addPanelIndicator("Stochastic", "STOCH", QVariantMap());

// ATR (Average True Range)
params.clear();
params["period"] = 14;
chartWidget->addPanelIndicator("ATR 14", "ATR", params);
```

---

## 📊 Available Indicators

### Overlay Indicators (on price chart)

| Indicator | Type Code | Parameters | Description |
|-----------|-----------|------------|-------------|
| Simple Moving Average | `SMA` | `period` (default: 20) | Average price over period |
| Exponential Moving Average | `EMA` | `period` (default: 20) | Weighted average favoring recent |
| Weighted Moving Average | `WMA` | `period` (default: 20) | Linear weighted average |
| Bollinger Bands | `BB` | `period` (20), `nbDevUp` (2.0), `nbDevDn` (2.0) | Volatility bands |

### Panel Indicators (separate chart)

| Indicator | Type Code | Parameters | Description |
|-----------|-----------|------------|-------------|
| Relative Strength Index | `RSI` | `period` (default: 14) | Momentum (0-100) |
| MACD | `MACD` | `fastPeriod` (12), `slowPeriod` (26), `signalPeriod` (9) | Trend following |
| Stochastic Oscillator | `STOCH` | `fastKPeriod` (5), `slowKPeriod` (3), `slowDPeriod` (3) | Momentum (0-100) |
| Average True Range | `ATR` | `period` (default: 14) | Volatility measure |
| Commodity Channel Index | `CCI` | `period` (default: 14) | Trend strength |

### Volume Indicators

| Indicator | Type Code | Description |
|-----------|-----------|-------------|
| On-Balance Volume | `OBV` | Cumulative volume flow |
| Chaikin A/D Line | `AD` | Accumulation/Distribution |
| Money Flow Index | `MFI` | Volume-weighted RSI |

---

## 🔍 Pattern Recognition

TA-Lib includes 50+ candlestick patterns. Here's how to use them:

```cpp
auto prices = TALibIndicators::extractPrices(candles);

// Detect Doji pattern
QVector<int> dojiPattern = TALibIndicators::detectDoji(
    prices.opens, prices.highs, prices.lows, prices.closes);

// Pattern values: 
//   +100 = bullish pattern (strong)
//   -100 = bearish pattern (strong)
//      0 = no pattern detected

for (int i = 0; i < dojiPattern.size(); i++) {
    if (dojiPattern[i] != 0) {
        qDebug() << "Doji detected at index" << i 
                 << "strength:" << dojiPattern[i];
        // Add marker to chart
    }
}

// Other patterns available:
auto hammer = TALibIndicators::detectHammer(prices.opens, prices.highs, 
                                           prices.lows, prices.closes);
auto engulfing = TALibIndicators::detectEngulfing(...);
auto morningStar = TALibIndicators::detectMorningStar(...);
auto eveningStar = TALibIndicators::detectEveningStar(...);
auto shootingStar = TALibIndicators::detectShootingStar(...);
auto hangingMan = TALibIndicators::detectHangingMan(...);
```

---

## 🎯 Integration with MainWindow

### Create Chart Window with Indicators

```cpp
// In MainWindow::createIndicatorChartWindow()
CustomMDISubWindow* MainWindow::createIndicatorChartWindow() {
    static int counter = 1;
    
    CustomMDISubWindow* window = new CustomMDISubWindow(
        QString("Indicator Chart %1").arg(counter++), m_mdiArea);
    window->setWindowType("IndicatorChart");
    
    IndicatorChartWidget* chartWidget = new IndicatorChartWidget(window);
    window->setContentWidget(chartWidget);
    window->resize(1400, 900);
    
    // Get best window context (symbol from active MarketWatch)
    WindowContext context = getBestWindowContext();
    
    if (context.isValid()) {
        chartWidget->loadSymbol(context.symbol, 
                               RepositoryManager::getExchangeSegmentID(
                                   "NSE", context.segment),
                               context.token);
        
        // Load historical data
        // TODO: Connect to HistoricalDataStore
        
        // Add default indicators
        QVariantMap params;
        params["period"] = 20;
        chartWidget->addOverlayIndicator("SMA 20", "SMA", params);
        
        params["period"] = 14;
        chartWidget->addPanelIndicator("RSI 14", "RSI", params);
    }
    
    connectWindowSignals(window);
    
    m_mdiArea->addWindow(window);
    window->show();
    window->setFocus();
    
    return window;
}
```

---

## 🔥 Advanced Usage

### Custom Indicator Calculations

```cpp
// Calculate multiple indicators and compare signals
auto prices = TALibIndicators::extractPrices(candles);

auto rsi = TALibIndicators::calculateRSI(prices.closes, 14);
auto macd = TALibIndicators::calculateMACD(prices.closes, 12, 26, 9);
auto bb = TALibIndicators::calculateBollingerBands(prices.closes, 20, 2.0, 2.0);

// Detect confluence signals
for (int i = 50; i < candles.size(); i++) {
    bool rsiOversold = rsi[i] < 30;
    bool macdBullish = macd.macdLine[i] > macd.signalLine[i];
    bool priceAtLowerBB = prices.closes[i] <= bb.lowerBand[i];
    
    if (rsiOversold && macdBullish && priceAtLowerBB) {
        qDebug() << "⚡ STRONG BUY SIGNAL at index" << i;
        qDebug() << "  RSI:" << rsi[i];
        qDebug() << "  MACD:" << macd.macdLine[i] << "Signal:" << macd.signalLine[i];
        qDebug() << "  Price:" << prices.closes[i] << "BB Lower:" << bb.lowerBand[i];
        
        // Add buy marker to chart
        // or trigger automated order
    }
}
```

### Strategy Backtesting

```cpp
struct BacktestResult {
    double totalReturn = 0.0;
    int totalTrades = 0;
    int winningTrades = 0;
    double maxDrawdown = 0.0;
};

BacktestResult backtestRSIStrategy(const QVector<ChartData::Candle>& candles) {
    BacktestResult result;
    
    auto prices = TALibIndicators::extractPrices(candles);
    auto rsi = TALibIndicators::calculateRSI(prices.closes, 14);
    
    bool inPosition = false;
    double entryPrice = 0.0;
    double maxEquity = 10000.0;  // Starting capital
    double equity = maxEquity;
    
    for (int i = 20; i < candles.size(); i++) {
        if (!inPosition && rsi[i] < 30) {
            // BUY signal
            inPosition = true;
            entryPrice = prices.closes[i];
            result.totalTrades++;
            qDebug() << "BUY at" << entryPrice << "RSI:" << rsi[i];
        }
        else if (inPosition && rsi[i] > 70) {
            // SELL signal
            double exitPrice = prices.closes[i];
            double pnl = exitPrice - entryPrice;
            equity += pnl;
            
            if (pnl > 0) result.winningTrades++;
            
            result.totalReturn += (pnl / entryPrice) * 100.0;
            
            // Track drawdown
            if (equity > maxEquity) maxEquity = equity;
            double drawdown = ((maxEquity - equity) / maxEquity) * 100.0;
            if (drawdown > result.maxDrawdown) {
                result.maxDrawdown = drawdown;
            }
            
            inPosition = false;
            qDebug() << "SELL at" << exitPrice << "P&L:" << pnl 
                     << "RSI:" << rsi[i];
        }
    }
    
    qDebug() << "=== Backtest Results ===";
    qDebug() << "Total Return:" << result.totalReturn << "%";
    qDebug() << "Total Trades:" << result.totalTrades;
    qDebug() << "Win Rate:" << (result.totalTrades > 0 ? 
        (result.winningTrades * 100.0 / result.totalTrades) : 0) << "%";
    qDebug() << "Max Drawdown:" << result.maxDrawdown << "%";
    
    return result;
}
```

### Automated Trading with Indicators

```cpp
class IndicatorTriggerEngine : public QObject {
    Q_OBJECT
    
public:
    void connectToChart(IndicatorChartWidget* chart) {
        // Monitor candle updates
        connect(chart, &IndicatorChartWidget::candleUpdated, this,
                [this, chart]() {
            evaluateTriggers(chart);
        });
    }
    
    void addTrigger(const QString& name, 
                   std::function<bool(const QVector<ChartData::Candle>&)> condition) {
        m_triggers[name] = condition;
    }
    
signals:
    void triggerFired(const QString& triggerName, const QString& action);
    
private:
    QHash<QString, std::function<bool(const QVector<ChartData::Candle>&)>> m_triggers;
    
    void evaluateTriggers(IndicatorChartWidget* chart) {
        auto candles = chart->getCandleData();
        if (candles.size() < 50) return;
        
        for (auto it = m_triggers.begin(); it != m_triggers.end(); ++it) {
            if (it.value()(candles)) {
                emit triggerFired(it.key(), "BUY");  // or "SELL"
            }
        }
    }
};

// Usage:
IndicatorTriggerEngine* engine = new IndicatorTriggerEngine(this);
engine->connectToChart(chartWidget);

// Add RSI oversold trigger
engine->addTrigger("RSI Oversold", [](const QVector<ChartData::Candle>& candles) {
    auto prices = TALibIndicators::extractPrices(candles);
    auto rsi = TALibIndicators::calculateRSI(prices.closes, 14);
    return rsi.last() < 30 && rsi[rsi.size() - 2] >= 30;  // Just crossed below 30
});

// Connect to order placement
connect(engine, &IndicatorTriggerEngine::triggerFired, mainWindow,
        [mainWindow](const QString& trigger, const QString& action) {
    qDebug() << "🔔 Trigger fired:" << trigger << "Action:" << action;
    
    // Place order
    XTS::OrderParams params;
    params.orderSide = action;
    // ... fill in other params ...
    mainWindow->placeOrder(params);
});
```

---

## 🎨 Chart Styling & Customization

### Dark Theme (Already Applied)

The `IndicatorChartWidget` uses a professional dark theme by default:
- Background: `#131722`
- Plot area: `#1e222d`
- Text: `#d1d4dc`
- Gridlines: `#363c4e`
- Bullish candles: `#26a69a` (green)
- Bearish candles: `#ef5350` (red)

### Custom Colors for Indicators

```cpp
// After adding indicator, customize colors
auto& info = chartWidget->getIndicatorInfo("SMA 20");
if (info.series1) {
    info.series1->setColor(QColor("#2196f3"));  // Blue
    info.series1->setWidth(2);
}

// For Bollinger Bands
auto& bbInfo = chartWidget->getIndicatorInfo("BB 20");
if (bbInfo.series1) {
    bbInfo.series1->setColor(QColor("#2196f3"));     // Blue for upper
    bbInfo.series2->setColor(QColor("#9c27b0"));     // Purple for middle
    bbInfo.series3->setColor(QColor("#2196f3"));     // Blue for lower
    
    // Make bands semi-transparent
    QPen pen1 = bbInfo.series1->pen();
    pen1.setStyle(Qt::DashLine);
    bbInfo.series1->setPen(pen1);
}
```

---

## 🐛 Troubleshooting

### TA-Lib Not Found

**Error:** `TA-Lib not compiled with TA-Lib support`

**Solution:**
1. Verify TA-Lib is installed at the correct path
2. Check `CMakeLists.txt` has correct `TALIB_ROOT` path
3. Ensure `-DHAVE_TALIB` is defined
4. Rebuild project completely: `cmake --build build --target clean && cmake --build build`

### Indicators Show Empty/Zero Values

**Cause:** Not enough candle data (indicators need minimum period)

**Solution:**
```cpp
// Check data before calculating
if (candles.size() < 50) {
    qWarning() << "Need at least 50 candles for indicator calculation";
    return;
}

// Or use adaptive checking
int period = 20;
if (candles.size() < period * 2) {
    qWarning() << "Insufficient data for period" << period;
}
```

### Chart Performance Issues

**Symptoms:** Slow rendering with many indicators

**Optimizations:**
```cpp
// 1. Disable animation
m_priceChart->setAnimationOptions(QChart::NoAnimation);

// 2. Limit visible candles
chartWidget->setVisibleRange(100);  // Show only last 100 candles

// 3. Update indicators in batches (not every tick)
QTimer* updateTimer = new QTimer(this);
updateTimer->setInterval(500);  // Update every 500ms
connect(updateTimer, &QTimer::timeout, [chartWidget]() {
    chartWidget->recalculateAllIndicators();
});
updateTimer->start();

// 4. Use OpenGL acceleration
m_priceChartView->setRenderHint(QPainter::Antialiasing, false);
QSurfaceFormat format;
format.setSamples(4);
m_priceChartView->setFormat(format);
```

---

## ✅ Testing Checklist

- [ ] TA-Lib initializes successfully
- [ ] SMA overlays correctly on price chart
- [ ] EMA displays with different color
- [ ] Bollinger Bands show 3 lines (upper/middle/lower)
- [ ] RSI panel displays below price (range 0-100)
- [ ] MACD panel shows two lines (MACD + Signal)
- [ ] Stochastic panel displays %K and %D lines
- [ ] Real-time candle updates refresh indicators
- [ ] Zoom In/Out works correctly
- [ ] Add/Remove indicators dynamically
- [ ] Pattern detection returns expected values
- [ ] Backtest runs without errors
- [ ] Chart remains smooth with 5+ indicators

---

## 📚 Next Steps

1. **Integrate with Historical Data:**
   ```cpp
   connect(historicalDataStore, &HistoricalDataStore::dataReady,
           chartWidget, &IndicatorChartWidget::setCandleData);
   ```

2. **Add Indicator Presets:**
   - "Scalping Setup" (EMA 9/21 + RSI)
   - "Swing Trading" (SMA 50/200 + MACD)
   - "ATM Strategy" (Custom indicators)

3. **Strategy Builder UI:**
   - Visual rule builder: "IF RSI < 30 AND MACD > Signal THEN BUY"
   - Backtest button with results display
   - One-click deployment to live trading

4. **Export/Import Indicators:**
   - Save indicator configurations to JSON
   - Share indicator templates between users
   - Load community indicators

---

## 🎓 Learning Resources

- **TA-Lib Documentation:** https://ta-lib.org/function.html
- **TA-Lib GitHub:** https://github.com/mrjbq7/ta-lib
- **Qt Charts Documentation:** https://doc.qt.io/qt-5/qtcharts-index.html
- **Technical Analysis Primer:** *Technical Analysis of the Financial Markets* by John Murphy

---

**Questions or Issues?**
Check logs for TA-Lib initialization (`[TALib]` prefix) and chart updates (`[IndicatorChart]` prefix).

