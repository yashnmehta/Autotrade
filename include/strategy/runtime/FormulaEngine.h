#ifndef FORMULA_ENGINE_H
#define FORMULA_ENGINE_H

/**
 * @file FormulaEngine.h
 * @brief Runtime formula/expression evaluator for strategy templates
 *
 * Parses and evaluates user-defined formulas that reference live market data,
 * computed indicators, and strategy parameters.
 *
 * ═══════════════════════════════════════════════════════════════════
 * FORMULA SYNTAX
 * ═══════════════════════════════════════════════════════════════════
 *
 * ── Literals ──
 *   42, 3.14, -0.5, 1e3
 *
 * ── Parameter references (from deploy-time TemplateParam values) ──
 *   RSI_PERIOD           → resolves to the deployed value (e.g. 14)
 *   OFFSET_PCT           → resolves to the deployed value (e.g. 0.5)
 *
 * ── Market data functions ──
 *   LTP(symbol_id)       → last traded price of the bound symbol
 *   OPEN(symbol_id)      → today's open
 *   HIGH(symbol_id)      → today's high (or N-candle high)
 *   LOW(symbol_id)       → today's low
 *   CLOSE(symbol_id)     → last close
 *   VOLUME(symbol_id)    → today's volume
 *   BID(symbol_id)       → best bid price
 *   ASK(symbol_id)       → best ask price
 *   CHANGE_PCT(symbol_id)→ % change from previous close
 *
 * ── Indicator functions ──
 *   RSI(symbol_id, period)     → current RSI value
 *   SMA(symbol_id, period)     → current SMA value
 *   EMA(symbol_id, period)     → current EMA value
 *   ATR(symbol_id, period)     → current ATR value
 *   VWAP(symbol_id)            → current VWAP
 *   BBANDS_UPPER(symbol_id, period)  → Bollinger upper band
 *   BBANDS_LOWER(symbol_id, period)  → Bollinger lower band
 *   MACD(symbol_id, fast, slow)      → MACD line value
 *   MACD_SIGNAL(symbol_id, fast, slow, signal) → signal line
 *
 * ── Greeks (options) ──
 *   IV(symbol_id)        → implied volatility
 *   DELTA(symbol_id)     → option delta
 *   GAMMA(symbol_id)     → option gamma
 *   THETA(symbol_id)     → option theta
 *   VEGA(symbol_id)      → option vega
 *
 * ── Portfolio functions ──
 *   MTM()                → total mark-to-market P&L
 *   NET_PREMIUM()        → net premium collected/paid
 *   NET_DELTA()          → portfolio delta
 *
 * ── Arithmetic ──
 *   +  -  *  /  %  ^(power)
 *   Parentheses: ( )
 *
 * ── Comparison (return 1.0 for true, 0.0 for false) ──
 *   >  >=  <  <=  ==  !=
 *
 * ── Logical (operands are truthy if != 0) ──
 *   &&  ||  !
 *
 * ── Ternary ──
 *   condition ? trueExpr : falseExpr
 *
 * ── Built-in math functions ──
 *   ABS(x)  MAX(a,b)  MIN(a,b)  ROUND(x)  FLOOR(x)  CEIL(x)
 *   SQRT(x) LOG(x)    POW(a,b)  CLAMP(x, lo, hi)
 *
 * ═══════════════════════════════════════════════════════════════════
 * USAGE EXAMPLE
 * ═══════════════════════════════════════════════════════════════════
 *
 *   FormulaEngine engine;
 *
 *   // Set deployed parameter values
 *   engine.setParam("RSI_PERIOD", 14.0);
 *   engine.setParam("OFFSET_PCT", 0.5);
 *
 *   // Provide a context that resolves live market data + indicators
 *   engine.setContext(&myFormulaContext);
 *
 *   // Evaluate
 *   bool ok;
 *   double sl = engine.evaluate("ATR(REF_1, 14) * 2.5", &ok);
 *   double trigger = engine.evaluate("VWAP(REF_1) * (1 + OFFSET_PCT / 100)", &ok);
 *   double adaptive = engine.evaluate("IV(TRADE_1) > 25 ? 0.02 : 0.01", &ok);
 */

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <functional>
#include <memory>

// ═══════════════════════════════════════════════════════════════════
// FORMULA CONTEXT — abstract interface for live data access
// ═══════════════════════════════════════════════════════════════════

class FormulaContext {
public:
    virtual ~FormulaContext() = default;

    // ── Market data ──
    // symbolId is the template-scoped slot ID ("REF_1", "TRADE_1")
    virtual double ltp(const QString &symbolId) const = 0;
    virtual double open(const QString &symbolId) const = 0;
    virtual double high(const QString &symbolId) const = 0;
    virtual double low(const QString &symbolId) const = 0;
    virtual double close(const QString &symbolId) const = 0;
    virtual double volume(const QString &symbolId) const = 0;
    virtual double bid(const QString &symbolId) const = 0;
    virtual double ask(const QString &symbolId) const = 0;
    virtual double changePct(const QString &symbolId) const = 0;

    // ── Indicators ──
    // Returns the current computed value of a standard indicator.
    // period/period2/period3 are the indicator parameters.
    virtual double indicator(const QString &symbolId,
                             const QString &indicatorType,
                             int period,
                             int period2 = 0,
                             int period3 = 0) const = 0;

    // ── Greeks ──
    virtual double iv(const QString &symbolId) const = 0;
    virtual double delta(const QString &symbolId) const = 0;
    virtual double gamma(const QString &symbolId) const = 0;
    virtual double theta(const QString &symbolId) const = 0;
    virtual double vega(const QString &symbolId) const = 0;

    // ── Portfolio-level ──
    virtual double mtm() const = 0;
    virtual double netPremium() const = 0;
    virtual double netDelta() const = 0;
};

// ═══════════════════════════════════════════════════════════════════
// TOKEN — internal representation of parsed formula elements
// ═══════════════════════════════════════════════════════════════════

struct FormulaToken {
    enum Type {
        Number,       // literal numeric value
        Identifier,   // parameter name or function name
        StringArg,    // string argument (e.g. symbol ID inside function)
        Operator,     // + - * / % ^ > < >= <= == != && || !
        LParen,       // (
        RParen,       // )
        Comma,        // ,
        Question,     // ? (ternary)
        Colon,        // : (ternary)
        End           // end of expression
    };

    Type    type = End;
    double  numVal = 0.0;
    QString strVal;
};

// ═══════════════════════════════════════════════════════════════════
// AST NODE — parsed expression tree
// ═══════════════════════════════════════════════════════════════════

struct FormulaASTNode {
    enum Kind {
        Literal,        // constant number
        ParamRef,       // named parameter reference
        BinaryOp,       // left op right
        UnaryOp,        // -expr or !expr
        FunctionCall,   // FUNC(args...)
        Ternary,        // cond ? trueExpr : falseExpr
    };

    Kind kind = Literal;
    double value = 0.0;       // Literal
    QString name;             // ParamRef name, FunctionCall name, BinaryOp/UnaryOp operator
    std::shared_ptr<FormulaASTNode> left;    // BinaryOp left, UnaryOp operand, Ternary condition
    std::shared_ptr<FormulaASTNode> right;   // BinaryOp right, Ternary falseExpr
    std::shared_ptr<FormulaASTNode> middle;  // Ternary trueExpr
    QVector<std::shared_ptr<FormulaASTNode>> args; // FunctionCall arguments
};

using ASTNodePtr = std::shared_ptr<FormulaASTNode>;

// ═══════════════════════════════════════════════════════════════════
// FORMULA ENGINE
// ═══════════════════════════════════════════════════════════════════

class FormulaEngine {
public:
    FormulaEngine();
    ~FormulaEngine() = default;

    // ── Context ──
    void setContext(FormulaContext *ctx);

    // ── Parameter values (from deploy-time configuration) ──
    void setParam(const QString &name, double value);
    void setParams(const QHash<QString, double> &params);
    void clearParams();
    double param(const QString &name) const;
    bool hasParam(const QString &name) const;

    // ── Evaluate ──
    // Parse and evaluate an expression string.
    // Returns the numeric result. Sets *ok = false on error.
    double evaluate(const QString &expression, bool *ok = nullptr) const;

    // ── Validate (parse-only, no evaluation) ──
    // Returns true if the expression is syntactically valid.
    // Sets errorMsg to a human-readable error description on failure.
    bool validate(const QString &expression, QString *errorMsg = nullptr) const;

    // ── Introspection ──
    // Extract all parameter names referenced in an expression.
    QStringList referencedParams(const QString &expression) const;
    // Extract all symbol IDs referenced in function calls.
    QStringList referencedSymbols(const QString &expression) const;

    // ── Last error ──
    QString lastError() const { return m_lastError; }

private:
    // ── Tokenizer ──
    QVector<FormulaToken> tokenize(const QString &expr, bool *ok) const;

    // ── Parser (recursive descent) ──
    ASTNodePtr parse(const QVector<FormulaToken> &tokens, bool *ok) const;
    ASTNodePtr parseTernary(const QVector<FormulaToken> &tokens, int &pos, bool *ok) const;
    ASTNodePtr parseOr(const QVector<FormulaToken> &tokens, int &pos, bool *ok) const;
    ASTNodePtr parseAnd(const QVector<FormulaToken> &tokens, int &pos, bool *ok) const;
    ASTNodePtr parseComparison(const QVector<FormulaToken> &tokens, int &pos, bool *ok) const;
    ASTNodePtr parseAddSub(const QVector<FormulaToken> &tokens, int &pos, bool *ok) const;
    ASTNodePtr parseMulDiv(const QVector<FormulaToken> &tokens, int &pos, bool *ok) const;
    ASTNodePtr parsePower(const QVector<FormulaToken> &tokens, int &pos, bool *ok) const;
    ASTNodePtr parseUnary(const QVector<FormulaToken> &tokens, int &pos, bool *ok) const;
    ASTNodePtr parsePrimary(const QVector<FormulaToken> &tokens, int &pos, bool *ok) const;

    // ── AST evaluator ──
    double eval(const ASTNodePtr &node, bool *ok) const;
    double callFunction(const QString &name,
                        const QVector<ASTNodePtr> &args,
                        bool *ok) const;

    // ── AST introspection ──
    void collectParamRefs(const ASTNodePtr &node, QStringList &out) const;
    void collectSymbolRefs(const ASTNodePtr &node, QStringList &out) const;

    // ── Data ──
    FormulaContext             *m_context = nullptr;
    QHash<QString, double>      m_params;
    mutable QString             m_lastError;
};

#endif // FORMULA_ENGINE_H
