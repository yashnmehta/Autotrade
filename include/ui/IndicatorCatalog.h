#ifndef INDICATOR_CATALOG_H
#define INDICATOR_CATALOG_H

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

// ─────────────────────────────────────────────────────────────────────────────
// IndicatorMeta — parsed entry from indicator_defaults.json
// ─────────────────────────────────────────────────────────────────────────────
struct IndicatorParamMeta {
    QString key;        // "param1", "param2", "param3"
    QString label;      // "Time Period", "Fast Period", …
    QString type;       // "int" | "double"
    double  minVal  = 0.0;
    double  maxVal  = 500.0;
    double  defVal  = 0.0;
};

struct IndicatorMeta {
    QString type;           // "RSI", "MACD", "BBANDS", …
    QString label;          // "Relative Strength Index"
    QString group;          // "Momentum Indicators"
    QString description;    // one-line description
    QStringList inputs;     // required OHLCV fields
    QStringList outputs;    // output series names

    // Auto-fill defaults for the indicators table row
    QString defaultId;          // e.g. "RSI_1"
    QString defaultParam1;      // e.g. "14"
    QString defaultParam2;      // e.g. ""
    double  defaultParam3 = 0.0;
    QString defaultPriceField;  // e.g. "close"

    QVector<IndicatorParamMeta> paramMeta;

    // Helper: how many configurable params does this indicator have?
    int paramCount() const { return paramMeta.size(); }
};

// ─────────────────────────────────────────────────────────────────────────────
// IndicatorCatalog — singleton, loads once from indicator_defaults.json
// ─────────────────────────────────────────────────────────────────────────────
class IndicatorCatalog {
public:
    static IndicatorCatalog &instance();

    // Load from file (call once at startup or lazily on first use).
    // Returns true on success. Safe to call multiple times (no-op if loaded).
    bool load(const QString &jsonFilePath);
    bool isLoaded() const { return m_loaded; }

    // All indicators (flat)
    const QVector<IndicatorMeta> &all() const { return m_all; }

    // All group names in insertion order
    QStringList groups() const;

    // Indicators for a specific group
    QVector<IndicatorMeta> forGroup(const QString &group) const;

    // Lookup by type string (case-insensitive)
    bool find(const QString &type, IndicatorMeta &out) const;

    // All type strings (for dropdowns)
    QStringList allTypes() const;

private:
    IndicatorCatalog() = default;

    void parseGroup(const QString &groupName, const QJsonArray &arr);

    bool                      m_loaded = false;
    QVector<IndicatorMeta>    m_all;
    QStringList               m_groups; // ordered
};

#endif // INDICATOR_CATALOG_H
