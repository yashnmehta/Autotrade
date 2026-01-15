# Portfolio & Column Profile Save/Load - Critical Bugs Analysis

**Date:** January 14, 2026  
**Component:** Market Watch Portfolio & Column Profile Management  
**Severity:** HIGH - Data Loss & Inconsistent Behavior

---

## 🔴 Executive Summary

The portfolio and column profile save/load system contains **multiple critical bugs** causing unpredictable column order changes, data inconsistencies, and race conditions. This document identifies 12+ major bugs affecting:

1. **Column Order Preservation** - Not maintained during save/load
2. **Profile State Synchronization** - Model vs View mismatch
3. **Serialization Issues** - Missing or incorrect data during JSON conversion
4. **Race Conditions** - Competing updates during profile changes

---

## 🐛 Critical Bugs Identified

### **BUG #1: Column Order Lost During View Capture** ⚠️⚠️⚠️
**File:** `src/views/MarketWatchWindow/Actions.cpp:470-507`  
**Severity:** CRITICAL

**Problem:**
The `captureProfileFromView()` function attempts to capture column order from the QHeaderView's visual indices, but fails because:

```cpp
// Line 474-475: Captures visual order
for (int visualIdx = 0; visualIdx < header->count(); ++visualIdx) {
    int logicalIdx = header->logicalIndex(visualIdx);
```

**Issues:**
1. **NO USER REORDERING SUPPORT**: The code assumes `header->logicalIndex(visualIdx)` returns meaningful reordering, but **QHeaderView column reordering is never enabled** in MarketWatchWindow!
   - Looking at `CustomMarketWatch.cpp`, there's NO call to `setSectionsMovable(true)`
   - Visual index always equals logical index (no actual reordering possible)
   - The captured order is ALWAYS the model's internal order, not user-arranged order

2. **WRONG MAPPING**: Maps visual→logical→modelColumns incorrectly
   - Assumes `modelColumns[logicalIdx]` gives the right column
   - But if model's column order differs from view order, this breaks

3. **HIDDEN COLUMNS MISHANDLED**: Hidden columns are appended at the end, disrupting original order

**Impact:**
- Column order appears random after save/load
- User cannot rearrange columns meaningfully
- Order changes unpredictably

**Root Cause:**
The system assumes QHeaderView manages visual column order, but this feature is **never activated**.

---

### **BUG #2: Model Reset Destroys View State** ⚠️⚠️⚠️
**File:** `src/models/MarketWatchModel.cpp:448-453`  
**Severity:** CRITICAL

**Problem:**
```cpp
void MarketWatchModel::setColumnProfile(const MarketWatchColumnProfile& profile)
{
    beginResetModel();
    m_columnProfile = profile;
    endResetModel();
    qDebug() << "[MarketWatchModel] Column profile updated:" << profile.name();
}
```

**Issues:**
1. **NUCLEAR RESET**: `beginResetModel()/endResetModel()` destroys ALL view state:
   - Column widths reset to defaults
   - Selection lost
   - Scroll position reset
   - Sort order lost
   - Any pending edits discarded

2. **NO PARTIAL UPDATE**: Qt provides `dataChanged()`, `headerDataChanged()`, and `layoutChanged()` for surgical updates, but this code uses a sledgehammer approach

3. **VIEW DESYNC**: After reset, view must re-read all data, causing:
   - UI flicker
   - Performance hit
   - Lost user context

**Impact:**
- User loses their current view state every time profile changes
- Column widths don't persist as expected
- Poor user experience

**Correct Approach:**
```cpp
void MarketWatchModel::setColumnProfile(const MarketWatchColumnProfile& profile)
{
    // Check what actually changed
    bool orderChanged = (m_columnProfile.columnOrder() != profile.columnOrder());
    bool visibilityChanged = (m_columnProfile.visibleColumns() != profile.visibleColumns());
    
    m_columnProfile = profile;
    
    if (orderChanged || visibilityChanged) {
        emit layoutAboutToBeChanged();
        // ... update persistent indexes ...
        emit layoutChanged();
    } else {
        // Just widths/metadata changed
        emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
    }
}
```

---

### **BUG #3: Save Portfolio Does NOT Capture Current Visual State** ⚠️⚠️
**File:** `src/views/MarketWatchWindow/Actions.cpp:344-367`  
**Severity:** HIGH

**Problem:**
```cpp
void MarketWatchWindow::onSavePortfolio()
{
    // ...
    MarketWatchColumnProfile currentProfile = m_model->getColumnProfile();
    captureProfileFromView(currentProfile);  // ← Captures view state
    
    m_model->setColumnProfile(currentProfile);  // ← Updates model (TRIGGERS RESET!)
    
    if (MarketWatchHelpers::savePortfolio(fileName, scrips, currentProfile)) {
```

**Issues:**
1. **RACE CONDITION**: 
   - Line 357: Captures column widths/order from current view
   - Line 360: Sets profile back to model → **TRIGGERS MODEL RESET** (Bug #2)
   - **View state is destroyed immediately after being captured!**
   - Save happens with potentially inconsistent state

2. **UNNECESSARY MODEL UPDATE**:
   - Why update the model with the captured profile?
   - Model already has a profile!
   - This line serves no purpose except to break things

3. **TIMING BUG**:
   - If view hasn't fully rendered after previous profile change, captured widths may be wrong
   - No synchronization between view render and save operation

**Impact:**
- Saved widths may not match what user sees
- Column order can be corrupted during save
- Unpredictable behavior

**Fix:**
```cpp
void MarketWatchWindow::onSavePortfolio()
{
    // ...
    MarketWatchColumnProfile currentProfile = m_model->getColumnProfile();
    captureProfileFromView(currentProfile);  
    
    // REMOVE THIS LINE - Don't update model during save!
    // m_model->setColumnProfile(currentProfile);
    
    if (MarketWatchHelpers::savePortfolio(fileName, scrips, currentProfile)) {
```

---

### **BUG #4: Profile fromJson Does NOT Validate Column Order** ⚠️⚠️
**File:** `src/models/MarketWatchColumnProfile.cpp:785-795`  
**Severity:** HIGH

**Problem:**
```cpp
// Column order
if (json.contains("columnOrder")) {
    QJsonArray order = json["columnOrder"].toArray();
    m_columnOrder.clear();
    for (const QJsonValue &val : order) {
        m_columnOrder.append(static_cast<MarketWatchColumn>(val.toInt()));
    }
}
```

**Issues:**
1. **NO DUPLICATE CHECK**: If JSON contains duplicate column IDs, the profile will have duplicates
2. **NO RANGE VALIDATION**: Invalid column IDs (e.g., 999, -1) are accepted without validation
3. **NO COMPLETENESS CHECK**: If JSON has only 5 columns but there are 40 total, the profile is incomplete
4. **NO CORRUPTION DETECTION**: If file is corrupted, silently loads garbage data

**Impact:**
- Corrupted files crash the application
- Missing columns become invisible forever
- Duplicate columns cause rendering issues
- No error feedback to user

**Fix:**
```cpp
// Column order
if (json.contains("columnOrder")) {
    QJsonArray order = json["columnOrder"].toArray();
    QList<MarketWatchColumn> newOrder;
    QSet<int> seen;
    
    for (const QJsonValue &val : order) {
        int colId = val.toInt();
        
        // Validate range
        if (colId < 0 || colId >= static_cast<int>(MarketWatchColumn::COLUMN_COUNT)) {
            qWarning() << "Invalid column ID in profile:" << colId;
            continue;
        }
        
        // Check duplicates
        if (seen.contains(colId)) {
            qWarning() << "Duplicate column ID in profile:" << colId;
            continue;
        }
        
        seen.insert(colId);
        newOrder.append(static_cast<MarketWatchColumn>(colId));
    }
    
    // Ensure all columns are present - add missing ones at end
    for (int i = 0; i < static_cast<int>(MarketWatchColumn::COLUMN_COUNT); ++i) {
        if (!seen.contains(i)) {
            qWarning() << "Adding missing column to profile:" << i;
            newOrder.append(static_cast<MarketWatchColumn>(i));
        }
    }
    
    m_columnOrder = newOrder;
}
```

---

### **BUG #5: Load Portfolio Overwrites Model Before Applying View** ⚠️⚠️
**File:** `src/views/MarketWatchWindow/Actions.cpp:369-408`  
**Severity:** HIGH

**Problem:**
```cpp
void MarketWatchWindow::onLoadPortfolio()
{
    // ... Load scrips and profile from file ...
    
    clearAll();  // ← Clears model data
    
    // Add scrips
    for (const auto &scrip : scrips) {
        // ...
    }
    
    // Apply the loaded profile keys to view
    if (!profile.name().isEmpty()) {
        m_model->setColumnProfile(profile);  // ← MODEL RESET!
        applyProfileToView(profile);         // ← View changes
```

**Issues:**
1. **DOUBLE UPDATE**: Model is updated (with reset), then view is updated separately
   - Model reset already notifies view
   - `applyProfileToView()` tries to apply settings that are already (partially) applied
   - Redundant and confusing

2. **INCOMPLETE VIEW UPDATE**: `applyProfileToView()` only sets widths and visibility, but column ORDER is not applied to view (because view doesn't support reordering!)

3. **NO ERROR HANDLING**: If profile application fails, no rollback or error message

**Impact:**
- Column order from loaded portfolio is ignored
- Confusing double-update causes flickering
- View state inconsistent with model state

---

### **BUG #6: captureProfileFromView Appends Hidden Columns Incorrectly** ⚠️
**File:** `src/views/MarketWatchWindow/Actions.cpp:494-504`  
**Severity:** MEDIUM

**Problem:**
```cpp
// 2. Append remaining columns (those already hidden in the profile/model)
// Maintain their relative order from the existing profile
QList<MarketWatchColumn> existingOrder = profile.columnOrder();
for (MarketWatchColumn col : existingOrder) {
    if (!capturedColumns.contains(col)) {
        newOrder.append(col);
    }
}
```

**Issues:**
1. **ORDER SCRAMBLING**: Hidden columns are appended at the END, not in their original positions
   - If user hides column at position 3, then unhides it later, it appears at the end
   - Original order is lost

2. **NO PRESERVATION OF HIDDEN COLUMN POSITIONS**: The original position of hidden columns in the full order is not tracked

**Impact:**
- Hidden columns lose their position in the order
- When unhidden, they appear at the end instead of original position
- Column order becomes unpredictable over time

**Fix:**
The entire approach is flawed. Instead of appending hidden columns, maintain a single canonical column order in the profile and use visibility flags separately:
- **Column order**: The FULL order of all columns (hidden + visible)
- **Visibility**: Separate boolean map for each column
- **View order**: Derived from column order by filtering visible columns

---

### **BUG #7: initializeDefaults Creates Duplicate Column Order** ⚠️
**File:** `src/models/MarketWatchColumnProfile.cpp:517-529`  
**Severity:** MEDIUM

**Problem:**
```cpp
void MarketWatchColumnProfile::initializeDefaults()
{
    initializeColumnMetadata();
    
    // Set all columns to default visibility
    for (int i = 0; i < static_cast<int>(MarketWatchColumn::COLUMN_COUNT); ++i) {
        MarketWatchColumn col = static_cast<MarketWatchColumn>(i);
        ColumnInfo info = s_columnMetadata[col];
        m_visibility[col] = info.visibleByDefault;
        m_widths[col] = info.defaultWidth;
        m_columnOrder.append(col);  // ← APPENDS EVERY TIME!
    }
}
```

**Issues:**
1. **DUPLICATE APPEND**: If `initializeDefaults()` is called multiple times (e.g., profile copy constructor), `m_columnOrder` accumulates duplicates!
2. **NO CLEAR BEFORE INIT**: Should clear existing order first

**Impact:**
- Profile corruption with duplicate columns
- Rendering artifacts
- Crashes in some cases

**Fix:**
```cpp
void MarketWatchColumnProfile::initializeDefaults()
{
    initializeColumnMetadata();
    
    m_columnOrder.clear();  // ← ADD THIS
    
    for (int i = 0; i < static_cast<int>(MarketWatchColumn::COLUMN_COUNT); ++i) {
        // ...
        m_columnOrder.append(col);
    }
}
```

---

### **BUG #8: saveState Saves Compact JSON Without Formatting** ⚠️
**File:** `src/views/MarketWatchWindow/Actions.cpp:421, 428`  
**Severity:** LOW

**Problem:**
```cpp
settings.setValue("scrips", QJsonDocument(scripsArray).toJson(QJsonDocument::Compact));
// ...
settings.setValue("columnProfile", QJsonDocument(m_model->getColumnProfile().toJson()).toJson(QJsonDocument::Compact));
```

**Issues:**
1. **UNREADABLE**: Compact JSON is hard to debug when things go wrong
2. **NO PRETTY PRINT**: QSettings stores binary-ish data, making it impossible to manually inspect/fix

**Impact:**
- Difficult to debug save/load issues
- Users cannot manually fix corrupted settings

**Fix:**
Use `QJsonDocument::Indented` for debugging builds, or save to separate .json files for human readability

---

### **BUG #9: No Validation in scripFromJson** ⚠️
**File:** `src/views/helpers/MarketWatchHelpers.cpp:237-260`  
**Severity:** MEDIUM

**Problem:**
```cpp
ScripData MarketWatchHelpers::scripFromJson(const QJsonObject &obj)
{
    ScripData scrip;
    
    scrip.symbol = obj["symbol"].toString();
    scrip.exchange = obj["exchange"].toString();
    scrip.token = obj["token"].toInt();
    scrip.isBlankRow = obj["isBlankRow"].toBool();
    // ...
}
```

**Issues:**
1. **NO NULL/MISSING CHECKS**: If JSON is missing required fields, returns default values
2. **NO TYPE VALIDATION**: If "token" is a string instead of int, returns 0
3. **NO ERROR RETURN**: Caller has no way to know if parsing failed

**Impact:**
- Silently loads corrupted data
- Invalid scrips added to list
- Crashes or wrong behavior downstream

**Fix:**
```cpp
bool MarketWatchHelpers::scripFromJson(const QJsonObject &obj, ScripData &scrip)
{
    if (!obj.contains("symbol") || !obj.contains("exchange") || !obj.contains("token")) {
        qWarning() << "Missing required fields in scrip JSON";
        return false;
    }
    
    scrip.symbol = obj["symbol"].toString();
    scrip.exchange = obj["exchange"].toString();
    scrip.token = obj["token"].toInt();
    scrip.isBlankRow = obj["isBlankRow"].toBool(false);
    
    // Validate
    if (!scrip.isBlankRow && (scrip.symbol.isEmpty() || scrip.exchange.isEmpty() || scrip.token <= 0)) {
        qWarning() << "Invalid scrip data in JSON";
        return false;
    }
    
    // ...
    return true;
}
```

---

### **BUG #10: Profile Manager Overwrites Profiles on Add** ⚠️
**File:** `src/models/MarketWatchColumnProfile.cpp:847-850`  
**Severity:** MEDIUM

**Problem:**
```cpp
void MarketWatchProfileManager::addProfile(const MarketWatchColumnProfile &profile)
{
    m_profiles[profile.name()] = profile;
    saveAllProfiles("profiles/marketwatch");  // ← SAVES ALL PROFILES!
}
```

**Issues:**
1. **EXPENSIVE**: Every addProfile() call saves ALL profiles to disk
   - O(N) file writes for N profiles
   - Slow and wasteful

2. **NO ATOMIC SAVE**: If app crashes during `saveAllProfiles()`, all profiles may be corrupted

3. **REDUNDANT I/O**: Called during initialization when loading default profiles

**Impact:**
- Slow startup
- File corruption risk
- Unnecessary disk wear

**Fix:**
```cpp
void MarketWatchProfileManager::addProfile(const MarketWatchColumnProfile &profile)
{
    m_profiles[profile.name()] = profile;
    // Don't auto-save here - let caller decide when to save
}

// Explicit save method
void MarketWatchProfileManager::save()
{
    saveAllProfiles("profiles/marketwatch");
}
```

---

### **BUG #11: restoreState Doesn't Call applyProfileToView** ⚠️
**File:** `src/views/MarketWatchWindow/Actions.cpp:431-463`  
**Severity:** HIGH

**Problem:**
```cpp
void MarketWatchWindow::restoreState(QSettings &settings)
{
    // Restore Scrips
    // ...
    
    // Restore Column Profile
    if (settings.contains("columnProfile")) {
        QByteArray data = settings.value("columnProfile").toByteArray();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            MarketWatchColumnProfile profile;
            profile.fromJson(doc.object());
            m_model->setColumnProfile(profile);  // ← Updates model
            m_model->layoutChanged();            // ← Forces view refresh
        }
    }
}
```

**Issues:**
1. **MISSING VIEW UPDATE**: Unlike `onLoadPortfolio()`, this doesn't call `applyProfileToView()`
   - Column widths not restored properly
   - Inconsistent with other load paths

2. **layoutChanged() MISUSE**: `layoutChanged()` is for model structure changes, not for forcing view refresh
   - Should emit `dataChanged()` or rely on model reset

3. **NO ERROR HANDLING**: If JSON parsing fails, silently ignores

**Impact:**
- Workspace restore doesn't fully restore column state
- Width/order mismatch between save and restore

---

### **BUG #12: applyProfileToView Only Sets Widths, Not Order** ⚠️
**File:** `src/views/MarketWatchWindow/Actions.cpp:507-535`  
**Severity:** CRITICAL

**Problem:**
```cpp
void MarketWatchWindow::applyProfileToView(const MarketWatchColumnProfile &profile)
{
    QHeaderView *header = this->horizontalHeader();
    
    // The Model reset (in setColumnProfile) sets up the logical columns 0..N 
    // to match profile.visibleColumns().
    // We just need to apply widths and ensure visibility.
    // NOTE: Visual order is automatically handled by the model reset because 
    // MarketWatchModel::columnCount and data() follow the profile's order.
    
    QList<MarketWatchColumn> visibleCols = profile.visibleColumns();
    
    for (int i = 0; i < visibleCols.size(); ++i) {
        // Ensure index is valid for the header
        if (i < header->count()) {
            MarketWatchColumn col = visibleCols[i];
            
            // Apply Width
            int width = profile.columnWidth(col);
            if (width > 0) {
                header->resizeSection(i, width);
            }
            
            // Ensure section is visible (cleanup any previous view state)
            header->setSectionHidden(i, false);
        }
    }
}
```

**Issues:**
1. **COMMENT LIES**: The comment says "Visual order is automatically handled by the model reset" but this is FALSE if QHeaderView has been manually reordered by the user!

2. **NO ORDER APPLICATION**: The function never calls `header->moveSection()` to actually reorder columns in the view

3. **ASSUMES MODEL ORDER = VIEW ORDER**: This is only true if:
   - View reordering is disabled (which it currently is, see Bug #1)
   - OR view order is always reset to match model (which it isn't)

4. **MISSING FUNCTIONALITY**: Should have code like:
```cpp
// Apply column order to view
QList<MarketWatchColumn> visibleCols = profile.visibleColumns();
for (int newVisualIdx = 0; newVisualIdx < visibleCols.size(); ++newVisualIdx) {
    MarketWatchColumn col = visibleCols[newVisualIdx];
    
    // Find current logical index of this column
    int logicalIdx = -1;
    for (int i = 0; i < header->count(); ++i) {
        if (m_model->getColumnProfile().visibleColumns()[i] == col) {
            logicalIdx = i;
            break;
        }
    }
    
    if (logicalIdx >= 0) {
        header->moveSection(header->visualIndex(logicalIdx), newVisualIdx);
    }
}
```

**Impact:**
- **THIS IS THE ROOT CAUSE OF THE USER'S BUG!**
- Column order from profiles is completely ignored
- View shows columns in arbitrary order
- Users see column order change unpredictably

---

## 🔧 Additional Issues

### **ISSUE #13: No setSectionsMovable(true) Anywhere**
**Files:** `src/views/MarketWatchWindow/UI.cpp`, `include/core/widgets/CustomMarketWatch.h`  
**Severity:** HIGH

**Problem:**
The entire column reordering infrastructure exists in the code, but **user cannot actually reorder columns via drag-and-drop** because `QHeaderView::setSectionsMovable(true)` is never called for MarketWatchWindow!

Other windows (OrderBook, TradeBook, etc.) DO call it:
```cpp
// From CustomOrderBook.cpp:65
horizontalHeader()->setSectionsMovable(true);
```

But MarketWatchWindow doesn't.

**Impact:**
- Users cannot manually arrange columns
- All column order capture code is useless
- Feature appears broken

**Fix:**
Add to `MarketWatchWindow::setupUI()`:
```cpp
horizontalHeader()->setSectionsMovable(true);
```

---

### **ISSUE #14: No Profile Metadata (Version, Timestamp)**
**File:** `src/models/MarketWatchColumnProfile.cpp:726-755`  
**Severity:** LOW

**Problem:**
Profile JSON has no version field or timestamp for tracking changes over time.

**Impact:**
- Cannot detect incompatible profile formats
- No migration path for future changes
- Hard to debug "when did this profile get corrupted?"

**Fix:**
Add to `toJson()`:
```cpp
json["version"] = 2;  // Profile format version
json["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
json["modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
```

---

### **ISSUE #15: Column Profile Dialog Inconsistency**
**File:** `src/views/ColumnProfileDialog.cpp`  
**Severity:** MEDIUM

**Problem:**
ColumnProfileDialog may manipulate profile order differently than captureProfileFromView(), leading to different orders depending on how profile is saved (via dialog vs. portfolio save).

**Impact:**
- Inconsistent behavior
- User confusion

---

## 🎯 Root Cause Analysis

### Why Column Order Changes Unpredictably:

1. **View reordering is disabled** (Bug #13) → Users can't arrange columns manually
2. **captureProfileFromView()** captures the MODEL order, not VIEW order (Bug #1)
3. **applyProfileToView()** doesn't apply order to view (Bug #12)
4. **Model reset destroys view state** (Bug #2)
5. **Save/Load race conditions** corrupt the order (Bug #3)

### The Cascade Effect:
```
User saves portfolio
  → captureProfileFromView() captures model order (not view order)
  → Appends hidden columns at end (Bug #6)
  → Order is scrambled
  → setColumnProfile() triggers reset (Bug #2)
  → View state lost
  → Portfolio saved with wrong order
  
User loads portfolio  
  → Profile loaded from JSON (no validation, Bug #4)
  → setColumnProfile() resets model (Bug #2)
  → applyProfileToView() sets widths but NOT order (Bug #12)
  → View shows columns in model order, not profile order
  → User sees different column order than what they saved!
```

---

## ✅ Recommended Fixes (Priority Order)

### **PRIORITY 1 - CRITICAL**
1. ✅ **Fix Bug #12** - Make `applyProfileToView()` actually apply column order
2. ✅ **Fix Bug #13** - Enable `setSectionsMovable(true)` for MarketWatchWindow
3. ✅ **Fix Bug #1** - Rewrite `captureProfileFromView()` to use actual view order
4. ✅ **Fix Bug #3** - Remove unnecessary model update during save

### **PRIORITY 2 - HIGH**
5. ✅ **Fix Bug #2** - Replace model reset with `layoutChanged()` for better performance
6. ✅ **Fix Bug #4** - Add JSON validation and error handling
7. ✅ **Fix Bug #11** - Make restoreState consistent with onLoadPortfolio

### **PRIORITY 3 - MEDIUM**
8. ✅ **Fix Bug #6** - Redesign hidden column order preservation
9. ✅ **Fix Bug #7** - Clear column order before initialization
10. ✅ **Fix Bug #9** - Add return value to scripFromJson for error handling
11. ✅ **Fix Bug #10** - Defer profile saving to explicit save calls

### **PRIORITY 4 - LOW**
12. ✅ **Fix Bug #8** - Use indented JSON for debugging
13. ✅ **Add Issue #14** - Profile versioning and metadata

---

## 🧪 Testing Requirements

After fixes, verify:

1. **Column Order Persistence**:
   - Arrange columns manually via drag-drop
   - Save portfolio
   - Close and reopen window
   - Load portfolio
   - ✅ Column order must match exactly

2. **Hidden Column Handling**:
   - Hide column at position 3
   - Save profile
   - Unhide column
   - ✅ Column should return to position 3, not end

3. **Width Preservation**:
   - Resize columns
   - Save and load
   - ✅ Widths must match exactly

4. **Corrupted File Handling**:
   - Manually corrupt a .json file
   - Try to load it
   - ✅ Should show error message, not crash

5. **Race Condition Test**:
   - Quickly change profile → save portfolio → change profile again
   - ✅ No crashes, consistent state

---

## 📊 Impact Assessment

| Bug | Severity | User Impact | Effort to Fix |
|-----|----------|-------------|---------------|
| #1  | CRITICAL | Column order always wrong | HIGH |
| #2  | CRITICAL | UI flicker, lost state | MEDIUM |
| #3  | HIGH | Data corruption during save | LOW |
| #4  | HIGH | Silent data corruption | MEDIUM |
| #5  | HIGH | Confusing behavior | LOW |
| #6  | MEDIUM | Order scrambling | HIGH |
| #7  | MEDIUM | Occasional corruption | LOW |
| #8  | LOW | Hard to debug | LOW |
| #9  | MEDIUM | Silent load failures | LOW |
| #10 | MEDIUM | Performance degradation | LOW |
| #11 | HIGH | Workspace restore broken | LOW |
| #12 | CRITICAL | Order never applied | MEDIUM |
| #13 | HIGH | Feature appears broken | TRIVIAL |

**Total Issues:** 13 bugs + 2 design issues  
**Critical Issues:** 3  
**High Priority Issues:** 5  
**Estimated Fix Time:** 2-3 days for critical bugs, 5-7 days for complete fix

---

## 📝 Conclusion

The portfolio and column profile save/load system is fundamentally broken due to:

1. **Architectural mismatch** between Model (manages column metadata) and View (manages visual presentation)
2. **Incomplete implementation** of column reordering support
3. **Missing validation** and error handling throughout
4. **Race conditions** and state synchronization issues

The good news: Most fixes are localized and straightforward. The bad news: They require careful testing to avoid introducing new bugs.

**Recommendation:** Fix in the priority order listed above, with comprehensive testing after each fix.

---

**Document Version:** 1.0  
**Last Updated:** January 14, 2026  
**Author:** GitHub Copilot Analysis
