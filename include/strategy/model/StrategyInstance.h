#ifndef STRATEGY_INSTANCE_H
#define STRATEGY_INSTANCE_H

#include <QDateTime>
#include <QSet>
#include <QString>
#include <QVariantMap>


enum class StrategyState {
  Created = 0,
  Running,
  Paused,
  Stopped,
  Error,
  Deleted
};

struct StrategyInstance {
  qint64 instanceId = 0;
  QString instanceName;
  QString strategyType;
  QString symbol;
  QString account; // Account ID for multi-account support
  int segment = 0; // Market segment (1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO)
  QString description;

  StrategyState state = StrategyState::Created;

  double mtm = 0.0;
  double stopLoss = 0.0;
  double target = 0.0;
  double entryPrice = 0.0;

  int quantity = 0;
  int activePositions = 0;
  int pendingOrders = 0;

  QVariantMap parameters;
  QSet<QString>
      lockedParameters; // Parameters that cannot be modified while running

  QDateTime createdAt;
  QDateTime lastUpdated;
  QDateTime lastStateChange;
  QDateTime startTime;

  QString lastError;

  static QString stateToString(StrategyState state) {
    switch (state) {
    case StrategyState::Created:
      return "CREATED";
    case StrategyState::Running:
      return "RUNNING";
    case StrategyState::Paused:
      return "PAUSED";
    case StrategyState::Stopped:
      return "STOPPED";
    case StrategyState::Error:
      return "ERROR";
    case StrategyState::Deleted:
      return "DELETED";
    default:
      return "UNKNOWN";
    }
  }

  static StrategyState stringToState(const QString &value) {
    QString upper = value.trimmed().toUpper();
    if (upper == "CREATED") {
      return StrategyState::Created;
    }
    if (upper == "RUNNING" || upper == "ACTIVATED" || upper == "ACTIVE") {
      return StrategyState::Running;
    }
    if (upper == "PAUSED") {
      return StrategyState::Paused;
    }
    if (upper == "STOPPED" || upper == "STOP") {
      return StrategyState::Stopped;
    }
    if (upper == "ERROR") {
      return StrategyState::Error;
    }
    if (upper == "DELETED") {
      return StrategyState::Deleted;
    }
    return StrategyState::Created;
  }

  static QString stateDisplay(StrategyState state) {
    switch (state) {
    case StrategyState::Created:
      return "Created";
    case StrategyState::Running:
      return "Running";
    case StrategyState::Paused:
      return "Paused";
    case StrategyState::Stopped:
      return "Stopped";
    case StrategyState::Error:
      return "Error";
    case StrategyState::Deleted:
      return "Deleted";
    default:
      return "Unknown";
    }
  }
};

#endif // STRATEGY_INSTANCE_H
