# Strategy Builder MVP: JSON Schema Definition

This document defines the **JSON Schema** for the "No-Code" Strategy Builder (Phase 1).
The goal is to provide a flexible, text-based format that our C++ engine can parse and execute.

---

## 1. High-Level Structure

The JSON file represents a single **Strategy Definition**.

```json
{
  "id": "uuid-string",
  "name": "My Custom Strategy",
  "version": "1.0",
  "author": "User",
  "description": "Short description of the logic.",
  
  "parameters": { ... },       // User-configurable inputs (Timeframe, Period, etc.)
  "indicators": [ ... ],       // List of technical indicators to calculate
  "entryLogic": { ... },       // Conditions to Enter Long/Short
  "exitLogic":  { ... },       // Conditions to Exit Long/Short
  "riskManagement": { ... }    // SL, TP, Trailing Logic
}
```

---

## 2. Core Components

### 2.1 Parameters `parameters`
Defines the inputs the user can tweak without changing the logic.

```json
"parameters": {
  "timeframe": { "type": "string", "default": "5min", "options": ["1min", "5min", "15min"] },
  "rsiPeriod": { "type": "int", "default": 14, "min": 2, "max": 100 },
  "overbought": { "type": "double", "default": 70.0 },
  "quantity": { "type": "int", "default": 50 }
}
```

### 2.2 Indicators `indicators`
Defines which calculations the engine must perform on every new candle/tick.
*   **RefID:** A unique internal name (e.g., `rsi_14`) used in logic conditions.
*   **Type:** The indicator name (mapped to C++ classes or TA-Lib).
*   **Inputs:** Arguments for the indicator. Can be static values or references to `parameters`.

```json
"indicators": [
  {
    "id": "fast_ema",
    "type": "EMA",
    "inputs": { "period": 9, "source": "close" }
  },
  {
    "id": "slow_ema",
    "type": "EMA",
    "inputs": { "period": 21, "source": "close" }
  },
  {
    "id": "rsi_main",
    "type": "RSI",
    "inputs": { 
        "period": "$rsiPeriod", // Reference to user parameter
        "source": "close" 
    }
  }
]
```

### 2.3 Logic Tree `entryLogic` / `exitLogic`
Recursive structure supporting `AND` / `OR` groups.

*   **Condition:** Compares `Left Operand` vs `Right Operand` using an `Operator`.
*   **Operand Types:**
    *   `indicator`: Value of a calculated indicator (e.g., `rate(fast_ema)`).
    *   `constant`: Static number (e.g., `50`).
    *   `series`: OHLCV data (e.g., `close`, `high`).
    *   `parameter`: User parameter (e.g., `$overbought`).

```json
"entryLogic": {
  "type": "AND", // Root group type
  "conditions": [
    {
      "type": "CROSSOVER",
      "left": { "type": "indicator", "id": "fast_ema" },
      "right": { "type": "indicator", "id": "slow_ema" }
    },
    {
      "type": "GREATER_THAN",
      "left": { "type": "indicator", "id": "rsi_main" },
      "right": { "type": "constant",  "value": 50 }
    }
  ]
}
```

### 2.4 Risk Management `riskManagement`

```json
"riskManagement": {
  "stopLoss": { 
      "type": "PERCENT", // or "POINTS", "ATR"
      "value": 1.0 
  },
  "target": { 
      "type": "RISK_REWARD", 
      "ratio": 2.0 
  },
  "trailingStop": {
      "active": true,
      "activation": 0.5, // Start trailing after 0.5% profit
      "callback": 0.2    // Trail distance
  }
}
```

---

## 3. Full Example: "Supertrend + RSI Strategy"

**Logic:**
*   **Buy** when Close crosses above Supertrend **AND** RSI > 50.
*   **Sell** when Close crosses below Supertrend **OR** RSI < 30.

```json
{
  "id": "st_rsi_v1",
  "name": "Supertrend RSI Combo",
  
  "parameters": {
    "st_period": { "type": "int", "default": 10 },
    "st_multiplier": { "type": "double", "default": 3.0 },
    "rsi_len": { "type": "int", "default": 14 }
  },

  "indicators": [
    {
      "id": "st_1",
      "type": "SUPERTREND",
      "inputs": { "period": "$st_period", "multiplier": "$st_multiplier" }
    },
    {
      "id": "rsi_1",
      "type": "RSI",
      "inputs": { "period": "$rsi_len" }
    }
  ],

  "entryLogic": {
    "type": "AND",
    "conditions": [
      {
        "type": "CROSSOVER",
        "left": { "type": "series", "id": "close" },
        "right": { "type": "indicator", "id": "st_1" }
      },
      {
        "type": "GREATER_THAN",
        "left": { "type": "indicator", "id": "rsi_1" },
        "right": { "type": "constant", "value": 50 }
      }
    ]
  },

  "exitLogic": {
    "type": "OR",
    "conditions": [
      {
        "type": "CROSSUNDER",
        "left": { "type": "series", "id": "close" },
        "right": { "type": "indicator", "id": "st_1" }
      },
      {
        "type": "LESS_THAN",
        "left": { "type": "indicator", "id": "rsi_1" },
        "right": { "type": "constant", "value": 30 }
      }
    ]
  },
  
  "riskManagement": {
      "stopLoss": { "type": "ATR", "period": 14, "multiplier": 2.0 },
      "target": { "type": "PERCENT", "value": 3.0 }
  }
}
```

---

## 4. Next Steps for Implementation

1.  **Parser:** Create a `StrategyParser` class in C++ using `QJsonDocument`.
2.  **Factory:** Extend `IndicatorFactory` to instantiate indicators based on the JSON `type` string.
3.  **Engine:** Update `CustomStrategy::onTick()` to:
    *   Update all `indicators`.
    *   Evaluate `entryLogic` recursively.
    *   Evaluate `exitLogic` recursively.
    *   Manage `riskManagement` state.
