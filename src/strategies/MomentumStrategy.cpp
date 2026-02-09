#include "strategies/MomentumStrategy.h"
#include <QDebug>

MomentumStrategy::MomentumStrategy(QObject *parent) : StrategyBase(parent) {}

void MomentumStrategy::onTick(const UDP::MarketTick &tick) {
  if (!m_isRunning)
    return;

  // Simple momentum logic:
  // Buy if LTP > Target_Entry
  // Sell if LTP < Stop_Loss

  double entry = getParameter<double>("entry_price", 0.0);
  double target = getParameter<double>("target", 0.0);
  double stopLoss = getParameter<double>("stop_loss", 0.0);

  // Check if we entered?
  // Assume we are NOT in a position yet if activePositions == 0

  if (m_instance.activePositions == 0) {
    if (tick.ltp > entry + target) {
      log(QString("Price %1 > Target %2, would BUY!")
              .arg(tick.ltp)
              .arg(target));
      // Trigger Buy Order
      // m_service->placeOrder(...)
    }
  } else {
    // Manage Position
    if (tick.ltp < entry - stopLoss) {
      log(QString("Price %1 < StopLoss %2, would SELL!")
              .arg(tick.ltp)
              .arg(stopLoss));
      // Trigger Sell Order
    }
  }
}
