#ifndef STRATEGY_TEMPLATE_H
#define STRATEGY_TEMPLATE_H

#include "strategy/ConditionNode.h"
#include "strategy/IndicatorEngine.h"
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

// ═══════════════════════════════════════════════════════════════════
// STRATEGY MODE
// What kind of strategy this template represents.
// ═══════════════════════════════════════════════════════════════════

enum class StrategyMode {
    IndicatorBased,  // Entry/exit driven by technical indicators
    OptionMultiLeg,  // Multi-leg options strategy (straddle, strangle, etc.)
    Spread,          // Futures/equity spread / Badla
    // Future: Arbitrage, PairsTrade, etc.
};

// ═══════════════════════════════════════════════════════════════════
// SYMBOL DEFINITION
// Declares one symbol slot in the template.
// The actual instrument is bound at deploy time (StrategyInstance).
// ═══════════════════════════════════════════════════════════════════

enum class SymbolRole {
    Reference,  // Price/indicator of this symbol drives conditions
                // (no orders placed on this symbol)
    Trade       // Orders are placed on this symbol
};

// Exchange segment — which market this symbol slot belongs to.
// Matches the segment codes used throughout the app (SymbolBinding::segment).
enum class SymbolSegment {
    NSE_CM  = 0,  // NSE Cash / Equity
    NSE_FO  = 1,  // NSE Futures & Options
    BSE_CM  = 2,  // BSE Cash / Equity
    BSE_FO  = 3,  // BSE Futures & Options
};

// Backward-compat alias so existing code using TradeSymbolType still compiles
using TradeSymbolType = SymbolSegment;

struct SymbolDefinition {
    QString id;              // Template-scoped id: "REF_1", "TRADE_1"
    QString label;           // Human label: "Reference Index", "Trade Instrument"
    SymbolRole role = SymbolRole::Reference;

    SymbolSegment segment = SymbolSegment::NSE_CM;

    // Entry side for TRADE symbols
    enum class EntrySide { Buy, Sell };
    EntrySide entrySide = EntrySide::Buy;

    // Convenience helpers
    bool isFO()     const { return segment == SymbolSegment::NSE_FO || segment == SymbolSegment::BSE_FO; }
    bool isOption() const { return isFO(); } // kept for backward compat

    // Backward-compat accessor
    TradeSymbolType tradeType;
};

// ═══════════════════════════════════════════════════════════════════
// INDICATOR DEFINITION
// Declares one indicator slot in the template.
// Period/params can be a fixed value or a named parameter placeholder.
// ═══════════════════════════════════════════════════════════════════

struct IndicatorDefinition {
    QString id;              // Template-scoped id: "RSI_MAIN", "SMA_FAST"
    QString type;            // "RSI", "SMA", "EMA", "MACD", "BBANDS", "ATR", etc.
    QString symbolId;        // Which symbol's candle data to compute on

    // ── Candle timeframe ─────────────────────────────────────────────────────
    // Which candle series to compute on. Matches CandleAggregator interval keys.
    // e.g. "1", "3", "5", "15", "30", "60", "D", "W"
    // Empty / "D" → daily (default for most strategies)
    QString timeframe = "D";

    // ── Parameters ───────────────────────────────────────────────────────────
    // Each can be a fixed value OR a named parameter placeholder "{{NAME}}"
    // At deploy time any "{{...}}" token is resolved from instance paramValues.
    QString periodParam;     // param1 — e.g. "14" or "{{RSI_PERIOD}}"
    QString period2Param;    // param2 — e.g. "26" or "{{MACD_SLOW}}"
    QString param3Str;       // param3 — e.g. "9"  or "{{MACD_SIGNAL}}"
    double  param3     = 0.0; // numeric convenience copy of param3Str

    QString priceField = "close"; // OHLCV input field

    // ── Param labels (auto-filled from IndicatorMeta at UI time) ─────────────
    // Stored so that when viewing a saved template the labels are always correct
    // even if indicator_defaults.json changes.
    QString param1Label;   // e.g. "Time Period", "Fast Period"
    QString param2Label;   // e.g. "Slow Period", "Signal Period"
    QString param3Label;   // e.g. "Std Dev Down (×)", "Slow K Period"

    // ── Output selector ───────────────────────────────────────────────────────
    // For indicators that produce multiple output series (e.g. BBANDS, MACD,
    // STOCH) this picks which series is used in conditions.
    // Empty string → use the first (and usually only) output series.
    QString outputSelector;  // e.g. "upperBand", "macd", "signal", "hist"

    // legacy numeric holder kept for backward-compat with old JSON
    double  param1 = 0.0;
};

// ═══════════════════════════════════════════════════════════════════
// TEMPLATE PARAMETER DECLARATION
// A named parameter that the user fills in at deploy time.
// ═══════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════
// PARAMETER VALUE TYPES
// ═══════════════════════════════════════════════════════════════════

enum class ParamValueType {
    Int,
    Double,
    Bool,
    String,
    Expression   // Formula-based: evaluated at runtime from other params/indicators
};

// ═══════════════════════════════════════════════════════════════════
// RECALCULATION TRIGGER
// Determines WHEN an Expression parameter is re-evaluated at runtime.
// ═══════════════════════════════════════════════════════════════════

enum class ParamTrigger {
    EveryTick,        // Recalculate on every market tick (high-frequency)
                      // Use case: dynamic SL/TP that follows price in real-time
                      //   e.g. "LTP(TRADE_1) - ATR(REF_1, 14) * 2"

    OnCandleClose,    // Recalculate only when a new candle closes
                      // Use case: indicator-based values that change per candle
                      //   e.g. "ATR(REF_1, 14) * 2.5"  (ATR only changes on candle close)
                      //   e.g. "RSI(REF_1, 14) * 0.5 + 15"

    OnEntry,          // Calculate once when an entry order is placed
                      // Use case: entry-price-relative values, freeze at order time
                      //   e.g. "LTP(TRADE_1) * 0.98"  (SL = 2% below entry)
                      //   e.g. "IV(TRADE_1)"  (snapshot IV at entry for comparison)

    OnExit,           // Calculate once when an exit order is placed
                      // Use case: values needed at exit decision time
                      //   e.g. "MTM() / NET_PREMIUM()"

    OnceAtStart,      // Calculate once when the strategy starts running
                      // Use case: session constants, VWAP anchor, opening range
                      //   e.g. "HIGH(REF_1)"  (day's high at strategy start)
                      //   e.g. "ATR(REF_1, 14) * 3"  (fixed SL width for the session)

    OnSchedule,       // Recalculate on a fixed time interval (e.g. every 5 min)
                      // Use case: periodic checks that don't need tick-level granularity
                      //   e.g. IV surface recalculation every 5 minutes

    Manual,           // Never auto-recalculate — value is set at deploy time and frozen
                      // This is the default for Int/Double/Bool/String params
};

// ═══════════════════════════════════════════════════════════════════
// TEMPLATE PARAMETER DECLARATION
// A named parameter that the user fills in at deploy time.
//
// Parameters can be:
//   FIXED   — user enters a value at deploy time (Int/Double/Bool/String)
//   DYNAMIC — formula evaluated at runtime with a configurable trigger
//
// Every Expression param has:
//   1. A FORMULA  — the expression to evaluate (e.g. "ATR(REF_1,14)*2.5")
//   2. A TRIGGER  — WHEN to evaluate it (OnCandleClose, EveryTick, OnEntry, ...)
//   3. An optional SCHEDULE INTERVAL — for OnSchedule trigger (seconds)
//   4. An optional CANDLE TIMEFRAME  — for OnCandleClose trigger (which TF?)
//
// At deploy time, the user can:
//   a) Keep the formula + trigger as designed
//   b) Override with a fixed value (making it effectively a Double param)
//
// At runtime, TemplateStrategy uses the trigger to decide when to call
// FormulaEngine::evaluate() and update the param value.
// ═══════════════════════════════════════════════════════════════════

struct TemplateParam {
    QString     name;           // e.g. "RSI_PERIOD", "IV_THRESHOLD", "SL_LEVEL"
    QString     label;          // Human label shown in deploy dialog
    ParamValueType valueType = ParamValueType::Double;
    QVariant    defaultValue;
    QVariant    minValue;       // optional min (for Int/Double)
    QVariant    maxValue;       // optional max (for Int/Double)
    QString     description;

    // ── Expression support ──────────────────────────────────────────────
    // When valueType == Expression, this holds the formula string.
    // Syntax: arithmetic over other param names, indicator refs, constants.
    //   e.g. "ATR(REF_1, 14) * 2.5"
    //        "VWAP(REF_1) * (1 + OFFSET_PCT / 100)"
    //        "IV(TRADE_1) > 25 ? LTP(TRADE_1) * 0.02 : LTP(TRADE_1) * 0.01"
    //
    // At deploy time the user can override with a fixed value, or keep the
    // expression for runtime evaluation.
    QString     expression;

    // ── Recalculation trigger ───────────────────────────────────────────
    // Determines WHEN the expression is re-evaluated during live trading.
    // Only meaningful when valueType == Expression.
    // Default: OnCandleClose (most expressions depend on indicators that
    //          update per candle, not per tick).
    ParamTrigger trigger = ParamTrigger::OnCandleClose;

    // ── Schedule interval (seconds) ─────────────────────────────────────
    // Only used when trigger == OnSchedule.
    //   e.g. 300 = recalculate every 5 minutes
    int scheduleIntervalSec = 300;

    // ── Candle timeframe for OnCandleClose trigger ──────────────────────
    // Which candle series triggers recalculation.
    // Empty = use the strategy's default timeframe.
    //   e.g. "5m", "15m", "1h", "1d"
    QString triggerTimeframe;

    // If true, user cannot change this param at deploy time
    bool        locked = false;

    // ── Helpers ──
    bool isExpression() const { return valueType == ParamValueType::Expression; }
    bool isDynamic() const { return isExpression() && trigger != ParamTrigger::Manual; }
};

// ═══════════════════════════════════════════════════════════════════
// RISK DEFAULTS
// Default risk parameters embedded in the template.
// All values can be overridden at deploy time.
// ═══════════════════════════════════════════════════════════════════

struct RiskDefaults {
    // Stop-loss
    double stopLossPercent  = 1.0;  // % of entry price
    bool   stopLossLocked   = false; // if true, cannot be changed at deploy

    // Target
    double targetPercent    = 2.0;
    bool   targetLocked     = false;

    // Trailing stop
    bool   trailingEnabled       = false;
    double trailingTriggerPct    = 1.0;  // activate after X% profit
    double trailingAmountPct     = 0.5;  // trail by X%

    // Time-based exit
    bool    timeExitEnabled = false;
    QString exitTime        = "15:15"; // HH:mm

    // Daily limits
    int    maxDailyTrades   = 10;
    double maxDailyLossRs   = 5000.0;
};

// ═══════════════════════════════════════════════════════════════════
// STRATEGY TEMPLATE
// The reusable blueprint. Symbol-agnostic and parameter-agnostic.
// One template → many deployed instances.
// ═══════════════════════════════════════════════════════════════════

struct StrategyTemplate {
    // ── Identity ──
    QString     templateId;   // UUID generated on save
    QString     name;
    QString     description;
    QString     version = "1.0";
    QDateTime   createdAt;
    QDateTime   updatedAt;

    // ── Mode ──
    StrategyMode mode = StrategyMode::IndicatorBased;

    // ── Flags ──
    bool usesTimeTrigger       = false;  // has time-window conditions
    bool predominantlyOptions  = false;  // affects UI hints

    // ── Symbol slots ──
    // Order matters: REF symbols first, then TRADE symbols
    QVector<SymbolDefinition> symbols;

    // ── Indicators ──
    // Each indicator references one symbol slot by symbolId
    QVector<IndicatorDefinition> indicators;

    // ── User-configurable parameters ──
    QVector<TemplateParam> params;

    // ── Conditions ──
    // Entry: single ConditionNode tree (AND/OR of any depth)
    ConditionNode entryCondition;

    // Exit: single ConditionNode tree
    ConditionNode exitCondition;

    // ── Risk defaults ──
    RiskDefaults riskDefaults;

    // ── Helpers ──
    bool isValid() const {
        return !name.isEmpty() && !symbols.isEmpty();
    }

    QVector<SymbolDefinition> referenceSymbols() const {
        QVector<SymbolDefinition> result;
        for (const auto &s : symbols)
            if (s.role == SymbolRole::Reference) result.append(s);
        return result;
    }

    QVector<SymbolDefinition> tradeSymbols() const {
        QVector<SymbolDefinition> result;
        for (const auto &s : symbols)
            if (s.role == SymbolRole::Trade) result.append(s);
        return result;
    }

    QString modeString() const {
        switch (mode) {
        case StrategyMode::IndicatorBased: return "indicator";
        case StrategyMode::OptionMultiLeg: return "option_multileg";
        case StrategyMode::Spread:         return "spread";
        }
        return "indicator";
    }

    static StrategyMode modeFromString(const QString &s) {
        if (s == "option_multileg") return StrategyMode::OptionMultiLeg;
        if (s == "spread")          return StrategyMode::Spread;
        return StrategyMode::IndicatorBased;
    }
};

// ═══════════════════════════════════════════════════════════════════
// SYMBOL BINDING
// At deploy time, each template symbol slot is bound to a real
// instrument token from the master file.
// ═══════════════════════════════════════════════════════════════════

struct SymbolBinding {
    QString symbolId;        // matches SymbolDefinition::id in template
    QString instrumentName;  // e.g. "NIFTY", "RELIANCE"
    int64_t token       = 0; // exchange instrument token
    int     segment     = 2; // 1=NSECM 2=NSEFO 11=BSECM 12=BSEFO
    int     lotSize     = 1;
    int     quantity    = 0; // user-specified qty at deploy time

    // Option-specific (populated at runtime when tradeType == Option)
    // Strike selection is resolved at runtime, NOT stored in the template
    QString expiryDate;      // e.g. "27FEB26"
    QString strikeSelMode;   // "atm_relative" | "premium_based" | "fixed"
    int     atmOffset   = 0; // for atm_relative
    double  targetPremium = 0.0; // for premium_based
    int     fixedStrike = 0;    // for fixed
};

#endif // STRATEGY_TEMPLATE_H
