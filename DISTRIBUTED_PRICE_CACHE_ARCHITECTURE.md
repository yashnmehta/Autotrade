# Distributed Price Cache Architecture & Data Flow

**Date**: January 18, 2026  
**Status**: Implementation Guide

---

## 📋 Table of Contents

1. [Overview](#overview)
2. [Architecture Decisions](#architecture-decisions)
3. [Distributed Store Structure](#distributed-store-structure)
4. [Data Flow](#data-flow)
5. [Thread Safety Model](#thread-safety-model)
6. [API Reference](#api-reference)
7. [Implementation Phases](#implementation-phases)

---

## 1. Overview

### Purpose
Replace centralized `PriceCache` with **distributed price stores** at UDP reader level to:
- Pre-populate contract master metadata (symbol, expiry, strike, lotSize)
- Provide immediate data on subscribe (critical for illiquid contracts) -- this is use case of price cache 
- Minimize data copying and latency
- Enable segment-specific optimizations (array vs hash map)

### Key Components
- **Distributed Stores** (`g_stores`): Thread-local, pre-populated, source of truth
- **UDP Parsers**: Decode packets → Store → Notify
- **UdpBroadcastService**: Bridge UDP callbacks → Qt signals
- **FeedHandler**: Pub/sub distribution + on-demand data access

---

## 2. Architecture Decisions

### Decision Matrix

| Question | Choice | Rationale |
|----------|--------|-----------|
| **Thread Safety** | `std::shared_mutex` | Simple, 1:10000 read:write ratio = 0.01% contention |
| **PriceCache Fate** | Delete, use g_stores | XTS uses getQuote API instead |
| **Parser Updates** | Update all parsers | Clean design, work on broadcast readers individually |
| **Callback Signature** | Pass token ID only | Minimal overhead, lock-free callback emission |
| **Subscription Model** | **Hybrid Notification** | Background Storage (Global) + Selective Notification (Local) |
| **Callback Payload** | **Int32 Token Only** | Zero-copy architecture; UI fetches fresh state from store (Conflation). |
| **Callback Type** | **Separate Callbacks** | Explicit Context (Price vs Depth vs Ticker) for targeted UI updates. |

### Subscription Model Rationale (Background Updates + Selective Notification)

| Layer | Responsibility | Mechanism | Why? |
|-------|----------------|-----------|------|
| **g_store** | **Global State** | Always Update | Background thread updates *every* token silently. Ensures data is never stale. |
| **UDP Parser** | **Notification Filter** | `isTokenEnabled` Check | Only emits callbacks for active tokens. Prevents choking the Qt thread. |
| **FeedHandler** | **Registry & Pull** | Map + Instant Read | Manages UI list and pulls fresh data from `g_store` the moment a user subscribes. |

### Data Structure Choice

| Segment | Store Type | Size | Reason |
|---------|------------|------|--------|
| **NSE FO** | Indexed Array | 90,000 slots (~108 MB) | Dense token range (26000-115999) |
| **NSE CM** | Hash Map | Variable | Sparse tokens, ~2000 active |
| **BSE FO** | Hash Map | Variable | Sparse tokens |
| **BSE CM** | Hash Map | Variable | Sparse tokens |

---

## 3. Distributed Store Structure

### 3.1 Unified State Record (Fused Struct)

Instead of separate structs for different message codes, we use a **Unified State Record**. This struct is an accumulator of fields from multiple UDP streams (Touchline, Depth, OI, etc.).

### 3.2 Message Categorization

Messages are classified into two categories based on their destination and scope.

| Category | Description | Examples | Handling |
|----------|-------------|----------|----------|
| **1. Token Specific** | Updates state of a single specific instrument. | **Price**: 7200, 7201, 17201<br>**Depth**: 7208<br>**OI**: 7202, 17202<br>**Misc**: 7211 (LPP), 7207 (VIX) | **Store Update** (Primary)<br>+<br>**Selective Notification** (Secondary) |
| **2. General / System** | Global market status, Stream Headers, Indices | **System**: 6501, 6511 (Market Status)<br>**Utility**: 7220 (Spread), Indices | **Direct Signal Emission**<br>(No storage in PriceStore) |

> **Note**: NSE FO does **NOT** have a separate Index Store. Indices (if any) are handled as General Messages or managed in the Cash Market (NSE CM) segment.

```cpp
struct UnifiedTokenState {
    // 1. STATIC METADATA (From Contract Master)
    // ... (Symbol, Expiry, LotSize) ...

    // 2. DYNAMIC STATE (Fused from multiple UDP Message Codes)
    
    // from 7200/7201/17201 (Price)
    double ltp, open, high, low, close;
    uint32_t volume, lastTradeQty, lastTradeTime;
    
    // from 7208 (Depth)
    DepthLevel bids[5], asks[5];
    double totalBuyQty, totalSellQty;

    // from 7202/17202 (OI)
    int64_t openInterest;
    
    // from 7211 (LPP - Limit Price Protection)
    double lppHigh, lppLow;
    
    // from 2002/Status
    uint16_t tradingStatus;
};
```

### 3.3 NSE FO Store (Indexed Array)

**File**: `lib/cpp_broacast_nsefo/include/nsefo_price_store.h`

```cpp
// 3.3 NSE FO Store (Indexed Array)
namespace nsefo {

class PriceStore {
public:
    static constexpr int TOKEN_OFFSET = 10000;
    static constexpr int ARRAY_SIZE = 90000;

    // Use Shared Mutex for Render/Write safety
    mutable std::shared_mutex mutex_;
    UnifiedTokenState store_[ARRAY_SIZE];

    // Thread-safe Update (Partial)
    void updateTouchline(const UnifiedTokenState& data) {
		// ... implementation ...
    }

    // Thread-safe read
    const UnifiedTokenState* getUnifiedState(uint32_t token) const {
		// ... implementation ...
    }
};

// Global instance (one per UDP thread)
extern PriceStore g_nseFoPriceStore;
// NOTE: No IndexStore for NSE FO

} // namespace nsefo
```

### 3.4 NSE FO Store (Indexed Array)
// ... existing code ...

### 3.5 Design Decisions: Callback Architecture

#### Why send only `int32_t token`?
1.  **Performance (Zero-Copy)**: Passing a 500-byte `UnifiedTokenState` struct by value (or even reference) incurs unnecessary cache coherence traffic in a multi-threaded system. Passing a 4-byte integer is effectively free.
2.  **Conflation**: The data in the callback payload is inherently a "snapshot of the past". By the time the UI thread processes the signal (e.g., 5-10ms later), the market may have moved. By passing only the ID, the UI is forced to call `getUnifiedState(token)`, ensuring it always reads the **latest live state**, automatically determining the most recent price.

#### Why separate callbacks (Touchline vs Depth vs Ticker)?
1.  **Explicit Context**: 
    *   `onTouchlineCallback` -> Implies fundamental price change (LTP, Open, High, Low).
    *   `onDepthCallback` -> Implies order book change.
    *   `onTickerCallback` -> Implies a trade occurred.
2.  **Targeted Updates**: A "Depth Ladder" widget only needs to listen to `onDepthCallback`, whereas a "Watchlist Row" primarily cares about `onTouchlineCallback`. Separation allows widgets to filter noise efficiently.
3.  **Legacy Compatibility**: Fits seamlessly with the existing Qt Signal/Slot architecture.

### 3.6 NSE CM / BSE Stores (Hash Map)

**Files**: 
- `lib/cpp_broadcast_nsecm/include/nsecm_price_store.h`
- `lib/cpp_broadcast_bsefo/include/bse_price_store.h`

```cpp
namespace nsecm {  // or bse
```cpp
struct TouchlineData {
    // 1. STATIC METADATA (From CSV - Never changes after startup)
    char symbol[32];
    char displayName[64];
    int lotSize;
    double strikePrice;
    char optionType[3];
    long long expiryDate;
    int assetToken;
    char instrumentType[16];
    double tickSize;
    
    // 2. DYNAMIC STATE (Fused from multiple UDP Message Codes)
    int token;
    long long lastPacketTimestamp;
    
    // From Msg 7200/7201 (NSE Touchline)
    double ltp, open, high, low, close;
    long long volume, turnover;
    double weightedAvgPrice;
    int totalBuyQty, totalSellQty;

    // From Msg 7208/7200 (Market Depth)
    DepthLevel bids[5], asks[5];

    // From Msg 7202 (Open Interest)
    long long openInterest;
    long long openInterestChange;

    // From Msg 2002 (Product State)
    int tradingStatus;
};
```

### 3.2 Selective Fusion (Partial Updates)

The `PriceStore` does not overwrite the whole struct on every packet. Instead, it performs **Partial Updates**. Each parser updates only the "Columns" relevant to its message code.

```cpp
class PriceStore {
public:
    // Partial Update: Price Fields (Msg 7200)
    void updatePrice(int token, double ltp, double open, ...) {
        std::unique_lock lock(mutex_);
        auto& row = store_[token];
        row.ltp = ltp;
        row.open = open;
        // bids/asks are preserved!
    }

    // Partial Update: Depth Fields (Msg 7208)
    void updateDepth(int token, const DepthLevel* bids, ...) {
        std::unique_lock lock(mutex_);
        auto& row = store_[token];
        memcpy(row.bids, bids, sizeof(row.bids));
        // ltp/open are preserved!
    }
};
```

### 3.3 NSE FO Store (Indexed Array)

**File**: `lib/cpp_broacast_nsefo/include/nsefo_price_store.h`

```cpp
// 3.3 NSE FO Store (Indexed Array)
namespace nsefo {

class PriceStore {
public:
    static constexpr int TOKEN_OFFSET = 10000;
    static constexpr int ARRAY_SIZE = 90000;

    // Use Shared Mutex for Render/Write safety
    mutable std::shared_mutex mutex_;
    UnifiedTokenState store_[ARRAY_SIZE];

    // Thread-safe Update (Partial)
    void updateTouchline(const UnifiedTokenState& data) {
        std::unique_lock lock(mutex_);
        int index = data.token - TOKEN_OFFSET;
        if (index >= 0 && index < ARRAY_SIZE) {
            // In reality, we copy FIELD-BY-FIELD here to preserve other columns
            store_[index].ltp = data.ltp;
            store_[index].volume = data.volume; 
            // ... etc
        }
    }

    
    // Thread-safe read (read lock ~100ns, multiple readers OK)
    const UnifiedTokenState* getTouchline(int token) const {
        std::shared_lock lock(mutex_);
        int index = token - TOKEN_OFFSET;
        if (index >= 0 && index < ARRAY_SIZE) {
            return &store_[index];
        }
        return nullptr;
    }
};

### 3.4 Initialization Strategy (Static vs Dynamic)

The `UnifiedTokenState` contains both static metadata (Symbol, Expiry) and dynamic data (LTP, OI). 

*   **Static Data**: Initialized **ONCE** at startup by `RepositoryManager::initializeDistributedStores()`. It reads from the Contract Master/CSV and populates the store.
*   **Dynamic Data**: Zero-initialized at startup. Updated continuously by UDP Parsers.
*   **Safety**: The `updateXXX` methods in the store must **NEVER** overwrite the static fields (Symbol, LotSize) to ensure data integrity.

        std::shared_lock lock(mutex_);
        int index = token - TOKEN_OFFSET;
        if (index >= 0 && index < ARRAY_SIZE) {
            return &store_[index];
        }
        return nullptr;
    }
    
    // Initialize contract master fields (called at startup)
    void initializeToken(int token, const char* symbol, 
                        const char* displayName, int lotSize,
                        double strikePrice, const char* optionType,
                        long long expiryDate, int assetToken,
                        const char* instrumentType, double tickSize) {
        std::unique_lock lock(mutex_);
        int index = token - TOKEN_OFFSET;
        if (index >= 0 && index < ARRAY_SIZE) {
            TouchlineData& data = store_[index];
            data.token = token;
            strncpy(data.symbol, symbol, 31);
            strncpy(data.displayName, displayName, 63);
            data.lotSize = lotSize;
            data.strikePrice = strikePrice;
            strncpy(data.optionType, optionType, 2);
            data.expiryDate = expiryDate;
            data.assetToken = assetToken;
            strncpy(data.instrumentType, instrumentType, 15);
            data.tickSize = tickSize;
        }
    }
    
private:
    static constexpr int TOKEN_OFFSET = 26000;
    static constexpr int ARRAY_SIZE = 90000;
    TouchlineData store_[ARRAY_SIZE];
    mutable std::shared_mutex mutex_;
};

// Global instance (one per UDP thread)
extern PriceStore g_nseFoPriceStore;

} // namespace nsefo
```

### 3.2 NSE CM / BSE Stores (Hash Map)

**Files**: 
- `lib/cpp_broadcast_nsecm/include/nsecm_price_store.h`
- `lib/cpp_broadcast_bsefo/include/bse_price_store.h`

```cpp
namespace nsecm {  // or bse

class PriceStore {
public:
    // Thread-safe update
    const TouchlineData* updateTouchline(const TouchlineData& data) {
        std::unique_lock lock(mutex_);
        store_[data.token] = data;
        return &store_[data.token];
    }
    
    // Thread-safe read
    const TouchlineData* getTouchline(int token) const {
        std::shared_lock lock(mutex_);
        auto it = store_.find(token);
        return (it != store_.end()) ? &it->second : nullptr;
    }
    
    // Initialize single token
    void initializeToken(int token, const char* symbol, ...fields) {
        std::unique_lock lock(mutex_);
        TouchlineData& data = store_[token];
        data.token = token;
        // ... copy static fields ...
    }
    
private:
    std::unordered_map<int, TouchlineData> store_;
    mutable std::shared_mutex mutex_;
};

extern PriceStore g_nseCmPriceStore;

} // namespace nsecm
```

---

## 4. Data Flow

### 4.1 Application Startup Flow

```
┌─────────────────────────────────────────────────────────┐
│ 1. APP STARTUP                                          │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ RepositoryManager::loadAll()                            │
│   - Load NSE_FO.csv                                     │
│   - Load NSE_CM.csv                                     │
│   - Load BSE_FO.csv                                     │
│   - Load BSE_CM.csv                                     │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ RepositoryManager::initializeDistributedStores()        │
│                                                          │
│   For each contract in NSE_FO.csv:                      │
│     g_nseFoPriceStore.initializeToken(                  │
│         token, symbol, displayName, lotSize,            │
│         strikePrice, optionType, expiryDate, ...        │
│     )                                                    │
│                                                          │
│   For each contract in NSE_CM.csv:                      │
│     g_nseCmPriceStore.initializeToken(...)              │
│                                                          │
│   For each contract in BSE:                             │
│     g_bseFoPriceStore.initializeToken(...)              │
│     g_bseCmPriceStore.initializeToken(...)              │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ RESULT: All g_stores pre-populated with:                │
│   ✓ symbol, displayName                                 │
│   ✓ lotSize, tickSize                                   │
│   ✓ strikePrice, optionType, expiryDate                 │
│   ✓ assetToken, instrumentType                          │
│   (Dynamic fields like ltp = 0 initially)               │
└─────────────────────────────────────────────────────────┘
```

### 4.2 Continuous UDP Data Flow

```
┌──────────────────────────────────────────────────────────┐
│ UDP PACKET ARRIVES (NSE FO example)                      │
│   Raw bytes from multicast socket                        │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│ MULTIPLE UDP PARSERS (Partial Updates)                   │
└──────────────────────────────────────────────────────────┘
           ↓                             ↓
┌──────────────────────────┐  ┌──────────────────────────┐
│ Msg 7200 Parser (Price)  │  │ Msg 7208 Parser (Depth)  │
└──────────────────────────┘  └──────────────────────────┘
           ↓                             ↓
    updatePrice(...)              updateDepth(...)
           ↓                             ↓
┌──────────────────────────────────────────────────────────┐
│ DISTRIBUTED STORE (THE ACCUMULATOR)                      │
│   Token 12345 Record:                                    │
│   [ Static Fields  ] -> Kept from Master                 │
│   [ Price Fields   ] <- Updated by 7200                  │
│   [ Depth Fields   ] <- Updated by 7208                  │
│   [ OI Fields      ] <- Updated by 7202                  │
└──────────────────────────────────────────────────────────┘
                           ↓
                   callback_(12345)
                           ↓
┌──────────────────────────────────────────────────────────┐
│ FeedHandler (Full State Delivery)                        │
│                                                           │
│   void onUpdate(int token) {                              │
│       // Pulls the "Fused" result                         │
│       const auto* state = g_store.getTouchline(token);    │
│                                                           │
│       // State now contains BOTH Price and Depth          │
│       // regardless of which packet came last.            │
│       distributeToUI(state);                              │
│   }                                                       │
└──────────────────────────────────────────────────────────┘
│                           ↓
│                   callback_(26000)
│                           ↓
│ ┌──────────────────────────────────────────────────────────┐
│ │ UdpBroadcastService (Qt Thread - Bridge Layer)           │
│ └──────────────────────────────────────────────────────────┘
│                           ↓
│               emit nseFoUpdate(26000)
│                           ↓
│ ┌──────────────────────────────────────────────────────────┐
│ │ FeedHandler (Qt Thread - Distribution Hub)               │
│ │                                                           │
│ │   void FeedHandler::onNseFoUpdate(int token) {           │
│ │       // Read from distributed store                     │
│ │       const auto* data =                                 │
│ │           nsefo::g_nseFoPriceStore.getTouchline(token);  │
│ │                                                           │
│ │       // Distribute to UI subscribers only               │
│ │       for (auto* sub : subscribers_[token]) {            │
│ │           sub->onUpdate(tick);                           │
│ │       }                                                   │
│ │   }                                                       │
│ └──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│ UI WIDGETS (MarketWatch, DepthWindow, etc.)              │
│   Display updated price, volume, depth                   │
└──────────────────────────────────────────────────────────┘
```

### 4.3 On-Demand Data Supply (User Subscribe)

```
┌──────────────────────────────────────────────────────────┐
│ USER ACTION: Add scrip to MarketWatch                    │
│   User selects: NIFTY 18JAN24 22000 CE                   │
│   Token: 26000, Segment: NSE_FO (2)                      │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│ UI LAYER (Qt Widget)                                     │
│ src/ui/marketwatch/MarketWatchTable.cpp                  │
│                                                           │
│   void addScrip(int segment, int token) {                │
│       // Request subscription from FeedHandler           │
│       FeedHandler::instance()->subscribe(                │
│           segment, token, this                           │
│       );                                                  │
│   }                                                       │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│ FeedHandler::subscribe() - HYBRID REGISTRATION           │
│ src/services/FeedHandler.cpp                             │
│                                                           │
│   void subscribe(int segment, int token, QObject* sub) { │
│       // 1. Register UI subscriber (Distribution list)   │
│       subscribers_[{segment, token}].append(sub);        │
│                                                           │
│       // 2. ENABLE FILTER IN UDP READER ⭐               │
│       //    Only first subscription triggers this        │
│       if (subscribers_[{segment, token}].size() == 1) {  │
│           UdpBroadcastService::instance()                │
│               ->enableToken(segment, token);             │
│       }                                                   │
│                                                           │
│       // 3. READ FROM STORE IMMEDIATELY (Cached data)    │
│       const TouchlineData* data = ...;                   │
│       if (data) sub->onUpdate(convertToTick(data));      │
│   }                                                       │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│ UI WIDGET UPDATES IMMEDIATELY                            │
│   ✓ Symbol: NIFTY 18JAN24 22000 CE                       │
│   ✓ Last Price: 125.50 (from yesterday/last tick)       │
│   ✓ Lot Size: 50                                         │
│   ✓ Expiry: 18-Jan-2024                                  │
│                                                           │
│   User doesn't wait for next UDP tick!                   │
└──────────────────────────────────────────────────────────┘
```

### 4.4 XTS WebSocket Data (Alternative Path)

```
┌──────────────────────────────────────────────────────────┐
│ XTS API: getQuote(segment, token)                        │
│   HTTP request to XTS server                             │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│ XTSMarketDataClient::getQuote()                          │
│ src/xts/XTSMarketDataClient.cpp                          │
│                                                           │
│   void getQuote(int segment, int token) {                │
│       // Make API call, emit signal on response          │
│       emit quoteReceived(segment, token, quoteData);     │
│   }                                                       │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│ FeedHandler (optional integration)                       │
│   - Can store XTS quote in g_store                       │
│   - Or handle separately for backup/validation           │
└──────────────────────────────────────────────────────────┘
```

---

## 5. Thread Safety Model

### 5.1 Lock Strategy

**Choice**: `std::shared_mutex` (C++17)

**Why**:
- Read:Write ratio = 10,000:1
- Multiple readers can read simultaneously (shared lock)
- Single writer has exclusive access (unique lock)
- Contention probability = 0.01% (negligible)

### 5.2 Lock Scopes

#### Write Lock (UDP Parser)
```cpp
void parse_message_7200(const MS_BCAST_MBO_MBP* msg) {
    TouchlineData data;
    // ... parse into data (no lock) ...
    
    {  // Minimal lock scope
        std::unique_lock lock(g_nseFoPriceStore.mutex_);
        g_nseFoPriceStore.store_[index] = data;  // ~100ns
    }  // Lock released
    
    callback_(data.token);  // No lock during callback!
}
```

**Performance**:
- Lock acquisition: ~50ns
- Hash map insert: ~100ns
- Total: ~150ns (0.00015ms)

#### Read Lock (FeedHandler)
```cpp
const TouchlineData* getTouchline(int token) const {
    std::shared_lock lock(mutex_);  // Shared - multiple readers OK
    auto it = store_.find(token);   // ~50ns
    return (it != store_.end()) ? &it->second : nullptr;
}  // Lock released automatically
```

**Performance**:
- Shared lock acquisition: ~30ns
- Hash lookup: ~50ns
- Total: ~80ns (0.00008ms)

### 5.3 Thread Interaction

```
┌─────────────────────────────────────────────────────────┐
│ UDP THREAD (Writer)                                     │
│   Frequency: ~5000 updates/second (NSE FO)              │
│   Lock: std::unique_lock (exclusive)                    │
│   Duration: ~150ns per write                            │
│   Total lock time: 5000 × 150ns = 0.75ms/sec            │
└─────────────────────────────────────────────────────────┘
                          ↓
                   [ SHARED DATA ]
                   g_nseFoPriceStore
                   (std::shared_mutex)
                          ↑
┌─────────────────────────────────────────────────────────┐
│ QT MAIN THREAD (Readers)                                │
│   Frequency: ~50 reads/second (user subscribes)         │
│   Lock: std::shared_lock (shared - no blocking)         │
│   Duration: ~80ns per read                              │
│   Conflict probability: 0.01% (negligible)              │
└─────────────────────────────────────────────────────────┘
```

### 5.4 Lock-Free Alternative (Future Optimization)

If profiling shows lock contention (unlikely):

```cpp
// Replace with atomic pointer + copy-on-write
class PriceStore {
private:
    std::atomic<TouchlineData*> data_ptr_;
    
public:
    void updateTouchline(const TouchlineData& data) {
        // Allocate new copy
        TouchlineData* newData = new TouchlineData(data);
        
        // Atomic swap (lock-free)
        TouchlineData* old = data_ptr_.exchange(newData);
        
        // Deferred delete (RCU-like)
        delete old;
    }
    
    const TouchlineData* getTouchline() const {
        return data_ptr_.load(std::memory_order_acquire);
    }
};
```

**Trade-off**: More complex, allocation overhead. Start with `std::shared_mutex`.

---

## 6. API Reference

### 6.1 Distributed Store API

#### PriceStore (NSE FO, NSE CM, BSE)

```cpp
// Initialize contract master fields (called at startup)
void initializeToken(
    int token,
    const char* symbol,
    const char* displayName,
    int lotSize,
    double strikePrice,
    const char* optionType,
    long long expiryDate,
    int assetToken,
    const char* instrumentType,
    double tickSize
);

// Update dynamic fields (called from UDP parser)
void updateTouchline(const TouchlineData& data);
const TouchlineData* updateTouchline(const TouchlineData& data);  // Returns pointer

// Read data (called from FeedHandler)
const TouchlineData* getTouchline(int token) const;

// Utility
size_t recordCount() const;
void clear();
```

### 6.2 FeedHandler API

#### Subscription Management

```cpp
// Subscribe to token updates (immediate data delivery)
void subscribe(
    int segment,        // 1=NSE_CM, 2=NSE_FO, 11=BSE_CM, 12=BSE_FO
    int token,          // Contract token
    QObject* subscriber // Receiver object
);

// Unsubscribe
void unsubscribe(int segment, int token, QObject* subscriber);

// Check subscription
bool hasSubscribers(int segment, int token) const;
```

#### Data Access

```cpp
// Get current data for token (on-demand)
TickData getCurrentData(int segment, int token) const;

// Internal: Handle UDP notifications
void onNseFoUpdate(int token);
void onNseCmUpdate(int token);
void onBseFoUpdate(int token);
void onBseCmUpdate(int token);
```

### 6.3 UDP Parser Notification Filtering API

#### Purpose
The `g_store` is updated for **every** packet to ensure background data is always fresh. The filter only restricts **notifications** (callbacks/signals) to the Qt thread.

```cpp
void parse_message_7200(const MS_BCAST_MBO_MBP* msg) {
    uint32_t token = be32toh_func(msg->token);
    
    // 1. UNCONDITIONAL STORE (Background State) ⭐
    TouchlineData data;
    // ... decode logic ...
    g_nseFoPriceStore.updateTouchline(data);
    
    // 2. SELECTIVE NOTIFICATION
    if (isTokenEnabled(token)) { 
        if (callback_) callback_(token);
    }
}
```

---

## 7. Implementation Phases

### Phase 1: BSE Integration ✅ COMPLETE
- ✅ BSE parser stores in g_bseFoPriceStore before callback
- ✅ Thread safety with std::shared_mutex
- ✅ Callback signature: token ID only

### Phase 2: NSE FO Integration (Next)

**Files to modify**: (~25 parser files)
- `lib/cpp_broacast_nsefo/src/parser/parse_message_7200.cpp`
- `lib/cpp_broacast_nsefo/src/parser/parse_message_7201.cpp`
- `lib/cpp_broacast_nsefo/src/parser/parse_message_7202.cpp`
- ... all message type parsers

**Changes per file**:
```cpp
// 1. Add include
#include "nsefo_price_store.h"

// 2. Change callback signature
- std::function<void(const TouchlineData&)> callback_;
+ std::function<void(int)> callback_;

// 4. Background Storage + Selective Notification ⭐
void parse_message_XXXX(...) {
    TouchlineData data;
    // ... parse ...
    
+   // Always update store first
+   g_nseFoPriceStore.updateTouchline(data);
    
+   // Only notify if UI is watching
+   if (isTokenEnabled(data.token)) {
+       if (callback_) callback_(data.token);
+   }
}
```

### Phase 3: NSE CM Integration

**Files**: `lib/cpp_broadcast_nsecm/src/parser/*.cpp`

Same pattern as NSE FO, use `g_nseCmPriceStore`.

### Phase 4: FeedHandler Integration

**File**: `src/services/FeedHandler.cpp`

```cpp
// Add includes
#include "nsefo_price_store.h"
#include "nsecm_price_store.h"
#include "bse_price_store.h"

void FeedHandler::subscribe(int segment, int token, QObject* subscriber) {
    // 1. Register subscriber
    subscribers_[{segment, token}].append(subscriber);
    
    // 2. ENABLE FILTER AT SOURCE ⭐
    if (subscribers_[{segment, token}].size() == 1) {
        UdpBroadcastService::instance()->enableToken(segment, token);
    }
    
    // 3. IMMEDIATE DATA DELIVERY (from Store)
    const void* data = nullptr;
    if (segment == 2) {
        data = nsefo::g_nseFoPriceStore.getTouchline(token);
    } else if (segment == 1) {
        data = nsecm::g_nseCmPriceStore.getTouchline(token);
    } else if (segment == 12) {
        data = bse::g_bseFoPriceStore.getRecord(token);
    } else if (segment == 11) {
        data = bse::g_bseCmPriceStore.getRecord(token);
    }
    
    if (data) {
        TickData tick = convertToTick(segment, data);
        subscriber->onUpdate(tick);
    }
}

void FeedHandler::onNseFoUpdate(int token) {
    if (!hasSubscribers(2, token)) return;
    
    const auto* data = nsefo::g_nseFoPriceStore.getTouchline(token);
    if (data) {
        TickData tick = convertToTick(2, data);
        distributeToSubscribers(2, token, tick);
    }
}

// Similar for onNseCmUpdate, onBseFoUpdate, onBseCmUpdate
```

### Phase 5: UdpBroadcastService Cleanup

**File**: `src/services/UdpBroadcastService.cpp`

```cpp
// Remove PriceCache update calls
- PriceCache::instance()->updatePrice(...);

// Keep only signal emission
void onNseFoTouchline(int token) {
    emit nseFoUpdate(token);  // Just notify
}
```

### Phase 6: Delete PriceCache

**Files to modify**:
- `include/services/PriceCache.h` → DELETE
- `src/services/PriceCache.cpp` → DELETE
- Remove from `CMakeLists.txt`

**Replace XTS usage**:
```cpp
// OLD
PriceCache::instance()->getPrice(token);

// NEW
XTSMarketDataClient::instance()->getQuote(segment, token);
```

---

## 8. Performance Characteristics

### Memory Usage

| Store | Type | Size | Tokens | Memory |
|-------|------|------|--------|--------|
| NSE FO | Array | 90,000 slots | Dense | ~108 MB |
| NSE CM | Hash | ~2,000 tokens | Sparse | ~512 KB |
| BSE FO | Hash | ~1,000 tokens | Sparse | ~256 KB |
| BSE CM | Hash | ~3,000 tokens | Sparse | ~768 KB |
| **Total** | | | | **~110 MB** |

### Latency Budget

| Operation | Duration | Notes |
|-----------|----------|-------|
| UDP packet arrival | 0µs | Baseline |
| Parser decode | 2-5µs | Per packet |
| Store write (with lock) | 0.15µs | Hash insert + lock |
| Callback emission | 0.1µs | Function pointer call |
| Qt signal emit | 1-2µs | Qt overhead |
| FeedHandler read (with lock) | 0.08µs | Hash lookup + lock |
| **Total UDP→UI** | **3-8µs** | Sub-millisecond! |

### Throughput

| Scenario | Rate | Performance |
|----------|------|-------------|
| NSE FO peak | 50,000 updates/sec | No contention |
| All segments combined | 100,000 updates/sec | CPU: ~5% |
| Read:Write ratio | 10,000:1 | Lock contention: 0.01% |

---

## 9. Testing Strategy

### Unit Tests

```cpp
TEST(PriceStore, InitializeAndUpdate) {
    nsefo::PriceStore store;
    
    // Initialize with contract master
    store.initializeToken(26000, "NIFTY", "NIFTY 18JAN24 22000 CE",
                         50, 22000.0, "CE", 1705622400, ...);
    
    // Verify static fields preserved
    const auto* data = store.getTouchline(26000);
    ASSERT_NE(data, nullptr);
    EXPECT_STREQ(data->symbol, "NIFTY");
    EXPECT_EQ(data->lotSize, 50);
    
    // Update dynamic fields
    TouchlineData update;
    update.token = 26000;
    update.ltp = 125.50;
    update.volume = 1000000;
    store.updateTouchline(update);
    
    // Verify dynamic updated, static preserved
    data = store.getTouchline(26000);
    EXPECT_DOUBLE_EQ(data->ltp, 125.50);
    EXPECT_EQ(data->volume, 1000000);
    EXPECT_STREQ(data->symbol, "NIFTY");  // Static preserved!
}

TEST(PriceStore, ThreadSafety) {
    nsefo::PriceStore store;
    
    // Writer thread (UDP simulation)
    std::thread writer([&]() {
        for (int i = 0; i < 100000; ++i) {
            TouchlineData data;
            data.token = 26000;
            data.ltp = i * 0.01;
            store.updateTouchline(data);
        }
    });
    
    // Reader threads (UI simulation)
    std::vector<std::thread> readers;
    for (int i = 0; i < 10; ++i) {
        readers.emplace_back([&]() {
            for (int j = 0; j < 100000; ++j) {
                const auto* data = store.getTouchline(26000);
                ASSERT_NE(data, nullptr);
            }
        });
    }
    
    writer.join();
    for (auto& t : readers) t.join();
    
    // No crashes = success!
}
```

### Integration Tests

1. **Immediate Subscribe Test**: 
   - Add illiquid option to MarketWatch
   - Verify data appears instantly (from store, not UDP)

2. **Continuous Update Test**:
   - Subscribe to liquid stock (NIFTY)
   - Verify UI updates with every UDP tick

3. **Multi-Segment Test**:
   - Subscribe to tokens from NSE FO, NSE CM, BSE FO, BSE CM simultaneously
   - Verify all segments work correctly

---

## 10. Migration Checklist

- [ ] Phase 1: BSE Integration ✅
  - [x] BSE parser stores first
  - [x] Thread safety added
  - [x] Callback signature changed
  
- [ ] Phase 2: NSE FO Integration
  - [ ] Update 25 parser files
  - [ ] Change callback signature
  - [ ] Store before callback
  
- [ ] Phase 3: NSE CM Integration
  - [ ] Update parser files
  - [ ] Same pattern as NSE FO
  
- [ ] Phase 4: FeedHandler Integration
  - [ ] Add `enableToken` / `disableToken` logic for hybrid filtering
  - [ ] Add immediate data delivery on subscribe
  - [ ] Update notification handlers to use Token ID
  
- [ ] Phase 5: UdpBroadcastService Cleanup
  - [ ] Remove PriceCache update calls
  - [ ] Simplify to signal emission only
  
- [ ] Phase 6: Delete PriceCache
  - [ ] Delete files
  - [ ] Update XTS integration to use getQuote
  - [ ] Remove from CMakeLists.txt

---

## 11. Troubleshooting

### Issue: UI not updating
**Check**:
1. Is token subscribed? `FeedHandler::hasSubscribers()`
2. Is data in store? `g_store.getTouchline(token)`
3. Is UDP receiving packets? Check parser logs

### Issue: Crashes on subscribe
**Check**:
1. Thread safety: Are locks properly acquired?
2. Null pointer: Does store have data for token?
3. Segment mismatch: Using correct g_store for segment?

### Issue: Slow performance
**Check**:
1. Lock contention: Profile with `perf` or VS profiler
2. Too many subscribers: Optimize distributeToSubscribers()
3. Parser overhead: Optimize decode logic

---

## 12. Future Enhancements

### Optimization Opportunities
1. **Lock-free stores**: Atomic pointers + RCU for zero contention
2. **SIMD parsing**: Vectorize packet decode (4x speedup)
3. **Batch notifications**: Accumulate tokens, emit batch signal
4. **Memory pool**: Pre-allocate TouchlineData structs

### Feature Extensions
1. **Historical data**: Add circular buffer to each store entry
2. **Aggregations**: Calculate VWAP, OHLC in store itself
3. **Snapshots**: Periodic dump of entire store to disk
4. **Replication**: Sync stores across multiple instances

---

**End of Document**

For questions or clarifications, refer to:
- Source code: `lib/cpp_broadcast_*/include/*.h`
- Implementation example: BSE parser (completed)
- Architecture decisions: This document Section 2
