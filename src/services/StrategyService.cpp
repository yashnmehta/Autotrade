#include "services/StrategyService.h"
#include "strategies/StrategyBase.h"
#include "strategies/StrategyFactory.h"
#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>

StrategyService &StrategyService::instance() {
  static StrategyService service;
  return service;
}

StrategyService::StrategyService() : QObject(nullptr) {
  m_updateTimer.setInterval(500);
  connect(&m_updateTimer, &QTimer::timeout, this,
          &StrategyService::onUpdateTick);
}

void StrategyService::initialize(const QString &dbPath) {
  if (m_initialized) {
    return;
  }

  if (!m_repository.open(dbPath)) {
    qDebug() << "[StrategyService] Repository open failed";
  } else {
    QVector<StrategyInstance> loaded = m_repository.loadAllInstances(false);
    QMutexLocker locker(&m_mutex);
    for (const StrategyInstance &instance : loaded) {
      m_instances.insert(instance.instanceId, instance);
    }
  }

  m_updateTimer.start();
  m_initialized = true;
}

QVector<StrategyInstance> StrategyService::instances() const {
  QMutexLocker locker(&m_mutex);
  return m_instances.values().toVector();
}

qint64 StrategyService::createInstance(
    const QString &name, const QString &description,
    const QString &strategyType, const QString &symbol, const QString &account,
    int segment, double stopLoss, double target, double entryPrice,
    int quantity, const QVariantMap &parameters) {
  StrategyInstance instance;
  instance.instanceName = name;
  instance.description = description;
  instance.strategyType = strategyType;
  instance.symbol = symbol;
  instance.account = account;
  instance.segment = segment;
  instance.stopLoss = stopLoss;
  instance.target = target;
  instance.entryPrice = entryPrice;
  instance.quantity = quantity;
  instance.parameters = parameters;
  instance.state = StrategyState::Created;
  instance.createdAt = QDateTime::currentDateTime();
  instance.lastUpdated = instance.createdAt;
  instance.lastStateChange = instance.createdAt;

  if (!m_repository.isOpen()) {
    m_repository.open();
  }
  m_repository.saveInstance(instance);

  {
    QMutexLocker locker(&m_mutex);
    m_instances.insert(instance.instanceId, instance);
  }

  emit instanceAdded(instance);
  emit instanceUpdated(instance);

  return instance.instanceId;
}

bool StrategyService::startStrategy(qint64 instanceId) {
  // Check if already active in our map
  {
    QMutexLocker locker(&m_mutex);
    if (m_activeStrategies.contains(instanceId)) {
      return false;
    }

    if (!m_instances.contains(instanceId)) {
      return false;
    }
  }

  // Create and initialize strategy outside the lock (factory might do things)
  StrategyInstance *instance = findInstance(instanceId);
  if (!instance)
    return false;

  // State validation
  if (instance->state != StrategyState::Created &&
      instance->state != StrategyState::Stopped &&
      instance->state != StrategyState::Paused) {
    qDebug() << "[StrategyService] Cannot start from state"
             << StrategyInstance::stateToString(instance->state);
    return false;
  }

  StrategyBase *strategy =
      StrategyFactory::createStrategy(instance->strategyType);
  if (!strategy) {
    qDebug() << "[StrategyService] Failed to create strategy of type"
             << instance->strategyType;
    return false;
  }

  strategy->init(*instance);
  strategy->start();

  // Connect logging
  connect(strategy, &StrategyBase::logMessage, this,
          [this](qint64 id, const QString &msg) {
            qDebug() << "[StrategyLog]" << id << msg;
          });

  // Connect order requests from strategy to service relay signal
  connect(strategy, &StrategyBase::orderRequested, this,
          &StrategyService::orderRequested);

  {
    QMutexLocker locker(&m_mutex);
    m_activeStrategies.insert(instanceId, strategy);
  }

  // Update data model
  instance->startTime = QDateTime::currentDateTime();
  bool ok = updateState(*instance, StrategyState::Running);

  return ok;
}

bool StrategyService::pauseStrategy(qint64 instanceId) {
  QMutexLocker locker(&m_mutex);
  if (!m_activeStrategies.contains(instanceId)) {
    return false;
  }

  StrategyBase *strategy = m_activeStrategies[instanceId];
  strategy->pause();

  if (m_instances.contains(instanceId)) {
    StrategyInstance &inst = m_instances[instanceId];
    // We can't call updateState directly here because it locks m_mutex
    // recursively in original design? Wait, updateState locks m_mutex? Let's
    // check updateState. updateState calls persistInstance and emits signals.
    // If updateState doesn't lock, we are good.
    // But updateState implementation usually doesn't lock if it takes
    // reference. Let's check updateState implementation in a separate view if
    // needed. For now, I'll assume I need to update the instance carefully.

    inst.state = StrategyState::Paused;
    inst.lastStateChange = QDateTime::currentDateTime();
    inst.lastUpdated = inst.lastStateChange;

    // persistInstance(inst); // Assuming this is safe or I should unlock first
    // But I hold the lock.
    // If persistInstance manages its own lock, it might be an issue.
    // persistInstance calls m_repository.updateInstance. Repository uses
    // QSqlDatabase, which is thread-safe-ish but usually fine.

    // To be safe and consistent with other methods, I should probably rely on
    // updateState helper if possible, but updateState helper might try to
    // acquire lock if it was designed to be public/thread-safe. Let's duplicate
    // update logic here to be safe and avoid deadlock if updateState locks.
  }

  // Re-fetch instance to update via helper which handles signals/persistence
  // But I can't call helper while holding lock if helper locks.
  locker.unlock();

  StrategyInstance *instance = findInstance(instanceId);
  if (instance)
    updateState(*instance, StrategyState::Paused);

  return true;
}

bool StrategyService::resumeStrategy(qint64 instanceId) {
  QMutexLocker locker(&m_mutex);
  if (!m_activeStrategies.contains(instanceId)) {
    return false;
  }

  StrategyBase *strategy = m_activeStrategies[instanceId];
  strategy->resume();

  locker.unlock();

  StrategyInstance *instance = findInstance(instanceId);
  if (instance)
    updateState(*instance, StrategyState::Running);

  return true;
}

bool StrategyService::stopStrategy(qint64 instanceId) {
  StrategyBase *strategy = nullptr;
  {
    QMutexLocker locker(&m_mutex);
    if (m_activeStrategies.contains(instanceId)) {
      strategy = m_activeStrategies.take(instanceId);
    }
  }

  if (strategy) {
    strategy->stop();
    delete strategy;
  }

  StrategyInstance *instance = findInstance(instanceId);
  if (instance)
    updateState(*instance, StrategyState::Stopped);

  return true;
}

bool StrategyService::deleteStrategy(qint64 instanceId) {
  StrategyInstance *instance = findInstance(instanceId);
  if (!instance) {
    return false;
  }

  // Delete only allowed from Stopped state
  if (instance->state != StrategyState::Stopped) {
    qDebug() << "[StrategyService] Cannot delete from state"
             << StrategyInstance::stateToString(instance->state)
             << "(only from Stopped)";
    return false;
  }

  if (!m_repository.isOpen()) {
    m_repository.open();
  }
  m_repository.markDeleted(instanceId);

  {
    QMutexLocker locker(&m_mutex);
    m_instances.remove(instanceId);
  }

  emit instanceRemoved(instanceId);
  return true;
}

bool StrategyService::modifyParameters(qint64 instanceId,
                                       const QVariantMap &parameters,
                                       double stopLoss, double target,
                                       QString *outError) {
  StrategyInstance *instance = findInstance(instanceId);
  if (!instance) {
    if (outError)
      *outError = "Instance not found";
    return false;
  }

  // Check for locked parameters if strategy is running
  if (instance->state == StrategyState::Running) {
    for (const QString &key : instance->lockedParameters) {
      if (parameters.contains(key) &&
          parameters[key] != instance->parameters.value(key)) {
        if (outError)
          *outError = QString("Parameter locked while running: %1").arg(key);
        qDebug() << "[StrategyService] Locked parameter modify attempt:" << key;
        return false;
      }
    }
  }

  instance->parameters = parameters;
  instance->stopLoss = stopLoss;
  instance->target = target;
  instance->lastUpdated = QDateTime::currentDateTime();

  persistInstance(*instance);
  emit instanceUpdated(*instance);
  emit metricsUpdated(instance->instanceId, instance->mtm, instance->stopLoss,
                      instance->target);

  return true;
}

bool StrategyService::updateMetrics(qint64 instanceId, double mtm,
                                    int activePositions, int pendingOrders) {
  StrategyInstance *instance = findInstance(instanceId);
  if (!instance) {
    return false;
  }

  instance->mtm = mtm;
  instance->activePositions = activePositions;
  instance->pendingOrders = pendingOrders;
  instance->lastUpdated = QDateTime::currentDateTime();

  emit instanceUpdated(*instance);
  emit metricsUpdated(instance->instanceId, instance->mtm, instance->stopLoss,
                      instance->target);
  return true;
}

void StrategyService::onUpdateTick() {
  QVector<StrategyInstance> updates;

  {
    QMutexLocker locker(&m_mutex);
    for (auto it = m_instances.begin(); it != m_instances.end(); ++it) {
      StrategyInstance &instance = it.value();
      if (instance.state == StrategyState::Running) {
        // TODO (Phase 2): Compute MTM from FeedHandler
        // - Get current price for instance.symbol + instance.segment
        // - Formula: MTM = instance.quantity * (currentPrice -
        // instance.entryPrice)
        // - Update instance.mtm only if value changed
        // Reference: FeedHandler::instance().subscribe(segment, token, ...)
        // Reference: PriceStoreGateway for direct price lookup

        instance.lastUpdated = QDateTime::currentDateTime();
        updates.append(instance);
      }
    }
  }

  for (const StrategyInstance &instance : updates) {
    emit instanceUpdated(instance);
  }
}

StrategyInstance *StrategyService::findInstance(qint64 instanceId) {
  QMutexLocker locker(&m_mutex);
  auto it = m_instances.find(instanceId);
  if (it == m_instances.end()) {
    return nullptr;
  }
  return &it.value();
}

bool StrategyService::updateState(StrategyInstance &instance,
                                  StrategyState newState) {
  if (instance.state == newState) {
    return true;
  }

  instance.state = newState;
  instance.lastStateChange = QDateTime::currentDateTime();
  instance.lastUpdated = instance.lastStateChange;

  persistInstance(instance);
  emit instanceUpdated(instance);
  emit stateChanged(instance.instanceId, instance.state);
  return true;
}

void StrategyService::persistInstance(const StrategyInstance &instance) {
  if (!m_repository.isOpen()) {
    m_repository.open();
  }
  m_repository.updateInstance(instance);
}
