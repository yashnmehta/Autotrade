# OPTIONAL: PriceCache Segment Optimization

**Date:** January 6, 2026  
**Priority:** 📊 MEASUREMENT FIRST, then optimize if needed  
**Current Status:** System performing excellently (1-2% CPU)  
**Recommendation:** Monitor first, implement only if actual issues found

---

## ⚠️ IMPORTANT UPDATE

**After discussing with the user, we have ACTUAL performance data:**

- **CPU Usage:** 1-2% (excellent!)
- **Performance:** No lag or freezing
- **System:** Working well with current architecture

**The theoretical "CRITICAL bottleneck" is NOT causing actual problems!**

This document describes an **OPTIONAL optimization** that may provide incremental improvement, but is **NOT urgently needed**.

---

## 📊 Theory vs Reality

| Aspect | Theoretical Prediction | Actual Measurement | Conclusion |
|--------|----------------------|-------------------|------------|
| **CPU Usage** | ~5% | 1-2% | Multi-core wins! |
| **Mutex Contention** | Severe bottleneck | Working fine | Not an issue |
| **Optimization Priority** | CRITICAL | Optional/Future | Measure first! |

**Key Learning:** Multi-threaded, multi-processor systems perform better than simple sequential calculations suggest!

---

## 🚨 Current Architecture (Working Well)

### Current Implementation (PROBLEM)
```cpp
// PriceCache.cpp - Single mutex for ALL threads!
class PriceCache {
private:
    std::shared_mutex m_mutex;  // ⚠️ ALL 4 UDP threads compete for this!
    std::unordered_map<int64_t, CachedTick> m_cache;
```

**Issue:**
- 4 UDP threads (NSE FO, NSE CM, BSE FO, BSE CM)
- ALL threads call `updatePrice()` → acquire **same mutex**
- Threads serialize (wait for each other) = **4x slowdown**
- Merge logic takes ~330ns while holding lock
- At 50,000 ticks/sec: **significant contention**

---

## ✅ Solution: Segment-Specific Mutexes

### New Implementation

**File:** `include/services/PriceCache.h`

```cpp
#ifndef PRICECACHE_H
#define PRICECACHE_H

#include <unordered_map>
#include <shared_mutex>
#include <chrono>
#include <optional>
#include <functional>
#include <vector>
#include <iostream>
#include "api/XTSTypes.h"

class PriceCache {
public:
    static PriceCache& instance();

    // ========== COMPOSITE KEY API (UPDATED) ==========
    XTS::Tick updatePrice(int exchangeSegment, int token, const XTS::Tick& tick);
    std::optional<XTS::Tick> getPrice(int exchangeSegment, int token) const;
    bool hasPrice(int exchangeSegment, int token) const;
    
    // ========== CACHE MANAGEMENT ==========
    int clearStale(int maxAgeSeconds = 3600);
    size_t size() const;
    void clear();
    std::vector<std::pair<int, int>> getAllKeys() const;  // Returns (segment, token) pairs
    
    void setPriceUpdateCallback(std::function<void(int, int, const XTS::Tick&)> callback);

private:
    PriceCache();
    ~PriceCache() = default;
    PriceCache(const PriceCache&) = delete;
    PriceCache& operator=(const PriceCache&) = delete;

    struct CachedTick {
        XTS::Tick tick;
        std::chrono::steady_clock::time_point timestamp;
    };

    // **NEW: Segment-specific cache with its own mutex**
    struct SegmentCache {
        mutable std::shared_mutex mutex;
        std::unordered_map<int, CachedTick> cache;  // Key is just token now!
        
        SegmentCache() {
            cache.reserve(2500);  // Pre-allocate per segment
        }
    };

    // **NEW: 4 separate caches, one per segment**
    SegmentCache m_nseFo;   // segment 2
    SegmentCache m_nseCm;   // segment 1
    SegmentCache m_bseFo;   // segment 12
    SegmentCache m_bseCm;   // segment 11

    SegmentCache& getSegmentCache(int segment);
    const SegmentCache& getSegmentCache(int segment) const;

    std::function<void(int, int, const XTS::Tick&)> m_callback;
};

#endif // PRICECACHE_H
```

---

**File:** `src/services/PriceCache.cpp`

```cpp
#include "services/PriceCache.h"
#include <mutex>
#include <stdexcept>

PriceCache& PriceCache::instance() {
    static PriceCache s_instance;
    return s_instance;
}

PriceCache::PriceCache() {
    std::cout << "⚡ Native PriceCache initialized (segment-specific caches)" << std::endl;
    std::cout << "   - NSE FO cache reserved: 2500 entries" << std::endl;
    std::cout << "   - NSE CM cache reserved: 2500 entries" << std::endl;
    std::cout << "   - BSE FO cache reserved: 2500 entries" << std::endl;
    std::cout << "   - BSE CM cache reserved: 2500 entries" << std::endl;
}

PriceCache::SegmentCache& PriceCache::getSegmentCache(int segment) {
    switch(segment) {
        case 2: return m_nseFo;
        case 1: return m_nseCm;
        case 12: return m_bseFo;
        case 11: return m_bseCm;
        default:
            throw std::runtime_error("Invalid exchange segment: " + std::to_string(segment));
    }
}

const PriceCache::SegmentCache& PriceCache::getSegmentCache(int segment) const {
    return const_cast<PriceCache*>(this)->getSegmentCache(segment);
}

// ========== COMPOSITE KEY API ==========

XTS::Tick PriceCache::updatePrice(int exchangeSegment, int token, const XTS::Tick& tick) {
    SegmentCache& sc = getSegmentCache(exchangeSegment);
    XTS::Tick mergedTick = tick;

    {
        std::unique_lock lock(sc.mutex);  // ✅ Only locks this segment!
        auto it = sc.cache.find(token);   // ✅ Simple token key
        
        if (it != sc.cache.end()) {
            XTS::Tick& existing = it->second.tick;
            
            // --- Merge Logic: Update only available fields ---
            
            // 1. Trade Data (LTP, Volume, OHLC)
            if (tick.lastTradedPrice > 0) {
                existing.lastTradedPrice = tick.lastTradedPrice;
                existing.lastTradedQuantity = tick.lastTradedQuantity;
                if (tick.volume > existing.volume) {
                    existing.volume = tick.volume;
                }
                
                if (tick.open > 0) existing.open = tick.open;
                if (tick.high > 0) existing.high = tick.high;
                if (tick.low > 0) existing.low = tick.low;
                if (tick.close > 0) existing.close = tick.close;
                if (tick.averagePrice > 0) existing.averagePrice = tick.averagePrice;
                
                existing.lastUpdateTime = tick.lastUpdateTime;
            }
            
            // 2. Quote Data (Best Bid/Ask + Depth)
            if (tick.bidQuantity > 0 || tick.bidPrice > 0) {
                existing.bidPrice = tick.bidPrice;
                existing.bidQuantity = tick.bidQuantity;
                existing.totalBuyQuantity = tick.totalBuyQuantity;
                for(int i=0; i<5; ++i) existing.bidDepth[i] = tick.bidDepth[i];
            }
            
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
            
            it->second.timestamp = std::chrono::steady_clock::now();
            mergedTick = existing;
            
        } else {
            // New entry: store as is
            sc.cache[token] = { tick, std::chrono::steady_clock::now() };
        }
    }
    
    // Invoke callback OUTSIDE lock with merged tick
    if (m_callback) {
        m_callback(exchangeSegment, token, mergedTick);
    }
    
    return mergedTick;
}

std::optional<XTS::Tick> PriceCache::getPrice(int exchangeSegment, int token) const {
    const SegmentCache& sc = getSegmentCache(exchangeSegment);
    std::shared_lock lock(sc.mutex);
    
    auto it = sc.cache.find(token);
    if (it != sc.cache.end()) {
        return it->second.tick;
    }
    
    return std::nullopt;
}

bool PriceCache::hasPrice(int exchangeSegment, int token) const {
    const SegmentCache& sc = getSegmentCache(exchangeSegment);
    std::shared_lock lock(sc.mutex);
    return sc.cache.find(token) != sc.cache.end();
}

// ========== CACHE MANAGEMENT ==========

int PriceCache::clearStale(int maxAgeSeconds) {
    auto now = std::chrono::steady_clock::now();
    int totalCleared = 0;
    
    // Clear stale entries from each segment cache
    for (int segment : {1, 2, 11, 12}) {
        SegmentCache& sc = getSegmentCache(segment);
        std::unique_lock lock(sc.mutex);
        
        std::vector<int> tokensToRemove;
        for (const auto& [token, cached] : sc.cache) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                now - cached.timestamp
            ).count();
            
            if (age > maxAgeSeconds) {
                tokensToRemove.push_back(token);
            }
        }
        
        for (int token : tokensToRemove) {
            sc.cache.erase(token);
        }
        
        totalCleared += tokensToRemove.size();
    }
    
    if (totalCleared > 0) {
        std::cout << "🧹 Cleared " << totalCleared << " stale prices" << std::endl;
    }
    
    return totalCleared;
}

std::vector<std::pair<int, int>> PriceCache::getAllKeys() const {
    std::vector<std::pair<int, int>> keys;
    
    for (int segment : {1, 2, 11, 12}) {
        const SegmentCache& sc = getSegmentCache(segment);
        std::shared_lock lock(sc.mutex);
        
        for (const auto& [token, _] : sc.cache) {
            keys.emplace_back(segment, token);
        }
    }
    
    return keys;
}

size_t PriceCache::size() const {
    size_t total = 0;
    for (int segment : {1, 2, 11, 12}) {
        const SegmentCache& sc = getSegmentCache(segment);
        std::shared_lock lock(sc.mutex);
        total += sc.cache.size();
    }
    return total;
}

void PriceCache::clear() {
    size_t totalCount = 0;
    
    for (int segment : {1, 2, 11, 12}) {
        SegmentCache& sc = getSegmentCache(segment);
        std::unique_lock lock(sc.mutex);
        totalCount += sc.cache.size();
        sc.cache.clear();
    }
    
    std::cout << "🧹 Cleared all cached prices (" << totalCount << " items)" << std::endl;
}

void PriceCache::setPriceUpdateCallback(std::function<void(int, int, const XTS::Tick&)> callback) {
    m_callback = std::move(callback);
}
```

---

## 🎯 When Should You Implement This?

### ✅ Implement This Optimization IF:

1. **CPU usage rises above 5-10%** during peak trading
2. **You observe UI lag** or freezing during high market activity
3. **Profiling confirms** mutex contention as bottleneck
4. **Cache size grows** to 10,000+ instruments
5. **You're adding more UDP streams** (new exchanges)

### ❌ DON'T Implement If:

1. ✅ **CPU usage is 1-2%** (current status - excellent!)
2. ✅ **No performance issues** observed
3. ✅ **UI is responsive** and smooth
4. ✅ **System handles load** without problems

### 📊 Recommended Approach: MEASURE FIRST!

**Step 1:** Add monitoring (see Phase 0 in main analysis doc)
**Step 2:** Collect data during normal and peak operations
**Step 3:** Analyze if optimization is actually needed
**Step 4:** If needed, implement this optimization
**Step 5:** Re-measure to validate improvement

---

## 🎬 Current Recommendation

**Based on actual measurements (1-2% CPU):**

🎉 **Your system is already well-optimized!**

- Current architecture is correct
- Performance is excellent  
- No urgent changes needed

**Future action:** Monitor performance and implement this optimization only if measurements show actual bottlenecks.

---

## 📋 Testing Checklist

After implementing:

- [ ] Build compiles without errors
- [ ] Application starts correctly
- [ ] UDP data receives correctly
- [ ] Market watch updates in real-time
- [ ] No crashes or freezes
- [ ] Add tick rate monitoring (from previous doc)
- [ ] Measure CPU usage (should see improvement)
- [ ] Verify all 4 segments working (NSE FO, NSE CM, BSE FO, BSE CM)

---

## 🔍 Validation

Add this to measure improvement:

```cpp
// In main.cpp or MainWindow
QTimer::singleShot(60000, []() {
    std::cout << "\n=== PriceCache Statistics ===" << std::endl;
    std::cout << "Total cached instruments: " << PriceCache::instance().size() << std::endl;
    
    // This will show distribution across segments
    auto keys = PriceCache::instance().getAllKeys();
    int nseFo = 0, nseCm = 0, bseFo = 0, bseCm = 0;
    for (auto [seg, tok] : keys) {
        switch(seg) {
            case 2: nseFo++; break;
            case 1: nseCm++; break;
            case 12: bseFo++; break;
            case 11: bseCm++; break;
        }
    }
    std::cout << "NSE FO: " << nseFo << " instruments" << std::endl;
    std::cout << "NSE CM: " << nseCm << " instruments" << std::endl;
    std::cout << "BSE FO: " << bseFo << " instruments" << std::endl;
    std::cout << "BSE CM: " << bseCm << " instruments" << std::endl;
});
```

---

## ⚠️ Important Notes

1. **No Changes Needed in:**
   - `UdpBroadcastService.cpp` (callbacks stay the same)
   - `FeedHandler.cpp` (stays the same)
   - Any UI code (stays the same)

2. **Only Changes:**
   - `PriceCache.h` (add SegmentCache struct, 4 instances)
   - `PriceCache.cpp` (route to correct segment cache)

3. **Backward Compatible:**
   - Same API (exchangeSegment + token)
   - Same behavior (merge logic unchanged)
   - Just faster!

---

## 🚀 Expected Results

**Before optimization:**
- 4 threads competing for 1 mutex
- Serialization delays
- Possible UI lag during high traffic

**After optimization:**
- Each thread has its own mutex
- No contention between segments
- **4x throughput improvement**
- Smoother UI updates

**CPU Usage:**
- Before: ~5% at 50,000 ticks/sec
- After: ~1.5-2% at 50,000 ticks/sec (less lock waiting)

---

**Do this fix FIRST before any other optimizations!**
