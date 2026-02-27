#include "strategy/builder/ParamEditorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

// ═══════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════

ParamEditorDialog::ParamEditorDialog(QWidget *parent)
    : QDialog(parent) {
  buildUi();
  setEditMode(false);
}

// ═══════════════════════════════════════════════════════════════════
// Build the form UI programmatically (no .ui file — this is a small
// focused dialog that's easier to maintain in code)
// ═══════════════════════════════════════════════════════════════════

void ParamEditorDialog::buildUi() {
  setMinimumWidth(640);

  // ── Light theme stylesheet ──
  setStyleSheet(R"(
    ParamEditorDialog {
      background: #ffffff;
      font-family: "Segoe UI", -apple-system, sans-serif;
      font-size: 13px;
      color: #1e293b;
    }
    QGroupBox {
      font-weight: 600;
      font-size: 12px;
      color: #1e293b;
      border: 1px solid #e2e8f0;
      border-radius: 6px;
      margin-top: 14px;
      padding: 14px 10px 10px 10px;
      background: #f8fafc;
    }
    QGroupBox::title {
      subcontrol-origin: margin;
      subcontrol-position: top left;
      left: 12px;
      padding: 0 6px;
      background: #f8fafc;
    }
    QLabel {
      color: #475569;
      font-size: 12px;
    }
    QLineEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
      border: 1px solid #e2e8f0;
      border-radius: 4px;
      padding: 5px 8px;
      background: #ffffff;
      color: #1e293b;
      font-size: 12px;
    }
    QLineEdit:focus, QPlainTextEdit:focus, QSpinBox:focus,
    QDoubleSpinBox:focus, QComboBox:focus {
      border-color: #3b82f6;
    }
    QComboBox::drop-down {
      border: none;
      width: 20px;
    }
    QCheckBox {
      color: #475569;
      font-size: 12px;
      spacing: 6px;
    }
    QPushButton {
      padding: 7px 18px;
      border-radius: 5px;
      font-size: 12px;
      font-weight: 500;
    }
    QPushButton#okBtn {
      background: #2563eb;
      color: white;
      border: none;
    }
    QPushButton#okBtn:hover {
      background: #1d4ed8;
    }
    QPushButton#cancelBtn {
      background: #f1f5f9;
      color: #475569;
      border: 1px solid #e2e8f0;
    }
    QPushButton#cancelBtn:hover {
      background: #e2e8f0;
    }
  )");

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(10);
  mainLayout->setContentsMargins(18, 18, 18, 14);

  // ── Title bar label ──
  auto *titleLabel = new QLabel(this);
  titleLabel->setObjectName("titleLabel");
  titleLabel->setStyleSheet(
      "font-size: 15px; font-weight: 700; color: #1e293b; "
      "padding: 0 0 6px 0; border-bottom: 2px solid #2563eb;");
  titleLabel->setText("Add Parameter");
  mainLayout->addWidget(titleLabel);

  // ═══════════════════════════════════════════════════════════════════
  // Section 1: Identity
  // ═══════════════════════════════════════════════════════════════════

  auto *identityGroup = new QGroupBox("Identity", this);
  auto *idForm = new QFormLayout(identityGroup);
  idForm->setLabelAlignment(Qt::AlignRight);
  idForm->setSpacing(8);

  m_nameEdit = new QLineEdit(identityGroup);
  m_nameEdit->setPlaceholderText("e.g. RSI_PERIOD, SL_LEVEL, IV_THRESHOLD");
  m_nameEdit->setToolTip(
      "Internal name used in formulas and conditions.\n"
      "Convention: UPPER_SNAKE_CASE (e.g. RSI_PERIOD)");
  idForm->addRow("Name:", m_nameEdit);

  m_labelEdit = new QLineEdit(identityGroup);
  m_labelEdit->setPlaceholderText("e.g. RSI Period, Stop Loss Level");
  m_labelEdit->setToolTip("Human-readable label shown to users at deploy time.");
  idForm->addRow("Label:", m_labelEdit);

  m_typeCombo = new QComboBox(identityGroup);
  m_typeCombo->addItems({"Double", "Int", "Bool", "String", "Expression"});
  m_typeCombo->setToolTip(
      "Parameter type:\n"
      "  Double / Int — numeric value set at deploy time\n"
      "  Bool — on/off toggle\n"
      "  String — text value\n"
      "  Expression — formula evaluated live during trading");
  idForm->addRow("Type:", m_typeCombo);
  connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ParamEditorDialog::onTypeChanged);

  mainLayout->addWidget(identityGroup);

  // ═══════════════════════════════════════════════════════════════════
  // Section 2: Fixed Value (shown for Int/Double/Bool/String)
  // ═══════════════════════════════════════════════════════════════════

  m_fixedGroup = new QGroupBox("Default Value", this);
  auto *fixedForm = new QFormLayout(m_fixedGroup);
  fixedForm->setLabelAlignment(Qt::AlignRight);
  fixedForm->setSpacing(8);

  m_defaultEdit = new QLineEdit(m_fixedGroup);
  m_defaultEdit->setPlaceholderText("Default value (e.g. 14, 0.5, true)");
  fixedForm->addRow("Default:", m_defaultEdit);

  // Min / Max on same row
  auto *rangeWidget = new QWidget(m_fixedGroup);
  auto *rangeLay = new QHBoxLayout(rangeWidget);
  rangeLay->setContentsMargins(0, 0, 0, 0);
  rangeLay->setSpacing(8);

  m_minEdit = new QLineEdit(rangeWidget);
  m_minEdit->setPlaceholderText("Min");
  m_minEdit->setFixedWidth(100);
  rangeLay->addWidget(new QLabel("Min:", rangeWidget));
  rangeLay->addWidget(m_minEdit);

  m_maxEdit = new QLineEdit(rangeWidget);
  m_maxEdit->setPlaceholderText("Max");
  m_maxEdit->setFixedWidth(100);
  rangeLay->addWidget(new QLabel("Max:", rangeWidget));
  rangeLay->addWidget(m_maxEdit);
  rangeLay->addStretch();

  fixedForm->addRow("Range:", rangeWidget);
  mainLayout->addWidget(m_fixedGroup);

  // ═══════════════════════════════════════════════════════════════════
  // Section 3: Expression (shown for Expression type)
  // ═══════════════════════════════════════════════════════════════════

  m_exprGroup = new QGroupBox("Formula", this);
  auto *exprLayout = new QVBoxLayout(m_exprGroup);
  exprLayout->setSpacing(6);

  m_formulaEdit = new QPlainTextEdit(m_exprGroup);
  m_formulaEdit->setPlaceholderText(
      "Enter formula...\n"
      "Examples:\n"
      "  LTP(REF_1) + LTP(TRADE_1)\n"
      "  (multiplier1 * DELTA(REF_1)) + multiplier2 * DELTA(TRADE_1)\n"
      "  ATR(REF_1, 14) * 2.5\n"
      "  IV(TRADE_1) > 25 ? 0.02 : 0.01\n"
      "\n"
      "Tip: Click chips below to insert references at cursor");
  m_formulaEdit->setMaximumHeight(100);
  m_formulaEdit->setTabChangesFocus(true);
  exprLayout->addWidget(m_formulaEdit);

  m_formulaHelpLabel = new QLabel(m_exprGroup);
  m_formulaHelpLabel->setWordWrap(true);
  m_formulaHelpLabel->setStyleSheet(
      "background: #f0fdf4; color: #166534; font-size: 10px; "
      "padding: 5px 8px; border: 1px solid #bbf7d0; border-radius: 3px;");
  m_formulaHelpLabel->setText(
      "<b>Functions:</b> "
      "<code>LTP  OPEN  HIGH  LOW  CLOSE  BID  ASK  VOLUME  CHANGE_PCT</code><br/>"
      "<b>Indicators:</b> "
      "<code>RSI(sym, period)  SMA  EMA  ATR  VWAP  BBANDS_UPPER  BBANDS_LOWER  MACD</code><br/>"
      "<b>Greeks:</b> "
      "<code>IV  DELTA  GAMMA  THETA  VEGA</code> · "
      "<b>Portfolio:</b> "
      "<code>MTM()  NET_PREMIUM()  NET_DELTA()</code><br/>"
      "<b>Math:</b> "
      "<code>ABS  MAX  MIN  ROUND  FLOOR  CEIL  SQRT  LOG  POW  CLAMP</code> · "
      "<b>Logic:</b> <code>&& || ! > < == !=</code> · "
      "<b>Ternary:</b> <code>cond ? a : b</code>");
  exprLayout->addWidget(m_formulaHelpLabel);

  mainLayout->addWidget(m_exprGroup);

  // ═══════════════════════════════════════════════════════════════════
  // Section 3b: Reference Palette — clickable chips to insert into formula
  // Shows available symbol slots, other parameters, indicators, and
  // common market-data functions. Click a chip → inserts text at cursor.
  // ═══════════════════════════════════════════════════════════════════

  m_paletteGroup = new QGroupBox("Insert Reference  (click to insert at cursor)", this);
  m_paletteGroup->setStyleSheet(
      m_paletteGroup->styleSheet() +
      "QGroupBox#paletteGroup { background: #f8fafc; }");
  m_paletteGroup->setObjectName("paletteGroup");

  auto *paletteLay = new QVBoxLayout(m_paletteGroup);
  paletteLay->setSpacing(4);
  paletteLay->setContentsMargins(8, 12, 8, 8);

  // Scroll area so it doesn't blow up the dialog height
  auto *paletteScroll = new QScrollArea(m_paletteGroup);
  paletteScroll->setWidgetResizable(true);
  paletteScroll->setFrameShape(QFrame::NoFrame);
  paletteScroll->setMaximumHeight(180);
  paletteScroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

  m_paletteContent = new QWidget(paletteScroll);
  m_paletteContent->setStyleSheet("background: transparent;");
  paletteScroll->setWidget(m_paletteContent);

  paletteLay->addWidget(paletteScroll);
  mainLayout->addWidget(m_paletteGroup);

  // ═══════════════════════════════════════════════════════════════════
  // Section 4: Trigger (inside expression, but separate group box)
  // ═══════════════════════════════════════════════════════════════════

  m_triggerGroup = new QGroupBox("Recalculation Trigger", this);
  auto *trigForm = new QFormLayout(m_triggerGroup);
  trigForm->setLabelAlignment(Qt::AlignRight);
  trigForm->setSpacing(8);

  m_triggerCombo = new QComboBox(m_triggerGroup);
  m_triggerCombo->addItem("⚡  Every Tick",          (int)ParamTrigger::EveryTick);
  m_triggerCombo->addItem("🕯  On Candle Close",     (int)ParamTrigger::OnCandleClose);
  m_triggerCombo->addItem("📥  On Entry",            (int)ParamTrigger::OnEntry);
  m_triggerCombo->addItem("📤  On Exit",             (int)ParamTrigger::OnExit);
  m_triggerCombo->addItem("🔒  Once at Start",       (int)ParamTrigger::OnceAtStart);
  m_triggerCombo->addItem("⏲  On Schedule",          (int)ParamTrigger::OnSchedule);
  m_triggerCombo->addItem("✋  Manual (frozen)",      (int)ParamTrigger::Manual);
  m_triggerCombo->setToolTip(
      "When should this formula be recalculated during live trading?\n\n"
      "⚡ Every Tick — on every price update (highest frequency)\n"
      "   Use for: dynamic SL/TP that tracks price in real-time\n\n"
      "🕯 On Candle Close — when a candle completes\n"
      "   Use for: ATR-based, RSI-based values\n\n"
      "📥 On Entry — once when entry order is placed\n"
      "   Use for: entry-relative SL/TP, snapshot IV at entry\n\n"
      "📤 On Exit — once when exit order fires\n\n"
      "🔒 Once at Start — calculated once when strategy starts\n"
      "   Use for: session constants, opening range\n\n"
      "⏲ On Schedule — every N seconds\n"
      "   Use for: periodic IV recalc, slow metrics\n\n"
      "✋ Manual — never recalculated, value frozen at deploy");
  trigForm->addRow("When:", m_triggerCombo);
  connect(m_triggerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ParamEditorDialog::onTriggerChanged);

  // Candle timeframe (shown for OnCandleClose)
  m_timeframeLabel = new QLabel("Candle TF:", m_triggerGroup);
  m_timeframeCombo = new QComboBox(m_triggerGroup);
  m_timeframeCombo->addItems(
      {"(strategy default)", "1m", "3m", "5m", "15m", "30m", "1h", "4h", "1d", "1w"});
  m_timeframeCombo->setToolTip(
      "Which candle timeframe triggers recalculation.\n"
      "Leave as default to use the strategy's primary timeframe.");
  trigForm->addRow(m_timeframeLabel, m_timeframeCombo);

  // Schedule interval (shown for OnSchedule)
  m_intervalLabel = new QLabel("Interval:", m_triggerGroup);
  m_intervalSpin = new QSpinBox(m_triggerGroup);
  m_intervalSpin->setRange(1, 86400);
  m_intervalSpin->setSuffix(" sec");
  m_intervalSpin->setValue(300);
  m_intervalSpin->setToolTip("Recalculation interval in seconds.\ne.g. 300 = every 5 minutes");
  trigForm->addRow(m_intervalLabel, m_intervalSpin);

  // Trigger description (dynamic, updates on selection)
  auto *trigHintLabel = new QLabel(m_triggerGroup);
  trigHintLabel->setObjectName("trigHint");
  trigHintLabel->setWordWrap(true);
  trigHintLabel->setStyleSheet(
      "color: #64748b; font-size: 10px; font-style: italic; padding: 2px 0;");
  trigForm->addRow("", trigHintLabel);

  mainLayout->addWidget(m_triggerGroup);

  // ═══════════════════════════════════════════════════════════════════
  // Section 5: Description + Locked
  // ═══════════════════════════════════════════════════════════════════

  auto *metaGroup = new QGroupBox("Details", this);
  auto *metaForm = new QFormLayout(metaGroup);
  metaForm->setLabelAlignment(Qt::AlignRight);
  metaForm->setSpacing(8);

  m_descEdit = new QPlainTextEdit(metaGroup);
  m_descEdit->setPlaceholderText("Optional description shown as tooltip at deploy time");
  m_descEdit->setMaximumHeight(50);
  m_descEdit->setTabChangesFocus(true);
  metaForm->addRow("Description:", m_descEdit);

  m_lockedCheck = new QCheckBox("Locked — user cannot change this at deploy time", metaGroup);
  metaForm->addRow("", m_lockedCheck);

  mainLayout->addWidget(metaGroup);

  // ═══════════════════════════════════════════════════════════════════
  // Buttons
  // ═══════════════════════════════════════════════════════════════════

  auto *btnLayout = new QHBoxLayout();
  btnLayout->addStretch();

  auto *cancelBtn = new QPushButton("Cancel", this);
  cancelBtn->setObjectName("cancelBtn");
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(cancelBtn);

  auto *okBtn = new QPushButton("Save Parameter", this);
  okBtn->setObjectName("okBtn");
  okBtn->setDefault(true);
  connect(okBtn, &QPushButton::clicked, this, &ParamEditorDialog::accept);
  btnLayout->addWidget(okBtn);

  mainLayout->addLayout(btnLayout);

  // ── Initial visibility ──
  onTypeChanged(m_typeCombo->currentIndex());
}

// ═══════════════════════════════════════════════════════════════════
// Populate from existing param (edit mode)
// ═══════════════════════════════════════════════════════════════════

void ParamEditorDialog::setParam(const TemplateParam &p) {
  m_nameEdit->setText(p.name);
  m_labelEdit->setText(p.label);
  m_typeCombo->setCurrentIndex(static_cast<int>(p.valueType));

  if (p.isExpression()) {
    m_formulaEdit->setPlainText(p.expression);
    // Set trigger combo by data value
    for (int i = 0; i < m_triggerCombo->count(); ++i) {
      if (m_triggerCombo->itemData(i).toInt() == static_cast<int>(p.trigger)) {
        m_triggerCombo->setCurrentIndex(i);
        break;
      }
    }
    // Timeframe
    if (!p.triggerTimeframe.isEmpty()) {
      int idx = m_timeframeCombo->findText(p.triggerTimeframe);
      if (idx >= 0) m_timeframeCombo->setCurrentIndex(idx);
    }
    // Interval
    m_intervalSpin->setValue(p.scheduleIntervalSec);
  } else {
    m_defaultEdit->setText(p.defaultValue.toString());
    m_minEdit->setText(p.minValue.toString());
    m_maxEdit->setText(p.maxValue.toString());
  }

  m_descEdit->setPlainText(p.description);
  m_lockedCheck->setChecked(p.locked);

  // Trigger visibility
  onTypeChanged(m_typeCombo->currentIndex());
}

// ═══════════════════════════════════════════════════════════════════
// Extract param from form
// ═══════════════════════════════════════════════════════════════════

TemplateParam ParamEditorDialog::param() const {
  TemplateParam p;
  p.name = m_nameEdit->text().trimmed();
  p.label = m_labelEdit->text().trimmed();
  p.valueType = static_cast<ParamValueType>(m_typeCombo->currentIndex());
  p.description = m_descEdit->toPlainText().trimmed();
  p.locked = m_lockedCheck->isChecked();

  if (p.isExpression()) {
    p.expression = m_formulaEdit->toPlainText().trimmed();
    p.defaultValue = 0.0;
    p.trigger = static_cast<ParamTrigger>(
        m_triggerCombo->currentData().toInt());

    // Timeframe
    QString tf = m_timeframeCombo->currentText();
    if (tf.startsWith("(")) tf.clear(); // "(strategy default)" → empty
    p.triggerTimeframe = tf;

    // Interval
    p.scheduleIntervalSec = m_intervalSpin->value();
  } else {
    QString defStr = m_defaultEdit->text().trimmed();
    if (!defStr.isEmpty()) p.defaultValue = defStr;

    QString minStr = m_minEdit->text().trimmed();
    if (!minStr.isEmpty()) p.minValue = minStr;

    QString maxStr = m_maxEdit->text().trimmed();
    if (!maxStr.isEmpty()) p.maxValue = maxStr;

    p.trigger = ParamTrigger::Manual;
  }

  return p;
}

// ═══════════════════════════════════════════════════════════════════
// Title mode
// ═══════════════════════════════════════════════════════════════════

void ParamEditorDialog::setEditMode(bool editing) {
  auto *titleLabel = findChild<QLabel *>("titleLabel");
  if (titleLabel) {
    titleLabel->setText(editing ? "Edit Parameter" : "Add Parameter");
  }
  setWindowTitle(editing ? "Edit Parameter" : "Add Parameter");
}

// ═══════════════════════════════════════════════════════════════════
// Type changed — show/hide sections
// ═══════════════════════════════════════════════════════════════════

void ParamEditorDialog::onTypeChanged(int index) {
  bool isExpr = (index == 4); // Expression

  m_fixedGroup->setVisible(!isExpr);
  m_exprGroup->setVisible(isExpr);
  m_paletteGroup->setVisible(isExpr);
  m_triggerGroup->setVisible(isExpr);

  if (isExpr) {
    onTriggerChanged(m_triggerCombo->currentIndex());
    rebuildPalette();
  }

  // Resize to fit
  adjustSize();
}

// ═══════════════════════════════════════════════════════════════════
// Trigger changed — show/hide interval/timeframe
// ═══════════════════════════════════════════════════════════════════

void ParamEditorDialog::onTriggerChanged(int index) {
  ParamTrigger trigger = static_cast<ParamTrigger>(
      m_triggerCombo->itemData(index).toInt());

  bool showTimeframe = (trigger == ParamTrigger::OnCandleClose);
  m_timeframeLabel->setVisible(showTimeframe);
  m_timeframeCombo->setVisible(showTimeframe);

  bool showInterval = (trigger == ParamTrigger::OnSchedule);
  m_intervalLabel->setVisible(showInterval);
  m_intervalSpin->setVisible(showInterval);

  // Update hint
  auto *hint = findChild<QLabel *>("trigHint");
  if (hint) {
    switch (trigger) {
    case ParamTrigger::EveryTick:
      hint->setText("⚡ Formula runs on EVERY market tick. "
                    "Best for: trailing SL, dynamic targets.");
      break;
    case ParamTrigger::OnCandleClose:
      hint->setText("🕯 Formula runs when a candle closes. "
                    "Best for: ATR/RSI-based values that update per bar.");
      break;
    case ParamTrigger::OnEntry:
      hint->setText("📥 Formula runs ONCE when an entry order is placed. "
                    "Best for: freezing entry-relative SL/TP.");
      break;
    case ParamTrigger::OnExit:
      hint->setText("📤 Formula runs ONCE when an exit is triggered.");
      break;
    case ParamTrigger::OnceAtStart:
      hint->setText("🔒 Formula runs ONCE when strategy starts. "
                    "Best for: session constants, opening range values.");
      break;
    case ParamTrigger::OnSchedule:
      hint->setText("⏲ Formula runs every N seconds. "
                    "Best for: periodic IV recalculation, slow metrics.");
      break;
    case ParamTrigger::Manual:
      hint->setText("✋ Formula is evaluated once at deploy time and never updated. "
                    "User can override with a fixed number.");
      break;
    }
  }

  adjustSize();
}

// ═══════════════════════════════════════════════════════════════════
// Validation
// ═══════════════════════════════════════════════════════════════════

bool ParamEditorDialog::validate() {
  QString name = m_nameEdit->text().trimmed();
  if (name.isEmpty()) {
    m_nameEdit->setFocus();
    QMessageBox::warning(this, "Validation",
                         "Parameter Name is required.\n"
                         "Use UPPER_SNAKE_CASE (e.g. RSI_PERIOD).");
    return false;
  }

  // Name must be valid identifier (letters, digits, underscore)
  QRegularExpression nameRx("^[A-Za-z_][A-Za-z0-9_]*$");
  if (!nameRx.match(name).hasMatch()) {
    m_nameEdit->setFocus();
    QMessageBox::warning(this, "Validation",
                         "Parameter Name must be a valid identifier:\n"
                         "  • Start with a letter or underscore\n"
                         "  • Contain only letters, digits, underscores\n"
                         "  • e.g. RSI_PERIOD, SL_Level, myParam_1");
    return false;
  }

  bool isExpr = (m_typeCombo->currentIndex() == 4);

  if (isExpr) {
    QString formula = m_formulaEdit->toPlainText().trimmed();
    if (formula.isEmpty()) {
      m_formulaEdit->setFocus();
      QMessageBox::warning(this, "Validation",
                           "Expression parameter needs a formula.\n"
                           "Example: ATR(REF_1, 14) * 2.5");
      return false;
    }

    // Basic bracket balance check
    int depth = 0;
    for (QChar c : formula) {
      if (c == '(') ++depth;
      else if (c == ')') --depth;
      if (depth < 0) break;
    }
    if (depth != 0) {
      m_formulaEdit->setFocus();
      QMessageBox::warning(this, "Validation",
                           "Unbalanced parentheses in formula.\n"
                           "Check that every '(' has a matching ')'.");
      return false;
    }

    // OnSchedule needs interval > 0
    ParamTrigger trigger = static_cast<ParamTrigger>(
        m_triggerCombo->currentData().toInt());
    if (trigger == ParamTrigger::OnSchedule && m_intervalSpin->value() <= 0) {
      m_intervalSpin->setFocus();
      QMessageBox::warning(this, "Validation",
                           "On Schedule trigger needs an interval > 0 seconds.");
      return false;
    }
  }

  return true;
}

void ParamEditorDialog::accept() {
  if (!validate())
    return;
  QDialog::accept();
}

// ═══════════════════════════════════════════════════════════════════
// Context — receive available symbols, params, indicators from parent
// ═══════════════════════════════════════════════════════════════════

void ParamEditorDialog::setContext(const QStringList &symbolIds,
                                   const QStringList &paramNames,
                                   const QStringList &indicatorIds) {
  m_symbolIds    = symbolIds;
  m_paramNames   = paramNames;
  m_indicatorIds = indicatorIds;
  rebuildPalette();
}

// ═══════════════════════════════════════════════════════════════════
// Insert text at cursor in the formula editor
// ═══════════════════════════════════════════════════════════════════

void ParamEditorDialog::insertTextAtCursor(const QString &text) {
  m_formulaEdit->setFocus();
  auto cursor = m_formulaEdit->textCursor();
  cursor.insertText(text);
  m_formulaEdit->setTextCursor(cursor);
}

// ═══════════════════════════════════════════════════════════════════
// Build the reference palette — clickable chips grouped by category
// ═══════════════════════════════════════════════════════════════════

void ParamEditorDialog::rebuildPalette() {
  // Clear existing content
  if (m_paletteContent->layout()) {
    QLayoutItem *child;
    while ((child = m_paletteContent->layout()->takeAt(0)) != nullptr) {
      delete child->widget();
      delete child;
    }
    delete m_paletteContent->layout();
  }

  auto *mainLay = new QVBoxLayout(m_paletteContent);
  mainLay->setSpacing(8);
  mainLay->setContentsMargins(4, 4, 4, 4);

  // Helper: create a styled chip button
  auto makeChip = [this](const QString &label, const QString &insertText,
                         const QString &bgColor, const QString &textColor,
                         const QString &tooltip = QString()) -> QPushButton * {
    auto *btn = new QPushButton(label, m_paletteContent);
    btn->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                "border-radius: 3px; padding: 3px 8px; font-size: 11px; "
                "font-family: 'SF Mono', 'Consolas', monospace; font-weight: 500; } "
                "QPushButton:hover { background: %3; color: white; }")
            .arg(bgColor, textColor, textColor));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(24);
    if (!tooltip.isEmpty())
      btn->setToolTip(tooltip);
    connect(btn, &QPushButton::clicked, this,
            [this, insertText]() { insertTextAtCursor(insertText); });
    return btn;
  };

  // Helper: create a section label
  auto makeSection = [this](const QString &text) -> QLabel * {
    auto *lbl = new QLabel(text, m_paletteContent);
    lbl->setStyleSheet(
        "font-size: 10px; font-weight: 700; color: #64748b; "
        "padding: 2px 0 1px 0; border: none; background: transparent;");
    return lbl;
  };

  // Helper: create a flow-style row of chips
  auto makeFlowRow = [this]() -> QWidget * {
    auto *w = new QWidget(m_paletteContent);
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    w->setStyleSheet("background: transparent;");
    return w;
  };

  // ── Section 1: Symbol Slots (market data) ──
  if (!m_symbolIds.isEmpty()) {
    mainLay->addWidget(makeSection("📊 SYMBOL MARKET DATA"));

    // Build per-symbol rows with market data functions
    QStringList mktFuncs = {"LTP", "BID", "ASK", "HIGH", "LOW", "OPEN", "CLOSE", "VOLUME", "CHANGE_PCT"};
    QStringList greekFuncs = {"IV", "DELTA", "GAMMA", "THETA", "VEGA"};

    for (const QString &sym : m_symbolIds) {
      // Price functions row
      auto *row = makeFlowRow();
      auto *rowLay = qobject_cast<QHBoxLayout *>(row->layout());

      auto *symLabel = new QLabel(sym + ":", row);
      symLabel->setStyleSheet(
          "font-size: 10px; font-weight: 600; color: #1e40af; "
          "min-width: 60px; border: none; background: transparent;");
      rowLay->addWidget(symLabel);

      for (const QString &fn : mktFuncs) {
        QString insert = fn + "(" + sym + ")";
        rowLay->addWidget(makeChip(fn, insert, "#dbeafe", "#1e40af",
                                   fn + "(" + sym + ") — " + fn + " of " + sym));
      }
      rowLay->addStretch();
      mainLay->addWidget(row);

      // Greeks row (separate line for readability)
      auto *greekRow = makeFlowRow();
      auto *greekLay = qobject_cast<QHBoxLayout *>(greekRow->layout());
      auto *spacer = new QLabel("", greekRow);
      spacer->setFixedWidth(60);
      spacer->setStyleSheet("border: none; background: transparent;");
      greekLay->addWidget(spacer);

      for (const QString &fn : greekFuncs) {
        QString insert = fn + "(" + sym + ")";
        greekLay->addWidget(makeChip(fn, insert, "#fae8ff", "#7e22ce",
                                     fn + "(" + sym + ") — " + fn + " of " + sym));
      }
      greekLay->addStretch();
      mainLay->addWidget(greekRow);
    }
  }

  // ── Section 2: Other Parameters ──
  // Filter out the current param name from the palette
  QString currentName = m_nameEdit->text().trimmed();
  QStringList otherParams;
  for (const QString &pn : m_paramNames) {
    if (pn.compare(currentName, Qt::CaseInsensitive) != 0)
      otherParams << pn;
  }

  if (!otherParams.isEmpty()) {
    mainLay->addWidget(makeSection("🔢 OTHER PARAMETERS  (reference by name)"));
    auto *row = makeFlowRow();
    auto *rowLay = qobject_cast<QHBoxLayout *>(row->layout());
    for (const QString &pn : otherParams) {
      rowLay->addWidget(makeChip(pn, pn, "#dcfce7", "#166534",
                                 "Insert parameter reference: " + pn));
    }
    rowLay->addStretch();
    mainLay->addWidget(row);
  }

  // ── Section 3: Indicators ──
  if (!m_indicatorIds.isEmpty()) {
    mainLay->addWidget(makeSection("📈 DECLARED INDICATORS"));
    auto *row = makeFlowRow();
    auto *rowLay = qobject_cast<QHBoxLayout *>(row->layout());
    for (const QString &indId : m_indicatorIds) {
      rowLay->addWidget(makeChip(indId, indId, "#fef3c7", "#92400e",
                                 "Insert indicator reference: " + indId));
    }
    rowLay->addStretch();
    mainLay->addWidget(row);
  }

  // ── Section 4: Portfolio Functions ──
  mainLay->addWidget(makeSection("💰 PORTFOLIO"));
  {
    auto *row = makeFlowRow();
    auto *rowLay = qobject_cast<QHBoxLayout *>(row->layout());
    rowLay->addWidget(makeChip("MTM()", "MTM()", "#fce7f3", "#9d174d",
                               "Total mark-to-market P&L"));
    rowLay->addWidget(makeChip("NET_PREMIUM()", "NET_PREMIUM()", "#fce7f3", "#9d174d",
                               "Net premium collected/paid"));
    rowLay->addWidget(makeChip("NET_DELTA()", "NET_DELTA()", "#fce7f3", "#9d174d",
                               "Portfolio-level delta"));
    rowLay->addStretch();
    mainLay->addWidget(row);
  }

  // ── Section 5: Math & Operators ──
  mainLay->addWidget(makeSection("🔣 MATH & OPERATORS"));
  {
    auto *row = makeFlowRow();
    auto *rowLay = qobject_cast<QHBoxLayout *>(row->layout());

    QStringList mathItems = {
      "ABS(", "MAX(", "MIN(", "ROUND(", "FLOOR(", "CEIL(",
      "SQRT(", "LOG(", "POW(", "CLAMP("
    };
    for (const QString &fn : mathItems) {
      rowLay->addWidget(makeChip(fn.chopped(1), fn, "#f1f5f9", "#475569",
                                 fn + "...) — math function"));
    }
    rowLay->addStretch();
    mainLay->addWidget(row);

    // Operators row
    auto *opRow = makeFlowRow();
    auto *opLay = qobject_cast<QHBoxLayout *>(opRow->layout());
    QStringList ops = {"+", "-", "*", "/", "%", ">", "<", ">=", "<=", "==", "!=", "&&", "||", "? :"};
    for (const QString &op : ops) {
      QString insert = (op == "? :") ? " ? " : " " + op + " ";
      opLay->addWidget(makeChip(op, insert, "#f1f5f9", "#475569"));
    }
    opLay->addStretch();
    mainLay->addWidget(opRow);
  }

  mainLay->addStretch();
}

QString ParamEditorDialog::typeToString(ParamValueType t) const {
  switch (t) {
  case ParamValueType::Int:        return "Int";
  case ParamValueType::Double:     return "Double";
  case ParamValueType::Bool:       return "Bool";
  case ParamValueType::String:     return "String";
  case ParamValueType::Expression: return "Expression";
  }
  return "Double";
}
