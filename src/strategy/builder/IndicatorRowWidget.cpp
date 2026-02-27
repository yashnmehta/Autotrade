#include "strategy/builder/IndicatorRowWidget.h"
#include "strategy/builder/IndicatorCatalog.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

// ─────────────────────────────────────────────────────────────────────────────
// Ensure catalog is loaded — mirrors the logic in StrategyTemplateBuilderDialog
// so IndicatorRowWidget can be used standalone too.
// ─────────────────────────────────────────────────────────────────────────────
static void ensureCatalogLoaded() {
  IndicatorCatalog &cat = IndicatorCatalog::instance();
  if (cat.isLoaded())
    return;

  QString binDir = QApplication::applicationDirPath();
  const QStringList candidates = {
      QDir(binDir).absoluteFilePath("configs/indicator_defaults.json"),
      QDir(binDir).absoluteFilePath("../configs/indicator_defaults.json"),
      QDir(binDir).absoluteFilePath("../../configs/indicator_defaults.json"),
      QDir(binDir).absoluteFilePath("../../../configs/indicator_defaults.json"),
      QDir(binDir).absoluteFilePath(
          "../../../../configs/indicator_defaults.json"),
  };
  for (const QString &p : candidates) {
    if (QFileInfo::exists(p) && cat.load(p)) {
      qDebug("IndicatorRowWidget: catalog loaded from %s", qPrintable(p));
      return;
    }
  }
  qWarning("IndicatorRowWidget: catalog not loaded — Type combo will be empty");
}

// ─────────────────────────────────────────────────────────────────────────────
// Stylesheet for one card (Light Theme)
// ─────────────────────────────────────────────────────────────────────────────
static const char *kCardStyle = R"(
    IndicatorRowWidget {
        background: #ffffff;
        border: 1px solid #e2e8f0;
        border-radius: 6px;
    }
    QLabel {
        color: #475569;
        font-size: 11px;
    }
    QLabel#indTypeLabel {
        color: #2563eb;
        font-weight: 700;
        font-size: 12px;
    }
    QLineEdit {
        background: #f8fafc;
        border: 1px solid #cbd5e1;
        border-radius: 4px;
        color: #0f172a;
        padding: 3px 7px;
        font-size: 12px;
    }
    QLineEdit:focus { border-color: #3b82f6; background: #ffffff; }
    QComboBox {
        background: #f8fafc;
        border: 1px solid #cbd5e1;
        border-radius: 4px;
        color: #0f172a;
        padding: 3px 7px;
        font-size: 12px;
        min-width: 70px;
    }
    QComboBox:hover  { border-color: #64748b; }
    QComboBox:focus  { border-color: #3b82f6; }
    QComboBox::drop-down { border: none; width: 16px; }
    QComboBox QAbstractItemView {
        background: #ffffff; color: #0f172a;
        border: 1px solid #e2e8f0;
        selection-background-color: #dbeafe;
        selection-color: #1e40af;
    }
    QPushButton#removeBtn {
        background: #fef2f2;
        color: #dc2626;
        border: 1px solid #fecaca;
        border-radius: 4px;
        font-size: 13px;
        font-weight: 700;
        padding: 0px 6px;
        min-width: 24px;
        max-width: 24px;
        min-height: 24px;
        max-height: 24px;
    }
    QPushButton#removeBtn:hover { background: #fee2e2; color: #991b1b; }
    QFrame#divider { color: #e2e8f0; }
)";

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
IndicatorRowWidget::IndicatorRowWidget(const QStringList &symbolIds,
                                       int indexHint, QWidget *parent)
    : QWidget(parent), m_indexHint(indexHint) {
  ensureCatalogLoaded(); // ← guarantee catalog is ready before we populate

  setStyleSheet(kCardStyle);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(10, 8, 10, 8);
  outer->setSpacing(6);

  // ─── Row 1: Type | Symbol | TF | Price | [✕] ─────────────────────────
  auto *topRow = new QHBoxLayout;
  topRow->setSpacing(8);

  // Type combo — all TA-Lib indicators from catalog
  m_typeCombo = new QComboBox(this);
  m_typeCombo->setMinimumWidth(130);
  m_typeCombo->setToolTip("Indicator type (all 158 TA-Lib indicators)");
  IndicatorCatalog &cat = IndicatorCatalog::instance();
  for (const QString &grp : cat.groups()) {
    m_typeCombo->addItem(QString("── %1 ──").arg(grp));
    int separatorIdx = m_typeCombo->count() - 1;
    // Make group headers non-selectable
    QStandardItemModel *model =
        qobject_cast<QStandardItemModel *>(m_typeCombo->model());
    if (model) {
      model->item(separatorIdx)->setEnabled(false);
      model->item(separatorIdx)->setForeground(QColor("#94a3b8")); // Slate-400
      QFont f = model->item(separatorIdx)->font();
      f.setItalic(true);
      model->item(separatorIdx)->setFont(f);
    }
    for (const IndicatorMeta &m : cat.forGroup(grp))
      m_typeCombo->addItem(QString("%1 — %2").arg(m.type, m.label), m.type);
  }

  // Symbol combo
  m_symCombo = new QComboBox(this);
  m_symCombo->setToolTip("Symbol slot to compute indicator on");
  m_symCombo->addItems(symbolIds.isEmpty() ? QStringList{"REF_1"} : symbolIds);

  // Timeframe combo
  m_tfCombo = new QComboBox(this);
  m_tfCombo->addItems({"1", "3", "5", "10", "15", "30", "60", "D", "W"});
  m_tfCombo->setCurrentText("D");
  m_tfCombo->setToolTip("Candle interval");
  m_tfCombo->setMinimumWidth(50);
  m_tfCombo->setMaximumWidth(65);

  // Price field combo
  m_priceCombo = new QComboBox(this);
  m_priceCombo->addItems(
      {"close", "open", "high", "low", "hl2", "hlc3", "ohlc4", "volume"});
  m_priceCombo->setToolTip("OHLCV input field");
  m_priceCombo->setMaximumWidth(80);

  // Remove button
  auto *removeBtn = new QPushButton("✕", this);
  removeBtn->setObjectName("removeBtn");
  removeBtn->setToolTip("Remove this indicator");
  connect(removeBtn, &QPushButton::clicked, this,
          &IndicatorRowWidget::removeRequested);

  topRow->addWidget(new QLabel("Type:", this));
  topRow->addWidget(m_typeCombo, 2);
  topRow->addSpacing(6);
  topRow->addWidget(new QLabel("Symbol:", this));
  topRow->addWidget(m_symCombo, 1);
  topRow->addWidget(new QLabel("TF:", this));
  topRow->addWidget(m_tfCombo);
  topRow->addWidget(new QLabel("Price:", this));
  topRow->addWidget(m_priceCombo);
  topRow->addSpacing(4);
  topRow->addWidget(removeBtn);
  outer->addLayout(topRow);

  // ─── Row 2: ID ─────────────────────────────────────────────────────────
  auto *idRow = new QHBoxLayout;
  idRow->setSpacing(6);
  auto *idLabel = new QLabel("ID:", this);
  idLabel->setFixedWidth(22);
  m_idEdit = new QLineEdit(this);
  m_idEdit->setPlaceholderText("e.g. RSI_1  (used in Conditions tab)");
  m_idEdit->setToolTip(
      "Unique ID used to reference this indicator in conditions");
  idRow->addWidget(idLabel);
  idRow->addWidget(m_idEdit);
  outer->addLayout(idRow);

  // ─── Row 3: Dynamic params (QFormLayout, rebuilt on type change) ───────
  m_paramForm = new QFormLayout;
  m_paramForm->setSpacing(4);
  m_paramForm->setContentsMargins(0, 0, 0, 0);
  m_paramForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  outer->addLayout(m_paramForm);

  // ─── Row 4: Output selector (hidden for single-output indicators) ───────
  auto *outRow = new QHBoxLayout;
  m_outputLabel = new QLabel("Output series:", this);
  m_outputCombo = new QComboBox(this);
  m_outputCombo->setToolTip(
      "Which output series to use in conditions.\n"
      "Multi-output indicators: BBANDS (upper/middle/lower),\n"
      "MACD (macd/signal/hist), STOCH (slowk/slowd), etc.");
  outRow->addWidget(m_outputLabel);
  outRow->addWidget(m_outputCombo);
  outRow->addStretch();
  outer->addLayout(outRow);

  // ─── Wire type change ───────────────────────────────────────────────────
  connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) {
            QString type = m_typeCombo->currentData().toString();
            if (!type.isEmpty())
              onTypeChanged(type);
          });

  // ─── Wire change signals ────────────────────────────────────────────────
  auto sig = [this]() { emit changed(); };
  connect(m_idEdit, &QLineEdit::textChanged, this, sig);
  connect(m_symCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          sig);
  connect(m_tfCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          sig);
  connect(m_priceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, sig);
  connect(m_outputCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, sig);

  // ─── Set initial type to RSI (first real indicator after group headers) ─
  // Block signals so we don't fire onTypeChanged during construction;
  // we call it manually once at the end.
  m_typeCombo->blockSignals(true);
  int firstReal = -1;
  // Prefer RSI as a sensible default
  for (int i = 0; i < m_typeCombo->count(); ++i) {
    if (m_typeCombo->itemData(i).toString() == "RSI") {
      firstReal = i;
      break;
    }
  }
  // Fallback: any real (non-header) item
  if (firstReal < 0) {
    for (int i = 0; i < m_typeCombo->count(); ++i) {
      if (!m_typeCombo->itemData(i).toString().isEmpty()) {
        firstReal = i;
        break;
      }
    }
  }
  if (firstReal >= 0)
    m_typeCombo->setCurrentIndex(firstReal);
  m_typeCombo->blockSignals(false);

  // Trigger param build for the default type
  if (firstReal >= 0) {
    QString defType = m_typeCombo->currentData().toString();
    if (!defType.isEmpty())
      onTypeChanged(defType);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// onTypeChanged — rebuild param rows and output combo from catalog
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorRowWidget::onTypeChanged(const QString &type) {
  IndicatorMeta meta;
  if (!IndicatorCatalog::instance().find(type, meta)) {
    clearParamRows();
    return;
  }
  m_currentMeta = meta;

  // Auto-set ID if empty or still matches an auto-generated pattern
  QString currentId = m_idEdit->text().trimmed();
  bool autoId = currentId.isEmpty() ||
                currentId.contains(QRegularExpression("^[A-Za-z0-9_]+_\\d+$"));
  if (autoId)
    m_idEdit->setText(QString("%1_%2").arg(type).arg(m_indexHint));

  rebuildParamRows(meta);

  // Rebuild output combo
  m_outputCombo->blockSignals(true);
  m_outputCombo->clear();
  m_outputCombo->addItems(meta.outputs);
  m_outputCombo->blockSignals(false);

  bool multiOut = meta.outputs.size() > 1;
  m_outputLabel->setVisible(multiOut);
  m_outputCombo->setVisible(multiOut);

  emit changed();
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildParamRows — dynamically generate labeled QLineEdit per param
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorRowWidget::rebuildParamRows(const IndicatorMeta &meta) {
  clearParamRows();
  m_paramEdits.clear();

  if (meta.paramMeta.isEmpty())
    return;

  for (const IndicatorParamMeta &pm : meta.paramMeta) {
    auto *lbl = new QLabel(pm.label + ":", this);
    auto *edit = new QLineEdit(this);

    // Set default value from catalog
    if (pm.type == "int")
      edit->setText(QString::number((int)pm.defVal));
    else
      edit->setText(QString::number(pm.defVal, 'g'));

    // Tooltip shows range
    edit->setToolTip(
        QString("%1\nRange: %2 – %3  (default: %4)")
            .arg(pm.label)
            .arg(pm.type == "int" ? QString::number((int)pm.minVal)
                                  : QString::number(pm.minVal, 'g'))
            .arg(pm.type == "int" ? QString::number((int)pm.maxVal)
                                  : QString::number(pm.maxVal, 'g'))
            .arg(pm.type == "int" ? QString::number((int)pm.defVal)
                                  : QString::number(pm.defVal, 'g')));
    edit->setPlaceholderText(QString("e.g. %1  or  {{PARAM_NAME}}")
                                 .arg(pm.type == "int"
                                          ? QString::number((int)pm.defVal)
                                          : QString::number(pm.defVal, 'g')));

    connect(edit, &QLineEdit::textChanged, this, &IndicatorRowWidget::changed);

    m_paramForm->addRow(lbl, edit);
    m_paramWidgets << lbl << edit;
    m_paramEdits << edit;
  }
}

void IndicatorRowWidget::clearParamRows() {
  // Remove all rows from the form layout and delete the widgets
  while (m_paramForm->rowCount() > 0)
    m_paramForm->removeRow(0);
  m_paramWidgets.clear();
  m_paramEdits.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// populate — fill from an existing IndicatorDefinition
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorRowWidget::populate(const IndicatorDefinition &ind) {
  // Select type in combo by stored type string
  for (int i = 0; i < m_typeCombo->count(); ++i) {
    if (m_typeCombo->itemData(i).toString() == ind.type) {
      m_typeCombo->setCurrentIndex(i);
      break;
    }
  }
  // onTypeChanged fires and rebuilds params/output — now override with saved
  // values
  m_idEdit->setText(ind.id);

  int tfIdx =
      m_tfCombo->findText(ind.timeframe.isEmpty() ? "D" : ind.timeframe);
  m_tfCombo->setCurrentIndex(tfIdx >= 0 ? tfIdx : m_tfCombo->findText("D"));

  int prIdx = m_priceCombo->findText(ind.priceField.isEmpty() ? "close"
                                                              : ind.priceField);
  m_priceCombo->setCurrentIndex(prIdx >= 0 ? prIdx : 0);

  int symIdx = m_symCombo->findText(ind.symbolId);
  if (symIdx >= 0)
    m_symCombo->setCurrentIndex(symIdx);

  // Restore params — m_paramEdits[0..2] correspond to param1/param2/param3
  QStringList vals = {ind.periodParam, ind.period2Param, ind.param3Str};
  for (int i = 0; i < m_paramEdits.size() && i < vals.size(); ++i)
    if (!vals[i].isEmpty())
      m_paramEdits[i]->setText(vals[i]);

  // Restore output selector
  if (m_outputCombo->isVisible() && !ind.outputSelector.isEmpty()) {
    int outIdx = m_outputCombo->findText(ind.outputSelector);
    if (outIdx >= 0)
      m_outputCombo->setCurrentIndex(outIdx);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// definition — extract current values
// ─────────────────────────────────────────────────────────────────────────────
IndicatorDefinition IndicatorRowWidget::definition() const {
  IndicatorDefinition ind;
  ind.id = m_idEdit->text().trimmed();
  ind.type = m_typeCombo->currentData().toString();
  ind.symbolId = m_symCombo->currentText();
  ind.timeframe = m_tfCombo->currentText();
  ind.priceField = m_priceCombo->currentText();
  ind.outputSelector =
      m_outputCombo->isVisible()
          ? m_outputCombo->currentText()
          : (m_currentMeta.outputs.isEmpty() ? QString()
                                             : m_currentMeta.outputs.first());

  if (m_paramEdits.size() >= 1) {
    ind.periodParam = m_paramEdits[0]->text().trimmed();
    bool ok = false;
    double v = ind.periodParam.toDouble(&ok);
    if (ok)
      ind.param1 = v;
  }
  if (m_paramEdits.size() >= 2)
    ind.period2Param = m_paramEdits[1]->text().trimmed();
  if (m_paramEdits.size() >= 3) {
    ind.param3Str = m_paramEdits[2]->text().trimmed();
    bool ok = false;
    double v = ind.param3Str.toDouble(&ok);
    if (ok)
      ind.param3 = v;
  }

  // Store param labels for self-documenting JSON
  if (!m_currentMeta.paramMeta.isEmpty()) {
    for (const auto &pm : m_currentMeta.paramMeta) {
      if (pm.key == "param1")
        ind.param1Label = pm.label;
      else if (pm.key == "param2")
        ind.param2Label = pm.label;
      else if (pm.key == "param3")
        ind.param3Label = pm.label;
    }
  }

  return ind;
}

// ─────────────────────────────────────────────────────────────────────────────
// setSymbolIds — update the symbol combo without losing selection
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorRowWidget::setSymbolIds(const QStringList &ids) {
  QString cur = m_symCombo->currentText();
  m_symCombo->blockSignals(true);
  m_symCombo->clear();
  m_symCombo->addItems(ids.isEmpty() ? QStringList{"REF_1"} : ids);
  int idx = m_symCombo->findText(cur);
  m_symCombo->setCurrentIndex(idx >= 0 ? idx : 0);
  m_symCombo->blockSignals(false);
}
