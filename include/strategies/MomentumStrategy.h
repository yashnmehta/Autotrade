#ifndef MOMENTUM_STRATEGY_H
#define MOMENTUM_STRATEGY_H

#include "strategies/StrategyBase.h"

// Momentum Strategy: Buy if trend is strong (price > moving average or
// breakout) Simple implementation: Buy if LTP > EntryPrice, Sell if LTP <
// EntryPrice
class MomentumStrategy : public StrategyBase {
  Q_OBJECT

public:
  explicit MomentumStrategy(QObject *parent = nullptr);

protected slots:
  void onTick(const UDP::MarketTick &tick) override;
};

#endif // MOMENTUM_STRATEGY_H
