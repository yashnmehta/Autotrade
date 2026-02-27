#include "strategy/builder/IndicatorCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────
IndicatorCatalog &IndicatorCatalog::instance()
{
    static IndicatorCatalog inst;
    return inst;
}

// ─────────────────────────────────────────────────────────────────────────────
// load
// ─────────────────────────────────────────────────────────────────────────────
bool IndicatorCatalog::load(const QString &jsonFilePath)
{
    if (m_loaded) return true;

    QFile f(jsonFilePath);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();

    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;

    QJsonObject root = doc.object();
    QJsonObject indicators = root.value("indicators").toObject();

    for (const QString &groupName : indicators.keys()) {
        QJsonValue val = indicators.value(groupName);
        if (val.isArray())
            parseGroup(groupName, val.toArray());
    }

    m_loaded = !m_all.isEmpty();
    return m_loaded;
}

// ─────────────────────────────────────────────────────────────────────────────
// parseGroup — internal
// ─────────────────────────────────────────────────────────────────────────────
void IndicatorCatalog::parseGroup(const QString &groupName, const QJsonArray &arr)
{
    if (!m_groups.contains(groupName))
        m_groups.append(groupName);

    for (const QJsonValue &v : arr) {
        if (!v.isObject()) continue;
        QJsonObject obj = v.toObject();

        IndicatorMeta m;
        m.type        = obj.value("type").toString();
        m.label       = obj.value("label").toString();
        m.group       = obj.value("group").toString(groupName);
        m.description = obj.value("description").toString();

        for (const QJsonValue &inp : obj.value("inputs").toArray())
            m.inputs << inp.toString();
        for (const QJsonValue &out : obj.value("outputs").toArray())
            m.outputs << out.toString();

        QJsonObject defs = obj.value("defaults").toObject();
        m.defaultId         = defs.value("id").toString();
        m.defaultParam1     = defs.value("param1").toString();
        m.defaultParam2     = defs.value("param2").toString();
        m.defaultParam3     = defs.value("param3").toDouble(0.0);
        m.defaultPriceField = defs.value("priceField").toString("close");

        for (const QJsonValue &pm : obj.value("paramMeta").toArray()) {
            QJsonObject pObj = pm.toObject();
            IndicatorParamMeta meta;
            meta.key    = pObj.value("key").toString();
            meta.label  = pObj.value("label").toString();
            meta.type   = pObj.value("type").toString("int");
            meta.minVal = pObj.value("min").toDouble(0.0);
            meta.maxVal = pObj.value("max").toDouble(500.0);
            meta.defVal = pObj.value("default").toDouble(0.0);
            m.paramMeta.append(meta);
        }

        if (!m.type.isEmpty())
            m_all.append(m);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Accessors
// ─────────────────────────────────────────────────────────────────────────────
QStringList IndicatorCatalog::groups() const
{
    return m_groups;
}

QVector<IndicatorMeta> IndicatorCatalog::forGroup(const QString &group) const
{
    QVector<IndicatorMeta> result;
    for (const auto &m : m_all)
        if (m.group == group)
            result.append(m);
    return result;
}

bool IndicatorCatalog::find(const QString &type, IndicatorMeta &out) const
{
    for (const auto &m : m_all) {
        if (m.type.compare(type, Qt::CaseInsensitive) == 0) {
            out = m;
            return true;
        }
    }
    return false;
}

QStringList IndicatorCatalog::allTypes() const
{
    QStringList types;
    for (const auto &m : m_all)
        types << m.type;
    return types;
}
