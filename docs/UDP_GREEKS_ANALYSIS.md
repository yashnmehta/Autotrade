# UDP Reader & Price Store Integration Analysis

**Date**: 2026-02-06
**Scope**: UDP Reader, Price Store Gateway, Greeks Calculation

---

## 1. Current Architecture Analysis

### **A. UDP Reader & Price Store Data Flow**
The system uses a highly optimized, tiered architecture for market data:

1.  **Network Layer (`lib/cpp_broadcast_nsefo`)**:
    *   Receives multicast UDP packets.
    *   Parsers immediately update the **Decentralized Price Store** (`nsefo::g_nseFoPriceStore`).
    *   This store is an array-indexed, thread-safe (Shared Mutex) in-memory database.
    *   **Status**: ✅ **Working**. "Price Cache" storage is already happening at the lowest latency layer.

2.  **Service Layer (`UdpBroadcastService`)**:
    *   Registers callbacks with the Network Layer.
    *   On callback:
        *   **Reads** the latest state from the Global Price Store (Zero-Copy).
        *   **Converts** to `UDP::MarketTick`.
        *   **Distributes** via `FeedHandler` to UI/Subscribers.
        *   **Triggers** Greeks Calculation (`onPriceUpdate`).

3.  **Application Gateway (`PriceStoreGateway`)**:
    *   Acting as a unified facade over the decentralized stores.
    *   `getUnifiedState(segment, token)` delegates directly to the specific global store (e.g., `nsefo::g_nseFoPriceStore`).
    *   **Status**: ✅ **Integrated**. The application correctly accesses the low-latency store.

### **B. Greeks Calculation Flow**
*   **Trigger**: `UdpBroadcastService` calls `GreeksCalculationService::onPriceUpdate` on every tick where LTP > 0.
*   **Throttling**: The current implementation contains explicit throttling:
    ```cpp
    // GreeksCalculationService.cpp
    if (now - lastTime < m_config.throttleMs) return; // Time-based
    if (priceDiff < 0.001) return;                    // Change-based
    ```
*   **Compliance**: ❌ **Does NOT meet requirement** "calculate on every feed received in NSE option".

---

## 2. Required Changes

### **Requirement 1: "Store price update in price cache"**
*   **Analysis**: The `nsefo::g_nseFoPriceStore` **IS** the price cache.
*   **Finding**: Updates are already being applied immediately upon packet reception in the parsing layer.
*   **Action**: No changes needed to the storage mechanism itself. It is already highly optimized. We just need to verify the Greeks service reads from here (which it does).

### **Requirement 2: "Greeks calculation on every feed (NSE Option)"**
*   **Analysis**: The throttling logic prevents calculation on every feed.
*   **Action**: Modify `GreeksCalculationService::onPriceUpdate` to **bypass throttling** specifically for:
    *   **Segment**: NSE_FO (2)
    *   **Instrument**: Option Contracts (implied by feed type, or check contract type)

---

## 3. Implementation Plan

### **Step 1: Modify `GreeksCalculationService.cpp`**
**Objective**: Disable throttling for NSE FO.

**Changes**:
1.  In `onPriceUpdate`:
    *   Add check: `if (exchangeSegment == 2 /*NSEFO*/) { bypass_throttling = true; }`
    *   Skip `throttleMs` check if bypassed.
    *   Skip `priceDiff` check if bypassed.
2.  In `onUnderlyingPriceUpdate`:
    *   Ensure linked options (NSE FO) are also updated immediately without "Liquid/Illiquid" delay filtering if possible, or tune the threshold.

### **Step 2: Verification**
1.  **Manual Test**: Run specific test case or check logs to ensure Greeks are calculated rapidly (logs currently show "THROTTLED" - should stop showing that for NSEFO).

---

## 4. Proposed Logic

```cpp
void GreeksCalculationService::onPriceUpdate(uint32_t token, double ltp, int segment) {
    // ...
    bool isNseOption = (segment == 2); // NSE_FO
    
    // Bypass throttle for NSE Options
    if (!isNseOption) {
        if (now - lastTime < throttleMs) return;
        if (priceDiff < smallChange) return;
    }
    
    calculateForToken(token, segment);
}
```

This ensures maximum responsiveness for the critical NSE Options segment while preserving performance protections for other segments if needed.
