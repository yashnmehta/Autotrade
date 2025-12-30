#include "services/FeedHandler.h"
#include "utils/LatencyTracker.h"
#include <iostream>
#include <QDebug>
#include <algorithm>

FeedHandler& FeedHandler::instance() {
    static FeedHandler s_instance;
    return s_instance;
}

FeedHandler::FeedHandler() {
    qDebug() << "[FeedHandler] Initialized - Event-driven push updates (no polling)";
}

FeedHandler::~FeedHandler() {
    clear();
}

FeedHandler::SubscriptionID FeedHandler::subscribe(int token, TickCallback callback, QObject* owner) {
    if (!callback) {
        qWarning() << "[FeedHandler] Null callback provided for token" << token;
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Create subscription
    auto sub = std::make_shared<Subscription>();
    sub->id = m_nextID.fetch_add(1, std::memory_order_relaxed);
    sub->token = token;
    sub->callback = std::move(callback);
    sub->owner = owner;

    // Add to token subscribers
    m_subscribers[token].push_back(sub);

    // Add to ID lookup
    m_subscriptionById[sub->id] = sub;

    // Add to owner lookup (for automatic cleanup)
    if (owner) {
        m_subscriptionsByOwner[owner].push_back(sub->id);
        
        // Connect to destroyed signal for automatic cleanup
        connect(owner, &QObject::destroyed, this, [this, owner]() {
            unsubscribeAll(owner);
        }, Qt::DirectConnection);
    }

    qDebug() << "[FeedHandler] Subscribed to token" << token 
             << "- ID:" << sub->id 
             << "- Total subscribers:" << m_subscribers[token].size();

    emit subscriptionCountChanged(token, m_subscribers[token].size());

    return sub->id;
}

std::vector<FeedHandler::SubscriptionID> FeedHandler::subscribe(
    const std::vector<int>& tokens,
    TickCallback callback,
    QObject* owner) {
    
    std::vector<SubscriptionID> ids;
    ids.reserve(tokens.size());

    for (int token : tokens) {
        SubscriptionID id = subscribe(token, callback, owner);
        if (id > 0) {
            ids.push_back(id);
        }
    }

    qDebug() << "[FeedHandler] Batch subscribed to" << tokens.size() << "tokens";

    return ids;
}

void FeedHandler::unsubscribe(SubscriptionID id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find subscription
    auto it = m_subscriptionById.find(id);
    if (it == m_subscriptionById.end()) {
        qWarning() << "[FeedHandler] Unsubscribe failed - ID not found:" << id;
        return;
    }

    auto sub = it->second;
    int token = sub->token;

    // Remove from token subscribers
    auto& subs = m_subscribers[token];
    subs.erase(
        std::remove_if(subs.begin(), subs.end(),
            [id](const std::shared_ptr<Subscription>& s) {
                return s->id == id;
            }),
        subs.end()
    );

    // Remove empty token entries
    if (subs.empty()) {
        m_subscribers.erase(token);
    }

    // Remove from owner lookup
    if (sub->owner) {
        auto& ownerSubs = m_subscriptionsByOwner[sub->owner];
        ownerSubs.erase(
            std::remove(ownerSubs.begin(), ownerSubs.end(), id),
            ownerSubs.end()
        );
        
        if (ownerSubs.empty()) {
            m_subscriptionsByOwner.erase(sub->owner);
        }
    }

    // Remove from ID lookup
    m_subscriptionById.erase(it);

    qDebug() << "[FeedHandler] Unsubscribed from token" << token 
             << "- ID:" << id
             << "- Remaining subscribers:" << (m_subscribers.count(token) ? m_subscribers[token].size() : 0);

    emit subscriptionCountChanged(token, m_subscribers.count(token) ? m_subscribers[token].size() : 0);
}

void FeedHandler::unsubscribe(const std::vector<SubscriptionID>& ids) {
    for (SubscriptionID id : ids) {
        unsubscribe(id);
    }
}

void FeedHandler::unsubscribeAll(QObject* owner) {
    if (!owner) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_subscriptionsByOwner.find(owner);
    if (it == m_subscriptionsByOwner.end()) {
        return;
    }

    std::vector<SubscriptionID> ids = it->second;
    m_subscriptionsByOwner.erase(it);

    // Unlock before calling unsubscribe (which will lock again)
    // But we copied the IDs, so this is safe
    for (SubscriptionID id : ids) {
        // Note: This will try to lock again, but we already removed from owner map
        // so it won't try to modify owner map again
        auto subIt = m_subscriptionById.find(id);
        if (subIt != m_subscriptionById.end()) {
            auto sub = subIt->second;
            int token = sub->token;

            // Remove from token subscribers
            auto& subs = m_subscribers[token];
            subs.erase(
                std::remove_if(subs.begin(), subs.end(),
                    [id](const std::shared_ptr<Subscription>& s) {
                        return s->id == id;
                    }),
                subs.end()
            );

            if (subs.empty()) {
                m_subscribers.erase(token);
            }

            // Remove from ID lookup
            m_subscriptionById.erase(subIt);

            emit subscriptionCountChanged(token, m_subscribers.count(token) ? m_subscribers[token].size() : 0);
        }
    }

    qDebug() << "[FeedHandler] Unsubscribed all for owner" << owner 
             << "- Removed" << ids.size() << "subscriptions";
}

void FeedHandler::onTickReceived(const XTS::Tick& tick) {
    int token = tick.exchangeInstrumentID;
    
    // Mark FeedHandler processing timestamp
    XTS::Tick trackedTick = tick;
    trackedTick.timestampFeedHandler = LatencyTracker::now();
    
    


    // Read lock - find subscribers
    std::vector<std::shared_ptr<Subscription>> subscribers;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_subscribers.find(token);
        if (it != m_subscribers.end()) {
            subscribers = it->second;  // Copy shared_ptrs (cheap)
        }
    }

    // Invoke callbacks OUTSIDE lock (avoid deadlocks)
    if (!subscribers.empty()) {
        for (const auto& sub : subscribers) {
            try {
                sub->callback(trackedTick);
            } catch (const std::exception& e) {
                qWarning() << "[FeedHandler] Callback exception for token" << token 
                          << "- ID:" << sub->id << "-" << e.what();
            } catch (...) {
                qWarning() << "[FeedHandler] Unknown callback exception for token" << token
                          << "- ID:" << sub->id;
            }
        }
    }
}

void FeedHandler::onTickBatch(const std::vector<XTS::Tick>& ticks) {
    if (ticks.empty()) return;

    // Batch processing - more efficient than individual calls
    // because we can reuse the lock and callback vector allocation
    
    std::unordered_map<int, std::vector<std::shared_ptr<Subscription>>> subscribersByToken;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Collect all unique tokens
        for (const auto& tick : ticks) {
            int token = tick.exchangeInstrumentID;
            auto it = m_subscribers.find(token);
            if (it != m_subscribers.end()) {
                subscribersByToken[token] = it->second;
            }
        }
    }

    // Invoke callbacks OUTSIDE lock 
    for (const auto& tick : ticks) {
        int token = tick.exchangeInstrumentID;
        auto it = subscribersByToken.find(token);
        if (it != subscribersByToken.end()) {
            for (const auto& sub : it->second) {
                try {
                    sub->callback(tick);
                } catch (const std::exception& e) {
                    qWarning() << "[FeedHandler] Batch callback exception for token" << token
                              << "- ID:" << sub->id << "-" << e.what();
                } catch (...) {
                    qWarning() << "[FeedHandler] Unknown batch callback exception for token" << token
                              << "- ID:" << sub->id;
                }
            }
        }
    }

    qDebug() << "[FeedHandler] Processed batch of" << ticks.size() << "ticks";
}

size_t FeedHandler::subscriberCount(int token) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_subscribers.find(token);
    return (it != m_subscribers.end()) ? it->second.size() : 0;
}

size_t FeedHandler::totalSubscriptions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_subscriptionById.size();
}

std::vector<int> FeedHandler::getSubscribedTokens() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<int> tokens;
    tokens.reserve(m_subscribers.size());
    
    for (const auto& pair : m_subscribers) {
        tokens.push_back(pair.first);
    }
    
    return tokens;
}

void FeedHandler::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t count = m_subscriptionById.size();
    
    m_subscribers.clear();
    m_subscriptionById.clear();
    m_subscriptionsByOwner.clear();
    
    if (count > 0) {
        qDebug() << "[FeedHandler] Cleared all subscriptions (" << count << "total)";
    }
}
