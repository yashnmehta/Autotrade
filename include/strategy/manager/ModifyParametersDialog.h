#ifndef MODIFY_PARAMETERS_DIALOG_H
#define MODIFY_PARAMETERS_DIALOG_H

#include <QDialog>
#include <QVariantMap>

class QDoubleSpinBox;
class QTextEdit;

namespace Ui { class ModifyParametersDialog; }

class ModifyParametersDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModifyParametersDialog(QWidget *parent = nullptr);
    ~ModifyParametersDialog();

    void setInitialValues(double stopLoss, double target, const QVariantMap& parameters);

    double stopLoss() const;
    double target() const;
    QVariantMap parameters() const;

protected:
    void accept() override;

private:
    QVariantMap parseParameters(bool* ok) const;

    Ui::ModifyParametersDialog *ui;
    QVariantMap m_cachedParameters;
};

#endif // MODIFY_PARAMETERS_DIALOG_H
