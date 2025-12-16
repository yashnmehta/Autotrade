# CustomScripComboBox - Quick Testing Guide

## 🧪 How to Test

### Test 1: Basic Filtering
1. Open the app
2. Click on Symbol field
3. Type "b" → Nothing happens (need 2 chars)
4. Type "ba" → Dropdown opens with: BHARTIARTL, BANKNIFTY
5. ✅ Should show yellow highlighting on "ba" in matches

### Test 2: Case-Insensitive
1. Clear the field
2. Type "BANK" (uppercase) → Should match "BANKNIFTY"
3. Clear the field
4. Type "bank" (lowercase) → Should match "BANKNIFTY"
5. Clear the field
6. Type "BaNk" (mixed) → Should match "BANKNIFTY"
7. ✅ All three should give same results

### Test 3: Tab Key Selection
1. Click on Symbol field
2. Type "rel"
3. Press Tab
4. ✅ Text "rel" should be selected (highlighted)

### Test 4: Escape Key
1. Click on Symbol field
2. Type "in" → Dropdown opens
3. Press Esc
4. ✅ Dropdown should close

### Test 5: Enter Key
1. Click on Symbol field
2. Type "tcs"
3. Use arrow keys to select "TCS"
4. Press Enter
5. ✅ Dropdown closes, "TCS" is selected

### Test 6: Alphabetical Sorting (Symbols)
1. Click on Symbol dropdown
2. Type "a" and wait
3. ✅ Should see items in order: ASIANPAINT, AXISBANK, BHARTIARTL...

### Test 7: Chronological Sorting (Expiry)
1. Click on Expiry field
2. Type "20" 
3. ✅ Should see dates in order: 19-Dec-2024, 26-Dec-2024, 02-Jan-2025...
4. ✅ NOT alphabetically (Dec, Jan, not Jan, Dec)

### Test 8: Numeric Sorting (Strike)
1. Click on Strike field
2. Type "1"
3. ✅ Should see: 17000, 17500, 18000, 18500, 19000...
4. ✅ NOT string order (17000, 17500, 18000, NOT 17000, 18000, 19000, 17500)

### Test 9: 'd' Key Bug (FIXED)
1. Click on Symbol field
2. Type "d"
3. ✅ Should show "d" in field (NOT clear all text)

### Test 10: No Clear Button
1. Look at Symbol field
2. ✅ Should NOT see an 'X' clear button on the right

### Test 11: Highlight Color
1. Click on Symbol field
2. Type "in"
3. ✅ Matched text should have GOLD/YELLOW background (#FFD700)

### Test 12: Debouncing (Performance)
1. Click on Symbol field
2. Type very fast: "BANKNIFTY"
3. ✅ Should feel smooth, no lag
4. ✅ Filtering happens after you stop typing (150ms delay)

### Test 13: Max Visible Items
1. Click on Symbol field
2. Type "a" (shows many matches)
3. ✅ Should show maximum 10 items in dropdown
4. ✅ Should have scrollbar if more than 10 matches

---

## 🎯 Expected Results Summary

| Field | Type | Sort Mode | Example Input | Expected Order |
|-------|------|-----------|---------------|----------------|
| **Symbol** | Text | Alphabetical | "ba" | BANKNIFTY, BHARTIARTL |
| **Expiry** | Date | Chronological | "jan" | 02-Jan-2025, 09-Jan-2025, 16-Jan-2025 |
| **Strike** | Number | Numeric | "18" | 18000, 18500, 21800 (if exists) |

---

## 🐛 Common Issues & Solutions

### Issue: Dropdown doesn't open
- **Check:** Did you type at least 2 characters?
- **Fix:** Type 2+ characters

### Issue: No highlighting
- **Check:** Is there a match?
- **Fix:** Type text that exists in items

### Issue: Wrong sort order
- **Check:** Is the correct sort mode set?
- **Fix:** Verify setSortMode() in ScripBar.cpp

### Issue: Case-sensitive matching
- **Check:** Did you update both files?
- **Fix:** Verify Qt::CaseInsensitive in both matchesFilter and proxyModel

---

## 📊 Visual Examples

### Before (Standard QComboBox)
```
┌─────────────────────┐
│ BANKNIFTY          ▼│ ← Dropdown arrow
└─────────────────────┘
Type: "ban"
┌─────────────────────┐
│ BANKNIFTY           │ ← No highlighting
│ RELIANCE            │ ← Shows all items (no filtering)
│ TCS                 │
│ INFY                │
└─────────────────────┘
```

### After (CustomScripComboBox)
```
┌─────────────────────┐
│ ban                │ ← No dropdown arrow
└─────────────────────┘
Type: "ban" (2+ chars)
┌─────────────────────┐
│ [BAN]KNIFTY        │ ← Yellow highlight
└─────────────────────┘
Only shows matches!
```

---

## ⚡ Performance Tests

### Test with 15 symbols
- ✅ Should be instant
- ✅ No noticeable delay

### Test with 100+ symbols (future)
- ✅ Should still be smooth with debouncing
- ✅ 150ms delay prevents lag

### Test fast typing
1. Type "BANKNIFTY" as fast as possible
2. ✅ Should not stutter
3. ✅ Should filter once after you stop

---

## 🎨 Style Verification

### Dark Theme
- Background: Dark gray (#1e1e1e) ✓
- Text: White (#ffffff) ✓
- Border: Medium gray (#3f3f46) ✓
- Focus Border: Blue (#0e639c) ✓
- Selection: Dark blue (#094771) ✓
- Hover: Dark gray (#2d2d30) ✓

### Highlight
- Background: Gold (#FFD700) ✓
- Text: Black (#000000) ✓
- Font: Bold ✓

---

## 📱 Keyboard Navigation Test

| Key | Expected Behavior |
|-----|-------------------|
| Type 2 chars | Dropdown opens |
| Tab | Select all text |
| Esc | Close dropdown |
| Enter | Confirm selection |
| ↑ | Move up in list |
| ↓ | Move down in list |
| Home | First item |
| End | Last item |

---

## ✅ Final Checklist

- [ ] Symbols sorted A-Z
- [ ] Expiries sorted by date
- [ ] Strikes sorted numerically
- [ ] Case-insensitive search works
- [ ] Highlighting visible
- [ ] Tab selects all text
- [ ] Esc closes dropdown
- [ ] Enter confirms selection
- [ ] 'd' key doesn't clear text
- [ ] No clear button visible
- [ ] Max 10 items in dropdown
- [ ] Smooth typing (no lag)
- [ ] Dark theme applied

---

## 🚀 Next Steps

If all tests pass:
1. ✅ Implementation is complete!
2. ✅ Ready for production use
3. ✅ Can be extended with more symbols/strikes/expiries

If any test fails:
1. Check CMakeLists.txt includes new files
2. Rebuild: `cd build && make`
3. Re-run the application
4. Check console for errors

---

## 📞 Quick Reference

**Files to check if issues:**
- `include/ui/CustomScripComboBox.h`
- `src/ui/CustomScripComboBox.cpp`
- `src/ui/ScripBar.cpp` (integration)
- `CMakeLists.txt` (build config)

**Key settings:**
- Min chars: 2
- Debounce: 150ms
- Max visible: 10 items
- Highlight: #FFD700 (gold)
