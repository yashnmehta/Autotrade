/**
 * @file TemplateStrategy.cpp
 * @brief Generic runtime engine for user-defined strategy templates.
 *
 * This is the single class that can run ANY strategy template created by the
 * user in the StrategyTemplateBuilderDialog. It evaluates conditions,
 * formulas, and indicators against live market data on every tick.
 */

#include "strategies/TemplateStrategy.h"
#include "data/PriceStoreGateway.h"
#include "services/CandleAggregator.h"
#include "services/FeedHandler.h"
#include "services/StrategyTemplateRepository.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTime>

// Forward declaration — used in setupFormulaEngine() and refreshExpressionParams()
static QString triggerToString(ParamTrigger t);

// ═══════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════════

TemplateStrategy::TemplateStrategy(QObject *parent) : StrategyBase(parent) {}

TemplateStrategy::~TemplateStrategy() {
  stop();
  qDeleteAll(m_indicators);
  m_indicators.clear();
  delete m_timeCheckTimer;
}

// ═══════════════════════════════════════════════════════════════════
// INIT — Load template + resolve bindings + setup engines
// ═══════════════════════════════════════════════════════════════════

void TemplateStrategy::init(const StrategyInstance &instance) {
  StrategyBase::init(instance);

  if (!loadTemplate()) {
    log("ERROR: Failed to load strategy template");
    updateState(StrategyState::Error);
    return;
  }

  setupBindings();
  setupIndicators();
  setupFormulaEngine();

  // Resolve risk parameters (from instance overrides or template defaults)
  m_stopLossPct = m_instance.stopLoss > 0
                      ? m_instance.stopLoss
                      : m_template.riskDefaults.stopLossPercent;
  m_targetPct = m_instance.target > 0 ? m_instance.target
                                      : m_template.riskDefaults.targetPercent;
  m_trailingEnabled = m_template.riskDefaults.trailingEnabled;
  m_trailingTriggerPct = m_template.riskDefaults.trailingTriggerPct;
  m_trailingAmountPct = m_template.riskDefaults.trailingAmountPct;
  m_timeExitEnabled = m_template.riskDefaults.timeExitEnabled;
  m_exitTime = m_template.riskDefaults.exitTime;

  log(QString("Template '%1' loaded. %2 symbols, %3 indicators, %4 params")
          .arg(m_template.name)
          .arg(m_template.symbols.size())
          .arg(m_template.indicators.size())
          .arg(m_template.params.size()));
}

bool TemplateStrategy::loadTemplate() {
  // Support both key formats:
  //   StrategyDeployDialog writes "__templateId__"
  //   CreateStrategyDialog writes "__template_id__"
  QString templateId =
      m_instance.parameters.value("__templateId__").toString();
  if (templateId.isEmpty()) {
    templateId =
        m_instance.parameters.value("__template_id__").toString();
  }
  if (templateId.isEmpty()) {
    log("ERROR: No template ID in parameters "
        "(tried __templateId__ and __template_id__)");
    return false;
  }

  // Load directly by ID using singleton repository
  StrategyTemplateRepository &repo = StrategyTemplateRepository::instance();
  bool ok = false;
  m_template = repo.loadTemplate(templateId, &ok);

  if (!ok) {
    log(QString("ERROR: Template '%1' not found in repository")
            .arg(templateId));
    return false;
  }

  m_templateLoaded = true;
  return true;
}

// ═══════════════════════════════════════════════════════════════════
// Setup: Symbol Bindings
// ═══════════════════════════════════════════════════════════════════

void TemplateStrategy::setupBindings() {
  // Support both formats:
  //   StrategyDeployDialog stores "__symbolBindings__" as QVariantList
  //   CreateStrategyDialog stores "__bindings__" as JSON string
  QVariant bindingsVar =
      m_instance.parameters.value("__symbolBindings__");
  if (!bindingsVar.isValid() || bindingsVar.isNull()) {
    bindingsVar = m_instance.parameters.value("__bindings__");
  }

  QVariantList bindingsList;

  if (bindingsVar.type() == QVariant::List) {
    // StrategyDeployDialog format: already a QVariantList
    bindingsList = bindingsVar.toList();
  } else if (bindingsVar.type() == QVariant::String) {
    // CreateStrategyDialog format: JSON string
    QString bindingsJson = bindingsVar.toString();
    if (bindingsJson.isEmpty()) {
      log("WARNING: No symbol bindings found");
      return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(bindingsJson.toUtf8());
    QJsonArray arr = doc.array();
    for (const auto &v : arr) {
      bindingsList.append(v.toObject().toVariantMap());
    }
  } else {
    log("WARNING: No symbol bindings found (unknown format)");
    return;
  }

  if (bindingsList.isEmpty()) {
    log("WARNING: Symbol bindings list is empty");
    return;
  }

  for (const QVariant &v : bindingsList) {
    QVariantMap obj = v.toMap();
    QString slotId = obj["symbolId"].toString();
    int segment = obj["segment"].toInt();
    uint32_t token = static_cast<uint32_t>(obj["token"].toLongLong());
    QString instrumentName = obj.value("instrumentName",
                                       obj.value("instrument")).toString();

    ResolvedSymbol rs;
    rs.segment = segment;
    rs.token = token;
    m_bindings[slotId] = rs;

    // Track instrument name for CandleAggregator routing
    if (!instrumentName.isEmpty()) {
      m_symbolNames[slotId] = instrumentName;
    }

    // Also bind in the formula context
    m_formulaContext.bindSymbol(slotId, segment, token);

    log(QString("  Bound %1 → segment=%2 token=%3 (%4)")
            .arg(slotId)
            .arg(segment)
            .arg(token)
            .arg(instrumentName));
  }
}

// ═══════════════════════════════════════════════════════════════════
// Setup: Indicator Engines
// ═══════════════════════════════════════════════════════════════════

void TemplateStrategy::setupIndicators() {
  // Group indicator definitions by symbol slot
  QHash<QString, QVector<IndicatorConfig>> configsBySymbol;

  for (const auto &indDef : m_template.indicators) {
    IndicatorConfig cfg;
    cfg.id = indDef.id;
    cfg.type = indDef.type;

    // Resolve period — could be a fixed value or a parameter reference
    QString periodStr = indDef.periodParam;
    if (periodStr.startsWith("{{") && periodStr.endsWith("}}")) {
      // Parameter reference: e.g. "{{RSI_PERIOD}}"
      QString paramName = periodStr.mid(2, periodStr.length() - 4);
      cfg.period = m_instance.parameters.value(paramName, 14).toInt();
    } else {
      cfg.period = periodStr.toInt();
    }

    // Same for period2
    QString p2 = indDef.period2Param;
    if (p2.startsWith("{{") && p2.endsWith("}}")) {
      QString paramName = p2.mid(2, p2.length() - 4);
      cfg.period2 = m_instance.parameters.value(paramName, 0).toInt();
    } else {
      cfg.period2 = p2.toInt();
    }

    cfg.priceField = indDef.priceField;

    configsBySymbol[indDef.symbolId].append(cfg);

    // Track timeframe per symbol slot for CandleAggregator subscription
    QString tf = indDef.timeframe.isEmpty() ? "1m" : indDef.timeframe;
    // Normalize timeframe strings:
    //   "D" or "d" → "1d", "W" → "1w"
    //   "5" → "5m", "15" → "15m", etc.
    if (tf == "D" || tf == "d") tf = "1d";
    else if (tf == "W" || tf == "w") tf = "1w";
    else if (!tf.endsWith("m") && !tf.endsWith("d") &&
             !tf.endsWith("h") && !tf.endsWith("w"))
      tf = tf + "m";
    m_slotTimeframes[indDef.symbolId] = tf;
  }

  // Create one IndicatorEngine per symbol slot
  for (auto it = configsBySymbol.begin(); it != configsBySymbol.end(); ++it) {
    auto *engine = new IndicatorEngine();
    engine->configure(it.value());
    m_indicators[it.key()] = engine;

    // Wire into formula context
    m_formulaContext.setIndicatorEngine(it.key(), engine);

    log(QString("  Indicators for %1: %2 configured (tf=%3)")
            .arg(it.key())
            .arg(it.value().size())
            .arg(m_slotTimeframes.value(it.key(), "1m")));
  }
}

// ═══════════════════════════════════════════════════════════════════
// Setup: Formula Engine
// ═══════════════════════════════════════════════════════════════════

void TemplateStrategy::setupFormulaEngine() {
  m_formulaEngine.setContext(&m_formulaContext);

  // ── Step 1: Load fixed (non-expression) parameter values ──
  for (auto it = m_instance.parameters.begin();
       it != m_instance.parameters.end(); ++it) {
    QString key = it.key();
    QVariant val = it.value();

    // Skip internal keys
    if (key.startsWith("__"))
      continue;

    // Check for legacy expression parameters: "__expr__:formula"
    // (from older deploy dialogs that used this encoding)
    if (val.type() == QVariant::String &&
        val.toString().startsWith("__expr__:")) {
      QString formula = val.toString().mid(9); // strip "__expr__:"
      m_expressionParams[key] = formula;
      m_expressionTriggers[key] = ParamTrigger::EveryTick; // legacy default
      log(QString("  Expression param '%1' = ƒ(%2) [trigger=EveryTick (legacy)]")
              .arg(key, formula));
      m_formulaEngine.setParam(key, 0.0);
      continue;
    }

    // Fixed value — set directly in formula engine
    bool ok = false;
    double numVal = val.toDouble(&ok);
    if (ok) {
      m_formulaEngine.setParam(key, numVal);
    }
  }

  // ── Step 2: Register Expression params from template definitions ──
  // These have proper trigger configuration from the template builder.
  for (const TemplateParam &p : m_template.params) {
    if (!p.isExpression() || p.expression.isEmpty())
      continue;

    // Check if the user overrode this formula with a fixed value at deploy time
    QVariant deployVal = m_instance.parameters.value(p.name);
    if (deployVal.isValid() && deployVal.type() != QVariant::String) {
      // User provided a numeric override — use as fixed value, skip formula
      bool ok = false;
      double numVal = deployVal.toDouble(&ok);
      if (ok) {
        m_formulaEngine.setParam(p.name, numVal);
        log(QString("  Param '%1' = %2 (user override, formula skipped)")
                .arg(p.name).arg(numVal));
        continue;
      }
    }
    // Check if deploy value is a string that looks like a formula override
    if (deployVal.isValid() && deployVal.type() == QVariant::String) {
      QString str = deployVal.toString().trimmed();
      // If it's a plain number string, treat as fixed override
      bool ok = false;
      double numVal = str.toDouble(&ok);
      if (ok) {
        m_formulaEngine.setParam(p.name, numVal);
        log(QString("  Param '%1' = %2 (user override, formula skipped)")
                .arg(p.name).arg(numVal));
        continue;
      }
      // If user typed a different formula at deploy time, use that instead
      if (!str.isEmpty() && str != p.expression) {
        m_expressionParams[p.name] = str;
        m_expressionTriggers[p.name] = p.trigger;
        m_expressionTimeframes[p.name] = p.triggerTimeframe;
        log(QString("  Expression param '%1' = ƒ(%2) [trigger=%3] (deploy override)")
                .arg(p.name, str, triggerToString(p.trigger)));
        m_formulaEngine.setParam(p.name, 0.0);
        continue;
      }
    }

    // Use the template's original formula + trigger
    m_expressionParams[p.name] = p.expression;
    m_expressionTriggers[p.name] = p.trigger;
    m_expressionTimeframes[p.name] = p.triggerTimeframe;
    m_formulaEngine.setParam(p.name, 0.0);

    log(QString("  Expression param '%1' = ƒ(%2) [trigger=%3]")
            .arg(p.name, p.expression, triggerToString(p.trigger)));
  }
}

// ═══════════════════════════════════════════════════════════════════
// START / STOP / PAUSE / RESUME
// ═══════════════════════════════════════════════════════════════════

void TemplateStrategy::start() {
  if (!m_templateLoaded) {
    log("Cannot start: template not loaded");
    return;
  }

  // Subscribe to FeedHandler for all bound symbols
  for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
    FeedHandler::instance().subscribe(it->segment,
                                      static_cast<int>(it->token), this,
                                      &TemplateStrategy::onTick);
    log(QString("  Subscribed to %1 (seg=%2, tok=%3)")
            .arg(it.key())
            .arg(it->segment)
            .arg(it->token));
  }

  // Subscribe to CandleAggregator for indicator candle feeds
  auto &agg = CandleAggregator::instance();
  for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
    QString slotId = it.key();
    QString tf = m_slotTimeframes.value(slotId, "1m");
    QString symName = m_symbolNames.value(slotId);
    if (!symName.isEmpty()) {
      agg.subscribeTo(symName, it->segment, {tf});
      log(QString("  CandleAggregator: %1 → %2 (tf=%3)")
              .arg(slotId, symName, tf));
    }
  }
  connect(&agg, &CandleAggregator::candleComplete,
          this, &TemplateStrategy::onCandleComplete);

  // Setup time-exit check if enabled
  if (m_timeExitEnabled && !m_exitTime.isEmpty()) {
    m_timeCheckTimer = new QTimer(this);
    m_timeCheckTimer->setInterval(5000); // check every 5 seconds
    connect(m_timeCheckTimer, &QTimer::timeout, this,
            &TemplateStrategy::checkTimeExit);
    m_timeCheckTimer->start();
  }

  m_isRunning = true;
  m_hasPosition = false;
  m_entrySignalFired = false;
  m_exitInProgress = false;
  m_dailyTradeCount = 0;
  m_dailyPnL = 0.0;
  m_prevOperandValues.clear();

  // ── Fire OnceAtStart expression params ──
  refreshExpressionParams(ParamTrigger::OnceAtStart);

  // ── Set up OnSchedule timers for params that need periodic recalculation ──
  for (auto it = m_expressionParams.begin(); it != m_expressionParams.end();
       ++it) {
    ParamTrigger pt = m_expressionTriggers.value(it.key());
    if (pt != ParamTrigger::OnSchedule)
      continue;

    // Find the interval from the template param definition
    int intervalSec = 300; // default 5 min
    for (const TemplateParam &p : m_template.params) {
      if (p.name == it.key()) {
        intervalSec = p.scheduleIntervalSec;
        break;
      }
    }

    auto *timer = new QTimer(this);
    timer->setInterval(intervalSec * 1000);
    QString paramName = it.key();
    QString formula = it.value();
    connect(timer, &QTimer::timeout, this, [this, paramName, formula]() {
      if (m_isRunning) {
        refreshSingleParam(paramName, formula);
      }
    });
    timer->start();
    m_scheduleTimers[it.key()] = timer;
    log(QString("  ⏲ Scheduled param '%1' every %2s").arg(it.key()).arg(intervalSec));
  }

  updateState(StrategyState::Running);
  log("Strategy STARTED");
}

void TemplateStrategy::stop() {
  if (!m_isRunning)
    return;

  // Unsubscribe from all feeds
  for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
    FeedHandler::instance().unsubscribe(it->segment,
                                        static_cast<int>(it->token), this);
  }

  // Disconnect from CandleAggregator
  disconnect(&CandleAggregator::instance(), &CandleAggregator::candleComplete,
             this, &TemplateStrategy::onCandleComplete);

  if (m_timeCheckTimer) {
    m_timeCheckTimer->stop();
  }

  m_isRunning = false;
  updateState(StrategyState::Stopped);
  log("Strategy STOPPED");
}

void TemplateStrategy::pause() {
  m_isRunning = false;
  updateState(StrategyState::Paused);
  log("Strategy PAUSED");
}

void TemplateStrategy::resume() {
  m_isRunning = true;
  updateState(StrategyState::Running);
  log("Strategy RESUMED");
}

// ═══════════════════════════════════════════════════════════════════
// ON TICK — The core runtime loop
// ═══════════════════════════════════════════════════════════════════

void TemplateStrategy::onTick(const UDP::MarketTick &tick) {
  if (!m_isRunning)
    return;

  Q_UNUSED(tick) // Price reads are via PriceStoreGateway snapshot;
                 // candle data is routed via onCandleComplete().

  // ── Step 1: Re-evaluate expression params with EveryTick trigger ──
  refreshExpressionParams(ParamTrigger::EveryTick);

  // ── Step 2: Check risk limits (with exit guard) ──
  if (m_hasPosition && !m_exitInProgress) {
    checkRiskLimits();
  }

  // ── Step 3: Evaluate entry condition ──
  if (!m_hasPosition && !m_entrySignalFired && !m_exitInProgress) {
    bool entryMet = evaluateCondition(m_template.entryCondition);
    if (entryMet) {
      log("✓ ENTRY condition met");
      m_entrySignalFired = true;
      placeEntryOrder();
    }
  }

  // ── Step 4: Evaluate exit condition (with exit guard) ──
  if (m_hasPosition && !m_exitInProgress) {
    bool exitMet = evaluateCondition(m_template.exitCondition);
    if (exitMet) {
      log("✓ EXIT condition met");
      m_exitInProgress = true;
      placeExitOrder();
    }
  }
}

// ═══════════════════════════════════════════════════════════════════
// Candle Routing — CandleAggregator → IndicatorEngine
// ═══════════════════════════════════════════════════════════════════

void TemplateStrategy::onCandleComplete(const QString &symbol, int segment,
                                        const QString &timeframe,
                                        const ChartData::Candle &candle) {
  if (!m_isRunning) return;

  // Find which slot this candle belongs to
  for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
    const QString &slotId = it.key();
    if (m_symbolNames.value(slotId) == symbol &&
        it->segment == segment &&
        m_slotTimeframes.value(slotId) == timeframe) {
      // Feed to the indicator engine for this slot
      auto indIt = m_indicators.find(slotId);
      if (indIt != m_indicators.end()) {
        (*indIt)->addCandle(candle);
      }
      break;
    }
  }

  // ── Trigger OnCandleClose expression params ──
  // Only evaluate params whose triggerTimeframe matches this candle's timeframe
  // (empty triggerTimeframe = matches any candle)
  for (auto it = m_expressionParams.begin(); it != m_expressionParams.end();
       ++it) {
    ParamTrigger pt = m_expressionTriggers.value(it.key());
    if (pt != ParamTrigger::OnCandleClose)
      continue;
    QString paramTf = m_expressionTimeframes.value(it.key());
    if (paramTf.isEmpty() || paramTf == timeframe) {
      refreshSingleParam(it.key(), it.value());
    }
  }
}

// ═══════════════════════════════════════════════════════════════════
// Crossover Helper — unique key for an operand
// ═══════════════════════════════════════════════════════════════════

QString TemplateStrategy::operandKey(const Operand &op) const {
  switch (op.type) {
  case Operand::Type::Price:     return QString("P_%1_%2").arg(op.symbolId, op.field);
  case Operand::Type::Indicator: return QString("I_%1_%2").arg(op.indicatorId, op.outputSeries);
  case Operand::Type::Constant:  return QString("C_%1").arg(op.constantValue);
  case Operand::Type::ParamRef:  return QString("R_%1").arg(op.paramName);
  case Operand::Type::Formula:   return QString("F_%1").arg(qHash(op.formulaExpression));
  case Operand::Type::Greek:     return QString("G_%1_%2").arg(op.symbolId, op.field);
  case Operand::Type::Spread:    return QString("S_%1").arg(op.symbolId);
  case Operand::Type::Total:     return QString("T_%1").arg(op.field);
  default:                       return QString("X_%1").arg(static_cast<int>(op.type));
  }
}

// ═══════════════════════════════════════════════════════════════════
// Expression Parameter Re-evaluation (trigger-based)
// ═══════════════════════════════════════════════════════════════════

// Helper: convert trigger enum to readable string for logs
static QString triggerToString(ParamTrigger t) {
  switch (t) {
  case ParamTrigger::EveryTick:     return "EveryTick";
  case ParamTrigger::OnCandleClose: return "OnCandleClose";
  case ParamTrigger::OnEntry:       return "OnEntry";
  case ParamTrigger::OnExit:        return "OnExit";
  case ParamTrigger::OnceAtStart:   return "OnceAtStart";
  case ParamTrigger::OnSchedule:    return "OnSchedule";
  case ParamTrigger::Manual:        return "Manual";
  }
  return "Unknown";
}

void TemplateStrategy::refreshSingleParam(const QString &name,
                                           const QString &formula) {
  bool ok = false;
  double val = m_formulaEngine.evaluate(formula, &ok);
  if (ok) {
    double prev = m_formulaEngine.param(name);
    m_formulaEngine.setParam(name, val);
    if (qAbs(val - prev) > 1e-9) {
      log(QString("  ⟳ Param '%1' = %2 (was %3)")
              .arg(name)
              .arg(val, 0, 'f', 4)
              .arg(prev, 0, 'f', 4));
    }
  } else {
    qWarning() << "[TemplateStrategy] Formula error for" << name << ":"
               << m_formulaEngine.lastError();
  }
}

void TemplateStrategy::refreshExpressionParams(ParamTrigger trigger) {
  for (auto it = m_expressionParams.begin(); it != m_expressionParams.end();
       ++it) {
    ParamTrigger paramTrigger = m_expressionTriggers.value(
        it.key(), ParamTrigger::EveryTick);

    // Only evaluate params whose trigger matches the current event
    if (paramTrigger != trigger)
      continue;

    refreshSingleParam(it.key(), it.value());
  }
}

// ═══════════════════════════════════════════════════════════════════
// Condition Evaluation (recursive)
// ═══════════════════════════════════════════════════════════════════

bool TemplateStrategy::evaluateCondition(const ConditionNode &node) {
  if (node.isEmpty())
    return false;

  if (node.isLeaf()) {
    double leftVal = resolveOperand(node.left);
    double rightVal = resolveOperand(node.right);

    if (node.op == ">")
      return leftVal > rightVal;
    if (node.op == ">=")
      return leftVal >= rightVal;
    if (node.op == "<")
      return leftVal < rightVal;
    if (node.op == "<=")
      return leftVal <= rightVal;
    if (node.op == "==")
      return qFuzzyCompare(leftVal, rightVal);
    if (node.op == "!=")
      return !qFuzzyCompare(leftVal, rightVal);

    // ── Crossover operators ──
    // Compare current vs previous tick values to detect line crossings
    if (node.op == "crosses_above" || node.op == "crosses_below") {
      QString kl = operandKey(node.left);
      QString kr = operandKey(node.right);
      double prevL = m_prevOperandValues.value(kl, leftVal);
      double prevR = m_prevOperandValues.value(kr, rightVal);
      m_prevOperandValues[kl] = leftVal;
      m_prevOperandValues[kr] = rightVal;

      if (node.op == "crosses_above")
        return (prevL <= prevR) && (leftVal > rightVal);
      else
        return (prevL >= prevR) && (leftVal < rightVal);
    }

    qWarning() << "[TemplateStrategy] Unknown operator:" << node.op;
    return false;
  }

  // Branch: AND or OR
  if (node.nodeType == ConditionNode::NodeType::And) {
    for (const auto &child : node.children) {
      if (!evaluateCondition(child))
        return false;
    }
    return !node.children.isEmpty();
  }

  if (node.nodeType == ConditionNode::NodeType::Or) {
    for (const auto &child : node.children) {
      if (evaluateCondition(child))
        return true;
    }
    return false;
  }

  return false;
}

  // ═══════════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════
// Operand Resolution
// ═══════════════════════════════════════════════════════════════════

double TemplateStrategy::resolveOperand(const Operand &op) const {
  switch (op.type) {

  case Operand::Type::Constant:
    return op.constantValue;

  case Operand::Type::ParamRef: {
    // Check if this is an expression parameter
    if (m_expressionParams.contains(op.paramName)) {
      // Already evaluated in refreshExpressionParams(), read the cached value
      return m_formulaEngine.param(op.paramName);
    }
    // Normal parameter: read from formula engine (set during init)
    return m_formulaEngine.param(op.paramName);
  }

  case Operand::Type::Formula: {
    // Direct formula operand: evaluate the expression in real-time
    if (op.formulaExpression.isEmpty())
      return 0.0;
    bool ok = false;
    double val = m_formulaEngine.evaluate(op.formulaExpression, &ok);
    if (!ok) {
      qWarning() << "[TemplateStrategy] Formula operand error:"
                 << m_formulaEngine.lastError()
                 << "expr:" << op.formulaExpression;
    }
    return val;
  }

  case Operand::Type::Price: {
    auto it = m_bindings.find(op.symbolId);
    if (it == m_bindings.end())
      return 0.0;
    auto state = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
        it->segment, it->token);

    if (op.field == "ltp")
      return state.ltp;
    if (op.field == "open")
      return state.open;
    if (op.field == "high")
      return state.high;
    if (op.field == "low")
      return state.low;
    if (op.field == "close")
      return state.close;
    return state.ltp; // default
  }

  case Operand::Type::Indicator: {
    auto it = m_indicators.find(op.symbolId);
    if (it == m_indicators.end())
      return 0.0;
    IndicatorEngine *eng = it.value();
    if (!eng)
      return 0.0;

    // indicator ID could be e.g. "RSI_1" from template,
    // or the computed form "RSI_14"
    QString indId = op.indicatorId;
    if (eng->isReady(indId)) {
      return eng->value(indId);
    }
    return 0.0;
  }

  case Operand::Type::Greek: {
    auto it = m_bindings.find(op.symbolId);
    if (it == m_bindings.end())
      return 0.0;
    auto state = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
        it->segment, it->token);

    if (op.field == "iv")
      return state.impliedVolatility;
    if (op.field == "delta")
      return state.delta;
    if (op.field == "gamma")
      return state.gamma;
    if (op.field == "theta")
      return state.theta;
    if (op.field == "vega")
      return state.vega;
    return 0.0;
  }

  case Operand::Type::Spread: {
    auto it = m_bindings.find(op.symbolId);
    if (it == m_bindings.end())
      return 0.0;
    auto state = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
        it->segment, it->token);
    // bid-ask spread
    return state.asks[0].price - state.bids[0].price;
  }

  case Operand::Type::Total: {
    // Support both "mtm" and "mtm_total" (ConditionBuilder uses "mtm_total")
    if (op.field == "mtm" || op.field == "mtm_total")
      return m_formulaContext.mtm();
    if (op.field == "net_premium")
      return m_formulaContext.netPremium();
    if (op.field == "net_delta")
      return m_formulaContext.netDelta();
    // Future: net_gamma, net_theta, net_vega, net_qty, open_trades
    return 0.0;
  }

  } // switch

  return 0.0;
}

// ═══════════════════════════════════════════════════════════════════
// Risk Management
// ═══════════════════════════════════════════════════════════════════

void TemplateStrategy::checkRiskLimits() {
  if (!m_hasPosition)
    return;

  // Daily trade limit
  if (m_dailyTradeCount >= m_template.riskDefaults.maxDailyTrades) {
    log("RISK: Max daily trades reached");
    return;
  }

  // Daily loss limit
  if (m_dailyPnL < -m_template.riskDefaults.maxDailyLossRs) {
    log(QString("RISK: Max daily loss reached (₹%.2f), stopping")
            .arg(m_dailyPnL));
    m_exitInProgress = true;
    placeExitOrder();
    stop();
    return;
  }

  // Stop-loss / Target check on first trade symbol
  for (const auto &sym : m_template.tradeSymbols()) {
    auto it = m_bindings.find(sym.id);
    if (it == m_bindings.end())
      continue;
    auto state = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
        it->segment, it->token);

    if (m_entryPrice > 0 && state.ltp > 0) {
      double pctMove = ((state.ltp - m_entryPrice) / m_entryPrice) * 100.0;

      // For SELL entries, invert the P&L direction
      if (sym.entrySide == SymbolDefinition::EntrySide::Sell) {
        pctMove = -pctMove;
      }

      // Stop-loss
      if (pctMove < -m_stopLossPct) {
        log(QString("RISK: Stop-loss triggered (%.2f%%)").arg(pctMove));
        m_exitInProgress = true;
        placeExitOrder();
        return;
      }

      // Target
      if (pctMove > m_targetPct) {
        log(QString("RISK: Target reached (%.2f%%)").arg(pctMove));
        m_exitInProgress = true;
        placeExitOrder();
        return;
      }
    }
    break; // only check first trade symbol for now
  }
}

void TemplateStrategy::checkTimeExit() {
  if (!m_hasPosition || !m_timeExitEnabled)
    return;

  QTime now = QTime::currentTime();
  QTime exitT = QTime::fromString(m_exitTime, "HH:mm");

  if (exitT.isValid() && now >= exitT) {
    log(QString("RISK: Time exit triggered at %1")
            .arg(now.toString("HH:mm:ss")));
    m_exitInProgress = true;
    placeExitOrder();
  }
}

// ═══════════════════════════════════════════════════════════════════
// Order Management (emit signals for StrategyService to handle)
// ═══════════════════════════════════════════════════════════════════

void TemplateStrategy::placeEntryOrder() {
  // Place orders on all TRADE symbols
  for (const auto &sym : m_template.tradeSymbols()) {
    auto it = m_bindings.find(sym.id);
    if (it == m_bindings.end())
      continue;

    XTS::OrderParams params;
    params.exchangeSegment = it->segment;
    params.exchangeInstrumentID = static_cast<int>(it->token);
    // Determine order side from template definition
    params.orderSide = (sym.entrySide == SymbolDefinition::EntrySide::Sell)
                           ? "SELL" : "BUY";
    params.orderType = "MARKET";
    params.orderQuantity = m_instance.quantity;

    log(QString("ENTRY ORDER: %1 x %2 (seg=%3, tok=%4)")
            .arg(params.orderSide)
            .arg(params.orderQuantity)
            .arg(params.exchangeSegment)
            .arg(params.exchangeInstrumentID));

    emit orderRequested(params);

    m_hasPosition = true;
    m_dailyTradeCount++;

    // Record entry price
    auto state = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
        it->segment, it->token);
    m_entryPrice = state.ltp;

    // Track PnL from this entry
    break; // one entry per signal for now
  }
}

void TemplateStrategy::placeExitOrder() {
  for (const auto &sym : m_template.tradeSymbols()) {
    auto it = m_bindings.find(sym.id);
    if (it == m_bindings.end())
      continue;

    // Exit side is opposite of entry side
    XTS::OrderParams params;
    params.exchangeSegment = it->segment;
    params.exchangeInstrumentID = static_cast<int>(it->token);
    params.orderSide = (sym.entrySide == SymbolDefinition::EntrySide::Sell)
                           ? "BUY" : "SELL";
    params.orderType = "MARKET";
    params.orderQuantity = m_instance.quantity;

    log(QString("EXIT ORDER: %1 x %2 (seg=%3, tok=%4)")
            .arg(params.orderSide)
            .arg(params.orderQuantity)
            .arg(params.exchangeSegment)
            .arg(params.exchangeInstrumentID));

    // Track daily PnL from exit
    auto state = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
        it->segment, it->token);
    if (m_entryPrice > 0 && state.ltp > 0) {
      double pnl = 0.0;
      if (sym.entrySide == SymbolDefinition::EntrySide::Buy) {
        pnl = (state.ltp - m_entryPrice) * params.orderQuantity;
      } else {
        pnl = (m_entryPrice - state.ltp) * params.orderQuantity;
      }
      m_dailyPnL += pnl;
      log(QString("  PnL: ₹%.2f (daily total: ₹%.2f)").arg(pnl).arg(m_dailyPnL));
    }

    emit orderRequested(params);
    break;
  }

  m_hasPosition = false;
  m_entrySignalFired = false;
  m_exitInProgress = false;
  m_entryPrice = 0.0;
}
