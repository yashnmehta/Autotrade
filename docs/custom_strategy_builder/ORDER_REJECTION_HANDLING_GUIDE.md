# Order Rejection Handling - Comprehensive Guide

**Project:** Trading Terminal C++  
**Date:** February 17, 2026  
**Version:** 1.0  
**Purpose:** Define robust order rejection handling strategies for automated trading systems

---

## Table of Contents

1. [Introduction](#introduction)
2. [Types of Rejections](#types-of-rejections)
3. [Rejection Codes & Meanings](#rejection-codes--meanings)
4. [Rejection Classification](#rejection-classification)
5. [Handling Strategies](#handling-strategies)
6. [State Machine Architecture](#state-machine-architecture)
7. [Retry Logic](#retry-logic)
8. [Recovery Mechanisms](#recovery-mechanisms)
9. [Logging & Alerting](#logging--alerting)
10. [Code Implementation](#code-implementation)
11. [Testing Scenarios](#testing-scenarios)

---

## Introduction

### Why Rejection Handling is Critical

Order rejections are **inevitable** in automated trading. Improper handling leads to:

- ❌ **Duplicate Orders**: Retry without checking state → double position
- ❌ **Missed Opportunities**: Give up too early → lose profitable trades
- ❌ **Infinite Loops**: Retry forever on permanent rejections → system overload
- ❌ **Capital Lock**: Margin blocked but order not executed → capital inefficiency
- ❌ **Compliance Violations**: Excessive order rate → exchange penalties

### Proper Rejection Handling Achieves:

- ✅ **Resilience**: Handle transient network/exchange issues
- ✅ **Efficiency**: Distinguish retryable vs permanent failures
- ✅ **Compliance**: Respect exchange order rate limits
- ✅ **Transparency**: Clear logging for audit trail
- ✅ **Safety**: Prevent runaway order loops

---

## Types of Rejections

### 1. RMS (Risk Management System) Rejections

**Source:** Broker's internal risk management system (pre-exchange validation)

**Characteristics:**
- Happens **before** order reaches exchange
- No exchange order ID generated
- Instant rejection (< 100ms)
- Most common type (60-70% of rejections)

**Common Reasons:**
- Insufficient margin/funds
- Position limit breach
- Order value exceeds limits
- Invalid instrument/strike
- Blocked scrip
- Account suspension

**Severity:** **CRITICAL** - Indicates account/risk issues

---

### 2. Exchange Rejections

**Source:** NSE/BSE/MCX exchange validation

**Characteristics:**
- Happens **after** order reaches exchange
- Exchange order ID may be generated
- Slower rejection (200-500ms)
- Less common (20-30% of rejections)

**Common Reasons:**
- Circuit filter hit
- Scrip in ASM/GSM list
- Price outside allowed range
- Invalid order attributes
- Exchange system downtime
- Order quote mismatch

**Severity:** **MEDIUM** - Indicates market/instrument issues

---

### 3. Broker API Rejections

**Source:** Broker's API layer (pre-RMS validation)

**Characteristics:**
- Happens **immediately** (local validation)
- No network call made
- Very fast rejection (< 10ms)
- Rare if API used correctly (5-10% of rejections)

**Common Reasons:**
- Invalid API credentials
- Malformed request payload
- Missing required fields
- Invalid data types
- API rate limit exceeded
- Session expired

**Severity:** **LOW** - Indicates integration issues

---

### 4. Network Failures

**Source:** Network/connectivity issues

**Characteristics:**
- Timeout without response
- Connection drops
- DNS resolution failure
- Ambiguous state (order may or may not be placed)

**Common Reasons:**
- Internet connectivity loss
- Broker server downtime
- Firewall blocking
- DNS issues
- SSL certificate errors

**Severity:** **HIGH** - Uncertain order state

---

## Rejection Codes & Meanings

### NSE/BSE Standard Rejection Codes

| Code | Category | Description | Retryable? | Action |
|------|----------|-------------|------------|--------|
| **17070** | RMS | Insufficient funds | ⚠️ Conditional | Check available margin, reduce quantity |
| **17071** | RMS | Order value exceeds limit | ⚠️ Conditional | Reduce order size or split orders |
| **17072** | RMS | Position limit breach | ❌ No | Square off existing positions first |
| **17073** | RMS | Quantity freeze limit | ❌ No | Split into smaller orders |
| **17074** | RMS | Price outside allowed range | ✅ Yes | Adjust limit price within range |
| **17075** | RMS | Scrip blocked by RMS | ❌ No | Contact broker, don't retry |
| **17076** | RMS | Account suspended | ❌ No | Critical - halt all trading |
| **17080** | Exchange | Circuit filter hit | ⏳ Wait | Wait for circuit to open, monitor market |
| **17081** | Exchange | ASM/GSM scrip | ❌ No | Additional margin required, check list |
| **17082** | Exchange | Invalid strike/expiry | ❌ No | Verify instrument master, update data |
| **17083** | Exchange | Order type not allowed | ❌ No | Change to allowed order type |
| **17084** | Exchange | Market closed | ⏳ Wait | Wait for market open |
| **17085** | Exchange | Pre-open session | ⏳ Wait | Wait for normal trading session |
| **17090** | Network | Order timeout | ✅ Yes | Check order status, retry if not placed |
| **17091** | Network | Connection failure | ✅ Yes | Reconnect, check order status |
| **17092** | API | Invalid session | ✅ Yes | Re-authenticate, retry |
| **17093** | API | Rate limit exceeded | ⏳ Wait | Wait 60 sec, implement throttling |
| **17094** | API | Duplicate order check | ⚠️ Conditional | Check if previous order went through |
| **17095** | Validation | Invalid quantity | ❌ No | Correct to exchange lot size |
| **17096** | Validation | Invalid price | ❌ No | Adjust to valid tick size |
| **17097** | Validation | Invalid product type | ❌ No | Change MIS/CNC/NRML as appropriate |

### Broker-Specific Codes (Examples)

**Zerodha:**
- `Order cancelled` → User cancellation (not rejection)
- `Quantity should be multiple of lot size` → Fix lot size
- `Trigger price for stoploss orders should be less than` → Adjust trigger

**Angel Broking:**
- `RMS:Margin Exceeds` → Insufficient margin
- `RMS:Open orders limit exceeded` → Cancel pending orders

**ICICI Direct:**
- `NEO_ORDER_ERR_001` → Invalid order parameters
- `NEO_ORDER_ERR_002` → Insufficient balance

---

## Rejection Classification

### Classification Matrix

```cpp
enum RejectionCategory {
    TRANSIENT,          // Temporary issue, retry automatically
    CONDITIONAL,        // May be retryable after user action
    PERMANENT,          // Don't retry, log and alert
    CRITICAL,           // Halt all trading immediately
    AMBIGUOUS           // Uncertain state, verify before retry
};

enum RejectionSeverity {
    LOW,                // Minor issue, won't affect trading
    MEDIUM,             // Important, may affect this trade
    HIGH,               // Serious, may affect strategy
    CRITICAL            // Severe, halt everything
};
```

### Decision Tree

```
Rejection Received
        ↓
┌───────┴────────────────────────────────────────────┐
│ Is rejection code known?                            │
├─────────────────────────────────────────────────────┤
│ YES → Classify based on rejection code table        │
│ NO  → Treat as AMBIGUOUS + HIGH severity           │
└───────┬─────────────────────────────────────────────┘
        ↓
┌───────┴────────────────────────────────────────────┐
│ Check Rejection Category                            │
├─────────────────────────────────────────────────────┤
│ TRANSIENT   → Auto-retry with backoff              │
│ CONDITIONAL → Check if condition fixable           │
│ PERMANENT   → Log, alert, don't retry              │
│ CRITICAL    → Kill switch, halt all trading        │
│ AMBIGUOUS   → Query order status, then decide      │
└───────┬─────────────────────────────────────────────┘
        ↓
   Take Action
```

---

## Handling Strategies

### Strategy 1: Transient Rejection Handling

**Use Case:** Network timeouts, temporary API issues, brief exchange downtime

**Approach:** Exponential backoff retry

```cpp
struct TransientRejectionHandler {
    int maxRetries = 3;
    int initialDelayMs = 500;        // 500ms first retry
    double backoffMultiplier = 2.0;  // Double each time
    int maxDelayMs = 8000;           // Cap at 8 seconds
    
    bool shouldRetry(int attemptCount) {
        return attemptCount < maxRetries;
    }
    
    int getDelayMs(int attemptCount) {
        int delay = initialDelayMs * pow(backoffMultiplier, attemptCount);
        return std::min(delay, maxDelayMs);
    }
    
    // Example:
    // Attempt 1: Wait 500ms
    // Attempt 2: Wait 1000ms
    // Attempt 3: Wait 2000ms
    // After 3 attempts: Give up, log failure
};
```

**Example Flow:**

```
Order Rejected (Network Timeout)
        ↓
Wait 500ms
        ↓
Retry Attempt 1
        ↓
Still Failed? → Wait 1000ms → Retry Attempt 2
                      ↓
                Still Failed? → Wait 2000ms → Retry Attempt 3
                                    ↓
                               Still Failed? → Give up, alert user
```

---

### Strategy 2: Conditional Rejection Handling

**Use Case:** Insufficient margin, position limits, order value limits

**Approach:** Attempt automatic fix, then retry

```cpp
struct ConditionalRejectionHandler {
    bool attemptFix(RejectionCode code, Order& order) {
        switch(code) {
            case INSUFFICIENT_MARGIN:
                return reduceQuantityToFitMargin(order);
            
            case ORDER_VALUE_EXCEEDS_LIMIT:
                return splitOrderIntoChunks(order);
            
            case PRICE_OUTSIDE_RANGE:
                return adjustPriceToValidRange(order);
            
            case QUANTITY_FREEZE:
                return splitOrderBelowFreezeQty(order);
            
            default:
                return false; // Can't fix automatically
        }
    }
    
    bool reduceQuantityToFitMargin(Order& order) {
        double availableMargin = getAvailableMargin();
        double requiredMarginPerUnit = getMarginPerUnit(order.symbol);
        
        int maxQty = availableMargin / requiredMarginPerUnit;
        
        if (maxQty < order.quantity && maxQty > 0) {
            // Round down to lot size
            maxQty = (maxQty / order.lotSize) * order.lotSize;
            
            if (maxQty > 0) {
                order.quantity = maxQty;
                return true; // Fixed, can retry
            }
        }
        return false; // Can't fix
    }
    
    bool splitOrderIntoChunks(Order& order) {
        double maxOrderValue = getMaxOrderValue();
        double orderValue = order.quantity * order.price;
        
        if (orderValue > maxOrderValue) {
            int chunksNeeded = ceil(orderValue / maxOrderValue);
            order.quantity = order.quantity / chunksNeeded;
            order.isPartOfSlice = true;
            order.remainingSlices = chunksNeeded - 1;
            return true; // Can retry with smaller qty
        }
        return false;
    }
};
```

---

### Strategy 3: Permanent Rejection Handling

**Use Case:** Account suspended, scrip blocked, invalid instrument

**Approach:** Log, alert, don't retry

```cpp
struct PermanentRejectionHandler {
    void handle(RejectionCode code, Order& order) {
        // Log to database
        logRejection(code, order, "PERMANENT");
        
        // Send immediate alert
        sendAlert(AlertLevel::HIGH, 
                 "Permanent rejection: " + getRejectionMessage(code));
        
        // Mark order as failed
        order.status = OrderStatus::FAILED_PERMANENT;
        
        // If critical code, trigger kill switch
        if (isCriticalRejection(code)) {
            triggerKillSwitch("Critical rejection: " + code);
        }
        
        // Update strategy state
        strategy->onOrderRejected(order, code, false); // false = don't retry
        
        // Specific actions based on code
        switch(code) {
            case ACCOUNT_SUSPENDED:
                haltAllStrategies();
                notifyAdmin("URGENT: Account suspended");
                break;
            
            case POSITION_LIMIT_BREACH:
                blockNewPositions();
                suggestSquareOff();
                break;
            
            case SCRIP_BLOCKED:
                blacklistSymbol(order.symbol);
                break;
        }
    }
    
    bool isCriticalRejection(RejectionCode code) {
        return code == ACCOUNT_SUSPENDED || 
               code == RMS_BROKER_SUSPENSION ||
               code == REGULATORY_BAN;
    }
};
```

---

### Strategy 4: Critical Rejection Handling

**Use Case:** Account suspension, regulatory ban, margin call

**Approach:** Immediate halt, square off positions, notify admin

```cpp
struct CriticalRejectionHandler {
    void handle(RejectionCode code, Order& order) {
        // 1. IMMEDIATE KILL SWITCH
        triggerKillSwitch("CRITICAL REJECTION: " + code);
        
        // 2. HALT ALL STRATEGIES
        for (auto strategy : activeStrategies) {
            strategy->halt("Critical rejection detected");
        }
        
        // 3. CANCEL ALL PENDING ORDERS
        cancelAllPendingOrders();
        
        // 4. DECIDE ON POSITION LIQUIDATION
        if (shouldSquareOffPositions(code)) {
            squareOffAllPositions(SquareOffMode::MARKET_URGENT);
        }
        
        // 5. SEND MULTI-CHANNEL ALERTS
        sendEmailAlert("CRITICAL", "Account/RMS issue: " + code);
        sendSMSAlert("Trading halted: " + code);
        sendWebhookAlert("critical_rejection", {
            {"code", code},
            {"message", getRejectionMessage(code)},
            {"timestamp", getCurrentTimestamp()}
        });
        
        // 6. LOG COMPREHENSIVE DETAILS
        logCriticalEvent({
            {"event", "CRITICAL_REJECTION"},
            {"code", code},
            {"order", order.toJson()},
            {"accountStatus", getAccountStatus()},
            {"openPositions", getOpenPositionsSnapshot()},
            {"availableMargin", getAvailableMargin()},
            {"pendingOrders", getPendingOrdersCount()}
        });
        
        // 7. LOCK SYSTEM FOR MANUAL INTERVENTION
        systemState = SystemState::LOCKED_REQUIRES_ADMIN;
        
        // 8. DISPLAY PROMINENT UI ALERT
        showModalAlert("CRITICAL: Trading Halted", 
                      getRejectionMessage(code) + "\n\nAdmin intervention required.");
    }
    
    bool shouldSquareOffPositions(RejectionCode code) {
        // Square off on margin call, but not on regulatory ban
        return code == MARGIN_CALL || 
               code == RMS_MARGIN_BREACH;
    }
};
```

---

### Strategy 5: Ambiguous State Handling

**Use Case:** Network timeout, uncertain if order placed

**Approach:** Query order status before any action

```cpp
struct AmbiguousRejectionHandler {
    void handle(Order& order, QString errorMessage) {
        // 1. MARK ORDER AS PENDING_VERIFICATION
        order.status = OrderStatus::PENDING_VERIFICATION;
        
        // 2. QUERY ORDER STATUS FROM BROKER
        OrderStatus actualStatus = queryOrderStatus(order.orderId);
        
        // 3. HANDLE BASED ON ACTUAL STATUS
        switch(actualStatus) {
            case OPEN:
            case PENDING:
                // Order is actually placed and pending
                order.status = OrderStatus::OPEN;
                strategy->onOrderPlaced(order);
                break;
            
            case COMPLETE:
                // Order already executed!
                order.status = OrderStatus::COMPLETE;
                strategy->onOrderFilled(order);
                break;
            
            case REJECTED:
                // Confirmed rejection, get reason
                RejectionCode code = getRejectionCode(order.orderId);
                handleRejection(code, order);
                break;
            
            case CANCELLED:
                // Order was cancelled (by user or system)
                order.status = OrderStatus::CANCELLED;
                strategy->onOrderCancelled(order);
                break;
            
            case UNKNOWN:
                // Still unknown after query
                handleUnknownState(order);
                break;
        }
    }
    
    void handleUnknownState(Order& order) {
        // Try multiple times with delay
        for (int i = 0; i < 5; i++) {
            sleep(2000); // Wait 2 seconds
            
            OrderStatus status = queryOrderStatus(order.orderId);
            if (status != UNKNOWN) {
                handle(order, "Delayed status update");
                return;
            }
        }
        
        // Still unknown after 10 seconds
        logError("Order in unknown state after 10s", order);
        sendAlert(AlertLevel::HIGH, 
                 "Order ID " + order.orderId + " in unknown state");
        
        // Conservative approach: Assume order is placed
        // (Better to manage an order than miss a filled one)
        order.status = OrderStatus::OPEN;
        order.requiresManualVerification = true;
    }
};
```

---

## State Machine Architecture

### Order State Machine

```
                    ┌──────────────────────────────────────┐
                    │         INITIAL                      │
                    │  (Order created in strategy)         │
                    └────────────┬─────────────────────────┘
                                 ↓
                    ┌────────────────────────────────────────┐
                    │      PENDING_PLACEMENT                │
                    │  (Sending to broker API)              │
                    └────────┬───────────────────────────────┘
                             ↓
              ┌──────────────┼──────────────┐
              ↓                             ↓
    ┌─────────────────────┐      ┌──────────────────────┐
    │   OPEN / PENDING    │      │     REJECTED         │
    │  (Placed success)   │      │  (See rejection tree)│
    └──────┬──────────────┘      └──────────┬───────────┘
           ↓                                 ↓
    ┌────────────────┐              ┌──────────────────────┐
    │   COMPLETE     │              │  RETRY_PENDING       │
    │   (Filled)     │              │  (Waiting for retry) │
    └────────────────┘              └──────┬───────────────┘
                                           ↓
                                    ┌──────────────────────┐
                                    │  Back to PENDING or  │
                                    │  FAILED_PERMANENT    │
                                    └──────────────────────┘
```

### Rejection Handling State Machine

```
                    ┌──────────────────────────────────────┐
                    │       REJECTION_RECEIVED             │
                    └────────────┬─────────────────────────┘
                                 ↓
                    ┌────────────────────────────────────────┐
                    │       CLASSIFYING_REJECTION           │
                    │  (Lookup rejection code, determine    │
                    │   category, severity, retryability)   │
                    └────────┬───────────────────────────────┘
                             ↓
              ┌──────────────┼──────────────────────────────┐
              ↓              ↓              ↓               ↓
    ┌─────────────┐  ┌──────────────┐  ┌────────────┐  ┌──────────┐
    │ TRANSIENT   │  │ CONDITIONAL  │  │ PERMANENT  │  │ CRITICAL │
    └──────┬──────┘  └──────┬───────┘  └─────┬──────┘  └────┬─────┘
           ↓                ↓                 ↓              ↓
    ┌──────────────┐ ┌─────────────────┐ ┌────────────┐ ┌──────────────┐
    │ RETRY_WAIT   │ │ ATTEMPTING_FIX  │ │ LOG_ALERT  │ │ KILL_SWITCH  │
    │ (Backoff)    │ │ (Modify order)  │ │ (Don't     │ │ (Halt all)   │
    └──────┬───────┘ └────────┬────────┘ │  retry)    │ └──────────────┘
           ↓                  ↓           └────────────┘
    ┌──────────────┐   ┌─────────────┐
    │ RETRY        │   │ FIX_SUCCESS?│
    │ (Attempt N)  │   └──────┬──────┘
    └──────┬───────┘          ↓
           ↓            ┌──────┴─────────┐
    ┌──────────────┐   ↓                ↓
    │ SUCCESS or   │  YES: RETRY       NO: FAILED_PERMANENT
    │ FAILED       │
    └──────────────┘
```

---

## Retry Logic

### Retry Configuration

```cpp
struct RetryConfig {
    // ═══════════════════════════════════════════════════════
    // RETRY LIMITS
    // ═══════════════════════════════════════════════════════
    
    int maxRetriesTransient = 3;      // Network/API issues
    int maxRetriesConditional = 2;    // After fixing issue
    int maxRetriesAmbiguous = 5;      // Uncertain state
    
    // ═══════════════════════════════════════════════════════
    // BACKOFF STRATEGY
    // ═══════════════════════════════════════════════════════
    
    enum BackoffStrategy {
        FIXED,              // Same delay each time
        LINEAR,             // Incremental delay (500, 1000, 1500...)
        EXPONENTIAL,        // Exponential (500, 1000, 2000, 4000...)
        FIBONACCI,          // Fibonacci (500, 500, 1000, 1500, 2500...)
        JITTERED            // Random jitter added (prevent thundering herd)
    };
    BackoffStrategy backoff = EXPONENTIAL;
    
    int initialDelayMs = 500;
    int maxDelayMs = 10000;           // Cap at 10 seconds
    double jitterPercent = 0.2;       // ±20% random jitter
    
    // ═══════════════════════════════════════════════════════
    // TIMEOUT CONFIGURATION
    // ═══════════════════════════════════════════════════════
    
    int orderPlacementTimeoutMs = 5000;  // 5 sec to place order
    int statusQueryTimeoutMs = 3000;     // 3 sec to query status
    int totalRetryTimeoutMs = 30000;     // Give up after 30 sec total
    
    // ═══════════════════════════════════════════════════════
    // CIRCUIT BREAKER (GLOBAL)
    // ═══════════════════════════════════════════════════════
    
    int maxRejectionsPerMinute = 10;     // Too many rejections = pause
    int circuitBreakerCooldownMs = 60000; // 60 sec pause
    
    // ═══════════════════════════════════════════════════════
    // SELECTIVE RETRY (BY REJECTION CODE)
    // ═══════════════════════════════════════════════════════
    
    QMap<RejectionCode, int> maxRetriesByCode = {
        {NETWORK_TIMEOUT, 5},              // More retries for network
        {RATE_LIMIT_EXCEEDED, 1},          // One retry after waiting
        {INSUFFICIENT_MARGIN, 1},          // One retry after fix
        {ACCOUNT_SUSPENDED, 0},            // Never retry
        {INVALID_INSTRUMENT, 0}            // Never retry
    };
};
```

### Retry Implementation

```cpp
class RetryManager {
private:
    RetryConfig config;
    QMap<QString, int> attemptCounts;      // orderId → attempt count
    QMap<QString, QDateTime> lastAttempts; // orderId → last attempt time
    
public:
    bool shouldRetry(const Order& order, RejectionCode code) {
        // 1. Check if rejection is retryable at all
        if (!isRetryable(code)) {
            return false;
        }
        
        // 2. Check attempt count
        int attempts = attemptCounts.value(order.orderId, 0);
        int maxRetries = getMaxRetries(code);
        
        if (attempts >= maxRetries) {
            return false;
        }
        
        // 3. Check total time elapsed
        QDateTime firstAttempt = getFirstAttemptTime(order.orderId);
        qint64 elapsedMs = firstAttempt.msecsTo(QDateTime::currentDateTime());
        
        if (elapsedMs > config.totalRetryTimeoutMs) {
            return false; // Timeout, give up
        }
        
        // 4. Check global circuit breaker
        if (isCircuitBreakerTriggered()) {
            return false;
        }
        
        return true;
    }
    
    int getRetryDelayMs(const Order& order, RejectionCode code) {
        int attempt = attemptCounts.value(order.orderId, 0);
        
        int delay = 0;
        switch(config.backoff) {
            case FIXED:
                delay = config.initialDelayMs;
                break;
            
            case LINEAR:
                delay = config.initialDelayMs * (attempt + 1);
                break;
            
            case EXPONENTIAL:
                delay = config.initialDelayMs * pow(2, attempt);
                break;
            
            case FIBONACCI:
                delay = config.initialDelayMs * fibonacci(attempt);
                break;
            
            case JITTERED:
                delay = config.initialDelayMs * pow(2, attempt);
                delay += getRandomJitter(delay, config.jitterPercent);
                break;
        }
        
        // Cap at max delay
        return std::min(delay, config.maxDelayMs);
    }
    
    void scheduleRetry(Order& order, RejectionCode code) {
        int delay = getRetryDelayMs(order, code);
        
        // Increment attempt count
        attemptCounts[order.orderId]++;
        lastAttempts[order.orderId] = QDateTime::currentDateTime();
        
        // Schedule retry
        QTimer::singleShot(delay, [this, order, code]() {
            retryOrder(order);
        });
        
        logRetryScheduled(order, code, delay, attemptCounts[order.orderId]);
    }
    
private:
    int fibonacci(int n) {
        if (n <= 1) return 1;
        int a = 1, b = 1;
        for (int i = 2; i <= n; i++) {
            int temp = a + b;
            a = b;
            b = temp;
        }
        return b;
    }
    
    int getRandomJitter(int delay, double jitterPercent) {
        int jitterRange = delay * jitterPercent;
        return (rand() % (2 * jitterRange)) - jitterRange;
    }
};
```

---

## Recovery Mechanisms

### 1. Order Status Verification

```cpp
class OrderStatusVerifier {
public:
    void verifyPendingOrders() {
        // Get all orders in uncertain state
        auto uncertainOrders = getOrdersInState({
            OrderStatus::PENDING_PLACEMENT,
            OrderStatus::PENDING_VERIFICATION,
            OrderStatus::RETRY_PENDING
        });
        
        for (auto& order : uncertainOrders) {
            // Check how long order has been in this state
            qint64 stateAge = order.lastStateChangeTime.msecsTo(
                QDateTime::currentDateTime()
            );
            
            // If > 10 seconds, force verification
            if (stateAge > 10000) {
                queryAndReconcile(order);
            }
        }
    }
    
    void queryAndReconcile(Order& order) {
        try {
            // Query broker API for actual status
            BrokerOrderStatus brokerStatus = brokerAPI->getOrderStatus(
                order.orderId
            );
            
            // Reconcile our state with broker state
            if (brokerStatus.status == "COMPLETE" && 
                order.status != OrderStatus::COMPLETE) {
                // Order filled but we didn't know!
                logWarning("Order filled but missed update: " + order.orderId);
                order.status = OrderStatus::COMPLETE;
                order.filledQty = brokerStatus.filledQty;
                order.avgPrice = brokerStatus.avgPrice;
                strategy->onOrderFilled(order);
            }
            else if (brokerStatus.status == "REJECTED" &&
                     order.status != OrderStatus::REJECTED) {
                // Order rejected but we didn't know
                logWarning("Order rejected but missed update: " + order.orderId);
                RejectionCode code = brokerStatus.rejectionCode;
                handleRejection(code, order);
            }
            else if (brokerStatus.status == "OPEN") {
                // Order is pending, update state
                order.status = OrderStatus::OPEN;
            }
        }
        catch (BrokerAPIException& e) {
            logError("Failed to verify order status", order, e);
        }
    }
};
```

### 2. Orphaned Order Detection

```cpp
class OrphanedOrderDetector {
public:
    void detectOrphans() {
        // Get all orders from broker
        auto brokerOrders = brokerAPI->getAllOrders();
        
        // Get all orders in our system
        auto systemOrders = orderManager->getAllOrders();
        
        // Find orders in broker but not in system
        for (auto& brokerOrder : brokerOrders) {
            if (!systemOrders.contains(brokerOrder.orderId)) {
                // Orphan detected!
                handleOrphanOrder(brokerOrder);
            }
        }
        
        // Find orders in system but not in broker
        for (auto& systemOrder : systemOrders) {
            if (!brokerOrders.contains(systemOrder.orderId) &&
                systemOrder.status == OrderStatus::OPEN) {
                // System thinks order is open, but broker doesn't have it
                handleGhostOrder(systemOrder);
            }
        }
    }
    
    void handleOrphanOrder(BrokerOrder& brokerOrder) {
        logWarning("Orphan order detected: " + brokerOrder.orderId);
        
        // Import into system
        Order order = convertBrokerOrderToSystem(brokerOrder);
        orderManager->addOrder(order);
        
        // Notify strategy
        strategy->onUnexpectedOrder(order);
    }
    
    void handleGhostOrder(Order& systemOrder) {
        logWarning("Ghost order detected: " + systemOrder.orderId);
        
        // Query broker one more time
        auto status = brokerAPI->getOrderStatus(systemOrder.orderId);
        
        if (status == OrderNotFound) {
            // Definitely ghost, mark as failed
            systemOrder.status = OrderStatus::FAILED_PERMANENT;
            systemOrder.rejectionReason = "Order not found in broker system";
        }
    }
};
```

### 3. Position Reconciliation

```cpp
class PositionReconciler {
public:
    void reconcilePositions() {
        // Get positions from broker
        auto brokerPositions = brokerAPI->getPositions();
        
        // Get positions from strategy
        auto strategyPositions = strategy->getPositions();
        
        // Compare
        for (auto& brokerPos : brokerPositions) {
            auto strategyPos = strategyPositions.find(brokerPos.symbol);
            
            if (strategyPos == strategyPositions.end()) {
                // Position in broker but not in strategy
                handleUnknownPosition(brokerPos);
            }
            else if (brokerPos.quantity != strategyPos->quantity) {
                // Quantity mismatch
                handleQuantityMismatch(brokerPos, *strategyPos);
            }
        }
    }
    
    void handleQuantityMismatch(BrokerPosition& broker, 
                                StrategyPosition& strategy) {
        logWarning("Position quantity mismatch: " + broker.symbol +
                  " Broker=" + QString::number(broker.quantity) +
                  " Strategy=" + QString::number(strategy.quantity));
        
        // Trust broker over strategy
        strategy.quantity = broker.quantity;
        strategy.avgPrice = broker.avgPrice;
        
        // Recalculate P&L
        strategy.updatePnL();
        
        sendAlert(AlertLevel::MEDIUM, "Position reconciliation: " + broker.symbol);
    }
};
```

---

## Logging & Alerting

### Comprehensive Rejection Logging

```cpp
struct RejectionLog {
    QDateTime timestamp;
    QString orderId;
    QString strategyId;
    QString symbol;
    int quantity;
    double price;
    QString orderType;
    
    QString rejectionCode;
    QString rejectionMessage;
    QString rejectionSource;       // "RMS", "Exchange", "Broker"
    
    RejectionCategory category;    // TRANSIENT, PERMANENT, etc.
    RejectionSeverity severity;
    
    QString handlingAction;        // "RETRY", "SKIP", "KILL_SWITCH"
    int attemptNumber;
    bool willRetry;
    int retryDelayMs;
    
    // Context
    double availableMargin;
    int openPositionsCount;
    int pendingOrdersCount;
    QString marketState;
    
    // Performance
    qint64 latencyMs;              // Time from order send to rejection
    
    void logToDatabase() {
        QString sql = R"(
            INSERT INTO rejection_log (
                timestamp, order_id, strategy_id, symbol, quantity, price,
                rejection_code, rejection_message, category, severity,
                handling_action, attempt_number, will_retry,
                available_margin, open_positions, market_state
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        db->execute(sql, {
            timestamp, orderId, strategyId, symbol, quantity, price,
            rejectionCode, rejectionMessage, category, severity,
            handlingAction, attemptNumber, willRetry,
            availableMargin, openPositionsCount, marketState
        });
    }
    
    void logToFile() {
        QString logLine = QString("[%1] REJECTION | Order=%2 | Symbol=%3 | "
                                 "Code=%4 | Msg=%5 | Action=%6 | Retry=%7")
            .arg(timestamp.toString("yyyy-MM-dd HH:mm:ss.zzz"))
            .arg(orderId)
            .arg(symbol)
            .arg(rejectionCode)
            .arg(rejectionMessage)
            .arg(handlingAction)
            .arg(willRetry ? "YES" : "NO");
        
        logger->log(LogLevel::ERROR, logLine);
    }
};
```

### Alert Configuration

```cpp
struct AlertConfig {
    // Alert channels
    bool emailAlerts = true;
    bool smsAlerts = true;
    bool webhookAlerts = true;
    bool uiPopupAlerts = true;
    
    // Alert thresholds
    struct AlertRule {
        RejectionSeverity minSeverity = MEDIUM;
        int consecutiveRejectionsThreshold = 3;
        int rejectionsPerMinuteThreshold = 5;
        QStringList criticalCodes = {
            "ACCOUNT_SUSPENDED",
            "MARGIN_CALL",
            "REGULATORY_BAN"
        };
    };
    AlertRule rules;
    
    // Alert rate limiting (prevent spam)
    int minMinutesBetweenSimilarAlerts = 5;
    int maxAlertsPerHour = 20;
};
```

---

## Code Implementation

### Complete Rejection Handler Class

```cpp
class RejectionHandler {
private:
    RetryConfig retryConfig;
    RetryManager retryManager;
    OrderStatusVerifier statusVerifier;
    OrphanedOrderDetector orphanDetector;
    PositionReconciler positionReconciler;
    AlertManager alertManager;
    
public:
    void handleRejection(Order& order, const QString& rejectionMsg) {
        // 1. PARSE REJECTION
        RejectionInfo info = parseRejection(rejectionMsg);
        
        // 2. LOG REJECTION
        RejectionLog log = createRejectionLog(order, info);
        log.logToDatabase();
        log.logToFile();
        
        // 3. CLASSIFY REJECTION
        RejectionCategory category = classifyRejection(info.code);
        RejectionSeverity severity = getSeverity(info.code);
        
        // 4. UPDATE ORDER STATE
        order.status = OrderStatus::REJECTED;
        order.rejectionCode = info.code;
        order.rejectionReason = info.message;
        order.rejectionCategory = category;
        
        // 5. HANDLE BY CATEGORY
        switch(category) {
            case TRANS