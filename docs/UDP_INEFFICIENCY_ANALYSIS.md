# Deep Dive Analysis: UDP Broadcast System Inefficiencies

**Date**: 2026-02-06
**Scope**: UDP Reader, Event Propagation, Data Granularity
**Objective**: Identify operational inefficiencies in the current "Unified Callback" architecture.

---

## 1. Executive Summary

**Current State**:
The system currently implements a **"One-Size-Fits-All"** approach. Regardless of whether a packet contains a simple Last Traded Price (LTP) update (Msg 7202), a Depth update (Msg 7208), or a Touchline update (Msg 7200), the system performs a **full state reconstruction**.

**Critical Flaws**:
1.  **Over-Processing**: A lightweight Ticker update (Msg 7202, ~40 bytes) triggers the construction of a full `UDP::MarketTick` (>200 bytes), including copying 5 levels of Bid/Ask depth that haven't changed.
2.  **Loss of Event Semantics**: The UI cannot distinguish *why* an update occurred. It just sees a new "Tick". It doesn't know if only the LTP changed, or if the Order Book shifted.
3.  **Redundant Traffic**: The `FeedHandler` broadcasts the full state to all subscribers every time, saturating the event loop with redundant data.

---

## 2. Detailed Technical Analysis

### **A. The "Unified Callback" Bottleneck**

**File**: `src/services/UdpBroadcastService.cpp` (Lines 396-425)

```cpp
auto unifiedCallback = [this](int32_t token) {
    // 1. FETCH FULL STATE
    const auto *data = nsefo::g_nseFoPriceStore.getUnifiedState(token);
    
    // 2. CONVERT FULL STATE (Costly!)
    UDP::MarketTick udpTick = convertNseFoUnified(*data); // <-- Copies EVERYTHING
    
    // 3. BROADCAST FULL STATE
    FeedHandler::instance().onUdpTickReceived(udpTick);
};
```

**What happens on a Ticker Update (Msg 7202)?**
1.  Parser updates `ltp`, `volume` in global store.
2.  Callback fires.
3.  `convertNseFoUnified` creates a new `UDP::MarketTick`.
4.  It copies `bids[0..4]` and `asks[0..4]` from the store (which are stale/unchanged).
5.  It emits this fat object.
6.  UI receives it and re-renders the *entire* row, including unchanged depth columns.

### **B. Message-Specific Inefficiencies**

| Message Type | What Changed | What We Do Now (Inefficient) | Ideal Behavior |
| :--- | :--- | :--- | :--- |
| **7202 (Ticker)** | LTP, LTQ, ATP, Vol | **Full Rebuild**: Copies 10 depth levels + OHLC | **Lite Update**: Send `Type=TRADE`, containing only Price/Vol/LTP |
| **7208 (Depth)** | Bids/Asks Only | **Full Rebuild**: Copies LTP, OHLC, Open Interest | **Depth Update**: Send `Type=DEPTH`, containing only Levels |
| **7200 (Touchline)** | Top Bid/Ask + LTP | **Full Rebuild**: Copies full 5-level depth (levels 2-5 unchanged) | **Touchline Update**: Send `Type=TOUCHLINE`, containing Top 1 + LTP |

### **C. Downstream Impact (Greeks)**

Currently:
```cpp
if (greeksService.isEnabled() && data->ltp > 0) {
    greeksService.onPriceUpdate(token, data->ltp, 2); 
}
```
*   **Issue**: Greeks are recalculated even if the update was purely a **Depth Change** (Msg 7208) which didn't affect the LTP.
*   **Waste**: If the Order Book shifts but LTP stays 24000.00, we still trigger a Greeks calculation cycle. Safe, but wasteful.

---

## 3. Operational Issues & Risks

1.  **CPU Saturation**: During high volatility, re-copying unchanged static data (Open, High, Low, PrevClose) and depth arrays millions of times per second wastes L1/L2 cache bandwidth.
2.  **UI Flicker / Repaint**: Since the UI receives a "new object", it may assume everything changed and trigger a full re-render of the ScripBar/MarketWatch row.
3.  **Network Saturation (Internal)**: If we extend this logic to send data over a WebSocket to a frontend (React/JS), we are sending 5x more JSON/Binary data than necessary.

---

## 4. Proposed Solution: "Granular Event Architecture"

We need to break the `unifiedCallback` into distinct handlers.

### **New Architecture**

**1. Defined Event Types**:
```cpp
enum class UpdateType {
    TRADE,      // LTP, Vol, ATP (Msg 7202)
    DEPTH,      // Bids, Asks (Msg 7208)
    QUOTE,      // Top Bid/Ask + LTP (Msg 7200)
    OI,         // Open Interest (Msg 7202/7201)
    FULL        // Initial snapshot or full recovery
};
```

**2. Distinct Callbacks in `UdpBroadcastService`**:
*   `onTickerUpdate(token)` -> Emit `MarketTick { type=TRADE, ltp=X, ... }`
*   `onDepthUpdate(token)` -> Emit `MarketTick { type=DEPTH, bids=..., asks=... }`

**3. Frontend Handling**:
*   Subscriber receives `TRADE` -> Only update LTP label color/value.
*   Subscriber receives `DEPTH` -> Only update Order Book widget.

---

## 5. Next Steps

1.  **Stop Merging Callbacks**: Unbind `unifiedCallback` from all registry events.
2.  **Create Specialized Callbacks**: Implement `handleTickerUpdate`, `handleDepthUpdate`, `handleTradeUpdate`.
3.  **Update `UDP::MarketTick`**: Add a `UpdateFlags` or `UpdateType` field to indicate validity of fields (e.g., "Depth valid", "LTP valid").
