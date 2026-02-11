# TradingView Chart - Complete Fix & Optimization Plan

## 🔴 CRITICAL BUG: Token Race Condition

### Root Cause
The `historicalDataRequested` signal passes `symbol` and `segment` but NOT the `token`. The lambda in the constructor reads `m_currentToken` which may not be set yet when TradingView calls `getBars()` immediately after `resolveSymbol()`.

**Race Condition Timeline:**
```
T0: User clicks search result "NIFTY 49500 CE"
T1: resolveSymbol() called → dataBridge.loadSymbol(symbol, segment, TOKEN)
T2: loadSymbol() starts → m_currentToken = TOKEN (but may not complete)
T3: TradingView IMMEDIATELY calls getBars()
T4: getBars() triggers historicalDataRequested signal
T5: Lambda executes → reads m_currentToken (might still be 0!)
T6: Result: "No token set for symbol NIFTY" error
```

### Fix Strategy
**Pass token as parameter through the entire chain:**

1. **Modify Signal:** Add `token` parameter to `historicalDataRequested` signal
2. **JavaScript:** Extract token from ticker and pass it to `requestHistoricalData()`
3. **Lambda:** Use token parameter instead of reading `m_currentToken`

---

## ✅ Phase 1: Fix Token Flow (IMMEDIATE)

### Changes Required:

#### 1. Header File: `include/ui/TradingViewChartWidget.h`
```cpp
// Change signal signature
signals:
    void historicalData Requested(const QString& symbol, int segment,
                                 const QString& resolution,
                                 qint64 from, qint64 to, qint64 token);  // ADD token parameter
```

#### 2. Data Bridge: `TradingViewDataBridge::requestHistoricalData()`
```cpp
void TradingViewDataBridge::requestHistoricalData(const QString& symbol, int segment,
                                                  const QString& resolution,
                                                  qint64 from, qint64 to, qint64 token)  // ADD parameter
{
    emit historicalDataRequested(symbol, segment, resolution, 
                                 from / 1000, to / 1000, token);  // PASS token
}
```

#### 3. JavaScript: Parse and pass token
```javascript
getBars: (symbolInfo, resolution, periodParams, onHistoryCallback, onErrorCallback) => {
    const parts = symbolInfo.ticker.split('_');
    const token = parseInt(parts[2]) || 0;  // Extract token
    
    dataBridge.requestHistoricalData(
        symbol, segment, resolution,
        periodParams.from * 1000,
        periodParams.to * 1000,
        token  // PASS token to C++
    );
}
```

#### 4. Constructor Lambda: Use parameter token
```cpp
connect(m_dataBridge, &TradingViewDataBridge::historicalDataRequested,
        this, [this](const QString& symbol, int segment, const QString& resolution,
                    qint64 from, qint64 to, qint64 token) {  // RECEIVE token
            
            // Use parameter token instead of m_currentToken
            int exchangeInstrumentID = token;  // FIXED!
            
            if (exchangeInstrumentID == 0) {
                qWarning() << "[TradingViewChart] No token provided for symbol" << symbol;
                // ...send empty data
            }
            // ... rest of API call
        });
```

---

## 🎨 Phase 2: Search UI/UX Improvements

### A. Result Grouping & Sorting

**Goal:** Organize results by type, sort intelligently

```cpp
// Separate results by type
QVector<ContractData> futures;
QVector<ContractData> callOptions;
QVector<ContractData> putOptions;
QVector<ContractData> equity;

for (const ContractData& contract : allResults) {
    if (contract.instrumentType == 1) {  // Future
        futures.append(contract);
    } else if (contract.instrumentType == 2) {  // Option
        if (contract.optionType.contains("C", Qt::CaseInsensitive)) {
            callOptions.append(contract);
        } else {
            putOptions.append(contract);
        }
    } else {  // Equity
        equity.append(contract);
    }
}

// Sort by expiry (nearest first), then strike (ATM centered)
auto sortByExpiry = [](const ContractData& a, const ContractData& b) {
    return a.expiryDate_dt < b.expiryDate_dt;
};
std::sort(futures.begin(), futures.end(), sortByExpiry);
std::sort(callOptions.begin(), callOptions.end(), sortByExpiry);
std::sort(putOptions.begin(), putOptions.end(), sortByExpiry);

// Add section headers in JSON
jsonResults.append(createSectionHeader("📈 Futures"));
jsonResults.append(...futures);
jsonResults.append(createSectionHeader("🟢 Call Options"));
jsonResults.append(...callOptions);
jsonResults.append(createSectionHeader("🔴 Put Options"));
jsonResults.append(...putOptions);
```

### B. Smart Strike Filtering

**Goal:** Only show relevant strikes (±10% from current price)

```cpp
double ltp = contract.ltp > 0 ? contract.ltp : contract.prevClose;
double atmStrike = qRound(ltp / 100) * 100;  // Round to nearest 100

auto isRelevantStrike = [atmStrike](const ContractData& contract) {
    double deviation = qAbs(contract.strikePrice - atmStrike) / atmStrike;
    return deviation <= 0.10;  // Within ±10%
};

QVector<ContractData> filteredOptions;
std::copy_if(allOptions.begin(), allOptions.end(),
             std::back_inserter(filteredOptions),
             isRelevantStrike);
```

### C. Search Performance

**1. Debouncing (JavaScript):**
```javascript
let searchTimeout;
function debouncedSearch(userInput) {
    clearTimeout(searchTimeout);
    searchTimeout = setTimeout(() => {
        dataBridge.searchSymbols(userInput, exchange, segment);
    }, 300);  // 300ms delay
}
```

**2. Result Caching (C++):**
```cpp
struct SearchCache {
    QString query;
    QJsonArray results;
    qint64 timestamp;
};
static QHash<QString, SearchCache> searchCache;

// Check cache first
QString cacheKey = searchText + exchange + segment;
if (searchCache.contains(cacheKey)) {
    auto& cache = searchCache[cacheKey];
    if (QDateTime::currentMSecsSinceEpoch() - cache.timestamp < 5000) {
        emit symbolSearchResults(cache.results);  // Return cached
        return;
    }
}
```

---

## 📊 Phase 3: API & Data Optimizations

### A. Historical Data Caching

```cpp
struct HistoricalDataCache {
    QString key;  // "TOKEN_RESOLUTION_FROM_TO"
    QJsonArray bars;
    qint64 timestamp;
};
static QHash<QString, HistoricalDataCache> histDataCache;

QString cacheKey = QString("%1_%2_%3_%4").arg(token).arg(resolution).arg(from).arg(to);
if (histDataCache.contains(cacheKey)) {
    // Return cached if < 1 minute old
    // ...
}
```

### B. Progressive Loading

```javascript
getBars: (symbolInfo, resolution, periodParams, onHistoryCallback, onErrorCallback) => {
    // First request: Load last 100 bars
    const requestBars = Math.min(100, Math.ceil((periodParams.to - periodParams.from) / 60));
    const adjustedFrom = periodParams.to - (requestBars * 60);
    
    // Request limited data
    dataBridge.requestHistoricalData(symbol, segment, resolution, adjustedFrom, periodParams.to, token);
    
    // Store original range for subsequent requests
    window.fullRange = { from: periodParams.from, to: periodParams.to };
}
```

### C. Loading Indicator

```javascript
function showLoadingOverlay() {
    widget.showLoadingScreen({ icon: 'clock', text: 'Loading historical data...' });
}

function hideLoadingOverlay() {
    widget.hideLoadingScreen();
}
```

---

## 🚀 Phase 4: Advanced Features (Future)

### A. WebSocket Real-time Integration

**Goal:** Replace tick aggregation with direct WebSocket candle events

```cpp
// Listen to XTS WebSocket "1505-json-full" event
void TradingViewChartWidget::onWebSocketCandle(const QJsonObject& candleData) {
    // Parse candle from WebSocket
    QJsonObject bar;
    bar["time"] = candleData["timestamp"].toVariant().toLongLong();
    bar["open"] = candleData["open"].toDouble();
    bar["high"] = candleData["high"].toDouble();
    bar["low"] = candleData["low"].toDouble();
    bar["close"] = candleData["close"].toDouble();
    bar["volume"] = candleData["volume"].toDouble();
    
    m_dataBridge->sendRealtimeBar(bar);
}
```

### B. Custom Search Dialog (Qt Widget)

Instead of TradingView's default search, create custom dialog:

```cpp
class AdvancedSymbolSearchDialog : public QDialog {
    // Exchange combo: NSE/BSE
    // Segment combo: CM/FO
    // Expiry combo: Current, Next, Monthly, etc.
    // Strike range slider
    // Results table with filtering
};
```

---

## 📌 Implementation Priority

### Week 1: Critical Fixes
- [ ] Fix token race condition
- [ ] Add comprehensive logging
- [ ] Test with multiple symbols

### Week 2: Search Improvements
- [ ] Group results by type
- [ ] Smart strike filtering
- [ ] Performance optimizations

### Week 3: Data Optimizations
- [ ] Add caching layers
- [ ] Progressive loading
- [ ] Loading indicators

### Week 4: Advanced Features
- [ ] WebSocket integration
- [ ] Custom search dialog
- [ ] Advanced charting features

---

## 🧪 Testing Checklist

- [ ] Search for "NIFTY" → Shows futures and options
- [ ] Select "NIFTY 24000 CE" → Chart loads with data
- [ ] Check browser console → No token errors
- [ ] Check terminal logs → Token flow visible
- [ ] Switch between symbols → No stale data
- [ ] Open multiple charts → Each has correct data
- [ ] Test with BSE symbols → Works correctly
- [ ] Test with equity → Shows cash market data

---

**Next Step:** Implement Phase 1 (Token Fix) first, then test thoroughly before moving to Phase 2.
