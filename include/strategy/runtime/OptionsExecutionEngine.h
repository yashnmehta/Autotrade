#ifndef OPTIONS_EXECUTION_ENGINE_H
#define OPTIONS_EXECUTION_ENGINE_H

#include <QString>
#include <QVector>
#include <cstdint>

/**
 * @brief Executes multi-leg options strategies using battle-tested components
 * 
 * POC Task 3.1: Thin wrapper around existing infrastructure:
 * - ATMCalculator (binary search for nearest strike)
 * - RepositoryManager (strike cache & token lookup)
 * 
 * See: docs/custom_stretegy_builder/form_based approach/08_BACKEND_AUDIT_FINDINGS.md
 */
class OptionsExecutionEngine {
public:
  /**
   * @brief Resolved leg with concrete strike and token
   */
  struct ResolvedLeg {
    QString legId;
    QString tradingSymbol;  // "NIFTY24550CE"
    int64_t token = 0;      // Contract token from master
    int strike = 0;         // Resolved strike (e.g., 24550)
    QString optionType;     // "CE" or "PE"
    QString side;           // "BUY" or "SELL"
    int quantity = 0;
    bool valid = false;
    QString errorMsg;
  };

  /**
   * @brief Resolve ATM strike from spot price using existing ATMCalculator
   * 
   * Implementation: Calls ATMCalculator::calculateFromActualStrikes()
   * with strikes from RepositoryManager::getStrikesForSymbolExpiry()
   * 
   * @param symbol Underlying symbol (e.g., "NIFTY")
   * @param expiry Expiry date (e.g., "30JAN26")
   * @param spotPrice Current spot/future price
   * @param offset Strike offset: 0=ATM, +1=OTM1, -1=ITM1
   * @return Resolved strike price (e.g., 24550)
   * 
   * Gate 1 Criteria #2: resolveATMStrike("NIFTY", "30JAN26", 24567.50, 0) returns 24550
   */
  static int resolveATMStrike(const QString& symbol,
                              const QString& expiry,
                              double spotPrice,
                              int offset = 0);

  /**
   * @brief Build option trading symbol
   * 
   * @param symbol Underlying (e.g., "NIFTY")
   * @param strike Strike price (e.g., 24550)
   * @param optionType "CE" or "PE"
   * @param expiry Expiry date (e.g., "30JAN26")
   * @return Trading symbol (e.g., "NIFTY24550CE")
   * 
   * Note: For POC, returns simple format. Production may need exchange-specific formats.
   */
  static QString buildOptionSymbol(const QString& symbol,
                                   int strike,
                                   const QString& optionType,
                                   const QString& expiry);

  /**
   * @brief Get contract token for option using RepositoryManager
   * 
   * @param symbol Underlying symbol
   * @param expiry Expiry date
   * @param strike Strike price
   * @param optionType "CE" or "PE"
   * @return Contract token (0 if not found)
   */
  static int64_t getContractToken(const QString& symbol,
                                  const QString& expiry,
                                  double strike,
                                  const QString& optionType);

  /**
   * @brief Resolve current weekly expiry (simplified for POC)
   * 
   * @param symbol Underlying symbol
   * @return Expiry date string (e.g., "30JAN26")
   * 
   * Note: For POC, uses hardcoded logic. Production should use RepositoryManager.
   */
  static QString resolveCurrentWeeklyExpiry(const QString& symbol);

  /**
   * @brief Resolve an OptionLeg to concrete strike/symbol/token
   * 
   * @param leg Leg definition from JSON
   * @param strategySymbol Fallback symbol if leg.symbol is empty
   * @param spotPrice Current spot/future price
   * @return Resolved leg with trading details
   */
  static ResolvedLeg resolveLeg(const OptionLeg& leg,
                                const QString& strategySymbol,
                                double spotPrice);

  /**
   * @brief Apply strike offset within sorted strikes array (public for testing)
   * 
   * @param strikes Sorted strike prices
   * @param atmStrike Current ATM strike
   * @param offset +1 for OTM, -1 for ITM, etc.
   * @return Offset strike (or ATM if offset out of bounds)
   */
  static double applyStrikeOffset(const QVector<double>& strikes,
                                  double atmStrike,
                                  int offset);

private:
};

#endif // OPTIONS_EXECUTION_ENGINE_H
