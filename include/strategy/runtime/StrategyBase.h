#ifndef STRATEGY_BASE_H
#define STRATEGY_BASE_H

#include "api/xts/XTSTypes.h"
#include "strategy/model/StrategyInstance.h"
#include "services/FeedHandler.h"
#include <QObject>
#include <QVariantMap>


class StrategyBase : public QObject {
  Q_OBJECT

public:
  explicit StrategyBase(QObject *parent = nullptr);
  virtual ~StrategyBase();

  virtual void init(const StrategyInstance &instance);
  virtual void start();
  virtual void stop();
  virtual void pause();
  virtual void resume();

  const StrategyInstance &instance() const { return m_instance; }

signals:
  void stateChanged(const StrategyInstance &instance, StrategyState newState);
  void metricsUpdated(const StrategyInstance &instance, double mtm,
                      int activePositions, int pendingOrders);
  void logMessage(qint64 instanceId, const QString &message);
  void orderRequested(const XTS::OrderParams &params);

protected slots:
  virtual void onTick(const UDP::MarketTick &tick) = 0;

protected:
  void subscribe();
  void unsubscribe();
  void updateState(StrategyState newState);
  void log(const QString &message);

  // Helper to extract JSON parameter safely
  template <typename T>
  T getParameter(const QString &key, T defaultValue) const {
    if (m_instance.parameters.contains(key))
      return m_instance.parameters[key].value<T>();
    return defaultValue;
  }

  StrategyInstance m_instance;
  bool m_isRunning = false;
};

#endif // STRATEGY_BASE_H
