# Week 1-2 POC: Backend Execution Feasibility
**Hands-On Task List with Code Locations**

**Date:** February 18, 2026  
**Status:** 🚀 READY TO START  
**Goal:** Prove options execution works or fail fast

---

## 📋 Executive Summary

### 🎯 What We're Building

**Single Test:** Deploy ATM Straddle template → Execute 2 legs (CE + PE)

**Success Criteria:**
```cpp
TEST POC: Deploy ATM Straddle on NIFTY
1. ✅ Parse JSON: {"legs": [{"type":"CE","strike":"ATM"}, ...]}
2. ✅ Resolve Strike: 24567.50 spot → "24550" ATM strike
3. ✅ Execute CE: Place SELL order for NIFTY24550CE
4. ✅ Execute PE: Place SELL order for NIFTY24550PE
5. ✅ Verify: Both orders reach broker API

If ALL pass → GO to Phase 1
If ANY fail → NO-GO, redesign backend (+4 weeks)
```

### 📊 Current State Analysis

**✅ What Exists:**
- `CustomStrategy.cpp` - Main strategy executor
- `StrategyParser.cpp` - JSON → StrategyDefinition converter
- `StrategyDefinition.h` - Has `OptionLeg` struct (incomplete)
- `OrderExecutionEngine` - Existing order placement infrastructure
- `ContractData` / `MasterFileParser` - Can lookup options symbols

**❌ What's Missing:**
- Options leg parsing (StrategyParser only handles indicators)
- ATM strike resolution logic
- Multi-leg execution coordinator
- `symbol` field in `OptionLeg` struct (per STRATEGY_BUILDER_REVIEW.md)

### ⏱️ Time Budget

**Total: 80 hours → 72 hours (2 developers × 2 weeks)**  
**🎯 REVISED after Day 1 Audit: Saved 8 hours by reusing ATMCalculator**

| Task | Hours | Revised | Developer | Day |
|------|-------|---------|-----------|-----|
| **Day 1-2: Audit** | 16h | ✅ 16h | Both | Mon-Tue |
| **Day 3-5: Options Parser** | 24h | 24h | Dev 1 | Wed-Fri |
| **Day 3-5: Strike Resolver** | 24h | **16h** ⚡ | Dev 2 | Wed-Fri |
| **Day 6-8: Integration Test** | 16h | 16h | Both | Mon-Wed |

**📊 Impact:** 8-hour buffer for edge case testing, documentation polish

---

## 🔍 Day 1-2: Backend Audit (16 hours)

### Task 1.1: Map Existing Execution Flow

**Goal:** Understand how strategies execute today

**File to Audit:**
```
c:\Users\admin\Desktop\trading_terminal_cpp\src\strategies\CustomStrategy.cpp
```

**Questions to Answer:**
```cpp
// 1. How does CustomStrategy parse JSON?
void CustomStrategy::init(const StrategyInstance &instance) {
  QString jsonStr = m_instance.parameters.value("definition").toString();
  m_definition = StrategyParser::parseJSON(doc.object(), error);
  // ⚠️ What happens if JSON has "mode":"options"?
}

// 2. How does it place orders?
void CustomStrategy::placeEntryOrder(const QString &side) {
  XTS::OrderParams params = buildLimitOrderParams(side, qty);
  emit orderRequested(params);
  // ✅ This works - we can reuse for options
}

// 3. Where does symbol resolution happen?
// ⚠️ Need to find if any ATM logic exists in JodiATMStrategy
```

**Deliverable:** Document with answers (Google Doc or `.md` file)

---

### Task 1.2: Audit JodiATMStrategy (ATM Logic Reuse)

**Goal:** Check if ATM strike resolution logic already exists

**File to Read:**
```
c:\Users\admin\Desktop\trading_terminal_cpp\src\strategies\JodiATMStrategy.cpp
```

**Code to Extract:**
```cpp
// Look for functions like:
QString JodiATMStrategy::resolveATMStrike(double spot) {
  // If this exists, we can copy it!
  // If not, we need to write it from scratch
}

QString JodiATMStrategy::buildOptionSymbol(
  const QString &underlying,
  double strike,
  const QString &type,  // "CE" or "PE"
  const QString &expiry
) {
  // If this exists, HUGE win!
}
```

**Expected Outcome:**
- ✅ BEST CASE: ATM logic exists → Copy to new OptionsExecutionEngine
- ❌ WORST CASE: No ATM logic → Write from scratch (add 4 hours)

**Action:** Read file and document findings

---

### Task 1.3: Audit ContractData Symbol Lookup

**Goal:** Verify we can lookup option contracts by strike

**Files to Check:**
```
c:\Users\admin\Desktop\trading_terminal_cpp\include\repository\ContractData.h
c:\Users\admin\Desktop\trading_terminal_cpp\src\repository\MasterFileParser.cpp
```

**Test Code to Write:**
```cpp
// In POC test file
void testSymbolLookup() {
  // Can we find NIFTY 24550 CE expiring next week?
  auto contracts = ContractData::instance().searchOptions(
    "NIFTY",      // underlying
    24550,        // strike
    "CE",         // type
    "2026-02-20"  // expiry
  );
  
  if (contracts.isEmpty()) {
    qCritical() << "FAIL: Cannot lookup options by strike";
    // This is a NO-GO issue
  } else {
    qDebug() << "SUCCESS: Found" << contracts.size() << "contracts";
    qDebug() << "Symbol:" << contracts.first().symbol;
    qDebug() << "Token:" << contracts.first().token;
  }
}
```

**Success Criteria:**
- ✅ Method exists to search options by (underlying, strike, type, expiry)
- ✅ Returns valid contract with tradingSymbol and token

**Deliverable:** Working test function that proves lookup works

---

## 🔨 Day 3-5: Stream A - Options Parser (24 hours)

### Task 2.1: Extend StrategyDefinition.h (2 hours)

**File:** `c:\Users\admin\Desktop\trading_terminal_cpp\include\strategy\StrategyDefinition.h`

**Changes Required:**

```cpp
// BEFORE (Line ~140):
struct OptionLeg {
  QString legId;
  QString side;
  QString optionType;
  StrikeSelectionMode strikeMode = StrikeSelectionMode::ATMRelative;
  int atmOffset = 0;
  double targetPremium = 0.0;
  int fixedStrike = 0;
  ExpiryType expiry = ExpiryType::CurrentWeekly;
  QString specificExpiry;
  int quantity = 0;
  // ❌ MISSING: symbol field
};

// AFTER:
struct OptionLeg {
  QString legId;
  QString side;
  QString optionType;
  
  // ✅ ADD: Per-leg symbol override
  QString symbol;           // NEW: "NIFTY" or "BANKNIFTY"
  bool usesBaseSymbol = true;  // NEW: If true, falls back to strategy symbol
  
  StrikeSelectionMode strikeMode = StrikeSelectionMode::ATMRelative;
  int atmOffset = 0;
  double targetPremium = 0.0;
  int fixedStrike = 0;
  ExpiryType expiry = ExpiryType::CurrentWeekly;
  QString specificExpiry;
  int quantity = 0;
};
```

**Next: Add to StrategyDefinition struct:**

```cpp
// Around Line 95
struct StrategyDefinition {
  QString strategyId;
  QString name;
  QString version;
  QString symbol;
  int segment = 0;
  QString timeframe;
  
  // ✅ ADD: Strategy mode
  enum class Mode {
    Indicator,  // Old: indicator-based entry/exit
    Options,    // New: multi-leg options
    MultiSymbol // Future: spread/correlation
  };
  Mode mode = Mode::Indicator;  // NEW
  
  // ✅ ADD: Options-specific fields
  QVector<OptionLeg> optionLegs;  // NEW
  int atmRecalcPeriodSec = 30;    // NEW
  
  // Existing fields...
  QVariantMap userParameters;
  QVector<IndicatorConfig> indicators;
  ConditionGroup longEntryRules;
  ConditionGroup shortEntryRules;
  ConditionGroup longExitRules;
  ConditionGroup shortExitRules;
  RiskParams riskManagement;
  
  bool isValid() const {
    if (mode == Mode::Options) {
      return !name.isEmpty() && !symbol.isEmpty() && !optionLegs.isEmpty();
    }
    return !name.isEmpty() && !symbol.isEmpty() &&
           (!longEntryRules.isEmpty() || !shortEntryRules.isEmpty());
  }
};
```

**Compile Test:**
```bash
cd c:\Users\admin\Desktop\trading_terminal_cpp\build_ninja
.\build.bat
```

**Success:** Compiles without errors

---

### Task 2.2: Extend StrategyParser.cpp (12 hours)

**File:** `c:\Users\admin\Desktop\trading_terminal_cpp\src\strategy\StrategyParser.cpp`

**Add New Parser Functions:**

```cpp
// At end of file (after existing functions)

// ═══════════════════════════════════════════════════════════
// OPTIONS PARSING - NEW
// ═══════════════════════════════════════════════════════════

OptionLeg StrategyParser::parseOptionLeg(const QJsonObject &json) {
  OptionLeg leg;
  
  leg.legId = json["id"].toString();
  leg.side = json["side"].toString().toUpper();  // "BUY" or "SELL"
  leg.optionType = json["option_type"].toString().toUpper();  // "CE" or "PE"
  
  // Per-leg symbol override
  leg.symbol = json["symbol"].toString();
  leg.usesBaseSymbol = json["uses_base_symbol"].toBool(true);
  
  // Strike selection
  QString strikeStr = json["strike"].toString();
  if (strikeStr.startsWith("ATM")) {
    leg.strikeMode = StrikeSelectionMode::ATMRelative;
    
    // Parse "ATM", "ATM+1", "ATM-2"
    if (strikeStr.length() > 3) {
      QString offsetStr = strikeStr.mid(3);  // "+1" or "-2"
      leg.atmOffset = offsetStr.toInt();
    } else {
      leg.atmOffset = 0;
    }
  } else if (strikeStr == "PremiumBased") {
    leg.strikeMode = StrikeSelectionMode::PremiumBased;
    leg.targetPremium = json["target_premium"].toDouble();
  } else {
    leg.strikeMode = StrikeSelectionMode::FixedStrike;
    leg.fixedStrike = strikeStr.toInt();
  }
  
  // Expiry
  QString expiryStr = json["expiry"].toString();
  if (expiryStr == "Current Weekly") {
    leg.expiry = ExpiryType::CurrentWeekly;
  } else if (expiryStr == "Next Weekly") {
    leg.expiry = ExpiryType::NextWeekly;
  } else if (expiryStr == "Current Monthly") {
    leg.expiry = ExpiryType::CurrentMonthly;
  } else {
    leg.expiry = ExpiryType::SpecificDate;
    leg.specificExpiry = expiryStr;
  }
  
  leg.quantity = json["quantity"].toInt();
  
  return leg;
}

QVector<OptionLeg> StrategyParser::parseOptionLegs(const QJsonArray &arr) {
  QVector<OptionLeg> legs;
  for (const auto &item : arr) {
    legs.append(parseOptionLeg(item.toObject()));
  }
  return legs;
}
```

**Update parseJSON() to Handle Options Mode:**

```cpp
// In parseJSON() function (around Line 20)
StrategyDefinition StrategyParser::parseJSON(const QJsonObject &json,
                                             QString &errorMsg) {
  StrategyDefinition def;
  errorMsg.clear();

  // Basic fields
  def.strategyId = json["strategy_id"].toString();
  def.name = json["name"].toString();
  def.version = json["version"].toString("1.0");
  def.symbol = json["symbol"].toString();
  def.segment = json["segment"].toInt(2);
  def.timeframe = json["timeframe"].toString("1m");

  if (def.name.isEmpty()) {
    errorMsg = "Strategy name is required";
    return def;
  }
  if (def.symbol.isEmpty()) {
    errorMsg = "Symbol is required";
    return def;
  }

  // ✅ NEW: Check strategy mode
  QString modeStr = json["mode"].toString("indicator");
  if (modeStr == "options") {
    def.mode = StrategyDefinition::Mode::Options;
    
    // Parse options-specific fields
    if (json.contains("options_config")) {
      QJsonObject optConfig = json["options_config"].toObject();
      def.atmRecalcPeriodSec = optConfig["atm_recalc_period_sec"].toInt(30);
    }
    
    // Parse legs
    if (json.contains("legs")) {
      def.optionLegs = parseOptionLegs(json["legs"].toArray());
    }
    
    if (def.optionLegs.isEmpty()) {
      errorMsg = "Options mode requires at least one leg";
      return def;
    }
    
  } else {
    // OLD: Indicator mode (existing code)
    def.mode = StrategyDefinition::Mode::Indicator;
    
    // Parse indicators
    if (json.contains("indicators")) {
      // ... existing indicator parsing code
    }
    
    // Parse entry/exit rules
    if (json.contains("long_entry_rules")) {
      // ... existing rules parsing code
    }
  }

  // Parse risk management (common to both modes)
  if (json.contains("risk_management")) {
    def.riskManagement = parseRiskParams(json["risk_management"].toObject());
  }
  
  return def;
}
```

**Update StrategyParser.h Header:**

```cpp
// Add to StrategyParser class (around Line 45)
private:
  static IndicatorConfig parseIndicatorConfig(const QJsonObject &json);
  static Condition parseCondition(const QJsonObject &json);
  static ConditionGroup parseConditionGroup(const QJsonObject &json);
  static RiskParams parseRiskParams(const QJsonObject &json);
  
  // ✅ NEW: Options parsing
  static OptionLeg parseOptionLeg(const QJsonObject &json);
  static QVector<OptionLeg> parseOptionLegs(const QJsonArray &arr);
```

**Unit Test:**

```cpp
// In tests/ folder
void testOptionsLegParsing() {
  QString json = R"({
    "mode": "options",
    "name": "ATM Straddle Test",
    "symbol": "NIFTY",
    "legs": [
      {
        "id": "LEG_1",
        "symbol": "NIFTY",
        "uses_base_symbol": false,
        "side": "SELL",
        "option_type": "CE",
        "strike": "ATM",
        "expiry": "Current Weekly",
        "quantity": 50
      },
      {
        "id": "LEG_2",
        "symbol": "NIFTY",
        "uses_base_symbol": false,
        "side": "SELL",
        "option_type": "PE",
        "strike": "ATM",
        "expiry": "Current Weekly",
        "quantity": 50
      }
    ]
  })";
  
  QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
  QString error;
  StrategyDefinition def = StrategyParser::parseJSON(doc.object(), error);
  
  // Assert
  assert(error.isEmpty());
  assert(def.mode == StrategyDefinition::Mode::Options);
  assert(def.optionLegs.size() == 2);
  assert(def.optionLegs[0].side == "SELL");
  assert(def.optionLegs[0].optionType == "CE");
  assert(def.optionLegs[0].atmOffset == 0);
  assert(def.optionLegs[1].optionType == "PE");
  
  qDebug() << "✅ Options parsing test PASSED";
}
```

---

## 🔨 Day 3-5: Stream B - Strike Resolver (16 hours) ⚡ REVISED

**🎯 AUDIT FINDING:** Existing `ATMCalculator` + `RepositoryManager` provide battle-tested ATM resolution!  
**📄 See:** [08_BACKEND_AUDIT_FINDINGS.md](./08_BACKEND_AUDIT_FINDINGS.md)

**Original Estimate:** 24 hours  
**Revised Estimate:** 16 hours (-8h savings)

**Reuse Components:**
- ✅ `ATMCalculator::calculateFromActualStrikes()` - Binary search for nearest strike
- ✅ `RepositoryManager::getStrikesForSymbolExpiry()` - O(1) sorted strikes cache  
- ✅ `RepositoryManager::getTokensForStrike()` - CE/PE token lookup

### Task 3.1: Create OptionsExecutionEngine Class (4 hours) ⚡ REDUCED

**New Files:**
```
c:\Users\admin\Desktop\trading_terminal_cpp\include\strategy\OptionsExecutionEngine.h
c:\Users\admin\Desktop\trading_terminal_cpp\src\strategy\OptionsExecutionEngine.cpp
```

**Required Includes (for reusing existing components):**
```cpp
#include "utils/ATMCalculator.h"              // ✅ Binary search ATM resolution
#include "repository/RepositoryManager.h"     // ✅ Strike cache & token lookup
```

**Header File:**

```cpp
// OptionsExecutionEngine.h
#ifndef OPTIONS_EXECUTION_ENGINE_H
#define OPTIONS_EXECUTION_ENGINE_H

#include "strategy/StrategyDefinition.h"
#include "api/XTSTypes.h"
#include <QDateTime>
#include <QString>
#include <QHash>

/**
 * @brief Executes multi-leg options strategies
 * 
 * Responsibilities:
 * - Resolve ATM strikes using ATMCalculator (REUSED from ATMWatchManager)
 * - Resolve expiry dates
 * - Build option symbols (NIFTY24550CE)
 * - Coordinate multi-leg execution
 * 
 * AUDIT NOTE: Reuses battle-tested ATMCalculator + RepositoryManager
 * See: 08_BACKEND_AUDIT_FINDINGS.md
 */
class OptionsExecutionEngine {
public:
  explicit OptionsExecutionEngine();
  
  /**
   * @brief Resolve ATM strike from spot price (REUSES ATMCalculator!)
   * @param spot Current spot price (e.g., 24567.50)
   * @param offset ATM offset: 0=ATM, +1=OTM1, -1=ITM1
   * @return Strike price (e.g., 24550)
   * 
   * Implementation: Calls ATMCalculator::calculateFromActualStrikes()
   *                 with strikes from RepositoryManager cache
   */
  static int resolveATMStrike(double spot, int offset = 0);
  
  /**
   * @brief Resolve expiry date
   * @param underlying Symbol (e.g., "NIFTY")
   * @param expiryType Current Weekly / Next Weekly / etc.
   * @return Expiry date string "2026-02-20"
   */
  static QString resolveExpiry(const QString &underlying, 
                                ExpiryType expiryType);
  
  /**
   * @brief Build option symbol for trading
   * @param underlying "NIFTY"
   * @param strike 24550
   * @param optionType "CE" or "PE"
   * @param expiry "2026-02-20"
   * @return Trading symbol "NIFTY24550CE26FEB2026"
   */
  static QString buildOptionSymbol(const QString &underlying,
                                   int strike,
                                   const QString &optionType,
                                   const QString &expiry);
  
  /**
   * @brief Resolve an OptionLeg to concrete symbol
   * @param leg Leg definition with ATM+1, etc.
   * @param spotPrice Current spot price
   * @return Resolved leg with concrete symbol and strike
   */
  struct ResolvedLeg {
    QString tradingSymbol;  // "NIFTY24550CE"
    uint32_t token;         // From contract master
    int strike;             // 24550
    QString side;           // "BUY" or "SELL"
    int quantity;           // 50
    bool valid;             // Resolution succeeded?
    QString errorMsg;       // If !valid
  };
  
  static ResolvedLeg resolveLeg(const OptionLeg &leg, 
                                 double spotPrice);
  
private:
  static int roundToStrikeInterval(double price, int interval = 50);
  static QString formatExpiry(const QDate &date);
};

#endif // OPTIONS_EXECUTION_ENGINE_H
```

**Implementation File (REVISED - Reuses Existing Components):**

```cpp
// OptionsExecutionEngine.cpp
#include "strategy/OptionsExecutionEngine.h"
#include "repository/ContractData.h"
#include "utils/ATMCalculator.h"              // ✅ REUSED: Binary search algorithm
#include "repository/RepositoryManager.h"     // ✅ REUSED: Strike cache & tokens
#include <QDate>
#include <QDebug>
#include <cmath>

OptionsExecutionEngine::OptionsExecutionEngine() {}

// ═══════════════════════════════════════════════════════════
// ATM STRIKE RESOLUTION (REVISED - Reuses Existing Components!)
// ═══════════════════════════════════════════════════════════

int OptionsExecutionEngine::resolveATMStrike(double spot, int offset) {
  // ✅ REUSE: ATMCalculator + RepositoryManager (8h time savings!)
  // See: 08_BACKEND_AUDIT_FINDINGS.md for details
  
  QString symbol = "NIFTY";   // TODO: Dynamic from leg.symbol
  QString expiry = "30JAN26";  // TODO: Dynamic from expiry resolution
  
  auto* repo = RepositoryManager::getInstance();
  
  // Step 1: Get sorted strikes from cache (O(1) hash lookup)
  const QVector<double>& strikes = 
      repo->getStrikesForSymbolExpiry(symbol, expiry);
  
  if (strikes.isEmpty()) {
    qCritical() << "No strikes found for" << symbol << expiry;
    return 0;
  }
  
  // Step 2: Find nearest strike (O(log n) binary search)
  auto result = ATMCalculator::calculateFromActualStrikes(spot, strikes, 0);
  
  if (!result.isValid) {
    qCritical() << "ATM calculation failed for spot:" << spot;
    return 0;
  }
  
  int atmStrike = static_cast<int>(result.atmStrike);
  
  // Step 3: Apply offset (ATM+1, ATM-2, etc.)
  if (offset != 0) {
    int currentIdx = strikes.indexOf(atmStrike);
    int targetIdx = currentIdx + offset;
    
    if (targetIdx >= 0 && targetIdx < strikes.size()) {
      atmStrike = static_cast<int>(strikes[targetIdx]);
    } else {
      qWarning() << "Strike offset out of bounds:" << offset;
    }
  }
  
  qDebug() << "ATM Resolution: Spot =" << spot 
           << "→ ATM =" << atmStrike 
           << "(offset =" << offset << ")";
  
  return atmStrike;
}

// roundToStrikeInterval() - NO LONGER NEEDED (Removed, using ATMCalculator instead)

// ═══════════════════════════════════════════════════════════
// EXPIRY RESOLUTION
// ═══════════════════════════════════════════════════════════

QString OptionsExecutionEngine::resolveExpiry(const QString &underlying,
                                               ExpiryType expiryType) {
  QDate today = QDate::currentDate();
  QDate expiryDate;
  
  switch (expiryType) {
    case ExpiryType::CurrentWeekly: {
      // Next Thursday
      int daysToThursday = (Qt::Thursday - today.dayOfWeek() + 7) % 7;
      if (daysToThursday == 0) daysToThursday = 7;  // If today is Thursday, next week
      expiryDate = today.addDays(daysToThursday);
      break;
    }
    
    case ExpiryType::NextWeekly: {
      // Thursday after next Thursday
      int daysToThursday = (Qt::Thursday - today.dayOfWeek() + 7) % 7;
      if (daysToThursday == 0) daysToThursday = 7;
      expiryDate = today.addDays(daysToThursday + 7);
      break;
    }
    
    case ExpiryType::CurrentMonthly: {
      // Last Thursday of current month
      // Simple approximation: 4 weeks from today
      // TODO: Implement accurate last-Thursday logic
      expiryDate = today.addDays(28);
      break;
    }
    
    default:
      expiryDate = today.addDays(7);
  }
  
  return formatExpiry(expiryDate);
}

QString OptionsExecutionEngine::formatExpiry(const QDate &date) {
  // Format: "2026-02-20"
  return date.toString("yyyy-MM-dd");
}

// ═══════════════════════════════════════════════════════════
// SYMBOL BUILDING
// ═══════════════════════════════════════════════════════════

QString OptionsExecutionEngine::buildOptionSymbol(
  const QString &underlying,
  int strike,
  const QString &optionType,
  const QString &expiry
) {
  // Convert expiry "2026-02-20" → "26FEB2026"
  QDate date = QDate::fromString(expiry, "yyyy-MM-dd");
  QString expiryPart = date.toString("yyMMMyyyy").toUpper();
  
  // Build: NIFTY24550CE26FEB2026
  QString symbol = QString("%1%2%3%4")
    .arg(underlying)
    .arg(strike)
    .arg(optionType)
    .arg(expiryPart);
  
  qDebug() << "Built symbol:" << symbol 
           << "from" << underlying << strike << optionType << expiry;
  
  return symbol;
}

// ═══════════════════════════════════════════════════════════
// LEG RESOLUTION (Combines above functions)
// ═══════════════════════════════════════════════════════════

OptionsExecutionEngine::ResolvedLeg 
OptionsExecutionEngine::resolveLeg(const OptionLeg &leg, double spotPrice) {
  ResolvedLeg resolved;
  resolved.side = leg.side;
  resolved.quantity = leg.quantity;
  resolved.valid = false;
  
  // 1. Resolve strike
  if (leg.strikeMode == StrikeSelectionMode::ATMRelative) {
    resolved.strike = resolveATMStrike(spotPrice, leg.atmOffset);
  } else if (leg.strikeMode == StrikeSelectionMode::FixedStrike) {
    resolved.strike = leg.fixedStrike;
  } else {
    resolved.errorMsg = "PremiumBased strike not yet implemented";
    return resolved;
  }
  
  // 2. Resolve expiry
  QString expiry = resolveExpiry(leg.symbol, leg.expiry);
  
  // 3. Build symbol
  QString tradingSymbol = buildOptionSymbol(
    leg.symbol,
    resolved.strike,
    leg.optionType,
    expiry
  );
  
  // 4. Lookup in contract master
  auto contract = ContractData::instance().findBySymbol(tradingSymbol);
  if (!contract.isValid()) {
    resolved.errorMsg = QString("Contract not found: %1").arg(tradingSymbol);
    qWarning() << resolved.errorMsg;
    return resolved;
  }
  
  // Success!
  resolved.tradingSymbol = tradingSymbol;
  resolved.token = contract.token;
  resolved.valid = true;
  
  qDebug() << "✅ Leg resolved:" 
           << leg.legId 
           << "→" << tradingSymbol 
           << "(token:" << resolved.token << ")";
  
  return resolved;
}
```

**Add to CMakeLists.txt:**

```cmake
# In src/CMakeLists.txt, add:
src/strategy/OptionsExecutionEngine.cpp
```

**Compile Test:**
```bash
cd build_ninja
.\build.bat
```

---

### Task 3.2: Unit Test Strike Resolution (2 hours) ⚡ REDUCED

**NOTE:** ATMCalculator already battle-tested via ATMWatchManager (270 symbols daily).  
Focus on **wrapper integration tests**, not algorithm correctness.

**Test File:** `tests/test_options_execution.cpp`

```cpp
#include "strategy/OptionsExecutionEngine.h"
#include "utils/ATMCalculator.h"
#include "repository/RepositoryManager.h"
#include <cassert>
#include <QDebug>

void testATMResolutionWrapper() {
  qDebug() << "\n=== Test: ATM Strike Resolution Wrapper ===";
  
  // Gate 1 Criteria #2: resolveATMStrike(24567.50, 0) returns 24550
  // This tests our WRAPPER, not ATMCalculator (already proven)
  
  // Test 1: Exact 50 boundary
  int strike1 = OptionsExecutionEngine::resolveATMStrike(24550.0, 0);
  assert(strike1 == 24550);
  qDebug() << "✅ Test 1 PASS: 24550.0 → 24550";
  
  // Test 2: Round down
  int strike2 = OptionsExecutionEngine::resolveATMStrike(24523.0, 0);
  assert(strike2 == 24500);
  qDebug() << "✅ Test 2 PASS: 24523.0 → 24500";
  
  // Test 3: Round up
  int strike3 = OptionsExecutionEngine::resolveATMStrike(24567.0, 0);
  assert(strike3 == 24550);
  qDebug() << "✅ Test 3 PASS: 24567.0 → 24550";
  
  // Test 4: ATM+1 (OTM)
  int strike4 = OptionsExecutionEngine::resolveATMStrike(24550.0, 1);
  assert(strike4 == 24600);
  qDebug() << "✅ Test 4 PASS: ATM+1 → 24600";
  
  // Test 5: ATM-1 (ITM)
  int strike5 = OptionsExecutionEngine::resolveATMStrike(24550.0, -1);
  assert(strike5 == 24500);
  qDebug() << "✅ Test 5 PASS: ATM-1 → 24500";
  
  qDebug() << "✅ All ATM resolution tests PASSED\n";
}

void testSymbolBuilding() {
  qDebug() << "\n=== Test: Symbol Building ===";
  
  QString symbol = OptionsExecutionEngine::buildOptionSymbol(
    "NIFTY",
    24550,
    "CE",
    "2026-02-20"
  );
  
  // Expected format: NIFTY24550CE26FEB2026
  qDebug() << "Built symbol:" << symbol;
  assert(symbol.contains("NIFTY"));
  assert(symbol.contains("24550"));
  assert(symbol.contains("CE"));
  
  qDebug() << "✅ Symbol building test PASSED\n";
}

void testExpiryResolution() {
  qDebug() << "\n=== Test: Expiry Resolution ===";
  
  QString expiry = OptionsExecutionEngine::resolveExpiry(
    "NIFTY",
    ExpiryType::CurrentWeekly
  );
  
  qDebug() << "Next weekly expiry:" << expiry;
  assert(!expiry.isEmpty());
  assert(expiry.contains("-"));  // Format: YYYY-MM-DD
  
  qDebug() << "✅ Expiry resolution test PASSED\n";
}

int main() {
  testATMResolution();
  testSymbolBuilding();
  testExpiryResolution();
  
  qDebug() << "\n🎉 ALL TESTS PASSED 🎉\n";
  return 0;
}
```

**Run Tests:**
```bash
cd build_ninja
.\test_options_execution.exe
```

**Expected Output:**
```
=== Test: ATM Strike Resolution ===
ATM Resolution: Spot = 24550 → ATM = 24550 (offset = 0)
✅ Test 1 PASS: 24550.0 → 24550
ATM Resolution: Spot = 24523 → ATM = 24500 (offset = 0)
✅ Test 2 PASS: 24523.0 → 24500
...
🎉 ALL TESTS PASSED 🎉
```

---

## 🔗 Day 6-8: Integration Test (16 hours)

### Task 4.1: Integrate into CustomStrategy (8 hours)

**File:** `c:\Users\admin\Desktop\trading_terminal_cpp\src\strategies\CustomStrategy.cpp`

**Add Options Mode Handling:**

```cpp
// Add new member in CustomStrategy.h
#include "strategy/OptionsExecutionEngine.h"

class CustomStrategy : public StrategyBase {
  // ... existing members
  
private:
  StrategyDefinition m_definition;
  IndicatorEngine m_indicatorEngine;
  
  // ✅ NEW: Options execution
  OptionsExecutionEngine m_optionsEngine;
  QVector<OptionsExecutionEngine::ResolvedLeg> m_resolvedLegs;
  QTimer *m_atmRecalcTimer = nullptr;
  
  // ✅ NEW: Options mode methods
  void initializeOptionsMode();
  void recalculateATMLegs();
  void executeOptionsLegs();
};
```

**Implementation:**

```cpp
// In CustomStrategy.cpp

void CustomStrategy::init(const StrategyInstance &instance) {
  StrategyBase::init(instance);

  // Parse JSON
  QString jsonStr = m_instance.parameters.value("definition").toString();
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
  QString error;
  m_definition = StrategyParser::parseJSON(doc.object(), error);
  
  if (!error.isEmpty()) {
    log("ERROR: Strategy parse failed: " + error);
    updateState(StrategyState::Error);
    return;
  }

  // ✅ NEW: Branch based on mode
  if (m_definition.mode == StrategyDefinition::Mode::Options) {
    initializeOptionsMode();
  } else {
    // Existing indicator mode
    m_indicatorEngine.configure(m_definition.indicators);
    m_timeframe = ChartData::stringToTimeframe(m_definition.timeframe);
  }
  
  log(QString("CustomStrategy initialized: '%1' | Mode: %2")
      .arg(m_definition.name)
      .arg(m_definition.mode == StrategyDefinition::Mode::Options 
           ? "Options" : "Indicator"));
}

void CustomStrategy::initializeOptionsMode() {
  log("Initializing OPTIONS mode");
  
  // Resolve all legs once at init (will recalc ATM periodically)
  recalculateATMLegs();
  
  // Setup ATM recalc timer
  if (m_definition.atmRecalcPeriodSec > 0) {
    m_atmRecalcTimer = new QTimer(this);
    m_atmRecalcTimer->setInterval(m_definition.atmRecalcPeriodSec * 1000);
    connect(m_atmRecalcTimer, &QTimer::timeout, 
            this, &CustomStrategy::recalculateATMLegs);
    m_atmRecalcTimer->start();
    
    log(QString("ATM recalculation every %1 seconds")
        .arg(m_definition.atmRecalcPeriodSec));
  }
}

void CustomStrategy::recalculateATMLegs() {
  // Get current spot price for main symbol
  double spotPrice = m_currentLTP;  // From latest tick
  
  if (spotPrice <= 0) {
    log("WARNING: No spot price available, cannot calc ATM");
    return;
  }
  
  m_resolvedLegs.clear();
  
  // Resolve each leg
  for (const auto &leg : m_definition.optionLegs) {
    // Use per-leg symbol or fall back to base symbol
    QString symbol = leg.usesBaseSymbol ? m_definition.symbol : leg.symbol;
    
    // TODO: Get spot price for this specific symbol
    // For now, assume all legs use same underlying spot
    
    auto resolved = m_optionsEngine.resolveLeg(leg, spotPrice);
    
    if (!resolved.valid) {
      log(QString("ERROR: Failed to resolve leg %1: %2")
          .arg(leg.legId)
          .arg(resolved.errorMsg));
      updateState(StrategyState::Error);
      return;
    }
    
    m_resolvedLegs.append(resolved);
    
    log(QString("Leg resolved: %1 %2 %3 x%4 → %5")
        .arg(leg.legId)
        .arg(resolved.side)
        .arg(leg.optionType)
        .arg(resolved.quantity)
        .arg(resolved.tradingSymbol));
  }
  
  log(QString("ATM recalculation complete: %1 legs resolved")
      .arg(m_resolvedLegs.size()));
}

void CustomStrategy::executeOptionsLegs() {
  if (m_resolvedLegs.isEmpty()) {
    log("ERROR: No resolved legs, cannot execute");
    return;
  }
  
  log("Executing multi-leg options strategy...");
  
  // Execute each leg
  for (const auto &leg : m_resolvedLegs) {
    // Build order params
    XTS::OrderParams params;
    params.exchangeInstrumentID = leg.token;
    params.orderSide = (leg.side == "BUY") 
                       ? XTS::OrderSide::Buy 
                       : XTS::OrderSide::Sell;
    params.orderQuantity = leg.quantity;
    params.orderType = XTS::OrderType::Limit;  // Always limit orders
    params.limitPrice = 0.0;  // TODO: Get market price with buffer
    params.productType = XTS::ProductType::Intraday;
    
    // Emit order request (handled by OrderExecutionEngine)
    emit orderRequested(params);
    
    log(QString("Order placed: %1 %2 %3 @ token %4")
        .arg(leg.side)
        .arg(leg.quantity)
        .arg(leg.tradingSymbol)
        .arg(leg.token));
  }
  
  log("✅ All legs execution requested");
  m_inPosition = true;
}
```

**Update onTick() to Handle Options Mode:**

```cpp
void CustomStrategy::onTick(const UDP::MarketTick &tick) {
  if (!m_isRunning)
    return;

  m_currentLTP = tick.ltp;
  m_latestTick = tick;

  // ✅ NEW: Options mode doesn't use candles
  if (m_definition.mode == StrategyDefinition::Mode::Options) {
    // Options mode: just track spot price
    // Entry logic: user manually clicks "Execute" or based on external trigger
    // For POC: auto-execute on first tick
    if (!m_inPosition && m_resolvedLegs.isEmpty()) {
      recalculateATMLegs();
    }
    return;
  }
  
  // OLD: Indicator mode
  updateCandle(tick);
  // ... rest of existing tick logic
}
```

---

### Task 4.2: End-to-End POC Test (8 hours)

**Test Scenario:**

```
1. Create ATM Straddle template JSON
2. Save to database
3. Deploy template via UI (manual)
4. Start strategy
5. Verify:
   - Legs resolved correctly
   - Orders sent to broker API
   - Both CE and PE orders visible
```

**Test JSON:**

```json
{
  "name": "POC ATM Straddle",
  "version": "1.0",
  "mode": "options",
  "symbol": "NIFTY",
  "segment": 2,
  "options_config": {
    "atm_recalc_period_sec": 30
  },
  "legs": [
    {
      "id": "LEG_CE",
      "symbol": "NIFTY",
      "uses_base_symbol": false,
      "side": "SELL",
      "option_type": "CE",
      "strike": "ATM",
      "expiry": "Current Weekly",
      "quantity": 50
    },
    {
      "id": "LEG_PE",
      "symbol": "NIFTY",
      "uses_base_symbol": false,
      "side": "SELL",
      "option_type": "PE",
      "strike": "ATM",
      "expiry": "Current Weekly",
      "quantity": 50
    }
  ],
  "risk_management": {
    "stop_loss_percent": 2.0,
    "target_percent": 3.0,
    "position_size": 50
  }
}
```

**Test Steps:**

1. **Insert into Database:**
```sql
INSERT INTO strategy_instances 
  (instance_name, strategy_type, symbol, segment, state, parameters)
VALUES
  ('POC_ATM_Straddle', 
   'Custom', 
   'NIFTY', 
   2, 
   'IDLE',
   '{"definition": <JSON_ABOVE>}');
```

2. **Start Application:**
```bash
cd build_ninja
.\TradingTerminal.exe
```

3. **Open Strategy Manager:**
- Find "POC_ATM_Straddle" in list
- Click "Start"

4. **Monitor Logs:**
```
[CustomStrategy] Initializing OPTIONS mode
[CustomStrategy] ATM recalculation complete: 2 legs resolved
[CustomStrategy] Leg resolved: LEG_CE SELL CE x50 → NIFTY24550CE26FEB2026
[CustomStrategy] Leg resolved: LEG_PE SELL PE x50 → NIFTY24550PE26FEB2026
[CustomStrategy] Order placed: SELL 50 NIFTY24550CE26FEB2026 @ token 12345
[CustomStrategy] Order placed: SELL 50 NIFTY24550PE26FEB2026 @ token 12346
[CustomStrategy] ✅ All legs execution requested
```

5. **Verify Orders in Broker:**
- Check if orders appear in broker's order book
- Verify symbol, quantity, side are correct

---

## 🚦 DECISION GATE 1: End of Week 2

### Gate Criteria

**To Pass Gate 1 (GO to Phase 1), Must Achieve:**

```
✅ 1. Options JSON Parsing
   Test: Parse JSON with "mode":"options" and "legs" array
   Result: StrategyDefinition.optionLegs populated correctly
   
✅ 2. Strike Resolution
   Test: resolveATMStrike(24567.50, 0) returns 24550
   Result: Within ±50 points of manual calculation
   
✅ 3. Symbol Building
   Test: buildOptionSymbol("NIFTY", 24550, "CE", "2026-02-20")
   Result: Returns valid trading symbol
   
✅ 4. Contract Lookup
   Test: Find contract by trading symbol
   Result: Returns valid token from master file
   
✅ 5. Order Placement
   Test: emit orderRequested() for CE and PE
   Result: orderRequested signal emitted twice with correct tokens
```

### Pass/Fail Determination

**PASS (GO):**
- All 5 criteria ✅
- orders visible in broker API (even if rejected)
- Logs show correct symbols and tokens

**FAIL (NO-GO):**
- ANY criteria ❌
- Strategy crashes on options mode
- Orders never reach broker API
- Wrong symbols/strikes calculated

### NO-GO Action Plan

**If Gate 1 Fails:**

1. **Assess Root Cause** (2 hours)
   - Is it parsing? → Fix parser
   - Is it strike logic? → Fix math
   - Is it symbol format? → Check NSE format rules
   - Is it contract lookup? → Check master file loading

2. **Redesign Decision Tree:**

   **Option A: Fix and Retry (2-4 days)**
   - If fix is < 16 hours → Fix and re-test

   **Option B: Simplify Scope (1 week)**
   - Remove ATM auto-calculation
   - Use fixed strikes only
   - Reduces risk but less flexible

   **Option C: External Engine (2 weeks)**
   - Embed Python execution engine
   - Use QuantConnect/Backtrader
   - Higher cost but proven

   **Option D: Abort Options Support (0 days)**
   - Keep only indicator-based strategies
   - Defer options to Phase 2
   - Lowest risk

3. **Escalate to Leadership:**
   - VP Engineering approval required
   - Discuss budget impact
   - Revise timeline

---

## 📊 Success Metrics

### Week 2 End KPIs

| Metric | Target | Measurement |
|--------|--------|-------------|
| **Parser Coverage** | 100% | All JSON fields parsed |
| **Strike Accuracy** | ±1 strike | Manual check vs code |
| **Order Success** | 2/2 orders | CE + PE both sent |
| **Crash Rate** | 0 | No crashes in POC test |
| **Code Coverage** | 70% | Unit tests pass |

**GATE 1 PASS = All targets met**

---

## 🛠️ Tools & Resources

### Development Tools
```
IDE: Visual Studio 2022 (MSVC)
Build: Ninja (build_ninja/build.bat)
Debugger: MSVC debugger
Testing: Qt Test Framework
```

### Reference Documentation
```
Options Symbol Format: NSE website
Strike Spacing: 50 for NIFTY/BANKNIFTY, 100 for BANKNIFTY (verify)
Expiry Rules: Last Thursday of month/week
```

### Contact Points
```
Backend Lead: [Your name]
QA: [QA lead]
Escalation: [VP Engineering]
```

---

## 📋 Daily Standup Template

**Day X of POC (Week 1-2)**

**Yesterday:**
- Task completed
- Blockers resolved

**Today:**
- Task in progress
- Expected completion time

**Blockers:**
- None / [Describe blocker]

**Risk Level:** 🟢 On Track / 🟡 At Risk / 🔴 Blocked

---

## ✅ Definition of Done

**Parser Complete:**
- [ ] Code compiles without errors
- [ ] Unit tests pass
- [ ] JSON with options mode parses successfully
- [ ] OptionLeg objects populated correctly

**Strike Resolver Complete:**
- [ ] resolveATMStrike() returns correct values
- [ ] All unit tests pass
- [ ] Manual verification matches code

**Integration Complete:**
- [ ] CustomStrategy initializes in options mode
- [ ] Legs resolve without errors
- [ ] orders emitted for all legs
- [ ] End-to-end test passes

**Gate 1 Complete:**
- [ ] All 5 criteria ✅
- [ ] Demo to stakeholders
- [ ] Decision: GO or NO-GO documented

---

## 🎯 Next Steps After Gate 1 PASS

**If Gate 1 Passes → Week 3 Sprint 1 Kickoff:**

1. **Refactor**: Clean up POC code
2. **Multi-Symbol**: Add support for different symbols per leg
3. **Execution Logic**: Implement sequential leg execution with rollback
4. **Error Handling**: Add comprehensive error handling
5. **Template UI**: Begin simple template creation UI

**Estimated: Week 3-6 (Phase 1 MVP Core)**

---

**Document Status:** ✅ Ready for Execution  
**Next Action:** Begin Day 1 backend audit  
**Owner:** Backend Lead  
**Timeline:** February 18 - March 1, 2026 (2 weeks)
