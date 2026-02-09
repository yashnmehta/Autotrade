#ifndef CREATE_STRATEGY_DIALOG_H
#define CREATE_STRATEGY_DIALOG_H

#include <QDialog>
#include <QVariantMap>

namespace Ui {
class CreateStrategyDialog;
}

class CreateStrategyDialog : public QDialog {
  Q_OBJECT

public:
  explicit CreateStrategyDialog(QWidget *parent = nullptr);
  ~CreateStrategyDialog();

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

private slots:
  void onTypeChanged(const QString &type);

protected:
  void accept() override;

private:
  QVariantMap parseParameters(bool *ok) const;

  Ui::CreateStrategyDialog *ui;
  QVariantMap m_cachedParameters;
};

#endif // CREATE_STRATEGY_DIALOG_H
