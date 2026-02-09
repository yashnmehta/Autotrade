#include "strategies/RangeBreakoutStrategy.h"
#include <QDebug>

RangeBreakoutStrategy::RangeBreakoutStrategy(QObject *parent)
    : StrategyBase(parent) {}

void RangeBreakoutStrategy::init(const StrategyInstance &instance) {
  StrategyBase::init(instance);

  // Parse JSON parameters for range
  m_high = getParameter<double>("high", 0.0);
  m_low = getParameter<double>("low", 0.0);

  // Alternatively, if not in JSON, derive from entryPrice +- offset?
}

void RangeBreakoutStrategy::onTick(const UDP::MarketTick &tick) {
  if (!m_isRunning)
    return;

  // Simple Range Breakout
  // If LTP > High -> BUY
  // If LTP < Low -> SELL

  if (m_instance.activePositions == 0) {
    if (tick.ltp > m_high && m_high > 0) {
      log(QString("Price %1 > Range High %2, entering LONG position.")
              .arg(tick.ltp)
              .arg(m_high));
      // Trigger Buy Order
    } else if (tick.ltp < m_low && m_low > 0) {
      log(QString("Price %1 < Range Low %2, entering SHORT position.")
              .arg(tick.ltp)
              .arg(m_low));
      // Trigger Sell Order
    }
  } else {
    // Manage Position (Check stop loss, target from base class parameters if
    // set)

    // Example: Trailing Stop?
  }
}
