# ScripBar Helper Functions - Architecture Recommendation

**Date**: 2026-02-06  
**Context**: ScripBar contains helper functions that manipulate repository data  
**Question**: Should these be in a separate file or moved to Repository layer?

---

## Current Helper Functions in ScripBar

### **1. `mapInstrumentToSeries()`**
**Purpose**: Maps UI instrument names to repository series codes  
**Example**:
```cpp
QString ScripBar::mapInstrumentToSeries(const QString &instrument) const {
  if (instrument == "EQUITY") return "";  // All equity series
  if (m_currentExchange == "BSE") {
    if (instrument == "FUTIDX") return "IF";  // BSE Index Futures
    if (instrument == "OPTIDX") return "IO";  // BSE Index Options
  }
  return instrument;  // NSE: direct mapping
}
```

**Current Issues**:
- ❌ Business logic in UI layer
- ❌ Hardcoded exchange-specific mappings
- ❌ Duplicated if used elsewhere

---

### **2. `getCurrentExchangeSegmentCode()`**
**Purpose**: Maps exchange+segment to XTS segment codes  
**Example**:
```cpp
int ScripBar::getCurrentExchangeSegmentCode() const {
  if (exchange == "NSE" && segment == "E") return 1;   // NSECM
  if (exchange == "NSE" && segment == "F") return 2;   // NSEFO
  if (exchange == "BSE" && segment == "E") return 11;  // BSECM
  // ... etc
}
```

**Current Issues**:
- ❌ Domain knowledge in UI layer
- ❌ Already exists in RepositoryManager!
- ❌ Duplicate code

---

## Analysis: Where Should These Live?

### **Option 1: Keep in ScripBar** ❌
**Pros**:
- No refactoring needed
- Self-contained

**Cons**:
- ❌ Violates separation of concerns
- ❌ Business logic in UI layer
- ❌ Hard to test
- ❌ Can't reuse in other UI components
- ❌ Duplicates RepositoryManager logic

**Verdict**: ❌ **NOT RECOMMENDED**

---

### **Option 2: Move to Separate Utility File** ⚠️
**Pros**:
- Reusable across UI components
- Testable in isolation
- Clean separation

**Cons**:
- ⚠️ Still UI-centric (not domain logic)
- ⚠️ Creates another layer
- ⚠️ Duplicates RepositoryManager knowledge

**Verdict**: ⚠️ **ACCEPTABLE but not optimal**

---

### **Option 3: Move to RepositoryManager** ✅
**Pros**:
- ✅ Domain logic in domain layer
- ✅ Single source of truth
- ✅ Already has `getExchangeSegmentID()` (duplicate!)
- ✅ Reusable by any component
- ✅ Easier to test
- ✅ Consistent with architecture

**Cons**:
- Requires refactoring (minimal)

**Verdict**: ✅ **STRONGLY RECOMMENDED**

---

## Recommended Solution

### **Strategy: Move to RepositoryManager + Create UI Helper**

#### **Step 1: Enhance RepositoryManager**

Add these methods to `RepositoryManager`:

```cpp
class RepositoryManager {
public:
  // Already exists - keep it!
  static int getExchangeSegmentID(const QString &exchange, const QString &segment);
  
  // NEW: Map instrument type to series filter
  static QString mapInstrumentToSeries(
      const QString &exchange,
      const QString &instrument
  );
  
  // NEW: Get valid instruments for exchange+segment
  static QStringList getValidInstruments(
      const QString &exchange,
      const QString &segment
  );
};
```

**Implementation**:
```cpp
QString RepositoryManager::mapInstrumentToSeries(
    const QString &exchange,
    const QString &instrument) {
  
  if (instrument == "EQUITY") {
    return "";  // All equity series
  }
  
  // BSE-specific mappings
  if (exchange == "BSE") {
    if (instrument == "FUTIDX") return "IF";
    if (instrument == "OPTIDX") return "IO";
    if (instrument == "FUTSTK") return "SF";
    if (instrument == "OPTSTK") return "SO";
  }
  
  // NSE: direct mapping (FUTIDX → FUTIDX, etc.)
  return instrument;
}
```

---

#### **Step 2: Simplify ScripBar**

**Before**:
```cpp
QString ScripBar::mapInstrumentToSeries(const QString &instrument) const {
  // 37 lines of hardcoded logic
}

int ScripBar::getCurrentExchangeSegmentCode() const {
  // 37 lines of hardcoded logic
}
```

**After**:
```cpp
// Use RepositoryManager directly
QString exchange = getCurrentExchange();
QString segment = getCurrentSegment();

QString series = RepositoryManager::mapInstrumentToSeries(exchange, instrument);
int segmentCode = RepositoryManager::getExchangeSegmentID(exchange, segment);
```

**Lines Removed**: ~74 lines from ScripBar!

---

## Architecture Comparison

### **Before** (Current):
```
┌─────────────┐
│  ScripBar   │ ← Business logic (mapping, codes)
│   (UI)      │ ← Domain knowledge
└──────┬──────┘
       │
       ↓
┌─────────────────┐
│ RepositoryMgr   │ ← Also has some mappings (duplicate!)
│   (Domain)      │
└─────────────────┘
```

**Problems**:
- Business logic in UI
- Duplicate knowledge
- Hard to maintain

---

### **After** (Recommended):
```
┌─────────────┐
│  ScripBar   │ ← Pure UI logic only
│   (UI)      │ ← Calls RepositoryManager
└──────┬──────┘
       │
       ↓
┌─────────────────┐
│ RepositoryMgr   │ ← All domain logic
│   (Domain)      │ ← Single source of truth
└─────────────────┘
```

**Benefits**:
- Clean separation
- No duplication
- Easy to maintain

---

## Implementation Plan

### **Phase 1: Add to RepositoryManager** (15 min)

1. Add `mapInstrumentToSeries()` to `RepositoryManager.h`
2. Implement in `RepositoryManager.cpp`
3. Add `getValidInstruments()` (optional, for future)

### **Phase 2: Refactor ScripBar** (10 min)

1. Replace `ScripBar::mapInstrumentToSeries()` with `RepositoryManager::mapInstrumentToSeries()`
2. Remove `ScripBar::getCurrentExchangeSegmentCode()` (use existing `RepositoryManager::getExchangeSegmentID()`)
3. Update all call sites

### **Phase 3: Test** (5 min)

1. Build and verify
2. Test ScripBar functionality
3. Verify mappings work correctly

**Total Time**: ~30 minutes

---

## Benefits Summary

| Aspect | Before | After | Improvement |
|--------|--------|-------|-------------|
| **Separation of Concerns** | ❌ Mixed | ✅ Clean | Better architecture |
| **Code Duplication** | ❌ Yes | ✅ No | DRY principle |
| **Testability** | ⚠️ Hard | ✅ Easy | Better quality |
| **Reusability** | ❌ No | ✅ Yes | Other UIs can use |
| **Maintainability** | ⚠️ Medium | ✅ High | Single source |
| **Lines of Code** | 1177 | ~1103 | -74 lines |

---

## Alternative: Create ScripBarHelper Utility

If you prefer **not** to add to RepositoryManager, create a separate utility:

### **File**: `include/utils/ExchangeMapper.h`

```cpp
class ExchangeMapper {
public:
  // Map instrument to series
  static QString mapInstrumentToSeries(
      const QString &exchange,
      const QString &instrument
  );
  
  // Get segment code (delegates to RepositoryManager)
  static int getSegmentCode(
      const QString &exchange,
      const QString &segment
  ) {
    return RepositoryManager::getExchangeSegmentID(exchange, segment);
  }
  
  // Get valid instruments for segment
  static QStringList getValidInstruments(
      const QString &exchange,
      const QString &segment
  );
};
```

**Pros**:
- Keeps RepositoryManager focused
- Dedicated utility for UI mappings
- Still reusable

**Cons**:
- Another file to maintain
- Still somewhat duplicates RepositoryManager knowledge

---

## Final Recommendation

### ✅ **RECOMMENDED: Option 3 - Move to RepositoryManager**

**Rationale**:
1. **Domain Logic Belongs in Domain Layer**: Exchange/segment/series mappings are domain knowledge, not UI logic
2. **Already Partially There**: `getExchangeSegmentID()` already exists in RepositoryManager
3. **Single Source of Truth**: Eliminates duplication
4. **Better Architecture**: Clean separation of concerns
5. **Easier to Test**: Domain logic testable without UI
6. **Reusable**: Any component can use these mappings

**Next Steps**:
1. Add `mapInstrumentToSeries()` to RepositoryManager
2. Refactor ScripBar to use it
3. Remove duplicate code
4. Test and verify

**Estimated Effort**: 30 minutes  
**Benefit**: Cleaner architecture, -74 lines, better maintainability

---

## Summary

| Option | Recommendation | Reason |
|--------|---------------|---------|
| **Keep in ScripBar** | ❌ No | Business logic in UI |
| **Separate Utility** | ⚠️ OK | Acceptable but not optimal |
| **Move to RepositoryManager** | ✅ **YES** | Best architecture |

**Verdict**: **Move to RepositoryManager** for clean, maintainable, reusable code.

Would you like me to implement this refactoring?
