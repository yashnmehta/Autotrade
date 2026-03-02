/**
 * @file test_condition_evaluation.cpp
 * @brief Unit tests for ConditionNode evaluation and Operand resolution
 *
 * Tests the condition tree evaluation logic that TemplateStrategy uses:
 *   - Leaf conditions with all comparison operators
 *   - Branch conditions (AND / OR)
 *   - Nested condition trees
 *   - All operand types (Price, Indicator, Constant, ParamRef, Formula, Greek, Spread, Total)
 *   - Crossover detection (crosses_above, crosses_below)
 *   - Edge cases (empty nodes, missing data, zero values)
 *
 * Uses a standalone evaluator that mirrors TemplateStrategy::evaluateCondition()
 * but without requiring any singletons or live market infrastructure.
 */

#include "strategy/model/ConditionNode.h"
#include "strategy/runtime/FormulaEngine.h"
#include <QCoreApplication>
#include <QDebug>
#include <QHash>
#include <cmath>

// ═══════════════════════════════════════════════════════════════════
// MOCK EVALUATOR
// Replicates TemplateStrategy's evaluateCondition() and resolveOperand()
// without PriceStoreGateway or IndicatorEngine singletons.
// ═══════════════════════════════════════════════════════════════════

class ConditionEvaluator {
public:
    // ── Configurable price data per symbol ──
    struct PriceData {
        double ltp = 0.0, open = 0.0, high = 0.0, low = 0.0, close = 0.0;
        double iv = 0.0, delta = 0.0, gamma = 0.0, theta = 0.0, vega = 0.0;
        double bidPrice = 0.0, askPrice = 0.0;
    };

    void setPrice(const QString &symbolId, const PriceData &data) {
        m_prices[symbolId] = data;
    }

    void setIndicatorValue(const QString &symbolId, const QString &indId,
                           double value) {
        m_indicators[symbolId + ":" + indId] = value;
    }

    void setIndicatorReady(const QString &symbolId, const QString &indId,
                           bool ready) {
        m_indicatorReady[symbolId + ":" + indId] = ready;
    }

    void setParam(const QString &name, double value) {
        m_engine.setParam(name, value);
    }

    void setPortfolioMtm(double v) { m_mtm = v; }
    void setPortfolioNetPremium(double v) { m_netPremium = v; }
    void setPortfolioNetDelta(double v) { m_netDelta = v; }

    // ── Evaluate ──
    bool evaluate(const ConditionNode &node) {
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

            // Crossover
            if (node.op == "crosses_above" || node.op == "crosses_below") {
                QString kl = operandKey(node.left);
                QString kr = operandKey(node.right);
                double prevL = m_prevValues.value(kl, leftVal);
                double prevR = m_prevValues.value(kr, rightVal);
                m_prevValues[kl] = leftVal;
                m_prevValues[kr] = rightVal;

                if (node.op == "crosses_above")
                    return (prevL <= prevR) && (leftVal > rightVal);
                else
                    return (prevL >= prevR) && (leftVal < rightVal);
            }

            return false;
        }

        // Branch: AND or OR
        if (node.nodeType == ConditionNode::NodeType::And) {
            for (const auto &child : node.children) {
                if (!evaluate(child))
                    return false;
            }
            return !node.children.isEmpty();
        }

        if (node.nodeType == ConditionNode::NodeType::Or) {
            for (const auto &child : node.children) {
                if (evaluate(child))
                    return true;
            }
            return false;
        }

        return false;
    }

    // Clear crossover state between test cases
    void resetCrossoverState() { m_prevValues.clear(); }

private:
    double resolveOperand(const Operand &op) const {
        switch (op.type) {
        case Operand::Type::Constant:
            return op.constantValue;

        case Operand::Type::ParamRef:
            return m_engine.param(op.paramName);

        case Operand::Type::Formula: {
            if (op.formulaExpression.isEmpty())
                return 0.0;
            bool ok;
            return m_engine.evaluate(op.formulaExpression, &ok);
        }

        case Operand::Type::Price: {
            auto it = m_prices.find(op.symbolId);
            if (it == m_prices.end())
                return 0.0;
            if (op.field == "ltp")   return it->ltp;
            if (op.field == "open")  return it->open;
            if (op.field == "high")  return it->high;
            if (op.field == "low")   return it->low;
            if (op.field == "close") return it->close;
            return it->ltp;
        }

        case Operand::Type::Indicator: {
            QString key = op.symbolId + ":" + op.indicatorId;
            return m_indicators.value(key, 0.0);
        }

        case Operand::Type::Greek: {
            auto it = m_prices.find(op.symbolId);
            if (it == m_prices.end())
                return 0.0;
            if (op.field == "iv")    return it->iv;
            if (op.field == "delta") return it->delta;
            if (op.field == "gamma") return it->gamma;
            if (op.field == "theta") return it->theta;
            if (op.field == "vega")  return it->vega;
            return 0.0;
        }

        case Operand::Type::Spread: {
            auto it = m_prices.find(op.symbolId);
            if (it == m_prices.end())
                return 0.0;
            return it->askPrice - it->bidPrice;
        }

        case Operand::Type::Total: {
            if (op.field == "mtm" || op.field == "mtm_total")
                return m_mtm;
            if (op.field == "net_premium")
                return m_netPremium;
            if (op.field == "net_delta")
                return m_netDelta;
            return 0.0;
        }
        }
        return 0.0;
    }

    QString operandKey(const Operand &op) const {
        switch (op.type) {
        case Operand::Type::Price:
            return QString("P_%1_%2").arg(op.symbolId, op.field);
        case Operand::Type::Indicator:
            return QString("I_%1_%2").arg(op.indicatorId, op.outputSeries);
        case Operand::Type::Constant:
            return QString("C_%1").arg(op.constantValue);
        case Operand::Type::ParamRef:
            return QString("R_%1").arg(op.paramName);
        case Operand::Type::Formula:
            return QString("F_%1").arg(qHash(op.formulaExpression));
        case Operand::Type::Greek:
            return QString("G_%1_%2").arg(op.symbolId, op.field);
        case Operand::Type::Spread:
            return QString("S_%1").arg(op.symbolId);
        case Operand::Type::Total:
            return QString("T_%1").arg(op.field);
        }
        return "?";
    }

    QHash<QString, PriceData> m_prices;
    QHash<QString, double> m_indicators;
    QHash<QString, bool> m_indicatorReady;
    FormulaEngine m_engine;
    QHash<QString, double> m_prevValues;
    double m_mtm = 0.0, m_netPremium = 0.0, m_netDelta = 0.0;
};

// ═══════════════════════════════════════════════════════════════════
// HELPERS: Build Operands and ConditionNodes
// ═══════════════════════════════════════════════════════════════════

static Operand makeConstant(double val) {
    Operand op;
    op.type = Operand::Type::Constant;
    op.constantValue = val;
    return op;
}

static Operand makePrice(const QString &symbolId, const QString &field) {
    Operand op;
    op.type = Operand::Type::Price;
    op.symbolId = symbolId;
    op.field = field;
    return op;
}

static Operand makeIndicator(const QString &symbolId, const QString &indId,
                              const QString &output = "") {
    Operand op;
    op.type = Operand::Type::Indicator;
    op.symbolId = symbolId;
    op.indicatorId = indId;
    op.outputSeries = output;
    return op;
}

static Operand makeParamRef(const QString &name) {
    Operand op;
    op.type = Operand::Type::ParamRef;
    op.paramName = name;
    return op;
}

static Operand makeFormula(const QString &expr) {
    Operand op;
    op.type = Operand::Type::Formula;
    op.formulaExpression = expr;
    return op;
}

static Operand makeGreek(const QString &symbolId, const QString &field) {
    Operand op;
    op.type = Operand::Type::Greek;
    op.symbolId = symbolId;
    op.field = field;
    return op;
}

static Operand makeSpread(const QString &symbolId) {
    Operand op;
    op.type = Operand::Type::Spread;
    op.symbolId = symbolId;
    return op;
}

static Operand makeTotal(const QString &field) {
    Operand op;
    op.type = Operand::Type::Total;
    op.field = field;
    return op;
}

static ConditionNode makeLeaf(const Operand &left, const QString &op,
                               const Operand &right) {
    ConditionNode node;
    node.nodeType = ConditionNode::NodeType::Leaf;
    node.left = left;
    node.right = right;
    node.op = op;
    return node;
}

static ConditionNode makeAnd(std::initializer_list<ConditionNode> children) {
    ConditionNode node;
    node.nodeType = ConditionNode::NodeType::And;
    for (const auto &c : children)
        node.children.append(c);
    return node;
}

static ConditionNode makeOr(std::initializer_list<ConditionNode> children) {
    ConditionNode node;
    node.nodeType = ConditionNode::NodeType::Or;
    for (const auto &c : children)
        node.children.append(c);
    return node;
}

// ═══════════════════════════════════════════════════════════════════
// TEST FRAMEWORK
// ═══════════════════════════════════════════════════════════════════

static int g_passed = 0;
static int g_failed = 0;

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
// TEST: Basic Comparison Operators
// ═══════════════════════════════════════════════════════════════════

void testBasicComparisons() {
    ConditionEvaluator eval;

    // 100 > 50
    ASSERT_TRUE(eval.evaluate(makeLeaf(makeConstant(100), ">", makeConstant(50))),
                "100 > 50");

    // 50 > 100
    ASSERT_FALSE(eval.evaluate(makeLeaf(makeConstant(50), ">", makeConstant(100))),
                 "50 > 100 (false)");

    // 50 >= 50
    ASSERT_TRUE(eval.evaluate(makeLeaf(makeConstant(50), ">=", makeConstant(50))),
                "50 >= 50");

    // 30 < 70
    ASSERT_TRUE(eval.evaluate(makeLeaf(makeConstant(30), "<", makeConstant(70))),
                "30 < 70");

    // 70 <= 70
    ASSERT_TRUE(eval.evaluate(makeLeaf(makeConstant(70), "<=", makeConstant(70))),
                "70 <= 70");

    // 5 == 5 (fuzzy)
    ASSERT_TRUE(eval.evaluate(makeLeaf(makeConstant(5), "==", makeConstant(5))),
                "5 == 5");

    // 5 != 6
    ASSERT_TRUE(eval.evaluate(makeLeaf(makeConstant(5), "!=", makeConstant(6))),
                "5 != 6");

    // 5 != 5
    ASSERT_FALSE(eval.evaluate(makeLeaf(makeConstant(5), "!=", makeConstant(5))),
                 "5 != 5 (false)");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Price Operands
// ═══════════════════════════════════════════════════════════════════

void testPriceOperands() {
    ConditionEvaluator eval;

    ConditionEvaluator::PriceData nifty;
    nifty.ltp = 22500.0;
    nifty.open = 22400.0;
    nifty.high = 22600.0;
    nifty.low = 22300.0;
    nifty.close = 22450.0;
    eval.setPrice("REF_1", nifty);

    // LTP > close → 22500 > 22450
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makePrice("REF_1", "ltp"), ">",
                               makePrice("REF_1", "close"))),
        "LTP > Close (22500 > 22450)");

    // LTP > high → 22500 > 22600 → false
    ASSERT_FALSE(
        eval.evaluate(makeLeaf(makePrice("REF_1", "ltp"), ">",
                               makePrice("REF_1", "high"))),
        "LTP > High (22500 > 22600 = false)");

    // LTP > constant 22000
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makePrice("REF_1", "ltp"), ">",
                               makeConstant(22000.0))),
        "LTP > 22000");

    // Unknown symbol → price = 0
    ASSERT_FALSE(
        eval.evaluate(makeLeaf(makePrice("UNKNOWN", "ltp"), ">",
                               makeConstant(0.0))),
        "unknown symbol LTP = 0, not > 0");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Indicator Operands
// ═══════════════════════════════════════════════════════════════════

void testIndicatorOperands() {
    ConditionEvaluator eval;

    eval.setIndicatorValue("REF_1", "RSI_14", 65.0);
    eval.setIndicatorValue("REF_1", "SMA_20", 22400.0);

    // RSI_14 > 30
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makeIndicator("REF_1", "RSI_14"), ">",
                               makeConstant(30.0))),
        "RSI > 30");

    // RSI_14 < 70
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makeIndicator("REF_1", "RSI_14"), "<",
                               makeConstant(70.0))),
        "RSI < 70 (oversold check)");

    // RSI_14 > 80 → false (RSI = 65)
    ASSERT_FALSE(
        eval.evaluate(makeLeaf(makeIndicator("REF_1", "RSI_14"), ">",
                               makeConstant(80.0))),
        "RSI > 80 (false, RSI=65)");

    // LTP > SMA_20 → 22500 > 22400
    ConditionEvaluator::PriceData nifty;
    nifty.ltp = 22500.0;
    eval.setPrice("REF_1", nifty);

    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makePrice("REF_1", "ltp"), ">",
                               makeIndicator("REF_1", "SMA_20"))),
        "LTP > SMA (22500 > 22400)");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Param Ref Operands
// ═══════════════════════════════════════════════════════════════════

void testParamRefOperands() {
    ConditionEvaluator eval;
    eval.setParam("RSI_THRESHOLD", 70.0);
    eval.setParam("STOP_LEVEL", 22000.0);

    eval.setIndicatorValue("REF_1", "RSI_14", 65.0);

    // RSI < RSI_THRESHOLD → 65 < 70
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makeIndicator("REF_1", "RSI_14"), "<",
                               makeParamRef("RSI_THRESHOLD"))),
        "RSI < RSI_THRESHOLD (65 < 70)");

    // RSI > RSI_THRESHOLD → 65 > 70 → false
    ASSERT_FALSE(
        eval.evaluate(makeLeaf(makeIndicator("REF_1", "RSI_14"), ">",
                               makeParamRef("RSI_THRESHOLD"))),
        "RSI > RSI_THRESHOLD (false)");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Formula Operands
// ═══════════════════════════════════════════════════════════════════

void testFormulaOperands() {
    ConditionEvaluator eval;
    eval.setParam("ATR_MULT", 2.5);
    eval.setParam("ATR_VAL", 150.0);

    ConditionEvaluator::PriceData ref;
    ref.ltp = 22500.0;
    eval.setPrice("REF_1", ref);

    // LTP > ATR_VAL * ATR_MULT → 22500 > 375
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makePrice("REF_1", "ltp"), ">",
                               makeFormula("ATR_VAL * ATR_MULT"))),
        "LTP > formula (22500 > 375)");

    // LTP < formula result → 22500 < 22501 = true
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makePrice("REF_1", "ltp"), "<",
                               makeFormula("22501"))),
        "LTP < 22501 (22500 < 22501 = true)");

    // LTP > formula result → 22500 > 22501 = false
    ASSERT_FALSE(
        eval.evaluate(makeLeaf(makePrice("REF_1", "ltp"), ">",
                               makeFormula("22501"))),
        "LTP > 22501 (22500 > 22501 = false)");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Greek Operands
// ═══════════════════════════════════════════════════════════════════

void testGreekOperands() {
    ConditionEvaluator eval;

    ConditionEvaluator::PriceData opt;
    opt.iv = 25.0;
    opt.delta = 0.55;
    opt.theta = -15.0;
    eval.setPrice("TRADE_1", opt);

    // IV > 20
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makeGreek("TRADE_1", "iv"), ">",
                               makeConstant(20.0))),
        "IV > 20 (25 > 20)");

    // Delta < 0.6
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makeGreek("TRADE_1", "delta"), "<",
                               makeConstant(0.6))),
        "Delta < 0.6 (0.55 < 0.6)");

    // Theta > -20
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makeGreek("TRADE_1", "theta"), ">",
                               makeConstant(-20.0))),
        "Theta > -20 (-15 > -20)");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Spread Operands
// ═══════════════════════════════════════════════════════════════════

void testSpreadOperands() {
    ConditionEvaluator eval;

    ConditionEvaluator::PriceData opt;
    opt.bidPrice = 100.0;
    opt.askPrice = 102.5;
    eval.setPrice("TRADE_1", opt);

    // Spread < 5 → (102.5 - 100) = 2.5 < 5
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makeSpread("TRADE_1"), "<",
                               makeConstant(5.0))),
        "Spread < 5 (2.5 < 5)");

    // Spread > 3 → 2.5 > 3 = false
    ASSERT_FALSE(
        eval.evaluate(makeLeaf(makeSpread("TRADE_1"), ">",
                               makeConstant(3.0))),
        "Spread > 3 (2.5 > 3 = false)");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Total (Portfolio) Operands
// ═══════════════════════════════════════════════════════════════════

void testTotalOperands() {
    ConditionEvaluator eval;
    eval.setPortfolioMtm(-500.0);
    eval.setPortfolioNetPremium(1200.0);
    eval.setPortfolioNetDelta(0.3);

    // MTM < -300
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makeTotal("mtm"), "<", makeConstant(-300.0))),
        "MTM < -300 (-500 < -300)");

    // MTM_TOTAL works too
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makeTotal("mtm_total"), "<", makeConstant(-300.0))),
        "mtm_total < -300");

    // net_premium > 1000
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makeTotal("net_premium"), ">",
                               makeConstant(1000.0))),
        "net_premium > 1000 (1200 > 1000)");

    // net_delta < 0.5
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makeTotal("net_delta"), "<",
                               makeConstant(0.5))),
        "net_delta < 0.5 (0.3 < 0.5)");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: AND Branches
// ═══════════════════════════════════════════════════════════════════

void testAndBranches() {
    ConditionEvaluator eval;
    eval.setIndicatorValue("REF_1", "RSI_14", 25.0);

    ConditionEvaluator::PriceData ref;
    ref.ltp = 22500.0;
    eval.setPrice("REF_1", ref);

    eval.setIndicatorValue("REF_1", "SMA_20", 22400.0);

    // AND: RSI < 30 AND LTP > SMA → true AND true → true
    auto cond = makeAnd({
        makeLeaf(makeIndicator("REF_1", "RSI_14"), "<", makeConstant(30.0)),
        makeLeaf(makePrice("REF_1", "ltp"), ">",
                 makeIndicator("REF_1", "SMA_20")),
    });
    ASSERT_TRUE(eval.evaluate(cond), "AND: RSI<30 && LTP>SMA → true");

    // AND: RSI < 30 AND LTP > 23000 → true AND false → false
    auto cond2 = makeAnd({
        makeLeaf(makeIndicator("REF_1", "RSI_14"), "<", makeConstant(30.0)),
        makeLeaf(makePrice("REF_1", "ltp"), ">", makeConstant(23000.0)),
    });
    ASSERT_FALSE(eval.evaluate(cond2), "AND: RSI<30 && LTP>23000 → false");

    // AND with empty children → false
    ConditionNode emptyAnd;
    emptyAnd.nodeType = ConditionNode::NodeType::And;
    ASSERT_FALSE(eval.evaluate(emptyAnd), "AND with no children → false");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: OR Branches
// ═══════════════════════════════════════════════════════════════════

void testOrBranches() {
    ConditionEvaluator eval;
    eval.setIndicatorValue("REF_1", "RSI_14", 75.0);

    ConditionEvaluator::PriceData ref;
    ref.ltp = 22500.0;
    eval.setPrice("REF_1", ref);

    // OR: RSI > 70 OR LTP > 23000 → true OR false → true
    auto cond = makeOr({
        makeLeaf(makeIndicator("REF_1", "RSI_14"), ">", makeConstant(70.0)),
        makeLeaf(makePrice("REF_1", "ltp"), ">", makeConstant(23000.0)),
    });
    ASSERT_TRUE(eval.evaluate(cond), "OR: RSI>70 || LTP>23000 → true");

    // OR: RSI > 80 OR LTP > 23000 → false OR false → false
    auto cond2 = makeOr({
        makeLeaf(makeIndicator("REF_1", "RSI_14"), ">", makeConstant(80.0)),
        makeLeaf(makePrice("REF_1", "ltp"), ">", makeConstant(23000.0)),
    });
    ASSERT_FALSE(eval.evaluate(cond2), "OR: RSI>80 || LTP>23000 → false");

    // OR with empty children → false
    ConditionNode emptyOr;
    emptyOr.nodeType = ConditionNode::NodeType::Or;
    ASSERT_FALSE(eval.evaluate(emptyOr), "OR with no children → false");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Nested Condition Trees
// ═══════════════════════════════════════════════════════════════════

void testNestedTrees() {
    ConditionEvaluator eval;

    eval.setIndicatorValue("REF_1", "RSI_14", 25.0);

    ConditionEvaluator::PriceData ref;
    ref.ltp = 22500.0;
    eval.setPrice("REF_1", ref);

    eval.setIndicatorValue("REF_1", "SMA_20", 22400.0);

    // AND
    //  ├── RSI < 30 (true)
    //  └── OR
    //      ├── LTP > SMA_20 (true)
    //      └── LTP > 23000  (false)
    auto nested = makeAnd({
        makeLeaf(makeIndicator("REF_1", "RSI_14"), "<", makeConstant(30.0)),
        makeOr({
            makeLeaf(makePrice("REF_1", "ltp"), ">",
                     makeIndicator("REF_1", "SMA_20")),
            makeLeaf(makePrice("REF_1", "ltp"), ">", makeConstant(23000.0)),
        }),
    });
    ASSERT_TRUE(eval.evaluate(nested),
                "Nested AND(RSI<30, OR(LTP>SMA, LTP>23000)) → true");

    // Same tree but RSI = 35 → first AND child fails → whole AND fails
    eval.setIndicatorValue("REF_1", "RSI_14", 35.0);
    ASSERT_FALSE(eval.evaluate(nested),
                 "Nested AND fails when RSI=35 (first child false)");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Crossover Detection
// ═══════════════════════════════════════════════════════════════════

void testCrossover() {
    ConditionEvaluator eval;

    // Simulate tick 1: RSI = 28 (below threshold 30)
    eval.setIndicatorValue("REF_1", "RSI_14", 28.0);
    auto crossAbove = makeLeaf(makeIndicator("REF_1", "RSI_14"),
                                "crosses_above", makeConstant(30.0));

    // First evaluation: no previous value → set both to current → no cross
    ASSERT_FALSE(eval.evaluate(crossAbove),
                 "crosses_above: first tick (no prev) → false");

    // Tick 2: RSI = 32 (crosses above 30)
    eval.setIndicatorValue("REF_1", "RSI_14", 32.0);
    ASSERT_TRUE(eval.evaluate(crossAbove),
                "crosses_above: 28→32 crosses 30 → true");

    // Tick 3: RSI = 35 (still above, no new cross)
    eval.setIndicatorValue("REF_1", "RSI_14", 35.0);
    ASSERT_FALSE(eval.evaluate(crossAbove),
                 "crosses_above: 32→35 (both above 30) → false");

    // Test crosses_below
    eval.resetCrossoverState();
    eval.setIndicatorValue("REF_1", "RSI_14", 72.0);
    auto crossBelow = makeLeaf(makeIndicator("REF_1", "RSI_14"),
                                "crosses_below", makeConstant(70.0));

    // First evaluation: initialize
    ASSERT_FALSE(eval.evaluate(crossBelow),
                 "crosses_below: first tick → false");

    // Tick 2: RSI = 68 (crosses below 70)
    eval.setIndicatorValue("REF_1", "RSI_14", 68.0);
    ASSERT_TRUE(eval.evaluate(crossBelow),
                "crosses_below: 72→68 crosses 70 → true");

    // Tick 3: RSI = 65 (still below)
    eval.setIndicatorValue("REF_1", "RSI_14", 65.0);
    ASSERT_FALSE(eval.evaluate(crossBelow),
                 "crosses_below: 68→65 (both below) → false");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Real-World Strategy Conditions
// ═══════════════════════════════════════════════════════════════════

void testRealWorldConditions() {
    ConditionEvaluator eval;

    // ── Scenario: RSI Oversold + Price Above SMA (buy signal) ──
    eval.setIndicatorValue("REF_1", "RSI_14", 28.0);

    ConditionEvaluator::PriceData ref;
    ref.ltp = 22500.0;
    eval.setPrice("REF_1", ref);

    eval.setIndicatorValue("REF_1", "SMA_200", 22000.0);

    auto buySignal = makeAnd({
        makeLeaf(makeIndicator("REF_1", "RSI_14"), "<", makeConstant(30.0)),
        makeLeaf(makePrice("REF_1", "ltp"), ">",
                 makeIndicator("REF_1", "SMA_200")),
    });
    ASSERT_TRUE(eval.evaluate(buySignal),
                "Buy signal: RSI<30 && LTP>SMA200 → true");

    // ── Scenario: Options exit — IV crush or MTM target ──
    ConditionEvaluator::PriceData opt;
    opt.iv = 15.0;
    eval.setPrice("TRADE_1", opt);
    eval.setPortfolioMtm(5000.0);

    auto exitSignal = makeOr({
        makeLeaf(makeGreek("TRADE_1", "iv"), "<", makeConstant(18.0)),
        makeLeaf(makeTotal("mtm"), ">", makeConstant(3000.0)),
    });
    ASSERT_TRUE(eval.evaluate(exitSignal),
                "Exit: IV<18 || MTM>3000 → true (both true)");

    // ── Scenario: Spread width check before entry ──
    ConditionEvaluator::PriceData spread;
    spread.bidPrice = 150.0;
    spread.askPrice = 153.0;
    eval.setPrice("TRADE_1", spread);

    auto spreadCheck = makeLeaf(makeSpread("TRADE_1"), "<", makeConstant(5.0));
    ASSERT_TRUE(eval.evaluate(spreadCheck), "Spread < 5 (3 < 5 = true)");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Edge Cases
// ═══════════════════════════════════════════════════════════════════

void testEdgeCases() {
    ConditionEvaluator eval;

    // Empty leaf node is not "isEmpty" (isLeaf returns true, isEmpty returns false)
    ConditionNode leaf;
    leaf.nodeType = ConditionNode::NodeType::Leaf;
    ASSERT_TRUE(leaf.isLeaf(), "leaf isLeaf()");
    ASSERT_FALSE(leaf.isEmpty(), "leaf isEmpty() = false");

    // Empty branch is isEmpty
    ConditionNode emptyBranch;
    emptyBranch.nodeType = ConditionNode::NodeType::And;
    ASSERT_FALSE(emptyBranch.isLeaf(), "branch isLeaf() = false");
    ASSERT_TRUE(emptyBranch.isEmpty(), "empty branch isEmpty() = true");

    // Single-child AND/OR
    auto singleAnd = makeAnd({
        makeLeaf(makeConstant(10), ">", makeConstant(5)),
    });
    ASSERT_TRUE(eval.evaluate(singleAnd), "Single-child AND → true");

    auto singleOr = makeOr({
        makeLeaf(makeConstant(1), ">", makeConstant(5)),
    });
    ASSERT_FALSE(eval.evaluate(singleOr), "Single-child OR (1>5) → false");

    // Many-child AND (all must be true)
    auto manyAnd = makeAnd({
        makeLeaf(makeConstant(10), ">", makeConstant(5)),
        makeLeaf(makeConstant(20), ">", makeConstant(15)),
        makeLeaf(makeConstant(30), ">", makeConstant(25)),
        makeLeaf(makeConstant(40), ">", makeConstant(35)),
    });
    ASSERT_TRUE(eval.evaluate(manyAnd), "4-child AND → all true");

    // Zero value comparisons
    ASSERT_FALSE(
        eval.evaluate(makeLeaf(makeConstant(0.0), ">", makeConstant(0.0))),
        "0 > 0 = false");
    ASSERT_TRUE(
        eval.evaluate(makeLeaf(makeConstant(0.0), ">=", makeConstant(0.0))),
        "0 >= 0 = true");
}

// ═══════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    qInfo() << "═══════════════════════════════════════════════════════";
    qInfo() << "  Condition Evaluation Unit Tests";
    qInfo() << "═══════════════════════════════════════════════════════";

    testBasicComparisons();
    testPriceOperands();
    testIndicatorOperands();
    testParamRefOperands();
    testFormulaOperands();
    testGreekOperands();
    testSpreadOperands();
    testTotalOperands();
    testAndBranches();
    testOrBranches();
    testNestedTrees();
    testCrossover();
    testRealWorldConditions();
    testEdgeCases();

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
