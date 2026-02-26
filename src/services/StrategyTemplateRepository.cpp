#include "services/StrategyTemplateRepository.h"
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

// ═══════════════════════════════════════════════════════════════════
// SINGLETON
// ═══════════════════════════════════════════════════════════════════

StrategyTemplateRepository &StrategyTemplateRepository::instance() {
    static StrategyTemplateRepository s_instance;
    if (!s_instance.isOpen()) {
        s_instance.open(); // auto-open with default path
    }
    return s_instance;
}

// ═══════════════════════════════════════════════════════════════════
// CONSTRUCTION
// ═══════════════════════════════════════════════════════════════════

StrategyTemplateRepository::StrategyTemplateRepository(QObject *parent)
    : QObject(parent)
    , m_connectionName("strategy_template_db") {}

StrategyTemplateRepository::~StrategyTemplateRepository() { close(); }

// ═══════════════════════════════════════════════════════════════════
// OPEN / CLOSE
// ═══════════════════════════════════════════════════════════════════

bool StrategyTemplateRepository::open(const QString &dbPath) {
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);

    if (dbPath.isEmpty()) {
        QDir dir(QDir::currentPath());
        if (!dir.exists("strategy_manager"))
            dir.mkpath("strategy_manager");
        m_dbPath = dir.filePath("strategy_manager/strategy_templates.db");
    } else {
        m_dbPath = dbPath;
    }

    m_db.setDatabaseName(m_dbPath);
    if (!m_db.open()) {
        qDebug() << "[TemplateRepo] Failed to open DB:" << m_db.lastError().text();
        return false;
    }
    return ensureSchema();
}

void StrategyTemplateRepository::close() {
    if (m_db.isOpen()) m_db.close();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool StrategyTemplateRepository::isOpen() const { return m_db.isOpen(); }

bool StrategyTemplateRepository::ensureSchema() {
    QSqlQuery q(m_db);
    bool ok = q.exec(
        "CREATE TABLE IF NOT EXISTS strategy_templates ("
        "  id          TEXT PRIMARY KEY,"
        "  name        TEXT NOT NULL,"
        "  description TEXT,"
        "  version     TEXT,"
        "  mode        TEXT NOT NULL DEFAULT 'indicator',"
        "  body_json   TEXT NOT NULL,"
        "  created_at  TEXT,"
        "  updated_at  TEXT,"
        "  deleted     INTEGER NOT NULL DEFAULT 0"
        ")"
    );
    if (!ok)
        qDebug() << "[TemplateRepo] Schema error:" << q.lastError().text();
    return ok;
}

// ═══════════════════════════════════════════════════════════════════
// SAVE (INSERT or UPDATE)
// ═══════════════════════════════════════════════════════════════════

bool StrategyTemplateRepository::saveTemplate(StrategyTemplate &tmpl) {
    if (!m_db.isOpen()) return false;

    // Assign id if new
    if (tmpl.templateId.isEmpty())
        tmpl.templateId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (tmpl.createdAt.isNull())
        tmpl.createdAt = QDateTime::currentDateTimeUtc();
    tmpl.updatedAt = QDateTime::currentDateTimeUtc();

    QString body = templateToJson(tmpl);

    QSqlQuery q(m_db);
    q.prepare(
        "INSERT INTO strategy_templates "
        "  (id, name, description, version, mode, body_json, created_at, updated_at, deleted) "
        "VALUES "
        "  (:id, :name, :desc, :ver, :mode, :body, :created, :updated, 0) "
        "ON CONFLICT(id) DO UPDATE SET "
        "  name=excluded.name, description=excluded.description, "
        "  version=excluded.version, mode=excluded.mode, "
        "  body_json=excluded.body_json, updated_at=excluded.updated_at"
    );
    q.bindValue(":id",      tmpl.templateId);
    q.bindValue(":name",    tmpl.name);
    q.bindValue(":desc",    tmpl.description);
    q.bindValue(":ver",     tmpl.version);
    q.bindValue(":mode",    tmpl.modeString());
    q.bindValue(":body",    body);
    q.bindValue(":created", tmpl.createdAt.toString(Qt::ISODate));
    q.bindValue(":updated", now);

    if (!q.exec()) {
        qDebug() << "[TemplateRepo] Save failed:" << q.lastError().text();
        return false;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════
// DELETE (soft delete)
// ═══════════════════════════════════════════════════════════════════

bool StrategyTemplateRepository::deleteTemplate(const QString &templateId) {
    if (!m_db.isOpen()) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE strategy_templates SET deleted=1 WHERE id=:id");
    q.bindValue(":id", templateId);
    if (!q.exec()) {
        qDebug() << "[TemplateRepo] Delete failed:" << q.lastError().text();
        return false;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════
// LOAD ALL
// ═══════════════════════════════════════════════════════════════════

QVector<StrategyTemplate>
StrategyTemplateRepository::loadAllTemplates(bool includeDeleted) {
    QVector<StrategyTemplate> results;
    if (!m_db.isOpen()) return results;

    QString sql = "SELECT id, name, description, version, mode, body_json, "
                  "created_at, updated_at FROM strategy_templates";
    if (!includeDeleted) sql += " WHERE deleted=0";
    sql += " ORDER BY created_at ASC";

    QSqlQuery q(m_db);
    if (!q.exec(sql)) {
        qDebug() << "[TemplateRepo] Load all failed:" << q.lastError().text();
        return results;
    }
    while (q.next()) {
        results.append(templateFromJson(
            q.value("id").toString(),
            q.value("name").toString(),
            q.value("description").toString(),
            q.value("version").toString(),
            q.value("mode").toString(),
            q.value("body_json").toString(),
            q.value("created_at").toString(),
            q.value("updated_at").toString()
        ));
    }
    return results;
}

// ═══════════════════════════════════════════════════════════════════
// LOAD ONE
// ═══════════════════════════════════════════════════════════════════

StrategyTemplate StrategyTemplateRepository::loadTemplate(
    const QString &templateId, bool *ok)
{
    if (ok) *ok = false;
    StrategyTemplate result;
    if (!m_db.isOpen()) return result;

    QSqlQuery q(m_db);
    q.prepare("SELECT id, name, description, version, mode, body_json, "
              "created_at, updated_at FROM strategy_templates "
              "WHERE id=:id AND deleted=0");
    q.bindValue(":id", templateId);
    if (!q.exec() || !q.next()) {
        qDebug() << "[TemplateRepo] Load one failed:" << q.lastError().text();
        return result;
    }
    result = templateFromJson(
        q.value("id").toString(),
        q.value("name").toString(),
        q.value("description").toString(),
        q.value("version").toString(),
        q.value("mode").toString(),
        q.value("body_json").toString(),
        q.value("created_at").toString(),
        q.value("updated_at").toString()
    );
    if (ok) *ok = true;
    return result;
}

// ═══════════════════════════════════════════════════════════════════
// SERIALISATION — StrategyTemplate → JSON string
// ═══════════════════════════════════════════════════════════════════

// ── Operand helpers ──────────────────────────────────────────────
static QJsonObject operandToJson(const Operand &op) {
    QJsonObject j;
    switch (op.type) {
    case Operand::Type::Price:
        j["type"] = "price";
        j["symbolId"] = op.symbolId;
        j["field"] = op.field;
        break;
    case Operand::Type::Indicator:
        j["type"] = "indicator";
        j["indicatorId"] = op.indicatorId;
        if (!op.outputSeries.isEmpty())
            j["outputSeries"] = op.outputSeries;
        break;
    case Operand::Type::Constant:
        j["type"] = "constant";
        j["value"] = op.constantValue;
        break;
    case Operand::Type::ParamRef:
        j["type"] = "param_ref";
        j["paramName"] = op.paramName;
        break;
    case Operand::Type::Formula:
        j["type"] = "formula";
        j["expression"] = op.formulaExpression;
        break;
    case Operand::Type::Greek:
        j["type"] = "greek";
        j["symbolId"] = op.symbolId;
        j["field"] = op.field;   // "iv","delta","gamma","theta","vega","rho",...
        break;
    case Operand::Type::Spread:
        j["type"] = "spread";
        j["symbolId"] = op.symbolId;
        j["field"] = op.field;   // "bid_ask_spread","leg_spread","net_spread",...
        break;
    case Operand::Type::Total:
        j["type"] = "total";
        j["field"] = op.field;   // "mtm_total","net_premium","net_delta",...
        break;
    }
    return j;
}

static Operand operandFromJson(const QJsonObject &j) {
    Operand op;
    QString t = j["type"].toString();
    if (t == "price") {
        op.type = Operand::Type::Price;
        op.symbolId = j["symbolId"].toString();
        op.field = j["field"].toString();
    } else if (t == "indicator") {
        op.type = Operand::Type::Indicator;
        op.indicatorId  = j["indicatorId"].toString();
        op.outputSeries = j["outputSeries"].toString();  // empty for single-output
    } else if (t == "constant") {
        op.type = Operand::Type::Constant;
        op.constantValue = j["value"].toDouble();
    } else if (t == "param_ref") {
        op.type = Operand::Type::ParamRef;
        op.paramName = j["paramName"].toString();
    } else if (t == "formula") {
        op.type = Operand::Type::Formula;
        op.formulaExpression = j["expression"].toString();
    } else if (t == "greek") {
        op.type = Operand::Type::Greek;
        op.symbolId = j["symbolId"].toString();
        op.field = j["field"].toString();
    } else if (t == "spread") {
        op.type = Operand::Type::Spread;
        op.symbolId = j["symbolId"].toString();
        op.field = j["field"].toString();
    } else if (t == "total") {
        op.type = Operand::Type::Total;
        op.field = j["field"].toString();
    }
    return op;
}

// ── ConditionNode helpers ─────────────────────────────────────────
static QJsonObject conditionToJson(const ConditionNode &node) {
    QJsonObject j;
    switch (node.nodeType) {
    case ConditionNode::NodeType::And: j["type"] = "and"; break;
    case ConditionNode::NodeType::Or:  j["type"] = "or";  break;
    case ConditionNode::NodeType::Leaf:
        j["type"] = "leaf";
        j["left"]  = operandToJson(node.left);
        j["op"]    = node.op;
        j["right"] = operandToJson(node.right);
        return j;
    }
    QJsonArray children;
    for (const auto &child : node.children)
        children.append(conditionToJson(child));
    j["children"] = children;
    return j;
}

static ConditionNode conditionFromJson(const QJsonObject &j) {
    ConditionNode node;
    QString t = j["type"].toString();
    if (t == "and") {
        node.nodeType = ConditionNode::NodeType::And;
    } else if (t == "or") {
        node.nodeType = ConditionNode::NodeType::Or;
    } else {
        // leaf
        node.nodeType = ConditionNode::NodeType::Leaf;
        node.left  = operandFromJson(j["left"].toObject());
        node.op    = j["op"].toString();
        node.right = operandFromJson(j["right"].toObject());
        return node;
    }
    for (const auto &childVal : j["children"].toArray())
        node.children.append(conditionFromJson(childVal.toObject()));
    return node;
}

// ── Main serialiser ───────────────────────────────────────────────
QString StrategyTemplateRepository::templateToJson(
    const StrategyTemplate &tmpl) const
{
    QJsonObject root;

    // flags
    root["usesTimeTrigger"]      = tmpl.usesTimeTrigger;
    root["predominantlyOptions"] = tmpl.predominantlyOptions;

    // symbols
    QJsonArray syms;
    for (const auto &s : tmpl.symbols) {
        QJsonObject o;
        o["id"]        = s.id;
        o["label"]     = s.label;
        o["role"]      = (s.role == SymbolRole::Reference) ? "reference" : "trade";
        o["segment"]   = (s.segment == SymbolSegment::NSE_FO) ? "nse_fo"
                       : (s.segment == SymbolSegment::BSE_CM) ? "bse_cm"
                       : (s.segment == SymbolSegment::BSE_FO) ? "bse_fo"
                                                               : "nse_cm";
        o["entrySide"] = (s.entrySide == SymbolDefinition::EntrySide::Sell)
                             ? "sell" : "buy";
        // legacy compat
        o["tradeType"] = o["segment"];
        syms.append(o);
    }
    root["symbols"] = syms;

    // indicators
    QJsonArray inds;
    for (const auto &ind : tmpl.indicators) {
        QJsonObject o;
        o["id"]             = ind.id;
        o["type"]           = ind.type;
        o["symbolId"]       = ind.symbolId;
        o["timeframe"]      = ind.timeframe;
        o["periodParam"]    = ind.periodParam;
        o["period2Param"]   = ind.period2Param;
        o["param3Str"]      = ind.param3Str;
        o["param3"]         = ind.param3;
        o["priceField"]     = ind.priceField;
        o["outputSelector"] = ind.outputSelector;
        o["param1Label"]    = ind.param1Label;
        o["param2Label"]    = ind.param2Label;
        o["param3Label"]    = ind.param3Label;
        // legacy field kept for forward-compat reading
        o["param1"]         = ind.param1;
        inds.append(o);
    }
    root["indicators"] = inds;

    // params
    QJsonArray pars;
    for (const auto &p : tmpl.params) {
        QJsonObject o;
        o["name"]        = p.name;
        o["label"]       = p.label;
        o["valueType"]   = (int)p.valueType;
        o["default"]     = QJsonValue::fromVariant(p.defaultValue);
        o["min"]         = QJsonValue::fromVariant(p.minValue);
        o["max"]         = QJsonValue::fromVariant(p.maxValue);
        o["description"] = p.description;
        o["expression"]  = p.expression;
        o["locked"]      = p.locked;
        // ── New trigger fields ──
        o["trigger"]             = (int)p.trigger;
        o["scheduleIntervalSec"] = p.scheduleIntervalSec;
        o["triggerTimeframe"]    = p.triggerTimeframe;
        pars.append(o);
    }
    root["params"] = pars;

    // conditions
    root["entryCondition"] = conditionToJson(tmpl.entryCondition);
    root["exitCondition"]  = conditionToJson(tmpl.exitCondition);

    // risk defaults
    QJsonObject risk;
    risk["stopLossPct"]        = tmpl.riskDefaults.stopLossPercent;
    risk["stopLossLocked"]     = tmpl.riskDefaults.stopLossLocked;
    risk["targetPct"]          = tmpl.riskDefaults.targetPercent;
    risk["targetLocked"]       = tmpl.riskDefaults.targetLocked;
    risk["trailingEnabled"]    = tmpl.riskDefaults.trailingEnabled;
    risk["trailingTriggerPct"] = tmpl.riskDefaults.trailingTriggerPct;
    risk["trailingAmountPct"]  = tmpl.riskDefaults.trailingAmountPct;
    risk["timeExitEnabled"]    = tmpl.riskDefaults.timeExitEnabled;
    risk["exitTime"]           = tmpl.riskDefaults.exitTime;
    risk["maxDailyTrades"]     = tmpl.riskDefaults.maxDailyTrades;
    risk["maxDailyLossRs"]     = tmpl.riskDefaults.maxDailyLossRs;
    root["riskDefaults"]       = risk;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

// ── Main deserialiser ─────────────────────────────────────────────
StrategyTemplate StrategyTemplateRepository::templateFromJson(
    const QString &id,
    const QString &name,
    const QString &description,
    const QString &version,
    const QString &mode,
    const QString &bodyJson,
    const QString &createdAt,
    const QString &updatedAt) const
{
    StrategyTemplate tmpl;
    tmpl.templateId  = id;
    tmpl.name        = name;
    tmpl.description = description;
    tmpl.version     = version;
    tmpl.mode        = StrategyTemplate::modeFromString(mode);
    tmpl.createdAt   = QDateTime::fromString(createdAt, Qt::ISODate);
    tmpl.updatedAt   = QDateTime::fromString(updatedAt,  Qt::ISODate);

    QJsonDocument doc = QJsonDocument::fromJson(bodyJson.toUtf8());
    if (doc.isNull() || !doc.isObject()) return tmpl;

    QJsonObject root = doc.object();

    tmpl.usesTimeTrigger      = root["usesTimeTrigger"].toBool();
    tmpl.predominantlyOptions = root["predominantlyOptions"].toBool();

    // symbols
    for (const auto &sv : root["symbols"].toArray()) {
        QJsonObject o = sv.toObject();
        SymbolDefinition s;
        s.id    = o["id"].toString();
        s.label = o["label"].toString();
        s.role  = (o["role"].toString() == "reference")
                  ? SymbolRole::Reference : SymbolRole::Trade;
        // read "segment" key; fall back to legacy "tradeType" key
        QString seg = o["segment"].toString();
        if (seg.isEmpty()) {
            // legacy mapping
            QString tt = o["tradeType"].toString();
            seg = (tt == "nse_fo" || tt == "option" || tt == "future") ? "nse_fo" : "nse_cm";
        }
        s.segment = (seg == "nse_fo") ? SymbolSegment::NSE_FO
                  : (seg == "bse_cm") ? SymbolSegment::BSE_CM
                  : (seg == "bse_fo") ? SymbolSegment::BSE_FO
                                      : SymbolSegment::NSE_CM;
        s.tradeType = s.segment; // keep alias in sync
        // Entry side (default: Buy for backward compat)
        QString side = o["entrySide"].toString("buy");
        s.entrySide = (side == "sell") ? SymbolDefinition::EntrySide::Sell
                                      : SymbolDefinition::EntrySide::Buy;
        tmpl.symbols.append(s);
    }

    // indicators
    for (const auto &iv : root["indicators"].toArray()) {
        QJsonObject o = iv.toObject();
        IndicatorDefinition ind;
        ind.id             = o["id"].toString();
        ind.type           = o["type"].toString();
        ind.symbolId       = o["symbolId"].toString();
        ind.timeframe      = o["timeframe"].toString("D");
        ind.periodParam    = o["periodParam"].toString();
        ind.period2Param   = o["period2Param"].toString();
        ind.priceField     = o["priceField"].toString("close");
        ind.param3Str      = o["param3Str"].toString();
        ind.param3         = o["param3"].toDouble(o["param3Str"].toString().toDouble());
        ind.outputSelector = o["outputSelector"].toString();
        ind.param1Label    = o["param1Label"].toString();
        ind.param2Label    = o["param2Label"].toString();
        ind.param3Label    = o["param3Label"].toString();
        ind.param1         = o["param1"].toDouble();
        tmpl.indicators.append(ind);
    }

    // params
    for (const auto &pv : root["params"].toArray()) {
        QJsonObject o = pv.toObject();
        TemplateParam p;
        p.name        = o["name"].toString();
        p.label       = o["label"].toString();
        p.valueType   = (ParamValueType)o["valueType"].toInt();
        p.defaultValue= o["default"].toVariant();
        p.minValue    = o["min"].toVariant();
        p.maxValue    = o["max"].toVariant();
        p.description = o["description"].toString();
        p.expression  = o["expression"].toString();
        p.locked      = o["locked"].toBool(false);
        // ── New trigger fields (backward-compat: default OnCandleClose) ──
        p.trigger             = (ParamTrigger)o["trigger"].toInt((int)ParamTrigger::OnCandleClose);
        p.scheduleIntervalSec = o["scheduleIntervalSec"].toInt(300);
        p.triggerTimeframe    = o["triggerTimeframe"].toString();
        tmpl.params.append(p);
    }

    // conditions
    if (root.contains("entryCondition"))
        tmpl.entryCondition = conditionFromJson(root["entryCondition"].toObject());
    if (root.contains("exitCondition"))
        tmpl.exitCondition  = conditionFromJson(root["exitCondition"].toObject());

    // risk defaults
    QJsonObject risk = root["riskDefaults"].toObject();
    tmpl.riskDefaults.stopLossPercent    = risk["stopLossPct"].toDouble(1.0);
    tmpl.riskDefaults.stopLossLocked     = risk["stopLossLocked"].toBool();
    tmpl.riskDefaults.targetPercent      = risk["targetPct"].toDouble(2.0);
    tmpl.riskDefaults.targetLocked       = risk["targetLocked"].toBool();
    tmpl.riskDefaults.trailingEnabled    = risk["trailingEnabled"].toBool();
    tmpl.riskDefaults.trailingTriggerPct = risk["trailingTriggerPct"].toDouble(1.0);
    tmpl.riskDefaults.trailingAmountPct  = risk["trailingAmountPct"].toDouble(0.5);
    tmpl.riskDefaults.timeExitEnabled    = risk["timeExitEnabled"].toBool();
    tmpl.riskDefaults.exitTime           = risk["exitTime"].toString("15:15");
    tmpl.riskDefaults.maxDailyTrades     = risk["maxDailyTrades"].toInt(10);
    tmpl.riskDefaults.maxDailyLossRs     = risk["maxDailyLossRs"].toDouble(5000.0);

    return tmpl;
}
