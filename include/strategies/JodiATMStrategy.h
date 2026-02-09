#ifndef JODI_ATM_STRATEGY_H
#define JODI_ATM_STRATEGY_H

#include "strategies/StrategyBase.h"
#include <cstdint>

// JodiATM Strategy: Specialized straddle management with leg shifting
// Logic: Dynamically shifts 25% quantity across 4 legs as price moves.
// Resets ATM reference if price move exceeds boundary (RCP).
class JodiATMStrategy : public StrategyBase {
  Q_OBJECT

public:
  explicit JodiATMStrategy(QObject *parent = nullptr);

  void init(const StrategyInstance &instance) override;
  void start() override;
  void stop() override;

protected slots:
  void onTick(const UDP::MarketTick &tick) override;
  void onATMUpdated();

private:
  enum class Trend { Nutral, Bullish, Bearish };

  void checkTrade(double cashPrice);
  void calculateThresholds(double atm);
  void resetATM(double newATM);

  // Tokens
  uint32_t m_cashToken = 0;
  uint32_t m_ceToken = 0;     // Current strike CE
  uint32_t m_peToken = 0;     // Current strike PE
  uint32_t m_ceTokenNext = 0; // Next strike CE (Up if bullish, Down if bearish)
  uint32_t m_peTokenNext = 0; // Next strike PE

  // Parameters
  double m_offset = 0.0;
  double m_threshold = 0.0;
  double m_adjPts = 0.0;
  double m_strikeDiff = 0.0;
  int m_baseQty = 0;

  // State
  Trend m_trend = Trend::Nutral;
  int m_currentLeg = 0; // 0 to 4
  double m_currentATM = 0.0;
  double m_blDP = 0.0;      // Bullish Decision Point
  double m_brDP = 0.0;      // Bearish Decision Point
  double m_reversalP = 0.0; // Reversal Point
  double m_blRCP = 0.0;     // Bullish Reset Constant Point
  double m_brRCP = 0.0;     // Bearish Reset Constant Point

  // Monitoring
  double m_cashPrice = 0.0;
  bool m_isFirstOrderPlaced = false;
};

#endif // JODI_ATM_STRATEGY_H
