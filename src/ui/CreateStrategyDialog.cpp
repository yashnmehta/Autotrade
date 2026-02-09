#include "ui/CreateStrategyDialog.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

CreateStrategyDialog::CreateStrategyDialog(QWidget *parent)
    : QDialog(parent), m_nameEdit(new QLineEdit(this)),
      m_typeCombo(new QComboBox(this)), m_symbolEdit(new QLineEdit(this)),
      m_accountEdit(new QLineEdit(this)), m_segmentCombo(new QComboBox(this)),
      m_stopLossSpin(new QDoubleSpinBox(this)),
      m_targetSpin(new QDoubleSpinBox(this)),
      m_entryPriceSpin(new QDoubleSpinBox(this)), m_qtySpin(new QSpinBox(this)),
      m_paramsEdit(new QTextEdit(this)) {
  setWindowTitle("Create Strategy Instance");
  setModal(true);

  m_stopLossSpin->setDecimals(2);
  m_stopLossSpin->setMaximum(1e9);
  m_targetSpin->setDecimals(2);
  m_targetSpin->setMaximum(1e9);
  m_entryPriceSpin->setDecimals(2);
  m_entryPriceSpin->setMaximum(1e9);
  m_qtySpin->setMaximum(1000000);

  m_segmentCombo->addItem("NSECM", 1);
  m_segmentCombo->addItem("NSEFO", 2);
  m_segmentCombo->addItem("BSECM", 11);
  m_segmentCombo->addItem("BSEFO", 12);

  m_paramsEdit->setPlaceholderText(
      "Optional JSON parameters. Example: {\"threshold\": 0.5}");

  QFormLayout *form = new QFormLayout();
  form->addRow("Instance Name", m_nameEdit);
  form->addRow("Strategy Type", m_typeCombo);
  form->addRow("Symbol", m_symbolEdit);
  form->addRow("Account ID", m_accountEdit);
  form->addRow("Segment", m_segmentCombo);
  form->addRow("Stop Loss", m_stopLossSpin);
  form->addRow("Target", m_targetSpin);
  form->addRow("Entry Price", m_entryPriceSpin);
  form->addRow("Quantity", m_qtySpin);
  form->addRow("Parameters", m_paramsEdit);

  QPushButton *okButton = new QPushButton("Create", this);
  QPushButton *cancelButton = new QPushButton("Cancel", this);
  connect(okButton, &QPushButton::clicked, this, &CreateStrategyDialog::accept);
  connect(cancelButton, &QPushButton::clicked, this,
          &CreateStrategyDialog::reject);

  QHBoxLayout *buttons = new QHBoxLayout();
  buttons->addStretch();
  buttons->addWidget(okButton);
  buttons->addWidget(cancelButton);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addLayout(buttons);

  setLayout(layout);

  // Apply Light Theme
  setStyleSheet(
      "QDialog { background-color: #ffffff; color: #1f1f1f; font-family: "
      "'Segoe UI'; }"
      "QLabel { color: #333333; font-weight: 600; font-size: 12px; }"
      "QLineEdit, QSpinBox, QDoubleSpinBox, QTextEdit, QComboBox {"
      "    background-color: #ffffff; color: #333333;"
      "    border: 1px solid #d1d5db; border-radius: 4px; padding: 6px;"
      "    font-size: 12px;"
      "}"
      "QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, "
      "QTextEdit:focus {"
      "    border: 1px solid #2563eb;"
      "}"
      "QPushButton {"
      "    background-color: #f3f4f6; color: #1f2937;"
      "    border: 1px solid #d1d5db; border-radius: 4px;"
      "    padding: 6px 16px; font-weight: 600; font-size: 12px;"
      "}"
      "QPushButton:hover { background-color: #e5e7eb; border-color: #9ca3af; }"
      "QPushButton:pressed { background-color: #d1d5db; }"
      "QTextEdit { font-family: 'Consolas', monospace; font-size: 11px; }");
}

void CreateStrategyDialog::setStrategyTypes(const QStringList &types) {
  m_typeCombo->clear();
  m_typeCombo->addItems(types);
}

QString CreateStrategyDialog::instanceName() const {
  return m_nameEdit->text().trimmed();
}

QString CreateStrategyDialog::strategyType() const {
  return m_typeCombo->currentText().trimmed();
}

QString CreateStrategyDialog::symbol() const {
  return m_symbolEdit->text().trimmed();
}

QString CreateStrategyDialog::account() const {
  return m_accountEdit->text().trimmed();
}

int CreateStrategyDialog::segment() const {
  return m_segmentCombo->currentData().toInt();
}

double CreateStrategyDialog::stopLoss() const {
  return m_stopLossSpin->value();
}

double CreateStrategyDialog::target() const { return m_targetSpin->value(); }

double CreateStrategyDialog::entryPrice() const {
  return m_entryPriceSpin->value();
}

int CreateStrategyDialog::quantity() const { return m_qtySpin->value(); }

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
  QString text = m_paramsEdit->toPlainText().trimmed();
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
