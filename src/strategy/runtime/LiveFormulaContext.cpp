/**
 * @file LiveFormulaContext.cpp
 * @brief Resolves live market data for FormulaEngine evaluation.
 *
 * Reads from PriceStoreGateway (zero-copy price store) and IndicatorEngine.
 */

#include "strategy/runtime/LiveFormulaContext.h"
#include "data/PriceStoreGateway.h"
#include "strategy/runtime/IndicatorEngine.h"
#include <QDebug>

// ═══════════════════════════════════════════════════════════════════
// Symbol binding
// ═══════════════════════════════════════════════════════════════════

void LiveFormulaContext::bindSymbol(const QString &symbolId, int segment, uint32_t token) {
    ResolvedSymbol rs;
    rs.segment = segment;
    rs.token   = token;
    m_symbols[symbolId.toUpper()] = rs;
}

void LiveFormulaContext::clearBindings() {
    m_symbols.clear();
    m_indicatorEngines.clear();
}

bool LiveFormulaContext::hasSymbol(const QString &symbolId) const {
    return m_symbols.contains(symbolId.toUpper());
}

void LiveFormulaContext::setIndicatorEngine(const QString &symbolId, IndicatorEngine *engine) {
    m_indicatorEngines[symbolId.toUpper()] = engine;
}

// ═══════════════════════════════════════════════════════════════════
// Helper: fetch UnifiedState snapshot for a symbol slot
// ═══════════════════════════════════════════════════════════════════

static MarketData::UnifiedState fetchState(const QHash<QString, ResolvedSymbol> &symbols,
                                            const QString &symbolId) {
    auto it = symbols.find(symbolId.toUpper());
    if (it == symbols.end()) {
        qWarning() << "[LiveFormulaContext] Unknown symbol slot:" << symbolId;
        return MarketData::UnifiedState();
    }
    return MarketData::PriceStoreGateway::instance()
        .getUnifiedSnapshot(it->segment, it->token);
}

// ═══════════════════════════════════════════════════════════════════
// Market data methods
// ═══════════════════════════════════════════════════════════════════

double LiveFormulaContext::ltp(const QString &symbolId) const {
    return fetchState(m_symbols, symbolId).ltp;
}

double LiveFormulaContext::open(const QString &symbolId) const {
    return fetchState(m_symbols, symbolId).open;
}

double LiveFormulaContext::high(const QString &symbolId) const {
    return fetchState(m_symbols, symbolId).high;
}

double LiveFormulaContext::low(const QString &symbolId) const {
    return fetchState(m_symbols, symbolId).low;
}

double LiveFormulaContext::close(const QString &symbolId) const {
    return fetchState(m_symbols, symbolId).close;
}

double LiveFormulaContext::volume(const QString &symbolId) const {
    return static_cast<double>(fetchState(m_symbols, symbolId).volume);
}

double LiveFormulaContext::bid(const QString &symbolId) const {
    auto state = fetchState(m_symbols, symbolId);
    return state.bids[0].price;  // Best bid
}

double LiveFormulaContext::ask(const QString &symbolId) const {
    auto state = fetchState(m_symbols, symbolId);
    return state.asks[0].price;  // Best ask
}

double LiveFormulaContext::changePct(const QString &symbolId) const {
    return fetchState(m_symbols, symbolId).percentChange;
}

// ═══════════════════════════════════════════════════════════════════
// Indicator access
// ═══════════════════════════════════════════════════════════════════

double LiveFormulaContext::indicator(const QString &symbolId,
                                      const QString &indicatorType,
                                      int period,
                                      int period2,
                                      int period3) const {
    Q_UNUSED(period2)
    Q_UNUSED(period3)

    auto it = m_indicatorEngines.find(symbolId.toUpper());
    if (it == m_indicatorEngines.end() || !*it) {
        qWarning() << "[LiveFormulaContext] No IndicatorEngine for symbol:" << symbolId;
        return 0.0;
    }

    // Build indicator ID matching IndicatorEngine convention: TYPE_PERIOD
    // e.g. "RSI_14", "SMA_20", "EMA_50"
    QString id = QString("%1_%2").arg(indicatorType.toUpper()).arg(period);
    IndicatorEngine *eng = *it;

    if (!eng->isReady(id)) {
        return 0.0;  // Insufficient candle data
    }
    return eng->value(id);
}

// ═══════════════════════════════════════════════════════════════════
// Greeks (from UnifiedState — populated by GreeksCalculationService)
// ═══════════════════════════════════════════════════════════════════

double LiveFormulaContext::iv(const QString &symbolId) const {
    return fetchState(m_symbols, symbolId).impliedVolatility;
}

double LiveFormulaContext::delta(const QString &symbolId) const {
    return fetchState(m_symbols, symbolId).delta;
}

double LiveFormulaContext::gamma(const QString &symbolId) const {
    return fetchState(m_symbols, symbolId).gamma;
}

double LiveFormulaContext::theta(const QString &symbolId) const {
    return fetchState(m_symbols, symbolId).theta;
}

double LiveFormulaContext::vega(const QString &symbolId) const {
    return fetchState(m_symbols, symbolId).vega;
}
