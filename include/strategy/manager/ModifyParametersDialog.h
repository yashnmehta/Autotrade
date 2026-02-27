#ifndef MODIFY_PARAMETERS_DIALOG_H
#define MODIFY_PARAMETERS_DIALOG_H

#include <QDialog>
#include <QVariantMap>

class QDoubleSpinBox;
class QTextEdit;

class ModifyParametersDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModifyParametersDialog(QWidget *parent = nullptr);

    void setInitialValues(double stopLoss, double target, const QVariantMap& parameters);

    double stopLoss() const;
    double target() const;
    QVariantMap parameters() const;

protected:
    void accept() override;

private:
    QVariantMap parseParameters(bool* ok) const;

    QDoubleSpinBox* m_stopLossSpin;
    QDoubleSpinBox* m_targetSpin;
    QTextEdit* m_paramsEdit;
    QVariantMap m_cachedParameters;
};

#endif // MODIFY_PARAMETERS_DIALOG_H
