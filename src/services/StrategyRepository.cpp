#include "services/StrategyRepository.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

StrategyRepository::StrategyRepository(QObject *parent)
    : QObject(parent), m_connectionName("strategy_manager_db") {}

StrategyRepository::~StrategyRepository() { close(); }

bool StrategyRepository::open(const QString &dbPath) {
  if (m_db.isOpen()) {
    return true;
  }

  if (!dbPath.isEmpty()) {
    m_dbPath = dbPath;
  } else if (m_dbPath.isEmpty()) {
    QString baseDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty()) {
      baseDir = QDir::currentPath();
    }
    QDir dir(baseDir);
    if (!dir.exists("strategy_manager")) {
      dir.mkpath("strategy_manager");
    }
    m_dbPath = dir.filePath("strategy_manager/strategy_manager.db");
  }

  if (QSqlDatabase::contains(m_connectionName)) {
    m_db = QSqlDatabase::database(m_connectionName);
  } else {
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(m_dbPath);
  }

  if (!m_db.open()) {
    qDebug() << "[StrategyRepository] Failed to open database:"
             << m_db.lastError().text();
    return false;
  }

  return ensureSchema();
}

void StrategyRepository::close() {
  if (m_db.isOpen()) {
    m_db.close();
  }
}

bool StrategyRepository::isOpen() const { return m_db.isOpen(); }

bool StrategyRepository::ensureSchema() {
  if (!m_db.isOpen()) {
    return false;
  }

  QSqlQuery query(m_db);
  QString sql = "CREATE TABLE IF NOT EXISTS strategy_instances ("
                "instance_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "instance_name TEXT NOT NULL,"
                "strategy_type TEXT NOT NULL,"
                "symbol TEXT,"
                "account TEXT,"
                "segment INTEGER,"
                "state TEXT NOT NULL,"
                "mtm REAL,"
                "stop_loss REAL,"
                "target REAL,"
                "entry_price REAL,"
                "quantity INTEGER,"
                "active_positions INTEGER,"
                "pending_orders INTEGER,"
                "parameters_json TEXT,"
                "created_at TEXT,"
                "last_updated TEXT,"
                "last_state_change TEXT,"
                "start_time TEXT,"
                "last_error TEXT,"
                "deleted INTEGER DEFAULT 0"
                ");";

  if (!query.exec(sql)) {
    qDebug() << "[StrategyRepository] Schema creation failed:"
             << query.lastError().text();
    return false;
  }

  return true;
}

qint64 StrategyRepository::saveInstance(StrategyInstance &instance) {
  if (!m_db.isOpen()) {
    return 0;
  }

  QSqlQuery query(m_db);
  query.prepare(
      "INSERT INTO strategy_instances (instance_name, strategy_type, symbol, "
      "account, segment, description, state, mtm, stop_loss, target, "
      "entry_price, quantity, active_positions, pending_orders, "
      "parameters_json, created_at, last_updated, last_state_change, "
      "start_time, last_error, deleted) "
      "VALUES (:instance_name, :strategy_type, :symbol, :account, :segment, "
      ":description, :state, :mtm, :stop_loss, :target, :entry_price, "
      ":quantity, :active_positions, :pending_orders, :parameters_json, "
      ":created_at, :last_updated, :last_state_change, :start_time, "
      ":last_error, 0)");

  QJsonDocument doc = QJsonDocument::fromVariant(instance.parameters);
  QString paramsJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

  query.bindValue(":instance_name", instance.instanceName);
  query.bindValue(":strategy_type", instance.strategyType);
  query.bindValue(":symbol", instance.symbol);
  query.bindValue(":account", instance.account);
  query.bindValue(":segment", instance.segment);
  query.bindValue(":description", instance.description);
  query.bindValue(":state", StrategyInstance::stateToString(instance.state));
  query.bindValue(":mtm", instance.mtm);
  query.bindValue(":stop_loss", instance.stopLoss);
  query.bindValue(":target", instance.target);
  query.bindValue(":entry_price", instance.entryPrice);
  query.bindValue(":quantity", instance.quantity);
  query.bindValue(":active_positions", instance.activePositions);
  query.bindValue(":pending_orders", instance.pendingOrders);
  query.bindValue(":parameters_json", paramsJson);
  query.bindValue(":created_at", toIsoString(instance.createdAt));
  query.bindValue(":last_updated", toIsoString(instance.lastUpdated));
  query.bindValue(":last_state_change", toIsoString(instance.lastStateChange));
  query.bindValue(":start_time", toIsoString(instance.startTime));
  query.bindValue(":last_error", instance.lastError);

  if (!query.exec()) {
    qDebug() << "[StrategyRepository] Insert failed:"
             << query.lastError().text();
    return 0;
  }

  instance.instanceId = query.lastInsertId().toLongLong();
  return instance.instanceId;
}

bool StrategyRepository::updateInstance(const StrategyInstance &instance) {
  if (!m_db.isOpen()) {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(
      "UPDATE strategy_instances SET "
      "instance_name = :instance_name, strategy_type = :strategy_type, symbol "
      "= :symbol, state = :state, mtm = :mtm, "
      "stop_loss = :stop_loss, target = :target, entry_price = :entry_price, "
      "quantity = :quantity, active_positions = :active_positions, "
      "pending_orders = :pending_orders, parameters_json = :parameters_json, "
      "created_at = :created_at, last_updated = :last_updated, "
      "last_state_change = :last_state_change, start_time = :start_time, "
      "last_error = :last_error "
      "WHERE instance_id = :instance_id");

  QJsonDocument doc = QJsonDocument::fromVariant(instance.parameters);
  QString paramsJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

  query.bindValue(":instance_id", instance.instanceId);
  query.bindValue(":instance_name", instance.instanceName);
  query.bindValue(":strategy_type", instance.strategyType);
  query.bindValue(":symbol", instance.symbol);
  query.bindValue(":state", StrategyInstance::stateToString(instance.state));
  query.bindValue(":mtm", instance.mtm);
  query.bindValue(":stop_loss", instance.stopLoss);
  query.bindValue(":target", instance.target);
  query.bindValue(":entry_price", instance.entryPrice);
  query.bindValue(":quantity", instance.quantity);
  query.bindValue(":active_positions", instance.activePositions);
  query.bindValue(":pending_orders", instance.pendingOrders);
  query.bindValue(":parameters_json", paramsJson);
  query.bindValue(":created_at", toIsoString(instance.createdAt));
  query.bindValue(":last_updated", toIsoString(instance.lastUpdated));
  query.bindValue(":last_state_change", toIsoString(instance.lastStateChange));
  query.bindValue(":start_time", toIsoString(instance.startTime));
  query.bindValue(":last_error", instance.lastError);

  if (!query.exec()) {
    qDebug() << "[StrategyRepository] Update failed:"
             << query.lastError().text();
    return false;
  }

  return true;
}

bool StrategyRepository::markDeleted(qint64 instanceId) {
  if (!m_db.isOpen()) {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare("UPDATE strategy_instances SET deleted = 1, state = 'DELETED' "
                "WHERE instance_id = :instance_id");
  query.bindValue(":instance_id", instanceId);

  if (!query.exec()) {
    qDebug() << "[StrategyRepository] Delete failed:"
             << query.lastError().text();
    return false;
  }

  return true;
}

QVector<StrategyInstance>
StrategyRepository::loadAllInstances(bool includeDeleted) {
  QVector<StrategyInstance> results;
  if (!m_db.isOpen()) {
    return results;
  }

  QSqlQuery query(m_db);
  QString sql = "SELECT * FROM strategy_instances";
  if (!includeDeleted) {
    sql += " WHERE deleted = 0";
  }
  sql += " ORDER BY instance_id ASC";

  if (!query.exec(sql)) {
    qDebug() << "[StrategyRepository] Load failed:" << query.lastError().text();
    return results;
  }

  while (query.next()) {
    results.append(fromQuery(query));
  }

  return results;
}

StrategyInstance StrategyRepository::fromQuery(QSqlQuery &query) const {
  StrategyInstance instance;
  instance.instanceId = query.value("instance_id").toLongLong();
  instance.instanceName = query.value("instance_name").toString();
  instance.strategyType = query.value("strategy_type").toString();
  instance.symbol = query.value("symbol").toString();
  instance.account = query.value("account").toString();
  instance.segment = query.value("segment").toInt();
  instance.description = query.value("description").toString();
  instance.state =
      StrategyInstance::stringToState(query.value("state").toString());
  instance.mtm = query.value("mtm").toDouble();
  instance.stopLoss = query.value("stop_loss").toDouble();
  instance.target = query.value("target").toDouble();
  instance.entryPrice = query.value("entry_price").toDouble();
  instance.quantity = query.value("quantity").toInt();
  instance.activePositions = query.value("active_positions").toInt();
  instance.pendingOrders = query.value("pending_orders").toInt();

  QString paramsJson = query.value("parameters_json").toString();
  if (!paramsJson.isEmpty()) {
    QJsonDocument doc = QJsonDocument::fromJson(paramsJson.toUtf8());
    if (doc.isObject()) {
      instance.parameters = doc.object().toVariantMap();
    }
  }

  instance.createdAt = fromIsoString(query.value("created_at").toString());
  instance.lastUpdated = fromIsoString(query.value("last_updated").toString());
  instance.lastStateChange =
      fromIsoString(query.value("last_state_change").toString());
  instance.startTime = fromIsoString(query.value("start_time").toString());
  instance.lastError = query.value("last_error").toString();

  return instance;
}

QString StrategyRepository::toIsoString(const QDateTime &value) const {
  return value.isValid() ? value.toString(Qt::ISODate) : QString();
}

QDateTime StrategyRepository::fromIsoString(const QString &value) const {
  if (value.isEmpty()) {
    return QDateTime();
  }
  return QDateTime::fromString(value, Qt::ISODate);
}
