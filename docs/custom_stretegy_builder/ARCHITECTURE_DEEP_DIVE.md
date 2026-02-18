# Strategy Builder Architecture: Deep Dive
**Understanding Template vs Instance Design Pattern**

**Date:** February 18, 2026  
**Author:** System Architecture Analysis  
**Status:** 🔴 Critical Design Gap Identified

---

## 📚 Table of Contents

1. [Executive Summary](#executive-summary)
2. [The Fundamental Problem](#the-fundamental-problem)
3. [Current Architecture (Instance-Centric)](#current-architecture-instance-centric)
4. [Expected Architecture (Template-Centric)](#expected-architecture-template-centric)
5. [Design Pattern: Template Method vs Strategy Pattern](#design-pattern-comparison)
6. [Variable Substitution Engine](#variable-substitution-engine)
7. [Database Schema Evolution](#database-schema-evolution)
8. [Code Architecture Changes](#code-architecture-changes)
9. [Migration Path](#migration-path)
10. [Real-World Examples](#real-world-examples)
11. [Advanced Concepts](#advanced-concepts)
12. [Implementation Roadmap](#implementation-roadmap)

---

## Executive Summary

### 🎯 The Core Issue

**Current Implementation:**
```
User creates "NIFTY ATM Straddle" strategy
↓
Strategy is LOCKED to NIFTY symbol
↓
Cannot reuse for BANKNIFTY / FINNIFTY
↓
Must recreate entire strategy from scratch
```

**Expected Behavior:**
```
User creates "ATM Straddle" template
↓
Template is SYMBOL-AGNOSTIC
↓
Deploy template for NIFTY, BANKNIFTY, FINNIFTY
↓
One strategy logic → Multiple deployments
```

### 📊 Impact Analysis

| Aspect | Current | Expected | Gap Severity |
|--------|---------|----------|--------------|
| **Reusability** | 0% (locked to symbol) | 100% (parameterized) | 🔴 Critical |
| **Strategy Sharing** | Not possible | Full marketplace | 🔴 Critical |
| **Multi-Instrument** | Rebuild each time | Deploy once | 🟡 High |
| **Team Collaboration** | No templates | Template library | 🟡 High |
| **Backtesting** | Re-configure per symbol | Use template | 🟢 Medium |
| **Version Control** | No versioning | Template versions | 🟢 Medium |

---

## The Fundamental Problem

### 🧠 Conceptual Gap: "Strategy" vs "Strategy Instance"

In software architecture, we distinguish between:

1. **Strategy (Class/Template)** - The reusable blueprint
2. **Strategy Instance (Object)** - A specific deployment

**Analogy:**
```
Class Car {
  constructor(color, owner) {
    this.color = color;
    this.owner = owner;
  }
}

// Template: Car blueprint
// Instance: new Car("Red", "John")  ← Specific car
// Instance: new Car("Blue", "Sarah") ← Another car
```

**In Our Context:**
```
Template: "ATM Straddle on High IV"
  - Entry: IV > {{IV_THRESHOLD}}
  - Legs: SELL CE ATM, SELL PE ATM
  - Exit: 50% profit or 2x loss

Instance 1: Deploy on NIFTY, Account=RAJ143, Qty=50
Instance 2: Deploy on BANKNIFTY, Account=RAJ143, Qty=25
Instance 3: Deploy on FINNIFTY, Account=AMIT789, Qty=75
```

### 🔍 What Went Wrong?

The current implementation **conflates template and instance**:

```cpp
// StrategyBuilderDialog currently mixes both concerns
class StrategyBuilderDialog {
  // ❌ Template-level data (should be abstract)
  QLineEdit *m_nameEdit;        // "ATM Straddle"
  QVector<IndicatorRow> m_indicators;
  QVector<LegRow> m_legs;
  QVector<ConditionRow> m_conditions;
  
  // ❌ Instance-level data (should be separate)
  QLineEdit *m_symbolEdit;      // "NIFTY" ← LOCKS THE TEMPLATE
  QLineEdit *m_accountEdit;     // "RAJ143" ← LOCKS THE TEMPLATE
  QSpinBox *m_positionSizeSpin; // 50 ← LOCKS THE TEMPLATE
  
  // Result: Creates ONE deployment, not a reusable template
};
```

**Why This Architecture Fails:**

1. **No Separation of Concerns**
   - Template logic mixed with deployment parameters
   - Cannot save strategy without specifying symbol/account
   - Cannot deploy same logic to different instruments

2. **Database Design Flaw**
   ```sql
   -- Current: Only one table for everything
   CREATE TABLE strategy_instances (
     instance_id INTEGER,
     strategy_type TEXT,      -- "Custom"
     symbol TEXT,             -- ❌ LOCKED at creation
     account TEXT,            -- ❌ LOCKED at creation
     parameters_json TEXT     -- ❌ Definition + deployment mixed
   );
   ```

3. **UI Flow Broken**
   ```
   Current:
   Build Strategy → Fill ALL fields → Create Instance
                                     ↓
                             Cannot reuse ❌
   
   Expected:
   Build Template → Save abstract strategy
        ↓
   Deploy Template → Fill deployment params → Create Instance
                                             ↓
                                    Can deploy 100x ✅
   ```

---

## Current Architecture (Instance-Centric)

### 📐 Class Diagram

```
┌─────────────────────────────────────────────────────────┐
│              StrategyBuilderDialog                      │
│─────────────────────────────────────────────────────────│
│ + m_nameEdit: QLineEdit                                 │
│ + m_symbolEdit: QLineEdit              ← INSTANCE DATA │
│ + m_accountEdit: QLineEdit             ← INSTANCE DATA │
│ + m_segmentCombo: QComboBox            ← INSTANCE DATA │
│ + m_positionSizeSpin: QSpinBox         ← INSTANCE DATA │
│ + m_indicators: QVector<IndicatorRow>  ← TEMPLATE DATA │
│ + m_legs: QVector<LegRow>              ← TEMPLATE DATA │
│ + m_conditions: QVector<ConditionRow>  ← TEMPLATE DATA │
│─────────────────────────────────────────────────────────│
│ + buildJSON(): QJsonObject                              │
│   → Returns COMPLETE instance (symbol + logic mixed)    │
└─────────────────────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────────────────────┐
│              StrategyService                            │
│─────────────────────────────────────────────────────────│
│ + createInstance(name, symbol, account, ...)           │
│   ↓                                                     │
│   Saves to DB with ALL fields populated                │
│   No concept of "template" vs "instance"               │
└─────────────────────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────────────────────┐
│              StrategyInstance (DB Record)               │
│─────────────────────────────────────────────────────────│
│ instance_id: 1                                          │
│ instance_name: "NIFTY_Straddle_Feb18"                  │
│ strategy_type: "Custom"                                 │
│ symbol: "NIFTY"                        ← LOCKED         │
│ account: "RAJ143"                      ← LOCKED         │
│ segment: 2                             ← LOCKED         │
│ quantity: 50                           ← LOCKED         │
│ parameters_json: '{                                     │
│   "definition": {                                       │
│     "symbol": "NIFTY",                 ← DUPLICATED     │
│     "indicators": [...],                                │
│     "legs": [...]                                       │
│   }                                                     │
│ }'                                                      │
└─────────────────────────────────────────────────────────┘
```

### 🔴 Problems with This Design

#### 1. **Duplicated Symbol Data**
```json
// DB Record
{
  "symbol": "NIFTY",           // ← Field 1
  "parameters_json": {
    "definition": {
      "symbol": "NIFTY"        // ← Field 2 (duplicate)
    }
  }
}
```

#### 2. **Cannot Extract Template**
```cpp
// If user wants to reuse "NIFTY_Straddle" for BANKNIFTY:
// ❌ Must manually:
// 1. Copy entire JSON
// 2. Find-replace "NIFTY" → "BANKNIFTY" in 10+ places
// 3. Recalculate quantities for different lot sizes
// 4. Create new instance from scratch
```

#### 3. **No Template Library**
```cpp
// ❌ Cannot do this:
QVector<StrategyTemplate> templates = TemplateService::listTemplates();
for (auto &tmpl : templates) {
  if (tmpl.name == "ATM Straddle") {
    // Deploy to multiple symbols
    deployTemplate(tmpl, "NIFTY", "RAJ143");
    deployTemplate(tmpl, "BANKNIFTY", "RAJ143");
  }
}
```

#### 4. **Zero Strategy Marketplace Potential**
```
Alice creates "Iron Condor" strategy
↓
❌ Cannot share with Bob (locked to Alice's account/symbol)
↓
Bob must recreate from scratch
```

---

## Expected Architecture (Template-Centric)

### 📐 Proper Class Diagram

```
┌─────────────────────────────────────────────────────────┐
│          StrategyTemplateBuilder                        │
│─────────────────────────────────────────────────────────│
│ + m_templateNameEdit: QLineEdit                         │
│ + m_descriptionEdit: QTextEdit                          │
│ + m_indicators: QVector<IndicatorRow>  ← TEMPLATE LOGIC│
│ + m_legs: QVector<LegRow>              ← TEMPLATE LOGIC│
│ + m_conditions: QVector<ConditionRow>  ← TEMPLATE LOGIC│
│ + m_parameters: QVector<TemplateParam> ← VARIABLES     │
│─────────────────────────────────────────────────────────│
│ + buildTemplateJSON(): QJsonObject                      │
│   → Returns ABSTRACT template ({{SYMBOL}} placeholders)│
└─────────────────────────────────────────────────────────┘
            ↓ saves to
┌─────────────────────────────────────────────────────────┐
│          StrategyTemplate (DB Record)                   │
│─────────────────────────────────────────────────────────│
│ template_id: "uuid-abc123"                              │
│ template_name: "ATM Straddle"                           │
│ description: "Sell ATM straddle on high IV"             │
│ definition_json: '{                                     │
│   "parameters": {                                       │
│     "symbol": "{{SYMBOL}}",          ← VARIABLE         │
│     "quantity": "{{QUANTITY}}",      ← VARIABLE         │
│     "iv_threshold": "{{IV_THRESH}}"  ← VARIABLE         │
│   },                                                    │
│   "legs": [                                             │
│     { "symbol": "{{SYMBOL}}", ... }                     │
│   ]                                                     │
│ }'                                                      │
│ created_at: "2026-02-18"                                │
│ is_public: true                      ← SHAREABLE        │
└─────────────────────────────────────────────────────────┘
            ↓ deploy via
┌─────────────────────────────────────────────────────────┐
│          DeployTemplateDialog                           │
│─────────────────────────────────────────────────────────│
│ + m_templateCombo: QComboBox  → "ATM Straddle"         │
│ + m_symbolEdit: QLineEdit     → "NIFTY"                │
│ + m_accountEdit: QLineEdit    → "RAJ143"               │
│ + m_quantitySpin: QSpinBox    → 50                     │
│ + m_ivThresholdSpin: QSpinBox → 20                     │
│─────────────────────────────────────────────────────────│
│ + deploy(): void                                        │
│   ↓                                                     │
│   1. Load template from DB                              │
│   2. Substitute {{SYMBOL}} → "NIFTY"                   │
│   3. Substitute {{QUANTITY}} → 50                      │
│   4. Create StrategyInstance with concrete values       │
└─────────────────────────────────────────────────────────┘
            ↓ creates
┌─────────────────────────────────────────────────────────┐
│          StrategyInstance (DB Record)                   │
│─────────────────────────────────────────────────────────│
│ instance_id: 1                                          │
│ template_id: "uuid-abc123"            ← REFERENCES      │
│ instance_name: "NIFTY_Straddle_1"                      │
│ deployed_params: '{                                     │
│   "symbol": "NIFTY",                 ← CONCRETE         │
│   "account": "RAJ143",               ← CONCRETE         │
│   "quantity": 50,                    ← CONCRETE         │
│   "iv_threshold": 20                 ← CONCRETE         │
│ }'                                                      │
│ state: "RUNNING"                                        │
│ mtm: -1250.50                                           │
└─────────────────────────────────────────────────────────┘
```

### ✅ Benefits of This Design

#### 1. **True Reusability**
```cpp
// Load template once
StrategyTemplate tmpl = TemplateService::load("ATM_Straddle");

// Deploy many times
TemplateService::deploy(tmpl, {{"SYMBOL", "NIFTY"}, {"QTY", 50}});
TemplateService::deploy(tmpl, {{"SYMBOL", "BANKNIFTY"}, {"QTY", 25}});
TemplateService::deploy(tmpl, {{"SYMBOL", "FINNIFTY"}, {"QTY", 75}});
```

#### 2. **Template Marketplace**
```cpp
// Alice shares template
TemplateService::publish("ATM_Straddle", "alice@trading.com");

// Bob discovers and uses
QVector<StrategyTemplate> public = TemplateService::publicTemplates();
StrategyTemplate alices = public.find("ATM_Straddle");
TemplateService::deploy(alices, {{"SYMBOL", "NIFTY"}});
```

#### 3. **Version Control**
```json
{
  "template_id": "uuid-abc123",
  "version": "2.0",
  "changelog": "Added trailing stop support",
  "previous_version": "uuid-xyz789"
}
```

#### 4. **Parameter Validation**
```cpp
// Template declares required parameters
StrategyTemplate tmpl;
tmpl.requiredParams = {"SYMBOL", "QUANTITY", "IV_THRESHOLD"};

// Deploy dialog validates
DeployTemplateDialog dlg;
dlg.setTemplate(tmpl);
// Auto-generates input fields for each required param
```

---

## Design Pattern Comparison

### 🎨 Pattern 1: Template Method Pattern (Current - Wrong)

```cpp
// Current implementation follows Template Method anti-pattern
class StrategyBuilderDialog {
  // Mixes algorithm (template) with data (instance)
  QJsonObject buildJSON() {
    QJsonObject json;
    json["symbol"] = m_symbolEdit->text();  // ❌ Instance data
    json["legs"] = buildLegs();             // ✅ Template logic
    return json;
  }
};
```

**Problem:** Cannot separate "how to build strategy" from "what to trade"

### 🎨 Pattern 2: Strategy Pattern (Expected - Correct)

```cpp
// Strategy Pattern: Separate strategy definition from execution

// 1. Strategy Interface
class StrategyTemplate {
public:
  virtual QJsonObject getDefinition() const = 0;
  virtual QStringList requiredParameters() const = 0;
};

// 2. Concrete Strategy (Template)
class ATMStraddleTemplate : public StrategyTemplate {
public:
  QJsonObject getDefinition() const override {
    return {
      {"legs", buildLegs()},
      {"conditions", buildConditions()},
      {"parameters", {
        {"symbol", "{{SYMBOL}}"},     // ← Variable
        {"quantity", "{{QUANTITY}}"}  // ← Variable
      }}
    };
  }
  
  QStringList requiredParameters() const override {
    return {"SYMBOL", "QUANTITY", "IV_THRESHOLD"};
  }
};

// 3. Strategy Executor (Instance)
class StrategyInstance {
  StrategyTemplate *m_template;
  QVariantMap m_deployedParams;
  
public:
  void execute() {
    QJsonObject def = m_template->getDefinition();
    QJsonObject concrete = substituteVariables(def, m_deployedParams);
    // Run strategy with concrete values
  }
};
```

**Benefit:** Clear separation between strategy logic and deployment data

### 🎨 Pattern 3: Builder Pattern (How to Construct)

```cpp
// Builder for creating templates
class StrategyTemplateBuilder {
public:
  StrategyTemplateBuilder& setName(const QString &name) {
    m_template.name = name;
    return *this;
  }
  
  StrategyTemplateBuilder& addParameter(const QString &name, const QString &defaultValue) {
    m_template.parameters[name] = defaultValue;
    return *this;
  }
  
  StrategyTemplateBuilder& addLeg(const LegDefinition &leg) {
    m_template.legs.append(leg);
    return *this;
  }
  
  StrategyTemplate build() {
    validate();
    return m_template;
  }
  
private:
  StrategyTemplate m_template;
};

// Usage:
StrategyTemplate tmpl = StrategyTemplateBuilder()
  .setName("ATM Straddle")
  .addParameter("SYMBOL", "")
  .addParameter("QUANTITY", "50")
  .addLeg({{"side", "SELL"}, {"type", "CE"}})
  .addLeg({{"side", "SELL"}, {"type", "PE"}})
  .build();
```

---

## Variable Substitution Engine

### 🔧 Core Concept

Replace placeholder variables in template with concrete values at deployment time.

**Template:**
```json
{
  "symbol": "{{SYMBOL}}",
  "quantity": "{{QUANTITY}}",
  "legs": [
    {
      "id": "LEG_1",
      "symbol": "{{SYMBOL}}",
      "quantity": "{{QUANTITY}}"
    }
  ],
  "conditions": [
    {
      "type": "IV",
      "operator": ">",
      "value": "{{IV_THRESHOLD}}"
    }
  ]
}
```

**Deployment Parameters:**
```cpp
QVariantMap params = {
  {"SYMBOL", "NIFTY"},
  {"QUANTITY", 50},
  {"IV_THRESHOLD", 20}
};
```

**Result (After Substitution):**
```json
{
  "symbol": "NIFTY",
  "quantity": 50,
  "legs": [
    {
      "id": "LEG_1",
      "symbol": "NIFTY",
      "quantity": 50
    }
  ],
  "conditions": [
    {
      "type": "IV",
      "operator": ">",
      "value": 20
    }
  ]
}
```

### 🛠️ Implementation

```cpp
class TemplateEngine {
public:
  /**
   * @brief Substitute all {{VAR}} in JSON with concrete values
   * @param templateJson Raw JSON string with {{VARIABLES}}
   * @param params Map of variable names to values
   * @return Substituted JSON string
   */
  static QString substituteVariables(
    const QString &templateJson,
    const QVariantMap &params
  ) {
    QString result = templateJson;
    
    // Replace each variable
    for (auto it = params.begin(); it != params.end(); ++it) {
      QString varName = QString("{{%1}}").arg(it.key());
      QString varValue = it.value().toString();
      result.replace(varName, varValue);
    }
    
    return result;
  }
  
  /**
   * @brief Extract all {{VAR}} from template
   * @param templateJson Template JSON string
   * @return List of variable names (without {{ }})
   */
  static QStringList extractVariables(const QString &templateJson) {
    QStringList vars;
    QRegularExpression re("\\{\\{([A-Z_]+)\\}\\}");
    QRegularExpressionMatchIterator it = re.globalMatch(templateJson);
    
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      QString varName = match.captured(1);
      if (!vars.contains(varName)) {
        vars.append(varName);
      }
    }
    
    return vars;
  }
  
  /**
   * @brief Validate all variables have values
   * @param templateJson Template JSON
   * @param params Deployment parameters
   * @param error Output error message
   * @return true if valid
   */
  static bool validateSubstitution(
    const QString &templateJson,
    const QVariantMap &params,
    QString &error
  ) {
    QStringList requiredVars = extractVariables(templateJson);
    QStringList missingVars;
    
    for (const QString &var : requiredVars) {
      if (!params.contains(var) || params[var].toString().isEmpty()) {
        missingVars.append(var);
      }
    }
    
    if (!missingVars.isEmpty()) {
      error = QString("Missing required parameters: %1")
        .arg(missingVars.join(", "));
      return false;
    }
    
    return true;
  }
  
  /**
   * @brief Smart substitution with type conversion
   * @param templateJson Template with variables
   * @param params Deployment parameters
   * @return QJsonObject with substituted values
   */
  static QJsonObject substituteJSON(
    const QJsonObject &templateObj,
    const QVariantMap &params
  ) {
    QJsonObject result;
    
    for (auto it = templateObj.begin(); it != templateObj.end(); ++it) {
      QString key = it.key();
      QJsonValue value = it.value();
      
      if (value.isString()) {
        QString str = value.toString();
        if (str.startsWith("{{") && str.endsWith("}}")) {
          // Extract variable name
          QString varName = str.mid(2, str.length() - 4);
          if (params.contains(varName)) {
            // Replace with concrete value
            result[key] = QJsonValue::fromVariant(params[varName]);
          } else {
            result[key] = value; // Keep placeholder
          }
        } else {
          result[key] = value;
        }
      } else if (value.isArray()) {
        // Recursively substitute arrays
        result[key] = substituteArray(value.toArray(), params);
      } else if (value.isObject()) {
        // Recursively substitute objects
        result[key] = substituteJSON(value.toObject(), params);
      } else {
        result[key] = value;
      }
    }
    
    return result;
  }
  
private:
  static QJsonArray substituteArray(
    const QJsonArray &arr,
    const QVariantMap &params
  ) {
    QJsonArray result;
    for (const QJsonValue &val : arr) {
      if (val.isObject()) {
        result.append(substituteJSON(val.toObject(), params));
      } else if (val.isString()) {
        QString str = val.toString();
        if (str.startsWith("{{") && str.endsWith("}}")) {
          QString varName = str.mid(2, str.length() - 4);
          if (params.contains(varName)) {
            result.append(QJsonValue::fromVariant(params[varName]));
          } else {
            result.append(val);
          }
        } else {
          result.append(val);
        }
      } else {
        result.append(val);
      }
    }
    return result;
  }
};
```

### 📝 Usage Example

```cpp
// 1. Load template from DB
StrategyTemplate tmpl = TemplateService::load("ATM_Straddle");
QString templateJson = tmpl.definitionJson;

// 2. Extract required parameters
QStringList required = TemplateEngine::extractVariables(templateJson);
// Returns: ["SYMBOL", "QUANTITY", "IV_THRESHOLD"]

// 3. Get deployment params from user
QVariantMap params = DeployDialog::getParams(required);
// Returns: {{"SYMBOL", "NIFTY"}, {"QUANTITY", 50}, {"IV_THRESHOLD", 20}}

// 4. Validate
QString error;
if (!TemplateEngine::validateSubstitution(templateJson, params, error)) {
  QMessageBox::critical(nullptr, "Validation Failed", error);
  return;
}

// 5. Substitute
QString concreteJson = TemplateEngine::substituteVariables(templateJson, params);

// 6. Create instance
StrategyService::createInstance(
  "NIFTY_Straddle_1",
  "Custom",
  params["SYMBOL"].toString(),
  params
);
```

---

## Database Schema Evolution

### 📊 Current Schema (Flawed)

```sql
CREATE TABLE strategy_instances (
  instance_id INTEGER PRIMARY KEY AUTOINCREMENT,
  instance_name TEXT NOT NULL,
  strategy_type TEXT NOT NULL,
  symbol TEXT NOT NULL,             -- ❌ Locked per instance
  account TEXT NOT NULL,            -- ❌ Locked per instance
  segment INTEGER NOT NULL,         -- ❌ Locked per instance
  description TEXT,
  state TEXT DEFAULT 'Idle',
  mtm REAL DEFAULT 0.0,
  stop_loss REAL DEFAULT 0.0,
  target REAL DEFAULT 0.0,
  entry_price REAL DEFAULT 0.0,
  quantity INTEGER DEFAULT 0,       -- ❌ Locked per instance
  active_positions INTEGER DEFAULT 0,
  pending_orders INTEGER DEFAULT 0,
  parameters_json TEXT,             -- ❌ Mixes template + instance
  created_at TEXT,
  last_updated TEXT,
  last_state_change TEXT,
  start_time TEXT,
  last_error TEXT
);
```

**Problems:**
1. No template concept
2. Cannot share strategies
3. Deployment params mixed with logic
4. No version control

### 📊 Improved Schema (Two Tables)

```sql
-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
-- TABLE 1: Strategy Templates (Reusable Blueprints)
-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
CREATE TABLE strategy_templates (
  -- Identity
  template_id TEXT PRIMARY KEY,           -- UUID
  template_name TEXT NOT NULL UNIQUE,
  description TEXT,
  author TEXT,                            -- User who created it
  
  -- Template definition (ABSTRACT - with {{VARS}})
  definition_json TEXT NOT NULL,          -- JSON with placeholders
  /*
    {
      "parameters": {
        "symbol": "{{SYMBOL}}",
        "quantity": "{{QUANTITY}}",
        "iv_threshold": "{{IV_THRESH}}"
      },
      "indicators": [...],
      "legs": [
        { "symbol": "{{SYMBOL}}", ... }
      ],
      "conditions": [...]
    }
  */
  
  -- Required parameters for deployment
  required_params TEXT NOT NULL,          -- JSON array: ["SYMBOL", "QUANTITY", "IV_THRESH"]
  default_params TEXT,                    -- JSON object: {"IV_THRESH": 20}
  
  -- Categorization
  category TEXT,                          -- "Options", "Momentum", "Reversal"
  tags TEXT,                              -- JSON array: ["Short Straddle", "IV-based"]
  
  -- Versioning
  version TEXT DEFAULT '1.0',
  parent_template_id TEXT,                -- For versioning
  
  -- Sharing
  is_public BOOLEAN DEFAULT 0,            -- Can others see it?
  is_verified BOOLEAN DEFAULT 0,          -- Reviewed by admin?
  
  -- Performance tracking
  total_deployments INTEGER DEFAULT 0,
  avg_pnl REAL DEFAULT 0.0,
  success_rate REAL DEFAULT 0.0,
  
  -- Metadata
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  
  FOREIGN KEY (parent_template_id) REFERENCES strategy_templates(template_id)
);

-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
-- TABLE 2: Strategy Instances (Concrete Deployments)
-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
CREATE TABLE strategy_instances (
  -- Identity
  instance_id INTEGER PRIMARY KEY AUTOINCREMENT,
  instance_name TEXT NOT NULL,
  
  -- Link to template
  template_id TEXT,                       -- References strategy_templates
  strategy_type TEXT NOT NULL,            -- "Custom", "Momentum", etc.
  
  -- Deployment parameters (CONCRETE)
  deployed_params TEXT NOT NULL,          -- JSON: {"SYMBOL": "NIFTY", "QUANTITY": 50}
  symbol TEXT NOT NULL,                   -- Denormalized for queries
  account TEXT NOT NULL,
  segment INTEGER NOT NULL,
  quantity INTEGER DEFAULT 0,
  
  -- Risk parameters
  stop_loss REAL DEFAULT 0.0,
  target REAL DEFAULT 0.0,
  entry_price REAL DEFAULT 0.0,
  
  -- Runtime state
  state TEXT DEFAULT 'Idle',
  mtm REAL DEFAULT 0.0,
  active_positions INTEGER DEFAULT 0,
  pending_orders INTEGER DEFAULT 0,
  
  -- Metadata
  description TEXT,
  created_at TEXT NOT NULL,
  last_updated TEXT,
  last_state_change TEXT,
  start_time TEXT,
  last_error TEXT,
  
  FOREIGN KEY (template_id) REFERENCES strategy_templates(template_id)
);

-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
-- Indexes for Performance
-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
CREATE INDEX idx_templates_public ON strategy_templates(is_public);
CREATE INDEX idx_templates_author ON strategy_templates(author);
CREATE INDEX idx_instances_template ON strategy_instances(template_id);
CREATE INDEX idx_instances_symbol ON strategy_instances(symbol);
CREATE INDEX idx_instances_state ON strategy_instances(state);
```

### 🔄 Migration Strategy

```sql
-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
-- STEP 1: Add template columns to existing table (temporary)
-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
ALTER TABLE strategy_instances ADD COLUMN is_template BOOLEAN DEFAULT 0;
ALTER TABLE strategy_instances ADD COLUMN template_id TEXT;

-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
-- STEP 2: Mark existing instances (for backwards compatibility)
-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
UPDATE strategy_instances SET is_template = 0;  -- All existing are instances

-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
-- STEP 3: Create new tables
-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
-- (Run CREATE TABLE statements above)

-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
-- STEP 4: Data stays in old table for now (instances remain functional)
-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
-- New templates go to strategy_templates table
-- New instances go to new strategy_instances table
-- Old instances continue to work from old table

-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
-- STEP 5: Gradual migration (user-initiated)
-- ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
-- User clicks "Convert to Template" on existing instance
-- → Extract definition, create template
-- → Link instance to template
```

---

## Code Architecture Changes

### 🏗️ New Classes Needed

#### 1. **StrategyTemplate.h**

```cpp
#ifndef STRATEGY_TEMPLATE_H
#define STRATEGY_TEMPLATE_H

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

/**
 * @brief Reusable strategy blueprint with variable parameters
 */
struct StrategyTemplate {
  // Identity
  QString templateId;        // UUID
  QString templateName;      // "ATM Straddle"
  QString description;       // "Sell ATM straddle on high IV"
  QString author;            // "alice@trading.com"
  
  // Definition (JSON with {{VARIABLES}})
  QString definitionJson;
  
  // Parameters
  QStringList requiredParams;  // ["SYMBOL", "QUANTITY", "IV_THRESHOLD"]
  QVariantMap defaultParams;   // {"IV_THRESHOLD": 20}
  
  // Categorization
  QString category;          // "Options", "Momentum"
  QStringList tags;          // ["Short Straddle", "IV-based"]
  
  // Versioning
  QString version;           // "1.0"
  QString parentTemplateId;  // For version history
  
  // Sharing
  bool isPublic = false;
  bool isVerified = false;
  
  // Performance
  int totalDeployments = 0;
  double avgPnL = 0.0;
  double successRate = 0.0;
  
  // Metadata
  QDateTime createdAt;
  QDateTime updatedAt;
  
  // Helpers
  bool isValid() const {
    return !templateId.isEmpty() && 
           !templateName.isEmpty() && 
           !definitionJson.isEmpty();
  }
  
  QJsonObject toJson() const;
  static StrategyTemplate fromJson(const QJsonObject &json);
};

#endif // STRATEGY_TEMPLATE_H
```

#### 2. **TemplateService.h**

```cpp
#ifndef TEMPLATE_SERVICE_H
#define TEMPLATE_SERVICE_H

#include "StrategyTemplate.h"
#include <QObject>
#include <QVector>

/**
 * @brief Manages strategy templates (CRUD operations)
 */
class TemplateService : public QObject {
  Q_OBJECT
  
public:
  static TemplateService &instance();
  
  // CRUD operations
  QString createTemplate(const StrategyTemplate &tmpl);
  StrategyTemplate loadTemplate(const QString &templateId);
  bool updateTemplate(const StrategyTemplate &tmpl);
  bool deleteTemplate(const QString &templateId);
  
  // Listing
  QVector<StrategyTemplate> listTemplates(bool publicOnly = false);
  QVector<StrategyTemplate> myTemplates(const QString &author);
  QVector<StrategyTemplate> searchTemplates(const QString &query);
  QVector<StrategyTemplate> templatesByCategory(const QString &category);
  
  // Deployment
  qint64 deployTemplate(
    const QString &templateId,
    const QVariantMap &deployParams,
    const QString &instanceName = QString()
  );
  
  // Validation
  bool validateDeployParams(
    const QString &templateId,
    const QVariantMap &params,
    QString &error
  );
  
  // Publishing
  bool publishTemplate(const QString &templateId);
  bool unpublishTemplate(const QString &templateId);
  
  // Statistics
  void updateTemplateStatistics(const QString &templateId, double pnl, bool success);
  
signals:
  void templateCreated(const StrategyTemplate &tmpl);
  void templateUpdated(const StrategyTemplate &tmpl);
  void templateDeleted(const QString &templateId);
  
private:
  explicit TemplateService(QObject *parent = nullptr);
  
  QString generateTemplateId() const;
  void initializeDatabase();
};

#endif // TEMPLATE_SERVICE_H
```

#### 3. **StrategyTemplateBuilder.h** (UI)

```cpp
#ifndef STRATEGY_TEMPLATE_BUILDER_H
#define STRATEGY_TEMPLATE_BUILDER_H

#include "StrategyTemplate.h"
#include <QDialog>

/**
 * @brief Dialog for creating/editing strategy templates
 * 
 * Key difference from StrategyBuilderDialog:
 * - No symbol/account/quantity fields (those are deployment params)
 * - Adds parameter definition section
 * - Generates JSON with {{VARIABLES}}
 */
class StrategyTemplateBuilder : public QDialog {
  Q_OBJECT
  
public:
  explicit StrategyTemplateBuilder(QWidget *parent = nullptr);
  
  // Edit mode
  void loadTemplate(const QString &templateId);
  
  // Result
  StrategyTemplate getTemplate() const;
  
private:
  // UI sections
  QWidget *createMetadataSection();
  QWidget *createParametersSection();
  QWidget *createLogicSection();
  QWidget *createPreviewSection();
  
  // Parameter management
  void addParameter(const QString &name, const QString &defaultValue);
  void removeParameter(const QString &name);
  
  // JSON generation
  QJsonObject buildTemplateJSON() const;
  
  // Validation
  QString validateTemplate() const;
  
  // Members
  StrategyTemplate m_template;
  // ... UI widgets similar to StrategyBuilderDialog
};

#endif // STRATEGY_TEMPLATE_BUILDER_H
```

#### 4. **DeployTemplateDialog.h** (UI)

```cpp
#ifndef DEPLOY_TEMPLATE_DIALOG_H
#define DEPLOY_TEMPLATE_DIALOG_H

#include "StrategyTemplate.h"
#include <QDialog>
#include <QVariantMap>

/**
 * @brief Dialog for deploying a template to create an instance
 * 
 * Shows only deployment parameters:
 * - Symbol, Account, Segment, Quantity
 * - Risk parameters (SL, Target)
 * - Template-specific parameters (e.g., IV threshold)
 */
class DeployTemplateDialog : public QDialog {
  Q_OBJECT
  
public:
  explicit DeployTemplateDialog(
    const StrategyTemplate &tmpl,
    QWidget *parent = nullptr
  );
  
  // Get deployment parameters
  QVariantMap getDeployParams() const;
  QString getInstanceName() const;
  
private:
  void setupUI();
  void createParameterFields();
  void validateParameters();
  
  StrategyTemplate m_template;
  QMap<QString, QWidget*> m_paramFields;
  
  // Standard deployment fields
  QLineEdit *m_instanceNameEdit;
  QComboBox *m_symbolCombo;
  QLineEdit *m_accountEdit;
  QComboBox *m_segmentCombo;
  QSpinBox *m_quantitySpin;
  QDoubleSpinBox *m_slSpin;
  QDoubleSpinBox *m_targetSpin;
};

#endif // DEPLOY_TEMPLATE_DIALOG_H
```

#### 5. **TemplateLibraryDialog.h** (UI)

```cpp
#ifndef TEMPLATE_LIBRARY_DIALOG_H
#define TEMPLATE_LIBRARY_DIALOG_H

#include "StrategyTemplate.h"
#include <QDialog>

/**
 * @brief Browse and manage strategy templates
 * 
 * Features:
 * - List view with categories
 * - Search and filter
 * - Preview template definition
 * - Deploy/Edit/Delete actions
 * - View statistics
 */
class TemplateLibraryDialog : public QDialog {
  Q_OBJECT
  
public:
  explicit TemplateLibraryDialog(QWidget *parent = nullptr);
  
private slots:
  void onCreateTemplate();
  void onEditTemplate();
  void onDeleteTemplate();
  void onDeployTemplate();
  void onPublishTemplate();
  void onCategoryChanged(const QString &category);
  void onSearchTextChanged(const QString &text);
  
private:
  void setupUI();
  void refreshTemplateList();
  void showTemplateDetails(const StrategyTemplate &tmpl);
  
  QListWidget *m_templateList;
  QComboBox *m_categoryFilter;
  QLineEdit *m_searchEdit;
  QTextEdit *m_detailsView;
};

#endif // TEMPLATE_LIBRARY_DIALOG_H
```

---

## Migration Path

### 🛤️ Phase 1: Quick Fix (This Week)

**Goal:** Enable template mode in current builder

```cpp
// Add to StrategyBuilderDialog.h
class StrategyBuilderDialog : public QDialog {
  // ...existing members
  
  enum BuildMode {
    InstanceMode,   // Old behavior (all fields filled)
    TemplateMode    // New behavior (symbol/account become {{VARS}})
  };
  
  QRadioButton *m_instanceModeRadio;
  QRadioButton *m_templateModeRadio;
  BuildMode m_buildMode = InstanceMode;
  
private slots:
  void onBuildModeChanged();
};
```

**Changes:**
1. Add radio buttons at top of dialog
2. When TemplateMode selected:
   - Disable symbol/account/quantity fields
   - Change placeholders to "{{SYMBOL}}", "{{ACCOUNT}}", "{{QTY}}"
   - buildJSON() uses placeholders instead of actual values
3. Add `is_template` flag to DB (ALTER TABLE)
4. Filter templates in StrategyManagerWindow

**Effort:** 2-3 hours  
**Risk:** Low (backwards compatible)

### 🛤️ Phase 2: Template Service (Next Week)

**Goal:** Proper template management infrastructure

1. Create `strategy_templates` table
2. Implement `TemplateService` class
3. Implement `TemplateEngine` (variable substitution)
4. Migrate StrategyBuilderDialog → StrategyTemplateBuilder
5. Create DeployTemplateDialog

**Effort:** 2-3 days  
**Risk:** Medium (requires testing)

### 🛤️ Phase 3: UI Enhancements (Week 3)

**Goal:** User-friendly template library

1. Create TemplateLibraryDialog
2. Add template categories/tags
3. Add search and filtering
4. Add template preview
5. Add template statistics

**Effort:** 2-3 days  
**Risk:** Low (mostly UI)

### 🛤️ Phase 4: Advanced Features (Month 2)

**Goal:** Marketplace and collaboration

1. Template sharing (public/private)
2. Template versioning
3. Import/export templates
4. Template validation and verification
5. Community ratings and reviews
6. Template marketplace

**Effort:** 1-2 weeks  
**Risk:** Low (additive features)

---

## Real-World Examples

### 📘 Example 1: Simple RSI Reversal

**Template Definition:**
```json
{
  "template_name": "RSI Reversal",
  "description": "Buy when RSI < oversold, Sell when RSI > overbought",
  "parameters": {
    "symbol": "{{SYMBOL}}",
    "timeframe": "{{TIMEFRAME}}",
    "rsi_period": "{{RSI_PERIOD}}",
    "oversold": "{{OVERSOLD}}",
    "overbought": "{{OVERBOUGHT}}",
    "quantity": "{{QUANTITY}}"
  },
  "indicators": [
    {
      "id": "RSI_{{RSI_PERIOD}}",
      "type": "RSI",
      "period": "{{RSI_PERIOD}}"
    }
  ],
  "entry_rules": {
    "logic": "OR",
    "conditions": [
      {
        "type": "Indicator",
        "indicator": "RSI_{{RSI_PERIOD}}",
        "operator": "<",
        "value": "{{OVERSOLD}}"
      }
    ]
  },
  "exit_rules": {
    "logic": "OR",
    "conditions": [
      {
        "type": "Indicator",
        "indicator": "RSI_{{RSI_PERIOD}}",
        "operator": ">",
        "value": "{{OVERBOUGHT}}"
      }
    ]
  },
  "risk_management": {
    "stop_loss_percent": 2.0,
    "target_percent": 3.0,
    "position_size": "{{QUANTITY}}"
  }
}
```

**Deployment 1: NIFTY Intraday**
```cpp
QVariantMap params = {
  {"SYMBOL", "NIFTY"},
  {"TIMEFRAME", "5m"},
  {"RSI_PERIOD", 14},
  {"OVERSOLD", 30},
  {"OVERBOUGHT", 70},
  {"QUANTITY", 50}
};
TemplateService::deploy("RSI_Reversal", params, "NIFTY_RSI_5m");
```

**Deployment 2: BANKNIFTY Swing**
```cpp
QVariantMap params = {
  {"SYMBOL", "BANKNIFTY"},
  {"TIMEFRAME", "1h"},
  {"RSI_PERIOD", 21},
  {"OVERSOLD", 20},
  {"OVERBOUGHT", 80},
  {"QUANTITY", 25}
};
TemplateService::deploy("RSI_Reversal", params, "BANKNIFTY_RSI_1h");
```

### 📘 Example 2: ATM Straddle (Options)

**Template Definition:**
```json
{
  "template_name": "ATM Straddle on High IV",
  "description": "Sell ATM straddle when IV crosses threshold",
  "parameters": {
    "symbol": "{{SYMBOL}}",
    "quantity": "{{QUANTITY}}",
    "iv_threshold": "{{IV_THRESHOLD}}",
    "atm_recalc_period": "{{ATM_RECALC}}"
  },
  "mode": "options",
  "options_config": {
    "atm_recalc_period_sec": "{{ATM_RECALC}}"
  },
  "legs": [
    {
      "id": "LEG_1",
      "symbol": "{{SYMBOL}}",
      "side": "SELL",
      "option_type": "CE",
      "strike_selection": "atm_relative",
      "atm_offset": 0,
      "expiry": "Current Weekly",
      "quantity": "{{QUANTITY}}"
    },
    {
      "id": "LEG_2",
      "symbol": "{{SYMBOL}}",
      "side": "SELL",
      "option_type": "PE",
      "strike_selection": "atm_relative",
      "atm_offset": 0,
      "expiry": "Current Weekly",
      "quantity": "{{QUANTITY}}"
    }
  ],
  "entry_rules": {
    "logic": "AND",
    "conditions": [
      {
        "type": "IV",
        "operator": ">",
        "value": "{{IV_THRESHOLD}}"
      }
    ]
  },
  "exit_rules": {
    "logic": "OR",
    "conditions": [
      {
        "type": "CombinedPremium",
        "operator": "<",
        "value": "50%"
      }
    ]
  }
}
```

**Deployment 1: NIFTY Weekly**
```cpp
QVariantMap params = {
  {"SYMBOL", "NIFTY"},
  {"QUANTITY", 50},
  {"IV_THRESHOLD", 20},
  {"ATM_RECALC", 30}
};
TemplateService::deploy("ATM_Straddle", params, "NIFTY_Straddle_Weekly");
```

**Deployment 2: BANKNIFTY Monthly**
```cpp
QVariantMap params = {
  {"SYMBOL", "BANKNIFTY"},
  {"QUANTITY", 25},
  {"IV_THRESHOLD", 25},
  {"ATM_RECALC", 60}
};
// Override expiry at deployment time
QJsonObject override;
override["legs"][0]["expiry"] = "Current Monthly";
override["legs"][1]["expiry"] = "Current Monthly";
TemplateService::deployWithOverrides("ATM_Straddle", params, override, "BANKNIFTY_Straddle_Monthly");
```

### 📘 Example 3: Index Pair Trade

**Template Definition:**
```json
{
  "template_name": "NIFTY-BANKNIFTY Correlation",
  "description": "Trade spread when correlation breaks",
  "parameters": {
    "symbol1": "{{SYMBOL1}}",
    "symbol2": "{{SYMBOL2}}",
    "quantity1": "{{QUANTITY1}}",
    "quantity2": "{{QUANTITY2}}",
    "spread_threshold": "{{SPREAD_THRESH}}"
  },
  "mode": "multi_symbol",
  "symbols": [
    {
      "id": "SYM_1",
      "symbol": "{{SYMBOL1}}",
      "segment": 2,
      "quantity": "{{QUANTITY1}}",
      "weight": 1.0
    },
    {
      "id": "SYM_2",
      "symbol": "{{SYMBOL2}}",
      "segment": 2,
      "quantity": "{{QUANTITY2}}",
      "weight": -1.0
    }
  ],
  "entry_rules": {
    "logic": "AND",
    "conditions": [
      {
        "type": "SymbolDiff",
        "symbol": "SYM_1",
        "operator": ">",
        "value": "{{SPREAD_THRESH}}"
      }
    ]
  }
}
```

**Deployment:**
```cpp
QVariantMap params = {
  {"SYMBOL1", "NIFTY"},
  {"SYMBOL2", "BANKNIFTY"},
  {"QUANTITY1", 50},
  {"QUANTITY2", 25},
  {"SPREAD_THRESH", 500}
};
TemplateService::deploy("Pair_Trade", params, "NIFTY_BANKNIFTY_Spread");
```

---

## Advanced Concepts

### 🎓 Concept 1: Computed Parameters

**Problem:** User wants to scale quantity based on capital

**Template:**
```json
{
  "parameters": {
    "symbol": "{{SYMBOL}}",
    "capital": "{{CAPITAL}}",
    "risk_percent": "{{RISK_PERCENT}}",
    "quantity": "{{COMPUTED:QUANTITY}}"  // ← Computed at deploy time
  }
}
```

**Deployment:**
```cpp
// User inputs
QVariantMap inputs = {
  {"SYMBOL", "NIFTY"},
  {"CAPITAL", 100000},
  {"RISK_PERCENT", 2.0}
};

// Compute quantity based on capital and risk
double risk = inputs["CAPITAL"].toDouble() * inputs["RISK_PERCENT"].toDouble() / 100.0;
double ltp = MarketData::getLTP("NIFTY");
int quantity = qFloor(risk / ltp);

// Add computed param
inputs["QUANTITY"] = quantity;

TemplateService::deploy("RSI_Reversal", inputs);
```

### 🎓 Concept 2: Conditional Logic in Templates

**Problem:** Different entry logic for different market conditions

**Template:**
```json
{
  "parameters": {
    "market_mode": "{{MARKET_MODE}}"  // "TRENDING" or "RANGING"
  },
  "entry_rules": {
    "{{IF:MARKET_MODE==TRENDING}}": {
      "logic": "AND",
      "conditions": [
        { "type": "Indicator", "indicator": "ADX", "operator": ">", "value": 25 }
      ]
    },
    "{{IF:MARKET_MODE==RANGING}}": {
      "logic": "AND",
      "conditions": [
        { "type": "Indicator", "indicator": "BB_WIDTH", "operator": "<", "value": 10 }
      ]
    }
  }
}
```

### 🎓 Concept 3: Multi-Timeframe Templates

**Template:**
```json
{
  "parameters": {
    "symbol": "{{SYMBOL}}",
    "entry_timeframe": "{{ENTRY_TF}}",
    "filter_timeframe": "{{FILTER_TF}}"
  },
  "indicators": [
    {
      "id": "SMA_200",
      "type": "SMA",
      "period": 200,
      "timeframe": "{{FILTER_TF}}"  // Higher TF for trend filter
    },
    {
      "id": "RSI_14",
      "type": "RSI",
      "period": 14,
      "timeframe": "{{ENTRY_TF}}"   // Lower TF for entry
    }
  ]
}
```

### 🎓 Concept 4: Symbol Groups

**Problem:** Deploy same strategy to multiple symbols at once

**Template:**
```json
{
  "parameters": {
    "symbol_group": "{{SYMBOL_GROUP}}"  // "NIFTY50", "BANKNIFTY", "FINNIFTY"
  }
}
```

**Deployment:**
```cpp
QStringList nifty50 = SymbolGroups::getSymbols("NIFTY50");
for (const QString &symbol : nifty50) {
  QVariantMap params = {{"SYMBOL", symbol}};
  TemplateService::deploy("RSI_Reversal", params);
}
```

---

## Implementation Roadmap

### 🗓️ Week 1: Foundation

**Day 1-2:**
- [ ] Create `strategy_templates` table
- [ ] Add `is_template` flag to existing `strategy_instances` table
- [ ] Implement `TemplateEngine::substituteVariables()`
- [ ] Unit tests for variable substitution

**Day 3-4:**
- [ ] Implement `TemplateService` (CRUD operations)
- [ ] Add template mode to `StrategyBuilderDialog`
- [ ] Update `StrategyManagerWindow` to filter templates

**Day 5:**
- [ ] Integration testing
- [ ] Documentation
- [ ] User acceptance testing

### 🗓️ Week 2: Deployment

**Day 1-2:**
- [ ] Create `DeployTemplateDialog` UI
- [ ] Implement parameter validation
- [ ] Wire up deployment flow

**Day 3-4:**
- [ ] Create `TemplateLibraryDialog` UI
- [ ] Implement template listing/search
- [ ] Add template preview

**Day 5:**
- [ ] Testing and bug fixes
- [ ] Performance optimization

### 🗓️ Week 3: Polish

**Day 1-2:**
- [ ] Add template categories and tags
- [ ] Implement template versioning
- [ ] Add import/export functionality

**Day 3-4:**
- [ ] Add template statistics
- [ ] Implement template sharing (public/private)
- [ ] Add template ratings

**Day 5:**
- [ ] Final testing
- [ ] Documentation
- [ ] Release

### 🗓️ Month 2: Advanced

**Week 4:**
- [ ] Template marketplace UI
- [ ] Community templates
- [ ] Template verification system

**Week 5:**
- [ ] Computed parameters support
- [ ] Conditional logic in templates
- [ ] Multi-timeframe support

**Week 6:**
- [ ] Symbol groups support
- [ ] Template inheritance
- [ ] Template composition

**Week 7:**
- [ ] AI-assisted template creation
- [ ] Backtesting integration
- [ ] Performance analytics

---

## Conclusion

### ✅ Key Takeaways

1. **Fundamental Design Flaw:** Current implementation conflates template and instance
2. **Impact:** Zero reusability - must rebuild strategy for each symbol
3. **Solution:** Separate template (abstract logic) from instance (concrete deployment)
4. **Architecture:** Two tables, variable substitution engine, template service
5. **Migration:** Phased approach - backwards compatible
6. **Benefits:** Reusability, sharing, marketplace, collaboration

### 🎯 Next Steps

**Immediate:**
1. Review this document with team
2. Approve migration plan
3. Begin Phase 1 implementation

**Short Term:**
1. Complete Week 1-3 roadmap
2. Deploy template system to production
3. Gather user feedback

**Long Term:**
1. Build template marketplace
2. Enable community sharing
3. Add AI-assisted features

### 📚 References

- **Design Patterns:** Gang of Four - Strategy Pattern, Template Method Pattern
- **Database Design:** Martin Fowler - Patterns of Enterprise Application Architecture
- **Qt Documentation:** Model/View Programming, JSON Handling
- **Trading Systems:** Building Algorithmic Trading Systems (Kevin Davey)

---

**Document Status:** 🟢 Complete  
**Last Updated:** February 18, 2026  
**Review Required:** Yes  
**Approvers:** Technical Lead, Product Owner

