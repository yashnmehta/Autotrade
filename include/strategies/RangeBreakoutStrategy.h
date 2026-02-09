#ifndef RANGE_BREAKOUT_STRATEGY_H
#define RANGE_BREAKOUT_STRATEGY_H

#include "strategies/StrategyBase.h"

// Range Breakout: Wait for price to break a defined range (min, max).
// If price > max -> Buy.
// If price < min -> Sell.
class RangeBreakoutStrategy : public StrategyBase {
  Q_OBJECT

public:
  explicit RangeBreakoutStrategy(QObject *parent = nullptr);

  void init(const StrategyInstance &instance) override;

protected slots:
  void onTick(const UDP::MarketTick &tick) override;

private:
  double m_high = 0.0;
  double m_low = 0.0;
};

#endif // RANGE_BREAKOUT_STRATEGY_H
