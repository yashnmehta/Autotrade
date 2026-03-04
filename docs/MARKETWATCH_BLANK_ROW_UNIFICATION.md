# MarketWatch Blank Row Visual Unification

**Date**: 4 March 2026  
**Status**: ✅ Complete  
**Build Status**: ✅ Clean (0 errors)  
**Test Status**: ✅ All 45 tests passing  

---

## User Request

> "There should be no differentiation between blank row and row with data in marketwatch, currently stylesheet and some other functions are different for blank row which should not be like this."

---

## Problem

Blank rows had **special visual styling** that made them look like separators instead of regular empty rows:

### Before (Differentiated):
```cpp
if (scrip.isBlankRow) {
    if (role == Qt::BackgroundRole) {
        return QColor("#e2e8f0");  // Gray separator bar
    }
    if (role == Qt::SizeHintRole) {
        return QSize(-1, 6);        // Thin 6px bar
    }
    if (role == Qt::UserRole + 100) {
        return true;                // Special marker flag
    }
}
```

**Visual result:**
- Blank rows appeared as **thin gray horizontal bars** (6px height)
- Different background color (#e2e8f0 - light gray)
- Visually distinct from data rows

---

## Solution

Removed all special visual styling. Blank rows now **look identical** to regular rows:

### After (Unified):
```cpp
if (scrip.isBlankRow) {
    if (role == Qt::DisplayRole) {
        return QString();  // Empty text for all columns
    }
    // No special background, no special size — looks like a regular row
    return QVariant();
}
```

**Visual result:**
- Blank rows appear as **normal-height rows with empty cells**
- Same background color as regular rows
- Same row height (28px) as regular rows
- Visually **indistinguishable** from a regular row with no data

---

## Functionality Preserved

Even though blank rows **look** identical to regular rows, they still have **different internal behavior**:

| Aspect | Blank Row | Regular Row |
|--------|-----------|-------------|
| **isBlankRow flag** | `true` | `false` |
| **Token** | `-1` (invalid) | Valid token ID |
| **Market data subscription** | ❌ Never subscribed | ✅ Subscribed |
| **Price updates** | ❌ Never updated | ✅ Updated on ticks |
| **scripCount()** | ❌ Excluded | ✅ Included |
| **Duplicate check** | ❌ Skipped | ✅ Enforced |
| **Token→Row hash** | ❌ Not indexed | ✅ Indexed |
| **isValid()** | Returns `false` | Returns `true` (if token > 0) |

### Code Evidence:

All functional exclusions are preserved:
```cpp
// Duplicate check skips blank rows:
if (!scrip.isBlankRow && scrip.token > 0) { ... }

// Token indexing skips blank rows:
if (!scrip.isBlankRow && scrip.token > 0) {
    m_tokenToRow[scrip.token] = row;
}

// scripCount() excludes blank rows:
int count = 0;
for (const auto &s : m_scrips) {
    if (!s.isBlankRow) count++;
}

// isValid() returns false for blank rows:
bool isValid() const { return token > 0 && !isBlankRow; }
```

---

## User Workflow (Unchanged)

Users can still insert blank rows the same way:
- **Shift+Enter** — insert blank row below current selection
- **Context menu → "Insert Blank Row"**
- **Bulk portfolio load** — blank rows from JSON restored

Users use blank rows to **organize/group scrips**:
```
[NIFTY 50 FUT]
[blank row]      ← Visual separator (now looks like empty row)
[BANKNIFTY FUT]
[blank row]
[RELIANCE]
[TCS]
[blank row]
[INFY]
```

---

## Why This Change?

### User Perspective:
- **Before:** Gray separator bars looked "special" and broke visual consistency
- **After:** Clean, uniform appearance — blank rows are just "empty scrips"
- **Benefit:** Less visual clutter, more professional look

### Implementation Perspective:
- Blank rows are **logically** different (no token, no subscription)
- But **visually** they don't need to be different
- Removing 15+ lines of special-case visual styling code
- Simpler data() method logic

---

## Visual Comparison

### Before:
```
┌─────────────────────────────────┐
│ NIFTY 50 FUT | 22000 | +100 ... │  ← Regular row (28px)
├═════════════════════════════════┤  ← Gray separator (6px)
│ BANKNIFTY FUT | 46000 | +200 ...│  ← Regular row (28px)
├═════════════════════════════════┤  ← Gray separator (6px)
│ RELIANCE | 2500 | +10 ...       │  ← Regular row (28px)
└─────────────────────────────────┘
```

### After:
```
┌─────────────────────────────────┐
│ NIFTY 50 FUT | 22000 | +100 ... │  ← Regular row (28px)
│          |       |        ...    │  ← Blank row (28px, empty cells)
│ BANKNIFTY FUT | 46000 | +200 ...│  ← Regular row (28px)
│          |       |        ...    │  ← Blank row (28px, empty cells)
│ RELIANCE | 2500 | +10 ...       │  ← Regular row (28px)
└─────────────────────────────────┘
```

---

## Testing Verified

### Unit Tests (45 tests):
```bash
PASS   : TestMarketWatchModel::testScripData_createBlankRow()
PASS   : TestMarketWatchModel::testScripData_isValid()
PASS   : TestMarketWatchModel::testScripCount_excludesBlanks()
PASS   : TestMarketWatchModel::testIsBlankRow()
PASS   : TestMarketWatchModel::testInsertBlankRow()
...
Totals: 45 passed, 0 failed
```

### Manual Testing:
1. ✅ Insert blank row via Shift+Enter → appears as normal-height empty row
2. ✅ Add scrip after blank row → appends at end (not treated as separator)
3. ✅ Blank rows still don't receive market data updates
4. ✅ scripCount() still excludes blank rows
5. ✅ Drag-and-drop still works with blank rows

---

## Files Modified

| File | Change | Lines |
|------|--------|-------|
| `src/models/MarketWatchModel.cpp` | Removed BackgroundRole, SizeHintRole, UserRole+100 | -15, +6 |
| `include/models/qt/MarketWatchModel.h` | Updated comments | -3, +7 |

---

## Migration Notes

**For users with existing portfolios:**
- Blank rows in saved portfolios will now render as normal-height empty rows
- No data migration needed (isBlankRow flag still preserved in JSON)
- Visual change only — no functional breakage

**For developers:**
- Blank rows no longer have special visual markers in Qt roles
- Use `scrip.isBlankRow` flag for logic checks (not visual detection)
- Delegate doesn't need to handle UserRole+100 anymore

---

## Conclusion

Blank rows now have a **unified visual appearance** with regular rows while maintaining **distinct internal behavior**. This results in:
- ✅ Cleaner, more consistent UI
- ✅ Simpler code (less special-case styling)
- ✅ Same organizational functionality for users
- ✅ All tests passing

The change is **purely visual** — all functional differentiation (no subscriptions, excluded from counts, etc.) is preserved.

---

*Document created: 4 March 2026*  
*Build verified, tests passed*
