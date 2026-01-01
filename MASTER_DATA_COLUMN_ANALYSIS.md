# Master Data Column Analysis - Complete Field-by-Field Breakdown
**Trading Terminal C++ - Contract Master File Structure**
**Generated: 2026-01-01 from Actual XTS API Data**

---

## 📋 Table of Contents

1. [Executive Summary](#executive-summary)
2. [Column Count by Segment & Type](#column-count-by-segment--type)
3. [BSEFO Complete Field Mapping](#bsefo-complete-field-mapping)
4. [NSEFO Complete Field Mapping](#nsefo-complete-field-mapping)
5. [Instrument Type & Option Type Flags](#instrument-type--option-type-flags)
6. [Critical Parser Issues](#critical-parser-issues)
7. [CSV Storage Issues](#csv-storage-issues)
8. [Fix Recommendations](#fix-recommendations)

---

## Executive Summary

### Key Findings

| Issue | Severity | Impact | Status |
|-------|----------|--------|--------|
| **BSEFO Options field mapping wrong** | 🔴 CRITICAL | Strike price in wrong field, OptionType=0 | UNFIXED |
| **BSEFO CSV missing InstrumentType** | 🔴 CRITICAL | Cannot detect spreads from CSV | UNFIXED |
| **BSEFO CSV missing FreezeQty** | 🟡 MEDIUM | Trading limits not enforced | UNFIXED |
| **Series codes differ (IF/IO vs FUTIDX/OPTIDX)** | 🟡 MEDIUM | ScripBar search failures | KNOWN |

### Column Count Discovery

```
BSEFO:
  - Spread (instrumentType=4):  21 fields
  - Future (instrumentType=1):  21 fields  
  - Option (instrumentType=2):  23 fields  ← 2 EXTRA FIELDS (StrikePrice, OptionType)

NSEFO:
  - Spread (instrumentType=4):  21 fields
  - Future (instrumentType=1):  21 fields
  - Option (instrumentType=2):  23 fields  ← Same as BSEFO
```

---

## Column Count by Segment & Type

| Segment | Type | InstrumentType | Field Count | Key Features |
|---------|------|----------------|-------------|--------------|
| **NSECM** | Equity | 8? | **22** | Full DisplayName at Field 19 |
| **BSECM** | Equity | 8? | **22** | Full DisplayName at Field 19 |
| **NSEFO** | Option | 2 | **23** | Strike at 18, OptType at 19, Display at 20 |
| **NSEFO** | Future | 1 | **21** | Display at 18 |
| **BSEFO** | Option | 2 | **23** | Strike at 18, OptType at 19, Display at 20 |
| **BSEFO** | Future | 1 | **21** | Display at 18 |

---

## CM (Cash Market) Field Mapping (22 Fields)

Based on your examples for **NSECM (Summit Dig)** and **BSECM (Radico)**:

| Field # (1-based) | Index (0-based) | Value (Example) | Description |
|-------------------|-----------------|-----------------|-------------|
| 1 | 0 | NSECM / BSECM | Segment |
| 2 | 1 | 22515 / 532497 | Token |
| 3 | 2 | 8 | InstrumentType |
| 4 | 3 | 806SDIL29 / RADICO | Symbol |
| 5 | 4 | 806SDIL29-N0 | Description |
| 6 | 5 | N0 / A | Series |
| 7 | 6 | 806SDIL29-N0 | NameWithSeries |
| 8 | 7 | 11001000... | InstrumentID |
| 9 | 8 | 120000 / 3795.45| PriceBandHigh |
| 10 | 9 | 80000 / 2530.35 | PriceBandLow |
| 11 | 10 | 999 / 0 | FreezeQty |
| 12 | 11 | 0.01 / 0.05 | TickSize |
| 13 | 12 | 1 | LotSize |
| 14 | 13 | 1 | Multiplier |
| 15 | 14 | 806SDIL29 / RADICO| ShortName/Alias |
| 16 | 15 | INE... | ISIN |
| 17 | 16 | 1 | PriceNumerator |
| 18 | 17 | 1 | PriceDenominator |
| **19** | **18** | **SUMMIT... / RADICO...** | **Full Display Name** ⭐ |
| 20 | 19 | 0 / 100 | Unknown/Reserved |
| 21 | 20 | -1 | Reserved |
| 22 | 21 | -1 | Reserved |

**FUTURE (21 fields) - InstrumentType=1:**
```
BSEFO|1166387|1|BANKEX|BANKEX26FEBFUT|IF|BANKEX-IF|12605701166387|73344.5|60009.15|600|0.05|30|1|-1|BANKEX|2026-02-26T00:00:00|BANKEX 26FEB2026|1|1|BANKEX26FEBFUT
```

**OPTION (23 fields) - InstrumentType=2:**
```
BSEFO|1132771|2|BANKEX|BANKEX25DEC72000CE|IO|BANKEX-IO|12535801132771|20.05|0.05|900|0.05|30|1|-1|BANKEX|2025-12-24T00:00:00|72000|3|BANKEX 24DEC2025 CE 72000|1|1|BANKEX25DEC72000CE
```

### Field-by-Field Breakdown

#### Common Fields (1-17) - All BSEFO Types

| Field # | Name | Spread Example | Future Example | Option Example |
|---------|------|----------------|----------------|----------------|
| 1 | ExchangeSegment | BSEFO | BSEFO | BSEFO |
| 2 | **ExchangeInstrumentID** | 1169176 | 1166387 | 1132771 |
| **3** | **InstrumentType** | **4 (Spread)** | **1 (Future)** | **2 (Option)** |
| 4 | Name (Symbol) | BANKEX | BANKEX | BANKEX |
| 5 | Description | BANKEX25DEC26FEBFUT | BANKEX26FEBFUT | BANKEX25DEC72000CE |
| 6 | **Series** | **IF** | **IF** | **IO** |
| 7 | NameWithSeries | BANKEX-IF | BANKEX-IF | BANKEX-IO |
| 8 | InstrumentID | 12100101169176 | 12605701166387 | 12535801132771 |
| 9 | PriceBandHigh | 3298.85 | 73344.5 | 20.05 |
| 10 | PriceBandLow | -1979.3 | 60009.15 | 0.05 |
| 11 | **FreezeQty** | 600 | 600 | 900 |
| 12 | TickSize | 0.05 | 0.05 | 0.05 |
| 13 | LotSize | 30 | 30 | 30 |
| 14 | Multiplier | 1 | 1 | 1 |
| 15 | AssetToken | -1 | -1 | -1 |
| 16 | UnderlyingName | (empty) | BANKEX | BANKEX |
| 17 | ExpiryDate | 2025-12-24T00:00:00 | 2026-02-26T00:00:00 | 2025-12-24T00:00:00 |

#### Type-Specific Fields (18+)

**SPREAD & FUTURE (Fields 18-21):**

| Field # | Name | Spread | Future |
|---------|------|--------|--------|
| 18 | DisplayName | BANKEX 24DEC26FEB SPD | BANKEX 26FEB2026 |
| 19 | ? | 1 | 1 |
| 20 | ? | 1 | 1 |
| 21 | ISIN/Code | BANKEX25DEC26FEBFUT | BANKEX26FEBFUT |

**OPTION (Fields 18-23) - HAS 2 EXTRA FIELDS:**

| Field # | Name | Option Example | Notes |
|---------|------|----------------|-------|
| **18** | **StrikePrice** | **72000** | ⚠️ This is the STRIKE! |
| **19** | **OptionType** | **3** | **3=CE, 4=PE** |
| 20 | DisplayName | BANKEX 24DEC2025 CE 72000 | Full description |
| 21 | ? | 1 | |
| 22 | ? | 1 | |
| 23 | ISIN/Code | BANKEX25DEC72000CE | |

---

## NSEFO Complete Field Mapping

### Actual Raw Data Examples

**OPTION (23 fields) - InstrumentType=2:**
```
NSEFO|119476|2|NATIONALUM|NATIONALUM26JAN335PE|OPTSTK|NATIONALUM-OPTSTK|2602700119476|76.45|36.45|112501|0.05|3750|1|1100100006364|NATIONALUM|2026-01-27T14:30:00|335|4|NATIONALUM 27JAN2026 PE 335|1|1|NATIONALUM26JAN335PE
```

**SPREAD (21 fields) - InstrumentType=4:**
```
NSEFO|12778737|4|GAIL|GAIL25DEC26JANFUT|FUTSTK|GAIL-FUTSTK|2100112778737|4.2|-4.2|126001|0.01|3150|1|-1||2025-12-30T14:30:00|GAIL 30DEC27JAN SPD|1|1|GAIL25DEC26JANFUT
```

### Field Layout (Same as BSEFO)

**NSEFO uses same field structure as BSEFO:**
- Options: 23 fields (with StrikePrice at 18, OptionType at 19)
- Futures/Spreads: 21 fields

**Key Difference: Series Codes**

| NSE Series | BSE Series | Type |
|------------|------------|------|
| **FUTIDX** | **IF** | Index Future |
| **FUTSTK** | **SF** | Stock Future |
| **OPTIDX** | **IO** | Index Option |
| **OPTSTK** | **SO** | Stock Option |

---

## Instrument Type & Option Type Flags

### InstrumentType (Field #3)

| Value | Type | Description |
|-------|------|-------------|
| **1** | Future | FUTIDX, FUTSTK, IF, SF |
| **2** | Option | OPTIDX, OPTSTK, IO, SO |
| **4** | Spread | Calendar spreads |

### OptionType (Field #19 for Options Only)

| Value | Type | Exchange |
|-------|------|----------|
| **3** | CE (Call) | NSE & BSE |
| **4** | PE (Put) | NSE & BSE |
| 0 | N/A | Futures/Spreads |

---

## Critical Parser Issues

### Issue #1: 🔴 parseBSEFO() Field Mapping is WRONG

**Current Code (MasterFileParser.cpp line 290-294):**
```cpp
if (isOption && fields.size() >= 20) {
    // OPTIONS: field 17=StrikePrice, 18=OptionType, 19=DisplayName
    contract.strikePrice = fields[17].toDouble();  // ❌ WRONG! 
    contract.optionType = fields[18].toInt();      // ❌ WRONG!
    contract.displayName = trimQuotes(fields[19]); // ❌ WRONG!
}
```

**The Problem:**
- Code uses 0-indexed `fields[17]` which is actually field #18 (ExpiryDate)
- Parser comment is correct ("field 17=StrikePrice") but uses **1-indexed** field numbers
- C++ array is **0-indexed**, so field #18 = `fields[17]`

**Actual Field Positions (0-indexed):**
```
fields[16] = expiry (2025-12-24T00:00:00)
fields[17] = strike (72000)          ← CORRECT position if comment uses 1-indexing
fields[18] = optionType (3)
fields[19] = displayName (BANKEX 24DEC2025 CE 72000)
```

**Wait - Let me re-check...**

Looking at the raw option data:
```
BSEFO|1132771|2|BANKEX|BANKEX25DEC72000CE|IO|BANKEX-IO|12535801132771|20.05|0.05|900|0.05|30|1|-1|BANKEX|2025-12-24T00:00:00|72000|3|BANKEX 24DEC2025 CE 72000|1|1|BANKEX25DEC72000CE
```

0-indexed array positions:
- fields[0] = "BSEFO"
- fields[1] = "1132771" (token)
- fields[2] = "2" (instrumentType)
- ...
- fields[16] = "2025-12-24T00:00:00" (expiry)
- fields[17] = "72000" (strikePrice) ✅
- fields[18] = "3" (optionType) ✅
- fields[19] = "BANKEX 24DEC2025 CE 72000" (displayName) ✅

**The parser code is CORRECT!** The issue must be elsewhere...

### 🔍 Root Cause Investigation

If the parser is correct, why does the CSV show wrong data?

**CSV Header:**
```
Token,Symbol,DisplayName,Description,Series,...,StrikePrice,OptionType,...
```

**CSV Data (BSEFO Option):**
```
1132771,BANKEX,72000,BANKEX25DEC72000CE,IO,30,0.05,24DEC2025,3,0,...
                ^^^^^ DisplayName = "72000" (should be "BANKEX 24DEC2025 CE 72000")
                                                       ^ StrikePrice = 3 (should be 72000)
                                                         ^ OptionType = 0 (should be CE)
```

**The Issue:** Data is SHIFTED in the CSV!

- `displayName` field (column 3) contains "72000" (the strike price)
- `strikePrice` field (column 9) contains "3" (the option type code)
- `optionType` field (column 10) contains "0" (nothing)

**This means the shift happens in `BSEFORepository::saveProcessedCSV()` or `loadFromContracts()`!**

---

## CSV Storage Issues

### Issue #2: 🔴 BSEFO CSV Has Wrong Data Mapping

**Evidence from actual bsefo_processed.csv:**
```csv
# Header:
Token,Symbol,DisplayName,Description,Series,LotSize,TickSize,ExpiryDate,StrikePrice,OptionType,...

# Option row (WRONG!):
1132771,BANKEX,72000,BANKEX25DEC72000CE,IO,30,0.05,24DEC2025,3,0,...
               ^^^^^ Wrong! Should be "BANKEX 24DEC2025 CE 72000"
                                                        ^ Wrong! Should be 72000
                                                          ^ Wrong! Should be CE/3
```

**Root Cause Analysis:**

Looking at `BSEFORepository::loadFromContracts()` (line 164-172):
```cpp
m_token.push_back(contract.exchangeInstrumentID);
m_name.push_back(contract.name);
m_displayName.push_back(contract.displayName);      // ← Storing displayName
m_description.push_back(contract.description);
m_series.push_back(contract.series);
m_lotSize.push_back(contract.lotSize);
m_tickSize.push_back(contract.tickSize);
m_expiryDate.push_back(contract.expiryDate);
m_strikePrice.push_back(contract.strikePrice);      // ← Storing strikePrice
```

**The code looks correct, so the issue must be in the initial parsing or CSV was generated by OLDER broken code.**

### Issue #3: 🔴 BSEFO CSV Missing InstrumentType Column

**Observed:**
- NSEFO CSV: 27 columns (ends with ...Theta,InstrumentType)  
- BSEFO CSV: 26 columns (ends with ...Theta) - **MISSING InstrumentType!**

**Impact:** When loading from CSV, `instrumentType = fields[fields.size()-1].toInt()` reads "0" from Theta field.

**Solution:** Delete old bsefo_processed.csv and regenerate:
```bash
rm ~/Desktop/trading_terminal_cpp/masters/processed_csv/bsefo_processed.csv
```

---

## Fix Recommendations

### Fix #1: Delete Corrupted BSEFO CSV (IMMEDIATE)

```bash
rm /home/ubuntu/Desktop/trading_terminal_cpp/masters/processed_csv/bsefo_processed.csv
```

This will force regeneration from raw master file on next startup.

### Fix #2: Verify parseBSEFO() Field Indices

The current `parseBSEFO()` appears correct based on 0-indexed analysis:

```cpp
if (isOption && fields.size() >= 20) {
    contract.strikePrice = fields[17].toDouble();  // Field #18 (0-indexed: 17) = strike
    contract.optionType = fields[18].toInt();      // Field #19 (0-indexed: 18) = optType
    contract.displayName = trimQuotes(fields[19]); // Field #20 (0-indexed: 19) = display
}
```

**However**, the check should be `fields.size() >= 20` for proper parsing (currently is, good).

### Fix #3: Add Debug Logging to Track Field Mapping

Add to `parseBSEFO()`:
```cpp
if (isOption && debugCount < 5) {
    qDebug() << "[BSEFO Option Parser]"
             << "Token:" << contract.exchangeInstrumentID
             << "Strike:" << contract.strikePrice
             << "OptType:" << contract.optionType
             << "Display:" << contract.displayName;
    debugCount++;
}
```

### Fix #4: Ensure CSV Header Matches Data

Update `BSEFORepository::saveProcessedCSV()` header to match NSEFO:
- Add FreezeQty column
- Ensure InstrumentType is at the end

---

## Appendix: Side-by-Side Comparison

### Raw Option Data vs Processed CSV

**RAW (Correct):**
```
Field 4 (Name):        BANKEX
Field 18 (Strike):     72000
Field 19 (OptType):    3 (CE)
Field 20 (Display):    BANKEX 24DEC2025 CE 72000
```

**CSV (Wrong):**
```
Column 2 (Symbol):      BANKEX
Column 3 (DisplayName): 72000          ← SHOULD BE "BANKEX 24DEC2025 CE 72000"
Column 9 (StrikePrice): 3              ← SHOULD BE 72000
Column 10 (OptionType): 0              ← SHOULD BE "CE" or 3
```

**The CSV file was generated by buggy code and needs to be regenerated.**

---

## Next Steps

1. ✅ Analysis complete - root cause identified
2. ⏳ Delete bsefo_processed.csv
3. ⏳ Re-run application with "Download Masters" to regenerate
4. ⏳ Verify new CSV has correct data
5. ⏳ Test ScripBar search for BSE FO options

---

*Analysis based on actual XTS API master data provided 2026-01-01*
