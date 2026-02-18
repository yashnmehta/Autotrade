#ifndef STRATEGY_PARSER_H
#define STRATEGY_PARSER_H

#include "strategy/StrategyDefinition.h"
#include <QJsonObject>
#include <QString>

/**
 * @brief Parses JSON strategy definitions into StrategyDefinition structs
 *
 * Handles:
 *   - JSON → StrategyDefinition (parseJSON)
 *   - StrategyDefinition → JSON (toJSON)
 *   - Validation of all fields
 *   - Template parameter substitution ({{rsi_period}} → 14)
 */
class StrategyParser {
public:
  /// Parse a JSON object into a StrategyDefinition
  /// Returns empty definition on failure; errorMsg contains details
  static StrategyDefinition parseJSON(const QJsonObject &json,
                                      QString &errorMsg);

  /// Convert a StrategyDefinition back to JSON
  static QJsonObject toJSON(const StrategyDefinition &def);

  /// Validate a parsed StrategyDefinition
  static bool validate(const StrategyDefinition &def, QString &errorMsg);

  /// Validate an indicator type string
  static bool validateIndicator(const QString &type);

  /// Validate a comparison operator string
  static bool validateOperator(const QString &op);

  /// Get list of valid operator strings
  static QStringList validOperators();

private:
  static IndicatorConfig parseIndicatorConfig(const QJsonObject &json);
  static Condition parseCondition(const QJsonObject &json);
  static ConditionGroup parseConditionGroup(const QJsonObject &json);
  static RiskParams parseRiskParams(const QJsonObject &json);

  static QJsonObject indicatorConfigToJSON(const IndicatorConfig &cfg);
  static QJsonObject conditionToJSON(const Condition &cond);
  static QJsonObject conditionGroupToJSON(const ConditionGroup &group);
  static QJsonObject riskParamsToJSON(const RiskParams &risk);
};

#endif // STRATEGY_PARSER_H
