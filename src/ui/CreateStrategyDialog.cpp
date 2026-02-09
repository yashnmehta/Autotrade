#include "ui/CreateStrategyDialog.h"
#include "ui_CreateStrategyDialog.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

CreateStrategyDialog::CreateStrategyDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::CreateStrategyDialog) {
  ui->setupUi(this);

  // Re-populate Segment Combo to ensure UserData is set correctly
  ui->m_segmentCombo->clear();
  ui->m_segmentCombo->addItem("NSECM", 1);
  ui->m_segmentCombo->addItem("NSEFO", 2);
  ui->m_segmentCombo->addItem("BSECM", 11);
  ui->m_segmentCombo->addItem("BSEFO", 12);

  // Setup connections
  connect(ui->okButton, &QPushButton::clicked, this,
          &CreateStrategyDialog::accept);
  connect(ui->cancelButton, &QPushButton::clicked, this,
          &CreateStrategyDialog::reject);
  connect(ui->m_typeCombo, &QComboBox::currentTextChanged, this,
          &CreateStrategyDialog::onTypeChanged);
}

CreateStrategyDialog::~CreateStrategyDialog() { delete ui; }

void CreateStrategyDialog::onTypeChanged(const QString &type) {
  QString templateText;
  QString t = type.toLower().trimmed().remove("-").remove(" ");

  if (t == "jodiatm") {
    templateText = "{\n"
                   "  \"offset\": 10.0,\n"
                   "  \"threshold\": 15.0,\n"
                   "  \"adj_pts\": 5.0,\n"
                   "  \"diff_points\": 0.0,\n"
                   "  \"ce_strike_idx\": 0,\n"
                   "  \"pe_strike_idx\": 0,\n"
                   "  \"is_trailing\": false\n"
                   "}";
  } else if (t == "rangebreakout") {
    templateText = "{\n"
                   "  \"high\": 0.0,\n"
                   "  \"low\": 0.0\n"
                   "}";
  } else if (t == "momentum") {
    templateText = "{\n"
                   "  \"target_offset\": 5.0,\n"
                   "  \"sl_offset\": 5.0\n"
                   "}";
  }

  if (!templateText.isEmpty() && ui->m_paramsEdit->toPlainText().isEmpty()) {
    ui->m_paramsEdit->setPlainText(templateText);
  }
}

void CreateStrategyDialog::setStrategyTypes(const QStringList &types) {
  ui->m_typeCombo->clear();
  ui->m_typeCombo->addItems(types);
}

QString CreateStrategyDialog::instanceName() const {
  return ui->m_nameEdit->text().trimmed();
}

QString CreateStrategyDialog::strategyType() const {
  return ui->m_typeCombo->currentText().trimmed();
}

QString CreateStrategyDialog::symbol() const {
  return ui->m_symbolEdit->text().trimmed();
}

QString CreateStrategyDialog::account() const {
  return ui->m_accountEdit->text().trimmed();
}

int CreateStrategyDialog::segment() const {
  return ui->m_segmentCombo->currentData().toInt();
}

double CreateStrategyDialog::stopLoss() const {
  return ui->m_stopLossSpin->value();
}

double CreateStrategyDialog::target() const {
  return ui->m_targetSpin->value();
}

double CreateStrategyDialog::entryPrice() const {
  return ui->m_entryPriceSpin->value();
}

int CreateStrategyDialog::quantity() const { return ui->m_qtySpin->value(); }

QVariantMap CreateStrategyDialog::parameters() const {
  return m_cachedParameters;
}

void CreateStrategyDialog::accept() {
  if (instanceName().isEmpty()) {
    QMessageBox::warning(this, "Validation", "Instance name is required.");
    return;
  }

  bool ok = true;
  m_cachedParameters = parseParameters(&ok);
  if (!ok) {
    QMessageBox::warning(this, "Validation",
                         "Parameters must be valid JSON object.");
    return;
  }

  QDialog::accept();
}

QVariantMap CreateStrategyDialog::parseParameters(bool *ok) const {
  QString text = ui->m_paramsEdit->toPlainText().trimmed();
  if (text.isEmpty()) {
    if (ok) {
      *ok = true;
    }
    return QVariantMap();
  }

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    if (ok) {
      *ok = false;
    }
    return QVariantMap();
  }

  if (ok) {
    *ok = true;
  }
  return doc.object().toVariantMap();
}
