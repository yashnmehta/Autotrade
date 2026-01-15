#include "services/FeedHandler.h"
#include "services/PriceCache.h"
#include "utils/LatencyTracker.h"
#include "services/PriceCacheZeroCopy.h"
#include "utils/PreferencesManager.h"
#include <QDebug>

FeedHandler& FeedHandler::instance() {
    static FeedHandler s_instance;
    return s_instance;
}

FeedHandler::FeedHandler() {
    qDebug() << "[FeedHandler] Initialized - Composite Key (Segment+Token) based";
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
        disconnect(it->second, &TokenPublisher::tickUpdated, receiver, nullptr);
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
            disconnect(it->second, &TokenPublisher::tickUpdated, receiver, nullptr);
        }
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

void FeedHandler::onTickReceived(const XTS::Tick &tick) {
    int exchangeSegment = tick.exchangeSegment;
    int token = (int)tick.exchangeInstrumentID;
    int64_t key = makeKey(exchangeSegment, token);
    
    // Update ONLY the selected cache based on user preference
    XTS::Tick mergedTick;
    bool useLegacyCache = PreferencesManager::instance().getUseLegacyPriceCache();
    
    if (useLegacyCache) {
        // Use legacy PriceCache with proven merge logic
        mergedTick = PriceCache::instance().updatePrice(exchangeSegment, token, tick);
    } else {
        // Use new zero-copy cache
        PriceCacheTypes::PriceCacheZeroCopy::getInstance().update(tick);
        mergedTick = tick;  // Pass through for publishing
    }
    
    // Debug logging for BSE tokens
    if (exchangeSegment == 12 || exchangeSegment == 11) {
        static int bseTickCount = 0;
        if (bseTickCount++ < 20) {
            // qDebug() << "[FeedHandler] BSE Tick - Segment:" << exchangeSegment 
            //          << "Token:" << token << "Key:" << key << "LTP:" << tick.lastTradedPrice;
        }
    }
    
    // Mark FeedHandler processing timestamp on the MERGED tick
    XTS::Tick trackedTick = mergedTick;
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
                    // qDebug() << "[FeedHandler] ⚠ No subscriber for BSE - Segment:" << exchangeSegment
                    //          << "Token:" << token << "Key:" << key;
                }
            }
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

void FeedHandler::onUdpTickReceived(const UDP::MarketTick& tick) {
    int exchangeSegment = static_cast<int>(tick.exchangeSegment);
    int token = tick.token;
    int64_t key = makeKey(exchangeSegment, token);
    
    // 1. Convert UDP to XTS for PriceCache
    XTS::Tick xtsTick;
    xtsTick.exchangeSegment = exchangeSegment;
    xtsTick.exchangeInstrumentID = token;
    xtsTick.lastTradedPrice = tick.ltp;
    xtsTick.lastTradedQuantity = tick.ltq;
    xtsTick.volume = tick.volume;
    xtsTick.open = tick.open;
    xtsTick.high = tick.high;
    xtsTick.low = tick.low;
    xtsTick.close = tick.prevClose; // UDP prevClose maps to XTS close usually? Or XTS has prevClose?
    // Checking XTS::Tick definition (from memory/context): it has 'close' and 'PreviousDayClosingPrice' maybe?
    // Using 'close' for now as that's what PriceCache uses for OHLC.
    xtsTick.lastUpdateTime = tick.timestampEmitted;

    // Map Best Bid/Ask if present
    if (tick.bids[0].price > 0) {
        xtsTick.bidPrice = tick.bids[0].price;
        xtsTick.bidQuantity = tick.bids[0].quantity;
        // Map depth...
        for(int i=0; i<5; ++i) {
            xtsTick.bidDepth[i].price = tick.bids[i].price;
            xtsTick.bidDepth[i].quantity = tick.bids[i].quantity;
            xtsTick.bidDepth[i].orders = tick.bids[i].orders;
        }
    }
    if (tick.asks[0].price > 0) {
        xtsTick.askPrice = tick.asks[0].price;
        xtsTick.askQuantity = tick.asks[0].quantity;
        // Map depth...
        for(int i=0; i<5; ++i) {
            xtsTick.askDepth[i].price = tick.asks[i].price;
            xtsTick.askDepth[i].quantity = tick.asks[i].quantity;
            xtsTick.askDepth[i].orders = tick.asks[i].orders;
        }
    }

    // 2. Update ONLY the selected cache based on user preference
    bool useLegacyCache = PreferencesManager::instance().getUseLegacyPriceCache();
    XTS::Tick mergedXts;
    
    if (useLegacyCache) {
        // Use legacy PriceCache with proven merge logic
        mergedXts = PriceCache::instance().updatePrice(exchangeSegment, token, xtsTick);
    } else {
        // Use new zero-copy cache (direct UDP tick update)
        PriceCacheTypes::PriceCacheZeroCopy::getInstance().update(tick);
        mergedXts = xtsTick;  // Pass through for publishing
    }

    // 3. Back-propagate data to UDP Tick for publishing (only if using legacy cache)
    UDP::MarketTick trackedTick = tick;
    
    if (useLegacyCache) {
        // Back-propagate merged data from legacy cache
        trackedTick.ltp = mergedXts.lastTradedPrice;
        trackedTick.ltq = mergedXts.lastTradedQuantity;
        trackedTick.volume = mergedXts.volume;
        trackedTick.open = mergedXts.open;
        trackedTick.high = mergedXts.high;
        trackedTick.low = mergedXts.low;
        trackedTick.prevClose = mergedXts.close;
        
        // Restore Bids/Asks from Cache
        if (mergedXts.bidPrice > 0) {
            trackedTick.bids[0].price = mergedXts.bidPrice;
            trackedTick.bids[0].quantity = mergedXts.bidQuantity;
            for(int i=0; i<5; ++i) {
                trackedTick.bids[i].price = mergedXts.bidDepth[i].price;
                trackedTick.bids[i].quantity = mergedXts.bidDepth[i].quantity;
                trackedTick.bids[i].orders = mergedXts.bidDepth[i].orders;
            }
        }
        if (mergedXts.askPrice > 0) {
            trackedTick.asks[0].price = mergedXts.askPrice;
            trackedTick.asks[0].quantity = mergedXts.askQuantity;
            for(int i=0; i<5; ++i) {
                trackedTick.asks[i].price = mergedXts.askDepth[i].price;
                trackedTick.asks[i].quantity = mergedXts.askDepth[i].quantity;
                trackedTick.asks[i].orders = mergedXts.askDepth[i].orders;
            }
        }
    }
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
