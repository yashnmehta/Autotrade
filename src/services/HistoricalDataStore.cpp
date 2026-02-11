#include "services/HistoricalDataStore.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QJsonDocument>
#include <QDateTime>

HistoricalDataStore::HistoricalDataStore()
    : QObject(nullptr)
{
}

HistoricalDataStore::~HistoricalDataStore()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

HistoricalDataStore& HistoricalDataStore::instance()
{
    static HistoricalDataStore instance;
    return instance;
}

bool HistoricalDataStore::initialize(const QString& dbPath)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        qWarning() << "[HistoricalDataStore] Already initialized";
        return true;
    }
    
    // Determine database path
    if (dbPath.isEmpty()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDir);
        m_dbPath = QDir(dataDir).filePath("chart_history.db");
    } else {
        m_dbPath = dbPath;
    }
    
    // Open/create database
    m_db = QSqlDatabase::addDatabase("QSQLITE", "chart_history");
    m_db.setDatabaseName(m_dbPath);
    
    if (!m_db.open()) {
        qCritical() << "[HistoricalDataStore] Failed to open database:"
                    << m_db.lastError().text();
        return false;
    }
    
    qDebug() << "[HistoricalDataStore] Database opened:" << m_dbPath;
    
    // Create tables
    if (!createTables()) {
        qCritical() << "[HistoricalDataStore] Failed to create tables";
        return false;
    }
    
    m_initialized = true;
    qDebug() << "[HistoricalDataStore] ✅ Initialized successfully";
    
    return true;
}

bool HistoricalDataStore::createTables()
{
    QSqlQuery query(m_db);
    
    // Candles table
    QString createCandles = R"(
        CREATE TABLE IF NOT EXISTS candles (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            symbol TEXT NOT NULL,
            segment INTEGER NOT NULL,
            timeframe TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            open REAL NOT NULL,
            high REAL NOT NULL,
            low REAL NOT NULL,
            close REAL NOT NULL,
            volume INTEGER DEFAULT 0,
            open_interest INTEGER DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(symbol, segment, timeframe, timestamp)
        )
    )";
    
    if (!query.exec(createCandles)) {
        qCritical() << "[HistoricalDataStore] Failed to create candles table:"
                    << query.lastError().text();
        return false;
    }
    
    // Create index for fast lookups
    QString createIndex = R"(
        CREATE INDEX IF NOT EXISTS idx_candles_lookup 
        ON candles(symbol, segment, timeframe, timestamp)
    )";
    
    if (!query.exec(createIndex)) {
        qWarning() << "[HistoricalDataStore] Failed to create index:"
                   << query.lastError().text();
    }
    
    // Indicator values table
    QString createIndicators = R"(
        CREATE TABLE IF NOT EXISTS indicator_values (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            candle_id INTEGER NOT NULL,
            indicator_name TEXT NOT NULL,
            value REAL NOT NULL,
            metadata TEXT,
            FOREIGN KEY(candle_id) REFERENCES candles(id) ON DELETE CASCADE
        )
    )";
    
    if (!query.exec(createIndicators)) {
        qCritical() << "[HistoricalDataStore] Failed to create indicators table:"
                    << query.lastError().text();
        return false;
    }
    
    QString createIndicatorIndex = R"(
        CREATE INDEX IF NOT EXISTS idx_indicator_lookup 
        ON indicator_values(candle_id, indicator_name)
    )";
    
    if (!query.exec(createIndicatorIndex)) {
        qWarning() << "[HistoricalDataStore] Failed to create indicator index:"
                   << query.lastError().text();
    }
    
    return true;
}

bool HistoricalDataStore::saveCandle(const QString& symbol, int segment,
                                    const QString& timeframe,
                                    const ChartData::Candle& candle)
{
    if (!m_initialized) {
        qWarning() << "[HistoricalDataStore] Not initialized";
        return false;
    }
    
    if (!candle.isValid()) {
        qWarning() << "[HistoricalDataStore] Invalid candle data";
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO candles 
        (symbol, segment, timeframe, timestamp, open, high, low, close, volume, open_interest)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    
    query.addBindValue(symbol);
    query.addBindValue(segment);
    query.addBindValue(timeframe);
    query.addBindValue(candle.timestamp);
    query.addBindValue(candle.open);
    query.addBindValue(candle.high);
    query.addBindValue(candle.low);
    query.addBindValue(candle.close);
    query.addBindValue(candle.volume);
    query.addBindValue(candle.openInterest);
    
    if (!query.exec()) {
        qWarning() << "[HistoricalDataStore] Failed to save candle:"
                   << query.lastError().text();
        return false;
    }
    
    emit candleSaved(symbol, segment, timeframe, candle);
    return true;
}

int HistoricalDataStore::saveCandleBatch(const QString& symbol, int segment,
                                        const QString& timeframe,
                                        const QVector<ChartData::Candle>& candles)
{
    if (!m_initialized || candles.isEmpty()) {
        return 0;
    }
    
    QMutexLocker locker(&m_mutex);
    
    // Begin transaction for batch insert
    m_db.transaction();
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO candles 
        (symbol, segment, timeframe, timestamp, open, high, low, close, volume, open_interest)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    
    int saved = 0;
    for (const auto& candle : candles) {
        if (!candle.isValid()) continue;
        
        query.addBindValue(symbol);
        query.addBindValue(segment);
        query.addBindValue(timeframe);
        query.addBindValue(candle.timestamp);
        query.addBindValue(candle.open);
        query.addBindValue(candle.high);
        query.addBindValue(candle.low);
        query.addBindValue(candle.close);
        query.addBindValue(candle.volume);
        query.addBindValue(candle.openInterest);
        
        if (query.exec()) {
            saved++;
        }
    }
    
    m_db.commit();
    
    qDebug() << "[HistoricalDataStore] Saved batch:" << saved << "candles for"
             << symbol << timeframe;
    
    return saved;
}

QVector<ChartData::Candle> HistoricalDataStore::getCandles(
    const QString& symbol, int segment, const QString& timeframe,
    qint64 startTime, qint64 endTime)
{
    QVector<ChartData::Candle> result;
    
    if (!m_initialized) {
        return result;
    }
    
    QMutexLocker locker(&m_mutex);
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT timestamp, open, high, low, close, volume, open_interest
        FROM candles
        WHERE symbol = ? AND segment = ? AND timeframe = ?
          AND timestamp >= ? AND timestamp <= ?
        ORDER BY timestamp ASC
    )");
    
    query.addBindValue(symbol);
    query.addBindValue(segment);
    query.addBindValue(timeframe);
    query.addBindValue(startTime);
    query.addBindValue(endTime);
    
    if (!query.exec()) {
        qWarning() << "[HistoricalDataStore] Query failed:" << query.lastError().text();
        return result;
    }
    
    while (query.next()) {
        ChartData::Candle candle(
            query.value(0).toLongLong(),  // timestamp
            query.value(1).toDouble(),     // open
            query.value(2).toDouble(),     // high
            query.value(3).toDouble(),     // low
            query.value(4).toDouble(),     // close
            query.value(5).toLongLong(),   // volume
            query.value(6).toLongLong()    // open_interest
        );
        result.append(candle);
    }
    
    return result;
}

QVector<ChartData::Candle> HistoricalDataStore::getRecentCandles(
    const QString& symbol, int segment, const QString& timeframe, int count)
{
    QVector<ChartData::Candle> result;
    
    if (!m_initialized) {
        return result;
    }
    
    QMutexLocker locker(&m_mutex);
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT timestamp, open, high, low, close, volume, open_interest
        FROM candles
        WHERE symbol = ? AND segment = ? AND timeframe = ?
        ORDER BY timestamp DESC
        LIMIT ?
    )");
    
    query.addBindValue(symbol);
    query.addBindValue(segment);
    query.addBindValue(timeframe);
    query.addBindValue(count);
    
    if (!query.exec()) {
        qWarning() << "[HistoricalDataStore] Query failed:" << query.lastError().text();
        return result;
    }
    
    // Read in reverse order, then reverse the vector
    QVector<ChartData::Candle> temp;
    while (query.next()) {
        ChartData::Candle candle(
            query.value(0).toLongLong(),
            query.value(1).toDouble(),
            query.value(2).toDouble(),
            query.value(3).toDouble(),
            query.value(4).toDouble(),
            query.value(5).toLongLong(),
            query.value(6).toLongLong()
        );
        temp.append(candle);
    }
    
    // Reverse to get chronological order (oldest first)
    for (int i = temp.size() - 1; i >= 0; --i) {
        result.append(temp[i]);
    }
    
    return result;
}

ChartData::Candle HistoricalDataStore::getLastCandle(
    const QString& symbol, int segment, const QString& timeframe)
{
    auto candles = getRecentCandles(symbol, segment, timeframe, 1);
    if (!candles.isEmpty()) {
        return candles.last();
    }
    return ChartData::Candle(); // Invalid candle
}

bool HistoricalDataStore::candleExists(const QString& symbol, int segment,
                                      const QString& timeframe, qint64 timestamp)
{
    if (!m_initialized) {
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT COUNT(*) FROM candles
        WHERE symbol = ? AND segment = ? AND timeframe = ? AND timestamp = ?
    )");
    
    query.addBindValue(symbol);
    query.addBindValue(segment);
    query.addBindValue(timeframe);
    query.addBindValue(timestamp);
    
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    
    return false;
}

bool HistoricalDataStore::saveIndicatorValue(qint64 candleId,
                                            const QString& indicatorName,
                                            double value,
                                            const QVariantMap& metadata)
{
    if (!m_initialized) {
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    
    QString metadataJson;
    if (!metadata.isEmpty()) {
        metadataJson = QString::fromUtf8(
            QJsonDocument::fromVariant(metadata).toJson(QJsonDocument::Compact)
        );
    }
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO indicator_values (candle_id, indicator_name, value, metadata)
        VALUES (?, ?, ?, ?)
    )");
    
    query.addBindValue(candleId);
    query.addBindValue(indicatorName);
    query.addBindValue(value);
    query.addBindValue(metadataJson);
    
    if (!query.exec()) {
        qWarning() << "[HistoricalDataStore] Failed to save indicator:"
                   << query.lastError().text();
        return false;
    }
    
    return true;
}

QVector<QPair<qint64, double>> HistoricalDataStore::getIndicatorValues(
    const QString& symbol, int segment, const QString& timeframe,
    const QString& indicatorName, qint64 startTime, qint64 endTime)
{
    QVector<QPair<qint64, double>> result;
    
    if (!m_initialized) {
        return result;
    }
    
    QMutexLocker locker(&m_mutex);
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT c.timestamp, i.value
        FROM indicator_values i
        JOIN candles c ON i.candle_id = c.id
        WHERE c.symbol = ? AND c.segment = ? AND c.timeframe = ?
          AND i.indicator_name = ?
          AND c.timestamp >= ? AND c.timestamp <= ?
        ORDER BY c.timestamp ASC
    )");
    
    query.addBindValue(symbol);
    query.addBindValue(segment);
    query.addBindValue(timeframe);
    query.addBindValue(indicatorName);
    query.addBindValue(startTime);
    query.addBindValue(endTime);
    
    if (!query.exec()) {
        qWarning() << "[HistoricalDataStore] Query failed:" << query.lastError().text();
        return result;
    }
    
    while (query.next()) {
        result.append(qMakePair(
            query.value(0).toLongLong(),
            query.value(1).toDouble()
        ));
    }
    
    return result;
}

int HistoricalDataStore::deleteOldCandles(qint64 olderThan)
{
    if (!m_initialized) {
        return 0;
    }
    
    QMutexLocker locker(&m_mutex);
    
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM candles WHERE timestamp < ?");
    query.addBindValue(olderThan);
    
    if (!query.exec()) {
        qWarning() << "[HistoricalDataStore] Delete failed:" << query.lastError().text();
        return 0;
    }
    
    int deleted = query.numRowsAffected();
    qDebug() << "[HistoricalDataStore] Deleted" << deleted << "old candles";
    
    return deleted;
}

QVariantMap HistoricalDataStore::getStatistics()
{
    QVariantMap stats;
    
    if (!m_initialized) {
        return stats;
    }
    
    QMutexLocker locker(&m_mutex);
    
    // Total candles
    QSqlQuery query(m_db);
    if (query.exec("SELECT COUNT(*) FROM candles") && query.next()) {
        stats["totalCandles"] = query.value(0).toInt();
    }
    
    // Unique symbols
    if (query.exec("SELECT COUNT(DISTINCT symbol) FROM candles") && query.next()) {
        stats["symbols"] = query.value(0).toInt();
    }
    
    // Oldest/newest candle
    if (query.exec("SELECT MIN(timestamp), MAX(timestamp) FROM candles") && query.next()) {
        stats["oldestCandle"] = QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong())
                                    .toString(Qt::ISODate);
        stats["newestCandle"] = QDateTime::fromSecsSinceEpoch(query.value(1).toLongLong())
                                    .toString(Qt::ISODate);
    }
    
    return stats;
}
