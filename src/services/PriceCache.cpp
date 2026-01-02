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
    
    {
        std::unique_lock lock(m_mutex);
        m_cache[key] = {
            tick,
            std::chrono::steady_clock::now()
        };
    }
    
    // Invoke callback OUTSIDE lock
    if (m_callback) {
        m_callback(exchangeSegment, token, tick);
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
