# Strategy Parameter UI Integration Guide
**How to Deploy Templates with Dynamic Parameters**

**Date:** February 18, 2026  
**Focus:** UI Design, Multi-Symbol Support, Integration Architecture

---

## 📚 Table of Contents

1. [The Parameter Problem](#the-parameter-problem)
2. [UI Solution Options](#ui-solution-options)
3. [Recommended: Wizard-Based Approach](#recommended-wizard-based-approach)
4. [Integration with Strategy Manager](#integration-with-strategy-manager)
5. [Multi-Symbol Scenarios](#multi-symbol-scenarios)
6. [Advanced: Multi-Reference + Multi-Trade](#advanced-multi-reference-multi-trade)
7. [Code Implementation](#code-implementation)
8. [Complete Integration Flow](#complete-integration-flow)

---

## The Parameter Problem

### 🎯 Core Challenge

**Template Definition:**
```json
{
  "template_name": "ATM Straddle",
  "parameters": {
    "symbol": "{{SYMBOL}}",
    "quantity": "{{QUANTITY}}",
    "iv_threshold": "{{IV_THRESHOLD}}"
  }
}
```

**User needs to provide:**
- `SYMBOL` → "NIFTY"
- `QUANTITY` → 50
- `IV_THRESHOLD` → 20

**Question:** How does the user fill these values?

### 📊 Parameter Types

| Parameter Type | Example | Input Widget | Validation |
|---------------|---------|--------------|------------|
| **Symbol** | NIFTY | Searchable combo + autocomplete | Must exist in master |
| **Quantity** | 50 | Spin box | > 0, respects lot size |
| **Account** | RAJ143 | Dropdown from active accounts | Must be valid |
| **Segment** | NSE FO | Dropdown | 1=NSECM, 2=NSEFO, etc. |
| **Product** | MIS | Dropdown | MIS/NRML/CNC |
| **Threshold** | 20 | Spin/slider | Custom range |
| **Timeframe** | 5m | Combo | 1m/5m/15m/1h/1d |
| **Boolean** | true | Checkbox | true/false |

### 🔍 Discovery Problem

**How does UI know what parameters a template needs?**

**Solution:** Template declares required parameters

```json
{
  "template_id": "uuid-123",
  "template_name": "ATM Straddle",
  "required_params": [
    {
      "name": "SYMBOL",
      "type": "symbol",
      "label": "Trading Symbol",
      "description": "Symbol to trade (e.g., NIFTY, BANKNIFTY)",
      "default": "",
      "required": true,
      "validation": {
        "segments": [2, 12]  // NSE FO or BSE FO
      }
    },
    {
      "name": "QUANTITY",
      "type": "integer",
      "label": "Lot Quantity",
      "description": "Number of lots to trade",
      "default": 50,
      "required": true,
      "validation": {
        "min": 1,
        "max": 1000,
        "step": 1
      }
    },
    {
      "name": "IV_THRESHOLD",
      "type": "float",
      "label": "IV Entry Threshold",
      "description": "Enter trade when IV crosses this level",
      "default": 20.0,
      "required": true,
      "validation": {
        "min": 10.0,
        "max": 50.0,
        "step": 0.5
      }
    }
  ]
}
```

---

## UI Solution Options

### 🎨 Option 1: Simple Dialog (Quick & Easy)

**Pros:** Fast to implement, familiar pattern  
**Cons:** Not scalable for complex templates

```
┌────────────────────────────────────────────┐
│  Deploy Template: ATM Straddle             │
├────────────────────────────────────────────┤
│                                            │
│  Instance Name: [NIFTY_Straddle_1        ]│
│                                            │
│  ┌─── Template Parameters ──────────────┐ │
│  │                                        │ │
│  │  Trading Symbol:  [NIFTY        ▼]    │ │
│  │  Lot Quantity:    [50           ↑↓]   │ │
│  │  IV Threshold:    [20.0         ↑↓]   │ │
│  │  Account:         [RAJ143       ▼]    │ │
│  │  Product:         [MIS          ▼]    │ │
│  │  Segment:         [NSE F&O      ▼]    │ │
│  │                                        │ │
│  └────────────────────────────────────────┘ │
│                                            │
│  ┌─── Risk Management ───────────────────┐ │
│  │  Stop Loss:  [2.0  %  ↑↓]             │ │
│  │  Target:     [3.0  %  ↑↓]             │ │
│  └────────────────────────────────────────┘ │
│                                            │
│              [Deploy]  [Cancel]            │
└────────────────────────────────────────────┘
```

**Code:**
```cpp
class SimpleDeployDialog : public QDialog {
  Q_OBJECT
public:
  SimpleDeployDialog(const StrategyTemplate &tmpl, QWidget *parent = nullptr) {
    // Fixed form based on template's required_params
    for (const auto &param : tmpl.requiredParams) {
      addParameterField(param);
    }
  }
  
private:
  void addParameterField(const ParamDefinition &param) {
    if (param.type == "symbol") {
      auto *combo = new SymbolSearchCombo;
      combo->setSegmentFilter(param.validation["segments"]);
      m_paramFields[param.name] = combo;
    } else if (param.type == "integer") {
      auto *spin = new QSpinBox;
      spin->setRange(param.validation["min"], param.validation["max"]);
      spin->setValue(param.defaultValue.toInt());
      m_paramFields[param.name] = spin;
    }
    // ... etc
  }
};
```

### 🎨 Option 2: Wizard-Based (Step-by-Step)

**Pros:** User-friendly, progressive disclosure, great for complex templates  
**Cons:** More code, multiple pages

```
┌────────────────────────────────────────────┐
│  Deploy Template: ATM Straddle             │
│  Step 1 of 3: Basic Information            │
├────────────────────────────────────────────┤
│                                            │
│  Instance Name:                            │
│  [NIFTY_Straddle_Session1              ]   │
│                                            │
│  Description (optional):                   │
│  [Sell ATM straddle when IV > 20       ]   │
│                                            │
│              [Back]  [Next >]              │
└────────────────────────────────────────────┘

┌────────────────────────────────────────────┐
│  Deploy Template: ATM Straddle             │
│  Step 2 of 3: Trading Parameters           │
├────────────────────────────────────────────┤
│                                            │
│  Trading Symbol:                           │
│  [NIFTY                             ▼]     │
│  ℹ Symbol to trade (e.g., NIFTY, BANKNIFTY)│
│                                            │
│  Lot Quantity:                             │
│  [50                                ↑↓]    │
│  ℹ Number of lots to trade                 │
│                                            │
│  IV Entry Threshold:                       │
│  [20.0                              ↑↓]    │
│  ℹ Enter when IV crosses this level        │
│                                            │
│           [< Back]  [Next >]               │
└────────────────────────────────────────────┘

┌────────────────────────────────────────────┐
│  Deploy Template: ATM Straddle             │
│  Step 3 of 3: Deployment Settings          │
├────────────────────────────────────────────┤
│                                            │
│  Account:    [RAJ143                ▼]     │
│  Segment:    [NSE F&O               ▼]     │
│  Product:    [MIS                   ▼]     │
│                                            │
│  Stop Loss:  [2.0  %  ↑↓]                  │
│  Target:     [3.0  %  ↑↓]                  │
│                                            │
│  ┌─ Preview ────────────────────────────┐  │
│  │ Symbol: NIFTY                        │  │
│  │ Quantity: 50 lots                    │  │
│  │ Entry: IV > 20                       │  │
│  │ Account: RAJ143                      │  │
│  └──────────────────────────────────────┘  │
│                                            │
│         [< Back]  [Deploy]  [Cancel]       │
└────────────────────────────────────────────┘
```

### 🎨 Option 3: Inline Editor (Advanced)

**Pros:** Power user efficiency, can edit multiple params at once  
**Cons:** Overwhelming for beginners

```
┌────────────────────────────────────────────────────────────┐
│  Strategy Manager                                           │
├────────────────────────────────────────────────────────────┤
│  Templates  [Instances]                                     │
│  ┌────────────────────────────────────────────────────────┐│
│  │☑ ATM Straddle        [Deploy ▼]                        ││
│  │  Sell ATM straddle when IV crosses threshold            ││
│  │                                                          ││
│  │  ┌─ Quick Deploy ─────────────────────────────────────┐││
│  │  │                                                      │││
│  │  │  Name: [NIFTY_Straddle_1        ]  Symbol: [NIFTY▼]│││
│  │  │  Qty:  [50 ↑↓]  IV: [20 ↑↓]  Account: [RAJ143  ▼] │││
│  │  │                                     [Deploy Now]    │││
│  │  └──────────────────────────────────────────────────────┘││
│  │                                                          ││
│  │  Or [Advanced Deploy...] for more options               ││
│  └────────────────────────────────────────────────────────┘│
│                                                             │
│  Running Instances:                                         │
│  ┌────────────────────────────────────────────────────────┐│
│  │ NIFTY_Straddle_1   NIFTY   RUNNING   MTM: -1250.50    ││
│  │ BANKNIFTY_Straddle BANKNIFTY PAUSED  MTM: +2300.00    ││
│  └────────────────────────────────────────────────────────┘│
└────────────────────────────────────────────────────────────┘
```

### 🎨 Option 4: Form Generator (Dynamic)

**Pros:** Zero hardcoding, works for any template  
**Cons:** Generic UI, less polished

```cpp
class DynamicFormBuilder {
public:
  static QWidget* buildParameterForm(const StrategyTemplate &tmpl) {
    auto *form = new QWidget;
    auto *layout = new QFormLayout(form);
    
    for (const ParamDefinition &param : tmpl.requiredParams) {
      QWidget *inputWidget = createInputWidget(param);
      layout->addRow(param.label, inputWidget);
    }
    
    return form;
  }
  
private:
  static QWidget* createInputWidget(const ParamDefinition &param) {
    switch (param.type) {
      case ParamType::Symbol:
        return createSymbolCombo(param);
      case ParamType::Integer:
        return createSpinBox(param);
      case ParamType::Float:
        return createDoubleSpinBox(param);
      case ParamType::Boolean:
        return createCheckBox(param);
      case ParamType::Choice:
        return createComboBox(param);
      default:
        return createLineEdit(param);
    }
  }
};
```

---

## Recommended: Wizard-Based Approach

### 🏆 Why Wizard?

1. **Progressive Disclosure** - Don't overwhelm user with 20 fields
2. **Contextual Help** - Each page explains parameters
3. **Validation Per Step** - Catch errors early
4. **Preview Before Deploy** - User sees final config
5. **Flexible** - Can add/skip pages based on template complexity

### 📐 Architecture

```cpp
class DeployTemplateWizard : public QWizard {
  Q_OBJECT
  
public:
  enum PageId {
    Page_Introduction,      // Template info + instance name
    Page_Parameters,        // Template-specific params
    Page_Deployment,        // Account/segment/risk
    Page_Preview           // Final review
  };
  
  DeployTemplateWizard(const StrategyTemplate &tmpl, QWidget *parent = nullptr);
  
  // Get results
  QString instanceName() const;
  QVariantMap deployParams() const;
  
private:
  StrategyTemplate m_template;
  
  // Pages
  IntroductionPage *m_introPage;
  ParametersPage *m_paramsPage;
  DeploymentPage *m_deployPage;
  PreviewPage *m_previewPage;
};
```

### 📄 Page 1: Introduction

```cpp
class IntroductionPage : public QWizardPage {
public:
  IntroductionPage(const StrategyTemplate &tmpl) {
    setTitle("Deploy Strategy Template");
    setSubTitle(QString("Template: %1").arg(tmpl.templateName));
    
    auto *layout = new QVBoxLayout(this);
    
    // Template info
    auto *infoBox = new QGroupBox("Template Information");
    auto *infoLayout = new QFormLayout(infoBox);
    infoLayout->addRow("Name:", new QLabel(tmpl.templateName));
    infoLayout->addRow("Description:", new QLabel(tmpl.description));
    infoLayout->addRow("Author:", new QLabel(tmpl.author));
    infoLayout->addRow("Version:", new QLabel(tmpl.version));
    layout->addWidget(infoBox);
    
    // Instance name
    auto *nameBox = new QGroupBox("Instance Configuration");
    auto *nameLayout = new QFormLayout(nameBox);
    m_instanceNameEdit = new QLineEdit;
    m_instanceNameEdit->setPlaceholderText("e.g., NIFTY_Straddle_Session1");
    nameLayout->addRow("Instance Name:", m_instanceNameEdit);
    
    m_descEdit = new QTextEdit;
    m_descEdit->setMaximumHeight(60);
    m_descEdit->setPlaceholderText("Optional description...");
    nameLayout->addRow("Description:", m_descEdit);
    layout->addWidget(nameBox);
    
    registerField("instanceName*", m_instanceNameEdit);
    registerField("description", m_descEdit, "plainText");
  }
  
private:
  QLineEdit *m_instanceNameEdit;
  QTextEdit *m_descEdit;
};
```

### 📄 Page 2: Parameters (Dynamic)

```cpp
class ParametersPage : public QWizardPage {
public:
  ParametersPage(const StrategyTemplate &tmpl) {
    setTitle("Trading Parameters");
    setSubTitle("Configure template-specific parameters");
    
    auto *layout = new QVBoxLayout(this);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    
    auto *paramWidget = new QWidget;
    auto *paramLayout = new QFormLayout(paramWidget);
    
    // Dynamically create fields based on template
    for (const ParamDefinition &param : tmpl.requiredParams) {
      QWidget *inputWidget = createInputForParam(param);
      
      // Add label with tooltip
      auto *label = new QLabel(param.label + ":");
      if (!param.description.isEmpty()) {
        label->setToolTip(param.description);
        
        // Add info icon
        auto *infoLabel = new QLabel(" ℹ");
        infoLabel->setStyleSheet("color: #2196F3;");
        infoLabel->setToolTip(param.description);
        
        auto *labelLayout = new QHBoxLayout;
        labelLayout->addWidget(label);
        labelLayout->addWidget(infoLabel);
        labelLayout->addStretch();
        
        paramLayout->addRow(labelLayout, inputWidget);
      } else {
        paramLayout->addRow(label, inputWidget);
      }
      
      m_paramWidgets[param.name] = inputWidget;
      
      // Register field for wizard validation
      if (param.required) {
        registerField(param.name + "*", inputWidget);
      } else {
        registerField(param.name, inputWidget);
      }
    }
    
    scroll->setWidget(paramWidget);
    layout->addWidget(scroll);
  }
  
  QVariantMap getParameters() const {
    QVariantMap params;
    for (auto it = m_paramWidgets.begin(); it != m_paramWidgets.end(); ++it) {
      QString name = it.key();
      QWidget *widget = it.value();
      params[name] = extractValueFromWidget(widget);
    }
    return params;
  }
  
private:
  QWidget* createInputForParam(const ParamDefinition &param) {
    if (param.type == "symbol") {
      auto *combo = new SymbolSearchCombo;
      // Filter by allowed segments
      if (param.validation.contains("segments")) {
        QVariantList segments = param.validation["segments"].toList();
        for (const auto &seg : segments) {
          combo->addSegmentFilter(seg.toInt());
        }
      }
      if (!param.defaultValue.isEmpty()) {
        combo->setCurrentSymbol(param.defaultValue);
      }
      return combo;
      
    } else if (param.type == "integer") {
      auto *spin = new QSpinBox;
      spin->setRange(
        param.validation.value("min", 0).toInt(),
        param.validation.value("max", 999999).toInt()
      );
      spin->setSingleStep(param.validation.value("step", 1).toInt());
      spin->setValue(param.defaultValue.toInt());
      return spin;
      
    } else if (param.type == "float") {
      auto *spin = new QDoubleSpinBox;
      spin->setRange(
        param.validation.value("min", 0.0).toDouble(),
        param.validation.value("max", 99999.0).toDouble()
      );
      spin->setSingleStep(param.validation.value("step", 0.1).toDouble());
      spin->setDecimals(2);
      spin->setValue(param.defaultValue.toDouble());
      return spin;
      
    } else if (param.type == "boolean") {
      auto *check = new QCheckBox;
      check->setChecked(param.defaultValue.toBool());
      return check;
      
    } else if (param.type == "choice") {
      auto *combo = new QComboBox;
      QStringList choices = param.validation["choices"].toStringList();
      combo->addItems(choices);
      combo->setCurrentText(param.defaultValue);
      return combo;
      
    } else {
      // Default: line edit
      auto *edit = new QLineEdit;
      edit->setPlaceholderText(param.description);
      edit->setText(param.defaultValue);
      return edit;
    }
  }
  
  QVariant extractValueFromWidget(QWidget *widget) const {
    if (auto *spin = qobject_cast<QSpinBox*>(widget)) {
      return spin->value();
    } else if (auto *dspin = qobject_cast<QDoubleSpinBox*>(widget)) {
      return dspin->value();
    } else if (auto *check = qobject_cast<QCheckBox*>(widget)) {
      return check->isChecked();
    } else if (auto *combo = qobject_cast<QComboBox*>(widget)) {
      return combo->currentText();
    } else if (auto *symbolCombo = qobject_cast<SymbolSearchCombo*>(widget)) {
      return symbolCombo->currentSymbol();
    } else if (auto *edit = qobject_cast<QLineEdit*>(widget)) {
      return edit->text();
    }
    return QVariant();
  }
  
  QMap<QString, QWidget*> m_paramWidgets;
};
```

### 📄 Page 3: Deployment Settings

```cpp
class DeploymentPage : public QWizardPage {
public:
  DeploymentPage() {
    setTitle("Deployment Settings");
    setSubTitle("Configure account and risk parameters");
    
    auto *layout = new QVBoxLayout(this);
    
    // Trading account
    auto *accountBox = new QGroupBox("Trading Account");
    auto *accountLayout = new QFormLayout(accountBox);
    
    m_accountCombo = new QComboBox;
    m_accountCombo->addItems(AccountManager::getActiveAccounts());
    accountLayout->addRow("Account:", m_accountCombo);
    
    m_segmentCombo = new QComboBox;
    m_segmentCombo->addItem("NSE Cash (CM)", 1);
    m_segmentCombo->addItem("NSE F&O", 2);
    m_segmentCombo->addItem("BSE Cash (CM)", 11);
    m_segmentCombo->addItem("BSE F&O", 12);
    m_segmentCombo->setCurrentIndex(1); // Default NSE FO
    accountLayout->addRow("Segment:", m_segmentCombo);
    
    m_productCombo = new QComboBox;
    m_productCombo->addItems({"MIS", "NRML", "CNC"});
    accountLayout->addRow("Product:", m_productCombo);
    
    layout->addWidget(accountBox);
    
    // Risk management
    auto *riskBox = new QGroupBox("Risk Management");
    auto *riskLayout = new QFormLayout(riskBox);
    
    m_slSpin = new QDoubleSpinBox;
    m_slSpin->setRange(0.1, 50.0);
    m_slSpin->setValue(2.0);
    m_slSpin->setSuffix(" %");
    riskLayout->addRow("Stop Loss:", m_slSpin);
    
    m_targetSpin = new QDoubleSpinBox;
    m_targetSpin->setRange(0.1, 100.0);
    m_targetSpin->setValue(3.0);
    m_targetSpin->setSuffix(" %");
    riskLayout->addRow("Target:", m_targetSpin);
    
    layout->addWidget(riskBox);
    
    registerField("account*", m_accountCombo, "currentText");
    registerField("segment", m_segmentCombo, "currentData");
    registerField("product", m_productCombo, "currentText");
    registerField("stopLoss", m_slSpin, "value");
    registerField("target", m_targetSpin, "value");
  }
  
private:
  QComboBox *m_accountCombo;
  QComboBox *m_segmentCombo;
  QComboBox *m_productCombo;
  QDoubleSpinBox *m_slSpin;
  QDoubleSpinBox *m_targetSpin;
};
```

### 📄 Page 4: Preview

```cpp
class PreviewPage : public QWizardPage {
public:
  PreviewPage(const StrategyTemplate &tmpl) : m_template(tmpl) {
    setTitle("Review and Deploy");
    setSubTitle("Verify configuration before deployment");
    
    auto *layout = new QVBoxLayout(this);
    
    m_previewText = new QTextEdit;
    m_previewText->setReadOnly(true);
    m_previewText->setFont(QFont("Consolas", 10));
    layout->addWidget(m_previewText);
    
    m_validationLabel = new QLabel;
    m_validationLabel->setWordWrap(true);
    layout->addWidget(m_validationLabel);
  }
  
  void initializePage() override {
    // Called when user reaches this page
    updatePreview();
  }
  
private:
  void updatePreview() {
    QString preview;
    preview += "═══════════════════════════════════════\n";
    preview += "  DEPLOYMENT PREVIEW\n";
    preview += "═══════════════════════════════════════\n\n";
    
    preview += "Template: " + m_template.templateName + "\n";
    preview += "Instance: " + field("instanceName").toString() + "\n\n";
    
    preview += "Parameters:\n";
    auto params = qobject_cast<ParametersPage*>(wizard()->page(Page_Parameters))->getParameters();
    for (auto it = params.begin(); it != params.end(); ++it) {
      preview += QString("  • %1: %2\n").arg(it.key()).arg(it.value().toString());
    }
    
    preview += "\nDeployment:\n";
    preview += QString("  • Account: %1\n").arg(field("account").toString());
    preview += QString("  • Segment: %1\n").arg(field("segment").toInt());
    preview += QString("  • Product: %1\n").arg(field("product").toString());
    preview += QString("  • Stop Loss: %1%\n").arg(field("stopLoss").toDouble());
    preview += QString("  • Target: %1%\n").arg(field("target").toDouble());
    
    m_previewText->setPlainText(preview);
    
    // Validate
    QString error;
    bool valid = TemplateEngine::validateSubstitution(
      m_template.definitionJson,
      params,
      error
    );
    
    if (valid) {
      m_validationLabel->setStyleSheet("color: green;");
      m_validationLabel->setText("✓ Configuration is valid. Ready to deploy.");
    } else {
      m_validationLabel->setStyleSheet("color: red;");
      m_validationLabel->setText("✗ Validation Error: " + error);
    }
  }
  
  StrategyTemplate m_template;
  QTextEdit *m_previewText;
  QLabel *m_validationLabel;
};
```

---

## Integration with Strategy Manager

### 🔗 Integration Points

```
StrategyManagerWindow
  ├─ [Templates Tab] ← NEW
  │  ├─ List templates
  │  ├─ Search/filter
  │  ├─ Preview template
  │  └─ [Deploy] → DeployTemplateWizard
  │
  └─ [Instances Tab] (existing)
     ├─ List running instances
     ├─ Start/stop/pause
     └─ [Create Custom] → StrategyBuilderDialog (creates template)
```

### 📊 Updated Strategy Manager UI

```
┌──────────────────────────────────────────────────────────────┐
│  Strategy Manager                                   [× Close] │
├──────────────────────────────────────────────────────────────┤
│  [Templates]  [Instances]                                     │
│  ┌──────────────────────────────────────────────────────────┐│
│  │                                                            ││
│  │  Search: [____________________] 🔍  Category: [All    ▼] ││
│  │                                                            ││
│  │  [+ Create Template]  [Import Template]                   ││
│  │                                                            ││
│  │  My Templates (3)                                          ││
│  │  ┌──────────────────────────────────────────────────────┐││
│  │  │ ☑ ATM Straddle               [Deploy] [Edit] [Delete]│││
│  │  │   Sell ATM straddle on high IV                        │││
│  │  │   Deployments: 5 | Avg PnL: +12,345 | Success: 80%   │││
│  │  │                                                        │││
│  │  │ ☑ RSI Reversal               [Deploy] [Edit] [Delete]│││
│  │  │   Buy/sell based on RSI levels                        │││
│  │  │   Deployments: 12 | Avg PnL: +2,456 | Success: 67%   │││
│  │  │                                                        │││
│  │  │ ☑ Iron Condor                [Deploy] [Edit] [Delete]│││
│  │  │   4-leg options strategy                              │││
│  │  │   Deployments: 0 | Not yet deployed                   │││
│  │  └──────────────────────────────────────────────────────┘││
│  │                                                            ││
│  │  Community Templates (10+)                                ││
│  │  ┌──────────────────────────────────────────────────────┐││
│  │  │ ★ Supertrend Momentum       [Deploy] [Preview] [Fork]│││
│  │  │   By: alice@trading.com       ⭐ 4.5/5 (23 reviews)   │││
│  │  │                                                        │││
│  │  │ ★ Bollinger Squeeze         [Deploy] [Preview] [Fork]│││
│  │  │   By: bob@strategies.in       ⭐ 4.8/5 (41 reviews)   │││
│  │  └──────────────────────────────────────────────────────┘││
│  └────────────────────────────────────────────────────────────┘
└──────────────────────────────────────────────────────────────┘
```

### 🔌 Code Integration

```cpp
// StrategyManagerWindow.cpp

void StrategyManagerWindow::setupUI() {
  // Create tab widget
  auto *tabs = new QTabWidget(this);
  
  // Templates tab (NEW)
  auto *templatesTab = createTemplatesTab();
  tabs->addTab(templatesTab, "Templates");
  
  // Instances tab (existing)
  auto *instancesTab = createInstancesTab();
  tabs->addTab(instancesTab, "Instances");
  
  // ...
}

QWidget* StrategyManagerWindow::createTemplatesTab() {
  auto *widget = new QWidget;
  auto *layout = new QVBoxLayout(widget);
  
  // Toolbar
  auto *toolbar = new QHBoxLayout;
  auto *searchEdit = new QLineEdit;
  searchEdit->setPlaceholderText("Search templates...");
  toolbar->addWidget(searchEdit);
  
  auto *categoryCombo = new QComboBox;
  categoryCombo->addItems({"All", "Options", "Momentum", "Reversal", "Arbitrage"});
  toolbar->addWidget(categoryCombo);
  
  auto *createBtn = new QPushButton("+ Create Template");
  connect(createBtn, &QPushButton::clicked, this, &StrategyManagerWindow::onCreateTemplate);
  toolbar->addWidget(createBtn);
  
  auto *importBtn = new QPushButton("Import Template");
  connect(importBtn, &QPushButton::clicked, this, &StrategyManagerWindow::onImportTemplate);
  toolbar->addWidget(importBtn);
  
  layout->addLayout(toolbar);
  
  // Template list
  m_templateList = new QListWidget;
  connect(m_templateList, &QListWidget::itemDoubleClicked, 
          this, &StrategyManagerWindow::onDeployTemplate);
  layout->addWidget(m_templateList);
  
  refreshTemplateList();
  
  return widget;
}

void StrategyManagerWindow::refreshTemplateList() {
  m_templateList->clear();
  
  // Load templates from service
  QVector<StrategyTemplate> templates = TemplateService::instance().listTemplates();
  
  for (const auto &tmpl : templates) {
    auto *item = new QListWidgetItem(m_templateList);
    auto *widget = new TemplateListItemWidget(tmpl);
    
    // Connect deploy button
    connect(widget, &TemplateListItemWidget::deployClicked,
            this, [this, tmpl]() { onDeployTemplate(tmpl); });
    
    item->setSizeHint(widget->sizeHint());
    m_templateList->setItemWidget(item, widget);
  }
}

void StrategyManagerWindow::onDeployTemplate(const StrategyTemplate &tmpl) {
  // Launch wizard
  DeployTemplateWizard wizard(tmpl, this);
  
  if (wizard.exec() != QDialog::Accepted) {
    return;
  }
  
  // Get deployment parameters
  QString instanceName = wizard.instanceName();
  QVariantMap deployParams = wizard.deployParams();
  
  // Deploy via service
  qint64 instanceId = TemplateService::instance().deployTemplate(
    tmpl.templateId,
    deployParams,
    instanceName
  );
  
  if (instanceId > 0) {
    QMessageBox::information(this, "Success", 
      QString("Template deployed successfully!\nInstance ID: %1").arg(instanceId));
    
    // Switch to instances tab and highlight new instance
    ui->tabWidget->setCurrentIndex(1); // Instances tab
    refreshInstanceList();
    selectInstance(instanceId);
  } else {
    QMessageBox::critical(this, "Error", "Failed to deploy template.");
  }
}

void StrategyManagerWindow::onCreateTemplate() {
  // Open template builder
  StrategyTemplateBuilder builder(this);
  
  if (builder.exec() != QDialog::Accepted) {
    return;
  }
  
  StrategyTemplate tmpl = builder.getTemplate();
  
  // Save via service
  QString templateId = TemplateService::instance().createTemplate(tmpl);
  
  if (!templateId.isEmpty()) {
    QMessageBox::information(this, "Success", "Template created successfully!");
    refreshTemplateList();
  } else {
    QMessageBox::critical(this, "Error", "Failed to create template.");
  }
}
```

### 🎨 Template List Item Widget

```cpp
class TemplateListItemWidget : public QWidget {
  Q_OBJECT
  
public:
  TemplateListItemWidget(const StrategyTemplate &tmpl, QWidget *parent = nullptr) 
    : QWidget(parent), m_template(tmpl) {
    
    auto *layout = new QHBoxLayout(this);
    
    // Template info
    auto *infoLayout = new QVBoxLayout;
    
    auto *nameLabel = new QLabel(QString("<b>%1</b>").arg(tmpl.templateName));
    infoLayout->addWidget(nameLabel);
    
    auto *descLabel = new QLabel(tmpl.description);
    descLabel->setStyleSheet("color: gray; font-size: 10pt;");
    infoLayout->addWidget(descLabel);
    
    auto *statsLabel = new QLabel(QString(
      "Deployments: %1 | Avg PnL: ₹%2 | Success: %3%"
    ).arg(tmpl.totalDeployments)
     .arg(tmpl.avgPnL, 0, 'f', 2)
     .arg(tmpl.successRate, 0, 'f', 1));
    statsLabel->setStyleSheet("color: #666; font-size: 9pt;");
    infoLayout->addWidget(statsLabel);
    
    layout->addLayout(infoLayout, 1);
    
    // Action buttons
    auto *deployBtn = new QPushButton("Deploy");
    deployBtn->setStyleSheet("background: #4CAF50; color: white; font-weight: bold;");
    connect(deployBtn, &QPushButton::clicked, this, &TemplateListItemWidget::deployClicked);
    layout->addWidget(deployBtn);
    
    auto *editBtn = new QPushButton("Edit");
    connect(editBtn, &QPushButton::clicked, this, &TemplateListItemWidget::editClicked);
    layout->addWidget(editBtn);
    
    auto *deleteBtn = new QPushButton("Delete");
    deleteBtn->setStyleSheet("color: #f44336;");
    connect(deleteBtn, &QPushButton::clicked, this, &TemplateListItemWidget::deleteClicked);
    layout->addWidget(deleteBtn);
  }
  
signals:
  void deployClicked();
  void editClicked();
  void deleteClicked();
  
private:
  StrategyTemplate m_template;
};
```

---

## Multi-Symbol Scenarios

### 🎯 Problem: Multiple Reference + Multiple Trade Symbols

**Use Case 1: Cross-Market Options**
- **Reference Symbol:** SENSEX (monitor IV)
- **Trade Symbols:** NIFTY options (execute straddle)

**Use Case 2: Index Arbitrage**
- **Reference Symbols:** NIFTY, BANKNIFTY (calculate spread)
- **Trade Symbol:** FINNIFTY (trade when correlation breaks)

**Use Case 3: Sector Rotation**
- **Reference Symbols:** NIFTYBANK index, NIFTYIT index (monitor sector strength)
- **Trade Symbols:** HDFCBANK, TCS (trade leaders when sector rotates)

### 📊 Template Structure for Multi-Symbol

```json
{
  "template_name": "Cross-Market Straddle",
  "parameters": {
    "reference_symbol": "{{REFERENCE_SYMBOL}}",
    "trade_symbol": "{{TRADE_SYMBOL}}",
    "quantity": "{{QUANTITY}}",
    "iv_threshold": "{{IV_THRESHOLD}}"
  },
  
  "monitoring": {
    "symbols": [
      {
        "id": "REF_1",
        "symbol": "{{REFERENCE_SYMBOL}}",
        "purpose": "monitor",
        "indicators": ["IV"]
      }
    ]
  },
  
  "trading": {
    "symbol": "{{TRADE_SYMBOL}}",
    "legs": [
      {
        "id": "LEG_1",
        "symbol": "{{TRADE_SYMBOL}}",
        "side": "SELL",
        "option_type": "CE"
      },
      {
        "id": "LEG_2",
        "symbol": "{{TRADE_SYMBOL}}",
        "side": "SELL",
        "option_type": "PE"
      }
    ]
  },
  
  "entry_rules": {
    "logic": "AND",
    "conditions": [
      {
        "type": "IV",
        "symbol": "REF_1",  // Reference SENSEX IV
        "operator": ">",
        "value": "{{IV_THRESHOLD}}"
      }
    ]
  }
}
```

### 🎨 UI for Multi-Symbol Input

```
┌────────────────────────────────────────────┐
│  Deploy: Cross-Market Straddle             │
│  Step 2 of 4: Symbol Configuration         │
├────────────────────────────────────────────┤
│                                            │
│  ┌─ Monitoring Symbols ──────────────────┐│
│  │                                         ││
│  │  Reference Symbol (for IV monitoring): ││
│  │  [SENSEX                         ▼]    ││
│  │  ℹ Symbol to monitor for entry signal  ││
│  │                                         ││
│  └─────────────────────────────────────────┘│
│                                            │
│  ┌─ Trading Symbols ─────────────────────┐│
│  │                                         ││
│  │  Trade Symbol (execute straddle):      ││
│  │  [NIFTY                          ▼]    ││
│  │  ℹ Symbol to actually trade            ││
│  │                                         ││
│  │  Lot Quantity: [50               ↑↓]   ││
│  │                                         ││
│  └─────────────────────────────────────────┘│
│                                            │
│           [< Back]  [Next >]               │
└────────────────────────────────────────────┘
```

### 📝 Advanced: Multiple References + Multiple Trades

**Template for Index Spread + Multi-Leg Trade:**

```json
{
  "template_name": "Spread Breakout Multi-Leg",
  "parameters": {
    "ref_symbols": [
      {
        "id": "REF_1",
        "symbol": "{{SYMBOL_1}}",
        "weight": 1.0
      },
      {
        "id": "REF_2",
        "symbol": "{{SYMBOL_2}}",
        "weight": -1.0
      }
    ],
    "trade_symbols": [
      {
        "id": "TRADE_1",
        "symbol": "{{TRADE_SYMBOL_1}}",
        "quantity": "{{QTY_1}}"
      },
      {
        "id": "TRADE_2",
        "symbol": "{{TRADE_SYMBOL_2}}",
        "quantity": "{{QTY_2}}"
      }
    ]
  },
  
  "entry_rules": {
    "logic": "AND",
    "conditions": [
      {
        "type": "SymbolSpread",
        "symbols": ["REF_1", "REF_2"],
        "operator": ">",
        "value": "{{SPREAD_THRESHOLD}}"
      }
    ]
  }
}
```

**UI for Dynamic Symbol Arrays:**

```
┌────────────────────────────────────────────┐
│  Deploy: Spread Breakout Multi-Leg         │
│  Step 2 of 4: Symbol Configuration         │
├────────────────────────────────────────────┤
│                                            │
│  ┌─ Reference Symbols (for Spread) ──────┐│
│  │                                         ││
│  │  Symbol 1: [NIFTY        ▼]  Weight: 1 ││
│  │  Symbol 2: [BANKNIFTY    ▼]  Weight:-1 ││
│  │                                         ││
│  │  [+ Add Reference Symbol]               ││
│  └─────────────────────────────────────────┘│
│                                            │
│  ┌─ Trade Symbols ───────────────────────┐│
│  │                                         ││
│  │  Symbol 1: [FINNIFTY     ▼]  Qty: 75   ││
│  │  Symbol 2: [MIDCPNIFTY   ▼]  Qty: 50   ││
│  │                                         ││
│  │  [+ Add Trade Symbol]                   ││
│  └─────────────────────────────────────────┘│
│                                            │
│  Spread Threshold: [500              ↑↓]   │
│                                            │
│           [< Back]  [Next >]               │
└────────────────────────────────────────────┘
```

### 🛠️ Implementation: Dynamic Symbol Arrays

```cpp
// In ParametersPage

class SymbolArrayWidget : public QWidget {
  Q_OBJECT
  
public:
  SymbolArrayWidget(const QString &label, QWidget *parent = nullptr) {
    auto *layout = new QVBoxLayout(this);
    
    auto *headerLayout = new QHBoxLayout;
    headerLayout->addWidget(new QLabel(label));
    headerLayout->addStretch();
    
    auto *addBtn = new QPushButton("+ Add Symbol");
    connect(addBtn, &QPushButton::clicked, this, &SymbolArrayWidget::addSymbolRow);
    headerLayout->addWidget(addBtn);
    
    layout->addLayout(headerLayout);
    
    m_symbolsLayout = new QVBoxLayout;
    layout->addLayout(m_symbolsLayout);
    
    // Add one default row
    addSymbolRow();
  }
  
  QVariantList getSymbols() const {
    QVariantList result;
    for (const auto &row : m_symbolRows) {
      QVariantMap symbolData;
      symbolData["symbol"] = row.symbolCombo->currentSymbol();
      symbolData["quantity"] = row.qtySpin->value();
      symbolData["weight"] = row.weightSpin->value();
      result.append(symbolData);
    }
    return result;
  }
  
private slots:
  void addSymbolRow() {
    SymbolRow row;
    row.container = new QWidget;
    auto *layout = new QHBoxLayout(row.container);
    
    layout->addWidget(new QLabel("Symbol:"));
    row.symbolCombo = new SymbolSearchCombo;
    layout->addWidget(row.symbolCombo, 2);
    
    layout->addWidget(new QLabel("Qty:"));
    row.qtySpin = new QSpinBox;
    row.qtySpin->setRange(1, 10000);
    row.qtySpin->setValue(50);
    layout->addWidget(row.qtySpin, 1);
    
    layout->addWidget(new QLabel("Weight:"));
    row.weightSpin = new QDoubleSpinBox;
    row.weightSpin->setRange(-10.0, 10.0);
    row.weightSpin->setValue(1.0);
    layout->addWidget(row.weightSpin, 1);
    
    auto *removeBtn = new QPushButton("×");
    removeBtn->setFixedWidth(30);
    removeBtn->setStyleSheet("color: red;");
    connect(removeBtn, &QPushButton::clicked, [this, row]() {
      removeSymbolRow(row);
    });
    layout->addWidget(removeBtn);
    
    m_symbolsLayout->addWidget(row.container);
    m_symbolRows.append(row);
  }
  
  void removeSymbolRow(const SymbolRow &row) {
    if (m_symbolRows.size() <= 1) {
      QMessageBox::warning(this, "Cannot Remove", "At least one symbol is required.");
      return;
    }
    
    m_symbolsLayout->removeWidget(row.container);
    row.container->deleteLater();
    m_symbolRows.removeAll(row);
  }
  
  struct SymbolRow {
    QWidget *container;
    SymbolSearchCombo *symbolCombo;
    QSpinBox *qtySpin;
    QDoubleSpinBox *weightSpin;
    
    bool operator==(const SymbolRow &other) const {
      return container == other.container;
    }
  };
  
  QVBoxLayout *m_symbolsLayout;
  QVector<SymbolRow> m_symbolRows;
};
```

---

## Code Implementation

### 📦 Complete Deployment Flow

```cpp
// Main deployment function in TemplateService

qint64 TemplateService::deployTemplate(
  const QString &templateId,
  const QVariantMap &deployParams,
  const QString &instanceName
) {
  // 1. Load template
  StrategyTemplate tmpl = loadTemplate(templateId);
  if (!tmpl.isValid()) {
    qWarning() << "Invalid template:" << templateId;
    return -1;
  }
  
  // 2. Validate parameters
  QString validationError;
  if (!validateDeployParams(templateId, deployParams, validationError)) {
    qWarning() << "Validation failed:" << validationError;
    return -1;
  }
  
  // 3. Substitute variables in template
  QJsonDocument templateDoc = QJsonDocument::fromJson(tmpl.definitionJson.toUtf8());
  QJsonObject substitutedDef = TemplateEngine::substituteJSON(
    templateDoc.object(),
    deployParams
  );
  
  // 4. Create instance
  QVariantMap instanceParams;
  instanceParams["definition"] = QJsonDocument(substitutedDef).toJson(QJsonDocument::Compact);
  instanceParams["template_id"] = templateId;
  instanceParams["deployed_params"] = deployParams;
  
  // 5. Call StrategyService to create instance
  qint64 instanceId = StrategyService::instance().createInstance(
    instanceName.isEmpty() ? generateInstanceName(tmpl, deployParams) : instanceName,
    tmpl.description,
    "Custom",  // strategyType
    deployParams.value("SYMBOL", deployParams.value("symbol")).toString(),
    deployParams.value("ACCOUNT", deployParams.value("account")).toString(),
    deployParams.value("SEGMENT", deployParams.value("segment", 2)).toInt(),
    deployParams.value("STOP_LOSS", deployParams.value("stopLoss", 2.0)).toDouble(),
    deployParams.value("TARGET", deployParams.value("target", 3.0)).toDouble(),
    0.0,  // entryPrice (auto)
    deployParams.value("QUANTITY", deployParams.value("quantity", 50)).toInt(),
    instanceParams
  );
  
  // 6. Update template statistics
  if (instanceId > 0) {
    QMutexLocker locker(&m_mutex);
    incrementDeploymentCount(templateId);
  }
  
  return instanceId;
}

QString TemplateService::generateInstanceName(
  const StrategyTemplate &tmpl,
  const QVariantMap &params
) {
  QString symbol = params.value("SYMBOL", params.value("symbol")).toString();
  QString baseName = tmpl.templateName.replace(" ", "_");
  int count = countInstancesForTemplate(tmpl.templateId);
  
  return QString("%1_%2_%3")
    .arg(symbol)
    .arg(baseName)
    .arg(count + 1);
}
```

---

## Complete Integration Flow

### 🔄 End-to-End User Journey

```
┌─────────────────────────────────────────────────────────────┐
│                      USER JOURNEY                           │
└─────────────────────────────────────────────────────────────┘

STEP 1: Open Strategy Manager
  │
  ├─ User: Click "Strategy Manager" button in main window
  │
  └─> StrategyManagerWindow opens with two tabs:
      • Templates (NEW)
      • Instances (existing)

STEP 2: Browse Templates
  │
  ├─ User: Switches to "Templates" tab
  ├─ UI shows: My Templates + Community Templates
  ├─ User: Searches for "Straddle"
  │
  └─> Finds "ATM Straddle on High IV" template

STEP 3: Deploy Template
  │
  ├─ User: Clicks "Deploy" button on template
  │
  └─> DeployTemplateWizard launches

STEP 4: Fill Parameters (Wizard Page 1)
  │
  ├─ Wizard shows template info
  ├─ User: Enters instance name "NIFTY_Straddle_Today"
  ├─ User: Clicks "Next"
  │
  └─> Moves to Page 2 (Parameters)

STEP 5: Configure Parameters (Wizard Page 2)
  │
  ├─ UI dynamically shows parameter fields:
  │  • Trading Symbol: [NIFTY]
  │  • Lot Quantity: [50]
  │  • IV Threshold: [20]
  │
  ├─ User: Fills values
  ├─ User: Clicks "Next"
  │
  └─> Moves to Page 3 (Deployment)

STEP 6: Deployment Settings (Wizard Page 3)
  │
  ├─ User: Selects Account [RAJ143]
  ├─ User: Selects Segment [NSE F&O]
  ├─ User: Sets Stop Loss [2%], Target [3%]
  ├─ User: Clicks "Next"
  │
  └─> Moves to Page 4 (Preview)

STEP 7: Review & Deploy (Wizard Page 4)
  │
  ├─ UI shows complete preview
  ├─ Validation: ✓ All parameters valid
  ├─ User: Clicks "Deploy"
  │
  └─> TemplateService::deployTemplate() called

STEP 8: Backend Processing
  │
  ├─ TemplateService: Load template
  ├─ TemplateEngine: Substitute {{VARS}} → concrete values
  ├─ StrategyService: Create instance
  ├─ Database: Insert strategy_instances record
  ├─ StrategyFactory: Instantiate CustomStrategy
  │
  └─> Instance created with ID 42

STEP 9: Success Notification
  │
  ├─ UI: Shows success message
  ├─ StrategyManagerWindow: Auto-switches to Instances tab
  ├─ Instance list: Highlights new instance
  │
  └─> User sees "NIFTY_Straddle_Today" in IDLE state

STEP 10: Start Trading
  │
  ├─ User: Clicks "Start" button
  ├─ CustomStrategy: Initializes, subscribes to NIFTY
  ├─ Strategy: Begins monitoring IV
  │
  └─> Status changes to RUNNING, awaits entry signal
```

---

**Document Complete!**

This guide covers:
- ✅ 4 UI solution options
- ✅ Recommended wizard-based approach with complete code
- ✅ Integration with StrategyManagerWindow
- ✅ Multi-symbol support (reference + trade symbols)
- ✅ Dynamic parameter arrays
- ✅ Complete deployment flow
- ✅ End-to-end user journey

**Next Steps:** Choose UI approach and begin implementation!
