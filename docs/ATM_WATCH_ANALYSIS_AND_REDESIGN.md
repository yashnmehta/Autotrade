# ATM Watch - In-Depth Analysis and Redesign Plan

**Date:** January 21, 2026  
**Status:** Critical Issues Identified - Redesign Required

---

## Executive Summary

The current ATM Watch implementation has several critical issues and optimization problems:

1. **Missing Repository Functions** - Functions called by ATMWatchManager don't exist in RepositoryManager
2. **Fallback Implementation Problems** - Using hardcoded fallbacks when base price is unavailable
3. **Inefficient Data Loading** - Loading all data at once instead of progressive loading
4. **No Proper Lifecycle Management** - Missing proper initialization and update sequencing

---

## Current Implementation Analysis

### 1. Repository Manager Issues

#### 1.1 Missing Functions

The `ATMWatchManager.cpp` calls these functions that **DO NOT EXIST** in `RepositoryManager.cpp`:

```cpp
// Called in ATMWatchManager::calculateAll() - Line 110
int64_t assetToken = repo->getAssetTokenForSymbol(config.symbol);

// Called in ATMWatchManager::calculateAll() - Line 121
int64_t futureToken = repo->getFutureTokenForSymbolExpiry(config.symbol, config.expiry);

// Called in ATMWatchManager::calculateAll() - Line 133
QVector<double> strikeList = repo->getStrikesForSymbolExpiry(config.symbol, config.expiry);

// Called in ATMWatchManager::calculateAll() - Line 152
auto tokens = repo->getTokensForStrike(config.symbol, config.expiry, result.atmStrike);
```

**Status:** ✗ Functions declared in header but NOT implemented in RepositoryManager.cpp

#### 1.2 Existing Repository Functions

Looking at `RepositoryManager.cpp` (from your active file), these are the actual functions:

```cpp
// Expiry cache functions (lines 1100-1178)
- buildExpiryCache()           // ✓ Implemented
- getOptionSymbols()           // ✓ Implemented  
- getSymbolsForExpiry()        // ✓ Implemented
- getCurrentExpiry()           // ✓ Implemented
- getAllExpiries()             // ✓ Implemented
- getOptionSymbolsFromArray()  // ✓ Implemented (line 1171)

// Missing ATM-specific functions
- getAssetTokenForSymbol()              // ✗ NOT IMPLEMENTED
- getFutureTokenForSymbolExpiry()       // ✗ NOT IMPLEMENTED
- getStrikesForSymbolExpiry()           // ✗ NOT IMPLEMENTED
- getTokensForStrike()                  // ✗ NOT IMPLEMENTED
```

---

### 2. ATMWatchManager Issues

#### 2.1 Fallback Logic Problems (Lines 200-220)

```cpp
double ATMWatchManager::fetchBasePrice(const ATMConfig &config) {
  if (config.source == BasePriceSource::Cash) {
    // ... tries to get asset token ...
    
    // PROBLEM: Falls back to hardcoded tokens
    if (config.symbol == "NIFTY") {
      return nsecm::getGenericLtp(26000);     // ✗ Hardcoded
    } else if (config.symbol == "BANKNIFTY") {
      return nsecm::getGenericLtp(26009);     // ✗ Hardcoded
    }
    // ... more hardcoded fallbacks ...
  }
}
```

**Issues:**
- Hardcoded token values (26000, 26009, etc.)
- No wait mechanism if base price is null/zero
- Returns 0.0 instead of waiting for next trigger

#### 2.2 Calculation Flow Problems

Current flow in `calculateAll()` (Lines 96-178):

```
For each symbol:
  1. Get asset token        // ✗ Function doesn't exist
  2. Fetch base price       // ✗ Uses fallbacks
  3. Get strikes            // ✗ Function doesn't exist
  4. Calculate ATM
  5. Get CE/PE tokens       // ✗ Function doesn't exist
  6. Store result
```

**User Requirement:** No fallback - wait for next trigger if base price unavailable

---

### 3. ATMWatchWindow Issues

#### 3.1 Data Loading (Lines 319-387)

```cpp
void ATMWatchWindow::loadAllSymbols() {
  // Step 1: Get option symbols
  QVector<QString> optionSymbols = repo->getOptionSymbols();
  
  // Step 4: Subscribe to base tokens (COMMENT ONLY - not implemented)
  // "when ATMWatchManager calls fetchBasePrice() during calculation"
  
  // Step 5: Prepare watch configs
  for (const QString &symbol : optionSymbols) {
    // Get expiry and add to watchConfigs
  }
  
  // Step 6: Add all watches in batch
  ATMWatchManager::getInstance()->addWatchesBatch(watchConfigs);
}
```

**Issues:**
1. No base price subscription before ATM calculation
2. Populates all data at once (symbol, expiry, base price, ATM strike, options)
3. No progressive loading as per user requirements

#### 3.2 Refresh Data (Lines 174-237)

```cpp
void ATMWatchWindow::refreshData() {
  // Clears everything
  FeedHandler::instance().unsubscribeAll(this);
  m_tokenToInfo.clear();
  
  // Repopulates all data from ATMWatchManager
  auto atmList = ATMWatchManager::getInstance()->getATMWatchArray();
  
  for (const auto &info : atmList) {
    // Populates ALL columns immediately:
    // - Symbol, Base Price, ATM Strike, Expiry
    // - CE LTP, Bid, Ask, Volume, OI
    // - PE LTP, Bid, Ask, Volume, OI
  }
}
```

**User Requirement:** Progressive loading
1. First: Symbol + Expiry only
2. Then: Subscribe base token → Base price updates live
3. Then: Calculate ATM → Populate ATM strike
4. Then: Subscribe CE/PE → Options update live

---

## Bugs Summary

### Critical Bugs

| # | Bug | Location | Impact | Status |
|---|-----|----------|--------|--------|
| 1 | Missing repository functions | RepositoryManager.cpp | **CRITICAL** - ATM calculation fails | ✗ Not implemented |
| 2 | Hardcoded token fallbacks | ATMWatchManager.cpp:206-220 | **HIGH** - Wrong tokens used | ✗ Present |
| 3 | No wait mechanism for null base price | ATMWatchManager.cpp:96-178 | **HIGH** - Skips calculations | ✗ Present |
| 4 | All-at-once data population | ATMWatchWindow.cpp:174-237 | **MEDIUM** - Poor UX | ✗ Present |
| 5 | No progressive loading | ATMWatchWindow.cpp:319-387 | **MEDIUM** - Poor UX | ✗ Present |

### Design Issues

| # | Issue | Description | User Requirement |
|---|-------|-------------|------------------|
| 1 | Fallback logic | Uses hardcoded values when base price unavailable | **Wait for next trigger** |
| 2 | Immediate population | Populates all data immediately | **Progressive: Symbol → Base → ATM → Options** |
| 3 | No base subscription | Base price not subscribed before calculation | **Subscribe base first** |
| 4 | Single refresh | Refreshes all data at once | **Incremental updates** |

---

## Proposed Redesign

### Phase 1: Fix Repository Functions

**Objective:** Implement missing cache functions for O(1) ATM calculations

#### 1.1 Build Strike Cache

```cpp
// In RepositoryManager.h
private:
  // ATM Watch optimization caches
  QHash<QString, QVector<double>> m_symbolExpiryStrikes;  // "NIFTY|27JAN2026" -> [18000, 18100, ...]
  QHash<QString, int64_t> m_symbolAssetToken;             // "NIFTY" -> 26000
  QHash<QString, int64_t> m_symbolExpiryFutureToken;      // "NIFTY|27JAN2026" -> 43210
  QHash<QString, QPair<int64_t, int64_t>> m_strikeTokens; // "NIFTY|27JAN2026|18000" -> (CE_token, PE_token)
```

#### 1.2 Populate Cache in buildExpiryCache()

```cpp
void RepositoryManager::buildExpiryCache() {
  // Existing code...
  
  // NEW: Build strike and token caches
  m_symbolExpiryStrikes.clear();
  m_symbolAssetToken.clear();
  m_symbolExpiryFutureToken.clear();
  m_strikeTokens.clear();
  
  if (m_nsefo) {
    QVector<ContractData> nsefoContracts = m_nsefo->getAllContracts();
    
    for (const auto& contract : nsefoContracts) {
      if (contract.series == "OPTSTK" || contract.series == "OPTIDX") {
        QString key = contract.name + "|" + contract.expiryDate;
        
        // Add strike to sorted list
        if (!m_symbolExpiryStrikes[key].contains(contract.strikePrice)) {
          m_symbolExpiryStrikes[key].append(contract.strikePrice);
        }
        
        // Map strike to CE/PE tokens
        QString strikeKey = key + "|" + QString::number(contract.strikePrice);
        if (contract.optionType == "CE") {
          m_strikeTokens[strikeKey].first = contract.exchangeInstrumentID;
        } else if (contract.optionType == "PE") {
          m_strikeTokens[strikeKey].second = contract.exchangeInstrumentID;
        }
        
        // Store asset token (underlying cash token)
        if (contract.assetToken > 0) {
          m_symbolAssetToken[contract.name] = contract.assetToken;
        }
      }
      
      // Store future tokens
      if (contract.series == "FUTIDX" || contract.series == "FUTSTK") {
        QString futKey = contract.name + "|" + contract.expiryDate;
        m_symbolExpiryFutureToken[futKey] = contract.exchangeInstrumentID;
      }
    }
    
    // Sort strikes ascending for each symbol+expiry
    for (auto& strikes : m_symbolExpiryStrikes) {
      std::sort(strikes.begin(), strikes.end());
    }
  }
  
  qDebug() << "[RepositoryManager] Strike cache built:";
  qDebug() << "  - Symbol+Expiry combinations:" << m_symbolExpiryStrikes.size();
  qDebug() << "  - Asset tokens:" << m_symbolAssetToken.size();
  qDebug() << "  - Future tokens:" << m_symbolExpiryFutureToken.size();
  qDebug() << "  - Strike→Token mappings:" << m_strikeTokens.size();
}
```

#### 1.3 Implement Cache Lookup Functions

```cpp
QVector<double> RepositoryManager::getStrikesForSymbolExpiry(
    const QString &symbol, const QString &expiry) const {
  QString key = symbol + "|" + expiry;
  return m_symbolExpiryStrikes.value(key);
}

int64_t RepositoryManager::getAssetTokenForSymbol(const QString &symbol) const {
  return m_symbolAssetToken.value(symbol, 0);
}

int64_t RepositoryManager::getFutureTokenForSymbolExpiry(
    const QString &symbol, const QString &expiry) const {
  QString key = symbol + "|" + expiry;
  return m_symbolExpiryFutureToken.value(key, 0);
}

QPair<int64_t, int64_t> RepositoryManager::getTokensForStrike(
    const QString &symbol, const QString &expiry, double strike) const {
  QString key = symbol + "|" + expiry + "|" + QString::number(strike);
  return m_strikeTokens.value(key, qMakePair(0LL, 0LL));
}
```

---

### Phase 2: Fix ATMWatchManager Calculation

**Objective:** Remove fallbacks, wait for valid base price

#### 2.1 Remove Hardcoded Fallbacks

```cpp
double ATMWatchManager::fetchBasePrice(const ATMConfig &config) {
  auto repo = RepositoryManager::getInstance();
  
  if (config.source == BasePriceSource::Cash) {
    // O(1) lookup from cache
    int64_t assetToken = repo->getAssetTokenForSymbol(config.symbol);
    
    if (assetToken > 0) {
      double ltp = nsecm::getGenericLtp(static_cast<uint32_t>(assetToken));
      if (ltp > 0) {
        return ltp;
      }
    }
    
    // NO FALLBACK - return 0 to signal unavailable
    return 0.0;
    
  } else { // BasePriceSource::Future
    int64_t futureToken = repo->getFutureTokenForSymbolExpiry(
        config.symbol, config.expiry);
    
    if (futureToken > 0) {
      auto *state = nsefo::g_nseFoPriceStore.getUnifiedState(
          static_cast<uint32_t>(futureToken));
      if (state && state->ltp > 0) {
        return state->ltp;
      }
    }
    
    // NO FALLBACK - return 0 to signal unavailable
    return 0.0;
  }
}
```

#### 2.2 Skip Calculation if Base Price Unavailable

```cpp
void ATMWatchManager::calculateAll() {
  std::unique_lock lock(m_mutex);
  
  auto repo = RepositoryManager::getInstance();
  if (!repo->isLoaded()) return;
  
  int successCount = 0;
  int skipCount = 0;  // NEW: Track skipped (waiting for base price)
  int failCount = 0;
  
  for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
    const auto &config = it.value();
    
    // Step 1: Fetch base price
    double basePrice = fetchBasePrice(config);
    
    // Step 2: SKIP if base price not available (NO FALLBACK)
    if (basePrice <= 0) {
      qDebug() << "[ATMWatch] Skipping" << config.symbol 
               << "- waiting for base price (will retry in 1 minute)";
      skipCount++;
      continue;  // Wait for next trigger
    }
    
    // Step 3: Get strikes from cache (O(1) lookup)
    QVector<double> strikeList = repo->getStrikesForSymbolExpiry(
        config.symbol, config.expiry);
    
    if (strikeList.isEmpty()) {
      qDebug() << "[ATMWatch] No strikes found for" 
               << config.symbol << config.expiry;
      failCount++;
      continue;
    }
    
    // Step 4: Calculate ATM strike
    auto result = ATMCalculator::calculateFromActualStrikes(
        basePrice, strikeList, config.rangeCount);
    
    if (!result.isValid) {
      failCount++;
      continue;
    }
    
    // Step 5: Get CE/PE tokens from cache (O(1) lookup)
    auto tokens = repo->getTokensForStrike(
        config.symbol, config.expiry, result.atmStrike);
    
    // Step 6: Store result
    ATMInfo info;
    info.symbol = config.symbol;
    info.expiry = config.expiry;
    info.basePrice = basePrice;
    info.atmStrike = result.atmStrike;
    info.callToken = tokens.first;
    info.putToken = tokens.second;
    info.lastUpdated = QDateTime::currentDateTime();
    info.isValid = true;
    
    m_results[config.symbol] = info;
    successCount++;
  }
  
  qDebug() << "[ATMWatch] Calculation complete:"
           << successCount << "succeeded,"
           << skipCount << "skipped (waiting for base price),"
           << failCount << "failed out of" << m_configs.size() << "symbols";
  
  emit atmUpdated();
}
```

---

### Phase 3: Redesign ATMWatchWindow Progressive Loading

**Objective:** Load data progressively: Symbol → Base Price → ATM → Options

#### 3.1 New Loading Phases

```cpp
enum class LoadingPhase {
  NotStarted,
  SymbolsLoaded,      // Phase 1: Symbol + Expiry populated
  BaseSubscribed,     // Phase 2: Base tokens subscribed
  ATMCalculated,      // Phase 3: ATM strikes populated
  OptionsSubscribed   // Phase 4: CE/PE subscribed, live updates
};
```

#### 3.2 Phase 1: Populate Symbol + Expiry Only

```cpp
void ATMWatchWindow::loadAllSymbols() {
  auto repo = RepositoryManager::getInstance();
  if (!repo->isLoaded()) {
    m_statusLabel->setText("Repository not loaded");
    return;
  }
  
  m_statusLabel->setText("Loading symbols...");
  m_loadingPhase = LoadingPhase::NotStarted;
  
  QtConcurrent::run([this, repo]() {
    // Get option symbols from cache
    QVector<QString> optionSymbols = repo->getOptionSymbols();
    
    // Prepare watch configs
    QVector<QPair<QString, QString>> watchConfigs;
    
    for (const QString &symbol : optionSymbols) {
      QString expiry;
      
      if (m_currentExpiry == "CURRENT") {
        expiry = repo->getCurrentExpiry(symbol);
      } else {
        QVector<QString> symbolsForExpiry = repo->getSymbolsForExpiry(m_currentExpiry);
        if (symbolsForExpiry.contains(symbol)) {
          expiry = m_currentExpiry;
        }
      }
      
      if (!expiry.isEmpty()) {
        watchConfigs.append(qMakePair(symbol, expiry));
      }
    }
    
    // Phase 1: Populate ONLY symbol and expiry columns
    QMetaObject::invokeMethod(this, [this, watchConfigs]() {
      populateSymbolsAndExpiries(watchConfigs);
      m_loadingPhase = LoadingPhase::SymbolsLoaded;
      m_statusLabel->setText(QString("Loaded %1 symbols").arg(watchConfigs.size()));
      
      // Proceed to Phase 2
      subscribeToBaseTokens(watchConfigs);
    }, Qt::QueuedConnection);
  });
}

void ATMWatchWindow::populateSymbolsAndExpiries(
    const QVector<QPair<QString, QString>> &configs) {
  
  m_callModel->setRowCount(0);
  m_symbolModel->setRowCount(0);
  m_putModel->setRowCount(0);
  
  int row = 0;
  for (const auto &config : configs) {
    m_callModel->insertRow(row);
    m_symbolModel->insertRow(row);
    m_putModel->insertRow(row);
    
    // Populate ONLY symbol and expiry
    m_symbolModel->setData(m_symbolModel->index(row, SYM_NAME), config.first);
    m_symbolModel->setData(m_symbolModel->index(row, SYM_EXPIRY), config.second);
    
    // Leave Base Price and ATM Strike empty (will be filled later)
    m_symbolModel->setData(m_symbolModel->index(row, SYM_PRICE), "-");
    m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM), "-");
    
    m_symbolToRow[config.first] = row;
    row++;
  }
}
```

#### 3.3 Phase 2: Subscribe to Base Tokens

```cpp
void ATMWatchWindow::subscribeToBaseTokens(
    const QVector<QPair<QString, QString>> &configs) {
  
  auto repo = RepositoryManager::getInstance();
  FeedHandler &feed = FeedHandler::instance();
  
  for (const auto &config : configs) {
    // Get base token (cash or future)
    int64_t baseToken = repo->getAssetTokenForSymbol(config.first);
    
    if (baseToken > 0) {
      // Subscribe to NSECM (cash)
      m_underlyingToRow[baseToken] = config.first;
      feed.subscribe(1, baseToken, this, &ATMWatchWindow::onBaseTickUpdate);
      
      // Immediately fetch current value if available
      double ltp = nsecm::getGenericLtp(static_cast<uint32_t>(baseToken));
      if (ltp > 0) {
        updateBasePriceInUI(config.first, ltp);
      }
    }
  }
  
  m_loadingPhase = LoadingPhase::BaseSubscribed;
  
  // Add watches to ATMWatchManager (this triggers calculation)
  ATMWatchManager::getInstance()->addWatchesBatch(configs);
  
  // ATMWatchManager will emit atmUpdated() when calculation completes
  // → Phase 3 will be triggered in onATMUpdated()
}

void ATMWatchWindow::onBaseTickUpdate(const UDP::MarketTick &tick) {
  if (!m_underlyingToRow.contains(tick.token)) return;
  
  QString symbol = m_underlyingToRow[tick.token];
  updateBasePriceInUI(symbol, tick.ltp);
}

void ATMWatchWindow::updateBasePriceInUI(const QString &symbol, double price) {
  int row = m_symbolToRow.value(symbol, -1);
  if (row < 0) return;
  
  // Update base price column LIVE
  m_symbolModel->setData(m_symbolModel->index(row, SYM_PRICE),
                         QString::number(price, 'f', 2));
}
```

#### 3.4 Phase 3: Populate ATM Strikes

```cpp
void ATMWatchWindow::onATMUpdated() {
  if (m_loadingPhase != LoadingPhase::BaseSubscribed) {
    return; // Only proceed after base subscription
  }
  
  auto atmList = ATMWatchManager::getInstance()->getATMWatchArray();
  
  for (const auto &info : atmList) {
    if (!info.isValid) continue;
    
    int row = m_symbolToRow.value(info.symbol, -1);
    if (row < 0) continue;
    
    // Populate ATM strike column
    m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                           QString::number(info.atmStrike, 'f', 2));
    
    // Store tokens for Phase 4
    m_symbolToATMInfo[info.symbol] = info;
  }
  
  m_loadingPhase = LoadingPhase::ATMCalculated;
  
  // Proceed to Phase 4
  subscribeToOptions();
}
```

#### 3.5 Phase 4: Subscribe to CE/PE Options

```cpp
void ATMWatchWindow::subscribeToOptions() {
  FeedHandler &feed = FeedHandler::instance();
  
  for (auto it = m_symbolToATMInfo.begin(); it != m_symbolToATMInfo.end(); ++it) {
    const auto &info = it.value();
    
    // Subscribe to Call option
    if (info.callToken > 0) {
      m_tokenToInfo[info.callToken] = {info.symbol, true}; // true = CE
      feed.subscribe(2, info.callToken, this, &ATMWatchWindow::onOptionTickUpdate);
      
      // Fetch current value
      if (auto state = MarketData::PriceStoreGateway::instance().getUnifiedState(2, info.callToken)) {
        UDP::MarketTick tick;
        tick.token = info.callToken;
        tick.ltp = state->ltp;
        tick.bids[0].price = state->bids[0].price;
        tick.asks[0].price = state->asks[0].price;
        tick.volume = state->volume;
        tick.openInterest = state->openInterest;
        onOptionTickUpdate(tick);
      }
    }
    
    // Subscribe to Put option
    if (info.putToken > 0) {
      m_tokenToInfo[info.putToken] = {info.symbol, false}; // false = PE
      feed.subscribe(2, info.putToken, this, &ATMWatchWindow::onOptionTickUpdate);
      
      // Fetch current value
      if (auto state = MarketData::PriceStoreGateway::instance().getUnifiedState(2, info.putToken)) {
        UDP::MarketTick tick;
        tick.token = info.putToken;
        tick.ltp = state->ltp;
        tick.bids[0].price = state->bids[0].price;
        tick.asks[0].price = state->asks[0].price;
        tick.volume = state->volume;
        tick.openInterest = state->openInterest;
        onOptionTickUpdate(tick);
      }
    }
  }
  
  m_loadingPhase = LoadingPhase::OptionsSubscribed;
  m_statusLabel->setText("Live updates active");
}

void ATMWatchWindow::onOptionTickUpdate(const UDP::MarketTick &tick) {
  if (!m_tokenToInfo.contains(tick.token)) return;
  
  auto info = m_tokenToInfo[tick.token];
  int row = m_symbolToRow.value(info.first, -1);
  if (row < 0) return;
  
  if (info.second) { // Call
    m_callModel->setData(m_callModel->index(row, CALL_LTP),
                         QString::number(tick.ltp, 'f', 2));
    m_callModel->setData(m_callModel->index(row, CALL_BID),
                         QString::number(tick.bestBid(), 'f', 2));
    m_callModel->setData(m_callModel->index(row, CALL_ASK),
                         QString::number(tick.bestAsk(), 'f', 2));
    m_callModel->setData(m_callModel->index(row, CALL_VOL),
                         QString::number(tick.volume));
    m_callModel->setData(m_callModel->index(row, CALL_OI),
                         QString::number(tick.openInterest));
  } else { // Put
    m_putModel->setData(m_putModel->index(row, PUT_LTP),
                        QString::number(tick.ltp, 'f', 2));
    m_putModel->setData(m_putModel->index(row, PUT_BID),
                        QString::number(tick.bestBid(), 'f', 2));
    m_putModel->setData(m_putModel->index(row, PUT_ASK),
                        QString::number(tick.bestAsk(), 'f', 2));
    m_putModel->setData(m_putModel->index(row, PUT_VOL),
                        QString::number(tick.volume));
    m_putModel->setData(m_putModel->index(row, PUT_OI),
                        QString::number(tick.openInterest));
  }
}
```

---

## Implementation Checklist

### Phase 1: Repository Functions ✓
- [ ] Add cache hash maps to RepositoryManager.h (m_symbolExpiryStrikes, etc.)
- [ ] Implement cache building in buildExpiryCache()
- [ ] Implement getStrikesForSymbolExpiry()
- [ ] Implement getAssetTokenForSymbol()
- [ ] Implement getFutureTokenForSymbolExpiry()
- [ ] Implement getTokensForStrike()
- [ ] Test cache population on startup

### Phase 2: ATMWatchManager Fixes ✓
- [ ] Remove hardcoded fallback tokens
- [ ] Return 0 when base price unavailable
- [ ] Skip calculation if basePrice <= 0
- [ ] Add skip counter to logging
- [ ] Test periodic recalculation (1-minute timer)

### Phase 3: Progressive Loading ✓
- [ ] Add LoadingPhase enum to ATMWatchWindow.h
- [ ] Implement populateSymbolsAndExpiries() (Phase 1)
- [ ] Implement subscribeToBaseTokens() (Phase 2)
- [ ] Implement onBaseTickUpdate() callback
- [ ] Update onATMUpdated() to populate ATM strikes (Phase 3)
- [ ] Implement subscribeToOptions() (Phase 4)
- [ ] Implement onOptionTickUpdate() callback
- [ ] Test complete flow: Symbol → Base → ATM → Options

### Phase 4: Testing & Validation ✓
- [ ] Test with repository not loaded
- [ ] Test with missing base price (should skip, not crash)
- [ ] Test progressive UI updates
- [ ] Test live price updates for base/CE/PE
- [ ] Test exchange/expiry filter changes
- [ ] Test 1-minute periodic recalculation
- [ ] Verify no hardcoded tokens used
- [ ] Verify O(1) cache lookups (no iteration)

---

## Expected Behavior After Redesign

### Startup Sequence
```
1. User opens ATM Watch window
   → Shows "Loading symbols..."

2. Phase 1: Symbol + Expiry
   → Populates Symbol and Expiry columns
   → Base Price shows "-"
   → ATM Strike shows "-"
   → CE/PE columns empty

3. Phase 2: Base Subscription
   → Subscribes to underlying cash tokens
   → Base Price column updates LIVE as prices arrive
   → Status: "Subscribed to base prices"

4. ATMWatchManager Calculation (triggered by addWatchesBatch)
   → If base price available: Calculates ATM
   → If base price unavailable: Skips, waits for next 1-minute trigger

5. Phase 3: ATM Population
   → onATMUpdated() fires
   → ATM Strike column fills in for symbols with valid base price
   → Symbols without base price still show "-"

6. Phase 4: Option Subscription
   → Subscribes to CE/PE tokens
   → CE/PE columns update LIVE as prices arrive
   → Status: "Live updates active"

7. Periodic Updates (every 1 minute)
   → ATMWatchManager recalculates
   → Previously skipped symbols retry (if base price now available)
   → ATM strikes update if base price changed significantly
```

### Error Handling
```
- Repository not loaded: Shows error message, no crash
- Base price unavailable: Skips calculation, waits for next trigger
- Strike list empty: Logs error, skips symbol
- CE/PE token not found: Shows "-" in option columns
- Network disconnection: Stops live updates, retries on reconnect
```

---

## Performance Expectations

### Cache Lookup Times
- `getAssetTokenForSymbol()`: O(1) - QHash lookup
- `getStrikesForSymbolExpiry()`: O(1) - QHash lookup
- `getTokensForStrike()`: O(1) - QHash lookup
- `getFutureTokenForSymbolExpiry()`: O(1) - QHash lookup

### Memory Overhead
- Strike cache: ~500 symbols × 10 expiries × 50 strikes = 250K entries
- Asset token cache: ~500 symbols = 500 entries
- Future token cache: ~500 symbols × 10 expiries = 5K entries
- Strike→Token cache: ~500 symbols × 10 expiries × 50 strikes × 2 (CE/PE) = 500K entries

**Total Estimated:** ~750K entries × 24 bytes = ~18 MB

### UI Update Performance
- Progressive loading: 4 phases, each completes in <100ms
- Live updates: Per-symbol, no full refresh
- No blocking: All calculations in background threads

---

## Conclusion

The redesigned ATM Watch will:
1. ✓ Use O(1) cache lookups (no iteration)
2. ✓ Wait for base price (no fallbacks)
3. ✓ Load progressively (Symbol → Base → ATM → Options)
4. ✓ Update live (per-symbol, per-column)
5. ✓ Retry on timer (1-minute recalculation)
6. ✓ Handle errors gracefully (no crashes)

**Next Steps:**
1. Implement Phase 1 (Repository cache functions)
2. Test cache population
3. Implement Phase 2 (ATMWatchManager fixes)
4. Implement Phase 3 (Progressive loading)
5. Integration testing
6. Deploy

