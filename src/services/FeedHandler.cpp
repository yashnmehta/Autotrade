#include "services/FeedHandler.h"

#include "utils/LatencyTracker.h"
#include "utils/PreferencesManager.h"
#include "services/UdpBroadcastService.h"

#include <QDebug>

FeedHandler& FeedHandler::instance() {
    static FeedHandler s_instance;
    return s_instance;
}

FeedHandler::FeedHandler() {
    qDebug() << "[FeedHandler] Initialized - Composite Key (Segment+Token) based";
}
void FeedHandler::registerTokenWithUdpService(uint32_t token, int segment) {
    UdpBroadcastService::instance().subscribeToken(token, segment);
}

FeedHandler::~FeedHandler() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_publishers) {
        delete pair.second;
    }
    m_publishers.clear();
}

TokenPublisher* FeedHandler::getOrCreatePublisher(int64_t compositeKey) {
    auto it = m_publishers.find(compositeKey);
    if (it != m_publishers.end()) {
        return it->second;
    }

    TokenPublisher* pub = new TokenPublisher(compositeKey, this);
    m_publishers[compositeKey] = pub;
    return pub;
}

void FeedHandler::unsubscribe(int exchangeSegment, int token, QObject* receiver) {
    if (!receiver) return;
    
    int64_t key = makeKey(exchangeSegment, token);
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_publishers.find(key);
    if (it != m_publishers.end()) {
        disconnect(it->second, &TokenPublisher::udpTickUpdated, receiver, nullptr);
    }
}

void FeedHandler::unsubscribe(int token, QObject* receiver) {
    if (!receiver) return;
    
    // Unsubscribe from all common segments
    static const int segments[] = {1, 2, 11, 12};
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (int seg : segments) {
        int64_t key = makeKey(seg, token);
        auto it = m_publishers.find(key);
        if (it != m_publishers.end()) {
            disconnect(it->second, &TokenPublisher::udpTickUpdated, receiver, nullptr);
        }
    }
}

void FeedHandler::unsubscribeAll(QObject* receiver) {
    if (!receiver) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_publishers) {
        disconnect(pair.second, &TokenPublisher::udpTickUpdated, receiver, nullptr);
    }
    qDebug() << "[FeedHandler] Disconnected all subscriptions for receiver" << receiver;
}


size_t FeedHandler::totalSubscriptions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_publishers.size();
}

void FeedHandler::onUdpTickReceived(const UDP::MarketTick& tick) {
    int exchangeSegment = static_cast<int>(tick.exchangeSegment);
    int token = tick.token;
    int64_t key = makeKey(exchangeSegment, token);
    
    // 1. Mark FeedHandler processing timestamp
    UDP::MarketTick trackedTick = tick;
    trackedTick.timestampFeedHandler = LatencyTracker::now();
    // If using zero-copy, tick is already complete (no back-propagation needed)
    
    // Debug logging for BSE tokens
    if (exchangeSegment == 12 || exchangeSegment == 11) {
        static int bseTickCount = 0;
        if (bseTickCount++ < 20) {
            // qDebug() << "[FeedHandler] BSE UDP Tick - Segment:" << exchangeSegment 
            //          << "Token:" << token << "Key:" << key << "LTP:" << tick.ltp;
        }
    }
    
    // Mark FeedHandler processing timestamp
    trackedTick.timestampFeedHandler = LatencyTracker::now();
    
    TokenPublisher* pub = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_publishers.find(key);
        if (it != m_publishers.end()) {
            pub = it->second;
        } else {
            // Log missing subscription for BSE tokens
            if (exchangeSegment == 12 || exchangeSegment == 11) {
                static int missingSubCount = 0;
                if (missingSubCount++ < 10) {
                    // qDebug() << "[FeedHandler] ⚠ No UDP subscriber for BSE - Segment:" << exchangeSegment
                    //          << "Token:" << token << "Key:" << key;
                }
            }
        }
    }

    if (pub) {
        pub->publish(trackedTick);
    }
}
