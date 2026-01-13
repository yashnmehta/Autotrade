# Price Cache Architecture - Zero-Copy, Lock-Free Design

## 📋 Overview

This document describes the **new Price Cache implementation** - a high-performance, zero-copy architecture for real-time market data distribution in the Trading Terminal.

**Key Principle**: PriceCache is a **REST-like, request-response system**, NOT a pub-sub system. It serves data on-demand (single-shot) and provides direct memory pointers for future reads.

---

## 🏗️ Architecture Diagram

```
┌─────────────────┐
│  Initialization │
│   - Load User   │
│   Preferences   │
│   - Load Master │
│     Files       │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────────────────────────────┐
│           Create 6 Memory Arrays                     │
│  (NSE CM, NSE FO, BSE CM, BSE FO, NSE CD, MCX)      │
│                                                      │
│  Array_Size = Tokens × Fields × 4 bytes             │
│  Fields = ~60 (LTP, Volume, OI, Bid, Ask, etc.)     │
└─────────────────────────────────────────────────────┘
         │
         ▼
┌──────────────────────────────────────────────────────┐
│                   Runtime Flow                        │
└──────────────────────────────────────────────────────┘

┌──────────────┐
│ UDP Receiver │ ──(direct write)──> [Memory Arrays]
│   (Thread)   │                            │
└──────────────┘                            │
                                            │
┌──────────────┐                            │
│   Object     │ ──(request)──────> ┌───────┴─────────┐
│ (Subscriber) │                    │   PriceCache    │
└──────┬───────┘                    │  (REST-like)    │
       │                            └───────┬─────────┘
       │                                    │
       │ ◄──(response: data + pointer)─────┘
       │
       │ (stores pointer)
       │
       ▼
┌──────────────────────────────────────────┐
│  Object reads directly from pointer      │
│  (triggered by poll/timer/notification)  │
└──────────────────────────────────────────┘
```

---

## 🔑 Core Components

### 1. **Memory Arrays (6 Total)**

One array per market segment:
- NSE Cash (CM)
- NSE Futures & Options (FO)
- BSE Cash (CM)
- BSE Futures & Options (FO)
- NSE Currency Derivatives (CD)
- MCX Commodities

**Array Structure:**
```
Array_Size = No. of Tokens × Fields × 4 bytes
           = N tokens × 60 fields × 4 bytes
           = N × 240 bytes per token
```

**Fields (~60):**
- LTP (Last Traded Price)
- Volume
- Open Interest
- Bid Price (5 levels)
- Ask Price (5 levels)
- Bid Quantity (5 levels)
- Ask Quantity (5 levels)
- Open, High, Low, Close
- Total Buy/Sell Quantity
- Timestamp
- ... (other market data fields)

### 2. **UDP Receiver**

**Responsibility:**
- Receives UDP broadcast packets from exchange
- Parses market data
- **Directly updates memory arrays** (zero-copy write)
- Does NOT go through PriceCache for updates

**Why Direct Updates?**
- Performance: Avoids function call overhead
- Simplicity: No intermediate layer
- Speed: Critical for low-latency market data

### 3. **PriceCache (Request-Response Server)**

**Role:** REST-like service - serves data on-demand

**Operations:**

**A. Subscription (One-Time Request):**
```cpp
// Object requests token data
SubscriptionResult result = priceCache.subscribe(token, segment);

// PriceCache returns:
// 1. Latest data snapshot
// 2. Memory pointer for direct access
// 3. Token index in array
```

**Response Contains:**
- `void* dataPointer` - Direct pointer to token's data in array
- `uint32_t tokenIndex` - Index position in array
- `MarketData snapshot` - Current data snapshot
- `void* baseAddress` - Base address of the array (optional)

**B. On-Demand Query:**
```cpp
// Object can also query without storing pointer
MarketData data = priceCache.getLatestData(token, segment);
```

**PriceCache Does NOT:**
- ❌ Emit continuous signals/updates
- ❌ Push data to subscribers
- ❌ Maintain subscriber lists
- ❌ Handle notifications

**PriceCache DOES:**
- ✅ Calculate memory locations
- ✅ Provide initial data snapshots
- ✅ Return direct memory pointers
- ✅ Handle token-to-index mapping

---

## 🔄 Data Flow

### Initial Subscription Flow

```
1. Object: "I want token 12345 (NSE)"
            ↓
2. Object → Feed Handler → MainUI → PriceCache
            ↓
3. PriceCache:
   - Looks up token in master index
   - Calculates: pointer = baseAddr + (tokenIndex × fieldSize)
   - Reads current data from array
            ↓
4. PriceCache → Returns to Object:
   {
     dataPointer: 0x7FFE12345678,
     tokenIndex: 456,
     snapshot: { ltp: 1234.50, volume: 50000, ... }
   }
            ↓
5. Object stores dataPointer for future use
```

### Ongoing Updates Flow

```
┌──────────────────────────────────────────────────────┐
│  UDP Receiver writes to array (continuous)           │
└──────────────────────────────────────────────────────┘
                    ↓
        [Memory Array Updated]
                    ↓
┌──────────────────────────────────────────────────────┐
│  Object needs to know "when" to read?                │
│  (This is the KEY QUESTION)                          │
└──────────────────────────────────────────────────────┘
```

---

## ⚡ Update Notification Mechanisms

Since PriceCache is one-shot (REST-like), **how does the subscriber know when data is updated?**

### **Option 1: Polling/Timer-Based (UI Components)**

**Use Case:** Market watch windows, charts, non-critical displays

**Implementation:**
```cpp
class MarketWatchRow {
    void* dataPointer; // Stored from PriceCache
    QTimer* refreshTimer;
    
    void onTimerTick() {
        // Direct memory read (zero-copy)
        MarketData* data = static_cast<MarketData*>(dataPointer);
        updateUI(data->ltp, data->volume);
    }
};

// Timer: 100ms refresh rate
refreshTimer->start(100);
```

**Pros:**
- ✅ Simple implementation
- ✅ Predictable UI refresh rate
- ✅ Low CPU for display-only objects

**Cons:**
- ❌ Not real-time (100ms latency)
- ❌ Reads even when no updates

---

### **Option 2: Lock-Free Queue Notification (Critical Components)**

**Use Case:** Algo engines, order management, P&L calculators

**Architecture:**
```
┌──────────────┐
│ UDP Receiver │
└──────┬───────┘
       │
       ├─(write)──> [Memory Array]
       │
       └─(notify)─> [Lock-Free Queue] ──> Object Thread
                         ↓
                    Notification:
                    { token: 12345,
                      segment: NSE_CM,
                      updateType: LTP_CHANGE }
```

**Implementation:**
```cpp
class AlgoEngine {
    void* dataPointer;
    LockFreeQueue<TokenUpdate>* notificationQueue;
    
    void processUpdates() {
        TokenUpdate update;
        while (notificationQueue->pop(update)) {
            // Direct memory read (zero-copy)
            MarketData* data = static_cast<MarketData*>(dataPointer);
            
            // Process immediately
            if (data->ltp > myTriggerPrice) {
                placeOrder();
            }
        }
    }
};
```

**Notification Manager:**
```cpp
class NotificationManager {
    std::unordered_map<Token, std::vector<LockFreeQueue*>> subscribers;
    
    void notifyUpdate(Token token, UpdateType type) {
        for (auto* queue : subscribers[token]) {
            queue->push({token, type});
        }
    }
};
```

**UDP Receiver Integration:**
```cpp
void UDPReceiver::onPacketReceived(Packet* pkt) {
    Token token = pkt->token;
    
    // 1. Update array (direct write)
    MarketData* data = getPointer(token);
    data->ltp = pkt->ltp;
    data->volume = pkt->volume;
    
    // 2. Notify subscribers (optional)
    notificationManager->notifyUpdate(token, LTP_CHANGE);
}
```

**Pros:**
- ✅ Real-time notifications (microsecond latency)
- ✅ Event-driven (only processes when needed)
- ✅ Perfect for algo trading

**Cons:**
- ❌ More complex implementation
- ❌ Requires thread management

---

### **Option 3: Hybrid Approach (Recommended)**

Combine both mechanisms based on use case:

| Component Type | Mechanism | Reason |
|----------------|-----------|--------|
| Market Watch UI | Polling (100ms timer) | Display-only, 100ms is acceptable |
| Order Book Display | Polling (50ms timer) | Near real-time display |
| Chart Updates | Polling (1000ms timer) | Low priority |
| Algo Engine | Lock-Free Queue | Needs microsecond latency |
| Order Manager | Lock-Free Queue | Critical for order execution |
| P&L Calculator | Lock-Free Queue | Real-time risk management |
| Position Tracker | Lock-Free Queue | Immediate position updates |

**Configuration:**
```cpp
enum NotificationType {
    NONE,           // Object polls on its own
    TIMER,          // Object uses timer-based polling
    LOCK_FREE_QUEUE // Object gets real-time notifications
};

SubscriptionResult priceCache.subscribe(
    token, 
    segment,
    NotificationType::LOCK_FREE_QUEUE  // Object specifies
);
```

---

## 💾 Memory Layout Example

**NSE Cash Market Array:**

```
Base Address: 0x7FFE00000000

Token Index | Token  | Offset    | Address       | Fields
------------|--------|-----------|---------------|------------------
0           | 3045   | 0         | 0x7FFE000000  | [LTP][Vol][OI]...
1           | 11536  | 240       | 0x7FFE0000F0  | [LTP][Vol][OI]...
2           | 25     | 480       | 0x7FFE0001E0  | [LTP][Vol][OI]...
...         | ...    | ...       | ...           | ...
N           | 47213  | N×240     | 0x7FFE00XXXX  | [LTP][Vol][OI]...
```

**Pointer Calculation:**
```cpp
void* calculatePointer(uint32_t tokenIndex) {
    return baseAddress + (tokenIndex * FIELD_SIZE * NUM_FIELDS);
    //     0x7FFE000000 + (2 * 4 * 60) = 0x7FFE0001E0
}
```

**Direct Access:**
```cpp
// Object has pointer: 0x7FFE0001E0
MarketData* data = static_cast<MarketData*>(dataPointer);

// Zero-copy read
float ltp = data->ltp;          // Read from 0x7FFE0001E0
uint64_t volume = data->volume; // Read from 0x7FFE0001E4
```

---

## 🚀 Performance Characteristics

### **Zero-Copy Design**

**Traditional Approach:**
```
UDP → Parse → Copy to Cache → Copy to Object → Process
      (3 copies, function calls, locks)
```

**New Approach:**
```
UDP → Parse → Write to Array
              ↓
              Object reads directly (0 copies!)
```

### **Latency Benchmarks (Expected)**

| Operation | Latency | Notes |
|-----------|---------|-------|
| UDP packet processing | ~5-10 µs | Parser + direct write |
| PriceCache subscription | ~1-2 µs | Pointer calculation |
| Direct memory read | ~50-100 ns | L1 cache hit |
| Timer-based update | ~100 ms | QTimer overhead |
| Lock-free queue notification | ~1-5 µs | Single producer, single consumer |

### **Scalability**

- **Tokens supported:** 100,000+ per segment
- **Memory footprint:** ~24 MB per 100K tokens (240 bytes × 100K)
- **Total memory:** ~144 MB for all 6 segments
- **Concurrent subscribers:** Unlimited (read-only access)

---

## 🛠️ Implementation Guidelines

### **1. PriceCache Interface**

```cpp
class PriceCache {
public:
    struct SubscriptionResult {
        void* dataPointer;
        uint32_t tokenIndex;
        MarketData snapshot;
        bool success;
    };
    
    // One-time subscription (REST-like)
    SubscriptionResult subscribe(
        uint32_t token,
        MarketSegment segment
    );
    
    // On-demand query
    MarketData getLatestData(
        uint32_t token,
        MarketSegment segment
    );
    
    // Unsubscribe (optional, for cleanup)
    void unsubscribe(uint32_t token, MarketSegment segment);
    
private:
    void* arrays[6]; // Base addresses for 6 segments
    std::unordered_map<uint32_t, uint32_t> tokenToIndex;
    
    void* calculatePointer(uint32_t tokenIndex, MarketSegment segment);
};
```

### **2. UDP Receiver Integration**

```cpp
class UDPReceiver {
private:
    void* arrayBaseAddress;
    NotificationManager* notifier; // Optional
    
    void onPacketReceived(UDPPacket* packet) {
        // 1. Parse packet
        Token token = parseToken(packet);
        uint32_t index = getTokenIndex(token);
        
        // 2. Calculate pointer
        MarketData* data = static_cast<MarketData*>(
            arrayBaseAddress + (index * sizeof(MarketData))
        );
        
        // 3. Direct write (zero-copy)
        data->ltp = packet->ltp;
        data->volume = packet->volume;
        data->timestamp = getCurrentTime();
        
        // 4. Optional: Notify subscribers
        if (notifier) {
            notifier->notifyUpdate(token, UpdateType::LTP_CHANGE);
        }
    }
};
```

### **3. Object Subscription Pattern**

```cpp
class MarketWatchRow : public QObject {
    Q_OBJECT
    
public:
    void subscribeToken(uint32_t token) {
        // 1. Request from PriceCache (one-time)
        auto result = priceCache->subscribe(token, MarketSegment::NSE_CM);
        
        if (result.success) {
            // 2. Store pointer
            dataPointer = result.dataPointer;
            
            // 3. Initial UI update
            updateUI(result.snapshot);
            
            // 4. Setup update mechanism
            setupUpdateMechanism();
        }
    }
    
private:
    void* dataPointer;
    QTimer* refreshTimer;
    
    void setupUpdateMechanism() {
        // Option A: Timer-based (UI)
        refreshTimer = new QTimer(this);
        connect(refreshTimer, &QTimer::timeout, this, &MarketWatchRow::onRefresh);
        refreshTimer->start(100); // 100ms refresh
        
        // Option B: Lock-free queue (Algo)
        // notificationQueue = notificationManager->subscribe(token);
        // std::thread(&MarketWatchRow::processQueue, this).detach();
    }
    
    void onRefresh() {
        // Direct memory read (zero-copy)
        MarketData* data = static_cast<MarketData*>(dataPointer);
        updateUI(*data);
    }
};
```

---

## 📊 Comparison with Traditional Pub-Sub

| Feature | Traditional Pub-Sub | New Architecture |
|---------|---------------------|------------------|
| Data copies | 2-3 copies | 0 copies |
| Latency | ~100-500 µs | ~1-10 µs |
| Memory overhead | High (per subscriber) | Low (shared arrays) |
| Scalability | Limited (per subscriber copy) | Excellent (read-only) |
| Notification | Built-in (signals/events) | Optional (timer/queue) |
| Complexity | Medium | Low (simple pointer) |
| Real-time | No (signal overhead) | Yes (direct read) |

---

## 🎯 Key Takeaways

1. **PriceCache = REST API** (not pub-sub)
   - One-time request → Returns data + pointer
   - No continuous updates from PriceCache

2. **UDP Receiver = Direct Writer**
   - Writes directly to memory arrays
   - Bypasses PriceCache for performance

3. **Object = Direct Reader**
   - Stores pointer from PriceCache
   - Reads directly from memory (zero-copy)

4. **Notification = Separate Concern**
   - Timer-based for UI (simple, acceptable latency)
   - Lock-free queue for critical components (real-time)

5. **Zero-Copy = Performance**
   - UDP writes once, objects read directly
   - No intermediate copies or function calls

---

## 🔮 Future Enhancements

1. **Memory-Mapped Files** (for persistence)
   - Arrays backed by mmap for crash recovery
   
2. **NUMA-Aware Allocation**
   - Pin arrays to specific CPU cores for cache locality

3. **Lock-Free Circular Buffers**
   - Historical tick data alongside latest values

4. **Batch Notifications**
   - Group multiple token updates for efficiency

5. **Conditional Notifications**
   - Only notify on significant changes (e.g., LTP > 1%)

---

## 📝 Summary

This architecture achieves:
- ✅ **Zero-copy data access** (direct memory reads)
- ✅ **Microsecond latency** (UDP → Array → Object)
- ✅ **Scalability** (100K+ tokens, unlimited subscribers)
- ✅ **Simplicity** (PriceCache is just pointer calculator)
- ✅ **Flexibility** (timer or queue-based notifications)

**The Magic:** PriceCache doesn't "push" data; it gives you a fishing rod (pointer) instead of a fish (data copy)! 🎣

---

**Last Updated:** January 13, 2026  
**Author:** Trading Terminal Team  
**Version:** 2.0
