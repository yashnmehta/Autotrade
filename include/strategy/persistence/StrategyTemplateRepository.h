#ifndef STRATEGY_TEMPLATE_REPOSITORY_H
#define STRATEGY_TEMPLATE_REPOSITORY_H

#include "strategy/model/StrategyTemplate.h"
#include <QObject>
#include <QSqlDatabase>
#include <QVector>

/**
 * @brief Persists and loads StrategyTemplate objects to/from SQLite.
 *
 * DB file: <appDir>/strategy_manager/strategy_templates.db
 *
 * Table: strategy_templates
 *   id          TEXT PRIMARY KEY  (UUID)
 *   name        TEXT NOT NULL
 *   description TEXT
 *   version     TEXT
 *   mode        TEXT              ("indicator" | "option_multileg" | "spread")
 *   flags_json  TEXT              (usesTimeTrigger, predominantlyOptions)
 *   body_json   TEXT NOT NULL     (full template JSON: symbols, indicators,
 *                                  params, conditions, riskDefaults)
 *   created_at  TEXT
 *   updated_at  TEXT
 *   deleted     INTEGER DEFAULT 0
 */
class StrategyTemplateRepository : public QObject {
    Q_OBJECT

public:
    // ── Singleton accessor (auto-opens DB on first call) ──
    static StrategyTemplateRepository &instance();

    explicit StrategyTemplateRepository(QObject *parent = nullptr);
    ~StrategyTemplateRepository() override;

    bool open(const QString &dbPath = QString());
    void close();
    bool isOpen() const;

    // ── CRUD ──
    bool saveTemplate(StrategyTemplate &tmpl);   // INSERT or UPDATE (by id)
    bool deleteTemplate(const QString &templateId);
    QVector<StrategyTemplate> loadAllTemplates(bool includeDeleted = false);
    StrategyTemplate loadTemplate(const QString &templateId, bool *ok = nullptr);

private:
    bool ensureSchema();

    // ── Serialisation helpers ──
    QString    templateToJson(const StrategyTemplate &tmpl) const;
    StrategyTemplate templateFromJson(const QString &id,
                                      const QString &name,
                                      const QString &description,
                                      const QString &version,
                                      const QString &mode,
                                      const QString &bodyJson,
                                      const QString &createdAt,
                                      const QString &updatedAt) const;

    QString m_connectionName;
    QString m_dbPath;
    QSqlDatabase m_db;
};

#endif // STRATEGY_TEMPLATE_REPOSITORY_H
