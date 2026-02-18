#include "strategies/CustomStrategy.h"
#include "repository/ContractData.h"
#include "repository/RepositoryManager.h"
#include "strategy/StrategyParser.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

CustomStrategy::CustomStrategy(QObject *parent) : StrategyBase(parent) {}

CustomStrategy::~CustomStrategy() = default;

// ═══════════════════════════════════════════════════════════
// INIT - Parse JSON definition, configure indicators
// ═══════════════════════════════════════════════════════════

void CustomStrategy::init(const StrategyInstance &instance) {
  StrategyBase::init(instance);

  // Parse strategy definition from parameters
  QString jsonStr = m_instance.parameters.value("definition").toString();
  if (jsonStr.isEmpty()) {
    log("ERROR: No 'definition' found in parameters");
    updateState(StrategyState::Error);
    return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
  if (doc.isNull()) {
    log("ERROR: Invalid JSON in 'definition'");
    updateState(StrategyState::Error);
    return;
  }

  QString error;
  m_definition = StrategyParser::parseJSON(doc.object(), error);
  if (!error.isEmpty()) {
    log("ERROR: Strategy parse failed: " + error);
    updateState(StrategyState::Error);
    return;
  }

  // Configure indicator engine
  m_indicatorEngine.configure(m_definition.indicators);

  // Parse timeframe
  m_timeframe = ChartData::stringToTimeframe(m_definition.timeframe);

  // Initialize risk params from definition
  m_instance.stopLoss = m_definition.riskManagement.stopLossPercent;
  m_instance.target = m_definition.riskManagement.targetPercent;
  m_instance.quantity = m_definition.riskManagement.positionSize;

  log(QString("CustomStrategy initialized: '%1' | Symbol: %2 | "
              "Timeframe: %3 | Indicators: %4 | SL: %5% | Target: %6%")
          .arg(m_definition.name)
          .arg(m_definition.symbol)
          .arg(m_definition.timeframe)
          .arg(m_definition.indicators.size())
          .arg(m_definition.riskManagement.stopLossPercent)
          .arg(m_definition.riskManagement.targetPercent));
}

// ═══════════════════════════════════════════════════════════
// START / STOP
// ═══════════════════════════════════════════════════════════

void CustomStrategy::start() {
  resetDailyCounters();
  m_inPosition = false;
  m_candleStarted = false;
  m_indicatorEngine.reset();
  m_indicatorEngine.configure(m_definition.indicators);

  StrategyBase::start();
  log("CustomStrategy started");
}

void CustomStrategy::stop() {
  if (m_inPosition) {
    log("WARNING: Stopping with open position. Consider manual exit.");
  }
  StrategyBase::stop();
  log("CustomStrategy stopped");
}

// ═══════════════════════════════════════════════════════════
// ON TICK - Main entry point for market data
// ═══════════════════════════════════════════════════════════

void CustomStrategy::onTick(const UDP::MarketTick &tick) {
  if (!m_isRunning)
    return;

  m_currentLTP = tick.ltp;
  m_latestTick = tick;  // Store full tick for depth-based limit pricing

  // Capture instrument token and tick size from first tick
  if (m_instrumentToken == 0 && tick.token > 0) {
    m_instrumentToken = tick.token;
    m_tickSize = resolveTickSize();
    log(QString("Instrument token resolved: %1 | Tick size: %2")
            .arg(m_instrumentToken)
            .arg(m_tickSize));
  }

  // Reset daily counters if new day
  resetDailyCounters();

  // Check if daily limit already hit
  if (m_dailyLimitHit) {
    return;
  }

  // Build candles from ticks
  updateCandle(tick);

  // Check risk limits first
  checkRiskLimits();

  if (m_dailyLimitHit)
    return;

  // Entry/Exit logic (only evaluate when we have indicators ready)
  if (m_indicatorEngine.candleCount() > 0) {
    if (!m_inPosition) {
      checkEntrySignals();
    } else {
      // Update position tracking
      if (m_positionSide == "BUY") {
        m_highestPriceSinceEntry =
            std::max(m_highestPriceSinceEntry, m_currentLTP);
      } else {
        m_lowestPriceSinceEntry =
            std::min(m_lowestPriceSinceEntry, m_currentLTP);
      }

      checkExitSignals();
      updateTrailingStop(m_currentLTP);
      checkTimeBasedExit();
    }
  }

  // Update metrics
  if (m_inPosition) {
    double pnl = 0.0;
    if (m_positionSide == "BUY") {
      pnl = (m_currentLTP - m_entryPrice) * m_instance.quantity;
    } else {
      pnl = (m_entryPrice - m_currentLTP) * m_instance.quantity;
    }
    emit metricsUpdated(m_instance, pnl, 1, 0);
  }
}

// ═══════════════════════════════════════════════════════════
// CANDLE BUILDING FROM TICKS
// ═══════════════════════════════════════════════════════════

void CustomStrategy::updateCandle(const UDP::MarketTick &tick) {
  qint64 now = QDateTime::currentSecsSinceEpoch();
  qint64 candleStart = ChartData::getCandleStartTime(now, m_timeframe);

  if (!m_candleStarted || candleStart != m_currentCandle.timestamp) {
    // Finalize previous candle if exists
    if (m_candleStarted && m_currentCandle.isValid()) {
      finalizeCandle();
    }

    // Start new candle
    m_currentCandle = ChartData::Candle();
    m_currentCandle.timestamp = candleStart;
    m_currentCandle.open = tick.ltp;
    m_currentCandle.high = tick.ltp;
    m_currentCandle.low = tick.ltp;
    m_currentCandle.close = tick.ltp;
    m_currentCandle.volume = tick.volume;
    m_candleStarted = true;
  } else {
    // Update current candle
    m_currentCandle.high = std::max(m_currentCandle.high, tick.ltp);
    m_currentCandle.low = std::min(m_currentCandle.low, tick.ltp);
    m_currentCandle.close = tick.ltp;
    m_currentCandle.volume = tick.volume; // Cumulative from exchange
  }
}

void CustomStrategy::finalizeCandle() {
  if (!m_currentCandle.isValid())
    return;

  m_indicatorEngine.addCandle(m_currentCandle);

  // Log indicator values periodically (every 10 candles)
  if (m_indicatorEngine.candleCount() % 10 == 0) {
    QStringList vals;
    auto all = m_indicatorEngine.allValues();
    for (auto it = all.begin(); it != all.end(); ++it) {
      vals << QString("%1=%2").arg(it.key()).arg(it.value(), 0, 'f', 2);
    }
    if (!vals.isEmpty()) {
      log("Indicators: " + vals.join(" | "));
    }
  }
}

// ═══════════════════════════════════════════════════════════
// CONDITION EVALUATION
// ═══════════════════════════════════════════════════════════

bool CustomStrategy::evaluateConditionGroup(const ConditionGroup &group) {
  if (group.isEmpty())
    return false;

  if (group.logicOperator == ConditionGroup::AND) {
    // All conditions must be true
    for (const Condition &cond : group.conditions) {
      if (!evaluateCondition(cond))
        return false;
    }
    // All nested groups must be true
    for (const ConditionGroup &nested : group.nestedGroups) {
      if (!evaluateConditionGroup(nested))
        return false;
    }
    return true;
  } else {
    // OR: at least one must be true
    for (const Condition &cond : group.conditions) {
      if (evaluateCondition(cond))
        return true;
    }
    for (const ConditionGroup &nested : group.nestedGroups) {
      if (evaluateConditionGroup(nested))
        return true;
    }
    return false;
  }
}

bool CustomStrategy::evaluateCondition(const Condition &condition) {
  double leftValue = 0.0;

  switch (condition.type) {
  case Condition::Indicator: {
    if (!m_indicatorEngine.isReady(condition.indicator))
      return false;
    leftValue = m_indicatorEngine.value(condition.indicator);
    double rightValue = resolveValue(condition.value);
    return compareValues(leftValue, condition.operator_, rightValue);
  }

  case Condition::Price: {
    leftValue = m_currentLTP;
    double rightValue = resolveValue(condition.value);
    return compareValues(leftValue, condition.operator_, rightValue);
  }

  case Condition::PriceVsIndicator: {
    leftValue = m_currentLTP;
    // Value is an indicator name (e.g., "SMA_20")
    QString indId = condition.value.toString();
    if (!m_indicatorEngine.isReady(indId))
      return false;
    double rightValue = m_indicatorEngine.value(indId);
    return compareValues(leftValue, condition.operator_, rightValue);
  }

  case Condition::PositionCount: {
    leftValue = m_inPosition ? 1.0 : 0.0;
    double rightValue = resolveValue(condition.value);
    return compareValues(leftValue, condition.operator_, rightValue);
  }

  case Condition::Time: {
    QTime now = QTime::currentTime();
    if (condition.operator_ == "between") {
      return now >= condition.timeStart && now <= condition.timeEnd;
    }
    return false;
  }
  }

  return false;
}

bool CustomStrategy::compareValues(double left, const QString &op,
                                   double right) {
  if (op == ">")
    return left > right;
  if (op == "<")
    return left < right;
  if (op == ">=")
    return left >= right;
  if (op == "<=")
    return left <= right;
  if (op == "==")
    return std::abs(left - right) < 1e-6;
  if (op == "!=")
    return std::abs(left - right) >= 1e-6;
  return false;
}

double CustomStrategy::resolveValue(const QVariant &value) {
  if (value.type() == QVariant::String) {
    QString strVal = value.toString();

    // Check if it's an indicator reference (e.g., "SMA_20")
    if (m_indicatorEngine.isReady(strVal)) {
      return m_indicatorEngine.value(strVal);
    }

    // Check if it's a user parameter reference (e.g., "{{rsi_period}}")
    if (strVal.startsWith("{{") && strVal.endsWith("}}")) {
      QString paramKey = strVal.mid(2, strVal.length() - 4);
      if (m_definition.userParameters.contains(paramKey)) {
        return m_definition.userParameters[paramKey].toDouble();
      }
    }

    // Try numeric conversion
    bool ok;
    double num = strVal.toDouble(&ok);
    if (ok)
      return num;
  }

  return value.toDouble();
}

// ═══════════════════════════════════════════════════════════
// ENTRY / EXIT SIGNALS
// ═══════════════════════════════════════════════════════════

void CustomStrategy::checkEntrySignals() {
  // Cooldown check
  qint64 now = QDateTime::currentSecsSinceEpoch();
  if (now - m_lastExitTimestamp < REENTRY_COOLDOWN_SEC)
    return;

  // Max positions check
  if (m_instance.activePositions >=
      m_definition.riskManagement.maxPositions)
    return;

  // Max daily trades check
  if (m_dailyTrades >= m_definition.riskManagement.maxDailyTrades) {
    return;
  }

  // Long entry
  if (!m_definition.longEntryRules.isEmpty() &&
      evaluateConditionGroup(m_definition.longEntryRules)) {
    placeEntryOrder("BUY");
    return;
  }

  // Short entry
  if (!m_definition.shortEntryRules.isEmpty() &&
      evaluateConditionGroup(m_definition.shortEntryRules)) {
    placeEntryOrder("SELL");
    return;
  }
}

void CustomStrategy::checkExitSignals() {
  double pnlPercent = 0.0;
  if (m_positionSide == "BUY") {
    pnlPercent = (m_currentLTP - m_entryPrice) / m_entryPrice * 100.0;
  } else {
    pnlPercent = (m_entryPrice - m_currentLTP) / m_entryPrice * 100.0;
  }

  // Stop loss
  if (pnlPercent <= -m_definition.riskManagement.stopLossPercent) {
    placeExitOrder("Stop Loss hit");
    return;
  }

  // Target
  if (pnlPercent >= m_definition.riskManagement.targetPercent) {
    placeExitOrder("Target reached");
    return;
  }

  // Trailing stop check
  if (m_trailingActivated && m_currentSL > 0) {
    if (m_positionSide == "BUY" && m_currentLTP <= m_currentSL) {
      placeExitOrder("Trailing SL hit");
      return;
    }
    if (m_positionSide == "SELL" && m_currentLTP >= m_currentSL) {
      placeExitOrder("Trailing SL hit");
      return;
    }
  }

  // Condition-based exit
  if (m_positionSide == "BUY" && !m_definition.longExitRules.isEmpty()) {
    if (evaluateConditionGroup(m_definition.longExitRules)) {
      placeExitOrder("Long exit condition met");
      return;
    }
  }
  if (m_positionSide == "SELL" && !m_definition.shortExitRules.isEmpty()) {
    if (evaluateConditionGroup(m_definition.shortExitRules)) {
      placeExitOrder("Short exit condition met");
      return;
    }
  }
}

// ═══════════════════════════════════════════════════════════
// ORDER MANAGEMENT
// ═══════════════════════════════════════════════════════════

void CustomStrategy::placeEntryOrder(const QString &side) {
  m_inPosition = true;
  m_positionSide = side;
  m_entryPrice = m_currentLTP;
  m_highestPriceSinceEntry = m_currentLTP;
  m_lowestPriceSinceEntry = m_currentLTP;
  m_trailingActivated = false;
  m_dailyTrades++;

  // Calculate SL/Target prices
  if (side == "BUY") {
    m_currentSL = m_entryPrice *
                  (1.0 - m_definition.riskManagement.stopLossPercent / 100.0);
    m_currentTarget = m_entryPrice *
                      (1.0 + m_definition.riskManagement.targetPercent / 100.0);
  } else {
    m_currentSL = m_entryPrice *
                  (1.0 + m_definition.riskManagement.stopLossPercent / 100.0);
    m_currentTarget = m_entryPrice *
                      (1.0 - m_definition.riskManagement.targetPercent / 100.0);
  }

  m_instance.activePositions = 1;
  m_instance.entryPrice = m_entryPrice;

  log(QString("ENTRY %1 | Price: %2 | Qty: %3 | SL: %4 | Target: %5")
          .arg(side)
          .arg(m_entryPrice, 0, 'f', 2)
          .arg(m_instance.quantity)
          .arg(m_currentSL, 0, 'f', 2)
          .arg(m_currentTarget, 0, 'f', 2));

  // Place order via signal → StrategyService → MainWindow → XTSInteractiveClient
  XTS::OrderParams params = buildLimitOrderParams(side, m_instance.quantity);
  emit orderRequested(params);
}

void CustomStrategy::placeExitOrder(const QString &reason) {
  double exitPrice = m_currentLTP;
  double pnl = 0.0;

  if (m_positionSide == "BUY") {
    pnl = (exitPrice - m_entryPrice) * m_instance.quantity;
  } else {
    pnl = (m_entryPrice - exitPrice) * m_instance.quantity;
  }

  m_dailyPnL += pnl;
  m_inPosition = false;
  m_instance.activePositions = 0;
  m_lastExitTimestamp = QDateTime::currentSecsSinceEpoch();

  log(QString("EXIT %1 | Reason: %2 | Entry: %3 | Exit: %4 | PnL: %5")
          .arg(m_positionSide)
          .arg(reason)
          .arg(m_entryPrice, 0, 'f', 2)
          .arg(exitPrice, 0, 'f', 2)
          .arg(pnl, 0, 'f', 2));

  // Place exit order (reverse side) - SEBI compliant limit order
  QString exitSide = (m_positionSide == "BUY") ? "SELL" : "BUY";
  XTS::OrderParams params = buildLimitOrderParams(exitSide, m_instance.quantity);
  emit orderRequested(params);

  // Reset position state
  m_positionSide.clear();
  m_entryPrice = 0.0;
  m_currentSL = 0.0;
  m_currentTarget = 0.0;
  m_trailingActivated = false;

  emit metricsUpdated(m_instance, m_dailyPnL, 0, 0);
}

// ═══════════════════════════════════════════════════════════
// ORDER PARAMS BUILDER (SEBI-Compliant Limit Orders)
// ═══════════════════════════════════════════════════════════
  // it it not necessary to use MIS as product type for intraday strategies. Using NRML allows us to keep positions overnight if needed, it depends on default value through config or user defined value while creating the stretegy .

XTS::OrderParams CustomStrategy::buildLimitOrderParams(const QString &side,
                                                    int qty) const {



QString productType =
m_instance.parameters.value("productType", "MIS").toString();
QString uniqueId = QString("CS_%1_%2_%3")
                    .arg(m_instance.instanceId)
                    .arg(side)
                    .arg(QDateTime::currentMSecsSinceEpoch());

  // Use OrderExecutionEngine for smart limit price calculation
  // Default to Smart pricing (auto-adjusts based on spread width)
  OrderExecutionEngine::ExecutionConfig config;
  config.mode = OrderExecutionEngine::Smart;
  config.bufferTicks = 2;
  config.defaultTickSize = m_tickSize;

  XTS::OrderParams params = OrderExecutionEngine::buildLimitOrder(
      m_latestTick, side, qty, productType,
      resolveExchangeSegment(), m_instance.account,
      uniqueId, m_tickSize, config);

  return params;
}

QString CustomStrategy::resolveExchangeSegment() const {
  return RepositoryManager::getExchangeSegmentName(m_instance.segment);
}

double CustomStrategy::resolveTickSize() const {
  // Try to get tick size from RepositoryManager contract data
  const ContractData *contract = RepositoryManager::getInstance()
      ->getContractByToken(m_instance.segment, m_instrumentToken);
  if (contract && contract->tickSize > 0) {
    return contract->tickSize;
  }
  // Fallback: 0.05 for most NSE instruments
  return 0.05;
}

// ═══════════════════════════════════════════════════════════
// RISK MANAGEMENT
// ═══════════════════════════════════════════════════════════

void CustomStrategy::checkRiskLimits() {
  // Daily loss limit
  if (m_dailyPnL <= -m_definition.riskManagement.maxDailyLoss) {
    if (!m_dailyLimitHit) {
      m_dailyLimitHit = true;
      log(QString("CIRCUIT BREAKER: Daily loss limit hit (₹%1). "
                   "Halting strategy for today.")
              .arg(m_dailyPnL, 0, 'f', 2));

      if (m_inPosition) {
        placeExitOrder("Daily loss limit - forced exit");
      }
    }
    return;
  }

  // Daily trade limit
  if (m_dailyTrades >= m_definition.riskManagement.maxDailyTrades) {
    if (!m_inPosition && !m_dailyLimitHit) {
      m_dailyLimitHit = true;
      log(QString("Daily trade limit reached (%1). No more entries today.")
              .arg(m_dailyTrades));
    }
  }
}

void CustomStrategy::updateTrailingStop(double currentPrice) {
  if (!m_definition.riskManagement.trailingStopEnabled)
    return;
  if (!m_inPosition)
    return;

  double pnlPercent = 0.0;
  if (m_positionSide == "BUY") {
    pnlPercent = (currentPrice - m_entryPrice) / m_entryPrice * 100.0;
  } else {
    pnlPercent = (m_entryPrice - currentPrice) / m_entryPrice * 100.0;
  }

  // Activate trailing stop when profit exceeds trigger
  if (pnlPercent >= m_definition.riskManagement.trailingTriggerPercent) {
    if (!m_trailingActivated) {
      m_trailingActivated = true;
      log(QString("Trailing stop activated at %1% profit")
              .arg(pnlPercent, 0, 'f', 2));
    }

    double trailPercent = m_definition.riskManagement.trailingAmountPercent;

    if (m_positionSide == "BUY") {
      double newSL =
          m_highestPriceSinceEntry * (1.0 - trailPercent / 100.0);
      if (newSL > m_currentSL) {
        m_currentSL = newSL;
      }
    } else {
      double newSL =
          m_lowestPriceSinceEntry * (1.0 + trailPercent / 100.0);
      if (newSL < m_currentSL || m_currentSL <= 0) {
        m_currentSL = newSL;
      }
    }
  }
}

void CustomStrategy::checkTimeBasedExit() {
  if (!m_definition.riskManagement.timeBasedExitEnabled)
    return;
  if (!m_inPosition)
    return;

  QTime now = QTime::currentTime();
  if (now >= m_definition.riskManagement.exitTime) {
    placeExitOrder("Time-based exit");
  }
}

void CustomStrategy::resetDailyCounters() {
  QDate today = QDate::currentDate();
  if (m_lastResetDate != today) {
    m_lastResetDate = today;
    m_dailyTrades = 0;
    m_dailyPnL = 0.0;
    m_dailyLimitHit = false;
    log("Daily counters reset");
  }
}
