#ifndef ORDER_EXECUTION_ENGINE_H
#define ORDER_EXECUTION_ENGINE_H

#include "api/XTSTypes.h"
#include "udp/UDPTypes.h"
#include <QObject>
#include <QString>

/**
 * @brief Smart order execution engine for SEBI-compliant limit orders.
 *
 * SEBI requires all algorithmic trading strategies to use limit orders only
 * (no market orders). This engine calculates optimal limit prices based on
 * the current order book depth, validates against exchange price protection
 * ranges (LPPR, DRP, TER), and rounds to valid tick sizes.
 *
 * Pricing Modes:
 *   - Passive: Place at best bid/ask (maker, wait in queue)
 *   - Aggressive: Cross the spread by N ticks (taker, fast fill)
 *   - Smart: Auto-adjust based on spread width and urgency
 *
 * Price Validation:
 *   - TER: Tick-by-Tick Execution Range (round to valid tick size)
 *   - LPPR: Limit Price Protection Range (within ±N% of LTP)
 *   - DRP: Dynamic Price Range / Circuit filters (within daily bands)
 */
class OrderExecutionEngine {
public:
  /**
   * @brief Pricing mode for limit order calculation.
   */
  enum PricingMode {
    Passive,   ///< Place at best bid (buy) / best ask (sell) - maker
    Aggressive,///< Cross spread with buffer ticks - taker, fast fill
    Smart      ///< Auto-select based on spread width
  };

  /**
   * @brief Configuration for order execution behavior.
   */
  struct ExecutionConfig {
    PricingMode mode = Smart;
    int bufferTicks = 2;         ///< Extra ticks beyond ask (buy) / below bid (sell)
    double defaultTickSize = 0.05; ///< Default tick size if contract data unavailable
    double lpprPercent = 5.0;    ///< LPPR tolerance percent (±5% of LTP)
  };

  /**
   * @brief Build a fully populated XTS::OrderParams for a limit order.
   *
   * Uses the latest market tick to determine optimal limit price.
   * Falls back to LTP + buffer if depth data is unavailable.
   *
   * @param tick Latest market tick (has depth, LTP, circuit limits)
   * @param side "BUY" or "SELL"
   * @param qty Order quantity
   * @param productType "MIS", "NRML", or "CNC"
   * @param exchangeSegment "NSECM", "NSEFO", etc.
   * @param clientID Account/client ID
   * @param uniqueId Unique order tracking identifier
   * @param tickSize Instrument tick size (0.05 for most, 0.10 for some)
   * @param config Execution configuration
   * @return Populated OrderParams with limit price
   */
  static XTS::OrderParams buildLimitOrder(
      const UDP::MarketTick &tick,
      const QString &side,
      int qty,
      const QString &productType,
      const QString &exchangeSegment,
      const QString &clientID,
      const QString &uniqueId,
      double tickSize = 0.05,
      const ExecutionConfig &config = ExecutionConfig());

  /**
   * @brief Calculate optimal limit price based on order book depth.
   *
   * @param tick Current market tick with depth data
   * @param side "BUY" or "SELL"
   * @param tickSize Instrument tick size
   * @param config Execution configuration
   * @return Optimal limit price (rounded to tick, validated)
   */
  static double calculateLimitPrice(
      const UDP::MarketTick &tick,
      const QString &side,
      double tickSize,
      const ExecutionConfig &config = ExecutionConfig());

  /**
   * @brief Round price to nearest valid tick size.
   *
   * Exchange requires all prices to be multiples of tick size.
   * Example: tickSize=0.05 → 125.37 becomes 125.35 (buy) or 125.40 (sell)
   *
   * @param price Raw price
   * @param tickSize Instrument tick size
   * @param roundUp If true, round up; if false, round down
   * @return Price rounded to valid tick
   */
  static double roundToTick(double price, double tickSize, bool roundUp);

  /**
   * @brief Validate price against LPPR (Limit Price Protection Range).
   *
   * Exchange rejects orders with prices beyond ±lpprPercent of LTP.
   *
   * @param price Proposed limit price
   * @param ltp Current Last Traded Price
   * @param lpprPercent Allowed deviation percent (typically 5%)
   * @return true if price is within LPPR range
   */
  static bool validateLPPR(double price, double ltp, double lpprPercent = 5.0);

  /**
   * @brief Validate price against DRP (Dynamic Price Range / Circuit limits).
   *
   * @param price Proposed limit price
   * @param lowerCircuit Lower circuit limit
   * @param upperCircuit Upper circuit limit
   * @return true if price is within circuit limits
   */
  static bool validateDRP(double price, double lowerCircuit, double upperCircuit);

  /**
   * @brief Clamp price within exchange-allowed range.
   *
   * Ensures the price is within both LPPR and DRP limits.
   * Rounds to valid tick size after clamping.
   *
   * @param price Raw proposed price
   * @param ltp Current LTP (for LPPR)
   * @param lowerCircuit Lower circuit
   * @param upperCircuit Upper circuit
   * @param tickSize Tick size
   * @param isBuy true for buy (round up clamps), false for sell (round down)
   * @param lpprPercent LPPR tolerance
   * @return Validated, clamped, tick-rounded price
   */
  static double clampAndValidate(
      double price, double ltp,
      double lowerCircuit, double upperCircuit,
      double tickSize, bool isBuy, double lpprPercent = 5.0);
};

#endif // ORDER_EXECUTION_ENGINE_H
