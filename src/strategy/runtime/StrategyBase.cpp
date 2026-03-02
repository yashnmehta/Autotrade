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
  // Base-class subscription uses the instance's single symbol + segment.
  // TemplateStrategy overrides start() to handle multi-symbol bindings itself,
  // so this path is only for simple single-symbol strategies.

  if (m_instance.symbol.isEmpty()) {
    log("WARNING: No symbol set — skipping subscription");
    return;
  }

  auto repo = RepositoryManager::getInstance();
  if (!repo) {
    log("ERROR: RepositoryManager not available");
    return;
  }

  // Resolve trading symbol → token via RepositoryManager
  int segment = m_instance.segment;
  QString exchange, series;

  // Map segment ID to exchange/series for repository lookup
  switch (segment) {
    case 1:  exchange = "NSE"; series = "CM"; break;
    case 2:  exchange = "NSE"; series = "FO"; break;
    case 11: exchange = "BSE"; series = "CM"; break;
    case 12: exchange = "BSE"; series = "FO"; break;
    default:
      log(QString("ERROR: Unknown segment %1").arg(segment));
      return;
  }

  // Try direct token lookup from parameters (StrategyDeployDialog may provide it)
  QVariant tokenVar = m_instance.parameters.value("__token__");
  uint32_t token = 0;

  if (tokenVar.isValid() && tokenVar.toLongLong() > 0) {
    token = static_cast<uint32_t>(tokenVar.toLongLong());
  } else {
    // Fallback: search by symbol name
    auto contracts = repo->searchScrips(exchange, series, "",
                                        m_instance.symbol, 1);
    if (!contracts.isEmpty()) {
      token = static_cast<uint32_t>(contracts.first().exchangeInstrumentID);
    }
  }

  if (token == 0) {
    log(QString("ERROR: Could not resolve token for '%1' in %2/%3")
            .arg(m_instance.symbol, exchange, series));
    return;
  }

  FeedHandler::instance().subscribe(segment, static_cast<int>(token),
                                    this, &StrategyBase::onTick);
  log(QString("Subscribed to %1 (seg=%2, tok=%3)")
          .arg(m_instance.symbol).arg(segment).arg(token));
}

void StrategyBase::unsubscribe() {
  // Unsubscribe this receiver from all FeedHandler publishers
  FeedHandler::instance().unsubscribeAll(this);
  log("Unsubscribed from all feeds");
}

void StrategyBase::log(const QString &message) {
  qDebug() << "[Strategy:" << m_instance.instanceName << "]" << message;
  emit logMessage(m_instance.instanceId, message);
}

void StrategyBase::updateState(StrategyState newState) {
  m_instance.state = newState;
  emit stateChanged(m_instance, newState);
}
