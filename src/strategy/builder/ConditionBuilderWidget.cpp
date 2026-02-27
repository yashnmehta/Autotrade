#include "strategy/builder/ConditionBuilderWidget.h"
#include "strategy/runtime/FormulaEngine.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

// ─────────────────────────────────────────────────────────────────────────────
// Internal item data roles
// ─────────────────────────────────────────────────────────────────────────────
static const int kNodeTypeRole = Qt::UserRole + 1;
static const int kLeafDataRole = Qt::UserRole + 2;

static int toInt(ConditionNode::NodeType t) { return static_cast<int>(t); }

// ─────────────────────────────────────────────────────────────────────────────
// Shared Operand ↔ QVariantMap helpers  (used by addLeafItem, openLeafEditor,
// nodeFromItem — single source of truth, no duplication)
// ─────────────────────────────────────────────────────────────────────────────
static QVariantMap operandToMap(const Operand &op, const QString &pfx) {
  QVariantMap m;
  m[pfx + "type"] = (int)op.type;
  m[pfx + "symbolId"] = op.symbolId;
  m[pfx + "field"] = op.field;
  m[pfx + "indicatorId"] = op.indicatorId;
  m[pfx + "outputSeries"] = op.outputSeries;
  m[pfx + "constVal"] = op.constantValue;
  m[pfx + "paramName"] = op.paramName;
  return m;
}

static Operand operandFromMap(const QVariantMap &m, const QString &pfx) {
  Operand op;
  op.type = (Operand::Type)m.value(pfx + "type", 0).toInt();
  op.symbolId = m.value(pfx + "symbolId").toString();
  op.field = m.value(pfx + "field").toString();
  op.indicatorId = m.value(pfx + "indicatorId").toString();
  op.outputSeries = m.value(pfx + "outputSeries").toString();
  op.constantValue = m.value(pfx + "constVal", 0.0).toDouble();
  op.paramName = m.value(pfx + "paramName").toString();
  return op;
}

static QVariantMap leafToMap(const ConditionNode &leaf) {
  QVariantMap m = operandToMap(leaf.left, "left_");
  m.insert(operandToMap(leaf.right, "right_"));
  m["op"] = leaf.op;
  return m;
}

static void leafFromMap(const QVariantMap &m, ConditionNode &leaf) {
  leaf.left = operandFromMap(m, "left_");
  leaf.right = operandFromMap(m, "right_");
  leaf.op = m.value("op", ">").toString();
}

// ─────────────────────────────────────────────────────────────────────────────
// LeafEditorDialog
// ─────────────────────────────────────────────────────────────────────────────
class LeafEditorDialog : public QDialog {
  Q_OBJECT
public:
  LeafEditorDialog(const Operand &left, const QString &op, const Operand &right,
                   const QStringList &symbolIds,
                   const QStringList &indicatorIds,
                   const QStringList &paramNames,
                   const QMap<QString, QStringList> &indicatorOutputMap,
                   QWidget *parent = nullptr)
      : QDialog(parent), m_indicatorOutputMap(indicatorOutputMap) {
    setWindowTitle("Edit Condition Leaf");
    setMinimumWidth(560);

    setStyleSheet(R"(
            QDialog        { background:#ffffff; color:#0f172a; }
            QGroupBox      { background:#f8fafc; border:1px solid #e2e8f0; border-radius:5px;
                             margin-top:14px; padding:8px 6px 6px 6px;
                             font-size:11px; font-weight:700; color:#475569; }
            QGroupBox::title { subcontrol-origin:margin; subcontrol-position:top left;
                               left:10px; top:0; padding:0 5px;
                               color:#2563eb; background:#f8fafc; }
            QLabel         { color:#475569; font-size:12px; }
            QLabel#previewLabel { background:#f1f5f9; color:#0f172a; font-size:12px;
                                  font-family:monospace; padding:6px 10px;
                                  border:1px solid #cbd5e1; border-radius:4px; }
            QComboBox      { background:#ffffff; border:1px solid #cbd5e1; border-radius:4px;
                             color:#0f172a; padding:4px 8px; font-size:12px; min-width:110px; }
            QComboBox:hover { border-color:#94a3b8; }
            QComboBox:focus { border-color:#3b82f6; }
            QComboBox::drop-down { border:none; width:20px; }
            QComboBox QAbstractItemView { background:#ffffff; color:#0f172a;
                                          border:1px solid #e2e8f0;
                                          selection-background-color:#dbeafe;
                                          selection-color:#1e40af; }
            QDoubleSpinBox { background:#ffffff; border:1px solid #cbd5e1; border-radius:4px;
                             color:#0f172a; padding:4px 8px; font-size:12px; }
            QDoubleSpinBox:focus { border-color:#3b82f6; }
            QDoubleSpinBox::up-button, QDoubleSpinBox::down-button
                           { background:#f1f5f9; border:none; width:16px; }
            QPushButton    { background:#f1f5f9; color:#475569; border:1px solid #cbd5e1;
                             border-radius:4px; padding:6px 18px; font-size:12px; }
            QPushButton:hover  { background:#e2e8f0; color:#1e293b; }
        )");

    auto *mainLay = new QVBoxLayout(this);
    mainLay->setSpacing(6);

    // ── Live preview banner ──
    m_previewLabel = new QLabel("—", this);
    m_previewLabel->setObjectName("previewLabel");
    m_previewLabel->setWordWrap(true);
    mainLay->addWidget(m_previewLabel);

    // ── Left / Operator / Right ──
    m_leftGroup =
        buildOperandGroup("Left Operand", symbolIds, indicatorIds, paramNames);
    m_rightGroup =
        buildOperandGroup("Right Operand", symbolIds, indicatorIds, paramNames);

    auto *opGroup = new QGroupBox("Operator", this);
    auto *opLay = new QHBoxLayout(opGroup);
    m_opCombo = new QComboBox(opGroup);
    m_opCombo->addItems(
        {">", ">=", "<", "<=", "==", "!=", "crosses_above", "crosses_below"});
    m_opCombo->setCurrentText(op.isEmpty() ? ">" : op);
    opLay->addWidget(m_opCombo);
    opLay->addStretch();

    mainLay->addWidget(m_leftGroup);
    mainLay->addWidget(opGroup);
    mainLay->addWidget(m_rightGroup);

    // ── Buttons ──
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    // Style OK blue
    if (auto *ok = buttons->button(QDialogButtonBox::Ok)) {
      ok->setStyleSheet(
          "QPushButton { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
          "stop:0 #2563eb,stop:1 #1d4ed8); color:#fff; border:none; "
          "font-weight:700; padding:6px 20px; border-radius:4px; }"
          "QPushButton:hover { background:#1e40af; }");
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLay->addWidget(buttons);

    // Restore saved state
    setOperand(m_leftGroup, left);
    setOperand(m_rightGroup, right);

    // Wire preview updates — any change in any combo/spin refreshes the label
    auto refresh = [this]() { refreshPreview(); };
    for (auto *g : {m_leftGroup, m_rightGroup}) {
      for (auto *c : g->findChildren<QComboBox *>())
        connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                refresh);
      for (auto *s : g->findChildren<QDoubleSpinBox *>())
        connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                refresh);
    }
    connect(m_opCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, refresh);
    refreshPreview();
  }

  Operand leftOperand() const { return extractOperand(m_leftGroup); }
  Operand rightOperand() const { return extractOperand(m_rightGroup); }
  QString op() const { return m_opCombo->currentText(); }

private:
  // ── Build one operand panel using QStackedWidget for clean panel switching
  // ──
  QGroupBox *buildOperandGroup(const QString &title,
                               const QStringList &symbolIds,
                               const QStringList &indicatorIds,
                               const QStringList &paramNames) {
    const QStringList syms =
        symbolIds.isEmpty() ? QStringList{"REF_1"} : symbolIds;

    auto *box = new QGroupBox(title, this);
    auto *vlay = new QVBoxLayout(box);
    vlay->setSpacing(4);

    // Type selector
    auto *typeCombo = new QComboBox(box);
    typeCombo->setObjectName("typeCombo");
    typeCombo->addItems({"Price", "Indicator", "Constant", "Parameter",
                         "Formula", "Greek / IV", "Spread",
                         "Total / Portfolio"});
    typeCombo->setToolTip(
        "<b>Price</b>         — LTP / OHLCV of a symbol slot<br>"
        "<b>Indicator</b>     — Technical indicator (RSI, MACD, BBANDS…)<br>"
        "<b>Constant</b>      — Fixed numeric value<br>"
        "<b>Parameter</b>     — Deploy-time configurable value {{NAME}}<br>"
        "<b>Formula</b>       — Dynamic expression evaluated at runtime<br>"
        "<b>Greek / IV</b>    — Options delta, gamma, theta, vega, IV…<br>"
        "<b>Spread</b>        — Bid-ask or leg spread of a symbol slot<br>"
        "<b>Total / Portfolio</b> — Strategy-wide MTM, net greeks, open "
        "trades…");

    auto *hrow = new QHBoxLayout;
    hrow->addWidget(new QLabel("Type:", box));
    hrow->addWidget(typeCombo);
    hrow->addStretch();
    vlay->addLayout(hrow);

    // Stacked pages — one per type
    auto *stack = new QStackedWidget(box);
    stack->setObjectName("typeStack");
    vlay->addWidget(stack);

    // ── Page 0: Price ──
    {
      auto *w = new QWidget;
      auto *l = new QHBoxLayout(w);
      l->setContentsMargins(0, 0, 0, 0);
      auto *s = new QComboBox(w);
      s->setObjectName("symCombo");
      s->addItems(syms);
      auto *f = new QComboBox(w);
      f->setObjectName("fieldCombo");
      f->addItems({"ltp", "open", "high", "low", "close", "volume", "oi"});
      l->addWidget(new QLabel("Symbol:", w));
      l->addWidget(s);
      l->addWidget(new QLabel("Field:", w));
      l->addWidget(f);
      l->addStretch();
      stack->addWidget(w);
    }
    // ── Page 1: Indicator ──
    {
      auto *w = new QWidget;
      auto *l = new QHBoxLayout(w);
      l->setContentsMargins(0, 0, 0, 0);
      auto *i = new QComboBox(w);
      i->setObjectName("indIdCombo");
      i->setEditable(true);
      i->addItems(indicatorIds.isEmpty() ? QStringList{"RSI_1"} : indicatorIds);
      i->setToolTip("Indicator ID declared in the Indicators tab");

      auto *outLabel = new QLabel("Output:", w);
      auto *o = new QComboBox(w);
      o->setObjectName("indOutCombo");
      o->setEditable(true);
      o->setMinimumWidth(110);
      o->setToolTip("Pick which output series to compare.\n"
                    "Single-output (RSI, SMA…): leave as 'value'\n"
                    "Multi-output examples:\n"
                    "  MACD   → macd, signal, hist\n"
                    "  BBANDS → upperBand, middleBand, lowerBand\n"
                    "  STOCH  → slowk, slowd\n"
                    "  STOCHF → fastk, fastd\n"
                    "  AROON  → aroonDown, aroonUp\n"
                    "Use separate conditions for each output you need.");

      // Helper: populate output combo for a given indicator id
      auto populateOutputs = [this, o](const QString &indId) {
        QString savedText = o->currentText();
        o->blockSignals(true);
        o->clear();
        QStringList outs = m_indicatorOutputMap.value(indId);
        if (outs.isEmpty()) {
          o->addItem("value"); // single-output default
        } else {
          o->addItems(outs);
        }
        // Restore previous selection if still valid
        int idx = o->findText(savedText);
        o->setCurrentIndex(idx >= 0 ? idx : 0);
        o->blockSignals(false);
      };

      // Populate immediately for first item
      if (!indicatorIds.isEmpty())
        populateOutputs(indicatorIds.first());

      // Re-populate when user picks a different indicator
      connect(i, &QComboBox::currentTextChanged, this,
              [populateOutputs](const QString &id) { populateOutputs(id); });

      l->addWidget(new QLabel("Indicator:", w));
      l->addWidget(i);
      l->addSpacing(8);
      l->addWidget(outLabel);
      l->addWidget(o);
      l->addStretch();
      stack->addWidget(w);
    }
    // ── Page 2: Constant ──
    {
      auto *w = new QWidget;
      auto *l = new QHBoxLayout(w);
      l->setContentsMargins(0, 0, 0, 0);
      auto *s = new QDoubleSpinBox(w);
      s->setObjectName("constSpin");
      s->setRange(-1e9, 1e9);
      s->setDecimals(4);
      s->setValue(0);
      l->addWidget(new QLabel("Value:", w));
      l->addWidget(s);
      l->addStretch();
      stack->addWidget(w);
    }
    // ── Page 3: Parameter ──
    {
      auto *w = new QWidget;
      auto *l = new QHBoxLayout(w);
      l->setContentsMargins(0, 0, 0, 0);
      auto *c = new QComboBox(w);
      c->setObjectName("paramCombo");
      c->setEditable(true);
      for (const QString &p : paramNames)
        c->addItem(QString("{{%1}}").arg(p));
      if (c->count() == 0)
        c->addItem("{{THRESHOLD}}");
      l->addWidget(new QLabel("Parameter:", w));
      l->addWidget(c);
      l->addStretch();
      stack->addWidget(w);
    }
    // ── Page 4: Formula (runtime expression) ──
    {
      auto *w = new QWidget;
      auto *vl = new QVBoxLayout(w);
      vl->setContentsMargins(0, 0, 0, 0);
      auto *le = new QLineEdit(w);
      le->setObjectName("formulaEdit");
      le->setPlaceholderText("e.g. ATR(REF_1, 14) * 2.5");
      le->setToolTip(
          "<b>Formula Expression</b><br>"
          "A math expression evaluated at runtime against live market "
          "data.<br><br>"
          "<b>Market data:</b> LTP(sym), OPEN(sym), HIGH(sym), LOW(sym), "
          "CLOSE(sym), "
          "VOLUME(sym), BID(sym), ASK(sym), CHANGE_PCT(sym)<br>"
          "<b>Indicators:</b> RSI(sym, period), SMA(sym, period), EMA(sym, "
          "period), "
          "ATR(sym, period), VWAP(sym), BBANDS_UPPER(sym, period), MACD(sym, "
          "fast, slow)<br>"
          "<b>Greeks:</b> IV(sym), DELTA(sym), GAMMA(sym), THETA(sym), "
          "VEGA(sym)<br>"
          "<b>Portfolio:</b> MTM(), NET_PREMIUM(), NET_DELTA()<br>"
          "<b>Math:</b> ABS(x), MAX(a,b), MIN(a,b), ROUND(x), SQRT(x), "
          "POW(a,b), CLAMP(x,lo,hi)<br>"
          "<b>Operators:</b> + - * / % ^ > >= < <= == != && || ! ? :<br><br>"
          "<b>Parameters:</b> Use deployed param names directly, e.g. "
          "RSI_PERIOD, OFFSET_PCT<br><br>"
          "<b>Examples:</b><br>"
          "  ATR(REF_1, 14) * 2.5<br>"
          "  VWAP(REF_1) * (1 + OFFSET_PCT / 100)<br>"
          "  IV(TRADE_1) > 25 ? LTP(TRADE_1) * 0.98 : LTP(TRADE_1) * 0.95<br>"
          "  MAX(SMA(REF_1, 20), SMA(REF_1, 50))");
      auto *hl = new QHBoxLayout;
      hl->addWidget(new QLabel("ƒ(x) =", w));
      hl->addWidget(le);
      vl->addLayout(hl);
      // Validation label
      auto *valLbl = new QLabel(w);
      valLbl->setObjectName("formulaValidation");
      valLbl->setStyleSheet("font-size:10px; padding:1px 4px;");
      vl->addWidget(valLbl);
      // Live validation as user types
      connect(le, &QLineEdit::textChanged, w, [valLbl](const QString &text) {
        if (text.trimmed().isEmpty()) {
          valLbl->setText("");
          return;
        }
        FormulaEngine engine;
        QString errMsg;
        bool valid = engine.validate(text, &errMsg);
        if (valid) {
          valLbl->setText(
              "<span style='color:#16a34a;'>✓ Valid formula</span>");
          valLbl->setStyleSheet(
              "font-size:10px; padding:1px 4px; color:#16a34a;");
        } else {
          valLbl->setText(QString("<span style='color:#dc2626;'>✗ %1</span>")
                              .arg(errMsg.toHtmlEscaped()));
          valLbl->setStyleSheet(
              "font-size:10px; padding:1px 4px; color:#dc2626;");
        }
      });
      stack->addWidget(w);
    }
    // ── Page 5: Greek / IV ──
    {
      auto *w = new QWidget;
      auto *l = new QHBoxLayout(w);
      l->setContentsMargins(0, 0, 0, 0);
      auto *s = new QComboBox(w);
      s->setObjectName("greekSymCombo");
      s->addItems(syms);
      auto *g = new QComboBox(w);
      g->setObjectName("greekFieldCombo");
      g->addItems({"iv", "delta", "gamma", "theta", "vega", "rho", "iv_rank",
                   "iv_percentile", "pop"});
      l->addWidget(new QLabel("Symbol:", w));
      l->addWidget(s);
      l->addWidget(new QLabel("Greek:", w));
      l->addWidget(g);
      l->addStretch();
      stack->addWidget(w);
    }
    // ── Page 6: Spread ──
    {
      auto *w = new QWidget;
      auto *l = new QHBoxLayout(w);
      l->setContentsMargins(0, 0, 0, 0);
      auto *s = new QComboBox(w);
      s->setObjectName("spreadSymCombo");
      s->addItems(syms);
      auto *f = new QComboBox(w);
      f->setObjectName("spreadFieldCombo");
      f->addItems(
          {"bid_ask_spread", "leg_spread", "net_spread", "slippage_pts"});
      f->setToolTip(
          "<b>Spread field meanings:</b><br>"
          "<b>bid_ask_spread</b>  — Current bid/ask spread of this symbol slot "
          "(in points)<br>"
          "<b>leg_spread</b>      — Spread between two specific legs in the "
          "strategy<br>"
          "<b>net_spread</b>      — Net spread across all legs combined<br>"
          "<b>slippage_pts</b>    — Estimated slippage in points for this "
          "symbol<br>"
          "<br>Symbol slot is the symbol whose spread you want to measure.<br>"
          "Use this to filter entries when spread is too wide (e.g. "
          "bid_ask_spread &lt; 5).");
      l->addWidget(new QLabel("Symbol:", w));
      l->addWidget(s);
      l->addWidget(new QLabel("Field:", w));
      l->addWidget(f);
      l->addStretch();
      stack->addWidget(w);
    }
    // ── Page 7: Total / Portfolio ──
    {
      auto *w = new QWidget;
      auto *l = new QHBoxLayout(w);
      l->setContentsMargins(0, 0, 0, 0);
      auto *f = new QComboBox(w);
      f->setObjectName("totalFieldCombo");
      f->addItems({"mtm_total", "net_premium", "net_delta", "net_gamma",
                   "net_theta", "net_vega", "net_qty", "open_trades"});
      f->setToolTip(
          "<b>Portfolio-level fields (aggregated across ALL legs):</b><br>"
          "<b>mtm_total</b>    — Mark-to-market P&amp;L of the entire strategy "
          "(₹)<br>"
          "<b>net_premium</b>  — Net premium received/paid across all option "
          "legs (₹)<br>"
          "<b>net_delta</b>    — Sum of (delta × qty) across all legs<br>"
          "<b>net_gamma</b>    — Sum of (gamma × qty) across all legs<br>"
          "<b>net_theta</b>    — Sum of (theta × qty) across all legs "
          "(₹/day)<br>"
          "<b>net_vega</b>     — Sum of (vega × qty) across all legs<br>"
          "<b>net_qty</b>      — Total open lot count (positive=long, "
          "negative=short)<br>"
          "<b>open_trades</b>  — Number of currently open legs/trades<br>"
          "<br>No symbol slot needed — these are strategy-wide aggregates.<br>"
          "Examples:<br>"
          "  mtm_total &lt; -5000    → exit if strategy is down ₹5000<br>"
          "  net_delta &gt; 50       → hedge when delta exposure exceeds 50<br>"
          "  open_trades == 0       → confirm full exit before re-entry");
      l->addWidget(new QLabel("Field:", w));
      l->addWidget(f);
      l->addStretch();
      stack->addWidget(w);
    }

    // typeCombo drives the stack
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            stack, &QStackedWidget::setCurrentIndex);

    // ── Per-type hint label ──
    auto *hintLabel = new QLabel(box);
    hintLabel->setObjectName("typeHintLabel");
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color:#64748b; font-size:10px; padding:2px 4px;");
    hintLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    vlay->addWidget(hintLabel);

    const QStringList hints = {
        /*Price*/ "LTP or OHLCV of a symbol slot — e.g. REF_1.close",
        /*Indicator*/
        "Technical indicator declared in the Indicators tab — e.g. RSI_1 < 30",
        /*Constant*/ "A fixed numeric value — e.g. 30, 70, 22500",
        /*Parameter*/
        "A user-configurable parameter filled at deploy time — e.g. "
        "{{THRESHOLD}}",
        /*Formula*/
        "A dynamic expression evaluated at runtime — e.g. ATR(REF_1,14) * 2.5",
        /*Greek*/
        "Options greek or IV of a symbol slot — e.g. TRADE_1.delta < 0.3",
        /*Spread*/
        "Bid-Ask or leg spread of a symbol slot — filter entries on wide "
        "spreads",
        /*Total*/
        "Strategy-wide P&L or greeks aggregate — no symbol needed\n"
        "e.g. [Total] mtm_total < -5000  →  exit when strategy down ₹5000",
    };

    auto updateHint = [hintLabel, hints](int idx) {
      hintLabel->setText(idx >= 0 && idx < hints.size() ? hints[idx] : "");
    };
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            hintLabel, [updateHint](int idx) { updateHint(idx); });
    updateHint(typeCombo->currentIndex());

    return box;
  }

  // ── Set operand back into the UI ──
  void setOperand(QGroupBox *box, const Operand &op) {
    auto *typeCombo = box->findChild<QComboBox *>("typeCombo");
    auto *stack = box->findChild<QStackedWidget *>("typeStack");
    if (!typeCombo || !stack)
      return;

    int idx = (int)op.type;
    typeCombo->setCurrentIndex(idx);
    stack->setCurrentIndex(idx);

    auto cb = [&](const char *n) { return box->findChild<QComboBox *>(n); };
    auto dsb = [&](const char *n) {
      return box->findChild<QDoubleSpinBox *>(n);
    };

    switch (op.type) {
    case Operand::Type::Price:
      if (auto *c = cb("symCombo"))
        c->setCurrentText(op.symbolId);
      if (auto *c = cb("fieldCombo"))
        c->setCurrentText(op.field.isEmpty() ? "ltp" : op.field);
      break;
    case Operand::Type::Indicator:
      if (auto *c = cb("indIdCombo"))
        c->setCurrentText(op.indicatorId);
      if (auto *c = cb("indOutCombo")) {
        // outputSeries empty = "value" (single-output default)
        QString sel = op.outputSeries.isEmpty() ? "value" : op.outputSeries;
        // Try exact match first, then add+select if not found (handles loaded
        // templates)
        int idx = c->findText(sel);
        if (idx < 0) {
          c->addItem(sel);
          idx = c->count() - 1;
        }
        c->setCurrentIndex(idx);
      }
      break;
    case Operand::Type::Constant:
      if (auto *s = dsb("constSpin"))
        s->setValue(op.constantValue);
      break;
    case Operand::Type::ParamRef:
      if (auto *c = cb("paramCombo"))
        c->setCurrentText(op.paramName);
      break;
    case Operand::Type::Formula: {
      auto *le = box->findChild<QLineEdit *>("formulaEdit");
      if (le)
        le->setText(op.formulaExpression);
      break;
    }
    case Operand::Type::Greek:
      if (auto *c = cb("greekSymCombo"))
        c->setCurrentText(op.symbolId);
      if (auto *c = cb("greekFieldCombo"))
        c->setCurrentText(op.field.isEmpty() ? "iv" : op.field);
      break;
    case Operand::Type::Spread:
      if (auto *c = cb("spreadSymCombo"))
        c->setCurrentText(op.symbolId);
      if (auto *c = cb("spreadFieldCombo"))
        c->setCurrentText(op.field.isEmpty() ? "bid_ask_spread" : op.field);
      break;
    case Operand::Type::Total:
      if (auto *c = cb("totalFieldCombo"))
        c->setCurrentText(op.field.isEmpty() ? "mtm_total" : op.field);
      break;
    }
  }

  // ── Extract operand from UI ──
  Operand extractOperand(QGroupBox *box) const {
    Operand op;
    auto *tc = box->findChild<QComboBox *>("typeCombo");
    if (!tc)
      return op;

    auto cb = [&](const char *n) -> QComboBox * {
      return box->findChild<QComboBox *>(n);
    };
    auto dsb = [&](const char *n) -> QDoubleSpinBox * {
      return box->findChild<QDoubleSpinBox *>(n);
    };
    auto txt = [&](const char *n, const char *def = "") -> QString {
      auto *c = cb(n);
      return c ? c->currentText() : QString(def);
    };

    op.type = (Operand::Type)tc->currentIndex();
    switch (op.type) {
    case Operand::Type::Price:
      op.symbolId = txt("symCombo");
      op.field = txt("fieldCombo", "ltp");
      break;
    case Operand::Type::Indicator:
      op.indicatorId = txt("indIdCombo");
      {
        QString outSel = txt("indOutCombo");
        // "value" or empty → single-output, store as empty
        op.outputSeries =
            (outSel == "value" || outSel.isEmpty()) ? QString() : outSel;
      }
      break;
    case Operand::Type::Constant:
      op.constantValue = dsb("constSpin") ? dsb("constSpin")->value() : 0.0;
      break;
    case Operand::Type::ParamRef:
      op.paramName = txt("paramCombo");
      break;
    case Operand::Type::Formula: {
      auto *le = box->findChild<QLineEdit *>("formulaEdit");
      op.formulaExpression = le ? le->text().trimmed() : QString();
      break;
    }
    case Operand::Type::Greek:
      op.symbolId = txt("greekSymCombo");
      op.field = txt("greekFieldCombo", "iv");
      break;
    case Operand::Type::Spread:
      op.symbolId = txt("spreadSymCombo");
      op.field = txt("spreadFieldCombo", "bid_ask_spread");
      break;
    case Operand::Type::Total:
      op.field = txt("totalFieldCombo", "mtm_total");
      break;
    }
    return op;
  }

  // ── Live preview ──
  QString operandText(const Operand &op) const {
    switch (op.type) {
    case Operand::Type::Price:
      return QString("%1.%2").arg(op.symbolId,
                                  op.field.isEmpty() ? "ltp" : op.field);
    case Operand::Type::Indicator: {
      QString out = op.outputSeries.isEmpty()
                        ? QString()
                        : QString(".%1").arg(op.outputSeries);
      return QString("%1%2").arg(op.indicatorId, out);
    }
    case Operand::Type::Constant:
      return QString::number(op.constantValue);
    case Operand::Type::ParamRef:
      return op.paramName;
    case Operand::Type::Formula:
      return QString("ƒ(%1)").arg(op.formulaExpression);
    case Operand::Type::Greek:
      return QString("%1.%2").arg(op.symbolId,
                                  op.field.isEmpty() ? "iv" : op.field);
    case Operand::Type::Spread:
      return QString("[Spread] %1.%2")
          .arg(op.symbolId, op.field.isEmpty() ? "bid_ask_spread" : op.field);
    case Operand::Type::Total:
      return QString("[Total] %1")
          .arg(op.field.isEmpty() ? "mtm_total" : op.field);
    }
    return "?";
  }

  void refreshPreview() {
    Operand l = extractOperand(m_leftGroup);
    Operand r = extractOperand(m_rightGroup);
    m_previewLabel->setText(
        QString("  %1   <b>%2</b>   %3")
            .arg(operandText(l), m_opCombo->currentText(), operandText(r)));
  }

  QComboBox *m_opCombo = nullptr;
  QGroupBox *m_leftGroup = nullptr;
  QGroupBox *m_rightGroup = nullptr;
  QLabel *m_previewLabel = nullptr;
  QMap<QString, QStringList> m_indicatorOutputMap;
};

#include "ConditionBuilderWidget.moc"

// ─────────────────────────────────────────────────────────────────────────────
// ConditionBuilderWidget
// ─────────────────────────────────────────────────────────────────────────────

ConditionBuilderWidget::ConditionBuilderWidget(QWidget *parent)
    : QWidget(parent) {
  auto *mainLay = new QVBoxLayout(this);
  mainLay->setContentsMargins(0, 0, 0, 0);
  mainLay->setSpacing(4);

  // ── Toolbar ──
  auto *toolBar = new QHBoxLayout;
  toolBar->setSpacing(4);
  m_addAndBtn = new QPushButton("＋ AND", this);
  m_addOrBtn = new QPushButton("＋ OR", this);
  m_addLeafBtn = new QPushButton("＋ Condition", this);
  m_removeBtn = new QPushButton("✕ Remove", this);
  m_removeBtn->setEnabled(false);

  // Inline styles that complement the parent's stylesheet (Light Theme)
  const QString addSS =
      "QPushButton { background:#f0fdf4; color:#16a34a; border:1px solid "
      "#bbf7d0;"
      " border-radius:4px; font-size:11px; font-weight:600; padding:3px 10px; }"
      "QPushButton:hover { background:#dcfce7; color:#15803d; }";
  const QString andSS =
      "QPushButton { background:#eff6ff; color:#2563eb; border:1px solid "
      "#bfdbfe;"
      " border-radius:4px; font-size:11px; font-weight:600; padding:3px 10px; }"
      "QPushButton:hover { background:#dbeafe; color:#1d4ed8; }";
  const QString orSS =
      "QPushButton { background:#fff7ed; color:#ea580c; border:1px solid "
      "#fed7aa;"
      " border-radius:4px; font-size:11px; font-weight:600; padding:3px 10px; }"
      "QPushButton:hover { background:#ffedd5; color:#c2410c; }";
  const QString rmSS =
      "QPushButton { background:#fef2f2; color:#dc2626; border:1px solid "
      "#fecaca;"
      " border-radius:4px; font-size:11px; font-weight:600; padding:3px 10px; }"
      "QPushButton:hover { background:#fee2e2; color:#b91c1c; }"
      "QPushButton:disabled { background:#f1f5f9; color:#94a3b8; "
      "border-color:#e2e8f0; }";

  m_addAndBtn->setStyleSheet(andSS);
  m_addOrBtn->setStyleSheet(orSS);
  m_addLeafBtn->setStyleSheet(addSS);
  m_removeBtn->setStyleSheet(rmSS);

  toolBar->addWidget(m_addAndBtn);
  toolBar->addWidget(m_addOrBtn);
  toolBar->addWidget(m_addLeafBtn);
  toolBar->addStretch();
  toolBar->addWidget(m_removeBtn);
  mainLay->addLayout(toolBar);

  // ── Tree ──
  m_tree = new QTreeWidget(this);
  m_tree->setColumnCount(1);
  m_tree->setHeaderHidden(true);
  m_tree->setDragDropMode(QAbstractItemView::InternalMove);
  m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_tree->setExpandsOnDoubleClick(false);
  m_tree->setMinimumHeight(160);
  m_tree->setStyleSheet(
      "QTreeWidget { background:#ffffff; border:1px solid #e2e8f0; "
      "border-radius:4px;"
      " color:#0f172a; font-size:12px; outline:none; }"
      "QTreeWidget::item { padding:3px 4px; }"
      "QTreeWidget::item:selected { background:#dbeafe; color:#1e40af; }"
      "QTreeWidget::item:hover { background:#f1f5f9; }"
      "QTreeWidget::branch { background:#ffffff; }");
  mainLay->addWidget(m_tree);

  // ── Hint ──
  auto *hint = new QLabel(
      "<small style='color:#64748b;'>"
      "💡 <b>+ Condition</b> auto-creates an AND group.  "
      "Select a group, then add more conditions inside it.  "
      "<b>Double-click</b> a leaf to edit.  "
      "Use <b>Total / Portfolio</b> type for strategy-wide P&amp;L checks, "
      "<b>Spread</b> for bid-ask filters.</small>",
      this);
  hint->setWordWrap(true);
  mainLay->addWidget(hint);

  // ── Connections ──
  connect(m_addAndBtn, &QPushButton::clicked, this,
          &ConditionBuilderWidget::onAddAndGroup);
  connect(m_addOrBtn, &QPushButton::clicked, this,
          &ConditionBuilderWidget::onAddOrGroup);
  connect(m_addLeafBtn, &QPushButton::clicked, this,
          &ConditionBuilderWidget::onAddLeaf);
  connect(m_removeBtn, &QPushButton::clicked, this,
          &ConditionBuilderWidget::onRemoveSelected);
  connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
          &ConditionBuilderWidget::onItemDoubleClicked);
  connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
    m_removeBtn->setEnabled(!m_tree->selectedItems().isEmpty());
  });
}

// ─────────────────────────────────────────────────────────────────────────────
// Context setters
// ─────────────────────────────────────────────────────────────────────────────

void ConditionBuilderWidget::setSymbolIds(const QStringList &ids) {
  m_symbolIds = ids;
}
void ConditionBuilderWidget::setIndicatorIds(const QStringList &ids) {
  m_indicatorIds = ids;
}
void ConditionBuilderWidget::setParamNames(const QStringList &names) {
  m_paramNames = names;
}
void ConditionBuilderWidget::setIndicatorOutputMap(
    const QMap<QString, QStringList> &m) {
  m_indicatorOutputMap = m;
}

// ─────────────────────────────────────────────────────────────────────────────
// Data access
// ─────────────────────────────────────────────────────────────────────────────

void ConditionBuilderWidget::clear() {
  m_tree->clear();
  emit conditionChanged();
}

bool ConditionBuilderWidget::isEmpty() const {
  return m_tree->topLevelItemCount() == 0;
}

void ConditionBuilderWidget::setCondition(const ConditionNode &root) {
  m_tree->clear();
  if (root.isLeaf()) {
    if (!root.op.isEmpty())
      addLeafItem(nullptr, root);
  } else {
    auto *rootItem = addGroupItem(nullptr, root.nodeType);
    for (const auto &child : root.children)
      itemFromNode(rootItem, child);
    rootItem->setExpanded(true);
  }
  emit conditionChanged();
}

ConditionNode ConditionBuilderWidget::condition() const {
  if (m_tree->topLevelItemCount() == 0)
    return ConditionNode{};
  if (m_tree->topLevelItemCount() == 1)
    return nodeFromItem(m_tree->topLevelItem(0));
  ConditionNode root;
  root.nodeType = ConditionNode::NodeType::And;
  for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    root.children.append(nodeFromItem(m_tree->topLevelItem(i)));
  return root;
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

void ConditionBuilderWidget::onAddAndGroup() {
  auto *item = addGroupItem(selectedGroupItem(), ConditionNode::NodeType::And);
  m_tree->setCurrentItem(item);
  item->setExpanded(true);
  emit conditionChanged();
}

void ConditionBuilderWidget::onAddOrGroup() {
  auto *item = addGroupItem(selectedGroupItem(), ConditionNode::NodeType::Or);
  m_tree->setCurrentItem(item);
  item->setExpanded(true);
  emit conditionChanged();
}

void ConditionBuilderWidget::onAddLeaf() {
  QTreeWidgetItem *parent = selectedGroupItem();
  if (!parent) {
    // Auto-create a top-level AND group so + Condition always works in one
    // click
    parent = addGroupItem(nullptr, ConditionNode::NodeType::And);
    parent->setExpanded(true);
  }
  ConditionNode leaf;
  leaf.nodeType = ConditionNode::NodeType::Leaf;
  auto *item = addLeafItem(parent, leaf);
  m_tree->setCurrentItem(item);
  parent->setExpanded(true);
  openLeafEditor(item);
  emit conditionChanged();
}

void ConditionBuilderWidget::onRemoveSelected() {
  auto sel = m_tree->selectedItems();
  if (sel.isEmpty())
    return;
  delete sel.first();
  emit conditionChanged();
}

void ConditionBuilderWidget::onItemDoubleClicked(QTreeWidgetItem *item, int) {
  if (item && item->data(0, kNodeTypeRole).toInt() ==
                  toInt(ConditionNode::NodeType::Leaf))
    openLeafEditor(item);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tree helpers
// ─────────────────────────────────────────────────────────────────────────────

QTreeWidgetItem *ConditionBuilderWidget::selectedGroupItem() const {
  auto sel = m_tree->selectedItems();
  if (sel.isEmpty())
    return nullptr;
  QTreeWidgetItem *cur = sel.first();
  int t = cur->data(0, kNodeTypeRole).toInt();
  if (t == toInt(ConditionNode::NodeType::And) ||
      t == toInt(ConditionNode::NodeType::Or))
    return cur;
  return cur->parent();
}

QTreeWidgetItem *
ConditionBuilderWidget::addGroupItem(QTreeWidgetItem *parent,
                                     ConditionNode::NodeType type) {
  const bool isAnd = (type == ConditionNode::NodeType::And);
  QTreeWidgetItem *item =
      parent
          ? new QTreeWidgetItem(parent, QStringList{isAnd ? "⊕ AND" : "⊗ OR"})
          : new QTreeWidgetItem(m_tree, QStringList{isAnd ? "⊕ AND" : "⊗ OR"});
  item->setData(0, kNodeTypeRole, toInt(type));
  QFont f = item->font(0);
  f.setBold(true);
  item->setFont(0, f);
  item->setForeground(0, isAnd ? QColor("#0369a1")
                               : QColor("#b45309")); // Blue-700 / Amber-700
  item->setExpanded(true);
  return item;
}

QTreeWidgetItem *
ConditionBuilderWidget::addLeafItem(QTreeWidgetItem *parent,
                                    const ConditionNode &leaf) {
  QString label = leafSummary(leaf);
  if (label.isEmpty())
    label = "⬡  (click to configure)";

  QTreeWidgetItem *item = parent
                              ? new QTreeWidgetItem(parent, QStringList{label})
                              : new QTreeWidgetItem(m_tree, QStringList{label});
  item->setData(0, kNodeTypeRole, toInt(ConditionNode::NodeType::Leaf));
  item->setForeground(0, QColor("#0f766e"));        // Teal-700
  item->setData(0, kLeafDataRole, leafToMap(leaf)); // ← shared helper
  return item;
}

// ─────────────────────────────────────────────────────────────────────────────
// Leaf editor popup
// ─────────────────────────────────────────────────────────────────────────────

void ConditionBuilderWidget::openLeafEditor(QTreeWidgetItem *item) {
  if (!item)
    return;

  QVariantMap map = item->data(0, kLeafDataRole).toMap();
  ConditionNode leaf;
  leafFromMap(map, leaf);

  LeafEditorDialog dlg(leaf.left, leaf.op, leaf.right, m_symbolIds,
                       m_indicatorIds, m_paramNames, m_indicatorOutputMap,
                       this);
  if (dlg.exec() != QDialog::Accepted)
    return;

  leaf.left = dlg.leftOperand();
  leaf.right = dlg.rightOperand();
  leaf.op = dlg.op();

  item->setText(0, leafSummary(leaf));
  item->setData(0, kLeafDataRole, leafToMap(leaf));
  emit conditionChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialisation helpers
// ─────────────────────────────────────────────────────────────────────────────

ConditionNode
ConditionBuilderWidget::nodeFromItem(QTreeWidgetItem *item) const {
  ConditionNode node;
  node.nodeType = (ConditionNode::NodeType)item->data(0, kNodeTypeRole).toInt();
  if (node.isLeaf()) {
    QVariantMap m = item->data(0, kLeafDataRole).toMap();
    leafFromMap(m, node); // ← shared helper
  } else {
    for (int i = 0; i < item->childCount(); ++i)
      node.children.append(nodeFromItem(item->child(i)));
  }
  return node;
}

void ConditionBuilderWidget::itemFromNode(QTreeWidgetItem *parent,
                                          const ConditionNode &node) {
  if (node.isLeaf()) {
    addLeafItem(parent, node);
  } else {
    auto *g = addGroupItem(parent, node.nodeType);
    for (const auto &child : node.children)
      itemFromNode(g, child);
    g->setExpanded(true);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Summary strings
// ─────────────────────────────────────────────────────────────────────────────

QString ConditionBuilderWidget::operandSummary(const Operand &op) const {
  switch (op.type) {
  case Operand::Type::Price:
    return QString("%1.%2").arg(op.symbolId,
                                op.field.isEmpty() ? "ltp" : op.field);
  case Operand::Type::Indicator: {
    QString out = op.outputSeries.isEmpty()
                      ? QString()
                      : QString(".%1").arg(op.outputSeries);
    return QString("%1%2").arg(op.indicatorId, out);
  }
  case Operand::Type::Constant:
    return QString::number(op.constantValue);
  case Operand::Type::ParamRef:
    return op.paramName;
  case Operand::Type::Formula:
    return QString("ƒ(%1)").arg(op.formulaExpression);
  case Operand::Type::Greek:
    return QString("%1.%2").arg(op.symbolId,
                                op.field.isEmpty() ? "iv" : op.field);
  case Operand::Type::Spread:
    return QString("[Spread] %1.%2")
        .arg(op.symbolId, op.field.isEmpty() ? "bid_ask_spread" : op.field);
  case Operand::Type::Total:
    return QString("[Total] %1")
        .arg(op.field.isEmpty() ? "mtm_total" : op.field);
  }
  return "?";
}

QString ConditionBuilderWidget::leafSummary(const ConditionNode &leaf) const {
  if (leaf.op.isEmpty())
    return "";
  return QString("⬡  %1  %2  %3")
      .arg(operandSummary(leaf.left), leaf.op, operandSummary(leaf.right));
}
