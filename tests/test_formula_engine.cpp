/**
 * @file test_formula_engine.cpp
 * @brief Unit tests for FormulaEngine expression parsing and evaluation
 *
 * Tests the recursive-descent parser and evaluator for:
 *   - Arithmetic (+ - * / % ^)
 *   - Comparison (> >= < <= == !=)
 *   - Logical (&& || !)
 *   - Ternary (? :)
 *   - Built-in functions (ABS, SQRT, MAX, MIN, etc.)
 *   - Parameter references
 *   - Market data functions (via mock context)
 *   - Validation and introspection
 *   - Error handling (division by zero, unknown params, etc.)
 */

#include "strategy/runtime/FormulaEngine.h"
#include <QCoreApplication>
#include <QDebug>
#include <cmath>
#include <iostream>

// ═══════════════════════════════════════════════════════════════════
// MOCK FORMULA CONTEXT
// Provides deterministic values for testing without singletons.
// ═══════════════════════════════════════════════════════════════════

class MockFormulaContext : public FormulaContext {
public:
    // Configurable price data per symbol
    struct SymbolData {
        double ltp = 0.0, open = 0.0, high = 0.0, low = 0.0, close = 0.0;
        double volume = 0.0, bid = 0.0, ask = 0.0, changePct = 0.0;
        double ivVal = 0.0, deltaVal = 0.0, gammaVal = 0.0;
        double thetaVal = 0.0, vegaVal = 0.0;
    };

    void setSymbolData(const QString &symbolId, const SymbolData &data) {
        m_data[symbolId] = data;
    }

    void setIndicatorValue(const QString &symbolId, const QString &type,
                           int period, double value) {
        QString key = QString("%1_%2_%3").arg(symbolId, type).arg(period);
        m_indicators[key] = value;
    }

    void setMtmValue(double v) { m_mtm = v; }
    void setNetPremiumValue(double v) { m_netPremium = v; }
    void setNetDeltaValue(double v) { m_netDelta = v; }

    // ── FormulaContext interface ──
    double ltp(const QString &id) const override { return m_data.value(id).ltp; }
    double open(const QString &id) const override { return m_data.value(id).open; }
    double high(const QString &id) const override { return m_data.value(id).high; }
    double low(const QString &id) const override { return m_data.value(id).low; }
    double close(const QString &id) const override { return m_data.value(id).close; }
    double volume(const QString &id) const override { return m_data.value(id).volume; }
    double bid(const QString &id) const override { return m_data.value(id).bid; }
    double ask(const QString &id) const override { return m_data.value(id).ask; }
    double changePct(const QString &id) const override { return m_data.value(id).changePct; }

    double indicator(const QString &symbolId, const QString &indicatorType,
                     int period, int period2, int period3) const override {
        Q_UNUSED(period2) Q_UNUSED(period3)
        QString key = QString("%1_%2_%3").arg(symbolId, indicatorType).arg(period);
        return m_indicators.value(key, 0.0);
    }

    double iv(const QString &id) const override { return m_data.value(id).ivVal; }
    double delta(const QString &id) const override { return m_data.value(id).deltaVal; }
    double gamma(const QString &id) const override { return m_data.value(id).gammaVal; }
    double theta(const QString &id) const override { return m_data.value(id).thetaVal; }
    double vega(const QString &id) const override { return m_data.value(id).vegaVal; }

    double mtm() const override { return m_mtm; }
    double netPremium() const override { return m_netPremium; }
    double netDelta() const override { return m_netDelta; }

private:
    QHash<QString, SymbolData> m_data;
    QHash<QString, double> m_indicators;
    double m_mtm = 0.0, m_netPremium = 0.0, m_netDelta = 0.0;
};

// ═══════════════════════════════════════════════════════════════════
// TEST FRAMEWORK (lightweight — no external dependency)
// ═══════════════════════════════════════════════════════════════════

static int g_passed = 0;
static int g_failed = 0;

#define ASSERT_EQ(expr, expected, name)                                        \
    do {                                                                       \
        auto _val = (expr);                                                    \
        auto _exp = (expected);                                                \
        if (_val == _exp) {                                                    \
            g_passed++;                                                        \
        } else {                                                               \
            g_failed++;                                                        \
            qWarning() << "[FAIL]" << name << ": expected" << _exp             \
                       << "got" << _val;                                       \
        }                                                                      \
    } while (0)

#define ASSERT_NEAR(expr, expected, eps, name)                                 \
    do {                                                                       \
        double _val = (expr);                                                  \
        double _exp = (expected);                                              \
        if (std::abs(_val - _exp) <= (eps)) {                                  \
            g_passed++;                                                        \
        } else {                                                               \
            g_failed++;                                                        \
            qWarning() << "[FAIL]" << name << ": expected" << _exp             \
                       << "got" << _val << "(eps=" << eps << ")";              \
        }                                                                      \
    } while (0)

#define ASSERT_TRUE(expr, name)                                                \
    do {                                                                       \
        if ((expr)) {                                                          \
            g_passed++;                                                        \
        } else {                                                               \
            g_failed++;                                                        \
            qWarning() << "[FAIL]" << name;                                    \
        }                                                                      \
    } while (0)

#define ASSERT_FALSE(expr, name)                                               \
    do {                                                                       \
        if (!(expr)) {                                                         \
            g_passed++;                                                        \
        } else {                                                               \
            g_failed++;                                                        \
            qWarning() << "[FAIL]" << name << "(expected false)";              \
        }                                                                      \
    } while (0)

// ═══════════════════════════════════════════════════════════════════
// TEST: Basic Arithmetic
// ═══════════════════════════════════════════════════════════════════

void testArithmetic() {
    FormulaEngine engine;
    bool ok;

    // Literals
    ASSERT_NEAR(engine.evaluate("42", &ok), 42.0, 1e-9, "literal 42");
    ASSERT_TRUE(ok, "literal ok");

    ASSERT_NEAR(engine.evaluate("3.14", &ok), 3.14, 1e-9, "literal 3.14");

    // Addition
    ASSERT_NEAR(engine.evaluate("2 + 3", &ok), 5.0, 1e-9, "2 + 3");
    ASSERT_TRUE(ok, "add ok");

    // Subtraction
    ASSERT_NEAR(engine.evaluate("10 - 7", &ok), 3.0, 1e-9, "10 - 7");

    // Multiplication
    ASSERT_NEAR(engine.evaluate("4 * 5", &ok), 20.0, 1e-9, "4 * 5");

    // Division
    ASSERT_NEAR(engine.evaluate("15 / 3", &ok), 5.0, 1e-9, "15 / 3");

    // Modulo
    ASSERT_NEAR(engine.evaluate("17 % 5", &ok), 2.0, 1e-9, "17 % 5");

    // Power
    ASSERT_NEAR(engine.evaluate("2 ^ 10", &ok), 1024.0, 1e-9, "2 ^ 10");

    // Complex expression with precedence
    ASSERT_NEAR(engine.evaluate("2 + 3 * 4", &ok), 14.0, 1e-9,
                "precedence: 2 + 3 * 4");
    ASSERT_NEAR(engine.evaluate("(2 + 3) * 4", &ok), 20.0, 1e-9,
                "parens: (2 + 3) * 4");

    // Nested parentheses
    ASSERT_NEAR(engine.evaluate("((1 + 2) * (3 + 4))", &ok), 21.0, 1e-9,
                "nested parens");

    // Unary minus
    ASSERT_NEAR(engine.evaluate("-5", &ok), -5.0, 1e-9, "unary minus");
    ASSERT_NEAR(engine.evaluate("-(3 + 4)", &ok), -7.0, 1e-9,
                "unary minus parens");

    // Double negation
    ASSERT_NEAR(engine.evaluate("--5", &ok), 5.0, 1e-9, "double negation");

    // Mixed operations
    ASSERT_NEAR(engine.evaluate("10 / 2 + 3 * 4 - 1", &ok), 16.0, 1e-9,
                "mixed ops");

    // Scientific notation
    ASSERT_NEAR(engine.evaluate("1e3", &ok), 1000.0, 1e-9, "scientific 1e3");
    ASSERT_NEAR(engine.evaluate("2.5e2", &ok), 250.0, 1e-9, "scientific 2.5e2");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Division by Zero
// ═══════════════════════════════════════════════════════════════════

void testDivisionByZero() {
    FormulaEngine engine;
    bool ok;

    engine.evaluate("10 / 0", &ok);
    ASSERT_FALSE(ok, "division by zero fails");

    engine.evaluate("10 % 0", &ok);
    ASSERT_FALSE(ok, "modulo by zero fails");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Comparison Operators
// ═══════════════════════════════════════════════════════════════════

void testComparisons() {
    FormulaEngine engine;
    bool ok;

    ASSERT_NEAR(engine.evaluate("5 > 3", &ok), 1.0, 1e-9, "5 > 3");
    ASSERT_NEAR(engine.evaluate("3 > 5", &ok), 0.0, 1e-9, "3 > 5");
    ASSERT_NEAR(engine.evaluate("5 >= 5", &ok), 1.0, 1e-9, "5 >= 5");
    ASSERT_NEAR(engine.evaluate("4 >= 5", &ok), 0.0, 1e-9, "4 >= 5");
    ASSERT_NEAR(engine.evaluate("3 < 5", &ok), 1.0, 1e-9, "3 < 5");
    ASSERT_NEAR(engine.evaluate("5 < 3", &ok), 0.0, 1e-9, "5 < 3");
    ASSERT_NEAR(engine.evaluate("5 <= 5", &ok), 1.0, 1e-9, "5 <= 5");
    ASSERT_NEAR(engine.evaluate("6 <= 5", &ok), 0.0, 1e-9, "6 <= 5");
    ASSERT_NEAR(engine.evaluate("5 == 5", &ok), 1.0, 1e-9, "5 == 5");
    ASSERT_NEAR(engine.evaluate("5 == 6", &ok), 0.0, 1e-9, "5 == 6");
    ASSERT_NEAR(engine.evaluate("5 != 6", &ok), 1.0, 1e-9, "5 != 6");
    ASSERT_NEAR(engine.evaluate("5 != 5", &ok), 0.0, 1e-9, "5 != 5");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Logical Operators
// ═══════════════════════════════════════════════════════════════════

void testLogical() {
    FormulaEngine engine;
    bool ok;

    // AND
    ASSERT_NEAR(engine.evaluate("1 && 1", &ok), 1.0, 1e-9, "1 && 1");
    ASSERT_NEAR(engine.evaluate("1 && 0", &ok), 0.0, 1e-9, "1 && 0");
    ASSERT_NEAR(engine.evaluate("0 && 1", &ok), 0.0, 1e-9, "0 && 1");
    ASSERT_NEAR(engine.evaluate("0 && 0", &ok), 0.0, 1e-9, "0 && 0");

    // OR
    ASSERT_NEAR(engine.evaluate("1 || 0", &ok), 1.0, 1e-9, "1 || 0");
    ASSERT_NEAR(engine.evaluate("0 || 1", &ok), 1.0, 1e-9, "0 || 1");
    ASSERT_NEAR(engine.evaluate("0 || 0", &ok), 0.0, 1e-9, "0 || 0");

    // NOT
    ASSERT_NEAR(engine.evaluate("!0", &ok), 1.0, 1e-9, "!0");
    ASSERT_NEAR(engine.evaluate("!1", &ok), 0.0, 1e-9, "!1");
    ASSERT_NEAR(engine.evaluate("!42", &ok), 0.0, 1e-9, "!42 (truthy)");

    // Short-circuit: 0 && (anything) should not evaluate RHS
    ASSERT_NEAR(engine.evaluate("0 && (10 / 0)", &ok), 0.0, 1e-9,
                "short-circuit AND");
    ASSERT_TRUE(ok, "short-circuit AND ok");

    // Short-circuit: 1 || (anything) should not evaluate RHS
    ASSERT_NEAR(engine.evaluate("1 || (10 / 0)", &ok), 1.0, 1e-9,
                "short-circuit OR");
    ASSERT_TRUE(ok, "short-circuit OR ok");

    // Combined: comparison + logical
    ASSERT_NEAR(engine.evaluate("(5 > 3) && (2 < 4)", &ok), 1.0, 1e-9,
                "combined compare+logical AND");
    ASSERT_NEAR(engine.evaluate("(5 > 3) && (2 > 4)", &ok), 0.0, 1e-9,
                "combined compare+logical AND (false)");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Ternary Operator
// ═══════════════════════════════════════════════════════════════════

void testTernary() {
    FormulaEngine engine;
    bool ok;

    ASSERT_NEAR(engine.evaluate("1 ? 10 : 20", &ok), 10.0, 1e-9,
                "ternary true");
    ASSERT_NEAR(engine.evaluate("0 ? 10 : 20", &ok), 20.0, 1e-9,
                "ternary false");

    // Nested ternary
    ASSERT_NEAR(engine.evaluate("1 ? (0 ? 10 : 20) : 30", &ok), 20.0, 1e-9,
                "nested ternary");

    // Ternary with comparison
    ASSERT_NEAR(engine.evaluate("5 > 3 ? 100 : 200", &ok), 100.0, 1e-9,
                "ternary with comparison true");
    ASSERT_NEAR(engine.evaluate("5 < 3 ? 100 : 200", &ok), 200.0, 1e-9,
                "ternary with comparison false");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Built-in Math Functions
// ═══════════════════════════════════════════════════════════════════

void testMathFunctions() {
    FormulaEngine engine;
    bool ok;

    // ABS
    ASSERT_NEAR(engine.evaluate("ABS(-5)", &ok), 5.0, 1e-9, "ABS(-5)");
    ASSERT_NEAR(engine.evaluate("ABS(5)", &ok), 5.0, 1e-9, "ABS(5)");

    // SQRT
    ASSERT_NEAR(engine.evaluate("SQRT(25)", &ok), 5.0, 1e-9, "SQRT(25)");
    ASSERT_NEAR(engine.evaluate("SQRT(2)", &ok), std::sqrt(2.0), 1e-9,
                "SQRT(2)");

    // SQRT of negative
    engine.evaluate("SQRT(-1)", &ok);
    ASSERT_FALSE(ok, "SQRT(-1) fails");

    // LOG
    ASSERT_NEAR(engine.evaluate("LOG(1)", &ok), 0.0, 1e-9, "LOG(1)");
    ASSERT_NEAR(engine.evaluate("LOG(2.718281828)", &ok), 1.0, 1e-6,
                "LOG(e) ≈ 1");

    // LOG of non-positive
    engine.evaluate("LOG(0)", &ok);
    ASSERT_FALSE(ok, "LOG(0) fails");
    engine.evaluate("LOG(-5)", &ok);
    ASSERT_FALSE(ok, "LOG(-5) fails");

    // ROUND, FLOOR, CEIL
    ASSERT_NEAR(engine.evaluate("ROUND(3.7)", &ok), 4.0, 1e-9, "ROUND(3.7)");
    ASSERT_NEAR(engine.evaluate("ROUND(3.2)", &ok), 3.0, 1e-9, "ROUND(3.2)");
    ASSERT_NEAR(engine.evaluate("FLOOR(3.9)", &ok), 3.0, 1e-9, "FLOOR(3.9)");
    ASSERT_NEAR(engine.evaluate("CEIL(3.1)", &ok), 4.0, 1e-9, "CEIL(3.1)");

    // MAX, MIN
    ASSERT_NEAR(engine.evaluate("MAX(10, 20)", &ok), 20.0, 1e-9,
                "MAX(10,20)");
    ASSERT_NEAR(engine.evaluate("MIN(10, 20)", &ok), 10.0, 1e-9,
                "MIN(10,20)");

    // POW
    ASSERT_NEAR(engine.evaluate("POW(2, 8)", &ok), 256.0, 1e-9, "POW(2,8)");

    // CLAMP
    ASSERT_NEAR(engine.evaluate("CLAMP(5, 0, 10)", &ok), 5.0, 1e-9,
                "CLAMP in range");
    ASSERT_NEAR(engine.evaluate("CLAMP(-5, 0, 10)", &ok), 0.0, 1e-9,
                "CLAMP below");
    ASSERT_NEAR(engine.evaluate("CLAMP(15, 0, 10)", &ok), 10.0, 1e-9,
                "CLAMP above");

    // IF (functional ternary)
    ASSERT_NEAR(engine.evaluate("IF(1, 100, 200)", &ok), 100.0, 1e-9,
                "IF true");
    ASSERT_NEAR(engine.evaluate("IF(0, 100, 200)", &ok), 200.0, 1e-9,
                "IF false");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Parameter References
// ═══════════════════════════════════════════════════════════════════

void testParameters() {
    FormulaEngine engine;
    bool ok;

    engine.setParam("RSI_PERIOD", 14.0);
    engine.setParam("OFFSET_PCT", 0.5);
    engine.setParam("THRESHOLD", 70.0);

    ASSERT_NEAR(engine.evaluate("RSI_PERIOD", &ok), 14.0, 1e-9,
                "param RSI_PERIOD");
    ASSERT_TRUE(ok, "param ok");

    ASSERT_NEAR(engine.evaluate("RSI_PERIOD + 1", &ok), 15.0, 1e-9,
                "param + literal");

    ASSERT_NEAR(engine.evaluate("100 * (1 + OFFSET_PCT / 100)", &ok), 100.5,
                1e-9, "param in formula");

    // Param in comparison
    ASSERT_NEAR(engine.evaluate("THRESHOLD > 50", &ok), 1.0, 1e-9,
                "param comparison");

    // Special constants
    ASSERT_NEAR(engine.evaluate("PI", &ok), M_PI, 1e-9, "PI constant");
    ASSERT_NEAR(engine.evaluate("TRUE", &ok), 1.0, 1e-9, "TRUE constant");
    ASSERT_NEAR(engine.evaluate("FALSE", &ok), 0.0, 1e-9, "FALSE constant");

    // Unknown parameter → error
    engine.evaluate("UNKNOWN_PARAM", &ok);
    ASSERT_FALSE(ok, "unknown param fails");

    // hasParam
    ASSERT_TRUE(engine.hasParam("RSI_PERIOD"), "hasParam existing");
    ASSERT_FALSE(engine.hasParam("NONEXISTENT"), "hasParam nonexistent");

    // clearParams
    engine.clearParams();
    engine.evaluate("RSI_PERIOD", &ok);
    ASSERT_FALSE(ok, "cleared param fails");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Market Data Functions (with mock context)
// ═══════════════════════════════════════════════════════════════════

void testMarketDataFunctions() {
    FormulaEngine engine;
    MockFormulaContext ctx;
    engine.setContext(&ctx);
    bool ok;

    MockFormulaContext::SymbolData nifty;
    nifty.ltp = 22500.0;
    nifty.open = 22400.0;
    nifty.high = 22600.0;
    nifty.low = 22300.0;
    nifty.close = 22450.0;
    nifty.volume = 1000000;
    nifty.bid = 22495.0;
    nifty.ask = 22505.0;
    nifty.changePct = 0.45;
    ctx.setSymbolData("REF_1", nifty);

    // Price functions
    ASSERT_NEAR(engine.evaluate("LTP(REF_1)", &ok), 22500.0, 1e-9,
                "LTP(REF_1)");
    ASSERT_TRUE(ok, "LTP ok");

    ASSERT_NEAR(engine.evaluate("OPEN(REF_1)", &ok), 22400.0, 1e-9,
                "OPEN(REF_1)");
    ASSERT_NEAR(engine.evaluate("HIGH(REF_1)", &ok), 22600.0, 1e-9,
                "HIGH(REF_1)");
    ASSERT_NEAR(engine.evaluate("LOW(REF_1)", &ok), 22300.0, 1e-9,
                "LOW(REF_1)");
    ASSERT_NEAR(engine.evaluate("CLOSE(REF_1)", &ok), 22450.0, 1e-9,
                "CLOSE(REF_1)");
    ASSERT_NEAR(engine.evaluate("BID(REF_1)", &ok), 22495.0, 1e-9,
                "BID(REF_1)");
    ASSERT_NEAR(engine.evaluate("ASK(REF_1)", &ok), 22505.0, 1e-9,
                "ASK(REF_1)");
    ASSERT_NEAR(engine.evaluate("CHANGE_PCT(REF_1)", &ok), 0.45, 1e-9,
                "CHANGE_PCT(REF_1)");

    // Computed expressions using market data
    ASSERT_NEAR(engine.evaluate("ASK(REF_1) - BID(REF_1)", &ok), 10.0, 1e-9,
                "spread calc");
    ASSERT_NEAR(engine.evaluate("(HIGH(REF_1) - LOW(REF_1)) / 2", &ok), 150.0,
                1e-9, "half range");
    ASSERT_NEAR(engine.evaluate("LTP(REF_1) * 1.01", &ok), 22725.0, 1e-9,
                "LTP * factor");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Greeks Functions
// ═══════════════════════════════════════════════════════════════════

void testGreeksFunctions() {
    FormulaEngine engine;
    MockFormulaContext ctx;
    engine.setContext(&ctx);
    bool ok;

    MockFormulaContext::SymbolData opt;
    opt.ivVal = 18.5;
    opt.deltaVal = 0.55;
    opt.gammaVal = 0.003;
    opt.thetaVal = -15.2;
    opt.vegaVal = 12.0;
    ctx.setSymbolData("TRADE_1", opt);

    ASSERT_NEAR(engine.evaluate("IV(TRADE_1)", &ok), 18.5, 1e-9,
                "IV(TRADE_1)");
    ASSERT_NEAR(engine.evaluate("DELTA(TRADE_1)", &ok), 0.55, 1e-9,
                "DELTA(TRADE_1)");
    ASSERT_NEAR(engine.evaluate("GAMMA(TRADE_1)", &ok), 0.003, 1e-9,
                "GAMMA(TRADE_1)");
    ASSERT_NEAR(engine.evaluate("THETA(TRADE_1)", &ok), -15.2, 1e-9,
                "THETA(TRADE_1)");
    ASSERT_NEAR(engine.evaluate("VEGA(TRADE_1)", &ok), 12.0, 1e-9,
                "VEGA(TRADE_1)");

    // Greeks in conditions
    ASSERT_NEAR(engine.evaluate("IV(TRADE_1) > 15 ? 0.02 : 0.01", &ok), 0.02,
                1e-9, "IV ternary");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Indicator Functions
// ═══════════════════════════════════════════════════════════════════

void testIndicatorFunctions() {
    FormulaEngine engine;
    MockFormulaContext ctx;
    engine.setContext(&ctx);
    bool ok;

    ctx.setIndicatorValue("REF_1", "RSI", 14, 65.0);
    ctx.setIndicatorValue("REF_1", "SMA", 20, 22400.0);
    ctx.setIndicatorValue("REF_1", "ATR", 14, 150.0);

    ASSERT_NEAR(engine.evaluate("RSI(REF_1, 14)", &ok), 65.0, 1e-9,
                "RSI(REF_1, 14)");
    ASSERT_NEAR(engine.evaluate("SMA(REF_1, 20)", &ok), 22400.0, 1e-9,
                "SMA(REF_1, 20)");

    // Indicator in formula
    ASSERT_NEAR(engine.evaluate("ATR(REF_1, 14) * 2.5", &ok), 375.0, 1e-9,
                "ATR * multiplier");

    // Indicator with param reference
    engine.setParam("ATR_MULT", 3.0);
    ASSERT_NEAR(engine.evaluate("ATR(REF_1, 14) * ATR_MULT", &ok), 450.0,
                1e-9, "ATR * param");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Portfolio Functions
// ═══════════════════════════════════════════════════════════════════

void testPortfolioFunctions() {
    FormulaEngine engine;
    MockFormulaContext ctx;
    engine.setContext(&ctx);
    bool ok;

    ctx.setMtmValue(-500.0);
    ctx.setNetPremiumValue(1200.0);
    ctx.setNetDeltaValue(0.25);

    ASSERT_NEAR(engine.evaluate("MTM()", &ok), -500.0, 1e-9, "MTM()");
    ASSERT_NEAR(engine.evaluate("NET_PREMIUM()", &ok), 1200.0, 1e-9,
                "NET_PREMIUM()");
    ASSERT_NEAR(engine.evaluate("NET_DELTA()", &ok), 0.25, 1e-9,
                "NET_DELTA()");

    // Portfolio in condition
    ASSERT_NEAR(engine.evaluate("MTM() < -400 ? 1 : 0", &ok), 1.0, 1e-9,
                "MTM risk check");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Validation
// ═══════════════════════════════════════════════════════════════════

void testValidation() {
    FormulaEngine engine;
    QString errMsg;

    ASSERT_TRUE(engine.validate("2 + 3", &errMsg), "valid: 2 + 3");
    ASSERT_TRUE(engine.validate("(1 + 2) * 3", &errMsg), "valid: parens");
    ASSERT_TRUE(engine.validate("ABS(-5)", &errMsg), "valid: function");
    ASSERT_TRUE(engine.validate("5 > 3 ? 1 : 0", &errMsg), "valid: ternary");

    ASSERT_FALSE(engine.validate("2 +", &errMsg), "invalid: trailing op");
    ASSERT_FALSE(engine.validate("(2 + 3", &errMsg), "invalid: unclosed paren");
    ASSERT_FALSE(engine.validate("ABS(", &errMsg), "invalid: unclosed func");

    // Empty expression is valid (returns 0)
    bool ok;
    ASSERT_NEAR(engine.evaluate("", &ok), 0.0, 1e-9, "empty = 0");
    ASSERT_TRUE(ok, "empty ok");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Introspection (referencedParams / referencedSymbols)
// ═══════════════════════════════════════════════════════════════════

void testIntrospection() {
    FormulaEngine engine;

    // referencedParams
    QStringList params = engine.referencedParams("RSI_PERIOD + OFFSET_PCT * 2");
    ASSERT_TRUE(params.contains("RSI_PERIOD"), "refs RSI_PERIOD");
    ASSERT_TRUE(params.contains("OFFSET_PCT"), "refs OFFSET_PCT");
    ASSERT_EQ(params.size(), 2, "exactly 2 params");

    // referencedSymbols
    QStringList syms = engine.referencedSymbols("LTP(REF_1) + RSI(TRADE_1, 14)");
    ASSERT_TRUE(syms.contains("REF_1"), "refs REF_1");
    ASSERT_TRUE(syms.contains("TRADE_1"), "refs TRADE_1");
    ASSERT_EQ(syms.size(), 2, "exactly 2 symbols");

    // No references
    QStringList empty = engine.referencedParams("42 + 3");
    ASSERT_EQ(empty.size(), 0, "no param refs in literal expr");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Complex Real-World Expressions
// ═══════════════════════════════════════════════════════════════════

void testRealWorldExpressions() {
    FormulaEngine engine;
    MockFormulaContext ctx;
    engine.setContext(&ctx);
    bool ok;

    // Setup market data
    MockFormulaContext::SymbolData ref;
    ref.ltp = 22500.0;
    ref.high = 22600.0;
    ref.low = 22300.0;
    ref.close = 22450.0;
    ctx.setSymbolData("REF_1", ref);

    MockFormulaContext::SymbolData trade;
    trade.ltp = 350.0;
    trade.ivVal = 22.0;
    ctx.setSymbolData("TRADE_1", trade);

    ctx.setIndicatorValue("REF_1", "ATR", 14, 150.0);
    ctx.setIndicatorValue("REF_1", "SMA", 20, 22400.0);
    ctx.setMtmValue(-200.0);

    engine.setParam("ATR_MULT", 2.5);
    engine.setParam("OFFSET_PCT", 0.5);

    // ATR-based stop loss level
    // SL = LTP(REF_1) - ATR(REF_1, 14) * ATR_MULT
    ASSERT_NEAR(engine.evaluate("LTP(REF_1) - ATR(REF_1, 14) * ATR_MULT", &ok),
                22500.0 - 150.0 * 2.5, 1e-9, "ATR stop loss");

    // Adaptive lot size based on IV
    // IV(TRADE_1) > 25 ? 0.02 : 0.01
    ASSERT_NEAR(engine.evaluate("IV(TRADE_1) > 25 ? 0.02 : 0.01", &ok), 0.01,
                1e-9, "adaptive lot (low IV)");

    // SMA offset entry: SMA(REF_1, 20) * (1 + OFFSET_PCT / 100)
    ASSERT_NEAR(
        engine.evaluate("SMA(REF_1, 20) * (1 + OFFSET_PCT / 100)", &ok),
        22400.0 * 1.005, 1e-6, "SMA offset entry");

    // Complex multi-function: MAX(LTP, SMA) + ABS(MTM())
    ASSERT_NEAR(
        engine.evaluate("MAX(LTP(REF_1), SMA(REF_1, 20)) + ABS(MTM())", &ok),
        22500.0 + 200.0, 1e-9, "MAX + ABS(MTM)");

    // Clamp risk: CLAMP(MTM(), -1000, 1000)
    ASSERT_NEAR(engine.evaluate("CLAMP(MTM(), -1000, 1000)", &ok), -200.0,
                1e-9, "CLAMP MTM in range");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Error Handling
// ═══════════════════════════════════════════════════════════════════

void testErrorHandling() {
    FormulaEngine engine;
    bool ok;

    // No context → market data function fails
    engine.evaluate("LTP(REF_1)", &ok);
    ASSERT_FALSE(ok, "LTP without context fails");

    // Unknown function
    engine.evaluate("FOOBAR(1, 2)", &ok);
    ASSERT_FALSE(ok, "unknown function fails");

    // Wrong arg count
    MockFormulaContext ctx;
    engine.setContext(&ctx);
    engine.evaluate("ABS(1, 2)", &ok);
    ASSERT_FALSE(ok, "ABS wrong arg count");

    engine.evaluate("MAX(1)", &ok);
    ASSERT_FALSE(ok, "MAX wrong arg count");

    // Invalid syntax
    engine.evaluate("@#$", &ok);
    ASSERT_FALSE(ok, "invalid syntax");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Case insensitivity
// ═══════════════════════════════════════════════════════════════════

void testCaseInsensitivity() {
    FormulaEngine engine;
    bool ok;

    // Identifiers are uppercased
    engine.setParam("my_param", 42.0);
    ASSERT_NEAR(engine.evaluate("MY_PARAM", &ok), 42.0, 1e-9,
                "case insensitive param");

    // Functions are case insensitive
    ASSERT_NEAR(engine.evaluate("abs(-5)", &ok), 5.0, 1e-9,
                "lowercase function");
    ASSERT_NEAR(engine.evaluate("Abs(-5)", &ok), 5.0, 1e-9,
                "mixed case function");
}

// ═══════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    qInfo() << "═══════════════════════════════════════════════════════";
    qInfo() << "  FormulaEngine Unit Tests";
    qInfo() << "═══════════════════════════════════════════════════════";

    testArithmetic();
    testDivisionByZero();
    testComparisons();
    testLogical();
    testTernary();
    testMathFunctions();
    testParameters();
    testMarketDataFunctions();
    testGreeksFunctions();
    testIndicatorFunctions();
    testPortfolioFunctions();
    testValidation();
    testIntrospection();
    testRealWorldExpressions();
    testErrorHandling();
    testCaseInsensitivity();

    qInfo() << "";
    qInfo() << "═══════════════════════════════════════════════════════";
    qInfo() << "  Results:" << g_passed << "passed," << g_failed << "failed";
    qInfo() << "  Total:" << (g_passed + g_failed) << "assertions";
    if (g_failed > 0)
        qInfo() << "  ❌ SOME TESTS FAILED";
    else
        qInfo() << "  ✅ ALL TESTS PASSED";
    qInfo() << "═══════════════════════════════════════════════════════";

    return g_failed > 0 ? 1 : 0;
}
