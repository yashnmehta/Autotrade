/**
 * @file FormulaEngine.cpp
 * @brief Runtime formula/expression evaluator implementation
 *
 * Implements a recursive-descent parser for the formula language described
 * in FormulaEngine.h. Produces an AST which is then evaluated against
 * live market data via the FormulaContext interface.
 *
 * Grammar (precedence low → high):
 *   ternary     := or ('?' ternary ':' ternary)?
 *   or          := and ('||' and)*
 *   and         := comparison ('&&' comparison)*
 *   comparison  := addSub (('>'|'>='|'<'|'<='|'=='|'!=') addSub)?
 *   addSub      := mulDiv (('+' | '-') mulDiv)*
 *   mulDiv      := power (('*' | '/' | '%') power)*
 *   power       := unary ('^' unary)?
 *   unary       := ('-' | '!') unary | primary
 *   primary     := NUMBER
 *                | IDENT '(' argList ')'     — function call
 *                | IDENT                     — parameter reference
 *                | '(' ternary ')'
 *   argList     := ternary (',' ternary)*
 */

#include "strategy/runtime/FormulaEngine.h"
#include <QDebug>
#include <QtMath>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════════

FormulaEngine::FormulaEngine() {}

void FormulaEngine::setContext(FormulaContext *ctx) { m_context = ctx; }

void FormulaEngine::setParam(const QString &name, double value) {
  m_params[name.toUpper()] = value;
}

void FormulaEngine::setParams(const QHash<QString, double> &params) {
  for (auto it = params.begin(); it != params.end(); ++it)
    m_params[it.key().toUpper()] = it.value();
}

void FormulaEngine::clearParams() { m_params.clear(); }

double FormulaEngine::param(const QString &name) const {
  return m_params.value(name.toUpper(), 0.0);
}

bool FormulaEngine::hasParam(const QString &name) const {
  return m_params.contains(name.toUpper());
}

// ═══════════════════════════════════════════════════════════════════
// TOKENIZER
// ═══════════════════════════════════════════════════════════════════

QVector<FormulaToken> FormulaEngine::tokenize(const QString &expr,
                                              bool *ok) const {
  QVector<FormulaToken> tokens;
  int i = 0;
  int len = expr.length();

  while (i < len) {
    QChar ch = expr[i];

    // Skip whitespace
    if (ch.isSpace()) {
      ++i;
      continue;
    }

    // Numbers: 123, 3.14, 1e5, -1.2e-3 (but minus is handled as unary op)
    if (ch.isDigit() || (ch == '.' && i + 1 < len && expr[i + 1].isDigit())) {
      int start = i;
      while (i < len && (expr[i].isDigit() || expr[i] == '.'))
        ++i;
      // Scientific notation
      if (i < len && (expr[i] == 'e' || expr[i] == 'E')) {
        ++i;
        if (i < len && (expr[i] == '+' || expr[i] == '-'))
          ++i;
        while (i < len && expr[i].isDigit())
          ++i;
      }
      FormulaToken tok;
      tok.type = FormulaToken::Number;
      tok.numVal = expr.mid(start, i - start).toDouble();
      tokens.append(tok);
      continue;
    }

    // Identifiers: [A-Za-z_][A-Za-z0-9_]*
    if (ch.isLetter() || ch == '_') {
      int start = i;
      while (i < len && (expr[i].isLetterOrNumber() || expr[i] == '_'))
        ++i;
      FormulaToken tok;
      tok.type = FormulaToken::Identifier;
      tok.strVal = expr.mid(start, i - start).toUpper();
      tokens.append(tok);
      continue;
    }

    // Two-character operators
    if (i + 1 < len) {
      QString two = expr.mid(i, 2);
      if (two == ">=" || two == "<=" || two == "==" || two == "!=" ||
          two == "&&" || two == "||") {
        FormulaToken tok;
        tok.type = FormulaToken::Operator;
        tok.strVal = two;
        tokens.append(tok);
        i += 2;
        continue;
      }
    }

    // Single-character operators and punctuation
    if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' ||
        ch == '^' || ch == '>' || ch == '<' || ch == '!') {
      FormulaToken tok;
      tok.type = FormulaToken::Operator;
      tok.strVal = ch;
      tokens.append(tok);
      ++i;
      continue;
    }

    if (ch == '(') {
      FormulaToken t;
      t.type = FormulaToken::LParen;
      tokens.append(t);
      ++i;
      continue;
    }
    if (ch == ')') {
      FormulaToken t;
      t.type = FormulaToken::RParen;
      tokens.append(t);
      ++i;
      continue;
    }
    if (ch == ',') {
      FormulaToken t;
      t.type = FormulaToken::Comma;
      tokens.append(t);
      ++i;
      continue;
    }
    if (ch == '?') {
      FormulaToken t;
      t.type = FormulaToken::Question;
      tokens.append(t);
      ++i;
      continue;
    }
    if (ch == ':') {
      FormulaToken t;
      t.type = FormulaToken::Colon;
      tokens.append(t);
      ++i;
      continue;
    }

    // Unknown character
    m_lastError =
        QString("Unexpected character '%1' at position %2").arg(ch).arg(i);
    if (ok)
      *ok = false;
    return {};
  }

  FormulaToken endTok;
  endTok.type = FormulaToken::End;
  tokens.append(endTok);

  if (ok)
    *ok = true;
  return tokens;
}

// ═══════════════════════════════════════════════════════════════════
// PARSER — recursive descent producing AST
// ═══════════════════════════════════════════════════════════════════

ASTNodePtr FormulaEngine::parse(const QVector<FormulaToken> &tokens,
                                bool *ok) const {
  int pos = 0;
  ASTNodePtr node = parseTernary(tokens, pos, ok);
  if (ok && *ok && tokens[pos].type != FormulaToken::End) {
    m_lastError =
        QString("Unexpected token after expression at position %1").arg(pos);
    *ok = false;
    return nullptr;
  }
  return node;
}

ASTNodePtr FormulaEngine::parseTernary(const QVector<FormulaToken> &tokens,
                                       int &pos, bool *ok) const {
  ASTNodePtr cond = parseOr(tokens, pos, ok);
  if (!ok || !*ok)
    return nullptr;

  if (tokens[pos].type == FormulaToken::Question) {
    ++pos; // consume '?'
    ASTNodePtr trueExpr = parseTernary(tokens, pos, ok);
    if (!ok || !*ok)
      return nullptr;

    if (tokens[pos].type != FormulaToken::Colon) {
      m_lastError = "Expected ':' in ternary expression";
      if (ok)
        *ok = false;
      return nullptr;
    }
    ++pos; // consume ':'

    ASTNodePtr falseExpr = parseTernary(tokens, pos, ok);
    if (!ok || !*ok)
      return nullptr;

    auto node = std::make_shared<FormulaASTNode>();
    node->kind = FormulaASTNode::Ternary;
    node->left = cond;
    node->middle = trueExpr;
    node->right = falseExpr;
    return node;
  }
  return cond;
}

ASTNodePtr FormulaEngine::parseOr(const QVector<FormulaToken> &tokens, int &pos,
                                  bool *ok) const {
  ASTNodePtr left = parseAnd(tokens, pos, ok);
  if (!ok || !*ok)
    return nullptr;

  while (tokens[pos].type == FormulaToken::Operator &&
         tokens[pos].strVal == "||") {
    ++pos;
    ASTNodePtr right = parseAnd(tokens, pos, ok);
    if (!ok || !*ok)
      return nullptr;
    auto node = std::make_shared<FormulaASTNode>();
    node->kind = FormulaASTNode::BinaryOp;
    node->name = "||";
    node->left = left;
    node->right = right;
    left = node;
  }
  return left;
}

ASTNodePtr FormulaEngine::parseAnd(const QVector<FormulaToken> &tokens,
                                   int &pos, bool *ok) const {
  ASTNodePtr left = parseComparison(tokens, pos, ok);
  if (!ok || !*ok)
    return nullptr;

  while (tokens[pos].type == FormulaToken::Operator &&
         tokens[pos].strVal == "&&") {
    ++pos;
    ASTNodePtr right = parseComparison(tokens, pos, ok);
    if (!ok || !*ok)
      return nullptr;
    auto node = std::make_shared<FormulaASTNode>();
    node->kind = FormulaASTNode::BinaryOp;
    node->name = "&&";
    node->left = left;
    node->right = right;
    left = node;
  }
  return left;
}

ASTNodePtr FormulaEngine::parseComparison(const QVector<FormulaToken> &tokens,
                                          int &pos, bool *ok) const {
  ASTNodePtr left = parseAddSub(tokens, pos, ok);
  if (!ok || !*ok)
    return nullptr;

  if (tokens[pos].type == FormulaToken::Operator) {
    QString op = tokens[pos].strVal;
    if (op == ">" || op == ">=" || op == "<" || op == "<=" || op == "==" ||
        op == "!=") {
      ++pos;
      ASTNodePtr right = parseAddSub(tokens, pos, ok);
      if (!ok || !*ok)
        return nullptr;
      auto node = std::make_shared<FormulaASTNode>();
      node->kind = FormulaASTNode::BinaryOp;
      node->name = op;
      node->left = left;
      node->right = right;
      return node;
    }
  }
  return left;
}

ASTNodePtr FormulaEngine::parseAddSub(const QVector<FormulaToken> &tokens,
                                      int &pos, bool *ok) const {
  ASTNodePtr left = parseMulDiv(tokens, pos, ok);
  if (!ok || !*ok)
    return nullptr;

  while (tokens[pos].type == FormulaToken::Operator &&
         (tokens[pos].strVal == "+" || tokens[pos].strVal == "-")) {
    QString op = tokens[pos].strVal;
    ++pos;
    ASTNodePtr right = parseMulDiv(tokens, pos, ok);
    if (!ok || !*ok)
      return nullptr;
    auto node = std::make_shared<FormulaASTNode>();
    node->kind = FormulaASTNode::BinaryOp;
    node->name = op;
    node->left = left;
    node->right = right;
    left = node;
  }
  return left;
}

ASTNodePtr FormulaEngine::parseMulDiv(const QVector<FormulaToken> &tokens,
                                      int &pos, bool *ok) const {
  ASTNodePtr left = parsePower(tokens, pos, ok);
  if (!ok || !*ok)
    return nullptr;

  while (tokens[pos].type == FormulaToken::Operator &&
         (tokens[pos].strVal == "*" || tokens[pos].strVal == "/" ||
          tokens[pos].strVal == "%")) {
    QString op = tokens[pos].strVal;
    ++pos;
    ASTNodePtr right = parsePower(tokens, pos, ok);
    if (!ok || !*ok)
      return nullptr;
    auto node = std::make_shared<FormulaASTNode>();
    node->kind = FormulaASTNode::BinaryOp;
    node->name = op;
    node->left = left;
    node->right = right;
    left = node;
  }
  return left;
}

ASTNodePtr FormulaEngine::parsePower(const QVector<FormulaToken> &tokens,
                                     int &pos, bool *ok) const {
  ASTNodePtr left = parseUnary(tokens, pos, ok);
  if (!ok || !*ok)
    return nullptr;

  if (tokens[pos].type == FormulaToken::Operator && tokens[pos].strVal == "^") {
    ++pos;
    ASTNodePtr right = parseUnary(tokens, pos, ok); // right-associative
    if (!ok || !*ok)
      return nullptr;
    auto node = std::make_shared<FormulaASTNode>();
    node->kind = FormulaASTNode::BinaryOp;
    node->name = "^";
    node->left = left;
    node->right = right;
    return node;
  }
  return left;
}

ASTNodePtr FormulaEngine::parseUnary(const QVector<FormulaToken> &tokens,
                                     int &pos, bool *ok) const {
  if (tokens[pos].type == FormulaToken::Operator) {
    if (tokens[pos].strVal == "-") {
      ++pos;
      ASTNodePtr operand = parseUnary(tokens, pos, ok);
      if (!ok || !*ok)
        return nullptr;
      auto node = std::make_shared<FormulaASTNode>();
      node->kind = FormulaASTNode::UnaryOp;
      node->name = "-";
      node->left = operand;
      return node;
    }
    if (tokens[pos].strVal == "!") {
      ++pos;
      ASTNodePtr operand = parseUnary(tokens, pos, ok);
      if (!ok || !*ok)
        return nullptr;
      auto node = std::make_shared<FormulaASTNode>();
      node->kind = FormulaASTNode::UnaryOp;
      node->name = "!";
      node->left = operand;
      return node;
    }
  }
  return parsePrimary(tokens, pos, ok);
}

ASTNodePtr FormulaEngine::parsePrimary(const QVector<FormulaToken> &tokens,
                                       int &pos, bool *ok) const {
  const FormulaToken &tok = tokens[pos];

  // Number literal
  if (tok.type == FormulaToken::Number) {
    ++pos;
    auto node = std::make_shared<FormulaASTNode>();
    node->kind = FormulaASTNode::Literal;
    node->value = tok.numVal;
    return node;
  }

  // Parenthesized expression
  if (tok.type == FormulaToken::LParen) {
    ++pos; // consume '('
    ASTNodePtr inner = parseTernary(tokens, pos, ok);
    if (!ok || !*ok)
      return nullptr;
    if (tokens[pos].type != FormulaToken::RParen) {
      m_lastError = QString("Expected ')' at position %1").arg(pos);
      if (ok)
        *ok = false;
      return nullptr;
    }
    ++pos; // consume ')'
    return inner;
  }

  // Identifier: could be a function call or a parameter reference
  if (tok.type == FormulaToken::Identifier) {
    QString name = tok.strVal;
    ++pos;

    // Check if it's a function call: IDENT '(' args ')'
    if (pos < tokens.size() && tokens[pos].type == FormulaToken::LParen) {
      ++pos; // consume '('
      QVector<ASTNodePtr> args;

      if (tokens[pos].type != FormulaToken::RParen) {
        // Parse first argument
        args.append(parseTernary(tokens, pos, ok));
        if (!ok || !*ok)
          return nullptr;

        // Parse remaining comma-separated arguments
        while (tokens[pos].type == FormulaToken::Comma) {
          ++pos; // consume ','
          args.append(parseTernary(tokens, pos, ok));
          if (!ok || !*ok)
            return nullptr;
        }
      }

      if (tokens[pos].type != FormulaToken::RParen) {
        m_lastError =
            QString("Expected ')' after function arguments for '%1'").arg(name);
        if (ok)
          *ok = false;
        return nullptr;
      }
      ++pos; // consume ')'

      auto node = std::make_shared<FormulaASTNode>();
      node->kind = FormulaASTNode::FunctionCall;
      node->name = name;
      node->args = args;
      return node;
    }

    // Not a function call → parameter reference
    auto node = std::make_shared<FormulaASTNode>();
    node->kind = FormulaASTNode::ParamRef;
    node->name = name;
    return node;
  }

  m_lastError = QString("Unexpected token at position %1").arg(pos);
  if (ok)
    *ok = false;
  return nullptr;
}

// ═══════════════════════════════════════════════════════════════════
// AST EVALUATOR
// ═══════════════════════════════════════════════════════════════════

double FormulaEngine::eval(const ASTNodePtr &node, bool *ok) const {
  if (!node) {
    m_lastError = "Null AST node";
    if (ok)
      *ok = false;
    return 0.0;
  }

  switch (node->kind) {
  case FormulaASTNode::Literal:
    return node->value;

  case FormulaASTNode::ParamRef: {
    if (m_params.contains(node->name))
      return m_params[node->name];
    // Some special constant names
    if (node->name == "PI")
      return M_PI;
    if (node->name == "E")
      return M_E;
    if (node->name == "TRUE")
      return 1.0;
    if (node->name == "FALSE")
      return 0.0;
    m_lastError = QString("Unknown parameter: '%1'").arg(node->name);
    if (ok)
      *ok = false;
    return 0.0;
  }

  case FormulaASTNode::UnaryOp: {
    double val = eval(node->left, ok);
    if (ok && !*ok)
      return 0.0;
    if (node->name == "-")
      return -val;
    if (node->name == "!")
      return (val == 0.0) ? 1.0 : 0.0;
    return 0.0;
  }

  case FormulaASTNode::BinaryOp: {
    double l = eval(node->left, ok);
    if (ok && !*ok)
      return 0.0;

    // Short-circuit for logical operators
    if (node->name == "&&") {
      if (l == 0.0)
        return 0.0;
      double r = eval(node->right, ok);
      return (r != 0.0) ? 1.0 : 0.0;
    }
    if (node->name == "||") {
      if (l != 0.0)
        return 1.0;
      double r = eval(node->right, ok);
      return (r != 0.0) ? 1.0 : 0.0;
    }

    double r = eval(node->right, ok);
    if (ok && !*ok)
      return 0.0;

    if (node->name == "+")
      return l + r;
    if (node->name == "-")
      return l - r;
    if (node->name == "*")
      return l * r;
    if (node->name == "/") {
      if (r == 0.0) {
        m_lastError = "Division by zero";
        if (ok)
          *ok = false;
        return 0.0;
      }
      return l / r;
    }
    if (node->name == "%") {
      if (r == 0.0) {
        m_lastError = "Modulo by zero";
        if (ok)
          *ok = false;
        return 0.0;
      }
      return std::fmod(l, r);
    }
    if (node->name == "^")
      return std::pow(l, r);
    if (node->name == ">")
      return (l > r) ? 1.0 : 0.0;
    if (node->name == ">=")
      return (l >= r) ? 1.0 : 0.0;
    if (node->name == "<")
      return (l < r) ? 1.0 : 0.0;
    if (node->name == "<=")
      return (l <= r) ? 1.0 : 0.0;
    if (node->name == "==")
      return (l == r) ? 1.0 : 0.0;
    if (node->name == "!=")
      return (l != r) ? 1.0 : 0.0;

    m_lastError = QString("Unknown operator: '%1'").arg(node->name);
    if (ok)
      *ok = false;
    return 0.0;
  }

  case FormulaASTNode::FunctionCall:
    return callFunction(node->name, node->args, ok);

  case FormulaASTNode::Ternary: {
    double cond = eval(node->left, ok);
    if (ok && !*ok)
      return 0.0;
    if (cond != 0.0)
      return eval(node->middle, ok);
    else
      return eval(node->right, ok);
  }
  }

  return 0.0;
}

// ═══════════════════════════════════════════════════════════════════
// FUNCTION CALL DISPATCH
// ═══════════════════════════════════════════════════════════════════

// Helper: evaluate an AST node that should be a string-like identifier
// (for symbol IDs passed to market data functions).
// If the node is a ParamRef, return its name as a string (the symbol slot ID).
// If it's a literal or function call, evaluate it normally.
static QString argAsSymbolId(const ASTNodePtr &arg) {
  if (arg && arg->kind == FormulaASTNode::ParamRef)
    return arg->name;
  return {};
}

double FormulaEngine::callFunction(const QString &name,
                                   const QVector<ASTNodePtr> &args,
                                   bool *ok) const {
  // ── Built-in math functions ──
  if (name == "ABS") {
    if (args.size() != 1) {
      m_lastError = "ABS() expects 1 argument";
      if (ok)
        *ok = false;
      return 0.0;
    }
    return std::abs(eval(args[0], ok));
  }
  if (name == "SQRT") {
    if (args.size() != 1) {
      m_lastError = "SQRT() expects 1 argument";
      if (ok)
        *ok = false;
      return 0.0;
    }
    double v = eval(args[0], ok);
    if (v < 0) {
      m_lastError = "SQRT of negative number";
      if (ok)
        *ok = false;
      return 0.0;
    }
    return std::sqrt(v);
  }
  if (name == "LOG") {
    if (args.size() != 1) {
      m_lastError = "LOG() expects 1 argument";
      if (ok)
        *ok = false;
      return 0.0;
    }
    double v = eval(args[0], ok);
    if (v <= 0) {
      m_lastError = "LOG of non-positive number";
      if (ok)
        *ok = false;
      return 0.0;
    }
    return std::log(v);
  }
  if (name == "ROUND") {
    if (args.size() != 1) {
      m_lastError = "ROUND() expects 1 argument";
      if (ok)
        *ok = false;
      return 0.0;
    }
    return std::round(eval(args[0], ok));
  }
  if (name == "FLOOR") {
    if (args.size() != 1) {
      m_lastError = "FLOOR() expects 1 argument";
      if (ok)
        *ok = false;
      return 0.0;
    }
    return std::floor(eval(args[0], ok));
  }
  if (name == "CEIL") {
    if (args.size() != 1) {
      m_lastError = "CEIL() expects 1 argument";
      if (ok)
        *ok = false;
      return 0.0;
    }
    return std::ceil(eval(args[0], ok));
  }
  if (name == "MAX") {
    if (args.size() != 2) {
      m_lastError = "MAX() expects 2 arguments";
      if (ok)
        *ok = false;
      return 0.0;
    }
    double a = eval(args[0], ok);
    if (ok && !*ok)
      return 0.0;
    double b = eval(args[1], ok);
    return std::max(a, b);
  }
  if (name == "MIN") {
    if (args.size() != 2) {
      m_lastError = "MIN() expects 2 arguments";
      if (ok)
        *ok = false;
      return 0.0;
    }
    double a = eval(args[0], ok);
    if (ok && !*ok)
      return 0.0;
    double b = eval(args[1], ok);
    return std::min(a, b);
  }
  if (name == "POW") {
    if (args.size() != 2) {
      m_lastError = "POW() expects 2 arguments";
      if (ok)
        *ok = false;
      return 0.0;
    }
    double a = eval(args[0], ok);
    if (ok && !*ok)
      return 0.0;
    double b = eval(args[1], ok);
    return std::pow(a, b);
  }
  if (name == "CLAMP") {
    if (args.size() != 3) {
      m_lastError = "CLAMP() expects 3 arguments (x, lo, hi)";
      if (ok)
        *ok = false;
      return 0.0;
    }
    double x = eval(args[0], ok);
    if (ok && !*ok)
      return 0.0;
    double lo = eval(args[1], ok);
    if (ok && !*ok)
      return 0.0;
    double hi = eval(args[2], ok);
    return std::max(lo, std::min(x, hi));
  }
  if (name == "IF") {
    // IF(cond, trueVal, falseVal) — same as ternary but functional form
    if (args.size() != 3) {
      m_lastError = "IF() expects 3 arguments";
      if (ok)
        *ok = false;
      return 0.0;
    }
    double cond = eval(args[0], ok);
    if (ok && !*ok)
      return 0.0;
    return (cond != 0.0) ? eval(args[1], ok) : eval(args[2], ok);
  }

  // ── Market data functions (require FormulaContext) ──
  if (!m_context) {
    m_lastError =
        QString("No FormulaContext set — cannot evaluate '%1()'").arg(name);
    if (ok)
      *ok = false;
    return 0.0;
  }

  // ── Price functions: FUNC(symbol_id) ──
  if (name == "LTP" || name == "OPEN" || name == "HIGH" || name == "LOW" ||
      name == "CLOSE" || name == "VOLUME" || name == "BID" || name == "ASK" ||
      name == "CHANGE_PCT") {
    if (args.size() != 1) {
      m_lastError = QString("%1() expects 1 argument (symbol_id)").arg(name);
      if (ok)
        *ok = false;
      return 0.0;
    }
    QString symId = argAsSymbolId(args[0]);
    if (symId.isEmpty()) {
      m_lastError =
          QString("%1() argument must be a symbol ID (e.g. REF_1)").arg(name);
      if (ok)
        *ok = false;
      return 0.0;
    }

    if (name == "LTP")
      return m_context->ltp(symId);
    if (name == "OPEN")
      return m_context->open(symId);
    if (name == "HIGH")
      return m_context->high(symId);
    if (name == "LOW")
      return m_context->low(symId);
    if (name == "CLOSE")
      return m_context->close(symId);
    if (name == "VOLUME")
      return m_context->volume(symId);
    if (name == "BID")
      return m_context->bid(symId);
    if (name == "ASK")
      return m_context->ask(symId);
    if (name == "CHANGE_PCT")
      return m_context->changePct(symId);
  }

  // ── Greeks: FUNC(symbol_id) ──
  if (name == "IV" || name == "DELTA" || name == "GAMMA" || name == "THETA" ||
      name == "VEGA") {
    if (args.size() != 1) {
      m_lastError = QString("%1() expects 1 argument (symbol_id)").arg(name);
      if (ok)
        *ok = false;
      return 0.0;
    }
    QString symId = argAsSymbolId(args[0]);
    if (symId.isEmpty()) {
      m_lastError = QString("%1() argument must be a symbol ID").arg(name);
      if (ok)
        *ok = false;
      return 0.0;
    }

    if (name == "IV")
      return m_context->iv(symId);
    if (name == "DELTA")
      return m_context->delta(symId);
    if (name == "GAMMA")
      return m_context->gamma(symId);
    if (name == "THETA")
      return m_context->theta(symId);
    if (name == "VEGA")
      return m_context->vega(symId);
  }

  // ── Indicator functions: FUNC(symbol_id, period [, period2 [, period3]]) ──
  if (name == "RSI" || name == "SMA" || name == "EMA" || name == "ATR" ||
      name == "VWAP" || name == "BBANDS_UPPER" || name == "BBANDS_LOWER" ||
      name == "BBANDS_MIDDLE" || name == "MACD" || name == "MACD_SIGNAL" ||
      name == "MACD_HIST" || name == "ADX" || name == "OBV" ||
      name == "STOCH_K" || name == "STOCH_D") {
    if (args.size() < 1) {
      m_lastError =
          QString("%1() expects at least 1 argument (symbol_id)").arg(name);
      if (ok)
        *ok = false;
      return 0.0;
    }
    QString symId = argAsSymbolId(args[0]);
    if (symId.isEmpty()) {
      m_lastError =
          QString("%1() first argument must be a symbol ID").arg(name);
      if (ok)
        *ok = false;
      return 0.0;
    }

    int period = (args.size() > 1) ? (int)eval(args[1], ok) : 14;
    if (ok && !*ok)
      return 0.0;
    int period2 = (args.size() > 2) ? (int)eval(args[2], ok) : 0;
    if (ok && !*ok)
      return 0.0;
    int period3 = (args.size() > 3) ? (int)eval(args[3], ok) : 0;
    if (ok && !*ok)
      return 0.0;

    // Map compound function names to indicator type + output selector
    QString indType = name;
    if (name == "BBANDS_UPPER")
      indType = "BBANDS";
    if (name == "BBANDS_LOWER")
      indType = "BBANDS";
    if (name == "BBANDS_MIDDLE")
      indType = "BBANDS";
    if (name == "MACD_SIGNAL")
      indType = "MACD";
    if (name == "MACD_HIST")
      indType = "MACD";
    if (name == "STOCH_K")
      indType = "STOCH";
    if (name == "STOCH_D")
      indType = "STOCH";

    return m_context->indicator(symId, indType, period, period2, period3);
  }

  // ── Portfolio functions: FUNC() ──
  if (name == "MTM")
    return m_context->mtm();
  if (name == "NET_PREMIUM")
    return m_context->netPremium();
  if (name == "NET_DELTA")
    return m_context->netDelta();

  m_lastError = QString("Unknown function: '%1'").arg(name);
  if (ok)
    *ok = false;
  return 0.0;
}

// ═══════════════════════════════════════════════════════════════════
// PUBLIC INTERFACE
// ═══════════════════════════════════════════════════════════════════

double FormulaEngine::evaluate(const QString &expression, bool *ok) const {
  if (expression.trimmed().isEmpty()) {
    if (ok)
      *ok = true;
    return 0.0;
  }

  bool tokenOk = true;
  QVector<FormulaToken> tokens = tokenize(expression, &tokenOk);
  if (!tokenOk) {
    if (ok)
      *ok = false;
    return 0.0;
  }

  bool parseOk = true;
  ASTNodePtr ast = parse(tokens, &parseOk);
  if (!parseOk || !ast) {
    if (ok)
      *ok = false;
    return 0.0;
  }

  bool evalOk = true;
  double result = eval(ast, &evalOk);
  if (ok)
    *ok = evalOk;
  return result;
}

bool FormulaEngine::validate(const QString &expression,
                             QString *errorMsg) const {
  m_lastError.clear();
  bool ok = true;
  tokenize(expression, &ok);
  if (!ok) {
    if (errorMsg)
      *errorMsg = m_lastError;
    return false;
  }

  QVector<FormulaToken> tokens = tokenize(expression, &ok);
  parse(tokens, &ok);
  if (!ok) {
    if (errorMsg)
      *errorMsg = m_lastError;
    return false;
  }
  return true;
}

// ═══════════════════════════════════════════════════════════════════
// INTROSPECTION — extract referenced names from AST
// ═══════════════════════════════════════════════════════════════════

void FormulaEngine::collectParamRefs(const ASTNodePtr &node,
                                     QStringList &out) const {
  if (!node)
    return;
  if (node->kind == FormulaASTNode::ParamRef) {
    if (!out.contains(node->name))
      out.append(node->name);
  }
  collectParamRefs(node->left, out);
  collectParamRefs(node->right, out);
  collectParamRefs(node->middle, out);
  for (const auto &arg : node->args)
    collectParamRefs(arg, out);
}

void FormulaEngine::collectSymbolRefs(const ASTNodePtr &node,
                                      QStringList &out) const {
  if (!node)
    return;
  if (node->kind == FormulaASTNode::FunctionCall && !node->args.isEmpty()) {
    // First argument of market data / indicator functions is typically a symbol
    // ID
    QString symId = argAsSymbolId(node->args[0]);
    if (!symId.isEmpty() && !out.contains(symId))
      out.append(symId);
  }
  collectSymbolRefs(node->left, out);
  collectSymbolRefs(node->right, out);
  collectSymbolRefs(node->middle, out);
  for (const auto &arg : node->args)
    collectSymbolRefs(arg, out);
}

QStringList FormulaEngine::referencedParams(const QString &expression) const {
  bool ok;
  QVector<FormulaToken> tokens = tokenize(expression, &ok);
  if (!ok)
    return {};
  ASTNodePtr ast = parse(tokens, &ok);
  if (!ok)
    return {};
  QStringList result;
  collectParamRefs(ast, result);
  return result;
}

QStringList FormulaEngine::referencedSymbols(const QString &expression) const {
  bool ok;
  QVector<FormulaToken> tokens = tokenize(expression, &ok);
  if (!ok)
    return {};
  ASTNodePtr ast = parse(tokens, &ok);
  if (!ok)
    return {};
  QStringList result;
  collectSymbolRefs(ast, result);
  return result;
}
