#ifndef ORDER_EXECUTION_ENGINE_H
#define ORDER_EXECUTION_ENGINE_H

#include "api/xts/XTSTypes.h"
#include "udp/UDPTypes.h"
#include <QObject>
#include <QString>

/**
 * @brief Pricing mode for limit order calculation (SEBI compliance).
 */
enum class OEEPricingMode {
    Passive,    ///< Place at best bid (buy) / best ask (sell) - maker
    Aggressive, ///< Cross spread with buffer ticks - taker, fast fill
    Smart       ///< Auto-select based on spread width
};

/**
 * @brief Configuration for order execution behavior.
 */
struct OEEExecutionConfig {
    OEEPricingMode mode    = OEEPricingMode::Smart;
    int    bufferTicks     = 2;     ///< Extra ticks beyond ask (buy) / below bid (sell)
    double defaultTickSize = 0.05;  ///< Default tick size if contract data unavailable
    double lpprPercent     = 5.0;   ///< LPPR tolerance percent (±5% of LTP)
};

/**
 * @brief Smart order execution engine for SEBI-compliant limit orders.
 *
 * SEBI requires all algorithmic trading strategies to use limit orders only
 * (no market orders). This engine calculates optimal limit prices based on
 * the current order book depth, validates against exchange price protection
 * ranges (LPPR, DRP, TER), and rounds to valid tick sizes.
 */
class OrderExecutionEngine {
public:
  // Expose types at class scope for backward compatibility
  using PricingMode   = OEEPricingMode;
  using ExecutionConfig = OEEExecutionConfig;

  static XTS::OrderParams buildLimitOrder(
      const UDP::MarketTick &tick,
      const QString &side,
      int qty,
      const QString &productType,
      const QString &exchangeSegment,
      const QString &clientID,
      const QString &uniqueId,
      double tickSize = 0.05,
      const OEEExecutionConfig &config = OEEExecutionConfig());

  static double calculateLimitPrice(
      const UDP::MarketTick &tick,
      const QString &side,
      double tickSize,
      const OEEExecutionConfig &config = OEEExecutionConfig());

  static double roundToTick(double price, double tickSize, bool roundUp);

  static bool validateLPPR(double price, double ltp, double lpprPercent = 5.0);

  static bool validateDRP(double price, double lowerCircuit, double upperCircuit);

  static double clampAndValidate(
      double price, double ltp,
      double lowerCircuit, double upperCircuit,
      double tickSize, bool isBuy, double lpprPercent = 5.0);
};

#endif // ORDER_EXECUTION_ENGINE_H
