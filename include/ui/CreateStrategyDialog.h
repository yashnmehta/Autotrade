#ifndef CREATE_STRATEGY_DIALOG_H
#define CREATE_STRATEGY_DIALOG_H

#include <QDialog>
#include <QString>
#include <QVariantMap>
#include <QWidget>

namespace Ui {
class CreateStrategyDialog;
}

class CreateStrategyDialog : public QDialog {
  Q_OBJECT

public:
  explicit CreateStrategyDialog(QWidget *parent = nullptr);
  ~CreateStrategyDialog();

  QString instanceName() const;
  QString description() const;
  QString strategyType() const;
  QString symbol() const;
  QString account() const;
  int segment() const;
  double stopLoss() const;
  double target() const;
  double entryPrice() const;
  int quantity() const;
  QVariantMap parameters() const;

  void setNextSrNo(int id);
  void setStrategyTypes(const QStringList &types);

private slots:
  void onTypeChanged(const QString &type);

protected:
  void accept() override;

private:
  QVariantMap parseParameters(bool *ok) const;

  Ui::CreateStrategyDialog *ui;
  QWidget *m_strategyWidget = nullptr;
  int m_nextId = 1;

  template <typename T> T *findWidget(const QString &name) const {
    if (!m_strategyWidget)
      return nullptr;
    return m_strategyWidget->findChild<T *>(name);
  }
};

#endif // CREATE_STRATEGY_DIALOG_H
