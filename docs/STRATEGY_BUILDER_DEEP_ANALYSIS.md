# Strategy Template Builder — Deep Analysis & Improvement Roadmap

> **Date:** February 2026  
> **Scope:** Full codebase audit of `StrategyTemplate`, `ConditionNode`, `FormulaEngine`, `TemplateStrategy`, `ConditionBuilderWidget`, `StrategyTemplateBuilderDialog`, and `StrategyTemplateRepository`

---

## Table of Contents

1. [System Architecture Overview](#1-system-architecture-overview)
2. [What Is Good — Genuine Strengths](#2-what-is-good--genuine-strengths)
3. [What Is Bad — Honest Weaknesses](#3-what-is-bad--honest-weaknesses)
4. [Real-World Viability Assessment](#4-real-world-viability-assessment)
5. [Critical Understanding Gaps](#5-critical-understanding-gaps)
6. [Bugs & Potential Issues (with File References)](#6-bugs--potential-issues-with-file-references)
7. [Ideal Strategy Template Builder Framework](#7-ideal-strategy-template-builder-framework)
8. [What to Change or Improve](#8-what-to-change-or-improve)
9. [Priority Action List](#9-priority-action-list)

---

## 1. System Architecture Overview

The current system has these major layers:

```
┌──────────────────────────────────────────────────────────────┐
│            StrategyTemplateBuilderDialog (UI)                 │
│  ┌─────────┐ ┌────────────┐ ┌────────┐ ┌──────────────────┐ │
│  │ Symbols │ │ Indicators │ │ Params │ │ ConditionBuilder │ │
│  └─────────┘ └────────────┘ └────────┘ └──────────────────┘ │
└───────────────────────────────┬──────────────────────────────┘
                                │ buildTemplate()
                                ▼
              ┌─────────────────────────────────┐
              │         StrategyTemplate         │  (plain struct)
              │  symbols / indicators / params   │
              │  entryCondition / exitCondition  │
              │  riskDefaults                    │
              └──────────────┬──────────────────┘
                             │ save/load
                             ▼
              ┌─────────────────────────────────┐
              │   StrategyTemplateRepository     │  (SQLite + JSON)
              └──────────────┬──────────────────┘
                             │ loadTemplate()
                             ▼
              ┌─────────────────────────────────┐
              │        TemplateStrategy          │  (runtime engine)
              │  IndicatorEngine (per symbol)    │
              │  FormulaEngine + LiveContext      │
              │  evaluateCondition() recursive   │
              │  placeEntryOrder / placeExit     │
              └─────────────────────────────────┘
```

The design is a **template-instance pattern**: a user designs a reusable blueprint (`StrategyTemplate`), which at deploy time becomes a `StrategyInstance` with concrete symbol bindings and parameter values, executed by `TemplateStrategy`.

---

## 2. What Is Good — Genuine Strengths

### ✅ 2.1 Clean Template / Instance Separation

The separation between `StrategyTemplate` (blueprint) and `StrategyInstance` (runtime deployment) is architecturally correct and matches how professional platforms (like Amibroker, NinjaTrader) work. The template itself is symbol-agnostic — you bind real instruments at deploy time via `SymbolBinding`. This is the single best design decision in the entire system.

### ✅ 2.2 Recursive Condition Tree (ConditionNode)

Using a proper recursive `AND/OR/Leaf` tree for conditions is the right call. It supports arbitrary depth logical nesting — `(RSI < 30 AND (LTP > SMA OR LTP > 22000))` — without hard-coded condition limits. The tree maps naturally to JSON for persistence and is easy to traverse.

### ✅ 2.3 FormulaEngine Is a Proper Recursive-Descent Parser

The FormulaEngine is a real tokenizer → parser → AST evaluator pipeline. It handles:
- Full operator precedence (not just `eval()` string tricks)
- Short-circuit evaluation for `&&` / `||`
- Ternary expressions
- Named parameter references
- Function calls dispatched to `FormulaContext`

This is significantly better than most homebrewed evaluators that use regex substitution. It is genuinely well-engineered.

### ✅ 2.4 Operand Type System Is Comprehensive

The `Operand::Type` enum covers:
- `Price`, `Indicator`, `Constant`, `ParamRef`, `Formula`, `Greek`, `Spread`, `Total`

This covers 95% of real Indian market strategy needs — including IV-based option exits, portfolio-level delta hedging, and bid-ask spread filtering. Having these as first-class operand types (not hacks) is clean.

### ✅ 2.5 FormulaContext Interface Is Properly Abstracted

`FormulaContext` is a pure abstract interface. `LiveFormulaContext` is the production implementation and can be replaced by a `BacktestFormulaContext` without changing any formula evaluation logic. This is the right OCP/DI pattern.

### ✅ 2.6 Persistent Storage with Soft Delete

Using SQLite with a `body_json` column + `deleted` flag is a reasonable choice. The soft delete prevents accidental loss of deployed strategy configurations. JSON inside SQLite makes the body schema-flexible.

### ✅ 2.7 Live Condition Preview in UI

The `LeafEditorDialog` has a live preview label that updates as the user changes operands: `RSI_1 < 30`. This significantly reduces configuration errors and is a UX detail that many commercial tools miss.

### ✅ 2.8 Parameter Expressions (Dynamic SL/Target)

The `ParamValueType::Expression` + `"__expr__:formula"` encoding allows parameters like `SL_LEVEL = ATR(REF_1, 14) * 2.5` to be computed on every tick. This enables ATR-based stops — a real professional feature.

### ✅ 2.9 Indicator Catalog Integration

Loading indicator metadata from `indicator_defaults.json` and auto-filling param labels, output selectors, and defaults in the UI is correct. It keeps the UI in sync with the backend indicator library.

### ✅ 2.10 Timeframe-Aware Indicators

`IndicatorDefinition::timeframe` ("1", "5", "15", "D") allows strategies to compute indicators on different candle intervals per symbol slot. This is crucial for real strategies that mix daily trend with intraday signals.

---

## 3. What Is Bad — Honest Weaknesses

### ❌ 3.1 `TemplateStrategy` Has No Candle Aggregation

**This is the most critical runtime failure.**

`TemplateStrategy::onTick()` receives UDP market ticks but **never feeds candles into `IndicatorEngine`**. The `IndicatorEngine::addCandle()` is never called from `TemplateStrategy`. Without candle history, every indicator returns `0.0` forever because `isReady()` is always false.

```cpp
// TemplateStrategy.cpp — onTick() — MISSING:
void TemplateStrategy::onTick(const UDP::MarketTick &tick) {
    if (!m_isRunning) return;
    Q_UNUSED(tick)  // ← Tick data is completely ignored!
    refreshExpressionParams();  // evaluates formulas with stale 0.0 indicators
    checkRiskLimits();
    ...
}
```

The tick data is tagged `Q_UNUSED` and discarded. There is no `CandleAggregator` wired in `TemplateStrategy`. All indicator conditions will evaluate against `0.0`, making every indicator-based strategy non-functional.

### ❌ 3.2 `crosses_above` / `crosses_below` Are Permanently Disabled

The crossover operators are hardcoded to `return false` with a `// TODO` comment:

```cpp
if (node.op == "crosses_above" || node.op == "crosses_below") {
    // TODO: Implement crossover with previous tick comparison
    return false;
}
```

These are the most commonly used signal types in technical analysis (EMA crossovers, RSI crossing thresholds, MACD signal line crossings). Any template that uses these operators will silently never fire an entry signal.

### ❌ 3.3 Order Side Is Hardcoded to "BUY"

```cpp
// TemplateStrategy.cpp — placeEntryOrder()
params.orderSide = "BUY";  // TODO: Determine from template
```

There is no way to express "SELL" entries (short trades, PUT buying, short futures). This makes the entire system only capable of long-only strategies. Options strategies (SELL calls, straddles) are completely broken.

### ❌ 3.4 Multi-Leg Order Management Is Absent

`StrategyTemplate` has `StrategyMode::OptionMultiLeg` and `StrategyMode::Spread`, but `TemplateStrategy::placeEntryOrder()` only places a single order on `tradeSymbols().first()`. There is no:
- Leg-by-leg order sequencing
- Leg ratio support (1:2 spreads, butterflies)
- Combined leg execution
- Per-leg stop-loss vs. combined P&L exit

### ❌ 3.5 `StrategyInstance` Has No Reference to Template's Symbol Bindings in Its Data Model

`StrategyInstance` stores `symbol` (a single string) and `segment` (a single int). It has no field for multiple symbol bindings (`__bindings__` is stuffed into `parameters` as a JSON string, not a typed field). This is fragile and makes querying/displaying binding info in the strategy list impossible without re-parsing JSON.

### ❌ 3.6 `TemplateStrategy::loadTemplate()` Does a Full Table Scan

```cpp
auto templates = repo.loadAll();  // loads every template
for (const auto &t : templates) {
    if (t.templateId == templateId) { ... }
}
```

This loads all templates into memory just to find one by ID. `StrategyTemplateRepository::loadTemplate(id)` exists but is not used here.

### ❌ 3.7 `StrategyTemplateRepository` Has No Singleton — Its `instance()` Is Called But Not Defined in the Provided Code

`TemplateStrategy.cpp` calls `StrategyTemplateRepository::instance()` but the repository class is a regular `QObject` with no static singleton pattern visible in the header. This will either fail to compile or silently reference a null/dangling object at runtime.

### ❌ 3.8 `SymbolBinding` and `StrategyTemplate` Structs Are Disconnected

`SymbolBinding` is defined in `StrategyTemplate.h` but is never stored inside `StrategyTemplate`. The template carries `SymbolDefinition` (the slot definitions), and bindings are packed into `StrategyInstance::parameters["__bindings__"]` as a JSON string by whoever deploys the template. There is no `DeployDialog` code shown that correctly creates this JSON, and the format is undocumented.

### ❌ 3.9 Risk Management Only Tracks the First Trade Symbol

```cpp
// checkRiskLimits() — only tracks first trade symbol
for (const auto &sym : m_template.tradeSymbols()) {
    ...
    break; // only check first trade symbol for now
}
```

In multi-leg strategies, this is wrong. Combined P&L across all legs must be tracked, not just the first leg.

### ❌ 3.10 Daily PnL Is Never Updated

```cpp
double m_dailyPnL = 0.0;
```

`m_dailyPnL` is initialized to 0.0 and checked in `checkRiskLimits()` but is never incremented after orders fill. There is no order fill callback wiring. The daily loss limit feature is therefore inert.

### ❌ 3.11 `IndicatorDefinition::timeframe` Is Stored But Never Used at Runtime

The `timeframe` field is stored in JSON and loaded correctly. But in `setupIndicators()`, timeframe is not read — there is no per-timeframe candle aggregator being created. All indicators will compute on whatever candle frequency they happen to receive, making the timeframe setting decorative.

### ❌ 3.12 No Deploy Dialog Is Connected to the Builder

Looking at `StrategyManagerWindow.cpp`, there is no visible `TemplateDeployDialog` that:
1. Takes a saved `StrategyTemplate`
2. Asks the user to bind each `SymbolDefinition` slot to a real instrument
3. Allows param value override
4. Packs `__bindings__` + `__template_id__` into `StrategyInstance::parameters`
5. Calls `TemplateStrategy::init()`

Without this glue code, a template can be built and saved but never actually deployed.

### ❌ 3.13 `FormulaEngine::callFunction` for Indicators Does Not Match `IndicatorEngine`'s ID System

```cpp
// FormulaEngine.cpp — calls:
m_context->indicator(symId, "BBANDS", period, period2, period3);

// But LiveFormulaContext bridges to:
IndicatorEngine::value("RSI_1");  // uses template-assigned ID

// IndicatorEngine stores results by ID like "RSI_1", "RSI_14", etc.
// The FormulaEngine calls indicator() by type+period, not by ID.
// LiveFormulaContext must create an ephemeral IndicatorConfig on the fly
// OR there is a mismatch: formula RSI(REF_1, 14) won't find "RSI_1" in the engine.
```

The bridge between `FormulaEngine`'s type+period style calls and `IndicatorEngine`'s ID-based storage is unimplemented or broken in `LiveFormulaContext`.

### ❌ 3.14 Condition Validation Has No Semantic Check

The `validate()` in `FormulaEngine` only checks syntax. In the condition builder, you can create semantically nonsensical conditions like `IV(TRADE_1) > VWAP(REF_1)` (comparing implied volatility percentage to price in rupees) with no warning. There is no unit/type checking.

### ❌ 3.15 UI: `refreshConditionContext()` Is Called Too Eagerly

Every `cellChanged`, every indicator card `changed` signal, every combo change triggers `refreshConditionContext()`. For large templates with many symbols, indicators, and params, this re-populates all combos in both condition builders synchronously on every keystroke. This will cause visible UI lag.

### ❌ 3.16 Template Name Uniqueness Is Not Enforced

Two templates can have the same name. Since the condition builder's `StrategyInstance` links by `templateId` (UUID), this is technically fine, but it causes confusion in the template picker list.

---

## 4. Real-World Viability Assessment

| Feature | Status | Reality Check |
|---|---|---|
| Save & reload a template | ✅ Works | SQLite persistence is solid |
| Multi-symbol slot design | ✅ Good design | Architecturally correct |
| Simple price comparison | ✅ Works | `LTP > 22000` type conditions work |
| Indicator-driven entry | ❌ Broken | Candles never fed to IndicatorEngine |
| Crossover signals | ❌ Broken | Hardcoded `return false` |
| Long-only BUY entry | ⚠️ Partially works | Hardcoded BUY, no fills tracked |
| Short entry / SELL | ❌ Missing | Hardcoded BUY |
| Options multi-leg | ❌ Broken | Only first leg executed, no sequencing |
| ATR-based dynamic SL | ❌ Broken | Depends on indicator (broken above) |
| Time-based exit | ✅ Mostly works | Timer fires but position PnL wrong |
| Daily loss limit | ❌ Broken | m_dailyPnL never updated |
| Template deploy flow | ❌ Missing | No DeployDialog wires it all together |
| Formula expressions | ✅ Engine works | Parser/evaluator is functional |
| Greeks-based conditions | ⚠️ Depends | Only if PriceStoreGateway has greeks populated |
| Backtesting | ❌ Not present | No BacktestFormulaContext implementation |

**Verdict:** The system is a well-designed prototype that cannot yet trade in production. The UI and data model are production-quality. The runtime engine has critical wiring gaps. Conservatively, **30-40% of the work remains** to make it actually execute strategies correctly.

---

## 5. Critical Understanding Gaps

### Gap 1: The Template vs. Instance vs. Execution Pipeline Is Not Fully Understood

The code reveals a confused understanding of who owns what:

- `StrategyTemplate` owns symbol *slots* (definitions)
- `SymbolBinding` (in `StrategyTemplate.h`) owns the *actual token mapping*
- But binding is serialized into `StrategyInstance::parameters["__bindings__"]` as raw JSON

This means there's no type-safe deploy step. The correct model is:

```
Template → (DeployDialog binds slots) → StrategyDeployment { template, bindings[], paramValues{} } → starts TemplateStrategy
```

This intermediate `StrategyDeployment` struct is missing from the codebase.

### Gap 2: Candle vs. Tick Distinction Is Not Internalized

The codebase receives UDP market ticks but computes indicators on candles. The bridge (a `CandleAggregator`) must:
1. Accumulate ticks into OHLCV candles per timeframe
2. On candle close, call `IndicatorEngine::addCandle()`
3. Indicator values are only valid on candle close, not on every tick

`TemplateStrategy` treats ticks and candle-based indicator evaluation as the same event. This misunderstanding is why the tick handler does `Q_UNUSED(tick)` — there's no candle pipeline connected.

### Gap 3: Order Side Must Be Part of the Template

The template needs a concept of **trade direction**:
- `BUY` entry / `SELL` exit (long strategy)
- `SELL` entry / `BUY` exit (short strategy)
- `BUY` call + `SELL` put (option strategy leg)

This must be specified per-leg in `SymbolDefinition` or in a separate `LegDefinition` struct. The current `SymbolDefinition` only has `role` (Reference vs. Trade) with no direction.

### Gap 4: Exit Condition vs. Stop-Loss vs. Target Are All Different Exit Triggers

The current code handles them as separate code paths (condition tree, `checkRiskLimits()`, `checkTimeExit()`) but there's no priority ordering. What happens if all three fire simultaneously? In real trading:
- Stop-loss should cancel any pending exit orders
- Only one exit order should be in flight at a time

The current implementation could fire multiple `placeExitOrder()` calls in the same tick.

### Gap 5: Indicator Re-computation on Historical Candles (Warm-up) Is Missing

When a strategy starts, IndicatorEngine needs historical candle data to warm up (RSI-14 needs 14 candles minimum). There is no candle history fetch on startup. RSI/EMA/ATR will return `0.0` for the first N minutes/days after the strategy starts, leading to false signals during warm-up.

### Gap 6: The `StrategyTemplateRepository::instance()` Pattern

`TemplateStrategy.cpp` calls `StrategyTemplateRepository::instance()` as if it's a singleton, but the class is a regular `QObject`. This is either a missing singleton implementation or a design mismatch that won't compile.

---

## 6. Bugs & Potential Issues (with File References)

### 🐛 Bug 1 — Critical: Ticks Ignored, Indicators Always Zero
**File:** `src/strategies/TemplateStrategy.cpp`, `onTick()` (line ~260)  
**Issue:** `Q_UNUSED(tick)` — tick data is discarded. No candle is built. No indicator gets data.  
**Effect:** Every indicator condition evaluates `0.0 < threshold` → true, causing spurious entries.

### 🐛 Bug 2 — Critical: `crosses_above` / `crosses_below` Always Return False
**File:** `src/strategies/TemplateStrategy.cpp`, `evaluateCondition()` (line ~310)  
**Issue:** Hardcoded `return false` for crossover operators.  
**Effect:** Any template using EMA cross, RSI crossing 30/70, MACD signal cross will never fire.

### 🐛 Bug 3 — Critical: Entry Order Always BUY
**File:** `src/strategies/TemplateStrategy.cpp`, `placeEntryOrder()` (line ~530)  
**Issue:** `params.orderSide = "BUY"` hardcoded.  
**Effect:** Short strategies, option sellers, all fail to execute correctly.

### 🐛 Bug 4 — Serious: `StrategyTemplateRepository::instance()` May Not Exist
**File:** `src/strategies/TemplateStrategy.cpp`, `loadTemplate()` (line ~80)  
**Issue:** `StrategyTemplateRepository::instance()` called but class has no singleton.  
**Effect:** Likely compile error or null pointer dereference at runtime.

### 🐛 Bug 5 — Serious: `m_dailyPnL` Never Updated
**File:** `include/strategies/TemplateStrategy.h` (declaration) + `src/strategies/TemplateStrategy.cpp`  
**Issue:** `m_dailyPnL` remains 0.0 forever. Daily loss limit check is always false.  
**Effect:** Max daily loss protection silently disabled.

### 🐛 Bug 6 — Serious: Timeframe Never Used in `setupIndicators()`
**File:** `src/strategies/TemplateStrategy.cpp`, `setupIndicators()` (line ~140)  
**Issue:** `indDef.timeframe` is read from the template but never used to select a candle series.  
**Effect:** All indicators compute on the same (nonexistent) candle stream regardless of their configured timeframe.

### 🐛 Bug 7 — Serious: Formula Indicator Calls Don't Map to IndicatorEngine IDs
**File:** `src/strategy/FormulaEngine.cpp`, `callFunction()` and `include/strategies/LiveFormulaContext.h`  
**Issue:** `FormulaEngine` calls `context->indicator(symId, "RSI", 14, 0, 0)` (type+period).  
`IndicatorEngine` stores values by ID string like `"RSI_1"`, `"RSI_14"`.  
`LiveFormulaContext::indicator()` must bridge this, but the implementation is not provided.  
**Effect:** All inline formula indicators (e.g., `RSI(REF_1, 14)` in a formula operand) likely return 0.0.

### 🐛 Bug 8 — Medium: Multiple Exit Firings in Single Tick
**File:** `src/strategies/TemplateStrategy.cpp`, `onTick()`  
**Issue:** `checkRiskLimits()` can call `placeExitOrder()`, then the exit condition tree can also call `placeExitOrder()` in the same tick.  
**Effect:** Double exit orders placed for the same position.

### 🐛 Bug 9 — Medium: `condition()` Creates Implicit AND if Multiple Top-Level Items
**File:** `src/ui/ConditionBuilderWidget.cpp`, `condition()` (line ~690)  
```cpp
if (m_tree->topLevelItemCount() > 1) {
    root.nodeType = ConditionNode::NodeType::And;
    // wraps everything in AND silently
}
```
**Issue:** If the user places two top-level OR groups, they get silently AND-ed.  
**Effect:** Logical structure doesn't match user intent, no warning given.

### 🐛 Bug 10 — Medium: `extractSymbols()` Accepts Empty ID Rows
**File:** `src/ui/StrategyTemplateBuilderDialog.cpp`, `extractSymbols()`  
**Issue:** Rows with empty ID are filtered out (`if (!sym.id.isEmpty())`), but the auto-generated IDs (`REF_1`, `TRADE_1`) can be manually blanked by the user. The condition builder then has no symbol to reference.  
**Effect:** Condition builder dropdowns go empty, leading to orphaned condition references.

### 🐛 Bug 11 — Medium: `Operand::Type::Total` Field Names Don't Match `resolveOperand()`
**File:** `src/strategies/TemplateStrategy.cpp`, `resolveOperand()` (line ~450)  
UI uses fields: `"mtm_total"`, `"net_premium"`, `"net_delta"` (from the combo)  
Runtime checks: `"mtm"`, `"net_premium"`, `"net_delta"`  
**Issue:** `"mtm_total"` ≠ `"mtm"` → the MTM check always returns 0.0.

### 🐛 Bug 12 — Medium: `IndicatorDefinition::param3` Duplication
**File:** `include/strategy/StrategyTemplate.h`  
`param3Str` (string, for `{{PARAM}}` or `"9"`) and `param3` (double) coexist.  
In JSON serialization, both are written. On load, `param3` is set from `param3Str.toDouble()` if available. But in `setupIndicators()`, `param3` is never read — only `period`, `period2` are resolved. Third parameter (e.g., BBANDS stddev multiplier) is always lost.

### 🐛 Bug 13 — Low: Indicator Card Insertion Off-by-One
**File:** `src/ui/StrategyTemplateBuilderDialog.cpp`, `addIndicatorCard()` (line ~363)  
```cpp
int insertPos = m_cardsLayout->count() - 1;
if (insertPos < 0) insertPos = 0;
```
When `m_cardsLayout` is empty (`count() == 1` due to the trailing stretch), `insertPos = 0`. New cards are inserted before the stretch, which is correct. But if the stretch was never added, `insertPos = -1` would corrupt the layout. The stretch is added in the constructor, so this is fragile.

### 🐛 Bug 14 — Low: `FormulaEngine::validate()` Tokenizes Twice
**File:** `src/strategy/FormulaEngine.cpp`, `validate()` (line ~720)  
```cpp
tokenize(expression, &ok);   // first call result discarded
QVector<FormulaToken> tokens = tokenize(expression, &ok);  // tokenized again
parse(tokens, &ok);
```
The first tokenize call is completely unused. Minor inefficiency.

### 🐛 Bug 15 — Low: `ConditionBuilderWidget::setCondition()` Top-Level Leaf Loses Empty Check
**File:** `src/ui/ConditionBuilderWidget.cpp`, `setCondition()`  
```cpp
if (root.isLeaf()) {
    if (!root.op.isEmpty()) addLeafItem(nullptr, leaf);
}
```
If a template is saved with a single bare leaf condition (valid JSON), it will be added as a top-level item in the tree. The toolbar can't add AND/OR groups as parents of this top-level leaf without deleting it first — there's no UX path to wrap it.

---

## 7. Ideal Strategy Template Builder Framework

This describes what the system *should* look like after proper re-engineering.

### 7.1 Core Data Model

```
StrategyTemplate              (reusable blueprint, no instruments)
├── SymbolSlot[]              (REF_1, TRADE_1, etc. — slot definitions)
├── IndicatorDefinition[]     (what to compute, which slot, which timeframe)
├── TemplateParam[]           (user-configurable knobs)
├── LegDefinition[]           (NEW: per-leg direction, qty ratio, instrument type)
├── ConditionSet              (entry, exit, reentry, hedge triggers)
│   ├── entryCondition        (ConditionNode tree)
│   ├── exitCondition         (ConditionNode tree)
│   ├── reentryCondition      (optional, for mean-reversion strategies)
│   └── adjustmentCondition   (optional, for delta hedging)
└── RiskDefaults

StrategyDeployment            (NEW: links template to real instruments)
├── templateId
├── SymbolBinding[]           (slot ID → real token + qty + lot size)
├── paramValues{}             (override map)
└── riskOverrides             (optional per-deployment risk overrides)

StrategyInstance              (running state)
├── deploymentId
├── state (Created/Running/Paused/Stopped/Error)
├── positions[]               (one per trade leg, with entry price, qty, side)
├── dailyStats                (trades, pnl, max drawdown)
└── log[]
```

### 7.2 Runtime Architecture

```
Market Feed (UDP ticks)
        │
        ▼
CandleAggregator (per symbol × timeframe)
        │  on candle close
        ▼
IndicatorEngine (per symbol slot)
        │
        ▼
TemplateStrategy::onCandle()      ← separate from onTick()
   ├── refreshExpressionParams()
   ├── evaluateCondition(entry)
   ├── evaluateCondition(exit)
   ├── evaluateCondition(adjustment)
   └── checkRiskLimits()           ← with guard flag to prevent double-fire

TemplateStrategy::onTick()        ← fast path, only for tick-based conditions
   ├── update portfolio MTM
   └── check time-exit timer
```

### 7.3 Condition System Improvements

**Current:** Two trees (entry, exit).  
**Ideal:** Four or more condition trees:

| Condition | When Used |
|---|---|
| `entryCondition` | No position open |
| `exitCondition` | Position open, normal exit |
| `stopLossCondition` | Override fixed % SL with dynamic condition |
| `adjustmentCondition` | Delta hedge, roll, or add-to-position |
| `reentryCondition` | After exit, when to re-enter |
| `hedgeCondition` | Options: when to buy protection |

**Crossover state machine:** Proper crossover detection needs one ring buffer of previous values:

```cpp
// Per operand cache — keyed by a hash of the operand definition
QHash<QString, double> m_prevOperandValues;

bool isCrossAbove(const Operand& left, const Operand& right) const {
    double curL = resolveOperand(left);
    double curR = resolveOperand(right);
    double prevL = m_prevOperandValues.value(operandKey(left), curL);
    double prevR = m_prevOperandValues.value(operandKey(right), curR);
    return prevL <= prevR && curL > curR;
}
```

### 7.4 Order Model Improvements

Each `LegDefinition` should specify:

```cpp
struct LegDefinition {
    QString symbolSlotId;           // which SymbolSlot
    OrderSide side;                 // BUY or SELL
    int qtyRatio = 1;               // for spreads: 1 long, 2 short, etc.
    OrderType entryOrderType;       // MARKET, LIMIT, SL, SL-M
    QString limitPriceFormula;      // e.g. "LTP(TRADE_1) - ATR(REF_1, 14) * 0.5"
    bool isHedge = false;           // hedge legs not counted in position P&L
    ExpirySelectionMode expiry;     // CURRENT_WEEK, NEXT_WEEK, MONTHLY
    StrikeSelectionMode strike;     // ATM, ATM+N, PREMIUM_BASED, FIXED
};
```

### 7.5 Template Versioning

Templates need semantic versioning with migration:
```
v1.0 → v1.1: added adjustmentCondition (backward compat — empty tree)
v1.1 → v2.0: changed LegDefinition schema (breaking change — needs migration)
```

Store in DB: `version TEXT`, and on load check version + run migration if needed.

### 7.6 Deploy Dialog Flow

The missing deploy dialog should:

1. **Step 1 — Select Template:** Show template list with mode badge (Indicator / MultiLeg / Spread)
2. **Step 2 — Bind Symbols:** For each `SymbolSlot`, provide an instrument search widget (with segment filter based on `SymbolSegment`)
3. **Step 3 — Configure Parameters:** Show only non-locked params with min/max spinboxes; locked params shown read-only
4. **Step 4 — Set Quantities:** One quantity spinner per Trade slot
5. **Step 5 — Review Risk:** Show template's RiskDefaults, allow override
6. **Step 6 — Deploy:** Validate all bindings, create `StrategyDeployment`, hand to `TemplateStrategy::init()`

### 7.7 Backtesting Integration

The `FormulaContext` abstraction is *designed for this*. Add:

```cpp
class BacktestFormulaContext : public FormulaContext {
    // Uses historical candle data from a CandleRepository
    // Simulates fills with slippage model
    // Produces trade log + performance metrics
};
```

### 7.8 Template Library & Sharing

A template marketplace or import/export:
- Export: serialize to JSON file (already have `templateToJson()`)
- Import: validate JSON schema before loading
- Library: ship a set of built-in templates (RSI Reversal, EMA Crossover, Short Straddle, etc.)

---

## 8. What to Change or Improve

### 8.1 Immediate Fixes (Production Blockers)

#### Fix 1: Wire CandleAggregator into TemplateStrategy

```cpp
// In setupIndicators(), after creating IndicatorEngine:
auto *agg = new CandleAggregator(indDef.timeframe.toInt(), this);
connect(agg, &CandleAggregator::candleClosed,
        engine, &IndicatorEngine::addCandle);
m_aggregators[slotId] = agg;

// In onTick():
void TemplateStrategy::onTick(const UDP::MarketTick &tick) {
    // Find which symbol slot this tick belongs to
    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
        if (it->segment == tick.segment && it->token == tick.token) {
            if (auto *agg = m_aggregators.value(it.key())) {
                agg->addTick(tick);  // builds candles → fires candleClosed → feeds IndicatorEngine
            }
        }
    }
    // ... rest of onTick
}
```

#### Fix 2: Implement Crossover Detection

```cpp
// Add to TemplateStrategy:
QHash<QString, double> m_prevOperandValues;

QString operandKey(const Operand &op) const {
    return QString("%1_%2_%3").arg((int)op.type).arg(op.symbolId).arg(op.indicatorId);
}

bool evaluateCrossover(const Operand &left, const Operand &right, bool above) const {
    double cur_l = resolveOperand(left);
    double cur_r = resolveOperand(right);
    QString kl = operandKey(left), kr = operandKey(right);
    double prev_l = m_prevOperandValues.value(kl, cur_l);
    double prev_r = m_prevOperandValues.value(kr, cur_r);
    const_cast<TemplateStrategy*>(this)->m_prevOperandValues[kl] = cur_l;
    const_cast<TemplateStrategy*>(this)->m_prevOperandValues[kr] = cur_r;
    return above ? (prev_l <= prev_r && cur_l > cur_r)
                 : (prev_l >= prev_r && cur_l < cur_r);
}
```

#### Fix 3: Add Order Side to SymbolDefinition/LegDefinition

```cpp
enum class OrderSide { Buy, Sell, Both }; // Both = straddle/strangle

struct SymbolDefinition {
    // ... existing fields ...
    OrderSide entrySide = OrderSide::Buy;   // NEW
};
```

And in `placeEntryOrder()`:
```cpp
params.orderSide = (sym.entrySide == OrderSide::Sell) ? "SELL" : "BUY";
```

#### Fix 4: Fix Total Operand Field Name Mismatch

In `resolveOperand()` in `TemplateStrategy.cpp`:
```cpp
case Operand::Type::Total: {
    if (op.field == "mtm_total" || op.field == "mtm") return m_formulaContext.mtm();
    // ... add both variants for all fields
}
```

#### Fix 5: Guard Against Double Exit

```cpp
bool m_exitInProgress = false;

void TemplateStrategy::checkRiskLimits() {
    if (!m_hasPosition || m_exitInProgress) return;
    ...
    m_exitInProgress = true;
    placeExitOrder();
}

void TemplateStrategy::placeExitOrder() {
    if (!m_hasPosition) return;  // guard
    m_hasPosition = false;       // atomically mark before placing order
    ...
}
```

### 8.2 Architecture Improvements (High Priority)

#### Add `StrategyDeployment` Struct

```cpp
struct StrategyDeployment {
    QString deploymentId;           // UUID
    QString templateId;
    QVector<SymbolBinding> bindings;   // slot → real instrument
    QVariantMap paramValues;           // override values
    RiskDefaults riskOverrides;
    QDateTime deployedAt;
};
```

Store this in a separate `strategy_deployments` SQLite table. Remove the `__bindings__` JSON hack from `StrategyInstance::parameters`.

#### Create `TemplateDeployDialog`

A 6-step wizard as described in section 7.6 above.

#### Add Warm-up Candle Fetch

On strategy start, before subscribing to live ticks, fetch historical candles from a `CandleRepository`:

```cpp
void TemplateStrategy::warmupIndicators() {
    for (auto it = m_indicators.begin(); it != m_indicators.end(); ++it) {
        int warmupNeeded = it.value()->minRequiredCandles();
        auto history = CandleRepository::instance().fetch(
            m_bindings[it.key()].segment,
            m_bindings[it.key()].token,
            m_indicatorTimeframes[it.key()],
            warmupNeeded + 10  // +10 buffer
        );
        for (const auto &c : history)
            it.value()->addCandle(c);
    }
}
```

### 8.3 UI Improvements

#### Debounce `refreshConditionContext()`

```cpp
// Add to StrategyTemplateBuilderDialog:
QTimer *m_refreshDebounceTimer = nullptr;

void StrategyTemplateBuilderDialog::scheduleContextRefresh() {
    if (!m_refreshDebounceTimer) {
        m_refreshDebounceTimer = new QTimer(this);
        m_refreshDebounceTimer->setSingleShot(true);
        m_refreshDebounceTimer->setInterval(150); // 150ms debounce
        connect(m_refreshDebounceTimer, &QTimer::timeout,
                this, &StrategyTemplateBuilderDialog::refreshConditionContext);
    }
    m_refreshDebounceTimer->start();
}
```

Replace all `refreshConditionContext()` calls with `scheduleContextRefresh()`.

#### Add Condition Validation Summary

Before saving, walk the full condition tree and validate:
- All indicator IDs in leaf conditions exist in the indicators list
- All param refs exist in the params list
- Formula operands are syntactically valid
- Show a summary of any issues in a pre-save review panel

#### Template Preview (Pseudocode Display)

On the final tab, show a human-readable pseudocode rendering of the template:

```
ENTRY when:
  AND:
    REF_1.RSI_14  <  30
    OR:
      TRADE_1.ltp  >  SMA_20
      TRADE_1.ltp  >  22000

EXIT when:
  OR:
    REF_1.RSI_14  >  70
    [Total] mtm_total  >  5000

RISK:
  Stop Loss: 1.0%  |  Target: 2.0%  |  Time Exit: 15:15
```

---

## 9. Priority Action List

| # | Item | Severity | Effort | Impact |
|---|---|---|---|---|
| 1 | Wire `CandleAggregator` into `TemplateStrategy::onTick()` | **CRITICAL** | Medium | All indicator strategies work |
| 2 | Implement `crosses_above` / `crosses_below` with prev-value cache | **CRITICAL** | Low | EMA cross / RSI cross strategies work |
| 3 | Add order side to template (`entrySide` per leg) | **CRITICAL** | Low | Short + option strategies work |
| 4 | Fix `Total` field name mismatch (`mtm_total` vs `mtm`) | **CRITICAL** | Trivial | P&L-based exits work |
| 5 | Fix double-exit guard in `onTick()` | **CRITICAL** | Low | No duplicate orders |
| 6 | Add `StrategyDeployment` struct + `TemplateDeployDialog` | **CRITICAL** | High | End-to-end deploy works |
| 7 | Add `StrategyTemplateRepository` singleton or DI | Serious | Low | Code compiles and runs |
| 8 | Implement `TemplateStrategy::warmupIndicators()` | Serious | Medium | No false signals on startup |
| 9 | Fix `m_dailyPnL` update on order fills | Serious | Medium | Daily loss limit works |
| 10 | Use `repo.loadTemplate(id)` instead of loadAll() scan | Serious | Trivial | Performance |
| 11 | Debounce `refreshConditionContext()` with 150ms timer | Medium | Low | Smooth UI typing |
| 12 | Add pre-save condition tree validation | Medium | Medium | User-facing error guidance |
| 13 | Add template pseudocode preview tab | Low | Low | UX |
| 14 | Add `BacktestFormulaContext` for strategy simulation | Low | High | Backtesting capability |
| 15 | Enforce template name uniqueness | Low | Trivial | UX clarity |

---

## Summary Verdict

The architecture **is fundamentally sound**. The template/instance pattern, recursive condition tree, proper formula parser, and abstract formula context are all production-grade design decisions. The data model is well-typed and persistable.

The runtime engine (`TemplateStrategy`) is where most of the bugs live — it's an incomplete implementation of an otherwise correct design. The two biggest gaps are:

1. **Candle pipeline is broken** — ticks are discarded, indicators never compute.
2. **Deploy flow is missing** — there is no UI path from "saved template" to "running strategy".

Fix those two things first, then work through the Priority Action List. Everything else is refinement on a solid foundation.

> **Estimated effort to production-ready:** ~3–4 weeks of focused development on the backend runtime fixes, plus ~1–2 weeks for the deploy dialog UI.
