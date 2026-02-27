#include "strategy/runtime/StrategyBase.h"
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include <QDebug>


StrategyBase::StrategyBase(QObject *parent) : QObject(parent) {}
StrategyBase::~StrategyBase() { unsubscribe(); }

void StrategyBase::init(const StrategyInstance &instance) {
  m_instance = instance;
  m_isRunning = (m_instance.state == StrategyState::Running);
}

void StrategyBase::start() {
  log("Starting strategy...");
  m_isRunning = true;
  m_instance.state =
      StrategyState::Running; // Temporary until service updates it

  subscribe();
  emit stateChanged(m_instance, StrategyState::Running);
}

void StrategyBase::stop() {
  log("Stopping strategy...");
  m_isRunning = false;
  m_instance.state = StrategyState::Stopped;

  unsubscribe();
  emit stateChanged(m_instance, StrategyState::Stopped);
}

void StrategyBase::pause() {
  log("Pausing strategy...");
  m_isRunning = false;
  m_instance.state = StrategyState::Paused;

  // Unsubscribe to save CPU? Or keep receiving ticks to update MTM?
  // Usually keep receiving for MTM, but don't execute logic.
  // For now, let's keep subscription.
  emit stateChanged(m_instance, StrategyState::Paused);
}

void StrategyBase::resume() {
  log("Resuming strategy...");
  m_isRunning = true;
  m_instance.state = StrategyState::Running;
  emit stateChanged(m_instance, StrategyState::Running);
}

void StrategyBase::subscribe() {
  // Determine token based on symbol
  auto repo = RepositoryManager::getInstance();

  if (m_instance.segment == 1) {        // NSECM
                                        // Not implemented fully
  } else if (m_instance.segment == 2) { // NSEFO
    // Find token. In real app, we need to find specific contract based on
    // symbol/expiry StrategyInstance has 'symbol' which might be just "NIFTY".
    // Where is Expiry stored? In parameters?
    // Assume 'symbol' in StrategyInstance is the TRADING SYMBOL if fully
    // qualified, or we need to lookup. For now, let's assume instance.symbol is
    // enough or we use parameters.

    // TODO: Implement proper token lookup.
  }
}

void StrategyBase::unsubscribe() {
  // Unsubscribe logic
}

void StrategyBase::log(const QString &message) {
  qDebug() << "[Strategy:" << m_instance.instanceName << "]" << message;
  emit logMessage(m_instance.instanceId, message);
}

void StrategyBase::updateState(StrategyState newState) {
  m_instance.state = newState;
  emit stateChanged(m_instance, newState);
}
