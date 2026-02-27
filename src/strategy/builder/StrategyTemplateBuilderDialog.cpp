#include "strategy/builder/StrategyTemplateBuilderDialog.h"
#include "strategy/builder/ConditionBuilderWidget.h"
#include "strategy/builder/IndicatorCatalog.h"
#include "strategy/builder/IndicatorRowWidget.h"
#include "strategy/builder/ParamEditorDialog.h"
#include "ui_StrategyTemplateBuilderDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QMap>
#include <QMessageBox>
#include <QScrollArea>
#include <QSpacerItem>
#include <QSpinBox>
#include <QStandardPaths>
#include <QUuid>
#include <QVBoxLayout>

// ─────────────────────────────────────────────────────────────────────────────
// Column enums — Symbols table
// ─────────────────────────────────────────────────────────────────────────────
enum SymbolCol { SC_ID = 0, SC_LABEL, SC_ROLE, SC_TYPE, SC_COUNT };

// Column enums — Parameters summary table (read-only)
// The table is a compact summary. All editing is done via ParamEditorDialog.
// Matches column order in StrategyTemplateBuilderDialog.ui:
//   Name | Label | Type | Value/Formula | Trigger | Locked
enum ParamCol {
  PC_NAME = 0,
  PC_LABEL,
  PC_TYPE,
  PC_VALUE,       // Fixed: default value, Expression: formula preview
  PC_TRIGGER,     // Expression: trigger label, Fixed: "—"
  PC_LOCKED,
  PC_COUNT
};

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

StrategyTemplateBuilderDialog::StrategyTemplateBuilderDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::StrategyTemplateBuilderDialog) {
  ui->setupUi(this);

  // Stylesheet is fully defined in the .ui file (resources/forms/StrategyTemplateBuilderDialog.ui)
  // No runtime stylesheet override needed — .ui is the single source of truth.

  // ── Load indicator catalog (no-op if already loaded) ──
  loadIndicatorCatalog();

  // ── Symbols table header ──
  ui->symbolsTable->horizontalHeader()->setSectionResizeMode(
      SC_LABEL, QHeaderView::Stretch);
  ui->symbolsTable->horizontalHeader()->setSectionResizeMode(
      SC_ID, QHeaderView::ResizeToContents);
  ui->symbolsTable->verticalHeader()->setVisible(false);

  // ── Parameters summary table header ──
  ui->paramsTable->horizontalHeader()->setSectionResizeMode(
      PC_NAME, QHeaderView::ResizeToContents);
  ui->paramsTable->horizontalHeader()->setSectionResizeMode(
      PC_LABEL, QHeaderView::Stretch);
  ui->paramsTable->horizontalHeader()->setSectionResizeMode(
      PC_TYPE, QHeaderView::ResizeToContents);
  ui->paramsTable->horizontalHeader()->setSectionResizeMode(
      PC_VALUE, QHeaderView::Stretch);
  ui->paramsTable->horizontalHeader()->setSectionResizeMode(
      PC_TRIGGER, QHeaderView::ResizeToContents);
  ui->paramsTable->horizontalHeader()->setSectionResizeMode(
      PC_LOCKED, QHeaderView::ResizeToContents);
  ui->paramsTable->verticalHeader()->setVisible(false);

  // ── Grab the cards layout from the scroll area (created in .ui) ──
  m_cardsLayout = ui->indicatorCardsLayout;
  // Add a bottom spacer so cards stack from the top
  m_cardsLayout->addStretch(1);

  // ── Button connections ──
  connect(ui->saveButton, &QPushButton::clicked, this,
          &StrategyTemplateBuilderDialog::accept);
  connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  connect(ui->addSymbolBtn, &QPushButton::clicked, this,
          &StrategyTemplateBuilderDialog::onAddSymbol);
  connect(ui->removeSymbolBtn, &QPushButton::clicked, this,
          &StrategyTemplateBuilderDialog::onRemoveSymbol);
  connect(ui->symbolsTable, &QTableWidget::cellChanged, this,
          &StrategyTemplateBuilderDialog::onSymbolTableChanged);

  connect(ui->addIndicatorBtn, &QPushButton::clicked, this,
          &StrategyTemplateBuilderDialog::onAddIndicator);

  connect(ui->addParamBtn, &QPushButton::clicked, this,
          &StrategyTemplateBuilderDialog::onAddParam);
  connect(ui->editParamBtn, &QPushButton::clicked, this,
          &StrategyTemplateBuilderDialog::onEditParam);
  connect(ui->removeParamBtn, &QPushButton::clicked, this,
          &StrategyTemplateBuilderDialog::onRemoveParam);
  connect(ui->paramsTable, &QTableWidget::cellDoubleClicked, this,
          [this](int row, int /*col*/) { onEditParamRow(row); });

  // Pre-fill two default symbol slots for new templates
  addDefaultSymbolSlots();
}

StrategyTemplateBuilderDialog::~StrategyTemplateBuilderDialog() { delete ui; }

// ─────────────────────────────────────────────────────────────────────────────
// Edit mode: populate from existing template
// ─────────────────────────────────────────────────────────────────────────────

void StrategyTemplateBuilderDialog::setTemplate(const StrategyTemplate &tmpl) {
  m_existingTemplateId = tmpl.templateId;
  m_existingVersion = tmpl.version;
  m_existingCreatedAt = tmpl.createdAt;

  populateMetadata(tmpl);
  populateSymbols(tmpl); // clears + repopulates — overwrites default slots
  populateIndicators(tmpl);
  populateParameters(tmpl);
  populateConditions(tmpl);
  populateRisk(tmpl);
  refreshConditionContext();
}

void StrategyTemplateBuilderDialog::populateMetadata(
    const StrategyTemplate &tmpl) {
  ui->nameEdit->setText(tmpl.name);
  ui->descEdit->setPlainText(tmpl.description);
  // modeCombo items have userData set in .ui; find by data string
  for (int i = 0; i < ui->modeCombo->count(); ++i) {
    if (ui->modeCombo->itemData(i).toString() == tmpl.modeString()) {
      ui->modeCombo->setCurrentIndex(i);
      break;
    }
  }
  ui->timeTriggerCheck->setChecked(tmpl.usesTimeTrigger);
  ui->optionsFlagCheck->setChecked(tmpl.predominantlyOptions);
}

void StrategyTemplateBuilderDialog::populateSymbols(
    const StrategyTemplate &tmpl) {
  ui->symbolsTable->blockSignals(true);
  ui->symbolsTable->setRowCount(0);
  for (const auto &sym : tmpl.symbols)
    addSymbolRow(sym.id, sym.label, sym.role == SymbolRole::Trade ? 1 : 0,
                 segmentToComboIndex(sym.segment));
  ui->symbolsTable->blockSignals(false);
}

void StrategyTemplateBuilderDialog::populateIndicators(
    const StrategyTemplate &tmpl) {
  // Remove all existing cards
  for (auto *card : m_indicatorCards) {
    m_cardsLayout->removeWidget(card);
    card->deleteLater();
  }
  m_indicatorCards.clear();

  for (const auto &ind : tmpl.indicators)
    addIndicatorCard(ind);
}

void StrategyTemplateBuilderDialog::populateParameters(
    const StrategyTemplate &tmpl) {
  m_params = tmpl.params;   // store full param data
  refreshParamsTable();
}

void StrategyTemplateBuilderDialog::populateConditions(
    const StrategyTemplate &tmpl) {
  ui->entryBuilder->setCondition(tmpl.entryCondition);
  ui->exitBuilder->setCondition(tmpl.exitCondition);
}

void StrategyTemplateBuilderDialog::populateRisk(const StrategyTemplate &tmpl) {
  const auto &r = tmpl.riskDefaults;
  ui->slPctSpin->setValue(r.stopLossPercent);
  ui->slLockedCheck->setChecked(r.stopLossLocked);
  ui->tgtPctSpin->setValue(r.targetPercent);
  ui->tgtLockedCheck->setChecked(r.targetLocked);
  ui->trailingCheck->setChecked(r.trailingEnabled);
  ui->trailTriggerSpin->setValue(r.trailingTriggerPct);
  ui->trailAmountSpin->setValue(r.trailingAmountPct);
  ui->timeExitCheck->setChecked(r.timeExitEnabled);
  ui->exitTimeEdit->setTime(QTime::fromString(r.exitTime, "HH:mm"));
  ui->maxTradesSpin->setValue(r.maxDailyTrades);
  ui->maxLossSpin->setValue(r.maxDailyLossRs);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots — Symbols tab
// ─────────────────────────────────────────────────────────────────────────────

// Private helper: insert one row into symbolsTable.
// Returns the newly created row index.
int StrategyTemplateBuilderDialog::addSymbolRow(const QString &id,
                                                const QString &label,
                                                int roleIndex,
                                                int segmentIndex) {
  int row = ui->symbolsTable->rowCount();
  ui->symbolsTable->insertRow(row);

  ui->symbolsTable->setItem(row, SC_ID, new QTableWidgetItem(id));
  ui->symbolsTable->setItem(row, SC_LABEL, new QTableWidgetItem(label));

  auto *roleCombo = new QComboBox(ui->symbolsTable);
  roleCombo->addItems({"Reference", "Trade"});
  roleCombo->setCurrentIndex(roleIndex);
  ui->symbolsTable->setCellWidget(row, SC_ROLE, roleCombo);

  auto *segCombo = new QComboBox(ui->symbolsTable);
  segCombo->addItems({"NSE CM", "NSE FO", "BSE CM", "BSE FO"});
  segCombo->setCurrentIndex(segmentIndex);
  ui->symbolsTable->setCellWidget(row, SC_TYPE, segCombo);

  connect(roleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &StrategyTemplateBuilderDialog::onSymbolTableChanged);

  return row;
}

// Private helper: add the two default slots (REF_1 / TRADE_1) for new
// templates.
void StrategyTemplateBuilderDialog::addDefaultSymbolSlots() {
  ui->symbolsTable->blockSignals(true);

  // REF_1 — Reference symbol, NSE CM (index 0)
  addSymbolRow("REF_1", "Reference Symbol", /*role=Reference*/ 0,
               /*seg=NSE CM*/ 0);

  // TRADE_1 — Trade symbol, NSE FO (index 1)
  addSymbolRow("TRADE_1", "Trade Instrument", /*role=Trade*/ 1,
               /*seg=NSE FO*/ 1);

  ui->symbolsTable->blockSignals(false);
  refreshConditionContext();
}

void StrategyTemplateBuilderDialog::onAddSymbol() {
  // Count how many Reference vs Trade rows already exist to build the auto-id
  int refCount = 0;
  int tradeCount = 0;
  for (int r = 0; r < ui->symbolsTable->rowCount(); ++r) {
    auto *rc =
        qobject_cast<QComboBox *>(ui->symbolsTable->cellWidget(r, SC_ROLE));
    if (rc && rc->currentIndex() == 1)
      ++tradeCount;
    else
      ++refCount;
  }
  // Next blank slot defaults to Reference
  QString autoId = QString("REF_%1").arg(refCount + 1);

  addSymbolRow(autoId, "", /*role=Reference*/ 0, /*seg=NSE CM*/ 0);

  ui->symbolsTable->scrollToBottom();
  refreshConditionContext();
}

void StrategyTemplateBuilderDialog::onRemoveSymbol() {
  int row = ui->symbolsTable->currentRow();
  if (row >= 0) {
    ui->symbolsTable->removeRow(row);
    refreshConditionContext();
  }
}

void StrategyTemplateBuilderDialog::onSymbolTableChanged() {
  refreshConditionContext();
}

// ─────────────────────────────────────────────────────────────────────────────
// Indicator catalog loader
// ─────────────────────────────────────────────────────────────────────────────

void StrategyTemplateBuilderDialog::loadIndicatorCatalog() {
  IndicatorCatalog &cat = IndicatorCatalog::instance();
  if (cat.isLoaded())
    return;

  // Try paths in order of preference
  QStringList candidates;
  QString binDir = QApplication::applicationDirPath();

  // 1. Next to the binary (deployed / build_ninja layout)
  candidates << QDir(binDir).absoluteFilePath(
      "configs/indicator_defaults.json");
  // 2. One level up  (build/ subdir)
  candidates << QDir(binDir).absoluteFilePath(
      "../configs/indicator_defaults.json");
  // 3. Two levels up  (macOS .app: Contents/MacOS → Contents → .app)
  candidates << QDir(binDir).absoluteFilePath(
      "../../configs/indicator_defaults.json");
  // 4. Three levels up (.app bundle sits inside build/)
  candidates << QDir(binDir).absoluteFilePath(
      "../../../configs/indicator_defaults.json");
  // 5. Four levels up  (build/TradingTerminal.app/Contents/MacOS → project
  // root)
  candidates << QDir(binDir).absoluteFilePath(
      "../../../../configs/indicator_defaults.json");

  for (const QString &path : candidates) {
    QFileInfo fi(path);
    if (fi.exists()) {
      if (cat.load(fi.absoluteFilePath())) {
        qDebug("IndicatorCatalog: loaded from %s",
               qPrintable(fi.absoluteFilePath()));
        return;
      }
    }
  }

  qWarning("IndicatorCatalog: could not load indicator_defaults.json\n"
           "  Binary dir: %s\n"
           "  Tried: %s",
           qPrintable(binDir), qPrintable(candidates.join("\n  ")));
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: insert a filled row into indicatorsTable
// ─────────────────────────────────────────────────────────────────────────────

int StrategyTemplateBuilderDialog::addIndicatorRow(
    const QString &id, const QString &type, const QString &symbolId,
    const QString &period1, const QString &period2, const QString &priceField,
    const QString &param3, const QString &outputSel, const QString &timeframe) {
  // Kept for compatibility (used from populateIndicators via addIndicatorCard)
  IndicatorDefinition ind;
  ind.id = id;
  ind.type = type;
  ind.symbolId = symbolId;
  ind.periodParam = period1;
  ind.period2Param = period2;
  ind.priceField = priceField.isEmpty() ? "close" : priceField;
  ind.param3Str = param3;
  ind.outputSelector = outputSel;
  ind.timeframe = timeframe.isEmpty() ? "D" : timeframe;
  addIndicatorCard(ind);
  return m_indicatorCards.size() - 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Card-based indicator management
// ─────────────────────────────────────────────────────────────────────────────

IndicatorRowWidget *StrategyTemplateBuilderDialog::addIndicatorCard(
    const IndicatorDefinition &ind) {
  // Build current symbol list
  QStringList symIds;
  for (int r = 0; r < ui->symbolsTable->rowCount(); ++r) {
    auto *item = ui->symbolsTable->item(r, SC_ID);
    if (item && !item->text().trimmed().isEmpty())
      symIds << item->text().trimmed();
  }
  if (symIds.isEmpty())
    symIds << "REF_1";

  int indexHint = m_indicatorCards.size() + 1;
  auto *card = new IndicatorRowWidget(symIds, indexHint, this);

  // Pre-fill if we have data
  if (!ind.type.isEmpty())
    card->populate(ind);

  // Insert before the trailing stretch (always last item)
  int insertPos = m_cardsLayout->count() - 1;
  if (insertPos < 0)
    insertPos = 0;
  m_cardsLayout->insertWidget(insertPos, card);
  m_indicatorCards.append(card);

  // Wire signals
  connect(card, &IndicatorRowWidget::removeRequested, this,
          [this, card]() { removeIndicatorCard(card); });
  connect(card, &IndicatorRowWidget::changed, this,
          &StrategyTemplateBuilderDialog::refreshConditionContext);

  // Scroll to show the new card
  QApplication::processEvents();
  ui->indicatorsScrollArea->ensureWidgetVisible(card);

  refreshConditionContext();
  return card;
}

void StrategyTemplateBuilderDialog::removeIndicatorCard(
    IndicatorRowWidget *card) {
  if (!card)
    return;
  m_cardsLayout->removeWidget(card);
  m_indicatorCards.removeOne(card);
  card->deleteLater();
  refreshConditionContext();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots — Indicators tab
// ─────────────────────────────────────────────────────────────────────────────

void StrategyTemplateBuilderDialog::onAddIndicator() {
  addIndicatorCard(); // blank card — user picks type from the combobox
  ui->tabWidget->setCurrentWidget(ui->indicatorsTab);
}

// ─────────────────────────────────────────────────────────────────────────────
// Parameters tab — form-based approach via ParamEditorDialog
// ─────────────────────────────────────────────────────────────────────────────

// Rebuild the read-only summary table from m_params.
void StrategyTemplateBuilderDialog::refreshParamsTable() {
  ui->paramsTable->blockSignals(true);
  ui->paramsTable->setRowCount(0);

  auto triggerLabel = [](const TemplateParam &p) -> QString {
    if (!p.isExpression()) return QStringLiteral("—");
    switch (p.trigger) {
    case ParamTrigger::EveryTick:    return "⚡ Every Tick";
    case ParamTrigger::OnCandleClose:
      return p.triggerTimeframe.isEmpty()
                 ? "🕯 Candle Close"
                 : QStringLiteral("🕯 Candle Close (%1)").arg(p.triggerTimeframe);
    case ParamTrigger::OnEntry:      return "📥 On Entry";
    case ParamTrigger::OnExit:       return "📤 On Exit";
    case ParamTrigger::OnceAtStart:  return "🔒 Once at Start";
    case ParamTrigger::OnSchedule:
      return QStringLiteral("⏲ Schedule (%1s)").arg(p.scheduleIntervalSec);
    case ParamTrigger::Manual:       return "✋ Manual";
    }
    return "—";
  };

  auto typeLabel = [](ParamValueType t) -> QString {
    switch (t) {
    case ParamValueType::Int:        return "Int";
    case ParamValueType::Double:     return "Double";
    case ParamValueType::Bool:       return "Bool";
    case ParamValueType::String:     return "String";
    case ParamValueType::Expression: return "Expression";
    }
    return "Double";
  };

  for (int i = 0; i < m_params.size(); ++i) {
    const TemplateParam &p = m_params[i];
    int row = ui->paramsTable->rowCount();
    ui->paramsTable->insertRow(row);

    auto setCell = [&](int col, const QString &text) {
      auto *item = new QTableWidgetItem(text);
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      ui->paramsTable->setItem(row, col, item);
    };

    setCell(PC_NAME,    p.name);
    setCell(PC_LABEL,   p.label);
    setCell(PC_TYPE,    typeLabel(p.valueType));
    setCell(PC_VALUE,   p.isExpression() ? p.expression
                                         : p.defaultValue.toString());
    setCell(PC_TRIGGER, triggerLabel(p));
    setCell(PC_LOCKED,  p.locked ? "🔒 Yes" : "");
  }

  ui->paramsTable->blockSignals(false);
  refreshConditionContext();
}

void StrategyTemplateBuilderDialog::onAddParam() {
  ParamEditorDialog dlg(this);
  dlg.setEditMode(false);

  // Pass available context so the formula palette shows symbol slots, params, indicators
  QStringList symIds;
  for (int r = 0; r < ui->symbolsTable->rowCount(); ++r) {
    auto *item = ui->symbolsTable->item(r, SC_ID);
    if (item && !item->text().trimmed().isEmpty())
      symIds << item->text().trimmed();
  }
  QStringList paramNames;
  for (const auto &p : m_params)
    if (!p.name.isEmpty()) paramNames << p.name;
  QStringList indIds;
  for (auto *card : m_indicatorCards) {
    IndicatorDefinition def = card->definition();
    if (!def.id.isEmpty()) indIds << def.id;
  }
  dlg.setContext(symIds, paramNames, indIds);

  if (dlg.exec() == QDialog::Accepted) {
    TemplateParam p = dlg.param();

    // Avoid duplicate names
    for (const auto &existing : m_params) {
      if (existing.name.compare(p.name, Qt::CaseInsensitive) == 0) {
        QMessageBox::warning(this, "Duplicate Name",
                             QString("A parameter named '%1' already exists.\n"
                                     "Please use a different name.")
                                 .arg(p.name));
        return;
      }
    }

    m_params.append(p);
    refreshParamsTable();
    ui->paramsTable->scrollToBottom();
  }
}

void StrategyTemplateBuilderDialog::onEditParam() {
  int row = ui->paramsTable->currentRow();
  if (row >= 0 && row < m_params.size())
    onEditParamRow(row);
}

void StrategyTemplateBuilderDialog::onEditParamRow(int row) {
  if (row < 0 || row >= m_params.size())
    return;

  ParamEditorDialog dlg(this);
  dlg.setEditMode(true);

  // Pass available context for the formula palette
  QStringList symIds;
  for (int r = 0; r < ui->symbolsTable->rowCount(); ++r) {
    auto *item = ui->symbolsTable->item(r, SC_ID);
    if (item && !item->text().trimmed().isEmpty())
      symIds << item->text().trimmed();
  }
  QStringList paramNames;
  for (const auto &p : m_params)
    if (!p.name.isEmpty()) paramNames << p.name;
  QStringList indIds;
  for (auto *card : m_indicatorCards) {
    IndicatorDefinition def = card->definition();
    if (!def.id.isEmpty()) indIds << def.id;
  }
  dlg.setContext(symIds, paramNames, indIds);
  dlg.setParam(m_params[row]);

  if (dlg.exec() == QDialog::Accepted) {
    TemplateParam p = dlg.param();

    // Check for duplicate name (excluding self)
    for (int i = 0; i < m_params.size(); ++i) {
      if (i == row) continue;
      if (m_params[i].name.compare(p.name, Qt::CaseInsensitive) == 0) {
        QMessageBox::warning(this, "Duplicate Name",
                             QString("A parameter named '%1' already exists.\n"
                                     "Please use a different name.")
                                 .arg(p.name));
        return;
      }
    }

    m_params[row] = p;
    refreshParamsTable();
    ui->paramsTable->setCurrentCell(row, 0);
  }
}

void StrategyTemplateBuilderDialog::onRemoveParam() {
  int row = ui->paramsTable->currentRow();
  if (row >= 0 && row < m_params.size()) {
    m_params.removeAt(row);
    refreshParamsTable();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Context refresh — push symbol/indicator/param IDs into condition builders
// ─────────────────────────────────────────────────────────────────────────────

void StrategyTemplateBuilderDialog::refreshConditionContext() {
  QStringList symIds;
  for (int r = 0; r < ui->symbolsTable->rowCount(); ++r) {
    auto *item = ui->symbolsTable->item(r, SC_ID);
    if (item && !item->text().trimmed().isEmpty())
      symIds << item->text().trimmed();
  }

  // Refresh symbol combos in every indicator card
  for (auto *card : m_indicatorCards)
    card->setSymbolIds(symIds);

  // Build indicator ID list and output map from cards
  QStringList indIds;
  QMap<QString, QStringList> outputMap;

  for (auto *card : m_indicatorCards) {
    IndicatorDefinition def = card->definition();
    if (def.id.isEmpty() || def.type.isEmpty())
      continue;

    indIds << def.id;

    // Look up outputs from catalog
    IndicatorMeta meta;
    if (IndicatorCatalog::instance().find(def.type, meta) &&
        !meta.outputs.isEmpty()) {
      outputMap[def.id] = meta.outputs;
    } else {
      // Single-output fallback
      outputMap[def.id] = QStringList{"value"};
    }
  }

  QStringList paramNames;
  for (const auto &p : m_params) {
    if (!p.name.isEmpty())
      paramNames << p.name;
  }

  ui->entryBuilder->setSymbolIds(symIds);
  ui->entryBuilder->setIndicatorIds(indIds);
  ui->entryBuilder->setIndicatorOutputMap(outputMap);
  ui->entryBuilder->setParamNames(paramNames);

  ui->exitBuilder->setSymbolIds(symIds);
  ui->exitBuilder->setIndicatorIds(indIds);
  ui->exitBuilder->setIndicatorOutputMap(outputMap);
  ui->exitBuilder->setParamNames(paramNames);
}

// ─────────────────────────────────────────────────────────────────────────────
// Extract helpers
// ─────────────────────────────────────────────────────────────────────────────

QVector<SymbolDefinition>
StrategyTemplateBuilderDialog::extractSymbols() const {
  QVector<SymbolDefinition> result;
  for (int r = 0; r < ui->symbolsTable->rowCount(); ++r) {
    SymbolDefinition sym;
    auto *idItem = ui->symbolsTable->item(r, SC_ID);
    auto *labelItem = ui->symbolsTable->item(r, SC_LABEL);
    sym.id = idItem ? idItem->text().trimmed() : QString("SYM_%1").arg(r + 1);
    sym.label = labelItem ? labelItem->text().trimmed() : sym.id;

    auto *roleCombo =
        qobject_cast<QComboBox *>(ui->symbolsTable->cellWidget(r, SC_ROLE));
    sym.role = (roleCombo && roleCombo->currentIndex() == 1)
                   ? SymbolRole::Trade
                   : SymbolRole::Reference;

    auto *segCombo =
        qobject_cast<QComboBox *>(ui->symbolsTable->cellWidget(r, SC_TYPE));
    sym.segment = segCombo ? segmentFromComboIndex(segCombo->currentIndex())
                           : ExchangeSegment::NSECM;
    sym.tradeType = sym.segment; // keep alias in sync

    if (!sym.id.isEmpty())
      result.append(sym);
  }
  return result;
}

QVector<IndicatorDefinition>
StrategyTemplateBuilderDialog::extractIndicators() const {
  QVector<IndicatorDefinition> result;
  for (auto *card : m_indicatorCards) {
    IndicatorDefinition ind = card->definition();
    if (!ind.id.isEmpty() && !ind.type.isEmpty())
      result.append(ind);
  }
  return result;
}

QVector<TemplateParam> StrategyTemplateBuilderDialog::extractParams() const {
  return m_params;
}

RiskDefaults StrategyTemplateBuilderDialog::extractRisk() const {
  RiskDefaults r;
  r.stopLossPercent = ui->slPctSpin->value();
  r.stopLossLocked = ui->slLockedCheck->isChecked();
  r.targetPercent = ui->tgtPctSpin->value();
  r.targetLocked = ui->tgtLockedCheck->isChecked();
  r.trailingEnabled = ui->trailingCheck->isChecked();
  r.trailingTriggerPct = ui->trailTriggerSpin->value();
  r.trailingAmountPct = ui->trailAmountSpin->value();
  r.timeExitEnabled = ui->timeExitCheck->isChecked();
  r.exitTime = ui->exitTimeEdit->time().toString("HH:mm");
  r.maxDailyTrades = ui->maxTradesSpin->value();
  r.maxDailyLossRs = ui->maxLossSpin->value();
  return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildTemplate
// ─────────────────────────────────────────────────────────────────────────────

StrategyTemplate StrategyTemplateBuilderDialog::buildTemplate() const {
  StrategyTemplate tmpl;

  tmpl.templateId = m_existingTemplateId.isEmpty()
                        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                        : m_existingTemplateId;
  tmpl.version = m_existingVersion.isEmpty() ? "1.0" : m_existingVersion;
  tmpl.createdAt = m_existingCreatedAt.isValid() ? m_existingCreatedAt
                                                 : QDateTime::currentDateTime();
  tmpl.updatedAt = QDateTime::currentDateTime();

  tmpl.name = ui->nameEdit->text().trimmed();
  tmpl.description = ui->descEdit->toPlainText().trimmed();
  tmpl.mode =
      StrategyTemplate::modeFromString(ui->modeCombo->currentData().toString());
  tmpl.usesTimeTrigger = ui->timeTriggerCheck->isChecked();
  tmpl.predominantlyOptions = ui->optionsFlagCheck->isChecked();

  tmpl.symbols = extractSymbols();
  tmpl.indicators = extractIndicators();
  tmpl.params = extractParams();
  tmpl.entryCondition = ui->entryBuilder->condition();
  tmpl.exitCondition = ui->exitBuilder->condition();
  tmpl.riskDefaults = extractRisk();

  return tmpl;
}

// ─────────────────────────────────────────────────────────────────────────────
// Validation & accept
// ─────────────────────────────────────────────────────────────────────────────

bool StrategyTemplateBuilderDialog::validate() {
  if (ui->nameEdit->text().trimmed().isEmpty()) {
    ui->tabWidget->setCurrentIndex(0);
    ui->nameEdit->setFocus();
    QMessageBox::warning(this, "Validation", "Template Name is required.");
    return false;
  }
  if (ui->symbolsTable->rowCount() == 0) {
    ui->tabWidget->setCurrentIndex(1);
    QMessageBox::warning(this, "Validation",
                         "At least one symbol slot is required.\n"
                         "Go to the Symbols tab and add at least one symbol.");
    return false;
  }

  // ── Validate Expression parameters ──
  for (int i = 0; i < m_params.size(); ++i) {
    const TemplateParam &p = m_params[i];
    if (!p.isExpression())
      continue;

    QString paramName = p.name.isEmpty() ? QString("row %1").arg(i + 1) : p.name;

    if (p.expression.trimmed().isEmpty()) {
      ui->tabWidget->setCurrentIndex(3); // Parameters tab
      ui->paramsTable->setCurrentCell(i, PC_VALUE);
      QMessageBox::warning(
          this, "Validation",
          QString("Expression parameter '%1' has an empty formula.\n"
                  "Double-click the row to edit and enter a formula "
                  "like: ATR(REF_1, 14) * 2.5")
              .arg(paramName));
      return false;
    }

    // If OnSchedule, check interval > 0
    if (p.trigger == ParamTrigger::OnSchedule && p.scheduleIntervalSec <= 0) {
      ui->tabWidget->setCurrentIndex(3);
      ui->paramsTable->setCurrentCell(i, PC_TRIGGER);
      QMessageBox::warning(
          this, "Validation",
          QString("Expression parameter '%1' uses On Schedule trigger "
                  "but has no interval.\n"
                  "Edit the parameter and set an interval > 0 seconds.")
              .arg(paramName));
      return false;
    }
  }

  return true;
}

void StrategyTemplateBuilderDialog::accept() {
  if (!validate())
    return;
  QDialog::accept();
}

// end of file
