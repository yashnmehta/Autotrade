# POC Week 1-2: Gate 1 Integration Test Guide

**Date:** February 18, 2026  
**Task:** Task 4 - Integration Testing  
**Duration:** 4-6 hours  
**Success Criteria:** All Gate 1 tests pass → GO to Phase 1

---

## 📋 Prerequisites

### 1. Build Status
```powershell
cd C:\Users\admin\Desktop\trading_terminal_cpp\build_ninja
Test-Path "TradingTerminal.exe"  # Should return True
```

### 2. Master Data
Ensure NIFTY master file is loaded with:
- **Expiry:** Current weekly (e.g., 20FEB26, 27FEB26)
- **Strikes:** At minimum: 24400, 24450, 24500, 24550, 24600, 24650, 24700
- **Instrument:** NIFTY FO (Segment = 2)

### 3. Test Strategy File
Location: `test_strategies/atm_straddle_poc.json`
- Mode: "options"
- Legs: 2 (CE + PE ATM Straddle)
- Symbol: "NIFTY"

---

## 🎯 Gate 1 Test Cases

### **Test 1: ATM Strike Resolution** ⭐ CRITICAL

**Objective:** Verify `resolveATMStrike()` returns correct strike

**Input:**
- Symbol: "NIFTY"
- Spot Price: 24567.50
- Offset: 0
- Expiry: Current weekly

**Expected Output:**
- ATM Strike: **24550**
- Reasoning: 24567.50 is between 24550 and 24600, rounds to nearest (24550)

**How to Test:**
1. Open TradingTerminal.exe
2. Enable console output (View → Console or check logs/)
3. Navigate to Strategy Manager
4. Load `atm_straddle_poc.json`
5. Set deployment parameters:
   - Symbol: NIFTY
   - Quantity: 25 (per leg)
   - Account: Test account
6. Click "Deploy Strategy" (do NOT start yet)
7. **CHECKPOINT:** Watch console for log:
   ```
   [OptionsEngine] ATM Resolution: Spot=24567.50 → ATM=24550 (offset=0)
   ```

**Result:**
- ✅ PASS: Log shows ATM=24550
- ❌ FAIL: ATM != 24550 → STOP, debug resolveATMStrike()

---

### **Test 2: Option Symbol Building**

**Objective:** Verify `buildOptionSymbol()` creates correct trading symbols

**Expected Output:**
- CE Symbol: "NIFTY24550CE"
- PE Symbol: "NIFTY24550PE"

**How to Test:**
1. Continue from Test 1
2. Watch console for logs:
   ```
   [OptionsEngine] Built symbol: NIFTY24550CE (NIFTY 24550 CE 20FEB26)
   [OptionsEngine] Built symbol: NIFTY24550PE (NIFTY 24550 PE 20FEB26)
   ```

**Result:**
- ✅ PASS: Symbols match expected format
- ❌ FAIL: Symbol format incorrect → Check buildOptionSymbol()

---

### **Test 3: Contract Token Lookup**

**Objective:** Verify `getContractToken()` fetches valid tokens from master

**Expected Output:**
- CE Token: Valid integer > 0 (e.g., 43521, 43522, etc.)
- PE Token: Valid integer > 0 (different from CE)

**How to Test:**
1. Continue from Test 2
2. Watch console for logs:
   ```
   [OptionsEngine] Contract lookup: NIFTY24550CE → Token=43521
   [OptionsEngine] Contract lookup: NIFTY24550PE → Token=43522
   ```

**Result:**
- ✅ PASS: Both tokens are valid (> 0)
- ❌ FAIL: Token = 0 → Master file not loaded or symbol not found

---

### **Test 4: Multi-Leg Resolution**

**Objective:** Verify `resolveLeg()` resolves both CE and PE legs

**Expected Output:**
- Leg 1 (CE): strike=24550, symbol="NIFTY24550CE", token=valid, side="SELL", qty=25
- Leg 2 (PE): strike=24550, symbol="NIFTY24550PE", token=valid, side="SELL", qty=25

**How to Test:**
1. Continue from Test 3
2. Watch console for logs:
   ```
   [OptionsEngine] Resolved Leg: ce_leg → NIFTY24550CE (Token=43521, SELL 25)
   [OptionsEngine] Resolved Leg: pe_leg → NIFTY24550PE (Token=43522, SELL 25)
   ```

**Result:**
- ✅ PASS: Both legs resolved with correct strikes, symbols, tokens
- ❌ FAIL: Any leg resolution failed → Check resolveLeg()

---

### **Test 5: Order Placement** ⚠️ CAUTION

**Objective:** Verify orders reach broker API (without execution)

**Setup:**
- Use **PAPER TRADING** or **TEST ACCOUNT**
- Verify broker connection is active
- Ensure sufficient margin

**How to Test:**
1. After Test 4 passes, click "Start Strategy"
2. Watch Strategy Manager UI for order status
3. Watch console for logs:
   ```
   [CustomStrategy] Placing order: SELL NIFTY24550CE @ LTP (Qty=25)
   [CustomStrategy] Placing order: SELL NIFTY24550PE @ LTP (Qty=25)
   [OrderExecutor] Order sent: OrderID=123456
   [OrderExecutor] Order sent: OrderID=123457
   ```

**Expected Outcome:**
- ✅ PASS: Both orders submitted to broker API (status: PENDING or OPEN)
- ⚠️ PARTIAL: Orders submitted but REJECTED → Check margin/lot size
- ❌ FAIL: Orders not sent → Check OrderExecutionEngine integration

---

## 🔍 Debugging Commands

### View Console Output
```powershell
# Run with console visible
cd C:\Users\admin\Desktop\trading_terminal_cpp\build_ninja
.\TradingTerminal.exe
```

### Check Log Files
```powershell
Get-Content ".\logs\strategy_$(Get-Date -Format 'yyyyMMdd').log" -Tail 100
```

### Verify Master Data Loaded
```cpp
// In RepositoryManager (via console logging)
qDebug() << "Loaded strikes for NIFTY:" << repo->getStrikesForSymbolExpiry("NIFTY", "20FEB26");
```

---

## ✅ Gate 1 Pass Criteria

ALL of the following must pass:

1. ✅ **Test 1:** ATM resolution returns 24550 for spot 24567.50
2. ✅ **Test 2:** Symbols built correctly (NIFTY24550CE/PE)
3. ✅ **Test 3:** Valid tokens retrieved from master
4. ✅ **Test 4:** Both legs resolved successfully
5. ✅ **Test 5:** Orders submitted to broker API

**If ALL PASS:**
- 🟢 **GO DECISION** → Proceed to Phase 1 (UI Development)
- Time saved: 8 hours (backend validated upfront)

**If ANY FAIL:**
- 🔴 **NO-GO DECISION** → Fix backend issues before UI
- Estimated fix time: +4 weeks if major redesign needed

---

## 📊 Expected Console Output (Success Scenario)

```
[OptionsEngine] ATM Resolution: Spot=24567.50 → ATM=24550 (offset=0)
[OptionsEngine] Built symbol: NIFTY24550CE (NIFTY 24550 CE 20FEB26)
[OptionsEngine] Built symbol: NIFTY24550PE (NIFTY 24550 PE 20FEB26)
[OptionsEngine] Contract lookup: NIFTY24550CE → Token=43521
[OptionsEngine] Contract lookup: NIFTY24550PE → Token=43522
[OptionsEngine] Resolved Leg: ce_leg → NIFTY24550CE (Token=43521, SELL 25)
[OptionsEngine] Resolved Leg: pe_leg → NIFTY24550PE (Token=43522, SELL 25)
[CustomStrategy] Strategy initialized: POC ATM Straddle
[CustomStrategy] Mode: Options (2 legs)
[CustomStrategy] Placing order: SELL NIFTY24550CE @ LTP (Qty=25)
[CustomStrategy] Placing order: SELL NIFTY24550PE @ LTP (Qty=25)
[OrderExecutor] Order sent: OrderID=123456 (Status: PENDING)
[OrderExecutor] Order sent: OrderID=123457 (Status: PENDING)
```

---

## 🚨 Common Issues & Solutions

### Issue 1: Token = 0
**Symptom:** `[OptionsEngine] Contract lookup: ... → Token=0`
**Cause:** Master file not loaded or symbol not in database
**Solution:**
1. Check if NIFTY FO master file exists in `MasterFiles/`
2. Verify expiry date format matches (e.g., "20FEB26" not "20-Feb-2026")
3. Reload master files via File → Reload Master Data

### Issue 2: ATM != 24550
**Symptom:** `ATM=24600` when spot=24567.50
**Cause:** Strike chain not available or wrong rounding logic
**Solution:**
1. Verify strikes array: `[24400, 24450, 24500, 24550, 24600, ...]`
2. Check ATMCalculator::calculateFromActualStrikes() logic
3. Add debug log to show all available strikes

### Issue 3: Strategy JSON Parse Error
**Symptom:** "Invalid JSON" or "Unknown mode: options"
**Cause:** StrategyParser doesn't recognize "mode": "options"
**Solution:**
1. Verify StrategyParser.cpp has `if (modeStr == "options")` branch
2. Check StrategyDefinition.h has `Mode::Options` enum
3. Rebuild application after parser changes

### Issue 4: Orders Not Placed
**Symptom:** No order logs appear
**Cause:** CustomStrategy doesn't handle options mode
**Solution:**
1. Verify CustomStrategy::init() detects `mode == Options`
2. Check if placeEntryOrder() is called for options legs
3. May need to implement separate `placeOptionsOrder()` method

---

## 📝 Test Execution Checklist

- [ ] Build TradingTerminal.exe with POC changes
- [ ] Load NIFTY master file
- [ ] Open Strategy Manager
- [ ] Load `atm_straddle_poc.json`
- [ ] Configure deployment (NIFTY, Qty=25, Test account)
- [ ] Deploy strategy (do NOT start)
- [ ] ✅ Test 1: Verify ATM=24550 in console
- [ ] ✅ Test 2: Verify symbols built correctly
- [ ] ✅ Test 3: Verify tokens retrieved
- [ ] ✅ Test 4: Verify both legs resolved
- [ ] Start strategy
- [ ] ✅ Test 5: Verify orders submitted
- [ ] Document results in `GATE1_TEST_RESULTS.md`
- [ ] Make GO/NO-GO decision

---

## 📄 Next Steps After Gate 1

**If PASS:**
1. Document findings in `08_BACKEND_AUDIT_FINDINGS.md` (update)
2. Create detailed integration test report
3. Proceed to Phase 1: UI Form Builder (4-6 weeks)

**If FAIL:**
1. Identify root cause (ATM logic, parser, master data)
2. Fix and re-test
3. Document lessons learned
4. Re-evaluate architecture if major redesign needed

---

**Estimated Test Duration:** 2-3 hours (with console monitoring)  
**Critical Path:** Tests 1, 4, 5 are blocking for GO decision
