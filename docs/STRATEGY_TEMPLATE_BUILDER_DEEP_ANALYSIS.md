# Strategy Template Builder — Deep Analysis

**Date:** June 2025  
**Scope:** All files in `include/strategy/`, `include/strategies/`, `src/strategy/`, `src/strategies/`, `src/ui/` related to the template builder, condition builder, formula engine, indicator engine, and the deploy/runtime pipeline.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Strengths](#2-strengths)
3. [Weaknesses & Design Gaps](#3-weaknesses--design-gaps)
4. [Potential Bugs & Runtime Issues](#4-potential-bugs--runtime-issues)
5. [Real-World Viability Assessment](#5-real-world-viability-assessment)
6. [Understanding Gaps](#6-understanding-gaps)
7. [Improvement Recommendations](#7-improvement-recommendations)
8. [Ideal Framework Vision](#8-ideal-framework-vision)
9. [Priority Action Items](#9-priority-action-items)

---

## 1. Architecture Overview

The system follows a **Template → Instance → Runtime** pipeline:

```
┌─────────────────────────────────────────────────────────────────────┐
│  DESIGN TIME (UI)                                                   │
│  ┌────────────────────────────────┐                                 │
│  │ StrategyTemplateBuilderDialog  │ ─── builds ──▶ StrategyTemplate │
│  │   ├── Symbols tab (table)      │                                 │
│  │   ├── Indicators tab (cards)   │                                 │
│  │   ├── Parameters tab (table)   │                                 │
│  │   ├── Conditions tab           │                                 │
│  │   │    └── ConditionBuilderWidget × 2 (entry/exit)              │
│  │   │         └── LeafEditorDialog (8 operand types)              │
│  │   └── Risk tab                 │                                 │
│  └────────────────────────────────┘                                 │
│                                     │                               │
│                    StrategyTemplateRepository (SQLite)               │
│                                     │                               │
├─────────────────────────────────────│───────────────────────────────┤
│  DEPLOY TIME (UI)                   ▼                               │
│  ┌────────────────────────────────┐                                 │
│  │ StrategyDeployDialog           │ ─── produces ──▶ StrategyInstance│
│  │   ├── Bind symbols to tokens   │    (with QVariantMap params)    │
│  │   ├── Fill param values         │                                │
│  │   └── Override risk settings    │                                │
│  └────────────────────────────────┘                                 │
│                                                                     │
├─────────────────────────────────────────────────────────────────────┤
│  RUNTIME                                                            │
│  ┌────────────────────────────────┐                                 │
│  │ TemplateStrategy : StrategyBase│                                 │
│  │   ├── loadTemplate()           │ ← StrategyTemplateRepository    │
│  │   ├── setupBindings()          │ ← SymbolBinding from params     │
│  │   ├── setupIndicators()        │ ← IndicatorEngine per symbol    │
│  │   ├── setupFormulaEngine()     │ ← FormulaEngine + LiveContext   │
│  │   ├── onTick()                 │ ← FeedHandler subscription      │
│  │   │   ├── refreshExpressionParams()                              │
│  │   │   ├── evaluateCondition()  │ ← Recursive tree evaluation     │
│  │   │   │   └── resolveOperand() │ ← Price/Indicator/Greek/etc.    │
│  │   │   ├── checkRiskLimits()    │                                 │
│  │   │   └── place{Entry|Exit}Order()                               │
│  │   └── onCandleComplete()       │ ← CandleAggregator → Indicator │
│  └────────────────────────────────┘                                 │
│                                                                     │
│  Supporting engines:                                                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐  │
│  │FormulaEngine  │  │IndicatorEng  │  │LiveFormulaContext        │  │
│  │ (AST parser)  │  │ (TA compute) │  │ (bridge: engine ↔ market)│  │
│  └──────────────┘  └──────────────┘  └──────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

### Key Data Structures

| Struct | File | Purpose |
|--------|------|---------|
| `StrategyTemplate` | `include/strategy/StrategyTemplate.h` | The reusable blueprint (symbol-agnostic) |
| `ConditionNode` | `include/strategy/ConditionNode.h` | Recursive AND/OR/Leaf condition tree |
| `Operand` | `include/strategy/ConditionNode.h` | One side of a comparison (8 types) |
| `IndicatorDefinition` | `include/strategy/StrategyTemplate.h` | Indicator slot with param placeholders |
| `TemplateParam` | `include/strategy/StrategyTemplate.h` | Deploy-time configurable value (incl. Expression) |
| `RiskDefaults` | `include/strategy/StrategyTemplate.h` | SL/Target/Trailing/TimeExit defaults |
| `SymbolBinding` | `include/strategy/StrategyTemplate.h` | Maps template slot → real token |
| `StrategyInstance` | `include/models/StrategyInstance.h` | Deployed instance with params in QVariantMap |
| `IndicatorMeta` | `include/ui/IndicatorCatalog.h` | Parsed indicator catalog entry |
| `FormulaASTNode` | `include/strategy/FormulaEngine.h` | Parsed expression tree node |

---

## 2. Strengths

### 2.1 Clean Separation of Concerns
- Template (blueprint) is completely **symbol-agnostic** — instruments are bound only at deploy time
- `FormulaEngine` is a **pure math evaluator** with a clean abstract `FormulaContext` interface
- `LiveFormulaContext` is the only bridge to real market data — easy to substitute for testing
- `IndicatorEngine` is self-contained: feed candles, get values

### 2.2 Rich Expression Language
- Full recursive-descent parser with proper precedence (ternary, logical, comparison, arithmetic, power, unary)
- Support for market data functions (`LTP()`, `RSI()`, `VWAP()`), greeks (`IV()`, `DELTA()`), and portfolio functions (`MTM()`, `NET_DELTA()`)
- Built-in math library (`ABS`, `MAX`, `MIN`, `CLAMP`, `SQRT`, `LOG`, etc.)
- Short-circuit evaluation for `&&` and `||`
- Formula validation with error messages (used in the UI for real-time feedback)
- AST introspection (`referencedParams()`, `referencedSymbols()`) for dependency analysis

### 2.3 Comprehensive Condition System
- 8 operand types: Price, Indicator, Constant, ParamRef, Formula, Greek, Spread, Total
- Recursive AND/OR tree with arbitrary nesting depth
- Crossover detection (`crosses_above`, `crosses_below`) with previous-tick tracking
- Clean JSON serialisation round-trip (template ↔ DB)

### 2.4 Solid UI Architecture
- `IndicatorCatalog` — data-driven indicator metadata from `indicator_defaults.json` (158 TA-Lib indicators)
- `IndicatorRowWidget` — card-based dynamic form that rebuilds param rows when indicator type changes
- `ConditionBuilderWidget` — tree-based condition editor with drag/drop support
- `LeafEditorDialog` — stacked-widget panels per operand type with live preview
- Real-time formula validation in the condition editor
- `refreshConditionContext()` keeps condition dropdowns in sync when symbols/indicators change

### 2.5 Robust Persistence
- SQLite-backed repository with UPSERT (ON CONFLICT DO UPDATE)
- Soft-delete for templates
- Proper JSON serialisation with backward-compat fallbacks (legacy key names, segment alias)
- Singleton pattern with auto-open on first access

### 2.6 Well-Documented Code
- Every file has architectural doc-comments explaining the flow
- `FormulaEngine.h` has a complete grammar specification
- `TemplateStrategy.h` has a step-by-step execution flow comment

---

## 3. Weaknesses & Design Gaps

### 3.1 Single-Entry / Single-Exit Model
**Impact: HIGH**

The `TemplateStrategy` only tracks a **single boolean** `m_hasPosition`. It processes the *first* trade symbol and breaks:

```cpp
// placeEntryOrder()
break; // one entry per signal for now

// placeExitOrder()
break;

// checkRiskLimits()
break; // only check first trade symbol for now
```

This makes multi-leg strategies (straddles, strangles, iron condors) **impossible to run** despite the template supporting multiple trade symbols. The `StrategyMode::OptionMultiLeg` enum exists but has no special handling.

### 3.2 No Order Confirmation / Fill Tracking
**Impact: HIGH**

`placeEntryOrder()` immediately sets `m_hasPosition = true` and records `m_entryPrice = state.ltp`. It doesn't wait for an order fill confirmation. In real markets:
- Orders can be rejected
- Fill price differs from LTP (slippage)
- Partial fills are common
- The `XTS::OrderParams` emitted via `orderRequested()` has no callback/feedback mechanism

### 3.3 No Re-entry Logic
**Impact: MEDIUM**

After an exit, `m_entrySignalFired = false` and `m_hasPosition = false`, so the strategy can re-enter on the very next tick. There's no:
- Cooldown period between trades
- "once-only" entry mode (fire-and-forget)
- Re-entry delay configuration

### 3.4 Naive Risk Management
**Impact: MEDIUM**

- **Stop-loss/target are percentage-based only** — no absolute price, ATR-based, or formula-based SL/TP
- **Trailing stop is defined but not implemented** in `checkRiskLimits()` — the fields `m_trailingEnabled`, `m_trailingTriggerPct`, `m_trailingAmountPct` are read during `init()` but never checked
- **Daily loss limit triggers `stop()`** which permanently stops the strategy — no "pause until next day" option

### 3.5 IndicatorEngine Re-computation on Every Candle
**Impact: LOW-MEDIUM**

`computeAll()` recomputes every configured indicator on every candle. For strategies with many indicators, this is inefficient. The `getPriceSeries()` call creates a new `QVector<double>` copy each time. The EMA implementation has an incremental path but the SMA, Bollinger, Stochastic, and ADX recompute from scratch on each candle.

### 3.6 No Candle History Pre-load
**Impact: MEDIUM**

The `IndicatorEngine` starts with zero candles. Indicators won't produce values until enough candles arrive. For a 200-period SMA on daily candles, that's ~200 trading days of waiting. There's no mechanism to pre-load historical candle data at strategy startup.

### 3.7 Tight Coupling to Tick Events
**Impact: LOW**

`onTick()` runs condition evaluation on *every* market tick. For high-frequency feeds (NSE: 10-20 ticks/sec per symbol, all symbols combined: hundreds/sec), this could be CPU-intensive. Entry/exit conditions are typically candle-based, so evaluating on every tick is wasteful for most strategies.

### 3.8 Formula Engine Has No Caching
**Impact: LOW**

Every `evaluate()` call tokenizes, parses, and evaluates from scratch. For expression parameters that are re-evaluated every tick (via `refreshExpressionParams()`), the AST could be cached.

### 3.9 Indicator ID Mismatch Between Engine and Context
**Impact: MEDIUM — POTENTIAL BUG**

The `LiveFormulaContext::indicator()` builds an indicator ID as `"TYPE_PERIOD"` (e.g. `"RSI_14"`), but the `IndicatorEngine` uses the `IndicatorConfig::id` (e.g. `"RSI_1"` from the template). When the `FormulaEngine` calls `RSI(REF_1, 14)`, the context looks for `"RSI_14"` but the engine has `"RSI_1"`. These don't match — **formula-based indicator lookups will always return 0.0**.

This is separate from the `resolveOperand(Operand::Type::Indicator)` path which uses `op.indicatorId` directly (which would be `"RSI_1"`).

### 3.10 No Versioning / Migration for Saved Templates
**Impact: LOW (for now)**

Templates are stored as a JSON blob. If the schema changes (new fields, renamed fields), old templates may silently load with missing data. The `version` field exists but is never checked during deserialization.

### 3.11 Validation is Minimal
**Impact: MEDIUM**

The `validate()` method only checks for a non-empty name and at least one symbol. It doesn't verify:
- Indicators reference valid symbol slots
- Condition operands reference valid indicator/symbol IDs
- Parameters referenced in `{{...}}` placeholders exist in the params list
- Entry and exit conditions are non-empty
- Risk defaults are reasonable (SL > 0, target > 0)

---

## 4. Potential Bugs & Runtime Issues

### 4.1 ⚠️ Indicator Type Mismatch (BB vs BBANDS)
The `IndicatorEngine::computeAll()` dispatches on `type.toUpper()`:
```cpp
if (type == "BB") computeBollingerBands(cfg);
```
But `FormulaEngine` and the catalog use `"BBANDS"`. The `IndicatorDefinition` stores the catalog type (`"BBANDS"`), which `setupIndicators()` passes to `IndicatorConfig::type`. The engine won't recognise `"BBANDS"` and will silently skip it.

**Same issue for:** `STOCH` (catalog) vs `STOCH` (engine matches), `BB` (engine) vs `BBANDS` (catalog). Cross-referencing the catalog and the engine's dispatch table:

| Catalog Type | Engine Dispatch | Match? |
|-------------|----------------|--------|
| `BBANDS` | `BB` | ❌ **Mismatch** |
| `STOCH` | `STOCH` | ✅ |
| `STOCHF` | — | ❌ **Not supported** |
| `SMA` | `SMA` | ✅ |
| `EMA` | `EMA` | ✅ |
| `RSI` | `RSI` | ✅ |
| `MACD` | `MACD` | ✅ |
| `ATR` | `ATR` | ✅ |
| `ADX` | `ADX` | ✅ |
| `OBV` | `OBV` | ✅ |
| `VOLUME` | `VOLUME` | ✅ |
| All other 148 types | — | ❌ **Not supported** |

The catalog advertises 158 indicators but the engine only computes 10. The other 148 will silently produce 0.0.

### 4.2 ⚠️ Crossover Detection on Tick (Not Candle)
The `crosses_above` / `crosses_below` operators track previous values per tick. On high-frequency tick data, the "crossing" can fire multiple times as price oscillates around a level — this produces **spurious signals**. Crossover should typically operate on candle close-to-close.

### 4.3 ⚠️ Empty Condition Tree Returns false
```cpp
bool TemplateStrategy::evaluateCondition(const ConditionNode &node) {
  if (node.isEmpty()) return false;
```
If the user doesn't define an exit condition, `evaluateCondition(m_template.exitCondition)` always returns `false` — the strategy will never exit on conditions (only on risk limits). This is arguably correct but should be documented. The entry condition having the same behaviour means a template with no entry condition will **never trade**.

### 4.4 ⚠️ `qFuzzyCompare` for `==` / `!=`
```cpp
if (node.op == "==") return qFuzzyCompare(leftVal, rightVal);
if (node.op == "!=") return !qFuzzyCompare(leftVal, rightVal);
```
`qFuzzyCompare` requires that **neither value is close to zero** (it uses relative comparison). Comparing `RSI < 0.001` to `0.0` will give wrong results. Should use an absolute epsilon check instead.

### 4.5 ⚠️ Spread Operand Assumes Depth Data Exists
```cpp
case Operand::Type::Spread: {
    ...
    return state.asks[0].price - state.bids[0].price;
}
```
If the order book is empty (pre-market, illiquid symbol), accessing `asks[0]` and `bids[0]` could access uninitialised/zero data, giving a meaningless spread of 0.

### 4.6 ⚠️ Formula Operand Errors Are Silent
```cpp
case Operand::Type::Formula: {
    double val = m_formulaEngine.evaluate(op.formulaExpression, &ok);
    if (!ok) { qWarning() << ...; }
    return val; // Returns 0.0 on error — could trigger false conditions
}
```
A formula error silently returns 0.0. If a condition is `ƒ(ATR(REF_1,14)*2.5) > 30`, and the formula fails, it becomes `0.0 > 30` = false. This is arguably safe for entry (no false triggers) but for exit conditions like `ƒ(expr) < threshold`, a 0.0 could trigger a **premature exit**.

### 4.7 ⚠️ CandleAggregator Connection Not Scoped
```cpp
connect(&agg, &CandleAggregator::candleComplete,
        this, &TemplateStrategy::onCandleComplete);
```
This connects to **all** candle events from the aggregator, not just the symbols this strategy cares about. The `onCandleComplete()` method filters by symbol name, but every candle for every symbol still triggers the slot. With 100+ strategies running, this is O(strategies × symbols × candle_events).

### 4.8 ⚠️ `m_exitInProgress` Guard Can Leak
If `placeExitOrder()` emits `orderRequested()` and the order is never confirmed, `m_exitInProgress` is reset to `false` at the end of `placeExitOrder()`. But if `placeExitOrder()` were to throw or fail before reaching the reset, the guard would permanently block further exits. Currently the code flow seems safe, but it's fragile.

### 4.9 ⚠️ param3 Handling Inconsistency
`IndicatorDefinition` has both `param3Str` (string) and `param3` (double). `setupIndicators()` doesn't read `param3` at all — it only maps `periodParam` → `cfg.period` and `period2Param` → `cfg.period2`. The third parameter (e.g., Bollinger stddev multiplier, MACD signal period) is **lost** during setup.

```cpp
// setupIndicators() — missing:
// cfg.period3 = ... (param3Str or param3)
// cfg.param1 = ...  (used by computeBollingerBands for stddev multiplier)
```

This means Bollinger Bands always use the default 2.0 stddev, and MACD always uses 9 for the signal period, regardless of what the user configured.

---

## 5. Real-World Viability Assessment

### 5.1 What Works Today (MVP Ready)
- ✅ **Simple single-leg indicator strategies**: RSI-based mean reversion, SMA crossover, ATR breakout
- ✅ **Template creation, save, load, edit** with a rich UI
- ✅ **Parameter customisation** at deploy time
- ✅ **Basic risk management** (percentage SL/TP, daily trade limit, time exit)
- ✅ **Formula-based parameters** for dynamic thresholds

### 5.2 What Doesn't Work Yet
- ❌ **Multi-leg options strategies** (straddle, iron condor) — single-entry model
- ❌ **Position sizing** — fixed quantity, no Kelly criterion, no % of capital
- ❌ **Order types** — hardcoded to MARKET orders, no LIMIT, no SL-M, no bracket orders
- ❌ **Trailing stop** — configured but not executed
- ❌ **Most TA-Lib indicators** — only 10 of 158 are computed
- ❌ **Backtesting** — no historical mode, no replay
- ❌ **Multi-timeframe analysis** — only one timeframe per indicator
- ❌ **Intraday candle warm-up** — indicators start cold, no historical pre-load
- ❌ **Strategy-wide MTM/Greek tracking** — `LiveFormulaContext.setMtm()` is never called by `TemplateStrategy`

### 5.3 Risk for Production Use
| Risk | Severity | Description |
|------|----------|-------------|
| False entry/exit on formula error | Medium | Silent 0.0 can trigger conditions |
| No fill confirmation | High | Position tracking assumes instant fill |
| Crossover on tick | Medium | Spurious signals on volatile ticks |
| BB/param3 loss | Medium | User-configured params silently ignored |
| No strategy isolation | Low | CandleAggregator signals broadcast to all strategies |

---

## 6. Understanding Gaps

Based on the code, these concepts are **modelled in the template** (UI, structs, serialisation) but **not wired in the runtime**:

| Feature | Template Support | Runtime Support |
|---------|:---:|:---:|
| Multiple trade symbols | ✅ Symbols tab | ❌ Only first processed |
| Trailing stop | ✅ Risk tab | ❌ Fields read, never checked |
| Strategy mode (OptionMultiLeg, Spread) | ✅ Metadata | ❌ No mode-specific logic |
| Entry side (Buy/Sell) | ✅ SymbolDefinition | ✅ placeEntryOrder |
| Expression params (`__expr__:`) | ✅ Param tab | ✅ refreshExpressionParams |
| Total/Portfolio operands | ✅ Condition builder | ⚠️ mtm/netPremium never set |
| Greek operands | ✅ Condition builder | ✅ Via UnifiedState |
| Spread operands | ✅ Condition builder | ⚠️ No depth validation |
| Locked parameters | ✅ Param tab | ❌ StrategyInstance.lockedParameters unused |
| Template version | ✅ Stored | ❌ Never checked |
| Min/Max param bounds | ✅ Param tab | ❌ Not enforced at deploy time |

---

## 7. Improvement Recommendations

### 7.1 Priority 1: Fix Bugs (< 1 week)

1. **Fix indicator type mismatch**: Normalise type strings in `IndicatorEngine::computeAll()` to accept both `"BB"` and `"BBANDS"`, or map catalog types to engine types in `setupIndicators()`.

2. **Pass param3 to IndicatorConfig**: In `setupIndicators()`, resolve `indDef.param3Str` → `cfg.period3` and `cfg.param1` (the stddev multiplier for BB).

3. **Fix `qFuzzyCompare` for zero values**: Replace with absolute epsilon comparison:
   ```cpp
   if (node.op == "==") return std::abs(leftVal - rightVal) < 1e-9;
   ```

4. **Fix LiveFormulaContext::indicator() ID construction**: Either build the ID as `"TYPE_PERIOD"` consistently, or look up by type+period in the engine instead of by string ID. Or maintain a secondary lookup map in the engine.

5. **Guard against empty depth arrays** in Spread operand resolution.

### 7.2 Priority 2: Complete Missing Runtime Features (1-2 weeks)

6. **Implement trailing stop**: In `checkRiskLimits()`, track `m_highWaterMark` and apply trailing logic when `m_trailingEnabled`.

7. **Multi-leg entry/exit**: Iterate all trade symbols instead of breaking after the first. Track per-symbol position state.

8. **Set portfolio MTM/Greeks**: After processing all trade symbols, compute aggregate MTM and call `m_formulaContext.setMtm(total)`.

9. **Enforce locked params and min/max bounds** in `StrategyDeployDialog`.

10. **Historical candle pre-load**: On `start()`, fetch historical candles from a data service and feed them to `IndicatorEngine` before subscribing to live data.

### 7.3 Priority 3: Architecture Improvements (2-4 weeks)

11. **Cache AST in FormulaEngine**: Add a `QHash<QString, ASTNodePtr> m_astCache` and parse only on first evaluation.

12. **Scoped CandleAggregator connection**: Use a filtering wrapper or subscribe with a unique key so each strategy only receives its own candles.

13. **Candle-based condition evaluation**: Add an `evaluateOnCandle` flag to conditions. When set, only evaluate when a new candle completes, not on every tick.

14. **Order management abstraction**: Replace `emit orderRequested()` with an `OrderManager` that tracks order lifecycle (placed → confirmed → filled/rejected → position updated).

15. **Expand IndicatorEngine**: Implement the full TA-Lib set via the actual TA-Lib C library (already included as `ta-lib-0.4.0-msvc.zip` in the project root).

16. **Add template schema versioning**: Check `version` during deserialization, run migration transforms if needed.

### 7.4 Priority 4: UX & Validation (1 week)

17. **Cross-reference validation** in `StrategyTemplateBuilderDialog::validate()`:
    - Verify all indicator `symbolId` fields reference existing symbol slots
    - Verify all condition operand references are valid
    - Verify all `{{...}}` parameter references exist in the params list
    - Warn if entry/exit conditions are empty

18. **Template cloning**: Add a "Duplicate" button in the template manager.

19. **Template export/import**: JSON file export for sharing templates between users.

20. **Dry-run / preview**: Show a human-readable summary of the template logic ("When RSI(NIFTY, 14) < 30 AND SMA(NIFTY, 20) crosses above SMA(NIFTY, 50), BUY …").

---

## 8. Ideal Framework Vision

An ideal strategy template builder for an Indian trading terminal should:

### 8.1 Data Model

```
StrategyTemplate
├── metadata (name, description, version, tags)
├── symbols[]
│   ├── role: Reference | Trade
│   ├── segment: NSE_CM | NSE_FO | BSE_CM | BSE_FO | MCX
│   ├── instrumentType: Equity | Future | Option | Index
│   └── selectionMode: Fixed | ATM+Offset | PremiumRange | StrikeRange
├── indicators[]
│   ├── computed from actual TA-Lib (not hand-rolled subset)
│   ├── multi-timeframe: { 1m: RSI(14), 1h: SMA(200) }
│   └── custom composite: "RSI(14) > 70 AND MACD(12,26,9).signal crosses_above 0"
├── parameters[] (with validation rules, range, step, expression support)
├── entryConditions: ConditionTree
│   ├── with cooldown / re-entry delay
│   ├── with "only on candle close" flag
│   └── with time window filter (9:20-15:00)
├── exitConditions: ConditionTree
├── riskRules
│   ├── stopLoss: Percentage | Absolute | ATR-multiple | Formula
│   ├── target: same
│   ├── trailingStop: { trigger, step, mode: percentage|absolute|ATR }
│   ├── maxLoss: { perTrade, perDay, perWeek }
│   ├── maxProfit: { perDay → optional stop-after-target }
│   ├── positionSizing: { mode: fixed|%capital|kelly, maxQty, maxExposure }
│   └── timeExit: { time, action: squareOff | convertToDelivery }
├── orderConfig
│   ├── entryType: MARKET | LIMIT | SL-M | Bracket
│   ├── exitType: MARKET | LIMIT
│   ├── slippage: { maxPts, maxPct }
│   └── legs[] (for multi-leg: each with own entry/exit order config)
└── backtestConfig (optional)
    ├── dateRange
    ├── initialCapital
    └── slippageModel
```

### 8.2 Runtime

```
TemplateStrategy
├── OrderManager (tracks order lifecycle, fills, partial fills)
├── PositionManager (multi-symbol, multi-leg position tracking)
│   ├── per-leg MTM, Greeks
│   └── aggregate portfolio Greeks
├── RiskManager
│   ├── Pre-trade checks (exposure limits, daily loss)
│   ├── In-trade checks (SL, TP, trailing, time)
│   └── Post-trade actions (cooldown, daily limit update)
├── IndicatorManager
│   ├── Historical warm-up on start
│   ├── Candle-scoped evaluation (not every tick)
│   └── Full TA-Lib integration
├── FormulaEngine (with AST caching)
├── EventLog (structured: timestamp, event type, values, action taken)
└── StrategyState (FSM: idle → waiting → entry_signal → order_placed →
                         position_open → exit_signal → exit_order_placed → idle)
```

### 8.3 Testing Infrastructure

- **Unit tests** for FormulaEngine (parse, evaluate, edge cases)
- **Unit tests** for IndicatorEngine (compare with known TA-Lib outputs)
- **Integration test** with mock FormulaContext
- **Backtest harness**: replay historical candles through TemplateStrategy, compare P&L

---

## 9. Priority Action Items

| # | Task | Effort | Impact | Priority |
|---|------|--------|--------|----------|
| 1 | Fix BBANDS type mismatch (BB→BBANDS) | 30 min | High | 🔴 P0 |
| 2 | Pass param3 through setupIndicators() | 30 min | High | 🔴 P0 |
| 3 | Fix qFuzzyCompare for zero values | 15 min | Medium | 🔴 P0 |
| 4 | Fix LiveFormulaContext indicator ID | 1 hr | Medium | 🔴 P0 |
| 5 | Guard Spread depth access | 15 min | Low | 🟡 P1 |
| 6 | Implement trailing stop | 2 hr | High | 🟡 P1 |
| 7 | Multi-leg entry/exit | 4 hr | High | 🟡 P1 |
| 8 | Set portfolio MTM in context | 1 hr | Medium | 🟡 P1 |
| 9 | Historical candle pre-load | 4 hr | High | 🟡 P1 |
| 10 | Cache FormulaEngine ASTs | 2 hr | Low | 🟢 P2 |
| 11 | Scoped CandleAggregator | 3 hr | Low | 🟢 P2 |
| 12 | Comprehensive validate() | 3 hr | Medium | 🟢 P2 |
| 13 | Full TA-Lib integration | 2 days | High | 🟢 P2 |
| 14 | Order management abstraction | 3 days | Critical | 🔵 P3 |
| 15 | Backtest harness | 1 week | High | 🔵 P3 |

---

## 10. Parameter System — Redesign (Implemented Feb 2026)

### Problem

The original parameter tab was a flat table with columns:  
`NAME | LABEL | TYPE | DEFAULT/FORMULA | MIN | MAX | LOCKED | DESCRIPTION`

When a user selected "Expression" type, the only option was to type a formula string in the Default column. There was:
- **No trigger selection** — user couldn't specify WHEN the formula recalculates
- **No schedule interval** — no way to set periodic recalculation
- **No candle timeframe** — no way to bind to a specific candle series
- **No formula syntax reference** — users had to remember available functions
- **No validation** — empty expressions were silently accepted

The backend (`TemplateStrategy.cpp`) had trigger infrastructure, but the UI never populated or exposed it.

### Solution — Full Trigger-Based Parameter System

#### Data Model (`StrategyTemplate.h`)

```
struct TemplateParam {
    QString     name, label, description;
    ParamValueType valueType;     // Int, Double, Bool, String, Expression
    QVariant    defaultValue;
    QVariant    minValue, maxValue;
    bool        locked;

    // ── Expression-only fields ──
    QString     expression;           // "ATR(REF_1, 14) * 2.5"
    ParamTrigger trigger;             // WHEN to recalculate
    int         scheduleIntervalSec;  // seconds (OnSchedule only)
    QString     triggerTimeframe;     // "5m", "1h" (OnCandleClose only)
};
```

#### Trigger Types (`ParamTrigger` enum)

| Trigger | When it fires | Use case |
|---------|---------------|----------|
| `EveryTick` | Every market price update | Dynamic SL/TP following price in real-time |
| `OnCandleClose` | When a candle completes (configurable TF) | ATR-based values that change per candle |
| `OnEntry` | Once when entry order is placed | Entry-price-relative SL/TP |
| `OnExit` | Once when exit order is placed | Exit-time calculations |
| `OnceAtStart` | Once when strategy starts | Session constants, VWAP anchor |
| `OnSchedule` | Fixed interval (N seconds) | Periodic IV recalculation |
| `Manual` | Never auto-recalculated | Deploy-time frozen values |

#### UI — Parameters Tab (Form-Based)

Parameters are now edited via a dedicated **ParamEditorDialog** modal form (not inline table editing).
The Parameters tab shows a **read-only summary table**: `NAME | LABEL | TYPE | VALUE/FORMULA | TRIGGER | LOCKED`.

**ParamEditorDialog** provides:
- **Identity section**: Name, Label, Type combo (Double/Int/Bool/String/Expression)
- **Fixed Value section** (non-Expression): Default, Min, Max
- **Formula section** (Expression): Multi-line formula editor with syntax help banner
- **Reference Palette** (Expression): Clickable insert chips organized by category:
  - 📊 Symbol market data: LTP, BID, ASK, HIGH, LOW, OPEN, CLOSE, VOLUME, CHANGE_PCT + Greeks per slot
  - 🔢 Other parameters: reference existing params by name (for pair-trading formulas)
  - 📈 Declared indicators: insert indicator IDs
  - 💰 Portfolio: MTM(), NET_PREMIUM(), NET_DELTA()
  - 🔣 Math & operators: ABS, MAX, MIN, ROUND, etc. + arithmetic/logic operators
- **Trigger section** (Expression): 7 recalculation modes with dynamic Timeframe/Interval
- **Details section**: Description, Locked checkbox
- Full validation: name format, bracket balance, schedule interval

**Workflow**:
1. Click "+ Add Parameter" → ParamEditorDialog opens in Add mode with context
2. Select type → form adapts (Fixed shows default/min/max, Expression shows formula + palette + trigger)
3. For pair-trading formulas like `LTP(REF_1) + LTP(TRADE_1)`, click the symbol chips to insert
4. Reference other parameters by name: `multiplier1 * DELTA(REF_1) + multiplier2 * DELTA(TRADE_1)`
5. Save → param added to internal list, summary table refreshed
6. Double-click a row → ParamEditorDialog opens in Edit mode, pre-filled

#### UI — Deploy Dialog

Expression params show:
- A QLineEdit with the formula (user can override with a plain number to freeze)
- A trigger badge label showing the trigger type (e.g. "🕯 On Candle Close (5m)")

#### Runtime Flow

```
Strategy starts
  → OnceAtStart params evaluated once
  → OnSchedule timers created (QTimer at N-second intervals)

On every tick:
  → EveryTick params re-evaluated
  → Conditions checked (entry/exit)

On candle complete:
  → OnCandleClose params re-evaluated (filtered by timeframe)

On entry order:
  → OnEntry params evaluated once (snapshot)

On exit order:
  → OnExit params evaluated once
```

### Files Changed

| File | Changes |
|------|---------|
| `resources/forms/StrategyTemplateBuilderDialog.ui` | Parameters tab simplified to read-only summary table + Add/Edit/Remove buttons |
| `include/ui/ParamEditorDialog.h` | New form dialog header with context-aware reference palette |
| `src/ui/ParamEditorDialog.cpp` | Full form UI, palette building, validation, insert-at-cursor |
| `src/ui/StrategyTemplateBuilderDialog.cpp` | `m_params` as source of truth, `refreshParamsTable()`, `onAddParam/onEditParam/onEditParamRow` open dialog with context, `extractParams()` returns `m_params` directly |
| `include/ui/StrategyTemplateBuilderDialog.h` | Added `m_params`, new slot/helper declarations |
| `CMakeLists.txt` | Added ParamEditorDialog.h and .cpp |
| `src/ui/StrategyDeployDialog.cpp` | Expression params show trigger badge + formula field |
| `include/strategy/StrategyTemplate.h` | `ParamTrigger` enum, new fields on `TemplateParam` |
| `include/strategies/TemplateStrategy.h` | Trigger maps, schedule timers |
| `src/strategies/TemplateStrategy.cpp` | Trigger-based `refreshExpressionParams()`, timer setup |
| `src/services/StrategyTemplateRepository.cpp` | Serialization of trigger/interval/timeframe |

---

## Summary

The Strategy Template Builder is a **well-architected MVP** with excellent design-time UX and a thoughtful data model. The core abstractions (FormulaEngine, ConditionNode tree, indicator catalog, deploy-time binding) are sound and extensible.

The primary gaps are in the **runtime execution layer**: single-leg execution, missing trailing stops, no order lifecycle tracking, and indicator type mismatches. These are all fixable without changing the template model or the UI — they're implementation gaps in `TemplateStrategy.cpp`, `IndicatorEngine.cpp`, and `LiveFormulaContext.cpp`.

The **parameter system** has been redesigned with a **form-based ParamEditorDialog** and a **context-aware reference palette**. Users can build pair-trading formulas like `LTP(REF_1) + LTP(TRADE_1)` or `multiplier1 * DELTA(REF_1) + multiplier2 * DELTA(TRADE_1)` by clicking clickable chips that insert function calls, parameter references, indicator IDs, and math operators at the cursor. The full trigger-based recalculation model gives users explicit control over WHEN each formula is evaluated — essential for production-grade algorithmic trading.

The **highest-ROI fixes** are items 1–4 (indicator bugs) which can be done in ~2 hours and will make the existing templates actually work correctly. After that, items 6–9 (trailing stop, multi-leg, candle warm-up) will make the system production-viable for real Indian market strategies.
