# MVP Implementation Plan: Strategy Builder Core

**Phase:** 1 (JSON-Based Engine)
**Goal:** Refactor existing Strategy classes to support the new, flexible JSON Schema defined in `04_STRATEGY_JSON_SCHEMA_MVP.md`.

---

## 1. Technical Architecture Changes

### A. Refactor `StrategyDefinition.h` (Generic Operands)
The current `Condition` struct relies on specific types like `PriceVsIndicator`. We will replace this with a generic `Operand` system to allow comparing any two values (Indicator vs Constant, Price vs Indicator, Indicator A vs Indicator B).

**New Structures:**

```cpp
// Represents a single value source
struct Operand {
    enum Type {
        Indicator,  // Value of a calculated indicator
        Price,      // OHLCV data (Close, High, etc.)
        Constant,   // Static number (e.g., 50.0)
        Parameter,  // User-defined parameter (e.g., $rsi_period)
        Variable    // Internal state (e.g., positions_count)
    };
    
    Type type = Constant;
    QString value;      // ID, field name, or numeric string
    int offset = 0;     // Lookback (0 = current, 1 = previous candle)
};

// Generic Comparison Condition
struct Condition {
    Operand left;
    QString operator_;  // >, <, ==, CROSSOVER
    Operand right;
};
```

### B. Update `StrategyParser.h` & `StrategyParser.cpp`
*   **Update `parseCondition`**: Logic to parse the new `left` and `right` JSON objects into `Operand` structs.
*   **Update `parseIndicatorConfig`**: Support a generic `QVariantMap inputs` instead of fixed fields (`period`, `period2`).

### C. Enhance `IndicatorEngine.h`
*   **Generic Configuration**: Accept `QVariantMap` for inputs to support varying parameters for different TA-Lib functions.
*   **TA-Lib Integration**: Implement wrappers for the identified "Classic" indicators (RSI, SMA, EMA, MACD, BBANDS, STOCH) using generic parameter extraction.

### D. Refactor `CustomStrategy.cpp` (The Core Engine)
*   **Simplify Evaluation**: Remove the big `switch(condition.type)` block. Replace with a `resolveOperand(Operand)` function that returns a `double`.
*   **`eval()` Logic**: `compare(resolve(left), op, resolve(right))`.

---

## 2. Implementation Roadmap

### Step 1: Definition Refactoring
1.  Target: `include/strategy/StrategyDefinition.h`
2.  Action: Replace old `Condition` struct with new `Operand` based struct.
3.  Action: Update `IndicatorConfig` to use `QVariantMap inputs`.

### Step 2: Parser Update
1.  Target: `src/strategy/StrategyParser.cpp`
2.  Action: Rewrite `parseCondition` to handle `{"type": "indicator", "id": "..."}` JSON objects.
3.  Action: Rewrite `parseIndicatorConfig` to generic map parsing.

### Step 3: Engine Refactoring
1.  Target: `src/strategies/CustomStrategy.cpp`
2.  Action: Implement `resolveOperand` helper.
3.  Action: Update `evaluateCondition` to use the helper.

### Step 4: Verification
1.  Create a strict **Unit Test Strategy JSON** covering all cases:
    *   Price > Constant
    *   Indicator > Constant
    *   Indicator CrossOver Indicator
    *   Parameter substitution
2.  Run the strategy in the existing `StrategyManager` or a test harness.

---

## 3. Verification Plan

### Automated Test: `TestJsonStrategy.cpp`
We will create a standalone test executable that:
1.  Instantiates `CustomStrategy`.
2.  Injects a mock `StrategyInstance` (with our JSON).
3.  Feeds synthesized `MarketTick` data (simulating a crossover).
4.  Asserts that `placeEntryOrder` is emitted.

```cpp
// Pseudocode for verification
void testStrategy() {
   CustomStrategy strategy;
   strategy.init(mockInstanceWithJson);
   strategy.start();
   
   // Tick 1: RSI = 40 (Below 50)
   strategy.onTick(createTick(100.0)); 
   
   // Tick 2: RSI = 60 (Above 50) -> Should Trigger BUY
   strategy.onTick(createTick(105.0));
   
   assert(strategy.hasActivePosition());
}
```

---

## 4. Dependencies & Risks
*   **Risk**: TA-Lib headers might be missing in some build environments.
*   **Mitigation**: Fallback to internal calculation if TA-Lib is not found (or ensure it's vendored). *Note: We confirmed TA-Lib is present in the codebase.*
*   **Dependency**: `qt_json` for parsing (Standard Qt feature).
