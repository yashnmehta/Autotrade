# Add Scrip to Market Watch - Complete Flow Documentation

## Overview

This document describes the complete flow when a user adds a scrip (instrument) from the ScripBar to a Market Watch window, including XTS API integration for real-time market data.

**Last Updated**: December 16, 2025  
**Status**: Production Ready (All APIs Working) ✅

**Key Achievements (Dec 16, 2025)**:
- ✅ getQuote API fixed - Added required `publishFormat: "JSON"` parameter
- ✅ Multiple instrument support added for getQuote
- ✅ "Already Subscribed" error handling implemented (e-session-0002 fallback to getQuote)
- ✅ Complete API response analysis documented
- ✅ All test scenarios passing

**Related Documentation**:
- [API Response Analysis](API_RESPONSE_ANALYSIS.md) - **NEW**: Comprehensive API testing results with request/response examples
- [Architecture Recommendations](ARCHITECTURE_RECOMMENDATIONS.md) - Proposed improvements
- [Implementation Roadmap](IMPLEMENTATION_ROADMAP.md) - 4-week improvement plan

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Component Interactions](#component-interactions)
3. [Data Flow Sequence](#data-flow-sequence)
4. [Data Structures](#data-structures)
5. [XTS API Integration](#xts-api-integration)
6. [Critical Issues & Solutions](#critical-issues--solutions)
7. [Testing Scenarios](#testing-scenarios)
8. [Future Improvements](#future-improvements)

---

## Architecture Overview

```
┌─────────────┐
│  ScripBar   │ ← User searches & selects instrument
│  (UI Layer) │
└──────┬──────┘
       │ Signal: addToWatchRequested(InstrumentData)
       ↓
┌──────────────┐
│  MainWindow  │ ← Orchestrator - maps data & calls APIs
│ (Controller) │
└──────┬───────┘
       │ 1) addScripFromContract(ScripData)
       │ 2) subscribe(ids, segment, callback)
       ↓
┌────────────────────┐
│ MarketWatchWindow  │ ← Validates, adds to model, updates UI
│   (View Layer)     │
└────────┬───────────┘
         │
    ┌────┴─────┬──────────────┬─────────────┐
    ↓          ↓              ↓             ↓
┌─────────┐ ┌──────┐ ┌─────────────┐ ┌──────────────┐
│  Model  │ │ Table│ │TokenAddress │ │TokenSubscr.  │
│         │ │ View │ │   Book      │ │  Manager     │
└─────────┘ └──────┘ └─────────────┘ └──────────────┘

┌─────────────────────┐
│ XTSMarketDataClient │ ← API Client
│   (Data Layer)      │
└──────┬──────────────┘
       │ 1) POST /instruments/subscription
       │ 2) Socket.IO WebSocket (live ticks)
       ↓
┌─────────────┐
│  XTS API    │
│   Server    │
└─────────────┘
```

---

## Component Interactions

### 1. ScripBar (Instrument Search & Selection)

**File**: `src/app/ScripBar.cpp`

**Responsibilities**:
- Provide UI for instrument search
- Cache search results in `m_instrumentCache`
- Display instrument details (token, expiry, strike, etc.)
- Emit signal when user clicks "Add to Watch"

**Key Methods**:
```cpp
void onSearchButtonClicked()
  → Calls XTS searchInstruments()
  → Stores results in m_instrumentCache
  → Updates UI fields

InstrumentData getCurrentInstrument()
  → Searches m_instrumentCache for exact match
  → Returns complete InstrumentData with all fields

void onAddToWatchClicked()
  → Gets current instrument from cache
  → Emits: addToWatchRequested(InstrumentData)
```

**InstrumentData Structure**:
```cpp
struct InstrumentData {
    int64_t exchangeInstrumentID;  // Token
    QString name;                   // Full name
    QString symbol;                 // Trading symbol
    QString series;                 // EQ, BE, etc.
    QString instrumentType;         // FUTIDX, OPTSTK, etc.
    QString expiryDate;            // DDMMMYYYY format
    double strikePrice;            // 0 for futures
    QString optionType;            // CE/PE/XX
    int exchangeSegment;           // 1=NSECM, 2=NSEFO, etc.
    QString scripCode;             // BSE code
};
```

---

### 2. MainWindow (Orchestration Layer)

**File**: `src/app/MainWindow.cpp`

**Responsibilities**:
- Listen to ScripBar signals
- Map InstrumentData → ScripData
- Call MarketWatch to add scrip
- Initiate XTS subscription for market data
- Route tick updates to all MarketWatch windows

**Key Methods**:
```cpp
void onAddToWatchRequested(const InstrumentData& instrument)
  → Find active MarketWatch window
  → Map InstrumentData to ScripData (all fields)
  → Call marketWatch->addScripFromContract(scripData)
  → If added successfully:
      - Call XTS subscribe(ids, segment, callback)
      - Subscribe returns initial snapshot if first time
      - Live updates come via WebSocket

void setXTSClients(XTSMarketDataClient* mdClient, ...)
  → Connect tickReceived signal to onTickReceived slot
  → Enable real-time price updates

void onTickReceived(const XTS::Tick& tick)
  → Update ALL MarketWatch windows
  → Calculate change and change%
  → Call marketWatch->updatePrice(token, ltp, change, changePercent)
```

**Data Mapping**:
```cpp
// InstrumentData → ScripData mapping
scripData.symbol = instrument.symbol;
scripData.token = instrument.exchangeInstrumentID;
scripData.instrumentType = instrument.instrumentType;
scripData.exchange = mapExchangeSegmentToString(instrument.exchangeSegment);
scripData.seriesExpiry = instrument.expiryDate;
scripData.strikePrice = instrument.strikePrice;
scripData.optionType = instrument.optionType;
// ... plus 40+ other fields initialized to defaults
```

---

### 3. MarketWatchWindow (View & Business Logic)

**File**: `src/views/MarketWatchWindow.cpp`

**Responsibilities**:
- Validate scrip data before adding
- Check for duplicates by token
- Add to model and sync with TokenAddressBook
- Subscribe via TokenSubscriptionManager
- Update prices when tick data arrives

**Key Methods**:
```cpp
bool addScripFromContract(const ScripData& scripData)
  → Validate: token > 0
  → Check: hasDuplicate(token) via TokenAddressBook
  → Add to model: m_model->addScrip(scripData)
  → Track token: m_tokenAddressBook->addToken(token, row)
  → Subscribe: TokenSubscriptionManager::subscribe(exchange, token)
  → Return: true if added, false if duplicate/invalid

void updatePrice(int token, double ltp, double change, double changePercent)
  → Lookup rows: m_tokenAddressBook->getRowsForToken(token)
  → Update model for each row: m_model->updatePrice(row, ...)
  → O(1) lookup performance
```

**Validation Logic**:
```cpp
if (scripData.token <= 0) {
    qWarning() << "Cannot add scrip with invalid token:" << scripData.token;
    return false;
}

if (hasDuplicate(scripData.token)) {
    qWarning() << "Duplicate token detected:" << scripData.token;
    return false;
}
```

---

### 4. TokenAddressBook (Fast Lookup)

**File**: `src/models/TokenAddressBook.cpp`

**Responsibilities**:
- Maintain mapping: token → list of row indices
- O(1) lookup for price updates
- Handle row insertions/deletions
- Support multiple windows with same token

**Key Methods**:
```cpp
void addToken(int token, int row)
  → m_tokenToRows[token].append(row)
  
QList<int> getRowsForToken(int token)
  → Returns all rows containing this token
  → Used by updatePrice() for multi-row updates

void onRowsInserted(int position, int count)
  → Adjust row indices for tokens below position

void onRowsRemoved(int position, int count)
  → Remove deleted rows from mapping
  → Adjust indices for remaining rows
```

**Use Case**: Same token in multiple rows
```cpp
// Token 49543 (NIFTY) at rows 1, 5, 10
m_tokenAddressBook->getRowsForToken(49543)
// Returns: [1, 5, 10]

// When tick arrives, update all 3 rows instantly
for (int row : rows) {
    m_model->updatePrice(row, ltp, change, changePercent);
}
```

---

### 5. MarketWatchModel (Data Model)

**File**: `src/models/MarketWatchModel.cpp`

**Responsibilities**:
- Store ScripData in `QVector<ScripData> m_scrips`
- Implement QAbstractTableModel interface
- Format data for display
- Emit dataChanged signals for UI updates

**Key Methods**:
```cpp
bool addScrip(const ScripData& scrip)
  → Append to m_scrips
  → Emit rowsInserted signal

void updatePrice(int row, double ltp, double change, double changePercent)
  → Update m_scrips[row] fields
  → Emit dataChanged(ltpColumn, changePercentColumn)
  → Triggers UI refresh

QVariant data(const QModelIndex& index, int role)
  → Return formatted data for table cells
  → Handle text, alignment, color roles
```

**ScripData Structure** (40+ fields):
```cpp
struct ScripData {
    // Identification
    int code, token;
    QString symbol, scripName, instrumentName;
    QString instrumentType, marketType, exchange;
    
    // F&O Specific
    double strikePrice;
    QString optionType, seriesExpiry;
    
    // Price Data
    double ltp, open, high, low, close;
    double change, changePercent;
    
    // Volume & Depth
    qint64 volume, ltq;
    double buyPrice, sellPrice;
    qint64 buyQty, sellQty;
    
    // ... 30+ more fields
    
    bool isBlankRow;  // For separator rows
};
```

---

## Data Flow Sequence

### Scenario 1: First-Time Subscription (Happy Path)

```
┌─────┐                ┌────────┐              ┌──────────┐           ┌─────────┐
│User │                │ScripBar│              │MainWindow│           │MarketWW │
└──┬──┘                └───┬────┘              └────┬─────┘           └────┬────┘
   │                       │                        │                      │
   │ 1. Search "NIFTY"     │                        │                      │
   ├──────────────────────>│                        │                      │
   │                       │ XTS searchInstruments()│                      │
   │                       ├───────────────────────>│                      │
   │                       │ Results + Cache        │                      │
   │                       │<───────────────────────┤                      │
   │                       │                        │                      │
   │ 2. Click "Add to MW"  │                        │                      │
   ├──────────────────────>│                        │                      │
   │                       │ getCurrentInstrument() │                      │
   │                       │ (from cache)           │                      │
   │                       │                        │                      │
   │                       │ emit addToWatchRequested(InstrumentData)      │
   │                       ├───────────────────────>│                      │
   │                       │                        │                      │
   │                       │        Map InstrumentData → ScripData         │
   │                       │                        │                      │
   │                       │                        │ addScripFromContract()
   │                       │                        ├─────────────────────>│
   │                       │                        │                      │
   │                       │                        │   Validate token > 0 │
   │                       │                        │   Check duplicate    │
   │                       │                        │   Add to model       │
   │                       │                        │   TokenAddressBook   │
   │                       │                        │   Subscribe (local)  │
   │                       │                        │                      │
   │                       │                        │<─────────────────────┤
   │                       │                        │ Return: true         │
   │                       │                        │                      │
   │                       │  XTS subscribe(ids, segment)                  │
   │                       │                        ├──────────────────────>
   │                       │                        │         ┌─────────┐  │
   │                       │                        │         │XTS API  │  │
   │                       │                        │         └────┬────┘  │
   │                       │                        │              │       │
   │                       │                        │  POST /instruments/  │
   │                       │                        │     subscription     │
   │                       │                        │              │       │
   │                       │  Response: {           │              │       │
   │                       │    "type": "success",  │              │       │
   │                       │    "result": {         │              │       │
   │                       │      "listQuotes": [   │              │       │
   │                       │        "{...Touchline..}" // Initial snapshot│
   │                       │      ]                 │              │       │
   │                       │    }                   │              │       │
   │                       │  }                     │              │       │
   │                       │                        │<─────────────┘       │
   │                       │                        │                      │
   │                       │    Parse listQuotes    │                      │
   │                       │    Extract Touchline   │                      │
   │                       │    Emit tickReceived() │                      │
   │                       │                        │                      │
   │                       │  onTickReceived(tick)  │                      │
   │                       │                        │  updatePrice(token, ltp...)
   │                       │                        ├─────────────────────>│
   │                       │                        │                      │
   │ 3. See updated price in table                  │   Update model       │
   │<───────────────────────────────────────────────┴──────────────────────┘
   │                       │                        │                      │
   │ 4. Live updates via WebSocket continue...      │                      │
   │                       │                        │ Socket.IO tick       │
   │                       │                        │<─────────────────────┤
   │                       │                        │ processTickData()    │
   │                       │                        │ emit tickReceived()  │
   │                       │  onTickReceived()      │ updatePrice()        │
   │                       │                        ├─────────────────────>│
   │<───────────────────────────────────────────────┴──────────────────────┘
```

### Scenario 2: Re-Subscription (Token Already Subscribed)

```
Scenario: User adds NIFTY to MarketWatch-1, then adds same NIFTY to MarketWatch-2

MarketWatch-1:
  1. Subscribe NIFTY token 49543
  2. XTS returns: listQuotes = ["...Touchline data..."]  ← Initial snapshot
  3. Price updates immediately
  4. WebSocket ticks flow to MW-1

MarketWatch-2 (same token):
  1. Subscribe NIFTY token 49543 (already subscribed globally)
  2. XTS returns: listQuotes = []  ← Empty! No snapshot
  3. No immediate price update
  4. BUT: WebSocket ticks flow to MW-2 via onTickReceived()
  5. Price updates arrive via live feed (not from subscription response)

Key Insight: 
- First subscription provides initial snapshot
- Re-subscription only enables live updates for that window
- Must rely on WebSocket for subsequent windows
```

---

## Data Structures

### InstrumentData (ScripBar → MainWindow)

```cpp
struct InstrumentData {
    int64_t exchangeInstrumentID;  // Primary key - the token
    QString name;                   // "NIFTY 26DEC2024 FUT"
    QString symbol;                 // "NIFTY"
    QString series;                 // "EQ", "BE", etc.
    QString instrumentType;         // "FUTIDX", "OPTSTK", "EQUITY"
    QString expiryDate;            // "26DEC2024"
    double strikePrice;            // 0 for futures, >0 for options
    QString optionType;            // "CE", "PE", "XX"
    int exchangeSegment;           // 2 (NSEFO)
    QString scripCode;             // BSE scrip code if applicable
};
```

### ScripData (MainWindow → MarketWatch)

```cpp
struct ScripData {
    // === Identification ===
    int code;                      // Scrip code
    QString symbol;                // "NIFTY"
    QString scripName;             // Short name
    QString instrumentName;        // Full descriptive name
    QString instrumentType;        // "FUTIDX", "OPTSTK", etc.
    QString marketType;            // "N" (Normal)
    QString exchange;              // "NSE", "BSE", "MCX"
    int token;                     // Exchange instrument ID
    
    // === F&O Specific ===
    double strikePrice;            // 25000.0
    QString optionType;            // "CE", "PE", "XX"
    QString seriesExpiry;          // "26DEC2024"
    
    // === Additional Identifiers ===
    QString isinCode;              // ISIN or BSE code
    
    // === Price & Trading Data ===
    double ltp;                    // Last traded price
    qint64 ltq;                    // Last traded quantity
    QString ltpTime;               // HH:MM:SS
    QString lastUpdateTime;        // Timestamp
    
    // === OHLC ===
    double open, high, low, close;
    QString dpr;                   // Daily price range
    
    // === Change Metrics ===
    double change;                 // Absolute Rs change
    double changePercent;          // % change
    QString trendIndicator;        // "↑", "↓", "→"
    
    // === Volume & Value ===
    double avgTradedPrice;
    qint64 volume;                 // Total traded qty
    double value;                  // Total traded value
    
    // === Depth (Bid/Ask) ===
    double buyPrice, sellPrice;
    qint64 buyQty, sellQty;
    qint64 totalBuyQty, totalSellQty;
    
    // === Open Interest ===
    qint64 openInterest;
    double oiChangePercent;
    
    // === Historical Range ===
    double week52High, week52Low;
    double lifetimeHigh, lifetimeLow;
    
    // === Additional Metrics ===
    double marketCap;
    QString tradeExecutionRange;
    
    // === Special ===
    bool isBlankRow;               // For separator rows
};
```

### XTS::Tick (WebSocket Updates)

```cpp
struct Tick {
    int exchangeSegment;              // 2
    int64_t exchangeInstrumentID;     // 49543
    double lastTradedPrice;           // 25960.0
    int lastTradedQuantity;           // 75
    qint64 totalBuyQuantity;          // 696825
    qint64 totalSellQuantity;         // 552150
    qint64 volume;                    // 2695425
    double open, high, low, close;
    double bidPrice, askPrice;
    int bidQuantity, askQuantity;
};
```

---

## XTS API Integration

### 1. Subscribe API

**Endpoint**: `POST /apimarketdata/instruments/subscription`

**Request**:
```json
{
  "instruments": [
    {
      "exchangeSegment": 2,
      "exchangeInstrumentID": 49543
    }
  ],
  "xtsMessageCode": 1502
}
```

**Response (First Time)**:
```json
{
  "type": "success",
  "code": "s-session-0001",
  "description": "Instrument subscribed successfully!",
  "result": {
    "mdp": 1502,
    "quotesList": [
      {
        "exchangeSegment": 2,
        "exchangeInstrumentID": 49543
      }
    ],
    "listQuotes": [
      "{\"MessageCode\":1502,\"ExchangeSegment\":2,\"ExchangeInstrumentID\":49543,\"Touchline\":{\"BidInfo\":{\"Price\":25960,...},\"AskInfo\":{\"Price\":25962,...},\"LastTradedPrice\":25960,\"Close\":26108.7,\"Open\":26012,\"High\":26045,\"Low\":25952.3,...}}"
    ],
    "Remaining_Subscription_Count": 48
  }
}
```

**Response (Already Subscribed)**:
```json
{
  "type": "success",
  "code": "s-session-0001",
  "description": "Instrument subscribed successfully!",
  "result": {
    "mdp": 1502,
    "quotesList": [
      {
        "exchangeSegment": 2,
        "exchangeInstrumentID": 49543
      }
    ],
    "listQuotes": [],  ← EMPTY - no snapshot
    "Remaining_Subscription_Count": 48
  }
}
```

**Key Behavior**:
- ✅ First subscription: Returns HTTP 200 with initial snapshot in `listQuotes`
- ⚠️ Re-subscription: Returns HTTP 400 with error code `e-session-0002` ("Instrument Already Subscribed !")
- ✅ Enables WebSocket updates for the subscribed token
- ⚠️ Must parse nested JSON string in `listQuotes[0]`

**Error Handling Strategy**:
```cpp
if (httpStatus == 400 && obj["code"].toString() == "e-session-0002") {
    // Already subscribed - fetch snapshot via getQuote instead
    m_xtsClient->getQuote(instrumentID, exchangeSegment, [](bool ok, const QJsonObject& quote, const QString& msg) {
        if (ok) {
            // Process quote and update UI
            processTickData(quote);
        }
    });
    callback(true, "Already subscribed (snapshot fetched)");
}
```

### 2. getQuote API (PRODUCTION READY ✅)

**Endpoint**: `POST /apimarketdata/instruments/quotes`

**Request** (Single Instrument):
```json
{
  "instruments": [
    {
      "exchangeSegment": 2,
      "exchangeInstrumentID": 49543
    }
  ],
  "xtsMessageCode": 1502,
  "publishFormat": "JSON"
}
```

**Request** (Multiple Instruments):
```json
{
  "instruments": [
    {"exchangeSegment": 2, "exchangeInstrumentID": 49543},
    {"exchangeSegment": 2, "exchangeInstrumentID": 59175}
  ],
  "xtsMessageCode": 1502,
  "publishFormat": "JSON"
}
```

**Response** (HTTP 200):
```json
{
  "type": "success",
  "code": "s-quotes-0001",
  "description": "Quotes fetched successfully",
  "result": {
    "listQuotes": [
      "{\"ExchangeSegment\":2,\"ExchangeInstrumentID\":49543,\"Touchline\":{\"LastTradedPrice\":25997.1,\"Close\":26108.7,\"TotalTradedQuantity\":3432975,\"High\":26183.2,\"Low\":25930.0,\"Open\":26166.4,\"BidInfo\":{\"Price\":25997.0,\"Size\":50},\"AskInfo\":{\"Price\":25997.1,\"Size\":150}}}"
    ]
  }
}
```

**Key Points**:
- ✅ **publishFormat parameter is REQUIRED** - returns 400 without it
- ✅ Response contains nested JSON string (same format as subscribe)
- ✅ Supports single and multiple instruments in one call
- ✅ Works for already-subscribed instruments (no error)
- ✅ Message codes: 1501 (touchline), 1502 (market depth)

### 3. WebSocket (Socket.IO)

**URL**: `ws://192.168.102.9:3000/apimarketdata/socket.io/?EIO=3&transport=websocket&token=xxx&userID=xxx&publishFormat=JSON&broadcastMode=Partial`

**Protocol**: Socket.IO v3

**Message Format**:
```
42["1502-json-partial",{"ExchangeSegment":2,"ExchangeInstrumentID":49543,"Touchline":{...}}]
```

**Packet Types**:
- `0` - Handshake
- `2` - Ping (respond with `3` pong)
- `40` - Namespace connect
- `42` - Event message

**Event Names**:
- `1501-json-full` - Full touchline update
- `1501-json-partial` - Partial touchline update
- `1502-json-full` - Full market depth
- `1502-json-partial` - Partial market depth (most common)
- `1105-json-partial` - Blank events (filtered out)

---

## XTS API Response Analysis

### Message Code Comparison: 1501 vs 1502

**Message Code 1501 (Touchline Only)**:
```json
{
  "ExchangeSegment": 2,
  "ExchangeInstrumentID": 49543,
  "Touchline": {
    "LastTradedPrice": 25997.1,
    "LastTradedQuantity": 50,
    "TotalTradedQuantity": 3432975,
    "AverageTradePrice": 26058.45,
    "Open": 26166.4,
    "High": 26183.2,
    "Low": 25930.0,
    "Close": 26108.7,
    "TotalBuyQuantity": 0,
    "TotalSellQuantity": 0,
    "BidInfo": {
      "Price": 25997.0,
      "Size": 50
    },
    "AskInfo": {
      "Price": 25997.1,
      "Size": 150
    }
  }
}
```

**Message Code 1502 (Market Depth)**:
```json
{
  "ExchangeSegment": 2,
  "ExchangeInstrumentID": 49543,
  "Touchline": {
    "LastTradedPrice": 25997.1,
    "LastTradedQuantity": 50,
    "TotalTradedQuantity": 3432975,
    "Open": 26166.4,
    "High": 26183.2,
    "Low": 25930.0,
    "Close": 26108.7,
    "BidInfo": {
      "Price": 25997.0,
      "Size": 50
    },
    "AskInfo": {
      "Price": 25997.1,
      "Size": 150
    }
  },
  "Bids": [
    {"Price": 25997.0, "Quantity": 50, "NumberOfOrders": 1},
    {"Price": 25996.9, "Quantity": 100, "NumberOfOrders": 2},
    {"Price": 25996.8, "Quantity": 75, "NumberOfOrders": 1},
    {"Price": 25996.7, "Quantity": 150, "NumberOfOrders": 3},
    {"Price": 25996.6, "Quantity": 200, "NumberOfOrders": 4}
  ],
  "Asks": [
    {"Price": 25997.1, "Quantity": 150, "NumberOfOrders": 2},
    {"Price": 25997.2, "Quantity": 100, "NumberOfOrders": 1},
    {"Price": 25997.3, "Quantity": 75, "NumberOfOrders": 1},
    {"Price": 25997.4, "Quantity": 200, "NumberOfOrders": 3},
    {"Price": 25997.5, "Quantity": 250, "NumberOfOrders": 5}
  ]
}
```

**Key Differences**:
| Feature | Message Code 1501 | Message Code 1502 |
|---------|-------------------|-------------------|
| Touchline | ✅ Yes | ✅ Yes |
| Bid/Ask Best | ✅ Yes | ✅ Yes |
| 5 Level Depth | ❌ No | ✅ Yes (Bids/Asks arrays) |
| Order Count | ❌ No | ✅ Yes (NumberOfOrders) |
| Data Size | ~200 bytes | ~600 bytes |
| Update Frequency | Same | Same |
| Use Case | Simple price display | Market depth analysis |

**Recommendation**: Use 1502 for market watch (provides depth for future features), use 1501 only if bandwidth is critical.

---

### Error Codes Reference

| Code | HTTP | Description | Solution |
|------|------|-------------|----------|
| `s-quotes-0001` | 200 | Quotes fetched successfully | Process listQuotes normally |
| `s-subs-0001` | 200 | Instruments subscribed successfully | Parse listQuotes for initial snapshot |
| `e-session-0002` | 400 | Instrument Already Subscribed ! | Fallback to getQuote for snapshot |
| `e-auth-0001` | 401 | Authentication failed | Re-login and get new token |
| `e-params-0001` | 400 | Missing required parameter | Check publishFormat, instruments array |

---

## Critical Issues & Solutions

### Issue 1: Token Mismatch (SOLVED ✅)

**Problem**: Market watch showing token 28085 instead of 49508

**Root Cause**: Using `qHash(symbol+expiry+strike)` instead of actual `exchangeInstrumentID`

**Solution**:
```cpp
// OLD (WRONG)
int token = qHash(symbol + expiry + QString::number(strike));

// NEW (CORRECT)
int token = instrument.exchangeInstrumentID;  // From cache
```

### Issue 2: Missing Fields (SOLVED ✅)

**Problem**: instrumentType, expiry, strike, optionType, series not populated

**Root Cause**: Signal passing individual QString parameters, losing data

**Solution**: Changed signal to pass complete `InstrumentData` structure
```cpp
// OLD
Signal: addToWatchRequested(QString symbol, QString expiry, double strike, QString optionType, int token)

// NEW
Signal: addToWatchRequested(const InstrumentData& instrument)
```

### Issue 3: WebSocket 404 Error (SOLVED ✅)

**Problem**: WebSocket connection failing with 404

**Root Cause**: Wrong URL path and protocol

**Solution**:
```cpp
// OLD
ws://host/marketdata/feed?token=xxx

// NEW (Socket.IO format)
ws://host/apimarketdata/socket.io/?EIO=3&transport=websocket&token=xxx&userID=xxx&publishFormat=JSON&broadcastMode=Partial
```

### Issue 4: Tick Data Not Parsing (SOLVED ✅)

**Problem**: processTickData expecting flat JSON, but XTS sends nested

**Root Cause**: Touchline data is nested inside JSON

**Solution**:
```cpp
void processTickData(const QJsonObject &json) {
    QJsonObject touchline = json["Touchline"].toObject();
    if (!touchline.isEmpty()) {
        tick.lastTradedPrice = touchline["LastTradedPrice"].toDouble();
        tick.close = touchline["Close"].toDouble();
        // ... parse nested structure
    }
}
```

### Issue 5: getQuote Failing (SOLVED ✅)

**Problem**: getQuote returns HTTP 400 Bad Request

**Root Cause**: Missing required `publishFormat: "JSON"` parameter in request

**Solution**: Added publishFormat parameter to all getQuote calls
```cpp
QJsonObject reqObj;
reqObj["instruments"] = instruments;
reqObj["xtsMessageCode"] = 1502;
reqObj["publishFormat"] = "JSON";  // REQUIRED - was missing
```

**Test Results**:
- ✅ Single getQuote: HTTP 200, returns complete snapshot (LTP=25997.1, Close=26108.7, Volume=3.43M)
- ✅ Multiple getQuote: HTTP 200, returns array with quotes for all instruments
- ✅ Message code 1501: HTTP 200, touchline only
- ✅ Message code 1502: HTTP 200, full market depth with Bids/Asks

**Enhanced Implementation**:
```cpp
// Added overloaded method for multiple instruments
void getQuote(const QVector<int64_t> &instrumentIDs, int exchangeSegment,
             std::function<void(bool, const QVector<QJsonObject>&, const QString&)> callback);

// Usage
QVector<int64_t> tokens = {49543, 59175};  // NIFTY + BANKNIFTY
m_xtsClient->getQuote(tokens, 2, [](bool ok, const QVector<QJsonObject>& quotes, const QString& msg) {
    for (const auto& quote : quotes) {
        // Process each quote
    }
});
```

### Issue 6: Re-Subscription Empty Response (BY DESIGN ✅)

**Behavior**: When token already subscribed, XTS returns empty `listQuotes`

**Impact**: Second market watch window won't get initial price immediately

**Solution**: Rely on WebSocket for updates
```cpp
// First MW window: Gets snapshot from subscribe
// Second MW window: Gets updates from WebSocket ticks
// Both receive real-time updates via Socket.IO
```

---

## Testing Scenarios

### Scenario A: Add Single Instrument (First Time)

**Steps**:
1. Search "NIFTY" in ScripBar
2. Click "Add to Watch"
3. Observe MarketWatch window

**Expected**:
- ✅ NIFTY appears in table immediately
- ✅ Token = 49543 (actual exchangeInstrumentID)
- ✅ All fields populated (instrumentType, expiry, strike, optionType)
- ✅ Subscribe API called
- ✅ Initial price appears from subscribe response
- ✅ Live updates flow via WebSocket

**Logs to Check**:
```
Adding to watch: "NIFTY" token: 49543
Successfully added "NIFTY" to market watch with token 49543
Subscription response: {...listQuotes:["{...Touchline...}"]...}
✅ Subscribed to 1 instruments
[MainWindow] Subscribed instrument 49543 - will receive live updates
Socket.IO event: "1502-json-partial"
```

### Scenario B: Add Same Instrument to Second Window

**Steps**:
1. Create second Market Watch window (Ctrl+M)
2. Add same "NIFTY" instrument

**Expected**:
- ✅ NIFTY added to second window
- ✅ Subscribe returns success but empty listQuotes
- ⚠️ No immediate price (expected)
- ✅ Prices update via WebSocket ticks
- ✅ Both windows update simultaneously

**Logs to Check**:
```
Adding to watch: "NIFTY" token: 49543
Subscription response: {...listQuotes:[]...}  ← Empty
✅ Subscribed to 1 instruments
Socket.IO event: "1502-json-partial"
updatePrice: token=49543, ltp=25960  ← Both windows updated
```

### Scenario C: Duplicate Detection

**Steps**:
1. Add "NIFTY" to Market Watch
2. Try adding same "NIFTY" again to same window

**Expected**:
- ❌ Second add rejected
- ⚠️ Warning logged: "Duplicate token detected: 49543"
- ✅ UI shows no change

### Scenario D: Multiple Instruments

**Steps**:
1. Add NIFTY (token 49543)
2. Add BANKNIFTY (token 59175)
3. Add RELIANCE (token 2885)

**Expected**:
- ✅ All three appear in table
- ✅ Each subscribes separately
- ✅ Live updates for all three
- ✅ TokenAddressBook tracks: {49543→[0], 59175→[1], 2885→[2]}

### Scenario E: Price Updates

**Steps**:
1. Add instrument
2. Wait for live tick updates

**Expected**:
- ✅ LTP updates every few seconds
- ✅ Change and %Change calculated correctly
- ✅ Color coding: green (up), red (down)
- ✅ O(1) lookup via TokenAddressBook

---

## API Testing Results Summary

### Test Scenarios Executed (Dec 16, 2025)

**Test 1: Single getQuote with Message Code 1502**
```bash
Result: HTTP 200 OK
Response: Complete market depth with touchline
LTP: 25997.1, Close: 26108.7, Volume: 3,432,975
Depth: 5 levels of Bids and Asks with order counts
Status: ✅ WORKING
```

**Test 2: Single getQuote with Message Code 1501**
```bash
Result: HTTP 200 OK
Response: Touchline data only (no market depth)
LTP: 25997.1, Close: 26108.7
Status: ✅ WORKING
```

**Test 3: Multiple getQuote (NIFTY + BANKNIFTY)**
```bash
Result: HTTP 200 OK
Response: Array with 2 quotes
NIFTY: LTP=25997.1
BANKNIFTY: LTP=60080.0
Status: ✅ WORKING
```

**Test 4: Subscribe Fresh Instrument (First Time)**
```bash
Result: HTTP 200 OK
Response: listQuotes contains initial snapshot (nested JSON string)
Status: ✅ WORKING
```

**Test 5: Subscribe Already-Subscribed Instrument**
```bash
Result: HTTP 400 Bad Request
Error Code: e-session-0002
Description: "Instrument Already Subscribed !"
Response: listQuotes is empty array
Status: ✅ EXPECTED - Fallback to getQuote working
```

**Test 6: WebSocket Live Updates (1502-json-partial)**
```bash
Result: Continuous stream
Format: Socket.IO packet with nested JSON
Frequency: 1-2 updates per second (during market hours)
Status: ✅ WORKING
```

### Complete Test Script Output

See `test_api_advanced.sh` for reproducible tests. All scenarios passing as of Dec 16, 2025.

---

## Future Improvements

### 1. getQuote Implementation (COMPLETED ✅)

**Status**: PRODUCTION READY

**Completed Items**:
- [x] Added required `publishFormat: "JSON"` parameter
- [x] Tested message codes 1501 (touchline) and 1502 (market depth)
- [x] Tested multiple exchange segments (NSEFO confirmed working)
- [x] Added support for multiple instruments in single call
- [x] Integrated as fallback for "Already Subscribed" errors

### 2. Handle Re-Subscription (IMPLEMENTED ✅)

**Current Solution**: Automatic fallback to getQuote on e-session-0002 error

**Implementation**:
```cpp
if (httpStatus == 400 && obj["code"].toString() == "e-session-0002") {
    // Already subscribed - fetch snapshot via getQuote
    getQuote(instrumentID, exchangeSegment, [this](bool ok, const QJsonObject& quote, const QString& msg) {
        if (ok) {
            processTickData(quote);  // Update UI immediately
        }
    });
    callback(true, "Already subscribed (snapshot fetched)");
}
```

**Status**: ✅ WORKING - No more "0.00 flash" issue

### 3. Global Price Cache (FUTURE ENHANCEMENT)

**Current**: Each re-subscription calls getQuote API

**Improvement**: Maintain in-memory cache of latest ticks

```cpp
// Proposed solution
if (listQuotes.isEmpty()) {
    // Token already subscribed - get last known price from cache
    XTS::Tick lastTick = m_tickCache.value(instrumentID);
    if (lastTick.lastTradedPrice > 0) {
        processTickData(lastTick);  // Use cached data
    }
}
```

### 3. Batch Subscription

**Current**: Subscribe one instrument at a time

**Improvement**: Batch multiple instruments
```cpp
// Instead of:
subscribe({49543}, 2, callback);
subscribe({59175}, 2, callback);

// Do:
subscribe({49543, 59175}, 2, callback);
```

**Benefits**:
- Fewer API calls
- Better performance
- Single response with multiple quotes

### 4. Unsubscribe Implementation

**Current**: Basic unsubscribe via WebSocket message

**Needs**:
- [ ] Test unsubscribe endpoint (DELETE method?)
- [ ] Handle subscription count tracking
- [ ] Remove from TokenAddressBook when last window closes
- [ ] Update TokenSubscriptionManager

### 5. Error Handling

**Improvements Needed**:
- [ ] Handle subscription failures gracefully
- [ ] Retry logic for failed subscriptions
- [ ] Show user-friendly errors in UI
- [ ] Handle WebSocket disconnection/reconnection
- [ ] Implement heartbeat/ping-pong monitoring

### 6. Performance Optimization

**Ideas**:
- [ ] Throttle price updates (max 10 per second per instrument)
- [ ] Batch UI updates (update all rows in one dataChanged emit)
- [ ] Use QTimer to debounce rapid ticks
- [ ] Implement virtual scrolling for large tables

### 7. Data Persistence

**Desired**:
- [ ] Save market watch layouts on close
- [ ] Restore subscriptions on app restart
- [ ] Remember column widths and order
- [ ] Persist token address book

---

## Code References

### Key Files

| File | Purpose | Lines |
|------|---------|-------|
| `src/app/ScripBar.cpp` | Instrument search UI | ~500 |
| `src/app/MainWindow.cpp` | Orchestration layer | ~1000 |
| `src/views/MarketWatchWindow.cpp` | Market watch logic | ~600 |
| `src/models/MarketWatchModel.cpp` | Data model | ~700 |
| `src/models/TokenAddressBook.cpp` | Fast token lookup | ~200 |
| `src/api/XTSMarketDataClient.cpp` | XTS API client | ~600 |
| `include/models/ScripData.h` | Data structures | ~150 |

### Signal/Slot Connections

```cpp
// ScripBar → MainWindow
connect(scripBar, &ScripBar::addToWatchRequested,
        mainWindow, &MainWindow::onAddToWatchRequested);

// XTS Client → MainWindow
connect(xtsClient, &XTSMarketDataClient::tickReceived,
        mainWindow, &MainWindow::onTickReceived);

// Model → TokenAddressBook (auto-sync)
connect(model, &MarketWatchModel::rowsInserted,
        addressBook, &TokenAddressBook::onRowsInserted);
connect(model, &MarketWatchModel::rowsRemoved,
        addressBook, &TokenAddressBook::onRowsRemoved);
```

---

## Appendices

### A. Exchange Segment Codes

| Code | Exchange | Segment |
|------|----------|---------|
| 1 | NSE | Cash Market (CM) |
| 2 | NSE | Futures & Options (FO) |
| 13 | NSE | Currency Derivatives (CD) |
| 11 | BSE | Cash Market (CM) |
| 12 | BSE | Futures & Options (FO) |
| 61 | BSE | Currency Derivatives (CD) |
| 51 | MCX | Futures & Options (FO) |

### B. Instrument Types

| Type | Description | Example |
|------|-------------|---------|
| FUTIDX | Index Future | NIFTY 26DEC2024 FUT |
| FUTSTK | Stock Future | RELIANCE 27DEC2024 FUT |
| OPTIDX | Index Option | NIFTY 26DEC2024 25000 CE |
| OPTSTK | Stock Option | RELIANCE 27DEC2024 2800 PE |
| EQUITY | Cash Equity | RELIANCE EQ |
| FUTCUR | Currency Future | USDINR 27DEC2024 FUT |
| FUTCOM | Commodity Future | GOLD 5DEC2024 FUT |

### C. Message Codes

| Code | Description | Data Returned |
|------|-------------|---------------|
| 1501 | Touchline | LTP, OHLC, Bid/Ask (basic) |
| 1502 | Market Depth | Full order book (5 levels) |
| 1510 | Candle Data | OHLC candles (historical) |
| 1512 | Open Interest | OI and OI change data |

### D. Troubleshooting Checklist

**No price updates appearing**:
- [ ] Check WebSocket connection status
- [ ] Verify token subscribed (check logs)
- [ ] Confirm Socket.IO events arriving
- [ ] Check processTickData() parsing
- [ ] Verify TokenAddressBook has correct mapping
- [ ] Check updatePrice() being called

**Duplicate add not prevented**:
- [ ] Verify hasDuplicate() logic
- [ ] Check TokenAddressBook state
- [ ] Ensure token field populated correctly

**Wrong token displayed**:
- [ ] Check getCurrentInstrument() returns correct data
- [ ] Verify cache populated from search results
- [ ] Ensure not using qHash() for token

**Fields not populated**:
- [ ] Verify InstrumentData signal carries all fields
- [ ] Check ScripData mapping in MainWindow
- [ ] Confirm addScripFromContract() receives all data

---

## Summary

The add-scrip-to-market-watch flow involves 8+ components working together:

1. **ScripBar**: Search → cache → emit InstrumentData
2. **MainWindow**: Orchestrate → map data → call APIs
3. **MarketWatch**: Validate → add to model → track tokens
4. **Model**: Store data → format display
5. **TokenAddressBook**: Fast O(1) lookups for updates
6. **XTS Client**: Subscribe → parse responses → emit ticks
7. **WebSocket**: Real-time tick stream
8. **TokenSubscriptionManager**: Global subscription tracking

**Key Insight**: Subscribe API provides initial snapshot on first subscription only. Re-subscriptions return empty listQuotes, requiring WebSocket for updates.

**Current Blocker**: getQuote API failing (400 Bad Request) - standalone tests needed to investigate proper format.

---

**Document Status**: ✅ Complete - Reflects current implementation as of Dec 16, 2025

**Next Review**: After standalone API tests reveal getQuote behavior
