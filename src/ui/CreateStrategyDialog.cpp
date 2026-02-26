#include "ui/CreateStrategyDialog.h"
#include "ui_CreateStrategyDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtUiTools/QUiLoader>

CreateStrategyDialog::CreateStrategyDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::CreateStrategyDialog) {
  ui->setupUi(this);

  // Setup main layout for sParamFrame if needed
  if (!ui->sParamFrame->layout()) {
    new QVBoxLayout(ui->sParamFrame);
  }
  ui->sParamFrame->layout()->setContentsMargins(0, 0, 0, 0);

  // Setup shell connections
  connect(ui->okButton, &QPushButton::clicked, this,
          &CreateStrategyDialog::accept);
  connect(ui->cancelButton, &QPushButton::clicked, this,
          &CreateStrategyDialog::reject);
  connect(ui->m_typeCombo, &QComboBox::currentTextChanged, this,
          &CreateStrategyDialog::onTypeChanged);
}

CreateStrategyDialog::~CreateStrategyDialog() { delete ui; }

#include "services/StrategyTemplateRepository.h"
#include "ui/SymbolBindingWidget.h"
#include <QVariantMap>

void CreateStrategyDialog::setNextSrNo(int id) {
  m_nextId = id;
  ui->m_srNoLabel->setText(QString("%1").arg(id, 3, 10, QChar('0')));
  onTypeChanged(ui->m_typeCombo->currentText()); // Refresh name
}

void CreateStrategyDialog::onTypeChanged(const QString &type) {
  QString t = type.toLower().trimmed().remove("-").remove(" ");

  // Auto-generate name logic
  QString currentName = ui->m_nameEdit->text();
  QString idStr = ui->m_srNoLabel->text();
  if (currentName.isEmpty() || currentName.startsWith(idStr + "_")) {
    ui->m_nameEdit->setText(QString("%1_%2").arg(idStr).arg(type));
  }

  // Clear existing UI
  clearStrategyUI();

  // Restore left-pane risk group visibility (may have been hidden for templates)
  ui->riskGroup->setVisible(true);

  // Check if it's a template
  m_isTemplateMode = false;
  for (const auto &tmpl : m_templates) {
    if (tmpl.name == type) {
      m_isTemplateMode = true;
      m_selectedTemplate = tmpl;
      setupTemplateUI(tmpl);

      // Hide the left-pane risk group — template mode has comprehensive
      // risk management in the right pane
      ui->riskGroup->setVisible(false);

      // Sync the left-pane fields as fallback values
      ui->m_targetSpin->setValue(tmpl.riskDefaults.targetPercent);
      ui->m_stopLossSpin->setValue(tmpl.riskDefaults.stopLossPercent);
      ui->m_isTrailingCheck->setChecked(tmpl.riskDefaults.trailingEnabled);

      return;
    }
  }

  QUiLoader loader;
  QFile file;

  if (t == "jodiatm") {
    file.setFileName(":/forms/stretegies/JodiATM/inputParam.ui");
  } else if (t == "tspecial") {
    file.setFileName(":/forms/stretegies/TSpecial/tspecialParam.ui");
  }

  if (file.open(QFile::ReadOnly)) {
    m_strategyWidget = loader.load(&file, this);
    file.close();

    if (m_strategyWidget) {
      ui->sParamFrame->layout()->addWidget(m_strategyWidget);

      // Initialize common fields in the loaded UI (segment combo, etc.)
      auto *segCombo = findWidget<QComboBox>("m_segmentCombo");
      if (segCombo) {
        segCombo->clear();
        segCombo->addItem("NSE Cash Market (CM)", 1);
        segCombo->addItem("NSE Futures & Options (FO)", 2);
        segCombo->addItem("BSE Cash Market (CM)", 11);
        segCombo->addItem("BSE Futures & Options (FO)", 12);
        segCombo->setCurrentIndex(1);

        // ... (segment help connection)
      }

      // Set precision for all spinboxes in loaded UI
      const auto spinboxes = m_strategyWidget->findChildren<QDoubleSpinBox *>();
      for (auto *sb : spinboxes)
        sb->setSingleStep(0.05);
    }
  } else {
    // Fallback: Custom JSON editor if no .ui found
    m_strategyWidget = new QWidget(this);
    QVBoxLayout *lay = new QVBoxLayout(m_strategyWidget);
    lay->addWidget(new QLabel("Custom Parameters (JSON):", m_strategyWidget));
    QTextEdit *edit = new QTextEdit(m_strategyWidget);
    edit->setObjectName("m_paramsEdit");
    lay->addWidget(edit);
    ui->sParamFrame->layout()->addWidget(m_strategyWidget);
  }
}

void CreateStrategyDialog::loadTemplates() {
  StrategyTemplateRepository &repo = StrategyTemplateRepository::instance();
  if (repo.isOpen()) {
    m_templates = repo.loadAllTemplates();
  }
}

void CreateStrategyDialog::setStrategyTypes(const QStringList &types) {
  ui->m_typeCombo->clear();
  ui->m_typeCombo->addItems(types);

  // Add templates if any
  loadTemplates();
  if (!m_templates.isEmpty()) {
    ui->m_typeCombo->insertSeparator(ui->m_typeCombo->count());
    for (const auto &tmpl : m_templates) {
      ui->m_typeCombo->addItem(tmpl.name);
    }
  }

  if (ui->m_srNoLabel->text() != "---") {
    onTypeChanged(ui->m_typeCombo->currentText());
  }
}

QString CreateStrategyDialog::instanceName() const {
  return ui->m_nameEdit->text().trimmed();
}
QString CreateStrategyDialog::description() const {
  return ui->m_descriptionEdit->text().trimmed();
}
QString CreateStrategyDialog::strategyType() const {
  if (m_isTemplateMode)
    return QString("template:%1").arg(m_selectedTemplate.templateId);
  return ui->m_typeCombo->currentText().trimmed();
}
QString CreateStrategyDialog::symbol() const {
  if (m_isTemplateMode)
    return "MULTI-LEG"; // or derived from main binding
  auto *edit = findWidget<QLineEdit>("m_symbolEdit");
  return edit ? edit->text().trimmed() : QString();
}
QString CreateStrategyDialog::account() const {
  return ui->m_accountEdit->text().trimmed();
}

int CreateStrategyDialog::segment() const {
  auto *combo = findWidget<QComboBox>("m_segmentCombo");
  return combo ? combo->currentData().toInt() : 0;
}

double CreateStrategyDialog::stopLoss() const {
  if (m_isTemplateMode && m_tmplSlPctSpin)
    return m_tmplSlPctSpin->value();
  // Try the .ui left-pane spin first (for non-template mode)
  return ui->m_stopLossSpin->value();
}

double CreateStrategyDialog::target() const {
  if (m_isTemplateMode && m_tmplTgtPctSpin)
    return m_tmplTgtPctSpin->value();
  return ui->m_targetSpin->value();
}

double CreateStrategyDialog::entryPrice() const {
  auto *sb = findWidget<QDoubleSpinBox>("m_entryPriceSpin");
  return sb ? sb->value() : 0.0;
}

int CreateStrategyDialog::quantity() const {
  if (m_isTemplateMode && m_globalQtySpin) {
    return m_globalQtySpin->value();
  }
  auto *sb = findWidget<QSpinBox>("m_qtySpin");
  return sb ? sb->value() : 0;
}

void CreateStrategyDialog::clearStrategyUI() {
  if (m_strategyWidget) {
    m_strategyWidget->hide();
    m_strategyWidget->deleteLater();
    m_strategyWidget = nullptr;
  }
  m_globalQtySpin = nullptr;
  m_productCombo = nullptr;
  m_orderTypeCombo = nullptr;
  m_priceSpin = nullptr;
  m_triggerSpin = nullptr;

  // Template risk widgets
  m_tmplSlPctSpin     = nullptr;
  m_tmplTgtPctSpin    = nullptr;
  m_tmplTrailingCheck = nullptr;
  m_tmplTrailTrigger  = nullptr;
  m_tmplTrailAmount   = nullptr;
  m_tmplTimeExitCheck = nullptr;
  m_tmplTimeExitEdit  = nullptr;
  m_tmplMaxTradesSpin = nullptr;
  m_tmplMaxLossSpin   = nullptr;
  m_entryCondLabel    = nullptr;
  m_exitCondLabel     = nullptr;

  // Helper to clear layout items
  QLayout *lay = ui->sParamFrame->layout();
  if (lay) {
    QLayoutItem *item;
    while ((item = lay->takeAt(0)) != nullptr) {
      if (item->widget()) {
        item->widget()->hide();
        item->widget()->deleteLater();
      }
      delete item;
    }
  }

  m_symbolBindings.clear();
  m_paramInputs.clear();
}

void CreateStrategyDialog::setupTemplateUI(const StrategyTemplate &tmpl) {
  m_strategyWidget = new QWidget(this);
  auto *layout = new QVBoxLayout(m_strategyWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  // ── Helper: consistent group box styling (light theme) ──
  auto styleGroupBox = [](QGroupBox *gb, const QString &accentColor = "#2563eb") {
    gb->setStyleSheet(QString(
      "QGroupBox { font-weight: bold; border: 1px solid #e2e8f0;"
      "  border-radius: 6px; margin-top: 10px; padding: 14px 10px 10px 10px;"
      "  background-color: #f8fafc; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 10px;"
      "  padding: 0 6px; color: %1; background-color: #f8fafc;"
      "  font-size: 11px; font-weight: 700; }").arg(accentColor));
  };

  // ══════════════════════════════════════════════════════════════
  // 1. SYMBOL BINDINGS
  // ══════════════════════════════════════════════════════════════
  if (!tmpl.symbols.isEmpty()) {
    auto *gb = new QGroupBox("Symbol Bindings", m_strategyWidget);
    styleGroupBox(gb, "#0e7490");
    auto *gbLay = new QVBoxLayout(gb);
    gbLay->setSpacing(4);
    gbLay->setContentsMargins(6, 8, 6, 6);
    for (const auto &def : tmpl.symbols) {
      auto *w = new SymbolBindingWidget(def, gb);
      w->setStyleSheet(
        "SymbolBindingWidget { background: #ffffff; border: 1px solid #e2e8f0;"
        "  border-radius: 5px; }"
        "QLabel { color: #475569; font-size: 11px; background: transparent; }"
        "QLabel#tokenLabel { color: #0e7490; font-size: 10px; font-family: monospace; }"
        "QLineEdit { background: #ffffff; border: 1px solid #cbd5e1;"
        "  border-radius: 3px; color: #0f172a; padding: 4px 6px; font-size: 11px; }"
        "QLineEdit:focus { border-color: #3b82f6; background: #f0f9ff; }"
        "QPushButton { background: #f1f5f9; color: #475569; border: 1px solid #cbd5e1;"
        "  border-radius: 3px; padding: 3px 8px; font-size: 11px; }"
        "QPushButton:hover { background: #e2e8f0; color: #1e293b; }"
        "QPushButton#clearBtn { background: #fef2f2; color: #dc2626;"
        "  border-color: #fecaca; }"
        "QPushButton#clearBtn:hover { background: #fee2e2; }"
        "QSpinBox { background: #ffffff; border: 1px solid #cbd5e1;"
        "  border-radius: 3px; color: #0f172a; padding: 2px 4px; font-size: 11px; }"
        "QTableWidget { background: #ffffff; border: 1px solid #e2e8f0;"
        "  color: #0f172a; font-size: 10px; gridline-color: #f1f5f9; }"
        "QTableWidget::item:selected { background: #dbeafe; color: #1e40af; }"
        "QHeaderView::section { background: #f8fafc; color: #64748b;"
        "  padding: 3px 6px; border: none; border-bottom: 1px solid #e2e8f0;"
        "  font-size: 10px; }"
      );
      gbLay->addWidget(w);
      m_symbolBindings[def.id] = w;
    }
    layout->addWidget(gb);
  }

  // ══════════════════════════════════════════════════════════════
  // 2. CONFIGURABLE PARAMETERS — editable values that feed into conditions
  //    Placed BEFORE conditions so the user understands what they control
  // ══════════════════════════════════════════════════════════════
  if (!tmpl.params.isEmpty()) {
    auto *gb = new QGroupBox(
      QString("⚙ Parameters (%1)").arg(tmpl.params.size()),
      m_strategyWidget);
    styleGroupBox(gb, "#7c3aed");
    auto *form = new QFormLayout(gb);
    form->setContentsMargins(8, 8, 8, 6);
    form->setSpacing(6);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    auto *paramHint = new QLabel(
      "<span style='color:#64748b; font-size:10px; font-weight:400; text-transform:none;'>"
      "These values feed into the strategy conditions below. "
      "Change them to adjust entry/exit thresholds.</span>", gb);
    paramHint->setWordWrap(true);
    paramHint->setTextFormat(Qt::RichText);
    paramHint->setStyleSheet("background: transparent;");
    form->addRow(paramHint);

    for (const auto &p : tmpl.params) {
      QWidget *editor = nullptr;

      if (p.isExpression()) {
        // ── Expression parameter: show formula + optional override ──
        auto *exprWidget = new QWidget(gb);
        auto *exprLay = new QVBoxLayout(exprWidget);
        exprLay->setContentsMargins(0, 0, 0, 0);
        exprLay->setSpacing(3);

        // Formula display
        auto *formulaLbl = new QLabel(
          QString("<span style='color:#7c3aed; font-size:10px; text-transform:none;'>"
                  "ƒ = <code>%1</code></span>")
              .arg(p.expression.toHtmlEscaped()),
          exprWidget);
        formulaLbl->setTextFormat(Qt::RichText);
        formulaLbl->setStyleSheet("background: transparent;");
        exprLay->addWidget(formulaLbl);

        // Override row: checkbox + spin box
        auto *overrideRow = new QHBoxLayout;
        overrideRow->setSpacing(4);
        auto *overrideCheck = new QCheckBox("Override with fixed value:", exprWidget);
        overrideCheck->setChecked(false);
        overrideCheck->setStyleSheet("font-size: 10px; color: #64748b;");

        auto *overrideSpin = new QDoubleSpinBox(exprWidget);
        overrideSpin->setRange(-999999, 999999);
        overrideSpin->setDecimals(4);
        overrideSpin->setValue(p.defaultValue.toDouble());
        overrideSpin->setFixedWidth(100);
        overrideSpin->setEnabled(false);

        connect(overrideCheck, &QCheckBox::toggled, overrideSpin, &QWidget::setEnabled);

        overrideRow->addWidget(overrideCheck);
        overrideRow->addWidget(overrideSpin);
        overrideRow->addStretch();
        exprLay->addLayout(overrideRow);

        editor = exprWidget;
        // Store the spin as the input so parameters() can read it
        // Tag the override checkbox for reading
        overrideCheck->setObjectName("override_" + p.name);
        overrideSpin->setObjectName("value_" + p.name);
        m_paramInputs[p.name] = overrideSpin;

        // Also connect for condition refresh
        connect(overrideSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &CreateStrategyDialog::refreshConditionDisplay);
        connect(overrideCheck, &QCheckBox::toggled,
                this, &CreateStrategyDialog::refreshConditionDisplay);

      } else if (p.valueType == ParamValueType::Bool) {
        auto *cb = new QCheckBox(gb);
        cb->setChecked(p.defaultValue.toBool());
        editor = cb;
      } else if (p.valueType == ParamValueType::Int) {
        auto *sb = new QSpinBox(gb);
        int lo = p.minValue.isValid() ? p.minValue.toInt() : -999999;
        int hi = p.maxValue.isValid() ? p.maxValue.toInt() : 999999;
        sb->setRange(lo, hi);
        sb->setValue(p.defaultValue.toInt());
        sb->setFixedWidth(100);
        editor = sb;
      } else if (p.valueType == ParamValueType::Double) {
        auto *dsb = new QDoubleSpinBox(gb);
        double lo = p.minValue.isValid() ? p.minValue.toDouble() : -999999.0;
        double hi = p.maxValue.isValid() ? p.maxValue.toDouble() : 999999.0;
        dsb->setRange(lo, hi);
        dsb->setDecimals(2);
        dsb->setSingleStep(0.1);
        dsb->setValue(p.defaultValue.toDouble());
        dsb->setFixedWidth(100);
        editor = dsb;
      } else {
        auto *le = new QLineEdit(p.defaultValue.toString(), gb);
        le->setFixedWidth(160);
        editor = le;
      }

      if (editor) {
        // Apply locked state
        if (p.locked && !p.isExpression()) {
          editor->setEnabled(false);
          editor->setToolTip("🔒 Locked by template designer");
        } else if (!p.description.isEmpty()) {
          editor->setToolTip(p.description);
        }

        QString labelText = p.label.isEmpty() ? p.name : p.label;
        if (p.locked)
          labelText += " 🔒";
        if (p.minValue.isValid() && p.maxValue.isValid())
          labelText += QString(" [%1–%2]").arg(
            p.minValue.toString(), p.maxValue.toString());

        // Row: editor + description hint
        auto *row = new QWidget(gb);
        auto *rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->setSpacing(4);
        rowLay->addWidget(editor);
        if (!p.description.isEmpty() && !p.isExpression()) {
          auto *hint = new QLabel(
            QString("<span style='color:#94a3b8; font-size:9px; text-transform:none;'>%1</span>")
              .arg(p.description), row);
          hint->setWordWrap(true);
          hint->setStyleSheet("background: transparent;");
          hint->setTextFormat(Qt::RichText);
          rowLay->addWidget(hint, 1);
        }
        rowLay->addStretch();

        form->addRow(labelText + ":", row);

        // Only store non-expression params here (expressions stored above)
        if (!p.isExpression())
          m_paramInputs[p.name] = editor;

        // Connect value changes to refresh condition display
        if (!p.isExpression()) {
          if (auto *sb = qobject_cast<QSpinBox*>(editor))
            connect(sb, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, &CreateStrategyDialog::refreshConditionDisplay);
          else if (auto *dsb = qobject_cast<QDoubleSpinBox*>(editor))
            connect(dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &CreateStrategyDialog::refreshConditionDisplay);
          else if (auto *cb = qobject_cast<QCheckBox*>(editor))
            connect(cb, &QCheckBox::toggled,
                    this, &CreateStrategyDialog::refreshConditionDisplay);
          else if (auto *le = qobject_cast<QLineEdit*>(editor))
            connect(le, &QLineEdit::textChanged,
                    this, &CreateStrategyDialog::refreshConditionDisplay);
        }
      }
    }
    layout->addWidget(gb);
  }

  // ══════════════════════════════════════════════════════════════
  // 3. STRATEGY LOGIC — read-only condition wireframe with live param values
  // ══════════════════════════════════════════════════════════════
  {
    auto *gb = new QGroupBox("Strategy Logic (from template)", m_strategyWidget);
    styleGroupBox(gb, "#16a34a");
    auto *gbLay = new QVBoxLayout(gb);
    gbLay->setSpacing(6);
    gbLay->setContentsMargins(8, 8, 8, 6);

    auto *logicHint = new QLabel(
      "<span style='color:#64748b; font-size:10px; font-weight:400; text-transform:none;'>"
      "Condition structure is defined in the template. "
      "<b style='color:#7c3aed;'>⚙ parameter</b> values above are shown inline.</span>", gb);
    logicHint->setWordWrap(true);
    logicHint->setTextFormat(Qt::RichText);
    logicHint->setStyleSheet("background: transparent;");
    gbLay->addWidget(logicHint);

    // Entry condition label (will be refreshed live)
    bool hasEntry = !tmpl.entryCondition.isEmpty() || tmpl.entryCondition.isLeaf();
    bool hasExit  = !tmpl.exitCondition.isEmpty()  || tmpl.exitCondition.isLeaf();

    if (hasEntry) {
      m_entryCondLabel = new QLabel(gb);
      m_entryCondLabel->setWordWrap(true);
      m_entryCondLabel->setTextFormat(Qt::RichText);
      m_entryCondLabel->setStyleSheet("background: transparent; padding: 2px;");
      gbLay->addWidget(m_entryCondLabel);
    }

    if (hasExit) {
      m_exitCondLabel = new QLabel(gb);
      m_exitCondLabel->setWordWrap(true);
      m_exitCondLabel->setTextFormat(Qt::RichText);
      m_exitCondLabel->setStyleSheet("background: transparent; padding: 2px;");
      gbLay->addWidget(m_exitCondLabel);
    }

    if (!hasEntry && !hasExit) {
      auto *noLbl = new QLabel(
        "<i style='color:#94a3b8; text-transform:none;'>No entry/exit conditions defined in this template.</i>", gb);
      noLbl->setStyleSheet("background: transparent;");
      gbLay->addWidget(noLbl);
    }

    // Indicator summary (compact)
    if (!tmpl.indicators.isEmpty()) {
      auto *indFrame = new QWidget(gb);
      auto *indLay = new QVBoxLayout(indFrame);
      indLay->setContentsMargins(0, 4, 0, 0);
      indLay->setSpacing(2);

      auto *indTitle = new QLabel(
        "<span style='color:#7c3aed; font-weight:700; font-size:11px;'>"
        "📊 INDICATORS</span>", indFrame);
      indTitle->setStyleSheet("background: transparent;");
      indLay->addWidget(indTitle);

      for (const auto &ind : tmpl.indicators) {
        QString desc = QString(
          "<span style='color:#334155; font-size:10px; text-transform:none;'>"
          "• <b>%1</b> (%2) on <span style='color:#0e7490;'>%3</span>")
          .arg(ind.id, ind.type, ind.symbolId);

        QStringList paramParts;
        if (!ind.periodParam.isEmpty()) {
          QString p1 = ind.periodParam;
          if (p1.startsWith("{{") && p1.endsWith("}}"))
            p1 = QString("<span style='color:#7c3aed; font-weight:600;'>%1</span>").arg(p1);
          paramParts << QString("%1: %2").arg(
            ind.param1Label.isEmpty() ? "P1" : ind.param1Label, p1);
        }
        if (!ind.period2Param.isEmpty()) {
          QString p2 = ind.period2Param;
          if (p2.startsWith("{{") && p2.endsWith("}}"))
            p2 = QString("<span style='color:#7c3aed; font-weight:600;'>%1</span>").arg(p2);
          paramParts << QString("%1: %2").arg(
            ind.param2Label.isEmpty() ? "P2" : ind.param2Label, p2);
        }
        if (!paramParts.isEmpty())
          desc += " — " + paramParts.join(", ");

        if (!ind.timeframe.isEmpty() && ind.timeframe != "D")
          desc += QString(" [TF:%1]").arg(ind.timeframe);

        desc += "</span>";

        auto *indLbl = new QLabel(desc, indFrame);
        indLbl->setWordWrap(true);
        indLbl->setTextFormat(Qt::RichText);
        indLbl->setStyleSheet("background: transparent;");
        indLay->addWidget(indLbl);
      }
      gbLay->addWidget(indFrame);
    }

    layout->addWidget(gb);

    // Trigger initial condition render
    refreshConditionDisplay();
  }

  // ══════════════════════════════════════════════════════════════
  // 4. RISK MANAGEMENT — full editable risk from template defaults
  // ══════════════════════════════════════════════════════════════
  {
    auto *gb = new QGroupBox("Risk Management", m_strategyWidget);
    styleGroupBox(gb, "#dc2626");
    auto *riskLay = new QVBoxLayout(gb);
    riskLay->setContentsMargins(8, 8, 8, 8);
    riskLay->setSpacing(6);

    const auto &r = tmpl.riskDefaults;

    // ── Row 1: Stop-Loss + Target side by side ──
    auto *slTgtRow = new QHBoxLayout;
    slTgtRow->setSpacing(16);

    // Stop-Loss
    auto *slFrame = new QWidget(gb);
    auto *slLay = new QFormLayout(slFrame);
    slLay->setContentsMargins(0, 0, 0, 0);
    slLay->setSpacing(4);
    slLay->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_tmplSlPctSpin = new QDoubleSpinBox(slFrame);
    m_tmplSlPctSpin->setRange(0, 100);
    m_tmplSlPctSpin->setSingleStep(0.1);
    m_tmplSlPctSpin->setDecimals(2);
    m_tmplSlPctSpin->setSuffix(" %");
    m_tmplSlPctSpin->setValue(r.stopLossPercent);
    m_tmplSlPctSpin->setFixedWidth(100);
    if (r.stopLossLocked) {
      m_tmplSlPctSpin->setEnabled(false);
      m_tmplSlPctSpin->setToolTip("Locked by template");
    }
    slLay->addRow("Stop-Loss:", m_tmplSlPctSpin);

    // Target
    auto *tgtFrame = new QWidget(gb);
    auto *tgtLay = new QFormLayout(tgtFrame);
    tgtLay->setContentsMargins(0, 0, 0, 0);
    tgtLay->setSpacing(4);
    tgtLay->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_tmplTgtPctSpin = new QDoubleSpinBox(tgtFrame);
    m_tmplTgtPctSpin->setRange(0, 1000);
    m_tmplTgtPctSpin->setSingleStep(0.1);
    m_tmplTgtPctSpin->setDecimals(2);
    m_tmplTgtPctSpin->setSuffix(" %");
    m_tmplTgtPctSpin->setValue(r.targetPercent);
    m_tmplTgtPctSpin->setFixedWidth(100);
    if (r.targetLocked) {
      m_tmplTgtPctSpin->setEnabled(false);
      m_tmplTgtPctSpin->setToolTip("Locked by template");
    }
    tgtLay->addRow("Target:", m_tmplTgtPctSpin);

    slTgtRow->addWidget(slFrame);
    slTgtRow->addWidget(tgtFrame);
    slTgtRow->addStretch();
    riskLay->addLayout(slTgtRow);

    // ── Row 2: Trailing stop ──
    auto *trailRow = new QHBoxLayout;
    trailRow->setSpacing(8);

    m_tmplTrailingCheck = new QCheckBox("Trailing SL", gb);
    m_tmplTrailingCheck->setChecked(r.trailingEnabled);

    m_tmplTrailTrigger = new QDoubleSpinBox(gb);
    m_tmplTrailTrigger->setRange(0, 100);
    m_tmplTrailTrigger->setSingleStep(0.1);
    m_tmplTrailTrigger->setDecimals(2);
    m_tmplTrailTrigger->setSuffix(" % trigger");
    m_tmplTrailTrigger->setValue(r.trailingTriggerPct);
    m_tmplTrailTrigger->setFixedWidth(120);
    m_tmplTrailTrigger->setEnabled(r.trailingEnabled);

    m_tmplTrailAmount = new QDoubleSpinBox(gb);
    m_tmplTrailAmount->setRange(0, 100);
    m_tmplTrailAmount->setSingleStep(0.1);
    m_tmplTrailAmount->setDecimals(2);
    m_tmplTrailAmount->setSuffix(" % trail");
    m_tmplTrailAmount->setValue(r.trailingAmountPct);
    m_tmplTrailAmount->setFixedWidth(120);
    m_tmplTrailAmount->setEnabled(r.trailingEnabled);

    connect(m_tmplTrailingCheck, &QCheckBox::toggled, m_tmplTrailTrigger, &QWidget::setEnabled);
    connect(m_tmplTrailingCheck, &QCheckBox::toggled, m_tmplTrailAmount, &QWidget::setEnabled);

    trailRow->addWidget(m_tmplTrailingCheck);
    trailRow->addWidget(m_tmplTrailTrigger);
    trailRow->addWidget(m_tmplTrailAmount);
    trailRow->addStretch();
    riskLay->addLayout(trailRow);

    // ── Row 3: Time exit + Daily limits side by side ──
    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(16);

    // Time exit
    auto *timeFrame = new QWidget(gb);
    auto *timeLay = new QHBoxLayout(timeFrame);
    timeLay->setContentsMargins(0, 0, 0, 0);
    timeLay->setSpacing(6);

    m_tmplTimeExitCheck = new QCheckBox("Exit at:", timeFrame);
    m_tmplTimeExitCheck->setChecked(r.timeExitEnabled);

    m_tmplTimeExitEdit = new QLineEdit(r.exitTime, timeFrame);
    m_tmplTimeExitEdit->setInputMask("99:99");
    m_tmplTimeExitEdit->setFixedWidth(55);
    m_tmplTimeExitEdit->setEnabled(r.timeExitEnabled);

    connect(m_tmplTimeExitCheck, &QCheckBox::toggled, m_tmplTimeExitEdit, &QWidget::setEnabled);

    timeLay->addWidget(m_tmplTimeExitCheck);
    timeLay->addWidget(m_tmplTimeExitEdit);

    // Daily limits
    auto *limitsFrame = new QWidget(gb);
    auto *limitsLay = new QFormLayout(limitsFrame);
    limitsLay->setContentsMargins(0, 0, 0, 0);
    limitsLay->setSpacing(4);
    limitsLay->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_tmplMaxTradesSpin = new QSpinBox(limitsFrame);
    m_tmplMaxTradesSpin->setRange(1, 999);
    m_tmplMaxTradesSpin->setValue(r.maxDailyTrades);
    m_tmplMaxTradesSpin->setFixedWidth(70);
    limitsLay->addRow("Max trades:", m_tmplMaxTradesSpin);

    m_tmplMaxLossSpin = new QDoubleSpinBox(limitsFrame);
    m_tmplMaxLossSpin->setRange(0, 9999999);
    m_tmplMaxLossSpin->setSingleStep(500);
    m_tmplMaxLossSpin->setDecimals(0);
    m_tmplMaxLossSpin->setPrefix("₹ ");
    m_tmplMaxLossSpin->setValue(r.maxDailyLossRs);
    m_tmplMaxLossSpin->setFixedWidth(100);
    limitsLay->addRow("Max loss:", m_tmplMaxLossSpin);

    bottomRow->addWidget(timeFrame);
    bottomRow->addWidget(limitsFrame);
    bottomRow->addStretch();
    riskLay->addLayout(bottomRow);

    layout->addWidget(gb);
  }

  // ══════════════════════════════════════════════════════════════
  // 5. EXECUTION SETTINGS
  // ══════════════════════════════════════════════════════════════
  {
    auto *gb = new QGroupBox("Execution Settings", m_strategyWidget);
    styleGroupBox(gb, "#475569");
    auto *gbLay = new QFormLayout(gb);
    gbLay->setContentsMargins(8, 8, 8, 6);
    gbLay->setSpacing(5);
    gbLay->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    gbLay->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    m_globalQtySpin = new QSpinBox(gb);
    m_globalQtySpin->setRange(1, 100000);
    m_globalQtySpin->setValue(1);
    m_globalQtySpin->setFixedWidth(80);
    gbLay->addRow("Qty / Multiplier:", m_globalQtySpin);

    m_productCombo = new QComboBox(gb);
    m_productCombo->addItems({"MIS", "NRML", "CNC"});
    m_productCombo->setFixedWidth(90);
    gbLay->addRow("Product:", m_productCombo);

    m_orderTypeCombo = new QComboBox(gb);
    m_orderTypeCombo->addItems({"MARKET", "LIMIT", "SL", "SL-M"});
    m_orderTypeCombo->setFixedWidth(90);
    gbLay->addRow("Order Type:", m_orderTypeCombo);

    m_priceSpin = new QDoubleSpinBox(gb);
    m_priceSpin->setRange(0.05, 9999999.0);
    m_priceSpin->setDecimals(2);
    m_priceSpin->setValue(0.0);
    m_priceSpin->setFixedWidth(100);
    gbLay->addRow("Price:", m_priceSpin);

    m_triggerSpin = new QDoubleSpinBox(gb);
    m_triggerSpin->setRange(0.05, 9999999.0);
    m_triggerSpin->setDecimals(2);
    m_triggerSpin->setValue(0.0);
    m_triggerSpin->setFixedWidth(100);
    gbLay->addRow("Trigger Price:", m_triggerSpin);

    auto updateFields = [this]() {
      if (!m_orderTypeCombo) return;
      QString type = m_orderTypeCombo->currentText();
      bool isLimit = (type == "LIMIT" || type == "SL");
      bool isSL    = (type == "SL" || type == "SL-M");
      if (m_priceSpin)   m_priceSpin->setVisible(isLimit);
      if (m_triggerSpin) m_triggerSpin->setVisible(isSL);
    };

    connect(m_orderTypeCombo, &QComboBox::currentTextChanged, this, updateFields);
    updateFields();

    layout->addWidget(gb);
  }

  // Add stretch at bottom
  layout->addStretch();
  ui->sParamFrame->layout()->addWidget(m_strategyWidget);
}

// ─────────────────────────────────────────────────────────────────────────────
// Live condition display refresh — called when any param value changes
// ─────────────────────────────────────────────────────────────────────────────
void CreateStrategyDialog::refreshConditionDisplay() {
  if (!m_isTemplateMode) return;

  auto renderConditionHtml = [this](const ConditionNode &node,
                                    const QString &title,
                                    const QString &accentColor) -> QString {
    QString html = QString(
      "<div style='margin-bottom:4px;'>"
      "<span style='color:%1; font-weight:700; font-size:11px;'>%2</span>"
      "</div>").arg(accentColor, title);

    html += "<div style='background:#f8fafc; border:1px solid #e2e8f0;"
            " border-radius:4px; padding:6px 8px; font-size:11px;"
            " color:#334155; font-family:monospace;'>";
    html += conditionToString(node);
    html += "</div>";
    return html;
  };

  if (m_entryCondLabel) {
    m_entryCondLabel->setText(renderConditionHtml(
      m_selectedTemplate.entryCondition, "▶ ENTRY CONDITION", "#16a34a"));
  }
  if (m_exitCondLabel) {
    m_exitCondLabel->setText(renderConditionHtml(
      m_selectedTemplate.exitCondition, "■ EXIT CONDITION", "#dc2626"));
  }
}

QString
CreateStrategyDialog::conditionToString(const ConditionNode &node) const {
  if (node.nodeType == ConditionNode::NodeType::Leaf) {
    // Format the operator nicely
    QString opStr = node.op;
    if (opStr == "crosses_above")
      opStr = "<span style='color:#16a34a; font-weight:600;'>crosses above</span>";
    else if (opStr == "crosses_below")
      opStr = "<span style='color:#dc2626; font-weight:600;'>crosses below</span>";
    else
      opStr = QString("<span style='color:#2563eb; font-weight:700;'>%1</span>").arg(opStr);

    return QString("%1 %2 %3")
        .arg(operandToString(node.left))
        .arg(opStr)
        .arg(operandToString(node.right));
  } else if (node.nodeType == ConditionNode::NodeType::And) {
    QStringList parts;
    for (const auto &child : node.children)
      parts << "(" + conditionToString(child) + ")";
    return parts.join(" <span style='color:#7c3aed; font-weight:700;'>AND</span> ");
  } else if (node.nodeType == ConditionNode::NodeType::Or) {
    QStringList parts;
    for (const auto &child : node.children)
      parts << "(" + conditionToString(child) + ")";
    return parts.join(" <span style='color:#ea580c; font-weight:700;'>OR</span> ");
  }
  return "<span style='color:#94a3b8;'>True</span>";
}

QString CreateStrategyDialog::operandToString(const Operand &op) const {
  switch (op.type) {
  case Operand::Type::Price:
    return QString("<span style='color:#0e7490;'>%1</span>.<span style='color:#0f766e;'>%2</span>")
        .arg(op.symbolId, op.field);
  case Operand::Type::Indicator:
    return QString("<span style='color:#7c3aed; font-weight:600;'>%1</span>"
                   "(<span style='color:#0e7490;'>%2</span>)")
        .arg(op.indicatorId, op.symbolId);
  case Operand::Type::Constant:
    return QString("<span style='color:#b45309; font-weight:600;'>%1</span>")
        .arg(op.constantValue);
  case Operand::Type::ParamRef: {
    // Show the current value from the parameter input if available
    QString currentVal;
    bool isExpressionParam = false;
    QString exprFormula;

    // Check if this is an expression param
    for (const auto &p : m_selectedTemplate.params) {
      if (p.name == op.paramName && p.isExpression()) {
        isExpressionParam = true;
        exprFormula = p.expression;
        // Check if user overrode with a fixed value
        auto *overrideCheck = m_strategyWidget
            ? m_strategyWidget->findChild<QCheckBox*>("override_" + op.paramName)
            : nullptr;
        if (overrideCheck && overrideCheck->isChecked()) {
          isExpressionParam = false; // user overrode, show the value
        }
        break;
      }
    }

    auto it = m_paramInputs.find(op.paramName);
    if (it != m_paramInputs.end()) {
      QWidget *w = it.value();
      if (auto *sb = qobject_cast<QSpinBox*>(w))
        currentVal = QString::number(sb->value());
      else if (auto *dsb = qobject_cast<QDoubleSpinBox*>(w))
        currentVal = QString::number(dsb->value(), 'f', 2);
      else if (auto *cb = qobject_cast<QCheckBox*>(w))
        currentVal = cb->isChecked() ? "true" : "false";
      else if (auto *le = qobject_cast<QLineEdit*>(w))
        currentVal = le->text();
    }

    if (isExpressionParam) {
      // Show formula badge
      return QString("<span style='background:#fef3c7; color:#92400e; padding:1px 4px;"
                     " border-radius:2px; font-weight:700; font-size:10px;'>"
                     "ƒ %1 = <code>%2</code></span>")
          .arg(op.paramName, exprFormula.toHtmlEscaped());
    }
    if (!currentVal.isEmpty()) {
      return QString("<span style='background:#ede9fe; color:#6d28d9; padding:1px 4px;"
                     " border-radius:2px; font-weight:700;'>⚙ %1 = %2</span>")
          .arg(op.paramName, currentVal);
    }
    return QString("<span style='background:#ede9fe; color:#6d28d9; padding:1px 4px;"
                   " border-radius:2px; font-weight:700;'>⚙ %1</span>")
        .arg(op.paramName);
  }
  case Operand::Type::Formula:
    return QString("<span style='background:#fef3c7; color:#92400e; padding:1px 4px;"
                   " border-radius:2px; font-weight:700; font-size:10px;'>"
                   "ƒ <code>%1</code></span>")
        .arg(op.formulaExpression.toHtmlEscaped());
  case Operand::Type::Greek:
    return QString("<span style='color:#be185d;'>%1</span>.<span style='color:#9d174d;'>%2</span>")
        .arg(op.symbolId, op.field);
  case Operand::Type::Spread:
    return QString("<span style='color:#ea580c;'>%1</span>.<span style='color:#c2410c;'>%2</span>")
        .arg(op.symbolId, op.field);
  case Operand::Type::Total:
    return QString("<span style='color:#4338ca; font-weight:600;'>Portfolio</span>.<span style='color:#3730a3;'>%1</span>")
        .arg(op.field);
  }
  return "?";
}

QVariantMap CreateStrategyDialog::parameters() const {
  if (m_isTemplateMode) {
    QVariantMap params;
    // 1. Collect from inputs — handles normal + expression params
    for (auto it = m_paramInputs.begin(); it != m_paramInputs.end(); ++it) {
      QString key = it.key();
      QWidget *w = it.value();

      // Check if this is an Expression param with override toggle
      auto *overrideCheck = m_strategyWidget
          ? m_strategyWidget->findChild<QCheckBox*>("override_" + key)
          : nullptr;

      if (overrideCheck && !overrideCheck->isChecked()) {
        // Expression mode: store the formula for runtime evaluation
        // Find the formula from the template params
        for (const auto &p : m_selectedTemplate.params) {
          if (p.name == key && p.isExpression()) {
            params[key] = QString("__expr__:%1").arg(p.expression);
            break;
          }
        }
        continue;
      }

      // Normal value extraction
      if (auto *cb = qobject_cast<QCheckBox *>(w))
        params[key] = cb->isChecked();
      else if (auto *sb = qobject_cast<QSpinBox *>(w))
        params[key] = sb->value();
      else if (auto *dsb = qobject_cast<QDoubleSpinBox *>(w))
        params[key] = dsb->value();
      else if (auto *le = qobject_cast<QLineEdit *>(w))
        params[key] = le->text();
    }

    // 2. Add symbol bindings to parameters (StrategyService expects them either
    // in fields or params) Usually strategy instances have a 'symbol' field for
    // the main symbol, but for multi-leg strategies, we might pack them into
    // params or handle them in the service. For now, let's put them in params
    // under "symbol_bindings" or individual keys if needed. Better approach:
    // The backend for templates likely expects "leg1_token", etc. OR the
    // template instance has a `symbols` map. Let's pack them into a "bindings"
    // map inside params for now.
    QVariantMap bindings;
    QJsonArray bindingsArray; // JSON array for TemplateStrategy runtime
    for (auto it = m_symbolBindings.begin(); it != m_symbolBindings.end();
         ++it) {
      if (it.value()->isResolved()) {
        SymbolBinding b = it.value()->binding();
        QVariantMap bMap;
        bMap["token"] = b.token;
        bMap["segment"] = b.segment;
        bMap["qty"] = b.quantity;
        bMap["expiry"] = b.expiryDate;
        // ... add other necessary fields
        bindings[it.key()] = bMap;

        // Also build the JSON array for TemplateStrategy::setupBindings()
        QJsonObject jObj;
        jObj["symbolId"]  = it.key();
        jObj["segment"]   = b.segment;
        jObj["token"]     = static_cast<qint64>(b.token);
        jObj["quantity"]  = b.quantity;
        jObj["expiry"]    = b.expiryDate;
        jObj["instrument"] = b.instrumentName;
        bindingsArray.append(jObj);
      }
    }
    params["bindings"] = bindings;

    // Store template identity for TemplateStrategy runtime
    params["__template_id__"] = m_selectedTemplate.templateId;
    params["__bindings__"] = QString::fromUtf8(
        QJsonDocument(bindingsArray).toJson(QJsonDocument::Compact));

    // Execution Settings
    if (m_productCombo)
      params["product_type"] = m_productCombo->currentText();
    if (m_orderTypeCombo)
      params["order_type"] = m_orderTypeCombo->currentText();
    if (m_priceSpin && m_priceSpin->isVisible())
      params["price"] = m_priceSpin->value();
    if (m_triggerSpin && m_triggerSpin->isVisible())
      params["trigger_price"] = m_triggerSpin->value();

    // Risk settings from template risk panel
    if (m_tmplSlPctSpin)
      params["stop_loss_pct"]  = m_tmplSlPctSpin->value();
    if (m_tmplTgtPctSpin)
      params["target_pct"]     = m_tmplTgtPctSpin->value();
    if (m_tmplTrailingCheck) {
      params["trailing_enabled"]     = m_tmplTrailingCheck->isChecked();
      params["trailing_trigger_pct"] = m_tmplTrailTrigger ? m_tmplTrailTrigger->value() : 0.0;
      params["trailing_amount_pct"]  = m_tmplTrailAmount  ? m_tmplTrailAmount->value()  : 0.0;
    }
    if (m_tmplTimeExitCheck) {
      params["time_exit_enabled"] = m_tmplTimeExitCheck->isChecked();
      params["exit_time"]         = m_tmplTimeExitEdit ? m_tmplTimeExitEdit->text() : "15:15";
    }
    if (m_tmplMaxTradesSpin)
      params["max_daily_trades"] = m_tmplMaxTradesSpin->value();
    if (m_tmplMaxLossSpin)
      params["max_daily_loss"]   = m_tmplMaxLossSpin->value();

    return params;
  }

  // ... (existing logic for built-ins)
  QVariantMap params;
  QString type = strategyType().toLower().trimmed().remove("-").remove(" ");

  if (type == "jodiatm") {
    auto *offset = findWidget<QDoubleSpinBox>("m_jodiOffset");
    auto *threshold = findWidget<QDoubleSpinBox>("m_jodiThreshold");
    auto *adj = findWidget<QDoubleSpinBox>("m_jodiAdjPts");
    auto *diff = findWidget<QDoubleSpinBox>("m_jodiDiffPts");
    auto *trailing = findWidget<QCheckBox>("m_jodiTrailing");

    if (offset)
      params["offset"] = offset->value();
    if (threshold)
      params["threshold"] = threshold->value();
    if (adj)
      params["adj_pts"] = adj->value();
    if (diff)
      params["diff_points"] = diff->value();
    if (trailing)
      params["is_trailing"] = trailing->isChecked();

    auto *ceStrike = findWidget<QComboBox>("m_ceStrikeCombo");
    auto *peStrike = findWidget<QComboBox>("m_peStrikeCombo");
    if (ceStrike)
      params["ce_strike"] = ceStrike->currentText();
    if (peStrike)
      params["pe_strike"] = peStrike->currentText();

  } else if (type == "tspecial") {
    auto *momentum = findWidget<QDoubleSpinBox>("m_momentumThreshold");
    auto *ema = findWidget<QSpinBox>("m_emaPeriod");
    auto *hedged = findWidget<QCheckBox>("m_isHedged");

    if (momentum)
      params["momentum_threshold"] = momentum->value();
    if (ema)
      params["ema_period"] = ema->value();
    if (hedged)
      params["is_hedged"] = hedged->isChecked();
  } else {
    // Custom JSON fallback
    auto *edit = findWidget<QTextEdit>("m_paramsEdit");
    if (edit) {
      bool ok = true;
      params = parseParameters(&ok);
    }
  }
  return params;
}

// Update accept() to validate bindings
void CreateStrategyDialog::accept() {
  ui->m_validationLabel->setText("");

  if (instanceName().isEmpty()) {
    ui->m_validationLabel->setText("Instance name is required");
    ui->m_nameEdit->setFocus();
    return;
  }

  if (m_isTemplateMode) {
    // Validate bindings
    for (auto it = m_symbolBindings.begin(); it != m_symbolBindings.end();
         ++it) {
      if (!it.value()->isResolved()) {
        ui->m_validationLabel->setText(
            QString("Symbol binding '%1' is not resolved").arg(it.key()));
        return;
      }
    }

    if (account().isEmpty()) {
      ui->m_validationLabel->setText("Client account is required");
      ui->m_accountEdit->setFocus();
      return;
    }

    QDialog::accept();
    return;
  }

  // Original validation logic
  if (symbol().isEmpty()) {
    ui->m_validationLabel->setText("Trading symbol is required");
    auto *edit = findWidget<QLineEdit>("m_symbolEdit");
    if (edit)
      edit->setFocus();
    return;
  }

  if (account().isEmpty()) {
    ui->m_validationLabel->setText("Client account is required");
    ui->m_accountEdit->setFocus();
    return;
  }

  bool ok = true;
  QString type = strategyType().toLower().trimmed().remove("-").remove(" ");
  if (type != "jodiatm" && type != "tspecial" && type != "vixmonkey") {
    parseParameters(&ok);
    if (!ok) {
      ui->m_validationLabel->setText("Parameters must be valid JSON object");
      auto *edit = findWidget<QTextEdit>("m_paramsEdit");
      if (edit)
        edit->setFocus();
      return;
    }
  }

  QDialog::accept();
}

QVariantMap CreateStrategyDialog::parseParameters(bool *ok) const {
  auto *edit = findWidget<QTextEdit>("m_paramsEdit");
  if (!edit) {
    if (ok)
      *ok = true;
    return QVariantMap();
  }
  QString text = edit->toPlainText().trimmed();
  if (text.isEmpty()) {
    if (ok)
      *ok = true;
    return QVariantMap();
  }

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    if (ok)
      *ok = false;
    return QVariantMap();
  }

  if (ok)
    *ok = true;
  return doc.object().toVariantMap();
}
