#ifndef STRATEGY_SERVICE_H
#define STRATEGY_SERVICE_H

#include "models/StrategyInstance.h"
#include "services/StrategyRepository.h"
#include <QHash>
#include <QMutex>
#include <QTimer>

class StrategyBase;

class StrategyService : public QObject {
  Q_OBJECT

public:
  static StrategyService &instance();

  void initialize(const QString &dbPath = QString());
  QVector<StrategyInstance> instances() const;

  qint64 createInstance(const QString &name, const QString &description,
                        const QString &strategyType, const QString &symbol,
                        const QString &account, int segment, double stopLoss,
                        double target, double entryPrice, int quantity,
                        const QVariantMap &parameters);

  bool startStrategy(qint64 instanceId);
  bool pauseStrategy(qint64 instanceId);
  bool resumeStrategy(qint64 instanceId);
  bool stopStrategy(qint64 instanceId);
  bool deleteStrategy(qint64 instanceId);

  bool modifyParameters(qint64 instanceId, const QVariantMap &parameters,
                        double stopLoss, double target,
                        QString *outError = nullptr);

  bool updateMetrics(qint64 instanceId, double mtm, int activePositions,
                     int pendingOrders);

signals:
  void instanceAdded(const StrategyInstance &instance);
  void instanceUpdated(const StrategyInstance &instance);
  void instanceRemoved(qint64 instanceId);
  void stateChanged(qint64 instanceId, StrategyState state);
  void metricsUpdated(qint64 instanceId, double mtm, double stopLoss,
                      double target);

private slots:
  void onUpdateTick();

private:
  StrategyService();
  StrategyInstance *findInstance(qint64 instanceId);
  bool updateState(StrategyInstance &instance, StrategyState newState);
  void persistInstance(const StrategyInstance &instance);

  mutable QMutex m_mutex;
  QHash<qint64, StrategyInstance> m_instances;
  StrategyRepository m_repository;
  QTimer m_updateTimer;
  bool m_initialized = false;

  // Active runtime strategies
  QHash<qint64, StrategyBase *> m_activeStrategies;
};

#endif // STRATEGY_SERVICE_H
