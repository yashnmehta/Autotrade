#include "strategy/manager/ModifyParametersDialog.h"
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

ModifyParametersDialog::ModifyParametersDialog(QWidget *parent)
    : QDialog(parent),
      m_stopLossSpin(new QDoubleSpinBox(this)),
      m_targetSpin(new QDoubleSpinBox(this)),
      m_paramsEdit(new QTextEdit(this))
{
    setWindowTitle("Modify Parameters");
    setModal(true);

    m_stopLossSpin->setDecimals(2);
    m_stopLossSpin->setMaximum(1e9);
    m_targetSpin->setDecimals(2);
    m_targetSpin->setMaximum(1e9);

    m_paramsEdit->setPlaceholderText("Optional JSON parameters.");

    QFormLayout* form = new QFormLayout();
    form->addRow("Stop Loss", m_stopLossSpin);
    form->addRow("Target", m_targetSpin);
    form->addRow("Parameters", m_paramsEdit);

    QPushButton* okButton = new QPushButton("Apply", this);
    QPushButton* cancelButton = new QPushButton("Cancel", this);
    connect(okButton, &QPushButton::clicked, this, &ModifyParametersDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &ModifyParametersDialog::reject);

    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addStretch();
    buttons->addWidget(okButton);
    buttons->addWidget(cancelButton);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttons);

    setLayout(layout);
}

void ModifyParametersDialog::setInitialValues(double stopLoss, double target, const QVariantMap& parameters)
{
    m_stopLossSpin->setValue(stopLoss);
    m_targetSpin->setValue(target);

    QJsonDocument doc = QJsonDocument::fromVariant(parameters);
    m_paramsEdit->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
}

double ModifyParametersDialog::stopLoss() const
{
    return m_stopLossSpin->value();
}

double ModifyParametersDialog::target() const
{
    return m_targetSpin->value();
}

QVariantMap ModifyParametersDialog::parameters() const
{
    return m_cachedParameters;
}

void ModifyParametersDialog::accept()
{
    bool ok = true;
    m_cachedParameters = parseParameters(&ok);
    if (!ok) {
        QMessageBox::warning(this, "Validation", "Parameters must be valid JSON object.");
        return;
    }

    QDialog::accept();
}

QVariantMap ModifyParametersDialog::parseParameters(bool* ok) const
{
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
