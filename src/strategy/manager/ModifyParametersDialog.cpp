#include "strategy/manager/ModifyParametersDialog.h"
#include "ui_ModifyParametersDialog.h"
#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>

ModifyParametersDialog::ModifyParametersDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ModifyParametersDialog)
{
    ui->setupUi(this);

    ui->paramsEdit->setPlaceholderText("Optional JSON parameters.");

    connect(ui->applyBtn, &QPushButton::clicked, this, &ModifyParametersDialog::accept);
    connect(ui->cancelBtn, &QPushButton::clicked, this, &ModifyParametersDialog::reject);
}

ModifyParametersDialog::~ModifyParametersDialog()
{
    delete ui;
}

void ModifyParametersDialog::setInitialValues(double stopLoss, double target, const QVariantMap& parameters)
{
    ui->stopLossSpin->setValue(stopLoss);
    ui->targetSpin->setValue(target);

    QJsonDocument doc = QJsonDocument::fromVariant(parameters);
    ui->paramsEdit->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
}

double ModifyParametersDialog::stopLoss() const
{
    return ui->stopLossSpin->value();
}

double ModifyParametersDialog::target() const
{
    return ui->targetSpin->value();
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
    QString text = ui->paramsEdit->toPlainText().trimmed();
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
