#include "services/TokenSubscriptionManager.h"
#include <QDebug>

TokenSubscriptionManager* TokenSubscriptionManager::s_instance = nullptr;

TokenSubscriptionManager* TokenSubscriptionManager::instance()
{
    if (!s_instance) {
        s_instance = new TokenSubscriptionManager(nullptr);
        qDebug() << "[TokenSubscriptionManager] Singleton instance created";
    }
    return s_instance;
}

void TokenSubscriptionManager::destroy()
{
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
        qDebug() << "[TokenSubscriptionManager] Singleton instance destroyed";
    }
}

TokenSubscriptionManager::TokenSubscriptionManager(QObject *parent)
    : QObject(parent)
{
}

void TokenSubscriptionManager::subscribe(const QString &exchange, int token)
{
    if (exchange.isEmpty()) {
        qWarning() << "[TokenSubscriptionManager] Cannot subscribe: empty exchange name";
        return;
    }
    
    if (token <= 0) {
        qWarning() << "[TokenSubscriptionManager] Cannot subscribe: invalid token" << token;
        return;
    }
    
    // Create exchange entry if it doesn't exist
    if (!m_subscriptions.contains(exchange)) {
        m_subscriptions[exchange] = QSet<int>();
    }
    
    // Check if already subscribed
    if (m_subscriptions[exchange].contains(token)) {
        qDebug() << "[TokenSubscriptionManager] Already subscribed:" << exchange << "token" << token;
        return;
    }
    
    // Add subscription
    m_subscriptions[exchange].insert(token);
    int count = m_subscriptions[exchange].size();
    
    qDebug() << "[TokenSubscriptionManager] Subscribed:" << exchange
             << "Token:" << token << "Total:" << count;
    
    emit tokenSubscribed(exchange, token);
    emit exchangeSubscriptionsChanged(exchange, count);
}

void TokenSubscriptionManager::unsubscribe(const QString &exchange, int token)
{
    if (!m_subscriptions.contains(exchange)) {
        qWarning() << "[TokenSubscriptionManager] Cannot unsubscribe: exchange" << exchange << "not found";
        return;
    }
    
    if (m_subscriptions[exchange].remove(token)) {
        int count = m_subscriptions[exchange].size();
        
        qDebug() << "[TokenSubscriptionManager] Unsubscribed:" << exchange
                 << "Token:" << token << "Remaining:" << count;
        
        emit tokenUnsubscribed(exchange, token);
        emit exchangeSubscriptionsChanged(exchange, count);
        
        // Clean up empty exchange entries
        if (m_subscriptions[exchange].isEmpty()) {
            m_subscriptions.remove(exchange);
            qDebug() << "[TokenSubscriptionManager] Removed empty exchange:" << exchange;
        }
    } else {
        qWarning() << "[TokenSubscriptionManager] Token" << token
                   << "not found in" << exchange << "subscriptions";
    }
}

void TokenSubscriptionManager::unsubscribeAll(const QString &exchange)
{
    if (m_subscriptions.contains(exchange)) {
        int count = m_subscriptions[exchange].size();
        m_subscriptions.remove(exchange);
        
        qDebug() << "[TokenSubscriptionManager] Cleared all subscriptions for" << exchange
                 << "(" << count << "tokens)";
        
        emit exchangeSubscriptionsChanged(exchange, 0);
    } else {
        qWarning() << "[TokenSubscriptionManager] Exchange" << exchange << "not found";
    }
}

void TokenSubscriptionManager::clearAll()
{
    int totalCount = totalSubscriptions();
    m_subscriptions.clear();
    
    qDebug() << "[TokenSubscriptionManager] Cleared all subscriptions"
             << "(" << totalCount << "tokens across all exchanges)";
    
    emit allSubscriptionsCleared();
}

void TokenSubscriptionManager::subscribeBatch(const QString &exchange, const QList<int> &tokens)
{
    if (exchange.isEmpty()) {
        qWarning() << "[TokenSubscriptionManager] Cannot batch subscribe: empty exchange name";
        return;
    }
    
    if (tokens.isEmpty()) {
        return;
    }
    
    // Create exchange entry if it doesn't exist
    if (!m_subscriptions.contains(exchange)) {
        m_subscriptions[exchange] = QSet<int>();
    }
    
    int addedCount = 0;
    for (int token : tokens) {
        if (token > 0 && !m_subscriptions[exchange].contains(token)) {
            m_subscriptions[exchange].insert(token);
            emit tokenSubscribed(exchange, token);
            ++addedCount;
        }
    }
    
    if (addedCount > 0) {
        int totalCount = m_subscriptions[exchange].size();
        qDebug() << "[TokenSubscriptionManager] Batch subscribed:" << exchange
                 << "Added:" << addedCount << "Total:" << totalCount;
        
        emit exchangeSubscriptionsChanged(exchange, totalCount);
    }
}

void TokenSubscriptionManager::unsubscribeBatch(const QString &exchange, const QList<int> &tokens)
{
    if (!m_subscriptions.contains(exchange)) {
        qWarning() << "[TokenSubscriptionManager] Cannot batch unsubscribe: exchange"
                   << exchange << "not found";
        return;
    }
    
    if (tokens.isEmpty()) {
        return;
    }
    
    int removedCount = 0;
    for (int token : tokens) {
        if (m_subscriptions[exchange].remove(token)) {
            emit tokenUnsubscribed(exchange, token);
            ++removedCount;
        }
    }
    
    if (removedCount > 0) {
        int remainingCount = m_subscriptions[exchange].size();
        qDebug() << "[TokenSubscriptionManager] Batch unsubscribed:" << exchange
                 << "Removed:" << removedCount << "Remaining:" << remainingCount;
        
        emit exchangeSubscriptionsChanged(exchange, remainingCount);
        
        // Clean up empty exchange entries
        if (m_subscriptions[exchange].isEmpty()) {
            m_subscriptions.remove(exchange);
            qDebug() << "[TokenSubscriptionManager] Removed empty exchange:" << exchange;
        }
    }
}

QSet<int> TokenSubscriptionManager::getSubscribedTokens(const QString &exchange) const
{
    return m_subscriptions.value(exchange, QSet<int>());
}

QList<QString> TokenSubscriptionManager::getSubscribedExchanges() const
{
    return m_subscriptions.keys();
}

bool TokenSubscriptionManager::isSubscribed(const QString &exchange, int token) const
{
    return m_subscriptions.value(exchange, QSet<int>()).contains(token);
}

int TokenSubscriptionManager::totalSubscriptions() const
{
    int total = 0;
    for (const QSet<int> &tokens : m_subscriptions) {
        total += tokens.size();
    }
    return total;
}

int TokenSubscriptionManager::getSubscriptionCount(const QString &exchange) const
{
    return m_subscriptions.value(exchange, QSet<int>()).size();
}

void TokenSubscriptionManager::dump() const
{
    qDebug() << "╔═══════════════════════════════════════════════════════╗";
    qDebug() << "║    TokenSubscriptionManager Dump                      ║";
    qDebug() << "╠═══════════════════════════════════════════════════════╣";
    
    if (m_subscriptions.isEmpty()) {
        qDebug() << "║   (no subscriptions)";
    } else {
        for (auto it = m_subscriptions.constBegin(); it != m_subscriptions.constEnd(); ++it) {
            QString exchange = it.key();
            const QSet<int> &tokens = it.value();
            
            qDebug() << "║";
            qDebug() << "║ Exchange:" << exchange;
            qDebug() << "║   Tokens:" << tokens.size();
            
            // Show first 10 tokens
            QList<int> tokenList = tokens.values();
            std::sort(tokenList.begin(), tokenList.end());
            
            QString tokensStr;
            int displayCount = qMin(10, tokenList.size());
            for (int i = 0; i < displayCount; ++i) {
                if (i > 0) tokensStr += ", ";
                tokensStr += QString::number(tokenList[i]);
            }
            
            if (tokenList.size() > 10) {
                tokensStr += QString(" ... (+%1 more)").arg(tokenList.size() - 10);
            }
            
            qDebug() << "║   " << tokensStr;
        }
    }
    
    qDebug() << "║";
    qDebug() << "║ Total Statistics:";
    qDebug() << "║   Exchanges:" << m_subscriptions.size();
    qDebug() << "║   Total subscriptions:" << totalSubscriptions();
    qDebug() << "╚═══════════════════════════════════════════════════════╝";
}

QMap<QString, int> TokenSubscriptionManager::getStatistics() const
{
    QMap<QString, int> stats;
    for (auto it = m_subscriptions.constBegin(); it != m_subscriptions.constEnd(); ++it) {
        stats[it.key()] = it.value().size();
    }
    return stats;
}
