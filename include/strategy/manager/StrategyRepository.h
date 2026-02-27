#ifndef STRATEGY_REPOSITORY_H
#define STRATEGY_REPOSITORY_H

#include <QObject>
#include <QSqlDatabase>
#include <QVector>
#include "strategy/model/StrategyInstance.h"

class StrategyRepository : public QObject
{
    Q_OBJECT

public:
    explicit StrategyRepository(QObject *parent = nullptr);
    ~StrategyRepository();

    bool open(const QString& dbPath = QString());
    void close();
    bool isOpen() const;

    bool ensureSchema();

    qint64 saveInstance(StrategyInstance& instance);
    bool updateInstance(const StrategyInstance& instance);
    bool markDeleted(qint64 instanceId);

    QVector<StrategyInstance> loadAllInstances(bool includeDeleted = false);

private:
    QString m_connectionName;
    QString m_dbPath;
    QSqlDatabase m_db;

    StrategyInstance fromQuery(class QSqlQuery& query) const;
    QString toIsoString(const QDateTime& value) const;
    QDateTime fromIsoString(const QString& value) const;
};

#endif // STRATEGY_REPOSITORY_H
