# Strategy Template Builder — Implementation Plan & Optimization Roadmap

> **Date:** 20 February 2026  
> **Status:** ✅ Phases 1–3 COMPLETED — Phase 4 remaining  
> **Prerequisite:** Read `STRATEGY_BUILDER_DEEP_ANALYSIS.md` for full context

---

## Progress Tracker

| Phase | Description | Status |
|-------|-------------|--------|
| **Phase 1** | Make It Compile & Connect | ✅ **DONE** |
| **Phase 2** | Make Indicators Work | ✅ **DONE** |
| **Phase 3** | Make Trading Work | ✅ **DONE** |
| **Phase 4** | Polish & Harden | 🔲 Pending |

### Completed Fixes (Phase 1–3):
- **B1/B13:** Fixed `__template_id__` vs `__templateId__` key mismatch → now reads both with fallback
- **B2/B14:** Fixed `__bindings__` vs `__symbolBindings__` key + type mismatch → handles QVariantList and JSON string
- **B3/B15:** Fixed `repo.loadAll()` → uses `loadTemplate(id)` directly
- **B4/B16:** Added `StrategyTemplateRepository::instance()` singleton with auto-open
- **B5:** Wired `CandleAggregator::candleComplete` → `IndicatorEngine::addCandle()` per symbol slot
- **B6:** Implemented `crosses_above` / `crosses_below` with `m_prevOperandValues` tracking
- **B7:** Fixed hardcoded `"BUY"` → reads `entrySide` from `SymbolDefinition`
- **B8:** Added PnL tracking in `placeExitOrder()` — `m_dailyPnL` now accumulates real P&L
- **B9:** Fixed `mtm_total` vs `mtm` field mismatch → both accepted in `resolveOperand()`
- **B10:** Added `m_exitInProgress` guard → prevents double-exit in same tick
- **B11:** Added timeframe tracking per symbol slot in `setupIndicators()` → feeds to CandleAggregator
- **NEW:** Added `entrySide` enum to `SymbolDefinition` with full JSON serialization/deserialization
- **NEW:** Added `m_symbolNames` tracking for CandleAggregator symbol routing
- **NEW:** Disconnect from CandleAggregator on `stop()`
- **NEW:** Reset `m_prevOperandValues` on `start()`

---

## Executive Summary

The Strategy Template Builder has strong architecture (template/instance pattern, recursive condition trees, proper formula parser) but the **runtime engine (`TemplateStrategy`) is broken in 8+ places**. Additionally, the **Deploy Dialog already exists** but has key mismatches with the runtime engine that prevent end-to-end execution.

This plan organizes fixes into 4 phases:
1. **Phase 1 — Make It Compile & Connect** (Critical wiring fixes)
2. **Phase 2 — Make Indicators Work** (Candle pipeline + crossovers)  
3. **Phase 3 — Make Trading Work** (Order side, multi-leg, PnL tracking)
4. **Phase 4 — Polish & Harden** (Validation, UX, edge cases)

---

## Current State Summary

### ✅ Already Working (No Changes Needed)
- `StrategyTemplate` data model — well-structured
- `ConditionNode` recursive tree — correct design
- `FormulaEngine` parser/evaluator — functional recursive descent
- `LiveFormulaContext` — correctly bridges to PriceStoreGateway
- `StrategyTemplateBuilderDialog` — full UI working (light theme applied)
- `ConditionBuilderWidget` — leaf editor, tree management working
- `StrategyTemplateRepository` — SQLite CRUD working
- `StrategyDeployDialog` — **EXISTS and is functional** (4-step wizard)
- `CandleAggregator` — **EXISTS as singleton**, processes ticks → candles

### ❌ Broken (Must Fix)
| # | Bug | File | Impact |
|---|---|---|---|
| B1 | `__template_id__` vs `__templateId__` key mismatch | TemplateStrategy.cpp ↔ StrategyDeployDialog.cpp | Template never loads |
| B2 | `__bindings__` vs `__symbolBindings__` key mismatch | TemplateStrategy.cpp ↔ StrategyDeployDialog.cpp | Bindings never load |
| B3 | `repo.loadAll()` called but method is `loadAllTemplates()` | TemplateStrategy.cpp | Won't compile |
| B4 | `StrategyTemplateRepository::instance()` — no singleton exists | TemplateStrategy.cpp | Won't compile |
| B5 | `Q_UNUSED(tick)` — candles never fed to IndicatorEngine | TemplateStrategy.cpp | Indicators always 0.0 |
| B6 | `crosses_above/below` hardcoded `return false` | TemplateStrategy.cpp | Crossover signals dead |
| B7 | `orderSide = "BUY"` hardcoded | TemplateStrategy.cpp | Short trades broken |
| B8 | `m_dailyPnL` never updated | TemplateStrategy.cpp | Daily loss limit inert |
| B9 | `mtm_total` vs `mtm` field mismatch in `resolveOperand()` | TemplateStrategy.cpp | Portfolio exits broken |
| B10 | Double exit possible in same tick | TemplateStrategy.cpp | Duplicate orders |
| B11 | Timeframe from template never used in runtime | TemplateStrategy.cpp | Wrong candle intervals |
| B12 | `FormulaEngine::validate()` tokenizes twice | FormulaEngine.cpp | Minor perf waste |

---

## Phase 1 — Make It Compile & Connect

> **Goal:** Fix all compilation errors and key mismatches so the deploy → runtime pipeline connects.  
> **Effort:** ~2 hours  
> **Files:** `TemplateStrategy.cpp`, `TemplateStrategy.h`, `StrategyTemplateRepository.h`

### 1.1 Fix Template ID Key Mismatch

**Problem:** Deploy stores `__templateId__`, runtime reads `__template_id__`

```
StrategyDeployDialog.cpp:  inst.parameters["__templateId__"]
TemplateStrategy.cpp:      m_instance.parameters.value("__template_id__")
```

**Fix in `TemplateStrategy.cpp`:**
```cpp
// Change to match StrategyDeployDialog's key:
QString templateId = m_instance.parameters.value("__templateId__").toString();
if (templateId.isEmpty()) {
    // Fallback for backward compat
    templateId = m_instance.parameters.value("__template_id__").toString();
}
```

### 1.2 Fix Bindings Key Mismatch

**Problem:** Deploy stores `__symbolBindings__` (as QVariantList), runtime reads `__bindings__` (as JSON string)

**Fix in `TemplateStrategy.cpp` `setupBindings()`:**
```cpp
// Read from the key that StrategyDeployDialog actually writes
QVariant bindingsVar = m_instance.parameters.value("__symbolBindings__");
if (!bindingsVar.isValid()) {
    // Fallback: try old key format
    bindingsVar = m_instance.parameters.value("__bindings__");
}

QVariantList bindingsList;
if (bindingsVar.type() == QVariant::List) {
    bindingsList = bindingsVar.toList();
} else if (bindingsVar.type() == QVariant::String) {
    // Legacy: parse JSON string
    QJsonDocument doc = QJsonDocument::fromJson(bindingsVar.toString().toUtf8());
    for (const auto &v : doc.array())
        bindingsList.append(v.toObject().toVariantMap());
}

for (const QVariant &v : bindingsList) {
    QVariantMap obj = v.toMap();
    // ... rest of binding setup
}
```

### 1.3 Fix `loadAll()` → `loadAllTemplates()` + Use `loadTemplate(id)` Instead

```cpp
bool TemplateStrategy::loadTemplate() {
    QString templateId = m_instance.parameters.value("__templateId__").toString();
    if (templateId.isEmpty())
        templateId = m_instance.parameters.value("__template_id__").toString();
    if (templateId.isEmpty()) {
        log("ERROR: No template ID in parameters");
        return false;
    }

    // Use direct ID lookup instead of loading all templates
    StrategyTemplateRepository repo;
    repo.open();
    bool ok = false;
    m_template = repo.loadTemplate(templateId, &ok);
    repo.close();

    if (!ok) {
        log(QString("ERROR: Template '%1' not found").arg(templateId));
        return false;
    }
    m_templateLoaded = true;
    return true;
}
```

### 1.4 Remove `StrategyTemplateRepository::instance()` Dependency

Since there's no singleton, create a local repo instance in `loadTemplate()` (as shown above) or add a singleton to the class header.

**Option A — Add singleton (preferred if used elsewhere too):**
Add to `StrategyTemplateRepository.h`:
```cpp
static StrategyTemplateRepository &instance() {
    static StrategyTemplateRepository s_instance;
    return s_instance;
}
```

---

## Phase 2 — Make Indicators Work

> **Goal:** Wire the existing `CandleAggregator` into `TemplateStrategy` so indicators compute real values.  
> **Effort:** ~4 hours  
> **Files:** `TemplateStrategy.h`, `TemplateStrategy.cpp`

### 2.1 Wire CandleAggregator → IndicatorEngine

The `CandleAggregator` already exists as a singleton and emits `candleComplete()`. We just need to subscribe and route candles.

**Add to `TemplateStrategy.h`:**
```cpp
// Candle routing
QHash<QString, QString> m_symbolNames;  // slotId → symbol name for CandleAggregator
QHash<QString, QString> m_slotTimeframes;  // slotId → timeframe string

// Crossover detection
QHash<QString, double> m_prevOperandValues;
QString operandKey(const Operand &op) const;

private slots:
    void onCandleComplete(const QString &symbol, int segment,
                          const QString &timeframe, const ChartData::Candle &candle);
```

**In `start()`:** Subscribe to `CandleAggregator` per symbol slot:
```cpp
auto &agg = CandleAggregator::instance();
for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
    QString slotId = it.key();
    // Determine timeframe for this slot's indicators
    QString tf = m_slotTimeframes.value(slotId, "1m");
    QString symName = m_symbolNames.value(slotId);
    agg.subscribeTo(symName, it->segment, {tf});
}
connect(&agg, &CandleAggregator::candleComplete,
        this, &TemplateStrategy::onCandleComplete);
```

**New slot `onCandleComplete()`:**
```cpp
void TemplateStrategy::onCandleComplete(const QString &symbol, int segment,
                                         const QString &timeframe,
                                         const ChartData::Candle &candle) {
    // Find which slot this candle belongs to
    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
        if (m_symbolNames.value(it.key()) == symbol &&
            it->segment == segment &&
            m_slotTimeframes.value(it.key()) == timeframe) {
            // Feed to the indicator engine for this slot
            auto indIt = m_indicators.find(it.key());
            if (indIt != m_indicators.end()) {
                (*indIt)->addCandle(candle);
            }
            break;
        }
    }
}
```

### 2.2 Store Timeframe per Symbol Slot During `setupIndicators()`

```cpp
void TemplateStrategy::setupIndicators() {
    // ... existing grouping code ...
    
    // Also track timeframe per slot
    for (const auto &indDef : m_template.indicators) {
        QString tf = indDef.timeframe.isEmpty() ? "1m" : indDef.timeframe;
        // Map TemplateBuilder's "D"/"5"/"15" to CandleAggregator's "1m"/"5m"/"15m"/"1d"
        if (tf == "D" || tf == "d") tf = "1d";
        else if (tf == "W") tf = "1w";
        else if (!tf.endsWith("m") && !tf.endsWith("d") && !tf.endsWith("h"))
            tf = tf + "m";  // "5" → "5m"
        m_slotTimeframes[indDef.symbolId] = tf;
    }
    
    // ... existing engine creation code ...
}
```

### 2.3 Implement `crosses_above` / `crosses_below`

```cpp
QString TemplateStrategy::operandKey(const Operand &op) const {
    switch (op.type) {
    case Operand::Type::Price:     return QString("P_%1_%2").arg(op.symbolId, op.field);
    case Operand::Type::Indicator: return QString("I_%1_%2").arg(op.indicatorId, op.outputSeries);
    case Operand::Type::Constant:  return QString("C_%1").arg(op.constantValue);
    case Operand::Type::ParamRef:  return QString("R_%1").arg(op.paramName);
    case Operand::Type::Formula:   return QString("F_%1").arg(qHash(op.formulaExpression));
    case Operand::Type::Greek:     return QString("G_%1_%2").arg(op.symbolId, op.field);
    default:                       return QString("X_%1").arg((int)op.type);
    }
}

// In evaluateCondition(), replace the TODO:
if (node.op == "crosses_above" || node.op == "crosses_below") {
    double curL  = resolveOperand(node.left);
    double curR  = resolveOperand(node.right);
    QString kl   = operandKey(node.left);
    QString kr   = operandKey(node.right);
    double prevL = m_prevOperandValues.value(kl, curL);
    double prevR = m_prevOperandValues.value(kr, curR);
    m_prevOperandValues[kl] = curL;
    m_prevOperandValues[kr] = curR;

    if (node.op == "crosses_above")
        return (prevL <= prevR) && (curL > curR);
    else
        return (prevL >= prevR) && (curL < curR);
}
```

### 2.4 Indicator Warm-up on Start

Before subscribing to live ticks, try to pre-load historical candles so indicators have warm-up data. This depends on whether a `HistoricalDataStore` is available — if not, skip gracefully.

---

## Phase 3 — Make Trading Work

> **Goal:** Fix order side, add exit-guard, update PnL, fix field mismatches.  
> **Effort:** ~3 hours  
> **Files:** `TemplateStrategy.cpp`, `TemplateStrategy.h`, `StrategyTemplate.h`

### 3.1 Add Order Side to Template

**In `StrategyTemplate.h` — `SymbolDefinition`:**
```cpp
struct SymbolDefinition {
    // ... existing fields ...
    
    enum class EntrySide { Buy, Sell };
    EntrySide entrySide = EntrySide::Buy;
};
```

**In `StrategyTemplateBuilderDialog`:** Add an "Entry Side" combo to the symbols table (BUY / SELL).

**In `TemplateStrategy::placeEntryOrder()`:**
```cpp
params.orderSide = (sym.entrySide == SymbolDefinition::EntrySide::Sell) ? "SELL" : "BUY";
```

**In `placeExitOrder()`:** Reverse of entry:
```cpp
params.orderSide = (sym.entrySide == SymbolDefinition::EntrySide::Sell) ? "BUY" : "SELL";
```

### 3.2 Fix Double-Exit Guard

**Add to `TemplateStrategy.h`:**
```cpp
bool m_exitInProgress = false;
```

**Fix `onTick()`:**
```cpp
void TemplateStrategy::onTick(const UDP::MarketTick &tick) {
    if (!m_isRunning) return;

    // Update formula context (prices already in PriceStoreGateway)
    refreshExpressionParams();

    // Check risk limits (with guard)
    if (m_hasPosition && !m_exitInProgress) {
        checkRiskLimits();
    }

    // Entry check
    if (!m_hasPosition && !m_entrySignalFired && !m_exitInProgress) {
        if (evaluateCondition(m_template.entryCondition)) {
            log("✓ ENTRY condition met");
            m_entrySignalFired = true;
            placeEntryOrder();
        }
    }

    // Exit check (only if risk didn't already trigger)
    if (m_hasPosition && !m_exitInProgress) {
        if (evaluateCondition(m_template.exitCondition)) {
            log("✓ EXIT condition met");
            m_exitInProgress = true;
            placeExitOrder();
        }
    }
}
```

**In `placeExitOrder()`:**
```cpp
m_hasPosition = false;
m_entrySignalFired = false;
m_exitInProgress = false;   // reset after order placed
m_entryPrice = 0.0;
```

### 3.3 Fix `Total` Operand Field Name Mismatch

**In `resolveOperand()`, `Operand::Type::Total` case:**
```cpp
case Operand::Type::Total: {
    if (op.field == "mtm_total" || op.field == "mtm")
        return m_formulaContext.mtm();
    if (op.field == "net_premium")
        return m_formulaContext.netPremium();
    if (op.field == "net_delta")
        return m_formulaContext.netDelta();
    if (op.field == "net_gamma" || op.field == "net_theta" ||
        op.field == "net_vega" || op.field == "net_qty" ||
        op.field == "open_trades")
        return 0.0;  // TODO: Add to LiveFormulaContext
    return 0.0;
}
```

### 3.4 Track Daily PnL

**Add order fill callback to `StrategyBase` (if not already present):**
```cpp
void TemplateStrategy::onOrderFill(int segment, int token, double fillPrice, int qty, const QString &side) {
    if (side == "SELL" || side == "BUY") {
        double pnl = 0.0;
        if (m_entryPrice > 0) {
            if (side == "SELL")
                pnl = (fillPrice - m_entryPrice) * qty;  // long close
            else
                pnl = (m_entryPrice - fillPrice) * qty;  // short close
        }
        m_dailyPnL += pnl;
        log(QString("Order filled: %1 @ %.2f, PnL: %.2f, Daily: %.2f")
            .arg(side).arg(fillPrice).arg(pnl).arg(m_dailyPnL));
    }
}
```

---

## Phase 4 — Polish & Harden

> **Goal:** UI improvements, validation, edge case handling.  
> **Effort:** ~4 hours  
> **Files:** Various UI files

### 4.1 Debounce `refreshConditionContext()`

Add a 150ms debounce timer to `StrategyTemplateBuilderDialog` to prevent lag during rapid typing.

### 4.2 Pre-Save Validation

Walk the condition tree before saving to verify:
- All referenced indicator IDs exist in the indicators list
- All `{{PARAM_NAME}}` references exist in the params list
- All formula operands parse successfully
- At least one entry condition exists
- At least one Trade symbol exists

### 4.3 Fix `FormulaEngine::validate()` Double-Tokenize

Remove the first unused `tokenize()` call.

### 4.4 Template Pseudocode Preview

Add a read-only text area on the Conditions tab that renders the condition tree as human-readable pseudocode.

### 4.5 Fix `ConditionBuilderWidget::condition()` Implicit AND Warning

If multiple top-level items exist, show a small warning label: "Multiple top-level conditions are combined with AND."

---

## Execution Order

```
Week 1:
  ├── Phase 1 (2h)  — All compile fixes, key mismatches
  ├── Phase 2 (4h)  — CandleAggregator wiring, crossovers
  └── Phase 3 (3h)  — Order side, exit guard, field fixes

Week 2:
  ├── Phase 4 (4h)  — Validation, debounce, polish
  └── Integration Test — End-to-end: build template → deploy → run
```

Total estimated effort: **~13 hours of focused development**

---

## Files Modified Per Phase

| Phase | Files Modified |
|---|---|
| Phase 1 | `TemplateStrategy.cpp`, `TemplateStrategy.h`, `StrategyTemplateRepository.h` |
| Phase 2 | `TemplateStrategy.cpp`, `TemplateStrategy.h` |
| Phase 3 | `TemplateStrategy.cpp`, `TemplateStrategy.h`, `StrategyTemplate.h`, `StrategyTemplateBuilderDialog.cpp` (add Side column), `StrategyDeployDialog.cpp` (serialize side) |
| Phase 4 | `StrategyTemplateBuilderDialog.cpp/.h`, `ConditionBuilderWidget.cpp`, `FormulaEngine.cpp` |

---

## New Bugs Discovered During Planning

These were not in the original analysis and were found during this planning review:

| # | Bug | Details |
|---|---|---|
| **B13** | `__templateId__` vs `__template_id__` key mismatch | Deploy dialog writes `__templateId__`, runtime reads `__template_id__` → template never loads |
| **B14** | `__symbolBindings__` vs `__bindings__` key mismatch + type mismatch | Deploy writes `QVariantList`, runtime reads `QString` + parses as JSON |
| **B15** | `repo.loadAll()` called but method is `loadAllTemplates()` | Won't compile |
| **B16** | No singleton pattern for `StrategyTemplateRepository` | `instance()` call has no implementation |

These are **higher severity than all previously documented bugs** because they prevent the code from even compiling, let alone running.
