#include "ui/CreateStrategyDialog.h"
#include "ui_CreateStrategyDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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

  // Clear existing widget and layout
  if (m_strategyWidget) {
    m_strategyWidget->hide();
    m_strategyWidget->deleteLater();
    m_strategyWidget = nullptr;
  }

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

  QUiLoader loader;
  QFile file;

  if (t == "jodiatm") {
    file.setFileName(":/forms/stretegies/JodiATM/inputParam.ui");
  } else if (t == "tspecial") {
    file.setFileName(":/forms/stretegies/TSpecial/tspecialParam.ui");
  }
  // Future strategies can load their own .ui files here:
  // else if (t == "vixmonkey") { file.setFileName(":/forms/vixmonkey.ui"); }

  if (file.open(QFile::ReadOnly)) {
    m_strategyWidget = loader.load(&file, this);
    file.close();

    if (m_strategyWidget) {
      ui->sParamFrame->layout()->addWidget(m_strategyWidget);

      // Initialize common fields in the loaded UI
      auto *segCombo = findWidget<QComboBox>("m_segmentCombo");
      if (segCombo) {
        segCombo->clear();
        segCombo->addItem("NSE Cash Market (CM)", 1);
        segCombo->addItem("NSE Futures & Options (FO)", 2);
        segCombo->addItem("BSE Cash Market (CM)", 11);
        segCombo->addItem("BSE Futures & Options (FO)", 12);
        segCombo->setCurrentIndex(1);

        auto *segHelp = findWidget<QLabel>("m_segmentHelp");
        connect(segCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, segCombo, segHelp]() {
                  int segment = segCombo->currentData().toInt();
                  QString text;
                  if (segment == 1)
                    text = "NSE equity and index cash";
                  else if (segment == 2)
                    text = "NSE FO for Index Options";
                  else if (segment == 11)
                    text = "BSE equity cash";
                  else if (segment == 12)
                    text = "BSE FO for Options";
                  if (segHelp)
                    segHelp->setText(text);
                });
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

void CreateStrategyDialog::setStrategyTypes(const QStringList &types) {
  ui->m_typeCombo->clear();
  ui->m_typeCombo->addItems(types);
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
  return ui->m_typeCombo->currentText().trimmed();
}
QString CreateStrategyDialog::symbol() const {
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
  auto *sb = findWidget<QDoubleSpinBox>("m_stopLossSpin");
  return sb ? sb->value() : 0.0;
}
double CreateStrategyDialog::target() const {
  auto *sb = findWidget<QDoubleSpinBox>("m_targetSpin");
  return sb ? sb->value() : 0.0;
}
double CreateStrategyDialog::entryPrice() const {
  auto *sb = findWidget<QDoubleSpinBox>("m_entryPriceSpin");
  return sb ? sb->value() : 0.0;
}
int CreateStrategyDialog::quantity() const {
  auto *sb = findWidget<QSpinBox>("m_qtySpin");
  return sb ? sb->value() : 0;
}

QVariantMap CreateStrategyDialog::parameters() const {
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

    // Support CE/PE strikes from the new UI
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

void CreateStrategyDialog::accept() {
  // Clear previous validation message
  ui->m_validationLabel->setText("");

  if (instanceName().isEmpty()) {
    ui->m_validationLabel->setText("Instance name is required");
    ui->m_nameEdit->setFocus();
    return;
  }

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
