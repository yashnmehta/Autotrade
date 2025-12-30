#include "services/FeedHandler.h"
#include "utils/LatencyTracker.h"
#include <QDebug>

FeedHandler& FeedHandler::instance() {
    static FeedHandler s_instance;
    return s_instance;
}

FeedHandler::FeedHandler() {
    qDebug() << "[FeedHandler] Initialized - TokenPublisher based (Signal/Slot)";
}

FeedHandler::~FeedHandler() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_publishers) {
        delete pair.second;
    }
    m_publishers.clear();
}

TokenPublisher* FeedHandler::getOrCreatePublisher(int token) {
    auto it = m_publishers.find(token);
    if (it != m_publishers.end()) {
        return it->second;
    }

    TokenPublisher* pub = new TokenPublisher(token, this);
    m_publishers[token] = pub;
    return pub;
}

void FeedHandler::unsubscribe(int token, QObject* receiver) {
    if (!receiver) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_publishers.find(token);
    if (it != m_publishers.end()) {
        disconnect(it->second, &TokenPublisher::tickUpdated, receiver, nullptr);
    }
}

void FeedHandler::unsubscribeAll(QObject* receiver) {
    if (!receiver) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_publishers) {
        disconnect(pair.second, &TokenPublisher::tickUpdated, receiver, nullptr);
    }
    qDebug() << "[FeedHandler] Disconnected all subscriptions for receiver" << receiver;
}

void FeedHandler::onTickReceived(const XTS::Tick& tick) {
    int token = (int)tick.exchangeInstrumentID;
    
    // Mark FeedHandler processing timestamp
    XTS::Tick trackedTick = tick;
    trackedTick.timestampFeedHandler = LatencyTracker::now();
    
    TokenPublisher* pub = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_publishers.find(token);
        if (it != m_publishers.end()) {
            pub = it->second;
        }
    }

    if (pub) {
        pub->publish(trackedTick);
    }
}

size_t FeedHandler::totalSubscriptions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_publishers.size();
}
