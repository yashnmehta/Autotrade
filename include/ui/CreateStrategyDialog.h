#ifndef CREATE_STRATEGY_DIALOG_H
#define CREATE_STRATEGY_DIALOG_H

#include "strategy/StrategyTemplate.h"
#include "ui/SymbolBindingWidget.h"
#include <QDialog>
#include <QMap>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

namespace Ui {
class CreateStrategyDialog;
}

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;

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

  // Template support
  QVector<StrategyTemplate> m_templates;
  StrategyTemplate m_selectedTemplate;
  bool m_isTemplateMode = false;

  // Dynamic widgets for template mode
  QMap<QString, SymbolBindingWidget *> m_symbolBindings;
  QMap<QString, QWidget *> m_paramInputs; // name -> widget

  // Execution widgets (template mode)
  QSpinBox *m_globalQtySpin = nullptr;
  QComboBox *m_productCombo = nullptr;
  QComboBox *m_orderTypeCombo = nullptr;
  QDoubleSpinBox *m_priceSpin = nullptr;
  QDoubleSpinBox *m_triggerSpin = nullptr;

  // Risk widgets for template mode (embedded in right pane)
  QDoubleSpinBox *m_tmplSlPctSpin       = nullptr;
  QDoubleSpinBox *m_tmplTgtPctSpin      = nullptr;
  QCheckBox      *m_tmplTrailingCheck   = nullptr;
  QDoubleSpinBox *m_tmplTrailTrigger    = nullptr;
  QDoubleSpinBox *m_tmplTrailAmount     = nullptr;
  QCheckBox      *m_tmplTimeExitCheck   = nullptr;
  QLineEdit      *m_tmplTimeExitEdit    = nullptr;
  QSpinBox       *m_tmplMaxTradesSpin   = nullptr;
  QDoubleSpinBox *m_tmplMaxLossSpin     = nullptr;

  // Condition display labels (for live update when params change)
  QLabel *m_entryCondLabel = nullptr;
  QLabel *m_exitCondLabel  = nullptr;

  // Helper methods
  void loadTemplates();
  void setupTemplateUI(const StrategyTemplate &tmpl);
  void clearStrategyUI();
  void refreshConditionDisplay();

  QString conditionToString(const ConditionNode &node) const;
  QString operandToString(const Operand &op) const;

  template <typename T> T *findWidget(const QString &name) const {
    if (!m_strategyWidget)
      return nullptr;
    return m_strategyWidget->findChild<T *>(name);
  }
};

#endif // CREATE_STRATEGY_DIALOG_H
