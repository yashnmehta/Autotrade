#ifndef STRATEGY_MANAGER_WINDOW_H
#define STRATEGY_MANAGER_WINDOW_H

#include "models/StrategyInstance.h"
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QPushButton;
class QTableView;
class StrategyFilterProxyModel;
class StrategyTableModel;

namespace Ui {
class StrategyManagerWindow;
}

class StrategyManagerWindow : public QWidget {
  Q_OBJECT

public:
  explicit StrategyManagerWindow(QWidget *parent = nullptr);
  ~StrategyManagerWindow();

private slots:
  void onCreateClicked();
  void onBuildCustomClicked();
  void onDeployTemplateClicked();
  void onManageTemplatesClicked();
  void onStartClicked();
  void onPauseClicked();
  void onResumeClicked();
  void onModifyClicked();
  void onStopClicked();
  void onDeleteClicked();

  void onStrategyFilterChanged(const QString &value);
  void onStatusFilterClicked(int id);

  void onInstanceAdded(const StrategyInstance &instance);
  void onInstanceUpdated(const StrategyInstance &instance);
  void onInstanceRemoved(qint64 instanceId);

  void onInstanceEdited(const StrategyInstance &instance);

private:
  void setupUI();
  void setupModels();
  void setupConnections();
  void refreshStrategyTypes();
  void updateSummary();

  StrategyInstance selectedInstance(bool *ok) const;

  Ui::StrategyManagerWindow *ui;
  QButtonGroup *m_statusGroup;

  StrategyTableModel *m_model;
  StrategyFilterProxyModel *m_proxy;
};

#endif // STRATEGY_MANAGER_WINDOW_H
