#include "services/PriceCache.h"
#include <mutex>

PriceCache& PriceCache::instance() {
    static PriceCache s_instance;  // Thread-safe since C++11
    return s_instance;
}

PriceCache::PriceCache() {
    // Pre-allocate space for typical number of subscriptions
    m_cache.reserve(500);  // Increased for multi-exchange
    std::cout << "⚡ Native PriceCache initialized (composite key: segment|token)" << std::endl;
}

// ========== COMPOSITE KEY API (RECOMMENDED) ==========

void PriceCache::updatePrice(int exchangeSegment, int token, const XTS::Tick& tick) {
    int64_t key = makeKey(exchangeSegment, token);
    XTS::Tick mergedTick = tick;

    {
        std::unique_lock lock(m_mutex);
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            XTS::Tick& existing = it->second.tick;
            
            // --- Merge Logic: Update only available fields ---
            
            // 1. Trade Data (LTP, Volume, OHLC)
            // If the incoming tick has a valid trade price, it's a trade packet (or full packet)
            if (tick.lastTradedPrice > 0) {
                existing.lastTradedPrice = tick.lastTradedPrice;
                existing.lastTradedQuantity = tick.lastTradedQuantity;
                if (tick.volume > existing.volume) {
                    existing.volume = tick.volume; // Only update if cumulative volume increases
                }
                existing.open = tick.open;
                existing.high = tick.high;
                existing.low = tick.low;
                existing.close = tick.close;
                existing.averagePrice = tick.averagePrice;
                existing.lastUpdateTime = tick.lastUpdateTime;
            }
            
            // 2. Quote Data (Best Bid/Ask + Depth)
            // If incoming tick has valid Bid info
            if (tick.bidQuantity > 0 || tick.bidPrice > 0) {
                existing.bidPrice = tick.bidPrice;
                existing.bidQuantity = tick.bidQuantity;
                existing.totalBuyQuantity = tick.totalBuyQuantity;
                for(int i=0; i<5; ++i) existing.bidDepth[i] = tick.bidDepth[i];
            }
            
            // If incoming tick has valid Ask info
            if (tick.askQuantity > 0 || tick.askPrice > 0) {
                existing.askPrice = tick.askPrice;
                existing.askQuantity = tick.askQuantity;
                existing.totalSellQuantity = tick.totalSellQuantity;
                for(int i=0; i<5; ++i) existing.askDepth[i] = tick.askDepth[i];
            }
            
            // 3. Open Interest
            if (tick.openInterest > 0) {
                existing.openInterest = tick.openInterest;
            }
            
            // Update timestamp
            it->second.timestamp = std::chrono::steady_clock::now();
            mergedTick = existing; // Use the full picture for callback
            
        } else {
            // New entry: store as is
            m_cache[key] = { tick, std::chrono::steady_clock::now() };
        }
    }
    
    // Invoke callback OUTSIDE lock
    if (m_callback) {
        m_callback(exchangeSegment, token, mergedTick);
    }
}

std::optional<XTS::Tick> PriceCache::getPrice(int exchangeSegment, int token) const {
    std::shared_lock lock(m_mutex);
    
    int64_t key = makeKey(exchangeSegment, token);
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        return it->second.tick;
    }
    
    return std::nullopt;
}

bool PriceCache::hasPrice(int exchangeSegment, int token) const {
    std::shared_lock lock(m_mutex);
    int64_t key = makeKey(exchangeSegment, token);
    return m_cache.find(key) != m_cache.end();
}

double PriceCache::getCacheAge(int exchangeSegment, int token) const {
    std::shared_lock lock(m_mutex);
    
    int64_t key = makeKey(exchangeSegment, token);
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        auto age = std::chrono::steady_clock::now() - it->second.timestamp;
        return std::chrono::duration<double>(age).count();
    }
    
    return -1.0;
}

// ========== LEGACY TOKEN-ONLY API ==========

void PriceCache::updatePrice(int token, const XTS::Tick& tick) {
    // Use segment from tick if available
    int segment = tick.exchangeSegment > 0 ? tick.exchangeSegment : 1;
    updatePrice(segment, token, tick);
}

std::optional<XTS::Tick> PriceCache::getPrice(int token) const {
    std::shared_lock lock(m_mutex);
    
    // Search common segments for backward compatibility
    static const int segments[] = {1, 2, 11, 12};
    for (int seg : segments) {
        int64_t key = makeKey(seg, token);
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            return it->second.tick;
        }
    }
    
    return std::nullopt;
}

bool PriceCache::hasPrice(int token) const {
    std::shared_lock lock(m_mutex);
    
    static const int segments[] = {1, 2, 11, 12};
    for (int seg : segments) {
        int64_t key = makeKey(seg, token);
        if (m_cache.find(key) != m_cache.end()) {
            return true;
        }
    }
    
    return false;
}

// ========== CACHE MANAGEMENT ==========

int PriceCache::clearStale(int maxAgeSeconds) {
    std::unique_lock lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    std::vector<int64_t> keysToRemove;
    keysToRemove.reserve(10);
    
    for (const auto& [key, cached] : m_cache) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - cached.timestamp
        ).count();
        
        if (age > maxAgeSeconds) {
            keysToRemove.push_back(key);
        }
    }
    
    for (int64_t key : keysToRemove) {
        m_cache.erase(key);
    }
    
    if (!keysToRemove.empty()) {
        std::cout << "🧹 Cleared " << keysToRemove.size() << " stale prices" << std::endl;
    }
    
    return keysToRemove.size();
}

std::vector<int64_t> PriceCache::getAllKeys() const {
    std::shared_lock lock(m_mutex);
    
    std::vector<int64_t> keys;
    keys.reserve(m_cache.size());
    
    for (const auto& [key, _] : m_cache) {
        keys.push_back(key);
    }
    
    return keys;
}

size_t PriceCache::size() const {
    std::shared_lock lock(m_mutex);
    return m_cache.size();
}

void PriceCache::clear() {
    std::unique_lock lock(m_mutex);
    size_t count = m_cache.size();
    m_cache.clear();
    std::cout << "🧹 Cleared all cached prices (" << count << " items)" << std::endl;
}

void PriceCache::setPriceUpdateCallback(std::function<void(int, int, const XTS::Tick&)> callback) {
    m_callback = std::move(callback);
}
