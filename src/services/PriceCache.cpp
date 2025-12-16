#include "services/PriceCache.h"
#include <QDebug>

PriceCache* PriceCache::s_instance = nullptr;

PriceCache* PriceCache::instance() {
    if (!s_instance) {
        s_instance = new PriceCache();
    }
    return s_instance;
}

PriceCache::PriceCache(QObject *parent)
    : QObject(parent)
{
    qDebug() << "✅ PriceCache initialized";
}

void PriceCache::updatePrice(int token, const XTS::Tick &tick) {
    QWriteLocker locker(&m_lock);
    
    CachedPrice cached;
    cached.tick = tick;
    cached.timestamp = QDateTime::currentDateTime();
    
    m_cache[token] = cached;
    
    locker.unlock();
    
    emit priceUpdated(token, tick);
    
    qDebug() << "💾 Cached price for token:" << token 
             << "LTP:" << tick.lastTradedPrice
             << "Time:" << cached.timestamp.toString("hh:mm:ss");
}

std::optional<XTS::Tick> PriceCache::getPrice(int token) const {
    QReadLocker locker(&m_lock);
    
    auto it = m_cache.find(token);
    if (it != m_cache.end()) {
        qDebug() << "📦 Retrieved cached price for token:" << token
                 << "LTP:" << it->tick.lastTradedPrice
                 << "Age:" << it->timestamp.secsTo(QDateTime::currentDateTime()) << "seconds";
        return it->tick;
    }
    
    qDebug() << "❌ No cached price for token:" << token;
    return std::nullopt;
}

bool PriceCache::hasPrice(int token) const {
    QReadLocker locker(&m_lock);
    return m_cache.contains(token);
}

void PriceCache::clearStale(int maxAgeSeconds) {
    QWriteLocker locker(&m_lock);
    
    QDateTime now = QDateTime::currentDateTime();
    QList<int> tokensToRemove;
    
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        int age = it->timestamp.secsTo(now);
        if (age > maxAgeSeconds) {
            tokensToRemove.append(it.key());
        }
    }
    
    for (int token : tokensToRemove) {
        m_cache.remove(token);
        qDebug() << "🧹 Removed stale price for token:" << token;
    }
    
    if (!tokensToRemove.isEmpty()) {
        qDebug() << "🧹 Cleared" << tokensToRemove.size() << "stale prices";
    }
}

QList<int> PriceCache::getAllTokens() const {
    QReadLocker locker(&m_lock);
    return m_cache.keys();
}

int PriceCache::size() const {
    QReadLocker locker(&m_lock);
    return m_cache.size();
}

void PriceCache::clear() {
    QWriteLocker locker(&m_lock);
    int count = m_cache.size();
    m_cache.clear();
    qDebug() << "🧹 Cleared all cached prices (" << count << "items)";
}
