#ifndef STRATEGY_DEFINITION_H
#define STRATEGY_DEFINITION_H

#include <QDateTime>
#include <QString>
#include <QTime>
#include <QVariant>
#include <QVector>

// ═══════════════════════════════════════════════════════════
// CONDITION - Single evaluation rule
// ═══════════════════════════════════════════════════════════

struct Condition {
  enum Type {
    Indicator,     // e.g. RSI(14) < 30
    Price,         // e.g. LTP > 22000
    Time,          // e.g. time between 09:30-15:00
    PositionCount, // e.g. positions == 0
    PriceVsIndicator // e.g. LTP > SMA(20)
  };

  Type type = Indicator;
  QString indicator;  // "RSI_14", "SMA_20", etc.
  QString operator_;  // ">", "<", ">=", "<=", "==", "!="
  QVariant value;     // 30, 70, "SMA_50", etc.
  QString field;      // "close", "high", "low", "open" (for price source)

  // Time-based condition extras
  QTime timeStart;
  QTime timeEnd;
};

// ═══════════════════════════════════════════════════════════
// CONDITION GROUP - AND/OR combination of conditions
// ═══════════════════════════════════════════════════════════

struct ConditionGroup {
  enum LogicOp { AND, OR };

  LogicOp logicOperator = AND;
  QVector<Condition> conditions;
  QVector<ConditionGroup> nestedGroups; // For complex (A AND B) OR (C AND D)

  bool isEmpty() const {
    return conditions.isEmpty() && nestedGroups.isEmpty();
  }
};

// ═══════════════════════════════════════════════════════════
// INDICATOR CONFIG - Which indicators to compute
// ═══════════════════════════════════════════════════════════

struct IndicatorConfig {
  QString id;       // Unique ID: "RSI_14", "SMA_20"
  QString type;     // "RSI", "SMA", "EMA", "MACD", "BB", "ATR", "STOCH", "ADX", "OBV", "VOLUME"
  int period = 14;  // Primary period
  int period2 = 0;  // Secondary period (e.g. MACD signal)
  int period3 = 0;  // Tertiary period (e.g. MACD histogram)
  QString priceField = "close"; // "close", "high", "low", "open", "hl2", "hlc3"
  double param1 = 0.0; // Extra param (e.g. BB stddev multiplier)
};

// ═══════════════════════════════════════════════════════════
// RISK PARAMS - Risk management configuration
// ═══════════════════════════════════════════════════════════

struct RiskParams {
  double stopLossPercent = 1.0;
  double targetPercent = 2.0;
  int positionSize = 1;
  int maxPositions = 1;
  double maxDailyLoss = 5000.0;
  int maxDailyTrades = 10;

  // Trailing stop
  bool trailingStopEnabled = false;
  double trailingTriggerPercent = 1.0; // Activate after X% profit
  double trailingAmountPercent = 0.5;  // Trail by X%

  // Time-based exit
  bool timeBasedExitEnabled = false;
  QTime exitTime = QTime(15, 15, 0);
};

// ═══════════════════════════════════════════════════════════
// STRATEGY DEFINITION - Complete JSON-based strategy spec
// ═══════════════════════════════════════════════════════════

struct StrategyDefinition {
  // Identification
  QString strategyId;
  QString name;
  QString version = "1.0";

  // Market context
  QString symbol;
  int segment = 2; // NSEFO default
  QString timeframe = "1m";

  // User-defined parameters (for template substitution)
  QVariantMap userParameters;

  // Indicators to compute
  QVector<IndicatorConfig> indicators;

  // Entry rules
  ConditionGroup longEntryRules;
  ConditionGroup shortEntryRules;

  // Exit rules (condition-based, in addition to SL/Target)
  ConditionGroup longExitRules;
  ConditionGroup shortExitRules;

  // Risk management
  RiskParams riskManagement;

  bool isValid() const {
    return !name.isEmpty() && !symbol.isEmpty() &&
           (!longEntryRules.isEmpty() || !shortEntryRules.isEmpty());
  }
};

// ═══════════════════════════════════════════════════════════
// OPTION LEG - Single leg in a multi-leg options strategy
// ═══════════════════════════════════════════════════════════

enum class StrikeSelectionMode {
  ATMRelative,   // ATM+0, ATM+1, ATM-2, etc.
  PremiumBased,  // Select strike nearest to target premium
  FixedStrike    // Explicit strike price
};

enum class ExpiryType {
  CurrentWeekly,
  NextWeekly,
  CurrentMonthly,
  SpecificDate
};

struct OptionLeg {
  QString legId;              // "LEG_1", "LEG_2"
  QString side;               // "BUY" or "SELL"
  QString optionType;         // "CE", "PE", "FUT"
  StrikeSelectionMode strikeMode = StrikeSelectionMode::ATMRelative;
  int atmOffset = 0;          // For ATM-relative: 0=ATM, +1=OTM1, -1=ITM1
  double targetPremium = 0.0; // For premium-based selection
  int fixedStrike = 0;        // For fixed strike
  ExpiryType expiry = ExpiryType::CurrentWeekly;
  QString specificExpiry;     // For SpecificDate
  int quantity = 0;           // Position size
};

#endif // STRATEGY_DEFINITION_H
