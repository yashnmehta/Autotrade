#ifndef LIVE_FORMULA_CONTEXT_H
#define LIVE_FORMULA_CONTEXT_H

/**
 * @file LiveFormulaContext.h
 * @brief Concrete FormulaContext that resolves live market data from
 *        PriceStoreGateway and indicator values from IndicatorEngine.
 *
 * This is the bridge between the FormulaEngine (pure math evaluator)
 * and the actual running market infrastructure.
 *
 * ═══════════════════════════════════════════════════════════════════
 * SYMBOL RESOLUTION
 * ═══════════════════════════════════════════════════════════════════
 *
 * The FormulaEngine uses template-scoped symbol IDs like "REF_1", "TRADE_1".
 * This context maps them to real exchange (segment, token) pairs using the
 * SymbolBinding table from the deployed strategy instance.
 *
 * Example:
 *   "REF_1"   →  segment=2 (NSEFO), token=26000  (NIFTY 50)
 *   "TRADE_1" →  segment=2 (NSEFO), token=49508  (NIFTY FEB FUT)
 *
 * ═══════════════════════════════════════════════════════════════════
 * USAGE
 * ═══════════════════════════════════════════════════════════════════
 *
 *   LiveFormulaContext ctx;
 *   ctx.bindSymbol("REF_1",   2, 26000);
 *   ctx.bindSymbol("TRADE_1", 2, 49508);
 *   ctx.setIndicatorEngine("REF_1", &refIndicatorEngine);
 *
 *   FormulaEngine engine;
 *   engine.setContext(&ctx);
 *   double val = engine.evaluate("LTP(REF_1) * 1.01", &ok);
 */

#include "strategy/FormulaEngine.h"  // FormulaContext base
#include <QHash>
#include <QString>

class IndicatorEngine;

// ═══════════════════════════════════════════════════════════════════
// Symbol resolution: maps template slot ID → real exchange identity
// ═══════════════════════════════════════════════════════════════════

struct ResolvedSymbol {
    int      segment = 0;     // 1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO
    uint32_t token   = 0;     // Exchange instrument token
};

// ═══════════════════════════════════════════════════════════════════
// LiveFormulaContext
// ═══════════════════════════════════════════════════════════════════

class LiveFormulaContext : public FormulaContext {
public:
    LiveFormulaContext() = default;
    ~LiveFormulaContext() override = default;

    // ── Symbol binding ──────────────────────────────────────────────
    // Call during strategy init to map template slot IDs to real tokens.
    void bindSymbol(const QString &symbolId, int segment, uint32_t token);
    void clearBindings();
    bool hasSymbol(const QString &symbolId) const;

    // ── Indicator engine binding ─────────────────────────────────────
    // Each symbol slot can have its own IndicatorEngine computing candle
    // indicators. Set during strategy init.
    void setIndicatorEngine(const QString &symbolId, IndicatorEngine *engine);

    // ── Portfolio-level data (set by TemplateStrategy on each tick) ──
    void setMtm(double v)         { m_mtm = v; }
    void setNetPremium(double v)  { m_netPremium = v; }
    void setNetDelta(double v)    { m_netDelta = v; }

    // ═══════════════════════════════════════════════════════════════════
    // FormulaContext interface implementation
    // ═══════════════════════════════════════════════════════════════════

    double ltp(const QString &symbolId) const override;
    double open(const QString &symbolId) const override;
    double high(const QString &symbolId) const override;
    double low(const QString &symbolId) const override;
    double close(const QString &symbolId) const override;
    double volume(const QString &symbolId) const override;
    double bid(const QString &symbolId) const override;
    double ask(const QString &symbolId) const override;
    double changePct(const QString &symbolId) const override;

    double indicator(const QString &symbolId,
                     const QString &indicatorType,
                     int period,
                     int period2 = 0,
                     int period3 = 0) const override;

    double iv(const QString &symbolId) const override;
    double delta(const QString &symbolId) const override;
    double gamma(const QString &symbolId) const override;
    double theta(const QString &symbolId) const override;
    double vega(const QString &symbolId) const override;

    double mtm() const override        { return m_mtm; }
    double netPremium() const override { return m_netPremium; }
    double netDelta() const override   { return m_netDelta; }

private:
    // Template slot ID → real exchange identity
    QHash<QString, ResolvedSymbol> m_symbols;

    // Template slot ID → IndicatorEngine for that symbol's candle data
    QHash<QString, IndicatorEngine*> m_indicatorEngines;

    // Portfolio-level values (updated externally before each evaluation)
    double m_mtm        = 0.0;
    double m_netPremium = 0.0;
    double m_netDelta   = 0.0;
};

#endif // LIVE_FORMULA_CONTEXT_H
