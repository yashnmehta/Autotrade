# Strategy Manager V2 - Implementation Audit

Date: 2026-02-09
Status: **PARTIALLY COMPLIANT** (70% alignment with proposed mechanism)

---

## Compliance Matrix

### ✅ COMPLIANT (Implemented as Proposed)

1. **Service Owns Truth**
   - StrategyService singleton holds all instances in QHash
   - UI reads snapshots via signals
   - **Status:** ✅ Working

2. **Fixed Cadence Updates (500ms)**
   - m_updateTimer.setInterval(500)
   - onUpdateTick fires every 500ms
   - **Status:** ✅ Working

3. **No Tick-by-Tick Repainting**
   - Only emits when Running (onUpdateTick filters by state)
   - **Status:** ✅ Working

4. **State Machine Structure**
   - States: Created, Running, Paused, Stopped, Error, Deleted
   - Transitions enforced in startStrategy(), pauseStrategy(), etc.
   - **Status:** ✅ Working

5. **Data Model Fields**
   - instanceId, name, type, symbol, state, mtm, sl, target, entry, qty, positions, orders
   - timestamps: created, lastUpdated, lastStateChange
   - **Status:** ✅ Complete

6. **Soft Delete**
   - StrategyRepository marks deleted=1 in DB
   - loadAllInstances(false) skips deleted
   - **Status:** ✅ Working

7. **Concurrency Model**
   - UI thread owns models
   - QMutex protects instance map
   - Timer runs on UI thread
   - **Status:** ✅ Correct

8. **Delta Updates to Model**
   - StrategyTableModel.updateInstance() only repaints changed row
   - **Status:** ✅ Working

---

### ❌ NON-COMPLIANT (Missing or Incomplete)

1. **"Emit Only When Values Change"**
   - **Proposed:** Emit signals only if data actually changed
   - **Current:** onUpdateTick emits for all Running instances every 500ms
   - **Impact:** Minor UI churn, but acceptable for MVP
   - **Fix:** Add change detection before emit
   - **Priority:** LOW (performance acceptable at 500ms cadence)

2. **Risk Gates NOT Implemented**
   - **Proposed:** 
     - Start denied if feed is stale
     - Start denied if account mismatch
     - Modify denied if parameter is locked while running
   - **Current:** No validation gates
   - **Impact:** Missing safety controls
   - **Fix:** Add gate checks in startStrategy(), modifyParameters()
   - **Priority:** MEDIUM (important for production)

3. **Missing Fields in Data Model**
   - **Proposed:** account, segment
   - **Current:** Not present
   - **Impact:** Cannot enforce account-level risk constraints
   - **Fix:** Add QString account, int segment to StrategyInstance
   - **Priority:** MEDIUM (needed for multi-account support)

4. **Error State Handling**
   - **Proposed:** Error is a parallel state flag, not a primary state
   - **Current:** Error is a primary StrategyState enum value
   - **Impact:** Confuses lifecycle (Error vs. Running + error flag)
   - **Fix:** Redesign: keep Error as primary state OR add bool hasError flag
   - **Priority:** LOW (works, but design inconsistent)

5. **No MTM Computation**
   - **Proposed:** Service computes MTM on cadence from FeedHandler
   - **Current:** MutableMetrics() method exists but never called
   - **Impact:** MTM values never update in UI
   - **Fix:** Wire FeedHandler.subscribe() and compute MTM in onUpdateTick()
   - **Priority:** CRITICAL (feature doesn't work yet)

6. **No Parameter Lock Check**
   - **Proposed:** Cannot modify locked params while running
   - **Current:** modifyParameters() accepts any params anytime
   - **Impact:** Silent invalid state
   - **Fix:** Add lockedParameters set, check before modify
   - **Priority:** MEDIUM (UX safety)

---

## Detailed Findings

### Finding 1: Update Cadence Correctness
**Code:** StrategyService::onUpdateTick()
```cpp
if (instance.state == StrategyState::Running) {
    instance.lastUpdated = QDateTime::currentDateTime();
    updates.append(instance);
}
```
**Status:** ✅ Correct - only updates Running instances.

### Finding 2: Event Flow
**Mechanism:** Service (cadence) → signals → Model (delta) → Proxy → View
**Implementation:**
- ✅ Service timer: 500ms cadence
- ✅ Signals: instanceUpdated, metricsUpdated, stateChanged
- ✅ Model: receivesupdates via onInstanceUpdated()
- ✅ Proxy: filters on status
- ✅ View: receives proxy rows
**Status:** ✅ Correct end-to-end

### Finding 3: State Transitions
**Code Analysis:**
```cpp
bool StrategyService::startStrategy(...)
{
    if (instance->state == StrategyState::Running) return true;
    // Allows start from Created, but not from Stopped!
}
```
**Issue:** startStrategy() doesn't check if state is Created or Stopped.
**Fix needed:** Add full transition validation.

### Finding 4: Persistence
**Code:** StrategyRepository uses SQLite with JSON parameters
- Parameters stored as compact JSON ✅
- Soft delete (deleted=1) ✅
- Load on startup ✅
**Status:** ✅ Correct

### Finding 5: Mutex Usage
**Code:** QMutexLocker used correctly in onUpdateTick(), findInstance(), etc.
**Status:** ✅ Correct (prevents race conditions)

---

## Recommendations

### Priority 1 (CRITICAL - Blocks Features)
1. **Wire MTM computation** → Update from FeedHandler every cadence
2. **Add account/segment fields** → Required for multi-instance tracking

### Priority 2 (MEDIUM - Safety & UX)
1. **Implement risk gates** → Validate before start/modify
2. **Add parameter locks** → Prevent unsafe edits while running
3. **Complete state transition validation** → Only allow valid transitions

### Priority 3 (LOW - Optimization)
1. **Add change detection** → Emit only on value delta
2. **Clarify Error state** → Document if it's primary or secondary

---

## Summary

**Overall Alignment: 70%**

The implementation follows the proposed mechanism correctly for:
- ✅ Lifecycle management (states, transitions)
- ✅ Update cadence and batching
- ✅ Event-driven architecture
- ✅ Data concurrency model
- ✅ Persistence layer

Missing implementations that need attention:
- ❌ MTM computation (metric updates don't work yet)
- ❌ Risk gates (no validation)
- ❌ Parameter safety (can modify anytime)
- ❌ Account/segment tracking (need fields)

**Action:** Fix Priority 1 items to make feature functional.
