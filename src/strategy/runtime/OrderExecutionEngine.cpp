#include "strategy/runtime/OrderExecutionEngine.h"
#include <QDebug>
#include <algorithm>
#include <cmath>

// ═══════════════════════════════════════════════════════════
// BUILD LIMIT ORDER
// ═══════════════════════════════════════════════════════════

XTS::OrderParams OrderExecutionEngine::buildLimitOrder(
    const UDP::MarketTick &tick,
    const QString &side,
    int qty,
    const QString &productType,
    const QString &exchangeSegment,
    const QString &clientID,
    const QString &uniqueId,
    double tickSize,
    const ExecutionConfig &config) {

  double limitPrice = calculateLimitPrice(tick, side, tickSize, config);

  // Clamp against exchange limits if available
  double lower = tick.low > 0 ? tick.low * 0.80 : 0.0;  // Fallback: 20% below day low
  double upper = tick.high > 0 ? tick.high * 1.20 : 0.0; // Fallback: 20% above day high

  // Use circuit limits from tick if available (via prevClose-based approximation)
  // In production, these come from UnifiedState.lowerCircuit/upperCircuit
  if (tick.ltp > 0 && limitPrice > 0) {
    limitPrice = clampAndValidate(
        limitPrice, tick.ltp, lower, upper,
        tickSize, side == "BUY", config.lpprPercent);
  }

  XTS::OrderParams params;
  params.exchangeSegment = exchangeSegment;
  params.exchangeInstrumentID = tick.token;
  params.productType = productType;
  params.orderType = "Limit";
  params.orderSide = side;
  params.timeInForce = "DAY";
  params.orderQuantity = qty;
  params.disclosedQuantity = 0;
  params.limitPrice = limitPrice;
  params.stopPrice = 0.0;
  params.orderUniqueIdentifier = uniqueId;
  params.clientID = clientID;

  return params;
}

// ═══════════════════════════════════════════════════════════
// CALCULATE LIMIT PRICE
// ═══════════════════════════════════════════════════════════

double OrderExecutionEngine::calculateLimitPrice(
    const UDP::MarketTick &tick,
    const QString &side,
    double tickSize,
    const ExecutionConfig &config) {

  bool isBuy = (side == "BUY");
  double bestBid = tick.bestBid();
  double bestAsk = tick.bestAsk();
  double ltp = tick.ltp;

  // Fallback if depth data is missing
  if (bestBid <= 0 && bestAsk <= 0) {
    // No depth at all — use LTP with buffer
    if (ltp <= 0) return 0.0;
    double buffer = config.bufferTicks * tickSize;
    double price = isBuy ? (ltp + buffer) : (ltp - buffer);
    return roundToTick(price, tickSize, isBuy);
  }

  // If only one side of depth is available
  if (bestBid <= 0) bestBid = bestAsk - tickSize;
  if (bestAsk <= 0) bestAsk = bestBid + tickSize;

  double spread = bestAsk - bestBid;
  double buffer = config.bufferTicks * tickSize;

  switch (config.mode) {

  case OEEPricingMode::Passive:
    // Place at best bid (buy) / best ask (sell) — join the queue
    if (isBuy) {
      return roundToTick(bestBid, tickSize, false);
    } else {
      return roundToTick(bestAsk, tickSize, true);
    }

  case OEEPricingMode::Aggressive:
    // Cross the spread with extra buffer ticks
    if (isBuy) {
      // Buy aggressive: go above best ask
      double price = bestAsk + buffer;
      return roundToTick(price, tickSize, true);
    } else {
      // Sell aggressive: go below best bid
      double price = bestBid - buffer;
      return roundToTick(price, tickSize, false);
    }

  case OEEPricingMode::Smart:
  default:
    // Auto-decide based on spread width
    //   - Tight spread (≤ 2 ticks): use passive + 1 tick (join near top)
    //   - Medium spread (3-5 ticks): cross to midpoint + 1 tick
    //   - Wide spread (> 5 ticks): aggressive with reduced buffer
    {
      double spreadTicks = spread / tickSize;

      if (spreadTicks <= 2.0) {
        // Tight spread: place 1 tick inside the spread
        if (isBuy) {
          return roundToTick(bestBid + tickSize, tickSize, true);
        } else {
          return roundToTick(bestAsk - tickSize, tickSize, false);
        }
      } else if (spreadTicks <= 5.0) {
        // Medium spread: aim for mid + 1 tick toward fill
        double mid = (bestBid + bestAsk) / 2.0;
        if (isBuy) {
          return roundToTick(mid + tickSize, tickSize, true);
        } else {
          return roundToTick(mid - tickSize, tickSize, false);
        }
      } else {
        // Wide spread: go aggressive but with smaller buffer
        if (isBuy) {
          return roundToTick(bestAsk + tickSize, tickSize, true);
        } else {
          return roundToTick(bestBid - tickSize, tickSize, false);
        }
      }
    }
  }
}

// ═══════════════════════════════════════════════════════════
// TICK SIZE ROUNDING (TER Compliance)
// ═══════════════════════════════════════════════════════════

double OrderExecutionEngine::roundToTick(double price, double tickSize,
                                          bool roundUp) {
  if (tickSize <= 0) tickSize = 0.05;
  if (price <= 0) return tickSize;

  // Round to nearest tick
  double ticks = price / tickSize;
  double rounded = roundUp ? std::ceil(ticks) : std::floor(ticks);
  double result = rounded * tickSize;

  // Avoid floating point artifacts: round to 2 decimal places
  result = std::round(result * 100.0) / 100.0;

  return std::max(result, tickSize); // Never return 0 or negative
}

// ═══════════════════════════════════════════════════════════
// LPPR VALIDATION
// ═══════════════════════════════════════════════════════════

bool OrderExecutionEngine::validateLPPR(double price, double ltp,
                                         double lpprPercent) {
  if (ltp <= 0 || price <= 0) return true; // Can't validate without LTP
  double deviation = std::abs(price - ltp) / ltp * 100.0;
  return deviation <= lpprPercent;
}

// ═══════════════════════════════════════════════════════════
// DRP VALIDATION (Circuit limits)
// ═══════════════════════════════════════════════════════════

bool OrderExecutionEngine::validateDRP(double price, double lowerCircuit,
                                        double upperCircuit) {
  if (lowerCircuit <= 0 && upperCircuit <= 0) return true; // No limits available
  if (lowerCircuit > 0 && price < lowerCircuit) return false;
  if (upperCircuit > 0 && price > upperCircuit) return false;
  return true;
}

// ═══════════════════════════════════════════════════════════
// CLAMP AND VALIDATE
// ═══════════════════════════════════════════════════════════

double OrderExecutionEngine::clampAndValidate(
    double price, double ltp,
    double lowerCircuit, double upperCircuit,
    double tickSize, bool isBuy, double lpprPercent) {

  if (price <= 0 || ltp <= 0) return price;

  // 1. Clamp to LPPR range
  double lpprLower = ltp * (1.0 - lpprPercent / 100.0);
  double lpprUpper = ltp * (1.0 + lpprPercent / 100.0);
  price = std::max(price, lpprLower);
  price = std::min(price, lpprUpper);

  // 2. Clamp to circuit limits (DRP)
  if (lowerCircuit > 0) price = std::max(price, lowerCircuit);
  if (upperCircuit > 0) price = std::min(price, upperCircuit);

  // 3. Round to valid tick size
  price = roundToTick(price, tickSize, isBuy);

  return price;
}
