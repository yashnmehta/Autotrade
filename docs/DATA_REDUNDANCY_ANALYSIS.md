# Redundant Data Structures Analysis & Optimization Plan

## Problem Statement

The application currently duplicates specific master data across three layers, leading to ~3x memory overhead for instrument data:

1. **Repository Layer** (`NSEFORepository`, etc.): Stores the master `ContractData` (Single Source of Truth).
2. **Caching Layer** (`SymbolCacheManager`): duplicates data into `QVector<InstrumentData>` for UI consumption.
3. **UI Layer** (`ScripBar`): Copies `InstrumentData` again into its own local `m_instrumentCache`.

## Analysis of Structures

| Struct | Size | Fields | Usage |
|--------|------|--------|-------|
| `ContractData` | ~200B | Full contract details (Strings, Greeks, Limits) | Repository storage |
| `InstrumentData` | ~100B | UI subset (Name, Token, Strings) | ScripBar & Cache |

While `InstrumentData` is smaller, constructing 9000+ instances of it (for NSE CM) creates significant heap fragmentation due to `QString` allocations (even if implicit sharing usage is high, the objects themselves are distinct).

## Optimization Strategy: Zero-Copy / On-Demand

We can eliminate layers 2 and 3 by leveraging the existing `NSEFORepositoryPreSorted` capabilities and extending `NSECMRepository`.

### 1. Remove `SymbolCacheManager`
**Status**: 🔴 Redundant
**Action**: Delete entirely.
**Reasoning**: It serves only to transform `ContractData` -> `InstrumentData` globally. This transformation can happen *locally* and *on-demand* in ScripBar for only the selected symbol (e.g., 50 items vs 9000).

### 2. Extend Repository Interface
Add a lightweight API to get unique symbols without fetching full contract objects.

**New API**:
```cpp
// In RepositoryManager / Repositories
QStringList getUniqueSymbols(const QString& exchange, const QString& segment);
```

**Implementation details**:
- **NSEFO (PreSorted)**: The file `src/repository/NSEFORepositoryPreSorted.cpp` confirms `m_symbolIndex` (Hash<QString, Vector<int64>>) is already built. We can simply return `m_symbolIndex.keys()`. This is an O(N) operation where N is the number of *unique symbols* (not contracts), which is very fast.
- **NSECM**: Iterate `m_name` once or cache a unique set. (O(N) linear scan is fast for 2500 items, <1ms).

### 3. Refactor ScripBar (The "Lazy" Approach)

Instead of populating everything at start:

**Current Flow (Expensive):**
```
1. populateSymbols() 
   -> Calls SymbolCacheManager 
   -> Gets 9000 InstrumentData items (Copy #1)
   -> Iterates 9000 items to find unique symbols
   -> ScripBar stores 9000 items in m_instrumentCache (Copy #2)
```

**Optimized Flow (Efficient):**
```
1. populateSymbols()
   -> Calls repo->getUniqueSymbols()
   -> Gets ~1500 strings (for NSECM) or ~200 (for NSEFO)
   -> Populates ComboBox directly.
   -> m_instrumentCache is EMPTY.

2. On User Selects "RELIANCE":
   -> Calls repo->getContractsBySymbol("RELIANCE")
   -> Gets ~50 ContractData items (Repository is fast)
   -> ScripBar converts these 50 items to InstrumentData for UI
   -> m_instrumentCache stores only these 50 items.
```

### 4. Memory Impact Analysis

| Metric | Current | Optimized | Reduction |
|--------|---------|-----------|-----------|
| `ScripBar` Cache | ~9000 items (for NSECM) | ~1-100 items (Active Symbol) | **99%** |
| `SymbolCacheManager` | Global Cache (~20k items) | **0 items** (Removed) | **100%** |
| `Repository` | Master Data | Master Data | 0% (Required) |
| **Startup Time** | ~100-200ms per segment | **< 1ms** | **99%** |

## Implementation Steps

1.  **Repository Update**:
    - Add `getUniqueSymbols()` to `NSEFORepository` and `NSECMRepository`.
    - `NSEFORepositoryPreSorted`: Return keys from `m_symbolIndex`.
    - `NSECMRepository`: Return unique values from `m_name`.

2.  **ScripBar Update**:
    - Modify `populateSymbols` to use `getUniqueSymbols`.
    - Modify `onSymbolChanged` to fetch contracts for *only* that symbol via `getContractsBySymbol`.
    - Remove dependency on `SymbolCacheManager`.

3.  **Cleanup**:
    - Delete `src/data/SymbolCacheManager.cpp`.
    - Delete `include/data/SymbolCacheManager.h`.

## Conclusion

This approach removes the redundancy completely. `ScripBar` becomes a lightweight view over the Repository. The memory footprint for an open ScripBar drops from megabytes to kilobytes, and application startup becomes significantly faster.
