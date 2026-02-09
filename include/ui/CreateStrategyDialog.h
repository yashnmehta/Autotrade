#ifndef CREATE_STRATEGY_DIALOG_H
#define CREATE_STRATEGY_DIALOG_H

#include <QDialog>
#include <QVariantMap>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;
class QTextEdit;

class CreateStrategyDialog : public QDialog {
  Q_OBJECT

public:
  explicit CreateStrategyDialog(QWidget *parent = nullptr);

  QString instanceName() const;
  QString strategyType() const;
  QString symbol() const;
  QString account() const;
  int segment() const;
  double stopLoss() const;
  double target() const;
  double entryPrice() const;
  int quantity() const;
  QVariantMap parameters() const;

  void setStrategyTypes(const QStringList &types);

protected:
  void accept() override;

private:
  QVariantMap parseParameters(bool *ok) const;

  QLineEdit *m_nameEdit;
  QComboBox *m_typeCombo;
  QLineEdit *m_symbolEdit;
  QLineEdit *m_accountEdit;
  QComboBox *m_segmentCombo;
  QDoubleSpinBox *m_stopLossSpin;
  QDoubleSpinBox *m_targetSpin;
  QDoubleSpinBox *m_entryPriceSpin;
  QSpinBox *m_qtySpin;
  QTextEdit *m_paramsEdit;
  QVariantMap m_cachedParameters;
};

#endif // CREATE_STRATEGY_DIALOG_H
