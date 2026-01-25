# Master Contracts File - Comprehensive Analysis

**Generated:** 2026-01-25T12:35:58
**Total Records:** 131009
**Parse Errors:** 130572

---

## 1. Executive Summary

### Key Findings

| Metric | Value | Status |
|--------|-------|--------|
| **Total Records** | 131009 | â |
| **Parse Errors** | 130572 | â ï¸ |
| **Unique Symbols** | 215 | â |
| **Unique Expiries** | 4 | â |
| **Records with Asset Token** | 437 (0.333565%) | â |
| **Records with Expiry** | 437 (0.333565%) | â |

## 2. Exchange Segment Breakdown

| Exchange Segment | Count | Percentage |
|-----------------|-------|------------|
| BSEFO | 9 | 0.01% |
| NSEFO | 428 | 0.33% |

## 3. Series Distribution

| Series | Count | Description |
|--------|-------|-------------|
| FUTIDX | 15 | Index Futures |
| FUTSTK | 413 | Stock Futures |
| IF | 9 | Unknown |

## 4. Token Range Analysis

**Overall Token Range:** 831921 - 15289466

| Exchange | Min Token | Max Token | Range | Recommended Storage |
|----------|-----------|-----------|-------|---------------------|
| BSEFO | 831921 | 1169522 | 337601 | Hash (sparse) |
| NSEFO | 12653045 | 15289466 | 2636421 | Hash (sparse) |

## 5. Asset Token Analysis (CRITICAL for Greeks Calculation)

### Summary

| Metric | Value |
|--------|-------|
| Records with Asset Token | 437 |
| Records without Asset Token | 0 |
| Records with asset_token = -1 | 437 (Index options) |

### Asset Token Patterns

| Pattern | Count | Note |
|---------|-------|------|
| MINUS_ONE (Index underlying - needs mapping) | 437 | ❌ **Needs index master mapping** |

### Most Common Asset Tokens

| Asset Token | Frequency | Note |
|-------------|-----------|------|

## 6. Expiry Date Format Analysis

### Format Distribution

| Format | Count | Percentage | Recommended Parser |
|--------|-------|------------|--------------------|
| ISO_DATETIME | 437 | 100.00% | QDateTime::fromString(expiry, Qt::ISODate) |

### Expiry Date Parsing Errors

â **No expiry date parsing errors found!**

## 7. Strike Price Analysis

### Summary

| Metric | Value |
|--------|-------|
| Records with Strike | 0 |
| Records without Strike | 437 (Futures, Cash) |
| Min Strike | 1e+09 |
| Max Strike | 0 |

### Strike Precision by Symbol (Top 10)

| Symbol | Fractional Values | Note |
|--------|------------------|------|

## 8. Option Type Distribution

| Option Type | Count | Description |
|-------------|-------|-------------|

### CE vs PE Balance (Top 10 Symbols)

| Symbol | CE Count | PE Count | Ratio | Balance |
|--------|----------|----------|-------|----------|

## 9. Exchange-Specific Quirks & Recommendations

### BSEFO

| Property | Value |
|----------|-------|
| Composite Tokens | â No |
| Index Underlying | â Yes |
| Fractional Strikes | â No |
| Asset Token Format |  |
| Expiry Format | ISO_DATETIME |

**Notes:**

- Uses underlyingIndexName field for index options

### NSEFO

| Property | Value |
|----------|-------|
| Composite Tokens | â No |
| Index Underlying | â Yes |
| Fractional Strikes | â No |
| Asset Token Format |  |
| Expiry Format | ISO_DATETIME |

**Notes:**

- Uses underlyingIndexName field for index options

## 10. Data Quality Issues

â **Found 130572 validation errors**

### Error Categories

| Error Type | Count |
|------------|-------|
| Invalid expiry date format: 1 | 22207 |
| Invalid strike price: 011NSETE | 1 |
| Invalid strike price: 021NSETE | 1 |
| Invalid strike price: 031NSETE | 1 |
| Invalid strike price: 041NSETE | 1 |
| Invalid strike price: 051NSETE | 1 |
| Invalid strike price: 061NSETE | 1 |
| Invalid strike price: 071NSETE | 1 |
| Invalid strike price: 081NSETE | 1 |
| Invalid strike price: 091NSETE | 1 |
| Invalid strike price: 101NSETE | 1 |
| Invalid strike price: 111NSETE | 1 |
| Invalid strike price: 121NSETE | 1 |
| Invalid strike price: 131NSETE | 1 |
| Invalid strike price: 141NSETE | 1 |
| Invalid strike price: 151NSETE | 1 |
| Invalid strike price: 161NSETE | 1 |
| Invalid strike price: 171NSETE | 1 |
| Invalid strike price: 181NSETE | 1 |
| Invalid strike price: 360ONE | 249 |
| Invalid strike price: ABB | 475 |
| Invalid strike price: ABCAPITA | 289 |
| Invalid strike price: ADANIENS | 489 |
| Invalid strike price: ADANIENT | 514 |
| Invalid strike price: ADANIGRE | 497 |
| Invalid strike price: ADANIPOR | 345 |
| Invalid strike price: ALKEM | 459 |
| Invalid strike price: AMBER | 319 |
| Invalid strike price: AMBUJACE | 453 |
| Invalid strike price: ANGELONE | 253 |
| Invalid strike price: APLAPOLL | 373 |
| Invalid strike price: APOLLOHO | 577 |
| Invalid strike price: ASHOKLEY | 383 |
| Invalid strike price: ASIANPAI | 549 |
| Invalid strike price: ASTRAL | 313 |
| Invalid strike price: AUBANK | 405 |
| Invalid strike price: AUROPHAR | 513 |
| Invalid strike price: AXISBANK | 515 |
| Invalid strike price: BAJAJ-AU | 379 |
| Invalid strike price: BAJAJFIN | 421 |
| Invalid strike price: BAJAJHLD | 459 |
| Invalid strike price: BAJFINAN | 435 |
| Invalid strike price: BANDHANB | 269 |
| Invalid strike price: BANKBARO | 471 |
| Invalid strike price: BANKEX | 1275 |
| Invalid strike price: BANKINDI | 613 |
| Invalid strike price: BDL | 309 |
| Invalid strike price: BEL | 329 |
| Invalid strike price: BHARATFO | 301 |
| Invalid strike price: BHARTIAR | 429 |
| Invalid strike price: BHEL | 527 |
| Invalid strike price: BIOCON | 345 |
| Invalid strike price: BLUESTAR | 387 |
| Invalid strike price: BOSCHLTD | 603 |
| Invalid strike price: BPCL | 311 |
| Invalid strike price: BRITANNI | 505 |
| Invalid strike price: BSE | 229 |
| Invalid strike price: CAMS | 339 |
| Invalid strike price: CANBK | 565 |
| Invalid strike price: CDSL | 349 |
| Invalid strike price: CGPOWER | 313 |
| Invalid strike price: CHOLAFIN | 393 |
| Invalid strike price: CIPLA | 615 |
| Invalid strike price: COALINDI | 618 |
| Invalid strike price: COFORGE | 387 |
| Invalid strike price: COLPAL | 445 |
| Invalid strike price: CONCOR | 439 |
| Invalid strike price: CROMPTON | 469 |
| Invalid strike price: CUMMINSI | 405 |
| Invalid strike price: DABUR | 401 |
| Invalid strike price: DALBHARA | 461 |
| Invalid strike price: DELHIVER | 349 |
| Invalid strike price: DIVISLAB | 553 |
| Invalid strike price: DIXON | 605 |
| Invalid strike price: DLF | 325 |
| Invalid strike price: DMART | 333 |
| Invalid strike price: DRREDDY | 549 |
| Invalid strike price: EICHERMO | 563 |
| Invalid strike price: ETERNAL | 275 |
| Invalid strike price: EXIDEIND | 615 |
| Invalid strike price: FEDERALB | 481 |
| Invalid strike price: FORTIS | 401 |
| Invalid strike price: GAIL | 645 |
| Invalid strike price: GLENMARK | 425 |
| Invalid strike price: GMRAIRPO | 475 |
| Invalid strike price: GODREJCP | 473 |
| Invalid strike price: GODREJPR | 523 |
| Invalid strike price: GRASIM | 553 |
| Invalid strike price: HAL | 349 |
| Invalid strike price: HAVELLS | 605 |
| Invalid strike price: HCLTECH | 339 |
| Invalid strike price: HDFCAMC | 539 |
| Invalid strike price: HDFCBANK | 629 |
| Invalid strike price: HDFCLIFE | 339 |
| Invalid strike price: HEROMOTO | 503 |
| Invalid strike price: HINDALCO | 361 |
| Invalid strike price: HINDPETR | 427 |
| Invalid strike price: HINDUNIL | 483 |
| Invalid strike price: HINDZINC | 555 |
| Invalid strike price: HUDCO | 377 |
| Invalid strike price: ICICIBAN | 541 |
| Invalid strike price: ICICIGI | 421 |
| Invalid strike price: ICICIPRU | 535 |
| Invalid strike price: IDEA | 117 |
| Invalid strike price: IDFCFIRS | 333 |
| Invalid strike price: IEX | 665 |
| Invalid strike price: IIFL | 99 |
| Invalid strike price: INDHOTEL | 619 |
| Invalid strike price: INDIANB | 333 |
| Invalid strike price: INDIGO | 415 |
| Invalid strike price: INDUSIND | 371 |
| Invalid strike price: INDUSTOW | 339 |
| Invalid strike price: INFY | 329 |
| Invalid strike price: INOXWIND | 259 |
| Invalid strike price: IOC | 605 |
| Invalid strike price: IRCTC | 458 |
| Invalid strike price: IREDA | 607 |
| Invalid strike price: IRFC | 481 |
| Invalid strike price: ITC | 711 |
| Invalid strike price: JINDALST | 435 |
| Invalid strike price: JIOFIN | 551 |
| Invalid strike price: JSWENERG | 417 |
| Invalid strike price: JSWSTEEL | 433 |
| Invalid strike price: JUBLFOOD | 509 |
| Invalid strike price: KALYANKJ | 501 |
| Invalid strike price: KAYNES | 378 |
| Invalid strike price: KEI | 375 |
| Invalid strike price: KFINTECH | 231 |
| Invalid strike price: KOTAKBAN | 701 |
| Invalid strike price: KPITTECH | 513 |
| Invalid strike price: LAURUSLA | 473 |
| Invalid strike price: LICHSGFI | 451 |
| Invalid strike price: LICI | 601 |
| Invalid strike price: LODHA | 487 |
| Invalid strike price: LT | 649 |
| Invalid strike price: LTF | 304 |
| Invalid strike price: LTIM | 525 |
| Invalid strike price: LUPIN | 447 |
| Invalid strike price: M&M | 297 |
| Invalid strike price: MANAPPUR | 523 |
| Invalid strike price: MANKIND | 482 |
| Invalid strike price: MARICO | 557 |
| Invalid strike price: MARUTI | 625 |
| Invalid strike price: MAXHEALT | 447 |
| Invalid strike price: MAZDOCK | 535 |
| Invalid strike price: MCX | 485 |
| Invalid strike price: MFSL | 353 |
| Invalid strike price: MOTHERSO | 541 |
| Invalid strike price: MPHASIS | 229 |
| Invalid strike price: MUTHOOTF | 329 |
| Invalid strike price: NATIONAL | 539 |
| Invalid strike price: NAUKRI | 283 |
| Invalid strike price: NBCC | 589 |
| Invalid strike price: NESTLEIN | 485 |
| Invalid strike price: NHPC | 349 |
| Invalid strike price: NIFTY MI | 1579 |
| Invalid strike price: NIFTY NE | 2109 |
| Invalid strike price: NMDC | 351 |
| Invalid strike price: NTPC | 533 |
| Invalid strike price: NUVAMA | 345 |
| Invalid strike price: NYKAA | 461 |
| Invalid strike price: Nifty 50 | 4031 |
| Invalid strike price: Nifty Ba | 2299 |
| Invalid strike price: Nifty Fi | 1079 |
| Invalid strike price: OBEROIRL | 373 |
| Invalid strike price: OFSS | 333 |
| Invalid strike price: OIL | 361 |
| Invalid strike price: ONGC | 652 |
| Invalid strike price: PAGEIND | 309 |
| Invalid strike price: PATANJAL | 493 |
| Invalid strike price: PAYTM | 287 |
| Invalid strike price: PERSISTE | 261 |
| Invalid strike price: PETRONET | 464 |
| Invalid strike price: PFC | 535 |
| Invalid strike price: PGEL | 275 |
| Invalid strike price: PHOENIXL | 399 |
| Invalid strike price: PIDILITI | 575 |
| Invalid strike price: PIIND | 609 |
| Invalid strike price: PNB | 503 |
| Invalid strike price: PNBHOUSI | 387 |
| Invalid strike price: POLICYBZ | 431 |
| Invalid strike price: POLYCAB | 321 |
| Invalid strike price: POWERGRI | 447 |
| Invalid strike price: POWERIND | 381 |
| Invalid strike price: PPLPHARM | 321 |
| Invalid strike price: PREMIERE | 385 |
| Invalid strike price: PRESTIGE | 373 |
| Invalid strike price: RBLBANK | 277 |
| Invalid strike price: RECLTD | 551 |
| Invalid strike price: RELIANCE | 653 |
| Invalid strike price: RVNL | 297 |
| Invalid strike price: SAIL | 525 |
| Invalid strike price: SAMMAANC | 249 |
| Invalid strike price: SBICARD | 411 |
| Invalid strike price: SBILIFE | 415 |
| Invalid strike price: SBIN | 609 |
| Invalid strike price: SENSEX | 3969 |
| Invalid strike price: SHREECEM | 441 |
| Invalid strike price: SHRIRAMF | 347 |
| Invalid strike price: SIEMENS | 279 |
| Invalid strike price: SNSX50 | 393 |
| Invalid strike price: SOLARIND | 550 |
| Invalid strike price: SONACOMS | 425 |
| Invalid strike price: SRF | 647 |
| Invalid strike price: SUNPHARM | 637 |
| Invalid strike price: SUPREMEI | 285 |
| Invalid strike price: SUZLON | 253 |
| Invalid strike price: SWIGGY | 377 |
| Invalid strike price: SYNGENE | 289 |
| Invalid strike price: TATACONS | 483 |
| Invalid strike price: TATAELXS | 469 |
| Invalid strike price: TATAPOWE | 615 |
| Invalid strike price: TATASTEE | 627 |
| Invalid strike price: TATATECH | 533 |
| Invalid strike price: TCS | 597 |
| Invalid strike price: TECHM | 349 |
| Invalid strike price: TIINDIA | 595 |
| Invalid strike price: TITAN | 609 |
| Invalid strike price: TMPV | 303 |
| Invalid strike price: TORNTPHA | 626 |
| Invalid strike price: TORNTPOW | 543 |
| Invalid strike price: TRENT | 377 |
| Invalid strike price: TVSMOTOR | 636 |
| Invalid strike price: ULTRACEM | 485 |
| Invalid strike price: UNIONBAN | 299 |
| Invalid strike price: UNITDSPR | 573 |
| Invalid strike price: UNOMINDA | 299 |
| Invalid strike price: UPL | 369 |
| Invalid strike price: VBL | 405 |
| Invalid strike price: VEDL | 497 |
| Invalid strike price: VOLTAS | 325 |
| Invalid strike price: WAAREEEN | 305 |
| Invalid strike price: WIPRO | 477 |
| Invalid strike price: YESBANK | 143 |
| Invalid strike price: ZYDUSLIF | 385 |

### First 20 Errors

```
Line 429: Invalid strike price: DRREDDY
Line 430: Invalid strike price: PETRONET
Line 431: Invalid strike price: ABCAPITAL
Line 432: Invalid strike price: ABCAPITAL
Line 433: Invalid strike price: BANDHANBNK
Line 434: Invalid strike price: RELIANCE
Line 435: Invalid strike price: TATAELXSI
Line 436: Invalid strike price: LTF
Line 437: Invalid strike price: IEX
Line 438: Invalid strike price: COFORGE
Line 439: Invalid strike price: PHOENIXLTD
Line 440: Invalid strike price: M&M
Line 441: Invalid strike price: 021NSETEST
Line 442: Invalid strike price: BAJAJFINSV
Line 443: Invalid strike price: DELHIVERY
Line 444: Invalid strike price: ANGELONE
Line 445: Invalid strike price: JSWSTEEL
Line 446: Invalid strike price: BOSCHLTD
Line 447: Invalid strike price: BHARATFORG
Line 448: Invalid strike price: LICI
```

## 11. Implementation Recommendations

### Critical Changes Needed

#### 1. Asset Token Extraction

```cpp
// Current logic has issues with composite tokens and -1 values
int64_t extractAssetToken(int64_t rawToken, const QString& symbol) {
    if (rawToken == -1) {
        // Index option - lookup from index master
        return m_indexNameTokenMap.value(symbol, 0);
    }
    
    if (rawToken > 10000000000LL) {
        // Composite format: extract last 5 digits
        return rawToken % 100000;
    }
    
    return rawToken;
}
```

#### 2. Expiry Date Parsing

```cpp
// All expiry dates are in ISO format: 2026-01-27T14:30:00
QDateTime expiryDateTime = QDateTime::fromString(expiryStr, Qt::ISODate);
if (expiryDateTime.isValid()) {
    contract.expiryDate_dt = expiryDateTime.date();
    contract.expiryDate = expiryDateTime.date().toString("ddMMMyyyy").toUpper();
}
```

#### 3. Repository Storage Strategy

Based on token range analysis:

| Exchange | Strategy | Reason |
|----------|----------|--------|
| BSEFO | Hash (token → index) | Tokens are sparse, hash lookup better |
| NSEFO | Hash (token → index) | Tokens are sparse, hash lookup better |

#### 4. Index Master Integration

**Critical:** 437 records have asset_token = -1 (index options)

**Action Required:**
1. Load index master FIRST before F&O
2. Build symbol â token mapping
3. Update asset tokens in NSEFO/BSEFO during parsing
4. Export index name â token map to UDP reader

---

**Analysis Complete**

Next Steps:
1. Review exchange-specific quirks
2. Implement recommended parsers
3. Update asset token extraction logic
4. Integrate index master BEFORE F&O loading
5. Add validation for all parsed fields
