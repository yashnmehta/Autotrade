#include "services/CandleAggregator.h"
#include "services/HistoricalDataStore.h"
#include "services/FeedHandler.h"
#include <QDebug>
#include <QDateTime>

CandleAggregator::CandleAggregator()
    : QObject(nullptr)
{
}

CandleAggregator::~CandleAggregator()
{
    if (m_timer.isActive()) {
        m_timer.stop();
    }
}

CandleAggregator& CandleAggregator::instance()
{
    static CandleAggregator instance;
    return instance;
}

void CandleAggregator::initialize(bool autoSave)
{
    if (m_initialized) {
        qWarning() << "[CandleAggregator] Already initialized";
        return;
    }
    
    m_autoSave = autoSave;
    
    // Timer to check for candle completion every second
    // This ensures candles complete even if no ticks arrive
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &CandleAggregator::checkCandleCompletion);
    m_timer.start();
    
    // Note: FeedHandler uses subscription model, not global signals
    // Ticks are received via subscribeTo() method which sets up per-symbol subscriptions
    
    m_initialized = true;
    qDebug() << "[CandleAggregator] ✅ Initialized (autoSave:" << m_autoSave << ")";
}

void CandleAggregator::subscribeTo(const QString& symbol, int segment,
                                   const QStringList& timeframes)
{
    QMutexLocker locker(&m_mutex);
    
    QString subKey = QString("%1_%2").arg(symbol).arg(segment);
    
    // Add to subscriptions
    if (!m_subscriptions.contains(subKey)) {
        m_subscriptions[subKey] = timeframes;
    } else {
        // Merge timeframes
        auto& existing = m_subscriptions[subKey];
        for (const QString& tf : timeframes) {
            if (!existing.contains(tf)) {
                existing.append(tf);
            }
        }
    }
    
    // Initialize builders for each timeframe
    qint64 now = QDateTime::currentSecsSinceEpoch();
    
    for (const QString& tf : timeframes) {
        QString key = makeKey(symbol, segment, tf);
        
        if (!m_builders.contains(key)) {
            CandleBuilder builder;
            builder.timeframe = ChartData::stringToTimeframe(tf);
            builder.startTime = ChartData::getCandleStartTime(now, builder.timeframe);
            builder.firstTick = true;
            
            m_builders[key] = builder;
            
            qDebug() << "[CandleAggregator] Subscribed:" << symbol << segment << tf
                     << "start:" << QDateTime::fromSecsSinceEpoch(builder.startTime).toString();
        }
    }
    
    qDebug() << "[CandleAggregator] Active subscriptions for" << symbol << ":" 
             << m_subscriptions[subKey];
}

void CandleAggregator::unsubscribeFrom(const QString& symbol, int segment)
{
    QMutexLocker locker(&m_mutex);
    
    QString subKey = QString("%1_%2").arg(symbol).arg(segment);
    
    if (!m_subscriptions.contains(subKey)) {
        return;
    }
    
    // Remove all builders for this symbol/segment
    auto it = m_builders.begin();
    while (it != m_builders.end()) {
        if (it.key().startsWith(subKey + "_")) {
            it = m_builders.erase(it);
        } else {
            ++it;
        }
    }
    
    m_subscriptions.remove(subKey);
    
    qDebug() << "[CandleAggregator] Unsubscribed:" << symbol << segment;
}

bool CandleAggregator::isSubscribed(const QString& symbol, int segment,
                                   const QString& timeframe) const
{
    QMutexLocker locker(&m_mutex);
    QString key = makeKey(symbol, segment, timeframe);
    return m_builders.contains(key);
}

ChartData::Candle CandleAggregator::getCurrentCandle(const QString& symbol, int segment,
                                                     const QString& timeframe) const
{
    QMutexLocker locker(&m_mutex);
    
    QString key = makeKey(symbol, segment, timeframe);
    
    if (m_builders.contains(key)) {
        return m_builders[key].build();
    }
    
    return ChartData::Candle(); // Invalid candle
}

QMap<QString, QStringList> CandleAggregator::getActiveSubscriptions() const
{
    QMutexLocker locker(&m_mutex);
    QMap<QString, QStringList> result;
    
    for (auto it = m_subscriptions.constBegin(); it != m_subscriptions.constEnd(); ++it) {
        result[it.key()] = it.value();
    }
    
    return result;
}

void CandleAggregator::onTick(const UDP::MarketTick& tick)
{
    // Find symbol from token (would need RepositoryManager integration)
    // For now, we work with all subscriptions and match by segment/token
    
    QMutexLocker locker(&m_mutex);
    
    // Iterate through all builders and update matching ones
    for (auto it = m_builders.begin(); it != m_builders.end(); ++it) {
        const QString& key = it.key();
        CandleBuilder& builder = it.value();
        
        // Parse key: "SYMBOL_SEGMENT_TIMEFRAME"
        QStringList parts = key.split('_');
        if (parts.size() < 3) continue;
        
        // QString symbol = parts[0];
        int segment = parts[1].toInt();
        QString timeframe = parts[2];
        
        // Match by segment (in full implementation, also match by token/symbol)
        if (static_cast<int>(tick.exchangeSegment) == segment) {
            // Update builder with this tick
            builder.update(tick);
            
            // Emit partial update for real-time chart
            emit candleUpdate(parts[0], segment, timeframe, builder.build());
        }
    }
}

void CandleAggregator::checkCandleCompletion()
{
    qint64 now = QDateTime::currentSecsSinceEpoch();
    
    QMutexLocker locker(&m_mutex);
    
    QStringList completedKeys;
    
    // Check all builders for completion
    for (auto it = m_builders.begin(); it != m_builders.end(); ++it) {
        const QString& key = it.key();
        CandleBuilder& builder = it.value();
        
        if (builder.shouldComplete(now)) {
            completedKeys.append(key);
        }
    }
    
    // Complete candles outside the iteration to avoid issues
    for (const QString& key : completedKeys) {
        QStringList parts = key.split('_');
        if (parts.size() >= 3) {
            QString symbol = parts[0];
            int segment = parts[1].toInt();
            QString timeframe = parts[2];
            
            completeCandle(key, symbol, segment, timeframe);
        }
    }
}

void CandleAggregator::completeCandle(const QString& key, const QString& symbol,
                                      int segment, const QString& timeframe)
{
    if (!m_builders.contains(key)) {
        return;
    }
    
    CandleBuilder& builder = m_builders[key];
    
    // Build the completed candle
    ChartData::Candle completedCandle = builder.build();
    
    // Only emit if candle has valid data
    if (completedCandle.isValid()) {
        qDebug() << "[CandleAggregator] Candle complete:" << symbol << timeframe
                 << "O:" << completedCandle.open
                 << "H:" << completedCandle.high
                 << "L:" << completedCandle.low
                 << "C:" << completedCandle.close
                 << "V:" << completedCandle.volume;
        
        emit candleComplete(symbol, segment, timeframe, completedCandle);
        
        // Auto-save to database
        if (m_autoSave) {
            HistoricalDataStore::instance().saveCandle(symbol, segment, timeframe, completedCandle);
        }
    }
    
    // Start new candle period
    qint64 duration = ChartData::timeframeDuration(builder.timeframe);
    qint64 newStartTime = builder.startTime + duration;
    builder.reset(newStartTime);
    
    qDebug() << "[CandleAggregator] New candle period started:" << symbol << timeframe
             << "at" << QDateTime::fromSecsSinceEpoch(newStartTime).toString();
}

QString CandleAggregator::makeKey(const QString& symbol, int segment,
                                  const QString& timeframe) const
{
    return QString("%1_%2_%3").arg(symbol).arg(segment).arg(timeframe);
}
