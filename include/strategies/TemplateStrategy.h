#ifndef TEMPLATE_STRATEGY_H
#define TEMPLATE_STRATEGY_H

/**
 * @file TemplateStrategy.h
 * @brief Generic strategy runtime that executes any user-defined StrategyTemplate.
 *
 * ═══════════════════════════════════════════════════════════════════
 * HOW IT WORKS
 * ═══════════════════════════════════════════════════════════════════
 *
 * 1. INIT: Receives a StrategyInstance whose `parameters` QVariantMap contains:
 *    - "__templateId__" (or legacy "__template_id__") → UUID of the StrategyTemplate
 *    - "__symbolBindings__" (or legacy "__bindings__") → Symbol bindings (QVariantList or JSON string)
 *    - Regular param values → "RSI_PERIOD" = 14, "OFFSET_PCT" = 0.5
 *    - Expression formulas  → "SL_LEVEL" = "__expr__:ATR(REF_1,14)*2.5"
 *
 * 2. START: Subscribes to FeedHandler for all bound symbol tokens.
 *           Sets up IndicatorEngine per symbol. Initializes FormulaEngine
 *           with LiveFormulaContext.
 *
 * 3. ON TICK: For every price update:
 *    a) Feed candle aggregator → IndicatorEngine
 *    b) Re-evaluate all Expression params → updates FormulaEngine::m_params
 *    c) Evaluate entry condition tree → if true, place order
 *    d) Evaluate exit condition tree → if true, close position
 *    e) Check risk limits (SL, target, time exit)
 *
 * 4. STOP: Unsubscribe, clean up.
 *
 * ═══════════════════════════════════════════════════════════════════
 * CONDITION EVALUATION
 * ═══════════════════════════════════════════════════════════════════
 *
 * Each ConditionNode leaf has two Operands (left, right) and an operator.
 * Operand resolution:
 *
 *   Price     → PriceStoreGateway::getUnifiedSnapshot(seg, token).{ltp|high|...}
 *   Indicator → IndicatorEngine::value("RSI_14")
 *   Constant  → operand.constantValue
 *   ParamRef  → FormulaEngine.param(name) OR evaluate(__expr__:formula)
 *   Greek     → UnifiedState.{iv|delta|gamma|theta|vega}
 *   Spread    → best_ask - best_bid (computed)
 *   Total     → LiveFormulaContext.{mtm|netPremium|netDelta}
 *
 * Branch nodes (And/Or) recurse into children.
 */

#include "strategies/LiveFormulaContext.h"
#include "strategies/StrategyBase.h"
#include "strategy/ConditionNode.h"
#include "strategy/FormulaEngine.h"
#include "strategy/IndicatorEngine.h"
#include "strategy/StrategyTemplate.h"
#include "data/CandleData.h"
#include <QHash>
#include <QTimer>

// Forward declaration
namespace MarketData { struct UnifiedState; }

class TemplateStrategy : public StrategyBase {
    Q_OBJECT

public:
    explicit TemplateStrategy(QObject *parent = nullptr);
    ~TemplateStrategy() override;

    // ── StrategyBase overrides ──
    void init(const StrategyInstance &instance) override;
    void start() override;
    void stop() override;
    void pause() override;
    void resume() override;

protected slots:
    void onTick(const UDP::MarketTick &tick) override;

private slots:
    // ── Candle routing from CandleAggregator ──
    void onCandleComplete(const QString &symbol, int segment,
                          const QString &timeframe, const ChartData::Candle &candle);

private:
    // ── Initialization helpers ──
    bool loadTemplate();
    void setupBindings();
    void setupIndicators();
    void setupFormulaEngine();

    // ── Condition evaluation ──
    bool evaluateCondition(const ConditionNode &node);
    double resolveOperand(const Operand &op) const;

    // ── Expression parameter re-evaluation ──
    // Evaluates expression parameters based on their configured trigger.
    // Each expression param has a ParamTrigger that determines WHEN it
    // should be recalculated:
    //   EveryTick     → called from onTick()
    //   OnCandleClose → called from onCandleComplete()
    //   OnEntry       → called from placeEntryOrder()
    //   OnExit        → called from placeExitOrder()
    //   OnceAtStart   → called from start()
    //   OnSchedule    → called from a QTimer at fixed intervals
    //   Manual        → never auto-recalculated
    void refreshExpressionParams(ParamTrigger trigger);
    void refreshSingleParam(const QString &name, const QString &formula);

    // ── Risk management ──
    void checkRiskLimits();
    void checkTimeExit();

    // ── Order management ──
    void placeEntryOrder();
    void placeExitOrder();

    // ── State ──
    StrategyTemplate m_template;
    bool m_templateLoaded = false;

    // Symbol bindings: slotId → (segment, token)
    QHash<QString, ResolvedSymbol> m_bindings;

    // Candle routing: slotId → symbol name for CandleAggregator lookup
    QHash<QString, QString> m_symbolNames;
    // Candle routing: slotId → timeframe string (e.g. "1m", "5m", "1d")
    QHash<QString, QString> m_slotTimeframes;

    // Indicator engines: one per symbol slot
    QHash<QString, IndicatorEngine*> m_indicators;

    // Formula evaluation
    FormulaEngine       m_formulaEngine;
    LiveFormulaContext   m_formulaContext;

    // Expression parameters: paramName → formula string
    QHash<QString, QString> m_expressionParams;

    // Expression trigger map: paramName → ParamTrigger
    QHash<QString, ParamTrigger> m_expressionTriggers;

    // Expression param → candle timeframe (for OnCandleClose trigger)
    QHash<QString, QString> m_expressionTimeframes;

    // Schedule timers: paramName → QTimer* (for OnSchedule trigger)
    QHash<QString, QTimer*> m_scheduleTimers;

    // Crossover detection: operand key → previous tick value
    QHash<QString, double> m_prevOperandValues;
    QString operandKey(const Operand &op) const;

    // Position tracking
    bool   m_hasPosition = false;
    bool   m_entrySignalFired = false;
    bool   m_exitInProgress = false;  // guard against double-exit in same tick
    double m_entryPrice = 0.0;
    int    m_dailyTradeCount = 0;
    double m_dailyPnL = 0.0;

    // Risk settings (resolved from template + user overrides)
    double m_stopLossPct = 1.0;
    double m_targetPct   = 2.0;
    bool   m_trailingEnabled = false;
    double m_trailingTriggerPct = 1.0;
    double m_trailingAmountPct  = 0.5;
    bool   m_timeExitEnabled = false;
    QString m_exitTime;

    // Time-based check timer
    QTimer *m_timeCheckTimer = nullptr;
};

#endif // TEMPLATE_STRATEGY_H
