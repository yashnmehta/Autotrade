# Current Architecture Analysis & Recovery Plan

## Date: January 16, 2026

---

## 🔍 CURRENT STATE ANALYSIS

### What EXISTS Now:

#### 1. **Distributed Stores (Created but partially integrated)**
- ✅ `lib/cpp_broacast_nsefo/include/nsefo_price_store.h` - NSE FO indexed array
- ✅ `lib/cpp_broadcast_nsecm/include/nsecm_price_store.h` - NSE CM hash map  
- ✅ `lib/cpp_broadcast_bsefo/include/bse_price_store.h` - BSE FO/CM hash maps
- ✅ Global instances: `g_nseFoPriceStore`, `g_nseCmPriceStore`, `g_bseFoPriceStore`, `g_bseCmPriceStore`
- ✅ Contract master fields added to data structures (symbol, expiry, strike, lotSize, etc.)
- ✅ `initializeToken()` methods for pre-population
- ✅ CMakeLists.txt created for all 3 UDP libraries

#### 2. **RepositoryManager Integration**
- ✅ `initializeDistributedStores()` method implemented
- ✅ Called after loading contract masters
- ✅ Pre-populates stores with contract metadata

#### 3. **Existing Cache & Distribution System**
- ✅ `PriceCache` (singleton) - Currently used for caching
- ✅ `FeedHandler` (singleton) - Pub/sub distribution to UI widgets
- ✅ `UdpBroadcastService` - Bridge between UDP and Qt

#### 4. **Current Data Flow**
```
UDP Parser → callback(data) → UdpBroadcastService 
    ↓
    ├→ PriceCache.updatePrice() 
    ├→ FeedHandler.distribute()
    └→ emit signals
```

#### 5. **Partial Integration**
- 🟡 ONE line in UdpBroadcastService calls `g_nseFoPriceStore.updateTouchline()`
- ❌ But result not used effectively
- ❌ UDP parsers still create temp structs, don't store in g_store first

---

## ❌ PROBLEMS IDENTIFIED

### 1. **Redundant Caching**
- `g_store` (distributed, thread-local, pre-populated with contract master)
- `PriceCache` (centralized, Qt-based, no contract master)
- **Both doing same job!**

### 2. **Incomplete Integration**
- g_stores exist but only used in ONE callback
- UDP parsers don't use stores
- FeedHandler doesn't read from g_stores

### 3. **Missing Use Case**
- **Your requirement**: On subscribe → get immediate data from store (for illiquid contracts)
- **Currently**: FeedHandler has no access to g_stores

### 4. **Thread Access Issue**
- g_stores are in UDP thread (pure C++)
- FeedHandler/UI in Qt main thread
- Need safe cross-thread read access

---

## 🎯 DESIRED ARCHITECTURE (What You Want)

### Data Flow:
```
1. APP STARTUP:
   CSV Load → RepositoryManager → g_stores pre-populated with contract master
   
2. UDP UPDATE:
   UDP Packet → Parser → g_store.update(dynamic data only)
                           ↓
                     callback(token) → FeedHandler
                                         ↓
                                   Distribute to subscribers

3. USER SUBSCRIBES TOKEN:
   UI → FeedHandler.subscribe(segment, token)
       ↓
       ├→ Read g_store.getTouchline(token) → Send IMMEDIATELY (illiquid fix!)
       └→ Register for future updates
```

### Components:
- **g_stores** (distributed): Source of truth (static + dynamic data)
- **FeedHandler**: Reads from g_stores, distributes updates
- **PriceCache**: ❌ DELETE or repurpose as backup
- **UdpBroadcastService**: Minimal adapter, just connects callbacks

---

## 📋 STEP-BY-STEP RECOVERY PLAN

### **PHASE 1: Clean Analysis (Current)**
- ✅ Document what exists
- ✅ Identify redundancies
- ✅ Define target architecture

### **PHASE 2: Enable Safe Cross-Thread Read Access**
**Goal**: Let FeedHandler read from g_stores safely

**Steps**:
1. Add `QReadWriteLock` or `std::shared_mutex` to each g_store
2. UDP thread: Write lock when updating
3. Qt thread: Read lock when subscribing
4. Or: Keep stores read-only from Qt (UDP owns writes)

**Files**:
- `lib/cpp_broacast_nsefo/include/nsefo_price_store.h`
- `lib/cpp_broadcast_nsecm/include/nsecm_price_store.h`  
- `lib/cpp_broadcast_bsefo/include/bse_price_store.h`

### **PHASE 3: Update FeedHandler to Read from g_stores**
**Goal**: Implement "immediate data on subscribe" feature

**Steps**:
1. Add g_store includes to FeedHandler
2. Modify `subscribe()` to:
   ```cpp
   void FeedHandler::subscribe(int segment, int token, ...) {
       // NEW: Get immediate data from g_store
       if (segment == 2) {
           auto* data = nsefo::g_nseFoPriceStore.getTouchline(token);
           if (data) {
               // Send to subscriber immediately!
               emit tickUpdate(convertToTick(data));
           }
       }
       // ... continue with existing subscription registration
   }
   ```

**Files**:
- `include/services/FeedHandler.h`
- `src/services/FeedHandler.cpp`

### **PHASE 4: Simplify UdpBroadcastService**
**Goal**: Remove redundant PriceCache usage

**Steps**:
1. Keep: `g_store.update()` calls
2. Keep: `FeedHandler.distribute()` calls  
3. Remove: `PriceCache.updatePrice()` calls (redundant)
4. Simplify: Callbacks just notify FeedHandler

**Files**:
- `src/services/UdpBroadcastService.cpp`

### **PHASE 5: Update UDP Parsers (Optional Optimization)**
**Goal**: Store in g_store BEFORE callback

**Steps**:
1. Add `#include "nsefo_price_store.h"` to parsers
2. Modify parse functions:
   ```cpp
   void parse_message_7200(const MS_BCAST_MBO_MBP* msg) {
       // Parse data...
       TouchlineData data;
       // ... populate data ...
       
       // Store first
       g_nseFoPriceStore.updateTouchline(data);
       
       // Then callback
       callback(data.token);  // Just token ID
   }
   ```

**Files**:
- `lib/cpp_broacast_nsefo/src/parser/parse_message_7200.cpp`
- ~25 other parser files

### **PHASE 6: Deprecate PriceCache**
**Goal**: Remove redundancy

**Options**:
A. Delete PriceCache entirely (g_stores replace it)
B. Keep PriceCache as backup for XTS WebSocket data (not UDP)
C. Repurpose PriceCache to wrap g_stores with Qt-friendly API

**Recommendation**: Option B or C

---

## 🚦 DECISION POINT

**Before proceeding, we need to decide:**

### Question 1: Thread Safety Approach
- **Option A**: Add mutex to g_stores (simple, slight performance cost)
- **Option B**: UDP writes, Qt reads without locks (lock-free, complex)
- **Your choice?**

### Question 2: PriceCache Fate
- **Option A**: Delete PriceCache (g_stores replace it)
- **Option B**: Keep for XTS WebSocket data only
- **Option C**: Wrap g_stores with PriceCache API

- **Your choice?**

### Question 3: Parser Updates
- **Option A**: Update all parsers to store first (clean, lots of files)
- **Option B**: Store in UdpBroadcastService only (quick, less clean)
- **Your choice?**

### Question 4: Callback Signature
- **Option A**: Keep current (pass data by value)
- **Option B**: Pass pointer (zero-copy, need thread safety)
- **Option C**: Pass token ID only (lightweight, FeedHandler reads store)
- **Your choice?**

---

## 📊 IMPACT ANALYSIS

### Current Mess Created:
- 6 new files created (price stores)
- 3 CMakeLists.txt added/modified
- RepositoryManager modified
- Data structures modified (contract master fields added)
- But: Minimal integration, mostly isolated changes

### Recovery Effort:
- **Phase 2**: 2-3 hours (thread safety)
- **Phase 3**: 1-2 hours (FeedHandler update)
- **Phase 4**: 1 hour (UdpBroadcastService cleanup)
- **Phase 5**: 4-6 hours (parser updates - optional)
- **Phase 6**: 2-3 hours (PriceCache decision)

**Total**: 10-15 hours for complete clean implementation

---

## 🎯 RECOMMENDED NEXT STEPS

1. **YOU DECIDE** on 4 questions above
2. **I implement** Phase 2 (thread safety)
3. **Test**: Build, verify no crashes
4. **I implement** Phase 3 (FeedHandler + immediate data)
5. **Test**: Subscribe to illiquid token, verify immediate data
6. **Continue** with Phases 4-6 based on success

---

**Ready to proceed? Please answer the 4 questions so I can implement the exact solution you want.**
