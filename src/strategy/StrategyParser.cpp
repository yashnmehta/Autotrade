#include "strategy/StrategyParser.h"
#include "strategy/IndicatorEngine.h"
#include <QJsonArray>
#include <QJsonDocument>

// ═══════════════════════════════════════════════════════════
// PARSE JSON → StrategyDefinition
// ═══════════════════════════════════════════════════════════

StrategyDefinition StrategyParser::parseJSON(const QJsonObject &json,
                                             QString &errorMsg) {
  StrategyDefinition def;
  errorMsg.clear();

  // ── Basic fields ──
  def.strategyId = json["strategy_id"].toString();
  def.name = json["name"].toString();
  def.version = json["version"].toString("1.0");
  def.symbol = json["symbol"].toString();
  def.segment = json["segment"].toInt(2);
  def.timeframe = json["timeframe"].toString("1m");

  if (def.name.isEmpty()) {
    errorMsg = "Strategy name is required";
    return def;
  }
  if (def.symbol.isEmpty()) {
    errorMsg = "Symbol is required";
    return def;
  }

  // ── User parameters ──
  if (json.contains("parameters")) {
    QJsonObject params = json["parameters"].toObject();
    for (auto it = params.begin(); it != params.end(); ++it) {
      def.userParameters[it.key()] = it.value().toVariant();
    }
  }

  // ── Indicators ──
  if (json.contains("indicators")) {
    QJsonArray indArr = json["indicators"].toArray();
    for (const QJsonValue &val : indArr) {
      if (val.isObject()) {
        IndicatorConfig cfg = parseIndicatorConfig(val.toObject());
        if (!IndicatorEngine::isValidIndicator(cfg.type)) {
          errorMsg =
              QString("Invalid indicator type: '%1'. Supported: %2")
                  .arg(cfg.type)
                  .arg(IndicatorEngine::supportedIndicators().join(", "));
          return def;
        }
        def.indicators.append(cfg);
      } else if (val.isString()) {
        // Simple string like "RSI_14" → parse into IndicatorConfig
        QString indStr = val.toString();
        IndicatorConfig cfg;
        cfg.id = indStr;

        // Parse "RSI_14" → type=RSI, period=14
        int underscoreIdx = indStr.indexOf('_');
        if (underscoreIdx > 0) {
          cfg.type = indStr.left(underscoreIdx);
          bool ok;
          int period = indStr.mid(underscoreIdx + 1).toInt(&ok);
          if (ok)
            cfg.period = period;
        } else {
          cfg.type = indStr;
        }

        if (!IndicatorEngine::isValidIndicator(cfg.type)) {
          errorMsg =
              QString("Invalid indicator type: '%1'. Supported: %2")
                  .arg(cfg.type)
                  .arg(IndicatorEngine::supportedIndicators().join(", "));
          return def;
        }
        def.indicators.append(cfg);
      }
    }
  }

  // ── Entry rules ──
  if (json.contains("entry_rules")) {
    QJsonObject entry = json["entry_rules"].toObject();
    if (entry.contains("long_entry")) {
      def.longEntryRules = parseConditionGroup(entry["long_entry"].toObject());
    }
    if (entry.contains("short_entry")) {
      def.shortEntryRules =
          parseConditionGroup(entry["short_entry"].toObject());
    }
  }

  // ── Exit rules (condition-based) ──
  if (json.contains("exit_rules")) {
    QJsonObject exitObj = json["exit_rules"].toObject();

    // Parse SL/Target into risk params
    if (exitObj.contains("stop_loss")) {
      QJsonObject sl = exitObj["stop_loss"].toObject();
      def.riskManagement.stopLossPercent = sl["value"].toDouble(1.0);
    }
    if (exitObj.contains("target")) {
      QJsonObject tgt = exitObj["target"].toObject();
      def.riskManagement.targetPercent = tgt["value"].toDouble(2.0);
    }

    // Time exit
    if (exitObj.contains("time_exit")) {
      QJsonObject te = exitObj["time_exit"].toObject();
      def.riskManagement.timeBasedExitEnabled = te["enabled"].toBool(false);
      QString timeStr = te["time"].toString("15:15:00");
      def.riskManagement.exitTime = QTime::fromString(timeStr, "HH:mm:ss");
    }

    // Trailing stop
    if (exitObj.contains("trailing_stop")) {
      QJsonObject ts = exitObj["trailing_stop"].toObject();
      def.riskManagement.trailingStopEnabled = ts["enabled"].toBool(false);
      def.riskManagement.trailingTriggerPercent =
          ts["trigger_profit"].toDouble(1.0);
      def.riskManagement.trailingAmountPercent =
          ts["trail_amount"].toDouble(0.5);
    }

    // Condition-based exit
    if (exitObj.contains("long_exit")) {
      def.longExitRules = parseConditionGroup(exitObj["long_exit"].toObject());
    }
    if (exitObj.contains("short_exit")) {
      def.shortExitRules =
          parseConditionGroup(exitObj["short_exit"].toObject());
    }
  }

  // ── Risk management ──
  if (json.contains("risk_management")) {
    RiskParams rp = parseRiskParams(json["risk_management"].toObject());
    // Merge: don't overwrite SL/Target if already set from exit_rules
    if (def.riskManagement.stopLossPercent <= 0)
      def.riskManagement.stopLossPercent = rp.stopLossPercent;
    if (def.riskManagement.targetPercent <= 0)
      def.riskManagement.targetPercent = rp.targetPercent;
    def.riskManagement.positionSize = rp.positionSize;
    def.riskManagement.maxPositions = rp.maxPositions;
    def.riskManagement.maxDailyLoss = rp.maxDailyLoss;
    def.riskManagement.maxDailyTrades = rp.maxDailyTrades;

    if (rp.trailingStopEnabled) {
      def.riskManagement.trailingStopEnabled = true;
      def.riskManagement.trailingTriggerPercent = rp.trailingTriggerPercent;
      def.riskManagement.trailingAmountPercent = rp.trailingAmountPercent;
    }
    if (rp.timeBasedExitEnabled) {
      def.riskManagement.timeBasedExitEnabled = true;
      def.riskManagement.exitTime = rp.exitTime;
    }
  }

  // ── Validate ──
  QString valError;
  if (!validate(def, valError)) {
    errorMsg = valError;
  }

  return def;
}

// ═══════════════════════════════════════════════════════════
// PARSE HELPERS
// ═══════════════════════════════════════════════════════════

IndicatorConfig
StrategyParser::parseIndicatorConfig(const QJsonObject &json) {
  IndicatorConfig cfg;
  cfg.type = json["type"].toString(json["name"].toString());
  cfg.period = json["period"].toInt(14);
  cfg.period2 = json["period2"].toInt(0);
  cfg.period3 = json["period3"].toInt(0);
  cfg.priceField = json["price_field"].toString("close");
  cfg.param1 = json["param1"].toDouble(0.0);

  // Generate ID if not provided
  cfg.id = json["id"].toString();
  if (cfg.id.isEmpty()) {
    cfg.id = cfg.type.toUpper() + "_" + QString::number(cfg.period);
    if (cfg.period2 > 0)
      cfg.id += "_" + QString::number(cfg.period2);
  }

  return cfg;
}

Condition StrategyParser::parseCondition(const QJsonObject &json) {
  Condition cond;

  QString typeStr = json["type"].toString("indicator").toLower();
  if (typeStr == "indicator")
    cond.type = Condition::Indicator;
  else if (typeStr == "price")
    cond.type = Condition::Price;
  else if (typeStr == "time")
    cond.type = Condition::Time;
  else if (typeStr == "position_count")
    cond.type = Condition::PositionCount;
  else if (typeStr == "price_vs_indicator")
    cond.type = Condition::PriceVsIndicator;
  else
    cond.type = Condition::Indicator;

  cond.indicator = json["indicator"].toString();
  cond.operator_ = json["operator"].toString(">");
  cond.value = json["value"].toVariant();
  cond.field = json["field"].toString("close");

  // Time conditions
  if (cond.type == Condition::Time) {
    cond.timeStart = QTime::fromString(json["start"].toString(), "HH:mm:ss");
    cond.timeEnd = QTime::fromString(json["end"].toString(), "HH:mm:ss");
  }

  return cond;
}

ConditionGroup
StrategyParser::parseConditionGroup(const QJsonObject &json) {
  ConditionGroup group;

  QString op = json["operator"].toString("AND").toUpper();
  group.logicOperator = (op == "OR") ? ConditionGroup::OR : ConditionGroup::AND;

  // Parse flat conditions
  if (json.contains("conditions")) {
    QJsonArray condArr = json["conditions"].toArray();
    for (const QJsonValue &val : condArr) {
      group.conditions.append(parseCondition(val.toObject()));
    }
  }

  // Parse nested groups
  if (json.contains("groups")) {
    QJsonArray groupArr = json["groups"].toArray();
    for (const QJsonValue &val : groupArr) {
      group.nestedGroups.append(parseConditionGroup(val.toObject()));
    }
  }

  return group;
}

RiskParams StrategyParser::parseRiskParams(const QJsonObject &json) {
  RiskParams rp;
  rp.stopLossPercent =
      json["stop_loss_percent"].toDouble(json["stop_loss"].toDouble(1.0));
  rp.targetPercent =
      json["target_percent"].toDouble(json["target"].toDouble(2.0));
  rp.positionSize = json["position_size"].toInt(1);
  rp.maxPositions = json["max_positions"].toInt(1);
  rp.maxDailyLoss = json["max_daily_loss"].toDouble(5000.0);
  rp.maxDailyTrades = json["max_daily_trades"].toInt(10);

  if (json.contains("trailing_stop")) {
    QJsonObject ts = json["trailing_stop"].toObject();
    rp.trailingStopEnabled = ts["enabled"].toBool(false);
    rp.trailingTriggerPercent = ts["trigger_profit"].toDouble(1.0);
    rp.trailingAmountPercent = ts["trail_amount"].toDouble(0.5);
  }

  if (json.contains("time_exit")) {
    QJsonObject te = json["time_exit"].toObject();
    rp.timeBasedExitEnabled = te["enabled"].toBool(false);
    rp.exitTime = QTime::fromString(te["time"].toString("15:15:00"), "HH:mm:ss");
  }

  return rp;
}

// ═══════════════════════════════════════════════════════════
// TO JSON - StrategyDefinition → QJsonObject
// ═══════════════════════════════════════════════════════════

QJsonObject StrategyParser::toJSON(const StrategyDefinition &def) {
  QJsonObject json;

  json["strategy_id"] = def.strategyId;
  json["name"] = def.name;
  json["version"] = def.version;
  json["symbol"] = def.symbol;
  json["segment"] = def.segment;
  json["timeframe"] = def.timeframe;

  // Parameters
  if (!def.userParameters.isEmpty()) {
    QJsonObject params;
    for (auto it = def.userParameters.begin(); it != def.userParameters.end();
         ++it) {
      params[it.key()] = QJsonValue::fromVariant(it.value());
    }
    json["parameters"] = params;
  }

  // Indicators
  QJsonArray indArr;
  for (const IndicatorConfig &cfg : def.indicators) {
    indArr.append(indicatorConfigToJSON(cfg));
  }
  json["indicators"] = indArr;

  // Entry rules
  QJsonObject entryRules;
  if (!def.longEntryRules.isEmpty())
    entryRules["long_entry"] = conditionGroupToJSON(def.longEntryRules);
  if (!def.shortEntryRules.isEmpty())
    entryRules["short_entry"] = conditionGroupToJSON(def.shortEntryRules);
  json["entry_rules"] = entryRules;

  // Exit rules
  QJsonObject exitRules;
  exitRules["stop_loss"] = QJsonObject{
      {"type", "percentage"},
      {"value", def.riskManagement.stopLossPercent}};
  exitRules["target"] = QJsonObject{
      {"type", "percentage"},
      {"value", def.riskManagement.targetPercent}};
  if (def.riskManagement.timeBasedExitEnabled) {
    exitRules["time_exit"] = QJsonObject{
        {"enabled", true},
        {"time", def.riskManagement.exitTime.toString("HH:mm:ss")}};
  }
  if (def.riskManagement.trailingStopEnabled) {
    exitRules["trailing_stop"] = QJsonObject{
        {"enabled", true},
        {"trigger_profit", def.riskManagement.trailingTriggerPercent},
        {"trail_amount", def.riskManagement.trailingAmountPercent}};
  }
  if (!def.longExitRules.isEmpty())
    exitRules["long_exit"] = conditionGroupToJSON(def.longExitRules);
  if (!def.shortExitRules.isEmpty())
    exitRules["short_exit"] = conditionGroupToJSON(def.shortExitRules);
  json["exit_rules"] = exitRules;

  // Risk management
  json["risk_management"] = riskParamsToJSON(def.riskManagement);

  return json;
}

QJsonObject
StrategyParser::indicatorConfigToJSON(const IndicatorConfig &cfg) {
  QJsonObject json;
  json["id"] = cfg.id;
  json["type"] = cfg.type;
  json["period"] = cfg.period;
  if (cfg.period2 > 0)
    json["period2"] = cfg.period2;
  if (cfg.period3 > 0)
    json["period3"] = cfg.period3;
  json["price_field"] = cfg.priceField;
  if (cfg.param1 > 0)
    json["param1"] = cfg.param1;
  return json;
}

QJsonObject StrategyParser::conditionToJSON(const Condition &cond) {
  QJsonObject json;

  switch (cond.type) {
  case Condition::Indicator:
    json["type"] = "indicator";
    break;
  case Condition::Price:
    json["type"] = "price";
    break;
  case Condition::Time:
    json["type"] = "time";
    break;
  case Condition::PositionCount:
    json["type"] = "position_count";
    break;
  case Condition::PriceVsIndicator:
    json["type"] = "price_vs_indicator";
    break;
  }

  if (!cond.indicator.isEmpty())
    json["indicator"] = cond.indicator;
  json["operator"] = cond.operator_;
  json["value"] = QJsonValue::fromVariant(cond.value);

  if (cond.type == Condition::Time) {
    json["start"] = cond.timeStart.toString("HH:mm:ss");
    json["end"] = cond.timeEnd.toString("HH:mm:ss");
  }

  return json;
}

QJsonObject
StrategyParser::conditionGroupToJSON(const ConditionGroup &group) {
  QJsonObject json;
  json["operator"] =
      (group.logicOperator == ConditionGroup::AND) ? "AND" : "OR";

  QJsonArray condArr;
  for (const Condition &cond : group.conditions) {
    condArr.append(conditionToJSON(cond));
  }
  json["conditions"] = condArr;

  if (!group.nestedGroups.isEmpty()) {
    QJsonArray groupArr;
    for (const ConditionGroup &nested : group.nestedGroups) {
      groupArr.append(conditionGroupToJSON(nested));
    }
    json["groups"] = groupArr;
  }

  return json;
}

QJsonObject StrategyParser::riskParamsToJSON(const RiskParams &risk) {
  QJsonObject json;
  json["stop_loss_percent"] = risk.stopLossPercent;
  json["target_percent"] = risk.targetPercent;
  json["position_size"] = risk.positionSize;
  json["max_positions"] = risk.maxPositions;
  json["max_daily_loss"] = risk.maxDailyLoss;
  json["max_daily_trades"] = risk.maxDailyTrades;

  if (risk.trailingStopEnabled) {
    json["trailing_stop"] = QJsonObject{
        {"enabled", true},
        {"trigger_profit", risk.trailingTriggerPercent},
        {"trail_amount", risk.trailingAmountPercent}};
  }

  if (risk.timeBasedExitEnabled) {
    json["time_exit"] = QJsonObject{
        {"enabled", true},
        {"time", risk.exitTime.toString("HH:mm:ss")}};
  }

  return json;
}

// ═══════════════════════════════════════════════════════════
// VALIDATE
// ═══════════════════════════════════════════════════════════

bool StrategyParser::validate(const StrategyDefinition &def,
                              QString &errorMsg) {
  if (def.name.isEmpty()) {
    errorMsg = "Strategy name is required";
    return false;
  }

  if (def.symbol.isEmpty()) {
    errorMsg = "Symbol is required";
    return false;
  }

  // Must have at least one entry rule
  if (def.longEntryRules.isEmpty() && def.shortEntryRules.isEmpty()) {
    errorMsg = "At least one entry rule (long or short) is required";
    return false;
  }

  // Validate indicators
  for (const IndicatorConfig &cfg : def.indicators) {
    if (!IndicatorEngine::isValidIndicator(cfg.type)) {
      errorMsg = QString("Invalid indicator type: '%1'").arg(cfg.type);
      return false;
    }
    if (cfg.period <= 0) {
      errorMsg = QString("Invalid period for indicator '%1': %2")
                     .arg(cfg.id)
                     .arg(cfg.period);
      return false;
    }
  }

  // Validate operators in conditions
  auto validateConditionGroup =
      [&](const ConditionGroup &group,
          const QString &context) -> bool {
    for (const Condition &cond : group.conditions) {
      if (cond.type != Condition::Time &&
          !validateOperator(cond.operator_)) {
        errorMsg = QString("Invalid operator '%1' in %2")
                       .arg(cond.operator_)
                       .arg(context);
        return false;
      }
    }
    return true;
  };

  if (!def.longEntryRules.isEmpty() &&
      !validateConditionGroup(def.longEntryRules, "long entry rules"))
    return false;
  if (!def.shortEntryRules.isEmpty() &&
      !validateConditionGroup(def.shortEntryRules, "short entry rules"))
    return false;

  // Validate risk params
  if (def.riskManagement.stopLossPercent <= 0 ||
      def.riskManagement.stopLossPercent > 50) {
    errorMsg = "Stop loss must be between 0% and 50%";
    return false;
  }
  if (def.riskManagement.targetPercent <= 0 ||
      def.riskManagement.targetPercent > 100) {
    errorMsg = "Target must be between 0% and 100%";
    return false;
  }
  if (def.riskManagement.maxDailyTrades <= 0) {
    errorMsg = "Max daily trades must be > 0";
    return false;
  }

  // Complexity limit: max 20 conditions total
  int totalConditions = def.longEntryRules.conditions.size() +
                        def.shortEntryRules.conditions.size() +
                        def.longExitRules.conditions.size() +
                        def.shortExitRules.conditions.size();
  if (totalConditions > 20) {
    errorMsg = QString("Too many conditions (%1). Maximum is 20.")
                   .arg(totalConditions);
    return false;
  }

  return true;
}

bool StrategyParser::validateIndicator(const QString &type) {
  return IndicatorEngine::isValidIndicator(type);
}

bool StrategyParser::validateOperator(const QString &op) {
  return validOperators().contains(op);
}

QStringList StrategyParser::validOperators() {
  return {">", "<", ">=", "<=", "==", "!=", "between"};
}
