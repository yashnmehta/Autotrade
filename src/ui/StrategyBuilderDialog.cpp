#include "ui/StrategyBuilderDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimeEdit>
#include <QVBoxLayout>

// ═══════════════════════════════════════════════════════════
// CONSTRUCTION
// ═══════════════════════════════════════════════════════════

StrategyBuilderDialog::StrategyBuilderDialog(QWidget *parent)
    : QDialog(parent) {
  setWindowTitle("Custom Strategy Builder");
  setMinimumSize(720, 700);
  resize(780, 800);
  setupUI();

  // Wire up live preview updates on any form change
  connect(m_nameEdit, &QLineEdit::textChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(m_symbolEdit, &QLineEdit::textChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);

  updateJSONPreview();
}

StrategyBuilderDialog::~StrategyBuilderDialog() = default;

// ═══════════════════════════════════════════════════════════
// SETUP UI
// ═══════════════════════════════════════════════════════════

void StrategyBuilderDialog::setupUI() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(6);

  // Tab widget for organized sections
  auto *tabs = new QTabWidget(this);

  // ── Tab 1: Strategy Info + Indicators/Legs/Symbols ──
  auto *tab1 = new QWidget;
  auto *tab1Layout = new QVBoxLayout(tab1);
  tab1Layout->addWidget(createInfoSection());
  m_indicatorSection = createIndicatorSection();
  m_legsSection = createLegsSection();
  m_symbolsSection = createSymbolsSection();
  m_legsSection->setVisible(false);
  m_symbolsSection->setVisible(false);
  tab1Layout->addWidget(m_indicatorSection);
  tab1Layout->addWidget(m_legsSection);
  tab1Layout->addWidget(m_symbolsSection);
  tab1Layout->addStretch();
  tabs->addTab(tab1, "Setup");

  // ── Tab 2: Entry & Exit Conditions ──
  auto *tab2 = new QWidget;
  auto *tab2Scroll = new QScrollArea;
  tab2Scroll->setWidgetResizable(true);
  auto *tab2Inner = new QWidget;
  auto *tab2Layout = new QVBoxLayout(tab2Inner);
  tab2Layout->addWidget(createConditionsSection("Long Entry Conditions", true));
  tab2Layout->addWidget(
      createConditionsSection("Long Exit Conditions", false));
  tab2Layout->addStretch();
  tab2Scroll->setWidget(tab2Inner);
  auto *tab2OuterLayout = new QVBoxLayout(tab2);
  tab2OuterLayout->setContentsMargins(0, 0, 0, 0);
  tab2OuterLayout->addWidget(tab2Scroll);
  tabs->addTab(tab2, "Conditions");

  // ── Tab 3: Risk Management ──
  auto *tab3 = new QWidget;
  auto *tab3Layout = new QVBoxLayout(tab3);
  tab3Layout->addWidget(createRiskSection());
  tab3Layout->addStretch();
  tabs->addTab(tab3, "Risk");

  // ── Tab 4: JSON Preview ──
  auto *tab4 = new QWidget;
  auto *tab4Layout = new QVBoxLayout(tab4);
  tab4Layout->addWidget(createPreviewSection());
  tabs->addTab(tab4, "Preview");

  mainLayout->addWidget(tabs);

  // ── Validation label ──
  m_validationLabel = new QLabel(this);
  m_validationLabel->setStyleSheet("color: #ef5350; font-weight: bold;");
  m_validationLabel->setWordWrap(true);
  mainLayout->addWidget(m_validationLabel);

  // ── Button row ──
  auto *btnLayout = new QHBoxLayout;
  auto *validateBtn = new QPushButton("Validate", this);
  auto *deployBtn = new QPushButton("Deploy", this);
  auto *cancelBtn = new QPushButton("Cancel", this);

  deployBtn->setDefault(true);
  deployBtn->setStyleSheet(
      "QPushButton { background-color: #2e7d32; color: white; padding: 6px "
      "16px; font-weight: bold; } QPushButton:hover { background-color: "
      "#388e3c; }");

  btnLayout->addStretch();
  btnLayout->addWidget(validateBtn);
  btnLayout->addWidget(deployBtn);
  btnLayout->addWidget(cancelBtn);
  mainLayout->addLayout(btnLayout);

  connect(validateBtn, &QPushButton::clicked, this,
          &StrategyBuilderDialog::onValidateClicked);
  connect(deployBtn, &QPushButton::clicked, this,
          &StrategyBuilderDialog::accept);
  connect(cancelBtn, &QPushButton::clicked, this,
          &StrategyBuilderDialog::reject);

  // Mode switching
  connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &StrategyBuilderDialog::onModeChanged);
}

// ═══════════════════════════════════════════════════════════
// SECTION: Strategy Info
// ═══════════════════════════════════════════════════════════

QWidget *StrategyBuilderDialog::createInfoSection() {
  auto *group = new QGroupBox("Strategy Info", this);
  auto *grid = new QVBoxLayout(group);

  // Row 0: Strategy Mode
  auto *modeRow = new QHBoxLayout;
  modeRow->addWidget(new QLabel("Strategy Mode:"));
  m_modeCombo = new QComboBox;
  m_modeCombo->addItems({"Indicator-Based", "Options Strategy", "Multi-Symbol"});
  m_modeCombo->setToolTip(
      "Indicator-Based: SMA/RSI/MACD triggers\n"
      "Options Strategy: multi-leg with premium/IV/spot triggers\n"
      "Multi-Symbol: pair/basket trading with multiple symbols");
  m_modeCombo->setStyleSheet(
      "QComboBox { font-weight: bold; padding: 4px 8px; }");
  modeRow->addWidget(m_modeCombo);
  modeRow->addStretch();
  grid->addLayout(modeRow);

  // Row 1: Name + Symbol
  auto *row1 = new QHBoxLayout;
  row1->addWidget(new QLabel("Name:"));
  m_nameEdit = new QLineEdit;
  m_nameEdit->setPlaceholderText("My RSI Strategy");
  row1->addWidget(m_nameEdit);

  row1->addWidget(new QLabel("Symbol:"));
  m_symbolEdit = new QLineEdit;
  m_symbolEdit->setPlaceholderText("NIFTY, RELIANCE, etc.");
  m_symbolEdit->setToolTip(
      "Base/reference symbol for strategy.\\n"
      "Options mode: Each leg can override this with its own symbol.\\n"
      "This enables cross-market strategies (e.g., monitor SENSEX IV, trade NIFTY)");
  row1->addWidget(m_symbolEdit);
  grid->addLayout(row1);

  // Row 2: Account + Segment + Timeframe + Product
  auto *row2 = new QHBoxLayout;
  row2->addWidget(new QLabel("Account:"));
  m_accountEdit = new QLineEdit;
  m_accountEdit->setPlaceholderText("Client ID");
  row2->addWidget(m_accountEdit);

  row2->addWidget(new QLabel("Segment:"));
  m_segmentCombo = new QComboBox;
  m_segmentCombo->addItem("NSE Cash (CM)", 1);
  m_segmentCombo->addItem("NSE F&O", 2);
  m_segmentCombo->addItem("BSE Cash (CM)", 11);
  m_segmentCombo->addItem("BSE F&O", 12);
  m_segmentCombo->setCurrentIndex(1); // Default: NSE F&O
  row2->addWidget(m_segmentCombo);

  row2->addWidget(new QLabel("Timeframe:"));
  m_timeframeCombo = new QComboBox;
  m_timeframeCombo->addItems(
      {"1m", "3m", "5m", "10m", "15m", "30m", "1h", "4h", "1D", "1W"});
  m_timeframeCombo->setCurrentIndex(0); // 1m default
  row2->addWidget(m_timeframeCombo);

  row2->addWidget(new QLabel("Product:"));
  m_productCombo = new QComboBox;
  m_productCombo->addItems({"MIS", "NRML", "CNC"});
  row2->addWidget(m_productCombo);

  grid->addLayout(row2);
  return group;
}

// ═══════════════════════════════════════════════════════════
// SECTION: Indicators
// ═══════════════════════════════════════════════════════════

QWidget *StrategyBuilderDialog::createIndicatorSection() {
  auto *group = new QGroupBox("Indicators", this);
  auto *outerLayout = new QVBoxLayout(group);

  // Header with Add button
  auto *headerLayout = new QHBoxLayout;
  headerLayout->addWidget(
      new QLabel("Configure indicators used in conditions:"));
  headerLayout->addStretch();
  auto *addBtn = new QPushButton("+ Add Indicator");
  addBtn->setStyleSheet("color: #26a69a; font-weight: bold;");
  connect(addBtn, &QPushButton::clicked, this,
          &StrategyBuilderDialog::addIndicator);
  headerLayout->addWidget(addBtn);
  outerLayout->addLayout(headerLayout);

  // Column headers
  auto *colHeader = new QHBoxLayout;
  colHeader->addWidget(new QLabel("ID"), 2);
  colHeader->addWidget(new QLabel("Type"), 2);
  colHeader->addWidget(new QLabel("Period"), 1);
  colHeader->addWidget(new QLabel("Period2"), 1);
  colHeader->addWidget(new QLabel(""), 0); // delete button
  outerLayout->addLayout(colHeader);

  // Scrollable indicator list
  m_indicatorLayout = new QVBoxLayout;
  outerLayout->addLayout(m_indicatorLayout);

  // Add two default indicators (SMA_20 and RSI_14)
  addIndicator();
  if (!m_indicators.isEmpty()) {
    m_indicators.last().idEdit->setText("SMA_20");
    m_indicators.last().typeCombo->setCurrentText("SMA");
    m_indicators.last().periodSpin->setValue(20);
  }
  addIndicator();
  if (m_indicators.size() >= 2) {
    m_indicators.last().idEdit->setText("RSI_14");
    m_indicators.last().typeCombo->setCurrentText("RSI");
    m_indicators.last().periodSpin->setValue(14);
  }

  return group;
}

void StrategyBuilderDialog::addIndicator() {
  IndicatorRow row;
  row.container = new QWidget;
  auto *layout = new QHBoxLayout(row.container);
  layout->setContentsMargins(0, 2, 0, 2);

  row.idEdit = new QLineEdit;
  row.idEdit->setPlaceholderText("e.g. SMA_20");
  layout->addWidget(row.idEdit, 2);

  row.typeCombo = new QComboBox;
  row.typeCombo->addItems({"SMA", "EMA", "RSI", "MACD", "BollingerBands",
                           "ATR", "Stochastic", "ADX", "OBV", "Volume"});
  layout->addWidget(row.typeCombo, 2);

  // Auto-generate ID when type or period changes
  row.periodSpin = new QSpinBox;
  row.periodSpin->setRange(1, 500);
  row.periodSpin->setValue(14);
  layout->addWidget(row.periodSpin, 1);

  row.period2Spin = new QSpinBox;
  row.period2Spin->setRange(0, 200);
  row.period2Spin->setValue(0);
  row.period2Spin->setToolTip("Signal period (MACD) or StdDev (BB)");
  layout->addWidget(row.period2Spin, 1);

  auto *delBtn = new QPushButton("×");
  delBtn->setFixedWidth(28);
  delBtn->setStyleSheet("color: #ef5350; font-weight: bold;");
  connect(delBtn, &QPushButton::clicked, this,
          &StrategyBuilderDialog::removeIndicator);
  layout->addWidget(delBtn);

  // Auto-generate ID on type/period change
  connect(row.typeCombo, &QComboBox::currentTextChanged, this,
          [row, this](const QString &type) {
            row.idEdit->setText(
                QString("%1_%2").arg(type).arg(row.periodSpin->value()));
            updateJSONPreview();
          });
  connect(row.periodSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [row, this](int val) {
            row.idEdit->setText(QString("%1_%2")
                                    .arg(row.typeCombo->currentText())
                                    .arg(val));
            updateJSONPreview();
          });

  m_indicatorLayout->addWidget(row.container);
  m_indicators.append(row);
  updateJSONPreview();
}

void StrategyBuilderDialog::removeIndicator() {
  auto *btn = qobject_cast<QPushButton *>(sender());
  if (!btn) return;

  for (int i = 0; i < m_indicators.size(); ++i) {
    if (m_indicators[i].container->isAncestorOf(btn)) {
      m_indicatorLayout->removeWidget(m_indicators[i].container);
      m_indicators[i].container->deleteLater();
      m_indicators.removeAt(i);
      refreshConditionCombos();
      updateJSONPreview();
      return;
    }
  }
}

// ═══════════════════════════════════════════════════════════
// MODE SWITCHING
// ═══════════════════════════════════════════════════════════

void StrategyBuilderDialog::onModeChanged(int index) {
  bool isOptions = (index == 1);
  bool isMultiSymbol = (index == 2);
  if (m_indicatorSection) m_indicatorSection->setVisible(index == 0);
  if (m_legsSection) m_legsSection->setVisible(isOptions);
  if (m_symbolsSection) m_symbolsSection->setVisible(isMultiSymbol);

  // Auto-add defaults
  if (isOptions && m_legs.isEmpty()) {
    addLeg();
    if (!m_legs.isEmpty()) {
      m_legs.last().legIdEdit->setText("LEG_1");
      m_legs.last().sideCombo->setCurrentText("SELL");
      m_legs.last().optionTypeCombo->setCurrentText("CE");
    }
    addLeg();
    if (m_legs.size() >= 2) {
      m_legs.last().legIdEdit->setText("LEG_2");
      m_legs.last().sideCombo->setCurrentText("SELL");
      m_legs.last().optionTypeCombo->setCurrentText("PE");
    }
  }

  if (isMultiSymbol && m_symbols.isEmpty()) {
    addSymbol();
    if (!m_symbols.isEmpty()) {
      m_symbols.last().symbolIdEdit->setText("SYM_1");
      m_symbols.last().symbolEdit->setText("NIFTY");
    }
    addSymbol();
    if (m_symbols.size() >= 2) {
      m_symbols.last().symbolIdEdit->setText("SYM_2");
      m_symbols.last().symbolEdit->setText("BANKNIFTY");
    }
  }

  refreshConditionCombos();
  updateJSONPreview();
}

QStringList StrategyBuilderDialog::conditionTypesForMode() const {
  if (!m_modeCombo) return {"Indicator", "Price"};
  int idx = m_modeCombo->currentIndex();

  if (idx == 1) {
    // Options Strategy mode
    return {"CombinedPremium", "LegPremium", "SpotPrice", "IV",
            "StraddlePremium", "TotalPnL", "LegPnL", "VIX",
            "Indicator", "Price"};
  } else if (idx == 2) {
    // Multi-Symbol mode
    return {"SymbolPrice", "SymbolDiff", "SymbolRatio", "SymbolSum",
            "SymbolSpread", "SymbolWeightedSum", "Indicator", "Price"};
  }
  // Indicator-Based mode (default)
  return {"Indicator", "Price", "PriceVsIndicator"};
}

void StrategyBuilderDialog::refreshConditionCombos() {
  QStringList types = conditionTypesForMode();
  if (!m_modeCombo) return;
  int mode = m_modeCombo->currentIndex();

  QStringList refs;
  if (mode == 1) {
    // Options: leg IDs
    for (const auto &leg : m_legs)
      refs << leg.legIdEdit->text().trimmed();
    refs << "ALL";
  } else if (mode == 2) {
    // Multi-Symbol: symbol IDs
    for (const auto &sym : m_symbols)
      refs << sym.symbolIdEdit->text().trimmed();
  } else {
    // Indicator: indicator IDs
    for (const auto &ind : m_indicators)
      refs << ind.idEdit->text().trimmed();
    refs << "LTP";
  }

  auto updateConds = [&](QVector<ConditionRow> &conditions) {
    for (auto &cond : conditions) {
      QString curType = cond.typeCombo->currentText();
      cond.typeCombo->blockSignals(true);
      cond.typeCombo->clear();
      cond.typeCombo->addItems(types);
      int idx = cond.typeCombo->findText(curType);
      if (idx >= 0) cond.typeCombo->setCurrentIndex(idx);
      cond.typeCombo->blockSignals(false);

      QString curRef = cond.indicatorCombo->currentText();
      cond.indicatorCombo->blockSignals(true);
      cond.indicatorCombo->clear();
      cond.indicatorCombo->addItems(refs);
      int ridx = cond.indicatorCombo->findText(curRef);
      if (ridx >= 0) cond.indicatorCombo->setCurrentIndex(ridx);
      cond.indicatorCombo->blockSignals(false);
    }
  };

  updateConds(m_entryConditions);
  updateConds(m_exitConditions);
}

// ═══════════════════════════════════════════════════════════
// SECTION: Option Legs
// ═══════════════════════════════════════════════════════════

QWidget *StrategyBuilderDialog::createLegsSection() {
  auto *group = new QGroupBox("Option Legs", this);
  auto *outerLayout = new QVBoxLayout(group);

  // Header with ATM recalc period + Add button
  auto *headerLayout = new QHBoxLayout;
  auto *headerLabel = new QLabel("Define legs for your options strategy:");
  headerLabel->setToolTip(
      "Each leg can have its own symbol (leave empty to use base symbol).\\n"
      "Example: Base symbol = SENSEX (for IV monitoring), Leg symbol = NIFTY (for trading)");
  headerLayout->addWidget(headerLabel);
  headerLayout->addWidget(new QLabel("ATM Recalc:"));
  m_atmRecalcPeriodSpin = new QSpinBox;
  m_atmRecalcPeriodSpin->setRange(1, 300);
  m_atmRecalcPeriodSpin->setValue(30);
  m_atmRecalcPeriodSpin->setSuffix(" sec");
  m_atmRecalcPeriodSpin->setToolTip(
      "How often to recalculate ATM strike for relative strike legs");
  headerLayout->addWidget(m_atmRecalcPeriodSpin);
  connect(m_atmRecalcPeriodSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &StrategyBuilderDialog::updateJSONPreview);
  headerLayout->addStretch();
  auto *addBtn = new QPushButton("+ Add Leg");
  addBtn->setStyleSheet("color: #26a69a; font-weight: bold;");
  connect(addBtn, &QPushButton::clicked, this,
          &StrategyBuilderDialog::addLeg);
  headerLayout->addWidget(addBtn);
  outerLayout->addLayout(headerLayout);

  // Column headers
  auto *colHeader = new QHBoxLayout;
  colHeader->addWidget(new QLabel("Leg ID"), 1);
  colHeader->addWidget(new QLabel("Symbol"), 1);
  colHeader->addWidget(new QLabel("Side"), 1);
  colHeader->addWidget(new QLabel("Type"), 1);
  colHeader->addWidget(new QLabel("Strike"), 1);
  colHeader->addWidget(new QLabel("Param"), 1);
  colHeader->addWidget(new QLabel("Expiry"), 1);
  colHeader->addWidget(new QLabel("Qty"), 1);
  colHeader->addWidget(new QLabel(""), 0);
  outerLayout->addLayout(colHeader);

  m_legsLayout = new QVBoxLayout;
  outerLayout->addLayout(m_legsLayout);

  return group;
}

void StrategyBuilderDialog::addLeg() {
  LegRow row;
  row.container = new QWidget;
  auto *layout = new QHBoxLayout(row.container);
  layout->setContentsMargins(0, 2, 0, 2);

  // Leg ID
  row.legIdEdit = new QLineEdit;
  row.legIdEdit->setText(QString("LEG_%1").arg(m_legs.size() + 1));
  layout->addWidget(row.legIdEdit, 1);

  // Symbol (optional - defaults to base symbol if empty)
  row.symbolEdit = new QLineEdit;
  row.symbolEdit->setPlaceholderText("Auto");
  row.symbolEdit->setToolTip(
      "Leave empty to use base symbol from Strategy Info.\n"
      "Specify to trade different symbol (e.g., NIFTY when base is SENSEX)");
  layout->addWidget(row.symbolEdit, 1);

  // Side
  row.sideCombo = new QComboBox;
  row.sideCombo->addItems({"BUY", "SELL"});
  layout->addWidget(row.sideCombo, 1);

  // Option type
  row.optionTypeCombo = new QComboBox;
  row.optionTypeCombo->addItems({"CE", "PE", "FUT"});
  layout->addWidget(row.optionTypeCombo, 1);

  // Strike selection mode
  row.strikeModCombo = new QComboBox;
  row.strikeModCombo->addItems({"ATM Relative", "Premium Based", "Fixed"});
  layout->addWidget(row.strikeModCombo, 1);

  // Strike parameter (stacked: ATM offset | target premium | fixed strike)
  row.strikeParamStack = new QStackedWidget;

  // Page 0: ATM Relative → offset spin
  row.atmOffsetSpin = new QSpinBox;
  row.atmOffsetSpin->setRange(-20, 20);
  row.atmOffsetSpin->setValue(0);
  row.atmOffsetSpin->setPrefix("ATM");
  row.atmOffsetSpin->setSpecialValueText("ATM");
  row.strikeParamStack->addWidget(row.atmOffsetSpin);

  // Page 1: Premium Based → premium spin
  row.premiumSpin = new QDoubleSpinBox;
  row.premiumSpin->setRange(0.05, 100000);
  row.premiumSpin->setValue(200);
  row.premiumSpin->setPrefix("₹");
  row.premiumSpin->setSingleStep(10);
  row.strikeParamStack->addWidget(row.premiumSpin);

  // Page 2: Fixed → strike spin
  row.fixedStrikeSpin = new QSpinBox;
  row.fixedStrikeSpin->setRange(100, 200000);
  row.fixedStrikeSpin->setValue(23000);
  row.fixedStrikeSpin->setSingleStep(50);
  row.strikeParamStack->addWidget(row.fixedStrikeSpin);

  row.strikeParamStack->setCurrentIndex(0);
  layout->addWidget(row.strikeParamStack, 1);

  // Switch stacked page on mode change
  connect(row.strikeModCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          row.strikeParamStack, &QStackedWidget::setCurrentIndex);

  // Expiry
  row.expiryCombo = new QComboBox;
  row.expiryCombo->addItems(
      {"Current Weekly", "Next Weekly", "Monthly"});
  layout->addWidget(row.expiryCombo, 1);

  // Quantity
  row.qtySpin = new QSpinBox;
  row.qtySpin->setRange(1, 100000);
  row.qtySpin->setValue(50);
  layout->addWidget(row.qtySpin, 1);

  // Delete button
  auto *delBtn = new QPushButton("×");
  delBtn->setFixedWidth(28);
  delBtn->setStyleSheet("color: #ef5350; font-weight: bold;");
  connect(delBtn, &QPushButton::clicked, this,
          &StrategyBuilderDialog::removeLeg);
  layout->addWidget(delBtn);

  // Connect for live preview
  connect(row.legIdEdit, &QLineEdit::textChanged, this, [this]() {
    refreshConditionCombos();
    updateJSONPreview();
  });
  connect(row.symbolEdit, &QLineEdit::textChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.sideCombo, &QComboBox::currentTextChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.optionTypeCombo, &QComboBox::currentTextChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.strikeModCombo, &QComboBox::currentTextChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.atmOffsetSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.premiumSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &StrategyBuilderDialog::updateJSONPreview);
  connect(row.fixedStrikeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &StrategyBuilderDialog::updateJSONPreview);
  connect(row.expiryCombo, &QComboBox::currentTextChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.qtySpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &StrategyBuilderDialog::updateJSONPreview);

  m_legsLayout->addWidget(row.container);
  m_legs.append(row);
  refreshConditionCombos();
  updateJSONPreview();
}

void StrategyBuilderDialog::removeLeg() {
  auto *btn = qobject_cast<QPushButton *>(sender());
  if (!btn) return;

  for (int i = 0; i < m_legs.size(); ++i) {
    if (m_legs[i].container->isAncestorOf(btn)) {
      m_legsLayout->removeWidget(m_legs[i].container);
      m_legs[i].container->deleteLater();
      m_legs.removeAt(i);
      refreshConditionCombos();
      updateJSONPreview();
      return;
    }
  }
}

// ═══════════════════════════════════════════════════════════
// SECTION: Multi-Symbol
// ═══════════════════════════════════════════════════════════

QWidget *StrategyBuilderDialog::createSymbolsSection() {
  auto *group = new QGroupBox("Multi-Symbol Trading", this);
  auto *outerLayout = new QVBoxLayout(group);

  // Header
  auto *headerLayout = new QHBoxLayout;
  headerLayout->addWidget(
      new QLabel("Define symbols for pair/basket strategies:"));
  headerLayout->addStretch();
  auto *addBtn = new QPushButton("+ Add Symbol");
  addBtn->setStyleSheet("color: #26a69a; font-weight: bold;");
  connect(addBtn, &QPushButton::clicked, this,
          &StrategyBuilderDialog::addSymbol);
  headerLayout->addWidget(addBtn);
  outerLayout->addLayout(headerLayout);

  // Column headers
  auto *colHeader = new QHBoxLayout;
  colHeader->addWidget(new QLabel("Symbol ID"), 1);
  colHeader->addWidget(new QLabel("Symbol"), 2);
  colHeader->addWidget(new QLabel("Segment"), 2);
  colHeader->addWidget(new QLabel("Qty"), 1);
  colHeader->addWidget(new QLabel("Weight"), 1);
  colHeader->addWidget(new QLabel(""), 0);
  outerLayout->addLayout(colHeader);

  m_symbolsLayout = new QVBoxLayout;
  outerLayout->addLayout(m_symbolsLayout);

  return group;
}

void StrategyBuilderDialog::addSymbol() {
  SymbolRow row;
  row.container = new QWidget;
  auto *layout = new QHBoxLayout(row.container);
  layout->setContentsMargins(0, 2, 0, 2);

  // Symbol ID
  row.symbolIdEdit = new QLineEdit;
  row.symbolIdEdit->setText(QString("SYM_%1").arg(m_symbols.size() + 1));
  layout->addWidget(row.symbolIdEdit, 1);

  // Symbol name
  row.symbolEdit = new QLineEdit;
  row.symbolEdit->setPlaceholderText("NIFTY, RELIANCE...");
  layout->addWidget(row.symbolEdit, 2);

  // Segment
  row.segmentCombo = new QComboBox;
  row.segmentCombo->addItem("NSE Cash (CM)", 1);
  row.segmentCombo->addItem("NSE F&O", 2);
  row.segmentCombo->addItem("BSE Cash (CM)", 11);
  row.segmentCombo->addItem("BSE F&O", 12);
  row.segmentCombo->setCurrentIndex(1); // Default NSE F&O
  layout->addWidget(row.segmentCombo, 2);

  // Quantity
  row.qtySpin = new QSpinBox;
  row.qtySpin->setRange(1, 100000);
  row.qtySpin->setValue(50);
  layout->addWidget(row.qtySpin, 1);

  // Weight (for weighted basket strategies)
  row.weightSpin = new QDoubleSpinBox;
  row.weightSpin->setRange(-10.0, 10.0);
  row.weightSpin->setValue(1.0);
  row.weightSpin->setSingleStep(0.1);
  row.weightSpin->setToolTip("Multiplier for weighted strategies (e.g., spread ratio)");
  layout->addWidget(row.weightSpin, 1);

  // Delete button
  auto *delBtn = new QPushButton("×");
  delBtn->setFixedWidth(28);
  delBtn->setStyleSheet("color: #ef5350; font-weight: bold;");
  connect(delBtn, &QPushButton::clicked, this,
          &StrategyBuilderDialog::removeSymbol);
  layout->addWidget(delBtn);

  // Connect for live preview
  connect(row.symbolIdEdit, &QLineEdit::textChanged, this, [this]() {
    refreshConditionCombos();
    updateJSONPreview();
  });
  connect(row.symbolEdit, &QLineEdit::textChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.segmentCombo, &QComboBox::currentTextChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.qtySpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.weightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &StrategyBuilderDialog::updateJSONPreview);

  m_symbolsLayout->addWidget(row.container);
  m_symbols.append(row);
  refreshConditionCombos();
  updateJSONPreview();
}

void StrategyBuilderDialog::removeSymbol() {
  auto *btn = qobject_cast<QPushButton *>(sender());
  if (!btn) return;

  for (int i = 0; i < m_symbols.size(); ++i) {
    if (m_symbols[i].container->isAncestorOf(btn)) {
      m_symbolsLayout->removeWidget(m_symbols[i].container);
      m_symbols[i].container->deleteLater();
      m_symbols.removeAt(i);
      refreshConditionCombos();
      updateJSONPreview();
      return;
    }
  }
}

// ═══════════════════════════════════════════════════════════
// SECTION: Conditions (Entry / Exit)
// ═══════════════════════════════════════════════════════════

QWidget *StrategyBuilderDialog::createConditionsSection(const QString &title,
                                                         bool isEntry) {
  auto *group = new QGroupBox(title, this);
  auto *outerLayout = new QVBoxLayout(group);

  // Logic selector + Add button
  auto *headerLayout = new QHBoxLayout;
  headerLayout->addWidget(new QLabel("Combine with:"));
  auto *logicCombo = new QComboBox;
  logicCombo->addItems({"AND", "OR"});
  headerLayout->addWidget(logicCombo);
  headerLayout->addStretch();

  auto *addBtn = new QPushButton("+ Add Condition");
  addBtn->setStyleSheet("color: #26a69a; font-weight: bold;");
  headerLayout->addWidget(addBtn);
  outerLayout->addLayout(headerLayout);

  // Column headers
  auto *colHeader = new QHBoxLayout;
  colHeader->addWidget(new QLabel("Type"), 2);
  colHeader->addWidget(new QLabel("Left (Indicator)"), 2);
  colHeader->addWidget(new QLabel("Op"), 1);
  colHeader->addWidget(new QLabel("Value / Indicator"), 2);
  colHeader->addWidget(new QLabel(""), 0);
  outerLayout->addLayout(colHeader);

  auto *condLayout = new QVBoxLayout;
  outerLayout->addLayout(condLayout);

  if (isEntry) {
    m_entryLogicCombo = logicCombo;
    m_entryLayout = condLayout;
    connect(addBtn, &QPushButton::clicked, this,
            &StrategyBuilderDialog::addEntryCondition);
    connect(logicCombo, &QComboBox::currentTextChanged, this,
            &StrategyBuilderDialog::updateJSONPreview);
  } else {
    m_exitLogicCombo = logicCombo;
    m_exitLayout = condLayout;
    connect(addBtn, &QPushButton::clicked, this,
            &StrategyBuilderDialog::addExitCondition);
    connect(logicCombo, &QComboBox::currentTextChanged, this,
            &StrategyBuilderDialog::updateJSONPreview);
  }

  return group;
}

void StrategyBuilderDialog::addEntryCondition() {
  ConditionRow row;
  row.container = new QWidget;
  auto *layout = new QHBoxLayout(row.container);
  layout->setContentsMargins(0, 2, 0, 2);

  row.typeCombo = new QComboBox;
  row.typeCombo->addItems(conditionTypesForMode());
  layout->addWidget(row.typeCombo, 2);

  row.indicatorCombo = new QComboBox;
  row.indicatorCombo->setEditable(true);
  if (!m_modeCombo) {
    row.indicatorCombo->setToolTip("Left side: indicator ID or LTP");
  } else {
    int mode = m_modeCombo->currentIndex();
    if (mode == 1) {
      // Options
      for (const auto &leg : m_legs)
        row.indicatorCombo->addItem(leg.legIdEdit->text().trimmed());
      row.indicatorCombo->addItem("ALL");
      row.indicatorCombo->setToolTip("Which leg (for leg-specific types)");
    } else if (mode == 2) {
      // Multi-Symbol
      for (const auto &sym : m_symbols)
        row.indicatorCombo->addItem(sym.symbolIdEdit->text().trimmed());
      row.indicatorCombo->setToolTip("Which symbol (for symbol-specific types)");
    } else {
      // Indicator
      for (const auto &ind : m_indicators)
        row.indicatorCombo->addItem(ind.idEdit->text().trimmed());
      row.indicatorCombo->addItem("LTP");
      row.indicatorCombo->setToolTip("Left side: indicator ID or LTP");
    }
  }
  layout->addWidget(row.indicatorCombo, 2);

  row.operatorCombo = new QComboBox;
  row.operatorCombo->addItems({">", "<", ">=", "<=", "==", "!="});
  layout->addWidget(row.operatorCombo, 1);

  row.valueEdit = new QLineEdit;
  row.valueEdit->setPlaceholderText("30, SMA_20, etc.");
  layout->addWidget(row.valueEdit, 2);

  auto *delBtn = new QPushButton("×");
  delBtn->setFixedWidth(28);
  delBtn->setStyleSheet("color: #ef5350; font-weight: bold;");
  connect(delBtn, &QPushButton::clicked, this,
          &StrategyBuilderDialog::removeEntryCondition);
  layout->addWidget(delBtn);

  // Connect for live preview
  connect(row.typeCombo, &QComboBox::currentTextChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.operatorCombo, &QComboBox::currentTextChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.valueEdit, &QLineEdit::textChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);

  m_entryLayout->addWidget(row.container);
  m_entryConditions.append(row);
  updateJSONPreview();
}

void StrategyBuilderDialog::removeEntryCondition() {
  auto *btn = qobject_cast<QPushButton *>(sender());
  if (!btn) return;

  for (int i = 0; i < m_entryConditions.size(); ++i) {
    if (m_entryConditions[i].container->isAncestorOf(btn)) {
      m_entryLayout->removeWidget(m_entryConditions[i].container);
      m_entryConditions[i].container->deleteLater();
      m_entryConditions.removeAt(i);
      updateJSONPreview();
      return;
    }
  }
}

void StrategyBuilderDialog::addExitCondition() {
  ConditionRow row;
  row.container = new QWidget;
  auto *layout = new QHBoxLayout(row.container);
  layout->setContentsMargins(0, 2, 0, 2);

  row.typeCombo = new QComboBox;
  row.typeCombo->addItems(conditionTypesForMode());
  layout->addWidget(row.typeCombo, 2);

  row.indicatorCombo = new QComboBox;
  row.indicatorCombo->setEditable(true);
  if (!m_modeCombo) {
    row.indicatorCombo->setToolTip("Left side: indicator ID or LTP");
  } else {
    int mode = m_modeCombo->currentIndex();
    if (mode == 1) {
      // Options
      for (const auto &leg : m_legs)
        row.indicatorCombo->addItem(leg.legIdEdit->text().trimmed());
      row.indicatorCombo->addItem("ALL");
      row.indicatorCombo->setToolTip("Which leg (for leg-specific types)");
    } else if (mode == 2) {
      // Multi-Symbol
      for (const auto &sym : m_symbols)
        row.indicatorCombo->addItem(sym.symbolIdEdit->text().trimmed());
      row.indicatorCombo->setToolTip("Which symbol (for symbol-specific types)");
    } else {
      // Indicator
      for (const auto &ind : m_indicators)
        row.indicatorCombo->addItem(ind.idEdit->text().trimmed());
      row.indicatorCombo->addItem("LTP");
      row.indicatorCombo->setToolTip("Left side: indicator ID or LTP");
    }
  }
  layout->addWidget(row.indicatorCombo, 2);

  row.operatorCombo = new QComboBox;
  row.operatorCombo->addItems({">", "<", ">=", "<=", "==", "!="});
  layout->addWidget(row.operatorCombo, 1);

  row.valueEdit = new QLineEdit;
  row.valueEdit->setPlaceholderText("70, EMA_50, etc.");
  layout->addWidget(row.valueEdit, 2);

  auto *delBtn = new QPushButton("×");
  delBtn->setFixedWidth(28);
  delBtn->setStyleSheet("color: #ef5350; font-weight: bold;");
  connect(delBtn, &QPushButton::clicked, this,
          &StrategyBuilderDialog::removeExitCondition);
  layout->addWidget(delBtn);

  connect(row.typeCombo, &QComboBox::currentTextChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.operatorCombo, &QComboBox::currentTextChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(row.valueEdit, &QLineEdit::textChanged, this,
          &StrategyBuilderDialog::updateJSONPreview);

  m_exitLayout->addWidget(row.container);
  m_exitConditions.append(row);
  updateJSONPreview();
}

void StrategyBuilderDialog::removeExitCondition() {
  auto *btn = qobject_cast<QPushButton *>(sender());
  if (!btn) return;

  for (int i = 0; i < m_exitConditions.size(); ++i) {
    if (m_exitConditions[i].container->isAncestorOf(btn)) {
      m_exitLayout->removeWidget(m_exitConditions[i].container);
      m_exitConditions[i].container->deleteLater();
      m_exitConditions.removeAt(i);
      updateJSONPreview();
      return;
    }
  }
}

// ═══════════════════════════════════════════════════════════
// SECTION: Risk Management
// ═══════════════════════════════════════════════════════════

QWidget *StrategyBuilderDialog::createRiskSection() {
  auto *group = new QGroupBox("Risk Management", this);
  auto *grid = new QVBoxLayout(group);

  // Row 1: SL, Target, Position Size
  auto *row1 = new QHBoxLayout;
  row1->addWidget(new QLabel("Stop Loss %:"));
  m_slSpin = new QDoubleSpinBox;
  m_slSpin->setRange(0.1, 50.0);
  m_slSpin->setValue(2.0);
  m_slSpin->setSingleStep(0.5);
  row1->addWidget(m_slSpin);

  row1->addWidget(new QLabel("Target %:"));
  m_targetSpin = new QDoubleSpinBox;
  m_targetSpin->setRange(0.1, 100.0);
  m_targetSpin->setValue(3.0);
  m_targetSpin->setSingleStep(0.5);
  row1->addWidget(m_targetSpin);

  row1->addWidget(new QLabel("Qty:"));
  m_positionSizeSpin = new QSpinBox;
  m_positionSizeSpin->setRange(1, 100000);
  m_positionSizeSpin->setValue(50);
  row1->addWidget(m_positionSizeSpin);
  grid->addLayout(row1);

  // Row 2: Max Positions, Max Daily Trades, Max Daily Loss
  auto *row2 = new QHBoxLayout;
  row2->addWidget(new QLabel("Max Positions:"));
  m_maxPositionsSpin = new QSpinBox;
  m_maxPositionsSpin->setRange(1, 100);
  m_maxPositionsSpin->setValue(1);
  row2->addWidget(m_maxPositionsSpin);

  row2->addWidget(new QLabel("Max Daily Trades:"));
  m_maxDailyTradesSpin = new QSpinBox;
  m_maxDailyTradesSpin->setRange(1, 1000);
  m_maxDailyTradesSpin->setValue(10);
  row2->addWidget(m_maxDailyTradesSpin);

  row2->addWidget(new QLabel("Max Daily Loss ₹:"));
  m_maxDailyLossSpin = new QDoubleSpinBox;
  m_maxDailyLossSpin->setRange(100, 10000000);
  m_maxDailyLossSpin->setValue(5000);
  m_maxDailyLossSpin->setSingleStep(500);
  row2->addWidget(m_maxDailyLossSpin);
  grid->addLayout(row2);

  // Row 3: Trailing Stop
  auto *row3 = new QHBoxLayout;
  m_trailingCheck = new QCheckBox("Trailing Stop");
  row3->addWidget(m_trailingCheck);

  row3->addWidget(new QLabel("Trigger %:"));
  m_trailingTriggerSpin = new QDoubleSpinBox;
  m_trailingTriggerSpin->setRange(0.1, 50.0);
  m_trailingTriggerSpin->setValue(1.5);
  m_trailingTriggerSpin->setEnabled(false);
  row3->addWidget(m_trailingTriggerSpin);

  row3->addWidget(new QLabel("Trail %:"));
  m_trailingAmountSpin = new QDoubleSpinBox;
  m_trailingAmountSpin->setRange(0.1, 50.0);
  m_trailingAmountSpin->setValue(0.5);
  m_trailingAmountSpin->setEnabled(false);
  row3->addWidget(m_trailingAmountSpin);

  connect(m_trailingCheck, &QCheckBox::toggled, this, [this](bool on) {
    m_trailingTriggerSpin->setEnabled(on);
    m_trailingAmountSpin->setEnabled(on);
    updateJSONPreview();
  });
  grid->addLayout(row3);

  // Row 4: Time-Based Exit
  auto *row4 = new QHBoxLayout;
  m_timeExitCheck = new QCheckBox("Time-Based Exit");
  row4->addWidget(m_timeExitCheck);

  row4->addWidget(new QLabel("Exit Time:"));
  m_exitTimeEdit = new QTimeEdit(QTime(15, 15));
  m_exitTimeEdit->setDisplayFormat("HH:mm");
  m_exitTimeEdit->setEnabled(false);
  row4->addWidget(m_exitTimeEdit);
  row4->addStretch();

  connect(m_timeExitCheck, &QCheckBox::toggled, this, [this](bool on) {
    m_exitTimeEdit->setEnabled(on);
    updateJSONPreview();
  });
  grid->addLayout(row4);

  // Connect all risk fields to preview update
  connect(m_slSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(m_targetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &StrategyBuilderDialog::updateJSONPreview);
  connect(m_positionSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(m_maxPositionsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &StrategyBuilderDialog::updateJSONPreview);
  connect(m_maxDailyTradesSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &StrategyBuilderDialog::updateJSONPreview);
  connect(m_maxDailyLossSpin,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &StrategyBuilderDialog::updateJSONPreview);

  return group;
}

// ═══════════════════════════════════════════════════════════
// SECTION: JSON Preview
// ═══════════════════════════════════════════════════════════

QWidget *StrategyBuilderDialog::createPreviewSection() {
  auto *widget = new QWidget(this);
  auto *layout = new QVBoxLayout(widget);

  layout->addWidget(new QLabel("Generated Strategy Definition JSON:"));
  m_jsonPreview = new QTextEdit;
  m_jsonPreview->setReadOnly(true);
  m_jsonPreview->setFont(QFont("Consolas", 9));
  m_jsonPreview->setStyleSheet(
      "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
  layout->addWidget(m_jsonPreview);

  return widget;
}

// ═══════════════════════════════════════════════════════════
// JSON GENERATION
// ═══════════════════════════════════════════════════════════

QJsonObject StrategyBuilderDialog::buildJSON() const {
  QJsonObject root;
  root["name"] = m_nameEdit ? m_nameEdit->text().trimmed() : QString();
  root["symbol"] = m_symbolEdit ? m_symbolEdit->text().trimmed() : QString();
  root["timeframe"] = m_timeframeCombo ? m_timeframeCombo->currentText()
                                       : "1m";

  int mode = m_modeCombo ? m_modeCombo->currentIndex() : 0;
  if (mode == 1) {
    root["mode"] = "options";
  } else if (mode == 2) {
    root["mode"] = "multi_symbol";
  } else {
    root["mode"] = "indicator";
  }

  if (mode == 1) {
    // ── Options Mode ──
    QJsonObject optionsConfig;
    optionsConfig["atm_recalc_period_sec"] = 
        m_atmRecalcPeriodSpin ? m_atmRecalcPeriodSpin->value() : 30;
    root["options_config"] = optionsConfig;

    QJsonArray legsArr;
    for (const auto &leg : m_legs) {
      QJsonObject obj;
      obj["id"] = leg.legIdEdit->text().trimmed();
      
      // Per-leg symbol (falls back to base symbol if empty)
      QString legSymbol = leg.symbolEdit->text().trimmed();
      if (legSymbol.isEmpty()) {
        obj["symbol"] = m_symbolEdit ? m_symbolEdit->text().trimmed() : QString();
        obj["uses_base_symbol"] = true;
      } else {
        obj["symbol"] = legSymbol;
        obj["uses_base_symbol"] = false;
      }
      
      obj["side"] = leg.sideCombo->currentText();
      obj["option_type"] = leg.optionTypeCombo->currentText();
      obj["quantity"] = leg.qtySpin->value();
      obj["expiry"] = leg.expiryCombo->currentText();

      int strikeIdx = leg.strikeModCombo->currentIndex();
      if (strikeIdx == 0) {
        obj["strike_selection"] = "atm_relative";
        obj["atm_offset"] = leg.atmOffsetSpin->value();
      } else if (strikeIdx == 1) {
        obj["strike_selection"] = "premium_based";
        obj["target_premium"] = leg.premiumSpin->value();
      } else {
        obj["strike_selection"] = "fixed";
        obj["fixed_strike"] = leg.fixedStrikeSpin->value();
      }
      legsArr.append(obj);
    }
    root["legs"] = legsArr;
  } else if (mode == 2) {
    // ── Multi-Symbol Mode ──
    QJsonArray symbolsArr;
    for (const auto &sym : m_symbols) {
      QJsonObject obj;
      obj["id"] = sym.symbolIdEdit->text().trimmed();
      obj["symbol"] = sym.symbolEdit->text().trimmed();
      obj["segment"] = sym.segmentCombo->currentData().toInt();
      obj["quantity"] = sym.qtySpin->value();
      obj["weight"] = sym.weightSpin->value();
      symbolsArr.append(obj);
    }
    root["symbols"] = symbolsArr;
  } else {
    // ── Indicator Mode ──
    QJsonArray indicatorsArr;
    for (const auto &ind : m_indicators) {
      QJsonObject obj;
      obj["id"] = ind.idEdit->text().trimmed();
      obj["type"] = ind.typeCombo->currentText();
      obj["period"] = ind.periodSpin->value();
      if (ind.period2Spin->value() > 0) {
        obj["period2"] = ind.period2Spin->value();
      }
      indicatorsArr.append(obj);
    }
    root["indicators"] = indicatorsArr;
  }

  // Entry rules
  root["entry_rules"] = buildConditionGroupJSON(true);

  // Exit rules
  root["exit_rules"] = buildConditionGroupJSON(false);

  // Risk management
  QJsonObject risk;
  risk["stop_loss_percent"] = m_slSpin->value();
  risk["target_percent"] = m_targetSpin->value();
  risk["position_size"] = m_positionSizeSpin->value();
  risk["max_positions"] = m_maxPositionsSpin->value();
  risk["max_daily_trades"] = m_maxDailyTradesSpin->value();
  risk["max_daily_loss"] = m_maxDailyLossSpin->value();

  if (m_trailingCheck->isChecked()) {
    risk["trailing_stop_enabled"] = true;
    risk["trailing_trigger_percent"] = m_trailingTriggerSpin->value();
    risk["trailing_amount_percent"] = m_trailingAmountSpin->value();
  } else {
    risk["trailing_stop_enabled"] = false;
  }

  if (m_timeExitCheck->isChecked()) {
    risk["time_based_exit"] = true;
    risk["exit_time"] = m_exitTimeEdit->time().toString("HH:mm");
  } else {
    risk["time_based_exit"] = false;
  }
  root["risk_management"] = risk;

  return root;
}

QJsonObject StrategyBuilderDialog::buildConditionGroupJSON(bool isEntry) const {
  const auto &conditions = isEntry ? m_entryConditions : m_exitConditions;
  const auto *logicCombo = isEntry ? m_entryLogicCombo : m_exitLogicCombo;

  QJsonObject group;
  group["logic"] = logicCombo ? logicCombo->currentText() : "AND";

  QJsonArray arr;
  for (const auto &cond : conditions) {
    QJsonObject obj;
    QString type = cond.typeCombo->currentText();
    obj["type"] = type;

    // Reference field: indicator ID, leg ID, or symbol ID
    QString ref = cond.indicatorCombo->currentText().trimmed();
    if (!ref.isEmpty() && ref != "ALL") {
      if (type == "Indicator" || type == "PriceVsIndicator") {
        obj["indicator"] = ref;
      } else if (type == "LegPremium" || type == "IV" || type == "LegPnL") {
        obj["leg"] = ref;
      } else if (type == "SymbolPrice" || type == "SymbolDiff" || 
                 type == "SymbolRatio" || type == "SymbolSpread") {
        obj["symbol"] = ref;
      }
    }

    obj["operator"] = cond.operatorCombo->currentText();

    // Value: try numeric first, else string (indicator reference)
    QString valStr = cond.valueEdit->text().trimmed();
    bool ok;
    double numVal = valStr.toDouble(&ok);
    if (ok) {
      obj["value"] = numVal;
    } else {
      obj["value"] = valStr;
    }

    arr.append(obj);
  }
  group["conditions"] = arr;

  return group;
}

void StrategyBuilderDialog::updateJSONPreview() {
  if (!m_jsonPreview) return;  // Not yet created during setupUI()
  QJsonDocument doc(buildJSON());
  m_jsonPreview->setPlainText(
      doc.toJson(QJsonDocument::Indented));
}

// ═══════════════════════════════════════════════════════════
// VALIDATION
// ═══════════════════════════════════════════════════════════

QString StrategyBuilderDialog::generateValidationErrors() const {
  QStringList errors;

  if (m_nameEdit->text().trimmed().isEmpty())
    errors << "Strategy name is required";

  if (m_symbolEdit->text().trimmed().isEmpty())
    errors << "Trading symbol is required";

  if (m_accountEdit->text().trimmed().isEmpty())
    errors << "Client account is required";

  int mode = m_modeCombo ? m_modeCombo->currentIndex() : 0;

  if (mode == 1) {
    // ── Options mode validation ──
    if (m_legs.isEmpty())
      errors << "At least one option leg is required";

    QSet<QString> legIds;
    for (const auto &leg : m_legs) {
      QString id = leg.legIdEdit->text().trimmed();
      if (id.isEmpty()) {
        errors << "All legs must have an ID";
        break;
      }
      if (legIds.contains(id))
        errors << QString("Duplicate leg ID: %1").arg(id);
      legIds.insert(id);

      if (leg.qtySpin->value() <= 0)
        errors << QString("Leg %1: quantity must be > 0").arg(id);
    }
  } else if (mode == 2) {
    // ── Multi-Symbol mode validation ──
    if (m_symbols.isEmpty())
      errors << "At least two symbols are required for multi-symbol strategies";

    QSet<QString> symIds;
    for (const auto &sym : m_symbols) {
      QString id = sym.symbolIdEdit->text().trimmed();
      QString symbol = sym.symbolEdit->text().trimmed();
      if (id.isEmpty()) {
        errors << "All symbols must have an ID";
        break;
      }
      if (symbol.isEmpty()) {
        errors << QString("Symbol %1: symbol name is required").arg(id);
      }
      if (symIds.contains(id))
        errors << QString("Duplicate symbol ID: %1").arg(id);
      symIds.insert(id);

      if (sym.qtySpin->value() <= 0)
        errors << QString("Symbol %1: quantity must be > 0").arg(id);
    }
  } else {
    // ── Indicator mode validation ──
    if (m_indicators.isEmpty())
      errors << "At least one indicator is required";

    QSet<QString> ids;
    for (const auto &ind : m_indicators) {
      QString id = ind.idEdit->text().trimmed();
      if (id.isEmpty()) {
        errors << "All indicators must have an ID";
        break;
      }
      if (ids.contains(id))
        errors << QString("Duplicate indicator ID: %1").arg(id);
      ids.insert(id);
    }
  }

  if (m_entryConditions.isEmpty())
    errors << "At least one entry condition is required";

  for (int i = 0; i < m_entryConditions.size(); ++i) {
    const auto &cond = m_entryConditions[i];
    if (cond.valueEdit->text().trimmed().isEmpty())
      errors << QString("Entry condition %1: value is required").arg(i + 1);
  }

  if (m_slSpin->value() <= 0)
    errors << "Stop loss must be greater than 0";

  if (m_targetSpin->value() <= 0)
    errors << "Target must be greater than 0";

  return errors.join("\n");
}

void StrategyBuilderDialog::onValidateClicked() {
  QString errors = generateValidationErrors();
  if (errors.isEmpty()) {
    m_validationLabel->setStyleSheet("color: #26a69a; font-weight: bold;");
    m_validationLabel->setText("✓ Strategy definition is valid!");
  } else {
    m_validationLabel->setStyleSheet("color: #ef5350; font-weight: bold;");
    m_validationLabel->setText(errors);
  }
}

void StrategyBuilderDialog::accept() {
  QString errors = generateValidationErrors();
  if (!errors.isEmpty()) {
    m_validationLabel->setStyleSheet("color: #ef5350; font-weight: bold;");
    m_validationLabel->setText(errors);
    return;
  }
  QDialog::accept();
}

// ═══════════════════════════════════════════════════════════
// PUBLIC ACCESSORS
// ═══════════════════════════════════════════════════════════

QString StrategyBuilderDialog::definitionJSON() const {
  QJsonDocument doc(buildJSON());
  return doc.toJson(QJsonDocument::Compact);
}

QString StrategyBuilderDialog::strategyName() const {
  return m_nameEdit->text().trimmed();
}

QString StrategyBuilderDialog::symbol() const {
  return m_symbolEdit->text().trimmed();
}

QString StrategyBuilderDialog::account() const {
  return m_accountEdit->text().trimmed();
}

int StrategyBuilderDialog::segment() const {
  return m_segmentCombo->currentData().toInt();
}

double StrategyBuilderDialog::stopLoss() const {
  return m_slSpin->value();
}

double StrategyBuilderDialog::target() const {
  return m_targetSpin->value();
}

int StrategyBuilderDialog::quantity() const {
  return m_positionSizeSpin->value();
}

QString StrategyBuilderDialog::productType() const {
  return m_productCombo->currentText();
}
