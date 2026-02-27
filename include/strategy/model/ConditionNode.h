#ifndef CONDITION_NODE_H
#define CONDITION_NODE_H

#include <QString>
#include <QVariant>
#include <QVector>

// ═══════════════════════════════════════════════════════════════════
// OPERAND
// One side of a condition comparison.
// Can be a price field, indicator value, or a named parameter
// that gets filled in at deploy time.
// ═══════════════════════════════════════════════════════════════════

struct Operand {
    enum class Type {
        Price,       // LTP / open / high / low / close of a symbol
        Indicator,   // RSI_14, SMA_20 … computed on a reference symbol
        Constant,    // fixed numeric value: 30, 70, 22000 …
        ParamRef,    // {{RSI_THRESHOLD}} → filled at deploy time

        // ── Formula (new) ──
        Formula,     // User-defined expression evaluated at runtime
                     // e.g. "ATR(REF_1, 14) * 2.5", "VWAP(REF_1) * 1.01"
                     // Stored in formulaExpression field

        // ── Options greeks & IV ──
        Greek,       // IV, Delta, Gamma, Theta, Vega, Rho of a symbol slot
        // ── Market microstructure ──
        Spread,      // Bid-Ask spread (or combined leg spread) of a symbol slot
        // ── Portfolio / strategy level ──
        Total,       // Portfolio-level MTM, net premium, net delta, etc.
    };

    Type type = Type::Constant;

    // Price / Greek / Spread / Total → symbolId  = "REF_1" | "TRADE_1" | "" (Total)
    //                                   field     = see per-type lists below
    // Indicator  → symbolId + indicatorId
    // Constant   → constantValue
    // ParamRef   → paramName
    // Formula    → formulaExpression (evaluated by FormulaEngine at runtime)

    // Shared fields
    QString symbolId;       // symbol slot id this operand belongs to
    QString field;          // Price: "ltp|open|high|low|close"
                            // Greek: "iv|delta|gamma|theta|vega|rho"
                            // Spread:"bid_ask|leg_spread|net_spread"
                            // Total: "mtm|net_premium|net_delta|net_qty"
    QString indicatorId;    // Indicator type only  (e.g. "RSI_1", "MACD_1")
    QString outputSeries;   // Indicator multi-output (e.g. "macd", "signal", "upper")
                            // Empty = use the single / default output
    double  constantValue = 0.0;
    QString paramName;      // ParamRef type only

    // ── Formula expression (used when type == Formula) ──
    // User-defined arithmetic expression evaluated against live data.
    // Syntax: see FormulaEngine.h for full grammar.
    // Examples:
    //   "ATR(REF_1, 14) * 2.5"
    //   "VWAP(REF_1) * (1 + OFFSET_PCT / 100)"
    //   "MAX(LTP(TRADE_1), SMA(REF_1, 20))"
    //   "IV(TRADE_1) > 25 ? LTP(TRADE_1) * 0.98 : LTP(TRADE_1) * 0.95"
    QString formulaExpression;
};

// ═══════════════════════════════════════════════════════════════════
// CONDITION NODE
// A node in a recursive condition tree.
//
// NodeType::And  → all children must be true
// NodeType::Or   → at least one child must be true
// NodeType::Leaf → evaluate: left  operator  right
//
// Example tree:
//   AND
//   ├── RSI_14 (of REF_1) < 30          [Leaf]
//   └── OR
//       ├── LTP (of TRADE_1) > SMA_20   [Leaf]
//       └── LTP (of TRADE_1) > 22000    [Leaf]
// ═══════════════════════════════════════════════════════════════════

struct ConditionNode {
    enum class NodeType {
        And,   // logical AND of all children
        Or,    // logical OR of all children
        Leaf   // single comparison
    };

    NodeType nodeType = NodeType::Leaf;

    // ── Leaf fields (used when nodeType == Leaf) ──
    Operand left;
    Operand right;

    // Supported operators:
    //   ">"  ">="  "<"  "<="  "=="  "!="
    //   "crosses_above"   (left crossed above right on this candle)
    //   "crosses_below"   (left crossed below right on this candle)
    QString op;

    // ── Branch fields (used when nodeType == And | Or) ──
    QVector<ConditionNode> children;

    // ── Helpers ──
    bool isLeaf() const { return nodeType == NodeType::Leaf; }
    bool isEmpty() const {
        return isLeaf() ? false : children.isEmpty();
    }
};

#endif // CONDITION_NODE_H
