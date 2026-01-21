# ATM Watch + Greeks - Implementation Strategy & Guidance

## Executive Summary

**RECOMMENDATION: Implement ATM Watch FIRST, then add Greeks as enhancement**

**Key Decision Factors:**
1. ✅ Greeks data available in NSEFO UDP broadcast (no calculation needed initially)
2. ✅ ATM watch provides immediate trading value without Greeks
3. ✅ Phased approach reduces complexity and risk
4. ✅ Can test and refine ATM mechanism independently
5. ✅ Greeks integration is additive, not foundational

---

## 1. Implementation Sequence Analysis

### Option A: ATM Watch → Greeks (RECOMMENDED ✅)

**Phase 1: ATM Watch Core (2-3 weeks)**
- Implement strike selection mechanism
- Subscription management
- Basic UI with Price, LTP, Change%, Volume, OI
- Test with real market data
- **Deliverable:** Functional ATM watch for trading

**Phase 2: Greeks Integration (1-2 weeks)**
- Extract Greeks from NSEFO UDP feed
- Store Greeks in PriceCache
- Add Greeks columns to ATM watch UI
- **Deliverable:** Enhanced ATM watch with Greeks

**Advantages:**
- ✅ Faster time to first deliverable
- ✅ Can validate ATM selection logic independently
- ✅ Users get value sooner
- ✅ Easier debugging (fewer moving parts)
- ✅ Can refine UI based on feedback before adding Greeks
- ✅ Reduced complexity during development

**Disadvantages:**
- ⚠️ Need to modify UI/data model later
- ⚠️ Two deployment cycles

---

### Option B: ATM Watch + Greeks Together

**Single Phase (4-5 weeks)**
- Implement everything at once
- ATM watch + Greeks extraction + UI

**Advantages:**
- ✅ Single deployment
- ✅ Complete feature from day one
- ✅ No schema changes later

**Disadvantages:**
- ❌ Longer development cycle
- ❌ Complex debugging (multiple new components)
- ❌ Higher risk of delays
- ❌ Can't iterate on ATM logic independently
- ❌ Users wait longer for any value

---

### Option C: Greeks First → ATM Watch

**Why NOT Recommended:**
- Greeks without ATM watch have limited standalone value
- ATM watch is the "killer feature" for options traders
- Greeks are enhancement to ATM watch, not vice versa
- Delays the high-value feature

---

## 2. Recommended Approach: Phased Implementation

### Phase 1: ATM Watch Core (Week 1-3)

**Scope:**
```
ATM Watch v1.0 (Without Greeks)
├── ATM strike calculation
├── Subscription management  
├── UI columns:
│   ├── Strike Price
│   ├── Call LTP / Put LTP
│   ├── Call Change% / Put Change%
│   ├── Call Volume / Put Volume
│   ├── Call OI / Put OI
│   └── Call Bid-Ask / Put Bid-Ask
└── Real-time updates
```

**Why This Works:**
- Provides immediate value to traders
- Core trading data (price, OI, volume) is sufficient for decisions
- Can validate the ATM selection mechanism
- Users can start using it while Greeks are in development

---

### Phase 2: Greeks Integration (Week 4-5)

**Scope:**
```
ATM Watch v2.0 (With Greeks)
├── Extract Greeks from NSEFO UDP
├── Store Greeks in PriceCache
├── Add UI columns:
│   ├── Call Delta / Put Delta
│   ├── Call Gamma / Put Gamma
│   ├── Call Theta / Put Theta
│   ├── Call Vega / Put Vega
│   └── Call IV / Put IV
└── Greeks-based sorting/filtering
```

**Integration Points:**
1. NSEFO UDP Reader → Parse Greeks fields
2. PriceCache → Store Greeks alongside price
3. ATM Watch UI → Add Greeks columns
4. Optional: Greeks calculator for fallback (if UDP Greeks missing)

---

## 3. Greeks in NSEFO UDP Broadcast

### 3.1 What Greeks are Available?

**Typical NSEFO Broadcast includes:**
- ✅ Implied Volatility (IV)
- ✅ Delta
- ✅ Gamma  
- ✅ Theta
- ✅ Vega
- ✅ Vanna (sometimes)
- ✅ Charm (sometimes)

**Need to Verify in Your Feed:**
- Check NSEFO protocol documentation
- Parse sample broadcast packets
- Confirm which Greeks are actually populated

### 3.2 Greeks Extraction Architecture

```cpp
// In NSEFO UDP packet structure (example)
struct NSEFOMarketUpdate {
    char symbol[30];
    double ltp;
    double open, high, low, close;
    int64_t volume;
    int64_t oi;
    
    // Greeks fields (need to confirm actual field names)
    double implied_volatility;  // IV
    double delta;
    double gamma;
    double theta;
    double vega;
    double rho;
    
    // ... other fields ...
};
```

**Extraction Flow:**
```
NSEFO UDP Reader
    ↓ Parse packet
    ↓ Extract Greeks
PriceData struct (enhanced)
    ↓ Store Greeks
PriceCache::updatePrice()
    ↓ Emit priceUpdated signal (with Greeks)
ATM Watch Instance
    ↓ Display in UI
```

### 3.3 PriceData Structure Enhancement

```cpp
// Current (simplified)
struct PriceData {
    QString symbol;
    double ltp;
    double change_percent;
    int64_t volume;
    int64_t oi;
    // ... other fields ...
};

// Enhanced for Greeks
struct PriceData {
    QString symbol;
    double ltp;
    double change_percent;
    int64_t volume;
    int64_t oi;
    
    // Greeks (Optional - populated for options)
    struct Greeks {
        double iv = 0.0;
        double delta = 0.0;
        double gamma = 0.0;
        double theta = 0.0;
        double vega = 0.0;
        double rho = 0.0;
        bool isValid = false;  // Flag if Greeks are populated
    };
    Greeks greeks;
    
    // ... other fields ...
};
```

---

## 4. Unified Access Architecture (Generic Underlying Price)

### 4.1 The Problem

Currently you have:
- **NSECM Repository** → Cash market stocks and indices (NIFTY spot, stock prices)
- **NSEFO Repository** → Futures and options (NIFTY futures, option contracts)

**Challenge:**
For ATM watch of NIFTY options, you need NIFTY spot price from NSECM, but options data from NSEFO. How to get underlying price generically?

### 4.2 Solution: Unified Underlying Price Service

**Architecture:**

```cpp
// Unified service to get underlying price regardless of source
class UnderlyingPriceService : public QObject {
    Q_OBJECT
    
public:
    static UnderlyingPriceService* instance();
    
    // Generic method - auto-detects source
    double getUnderlyingPrice(const QString& symbol);
    
    // Subscribe to underlying (auto-routes to correct source)
    void subscribe(const QString& symbol, QObject* subscriber);
    void unsubscribe(const QString& symbol, QObject* subscriber);
    
    // Check if symbol is available
    bool hasUnderlying(const QString& symbol);
    
signals:
    void underlyingPriceUpdated(const QString& symbol, double ltp);
    
private:
    UnderlyingPriceService(QObject* parent = nullptr);
    
    // Determine source for symbol
    enum class Source {
        NSECM,    // Cash market
        NSEFO     // Futures (for indices without cash)
    };
    
    Source determineSource(const QString& symbol);
    
    // Route subscription to appropriate repository
    void subscribeToNSECM(const QString& symbol);
    void subscribeToNSEFO(const QString& symbol);
    
    // Cache for quick lookup
    QMap<QString, Source> m_symbolSourceCache;
    
    // Track subscriptions
    QMap<QString, QSet<QObject*>> m_subscribers;
};
```

### 4.3 Implementation Logic

```cpp
UnderlyingPriceService::Source 
UnderlyingPriceService::determineSource(const QString& symbol) {
    
    // Check cache first
    if (m_symbolSourceCache.contains(symbol)) {
        return m_symbolSourceCache[symbol];
    }
    
    Source source;
    
    // Strategy 1: Prefer cash market if available
    if (NSECMRepository::instance()->hasSymbol(symbol)) {
        source = Source::NSECM;
    }
    // Strategy 2: Fallback to futures
    else if (NSEFORepository::instance()->hasFuture(symbol)) {
        source = Source::NSEFO;
    }
    else {
        qWarning() << "Underlying not found in any source:" << symbol;
        source = Source::NSECM;  // Default
    }
    
    m_symbolSourceCache[symbol] = source;
    return source;
}

double UnderlyingPriceService::getUnderlyingPrice(const QString& symbol) {
    Source source = determineSource(symbol);
    
    switch (source) {
        case Source::NSECM:
            return NSECMPriceCache::instance()->getLastPrice(symbol);
            
        case Source::NSEFO:
            // Get current month future price as proxy
            QString futureSymbol = buildFutureSymbol(symbol);
            return NSEFOPriceCache::instance()->getLastPrice(futureSymbol);
    }
    
    return 0.0;
}

void UnderlyingPriceService::subscribe(const QString& symbol, 
                                       QObject* subscriber) {
    Source source = determineSource(symbol);
    
    // Track subscription
    m_subscribers[symbol].insert(subscriber);
    
    // Route to appropriate repository
    switch (source) {
        case Source::NSECM:
            subscribeToNSECM(symbol);
            break;
            
        case Source::NSEFO:
            subscribeToNSEFO(symbol);
            break;
    }
}

void UnderlyingPriceService::subscribeToNSECM(const QString& symbol) {
    // Subscribe to NSECM price cache
    NSECMPriceCache::instance()->subscribe(symbol, this);
    
    // Connect signal if not already connected
    connect(NSECMPriceCache::instance(), 
            &NSECMPriceCache::priceUpdated,
            this, 
            [this](const QString& sym, double price) {
                // Forward to our subscribers
                emit underlyingPriceUpdated(sym, price);
            });
}

void UnderlyingPriceService::subscribeToNSEFO(const QString& symbol) {
    // Build future symbol for current month
    QString futureSymbol = buildCurrentMonthFuture(symbol);
    
    // Subscribe to NSEFO price cache
    NSEFOPriceCache::instance()->subscribe(futureSymbol, this);
    
    // Connect signal
    connect(NSEFOPriceCache::instance(),
            &NSEFOPriceCache::priceUpdated,
            this,
            [this, symbol, futureSymbol](const QString& sym, double price) {
                if (sym == futureSymbol) {
                    // Emit as underlying symbol
                    emit underlyingPriceUpdated(symbol, price);
                }
            });
}
```

### 4.4 Usage in ATM Watch

```cpp
// Before (complicated)
void ATMWatchInstance::subscribeToUnderlying() {
    // Need to know if NIFTY is in CM or FO
    if (m_config.underlyingSymbol == "NIFTY") {
        // Subscribe to NSECM
        NSECMPriceCache::instance()->subscribe("NIFTY", this);
    } else if (isIndex(m_config.underlyingSymbol)) {
        // Subscribe to NSEFO future
        QString futSym = buildFutureSymbol(m_config.underlyingSymbol);
        NSEFOPriceCache::instance()->subscribe(futSym, this);
    } else {
        // Stock - subscribe to NSECM
        NSECMPriceCache::instance()->subscribe(m_config.underlyingSymbol, this);
    }
}

// After (simple and generic)
void ATMWatchInstance::subscribeToUnderlying() {
    // Just use the unified service
    UnderlyingPriceService::instance()->subscribe(
        m_config.underlyingSymbol, this);
    
    connect(UnderlyingPriceService::instance(),
            &UnderlyingPriceService::underlyingPriceUpdated,
            this,
            &ATMWatchInstance::onUnderlyingPriceUpdate);
}
```

### 4.5 Configuration-Based Routing

**Alternative: Let user configure source**

```ini
[UnderlyingMapping]
NIFTY=NSECM           # Use NIFTY spot from cash market
BANKNIFTY=NSECM       # Use BANKNIFTY spot
RELIANCE=NSECM        # Use stock price from cash
SENSEX=NSEFO          # Use futures (not in cash)
```

```cpp
Source UnderlyingPriceService::determineSource(const QString& symbol) {
    // Check config file first
    QSettings settings("config.ini", QSettings::IniFormat);
    QString configSource = settings.value(
        QString("UnderlyingMapping/%1").arg(symbol)).toString();
    
    if (configSource == "NSECM") return Source::NSECM;
    if (configSource == "NSEFO") return Source::NSEFO;
    
    // Fallback to auto-detection
    return autoDetectSource(symbol);
}
```

---

## 5. Implementation Roadmap

### Week 1: Foundation
- [ ] Implement `UnderlyingPriceService` (unified access)
- [ ] Test with NIFTY, BANKNIFTY, stocks
- [ ] Implement `ATMCalculator` (pure calculation)
- [ ] Unit tests

### Week 2: ATM Watch Core
- [ ] Implement `ATMWatchInstance`
- [ ] Subscription management
- [ ] Integration with `UnderlyingPriceService`
- [ ] Basic UI (Price, Volume, OI only)

### Week 3: ATM Watch Polish
- [ ] Implement `ATMWatchManager`
- [ ] ATM configuration dialog
- [ ] UI enhancements
- [ ] Testing with real market data
- [ ] **RELEASE v1.0** ✅

### Week 4: Greeks Extraction
- [ ] Verify Greeks availability in NSEFO UDP
- [ ] Parse Greeks from UDP packets
- [ ] Enhance `PriceData` structure
- [ ] Store Greeks in PriceCache
- [ ] Unit tests for Greeks parsing

### Week 5: Greeks Integration
- [ ] Add Greeks columns to ATM watch UI
- [ ] Greeks-based sorting
- [ ] Greeks highlighting (e.g., high delta)
- [ ] Optional: Greeks calculator for fallback
- [ ] **RELEASE v2.0** ✅

---

## 6. Data Model Evolution

### Phase 1: ATM Watch without Greeks

```cpp
struct ATMRowData {
    QString expiry;
    int strike;
    
    // Call side
    double callLTP;
    double callChange;
    int64_t callVolume;
    int64_t callOI;
    
    // Put side  
    double putLTP;
    double putChange;
    int64_t putVolume;
    int64_t putOI;
};
```

### Phase 2: ATM Watch with Greeks

```cpp
struct ATMRowData {
    QString expiry;
    int strike;
    
    // Call side
    double callLTP;
    double callChange;
    int64_t callVolume;
    int64_t callOI;
    double callDelta;    // NEW
    double callGamma;    // NEW
    double callTheta;    // NEW
    double callVega;     // NEW
    double callIV;       // NEW
    
    // Put side
    double putLTP;
    double putChange;
    int64_t putVolume;
    int64_t putOI;
    double putDelta;     // NEW
    double putGamma;     // NEW
    double putTheta;     // NEW
    double putVega;      // NEW
    double putIV;        // NEW
};
```

**Migration is simple - just add columns!**

---

## 7. Quick Win: Verify Greeks in NSEFO Feed

**Before starting implementation, confirm Greeks availability:**

```cpp
// Add temporary debug code to NSEFO UDP reader
void NSEFOUDPReader::onPacketReceived(const QByteArray& packet) {
    // Parse packet
    auto data = parsePacket(packet);
    
    // Log Greeks for verification
    if (data.symbol.contains("NIFTY") && data.symbol.contains("CE")) {
        qDebug() << "Symbol:" << data.symbol;
        qDebug() << "  LTP:" << data.ltp;
        qDebug() << "  IV:" << data.implied_volatility;
        qDebug() << "  Delta:" << data.delta;
        qDebug() << "  Gamma:" << data.gamma;
        qDebug() << "  Theta:" << data.theta;
        qDebug() << "  Vega:" << data.vega;
    }
    
    // ... rest of processing ...
}
```

**Run during market hours and verify:**
- Are Greeks fields populated?
- Are values reasonable? (Delta 0-1, IV in %, etc.)
- Are all Greeks available or only some?

---

## 8. Risk Mitigation

### If Greeks NOT in UDP Feed:

**Fallback Plan:**
1. Implement Black-Scholes calculator
2. Calculate Greeks from:
   - Underlying price (from UnderlyingPriceService)
   - Strike price (from symbol)
   - Expiry (from symbol)
   - LTP (implied IV calculation)
   - Risk-free rate (configurable)

3. Store calculated Greeks in PriceCache

**Note:** Calculated Greeks will be less accurate than exchange-provided Greeks, but still useful.

---

## 9. Decision Matrix

| Factor | ATM First | Greeks First | Both Together |
|--------|-----------|--------------|---------------|
| Time to value | ✅ 2-3 weeks | ❌ 4-5 weeks | ❌ 4-5 weeks |
| Complexity | ✅ Low | ⚠️ Medium | ❌ High |
| User feedback | ✅ Early | ❌ Late | ❌ Late |
| Testing ease | ✅ Easy | ⚠️ Medium | ❌ Hard |
| Risk | ✅ Low | ⚠️ Medium | ❌ High |
| Completeness | ⚠️ Partial | ⚠️ Partial | ✅ Complete |

**Winner: ATM First** ✅

---

## 10. Recommendations Summary

### ✅ DO:
1. **Implement ATM watch first** (Weeks 1-3)
2. **Build UnderlyingPriceService** for generic access
3. **Verify Greeks in NSEFO feed** before Week 4
4. **Add Greeks as enhancement** (Weeks 4-5)
5. **Release incrementally** (v1.0 without Greeks, v2.0 with Greeks)

### ❌ DON'T:
1. Wait for Greeks before starting ATM watch
2. Implement both simultaneously
3. Hard-code CM vs FO logic in ATM watch
4. Calculate Greeks if feed provides them

### 🎯 Success Criteria:

**Week 3:** 
- Traders can use ATM watch with price/volume/OI
- Subscription management working smoothly
- Positive user feedback

**Week 5:**
- Greeks displayed alongside prices
- Enhanced decision-making capability
- Complete ATM watch feature

---

## Appendix: Sample UI Mock

### Phase 1 UI (Without Greeks)
```
NIFTY ATM Watch - 27FEB25
Underlying: 22,450.75 ▲ +0.45%

Strike | Call LTP | Call Chg% | Call Vol | Call OI | Put LTP | Put Chg% | Put Vol | Put OI
-------|----------|-----------|----------|---------|---------|----------|---------|--------
22300  | 245.50   | +2.3%     | 25.4K    | 45.2K   | 15.25   | -5.2%    | 18.9K   | 38.1K
22350  | 198.75   | +3.1%     | 32.1K    | 52.8K   | 22.40   | -3.8%    | 24.5K   | 42.6K
22400  | 155.25   | +4.2%     | 45.6K    | 68.3K   | 35.80   | -2.1%    | 38.2K   | 55.9K
22450  | 115.50   | +5.8%     | 62.8K    | 85.7K   | 58.25   | +1.2%    | 59.3K   | 71.4K ← ATM
22500  | 82.40    | +8.2%     | 58.3K    | 78.2K   | 95.75   | +3.4%    | 52.1K   | 65.8K
22550  | 56.75    | +12.5%    | 42.5K    | 62.5K   | 148.50  | +5.7%    | 38.9K   | 58.2K
22600  | 38.25    | +18.2%    | 28.7K    | 48.3K   | 218.75  | +7.2%    | 25.6K   | 45.7K
```

### Phase 2 UI (With Greeks)
```
NIFTY ATM Watch - 27FEB25
Underlying: 22,450.75 ▲ +0.45%

Strike | Call LTP | Δ    | Γ     | θ     | IV  | Put LTP | Δ    | Γ     | θ     | IV
-------|----------|------|-------|-------|-----|---------|------|-------|-------|-----
22300  | 245.50   | 0.82 | 0.005 | -12.5 | 18% | 15.25   | -0.18| 0.005 | -8.2  | 17%
22350  | 198.75   | 0.75 | 0.008 | -15.8 | 19% | 22.40   | -0.25| 0.008 | -10.5 | 18%
22400  | 155.25   | 0.68 | 0.012 | -18.2 | 20% | 35.80   | -0.32| 0.012 | -12.8 | 19%
22450  | 115.50   | 0.58 | 0.015 | -22.5 | 21% | 58.25   | -0.42| 0.015 | -16.2 | 20% ← ATM
22500  | 82.40    | 0.45 | 0.014 | -20.8 | 21% | 95.75   | -0.55| 0.014 | -18.5 | 21%
22550  | 56.75    | 0.32 | 0.011 | -17.5 | 20% | 148.50  | -0.68| 0.011 | -20.2 | 20%
22600  | 38.25    | 0.22 | 0.008 | -14.2 | 19% | 218.75  | -0.78| 0.008 | -22.5 | 19%
```

---

**Final Recommendation: Build ATM Watch v1.0 (Weeks 1-3), then enhance with Greeks v2.0 (Weeks 4-5). This provides fastest time to value while maintaining clean architecture for future enhancements.**

