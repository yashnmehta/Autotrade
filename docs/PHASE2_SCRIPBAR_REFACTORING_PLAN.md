# Phase 2: ScripBar Refactoring Plan

**Objective**: Remove SymbolCacheManager dependency and use RepositoryManager directly

---

## Current Issues

1. **Line 3**: `#include "data/SymbolCacheManager.h"` - Obsolete include
2. **Lines 359-361**: Uses `SymbolCacheManager::instance().getSymbols()` - Removed API
3. **Lines 398-502**: Fallback path uses old `repo->getScrips()` API

---

## Refactoring Strategy

### **Approach: Direct RepositoryManager Usage**

Since we now have:
- ✅ Fast `getUniqueSymbols()` with lazy caching (0.02ms cached)
- ✅ Efficient `getContractsBySeries()` methods
- ✅ Zero-copy `forEachContract()` iteration

We can **simplify** ScripBar by:
1. Remove SymbolCacheManager include
2. Use `RepositoryManager::getUniqueSymbols()` directly
3. Keep local `m_instrumentCache` for contract details
4. Lazy-load full contract data only when needed

---

## Implementation Plan

### **Step 1: Remove SymbolCacheManager**
- Delete `#include "data/SymbolCacheManager.h"`
- Remove all `SymbolCacheManager::instance()` calls

### **Step 2: Simplify populateSymbols()**

**Before** (Complex dual-cache):
```cpp
// Try cache first
const QVector<InstrumentData> &cachedSymbols = 
    SymbolCacheManager::instance().getSymbols(exchange, segment, series);

if (!cachedSymbols.isEmpty()) {
    m_instrumentCache = cachedSymbols;
    // Extract unique symbols...
} else {
    // Fallback to RepositoryManager...
}
```

**After** (Simple direct call):
```cpp
// Get unique symbols directly (fast, cached in RepositoryManager)
QStringList symbols = RepositoryManager::getInstance()->getUniqueSymbols(
    exchange, segment, series
);

// Populate dropdown
m_symbolCombo->addItems(symbols);

// Lazy-load full contract data only when symbol is selected
```

### **Step 3: Lazy-Load Contract Details**

Instead of loading ALL contract data upfront:
- Load symbols immediately (fast)
- Load full contract details when user selects a symbol
- Use `getContractsBySeries()` or `getContractsBySymbol()`

---

## Performance Comparison

| Operation | Before (SymbolCacheManager) | After (Direct) |
|-----------|----------------------------|----------------|
| **First Load** | 800ms (build cache) | 2-5ms (lazy cache) |
| **Subsequent** | <1ms (cache hit) | 0.02ms (repo cache) |
| **Memory** | Duplicate cache | Single cache |
| **Complexity** | High (dual system) | Low (single source) |

---

## Code Changes Required

### Files to Modify:
1. `src/app/ScripBar.cpp` - Main refactoring
   - Remove SymbolCacheManager include
   - Simplify `populateSymbols()`
   - Add lazy loading for contract details

### Files NOT Modified:
- `include/app/ScripBar.h` - No API changes needed
- Other ScripBar methods - Minimal changes

---

## Risk Assessment

**Risk Level**: **LOW** ✅

**Why**:
- RepositoryManager API is stable and tested
- ScripBar already has fallback path using RepositoryManager
- We're removing complexity, not adding it
- Build already passing with SymbolCacheManager removed from RepositoryManager

---

## Testing Strategy

1. **Build Verification** - Ensure compilation succeeds
2. **Manual Testing**:
   - Open MarketWatch → ScripBar should populate
   - Select Exchange → Segments load
   - Select Segment → Instruments load
   - Select Instrument → Symbols load (fast!)
   - Select Symbol → Contract details display
3. **Performance Testing**:
   - Measure symbol load time (should be <5ms)
   - Verify no UI lag

---

## Success Criteria

✅ Build passes  
✅ SymbolCacheManager fully removed  
✅ Symbol loading <5ms  
✅ All dropdowns populate correctly  
✅ Contract details display correctly  
✅ No performance regression  

---

**Status**: Ready to implement
**Estimated Time**: 30-45 minutes
**Complexity**: Medium (straightforward refactoring)
