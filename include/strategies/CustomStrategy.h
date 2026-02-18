#ifndef CUSTOM_STRATEGY_H
#define CUSTOM_STRATEGY_H

#include "api/XTSTypes.h"
#include "data/CandleData.h"
#include "strategies/StrategyBase.h"
#include "strategy/IndicatorEngine.h"
#include "strategy/OrderExecutionEngine.h"
#include "strategy/StrategyDefinition.h"
#include <QDate>
#include <QHash>

/**
 * @brief JSON-driven custom strategy implementation
 *
 * Extends StrategyBase to evaluate user-defined JSON conditions
 * on each market tick. Supports:
 *   - Multiple indicator-based entry/exit conditions
 *   - AND/OR logic combinations
 *   - Stop loss, target, trailing stop
 *   - Time-based exit
 *   - Daily loss/trade limits
 *   - Candle aggregation from ticks
 *
 * The strategy definition is stored as JSON in StrategyInstance::parameters
 * under the key "definition".
 */
class CustomStrategy : public StrategyBase {
  Q_OBJECT

public:
  explicit CustomStrategy(QObject *parent = nullptr);
  ~CustomStrategy() override;

  void init(const StrategyInstance &instance) override;
  void start() override;
  void stop() override;

protected slots:
  void onTick(const UDP::MarketTick &tick) override;

private:
  // ── Candle building from ticks ──
  void updateCandle(const UDP::MarketTick &tick);
  void finalizeCandle();

  // ── Condition evaluation ──
  bool evaluateConditionGroup(const ConditionGroup &group);
  bool evaluateCondition(const Condition &condition);
  bool compareValues(double left, const QString &op, double right);
  double resolveValue(const QVariant &value);

  // ── Signal checking ──
  void checkEntrySignals();
  void checkExitSignals();

  // ── Order management ──
  void placeEntryOrder(const QString &side);
  void placeExitOrder(const QString &reason);
  XTS::OrderParams buildLimitOrderParams(const QString &side, int qty) const;
  QString resolveExchangeSegment() const;
  double resolveTickSize() const;

  // ── Risk management ──
  void checkRiskLimits();
  void updateTrailingStop(double currentPrice);
  void checkTimeBasedExit();
  void resetDailyCounters();

  // ── Data ──
  StrategyDefinition m_definition;
  IndicatorEngine m_indicatorEngine;

  // Candle building
  ChartData::Candle m_currentCandle;
  ChartData::Timeframe m_timeframe = ChartData::Timeframe::OneMinute;
  bool m_candleStarted = false;

  // Position tracking
  bool m_inPosition = false;
  QString m_positionSide; // "BUY" or "SELL"
  double m_entryPrice = 0.0;
  double m_currentSL = 0.0;
  double m_currentTarget = 0.0;
  double m_highestPriceSinceEntry = 0.0;
  double m_lowestPriceSinceEntry = 0.0;
  bool m_trailingActivated = false;

  // Daily risk counters
  int m_dailyTrades = 0;
  double m_dailyPnL = 0.0;
  QDate m_lastResetDate;
  bool m_dailyLimitHit = false;

  // Current price & instrument identification
  double m_currentLTP = 0.0;
  int64_t m_instrumentToken = 0;  // Exchange instrument ID (from first tick)
  UDP::MarketTick m_latestTick;   // Latest tick (for depth-based limit pricing)
  double m_tickSize = 0.05;       // Instrument tick size

  // Cooldown: prevent re-entry immediately after exit
  qint64 m_lastExitTimestamp = 0;
  static constexpr int REENTRY_COOLDOWN_SEC = 5;
};

#endif // CUSTOM_STRATEGY_H
