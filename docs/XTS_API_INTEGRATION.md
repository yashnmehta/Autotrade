# XTS API Integration Guide

## Overview
Complete integration with XTS (Symphony Fintech) API for market data and trading operations.

## API Structure

### Base URLs
```cpp
// From config.ini: https://mtrade.arhamshare.com
QString baseURL = config->getXTSUrl();

// Market Data API
QString mdBaseURL = baseURL + "/apimarketdata";
// Example: https://mtrade.arhamshare.com/apimarketdata

// Interactive API  
QString iaBaseURL = baseURL;
// Example: https://mtrade.arhamshare.com
```

## Market Data API (XTSMarketDataClient)

### Authentication
```cpp
POST /auth/login
Content-Type: application/json
Body: {
    "appKey": "2d832e8d71e1d180aee499",
    "secretKey": "Snvd485$cC",
    "source": "WEBAPI"
}
Response: {
    "type": "success",
    "result": {
        "token": "...",
        "userID": "..."
    }
}
```

### Implemented Endpoints

#### 1. Search Instruments
```cpp
GET /search/instruments?searchString=RELIANCE
Authorization: <token>

Response: {
    "type": "success",
    "result": [
        {
            "ExchangeSegment": 1,
            "ExchangeInstrumentID": 2885,
            "Name": "RELIANCE",
            "Series": "EQ"
        }
    ]
}
```

**Usage:**
```cpp
xtsClient->searchInstruments("RELIANCE", 1, [](bool success, const QVector<XTS::Instrument> &instruments, const QString &error) {
    if (success) {
        for (const auto &inst : instruments) {
            qDebug() << inst.instrumentName << inst.exchangeInstrumentID;
        }
    }
});
```

#### 2. Download Master Contracts
```cpp
POST /instruments/master
Authorization: <token>
Content-Type: application/json
Body: {
    "exchangeSegmentList": ["NSEFO", "NSECM", "BSEFO", "BSECM"]
}

Response: {
    "type": "success",
    "result": "<CSV_DATA>"
}
```

**Usage:**
```cpp
QStringList segments = {"NSEFO", "NSECM", "BSEFO", "BSECM"};
xtsClient->downloadMasterContracts(segments, [](bool success, const QString &csvData, const QString &error) {
    if (success) {
        // Save to file
        QFile file("Masters/master_contracts_latest.txt");
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream(&file) << csvData;
    }
});
```

#### 3. WebSocket Connection
```cpp
wss://domain/apimarketdata/socket.io/?token=<token>&userID=<userID>&EIO=3&transport=websocket
```

#### 4. Subscribe to Instruments
```cpp
// Send via WebSocket
{
    "a": "subscribe",
    "v": [2885, 26000],  // Instrument IDs
    "m": "full"          // full mode for complete tick data
}
```

### Exchange Segment Codes
```cpp
NSECM  = 1    // NSE Cash Market
NSEFO  = 2    // NSE F&O
NSECD  = 13   // NSE Currency Derivatives
BSECM  = 11   // BSE Cash Market
BSEFO  = 12   // BSE F&O
BSECD  = 61   // BSE Currency Derivatives
MCXFO  = 51   // MCX Futures & Options
```

## Interactive API (XTSInteractiveClient)

### Authentication
```cpp
POST /interactive/user/session
Content-Type: application/json
Body: {
    "appKey": "5820d8e017294c81d71873",
    "secretKey": "Ibvk668@NX",
    "source": "TWSAPI"
}
Response: {
    "type": "success",
    "result": {
        "token": "...",
        "userID": "...",
        "clientCodes": ["PRO11"]
    }
}
```

### Implemented Endpoints

#### 1. Get Positions
```cpp
GET /interactive/portfolio/positions?dayOrNet=DayWise
// or dayOrNet=NetWise
Authorization: <token>

Response: {
    "type": "success",
    "result": {
        "positionList": [
            {
                "exchangeSegment": "NSECM",
                "exchangeInstrumentID": 2885,
                "ProductType": "MIS",
                "NetQuantity": 100,
                "MTM": 1500.50
            }
        ]
    }
}
```

**Usage:**
```cpp
iaClient->getPositions("DayWise", [](bool success, const QVector<XTS::Position> &positions, const QString &error) {
    if (success) {
        for (const auto &pos : positions) {
            qDebug() << "Position:" << pos.exchangeSegment << pos.netQuantity << pos.mtm;
        }
    }
});
```

#### 2. Get Orders
```cpp
GET /interactive/orders
Authorization: <token>

Response: {
    "type": "success",
    "result": [
        {
            "AppOrderID": "123",
            "OrderID": "456",
            "ExchangeInstrumentID": 2885,
            "OrderSide": "BUY",
            "OrderStatus": "Filled",
            "FilledQuantity": 100
        }
    ]
}
```

#### 3. Get Trades
```cpp
GET /interactive/orders/trades
Authorization: <token>

Response: {
    "type": "success",
    "result": [
        {
            "TradeID": "T001",
            "OrderID": "456",
            "ExchangeInstrumentID": 2885,
            "TradePrice": 2500.50,
            "TradeQuantity": 100
        }
    ]
}
```

## Configuration (config.ini)

```ini
[XTS]
url = https://mtrade.arhamshare.com

[CREDENTIALS]
marketdata_appkey = 2d832e8d71e1d180aee499
marketdata_secretkey = Snvd485$cC
interective_appkey = 5820d8e017294c81d71873
interective_secretkey = Ibvk668@NX
userid = PRO11
```

## Login Flow Implementation

### LoginFlowService Phases

```cpp
Phase 1 (10-30%): Market Data API Login
├── POST /apimarketdata/auth/login
└── Store token and userID

Phase 2 (40-60%): Interactive API Login
├── POST /interactive/user/session
└── Store token, userID, clientCodes

Phase 3 (70-75%): Download Masters (optional)
├── POST /apimarketdata/instruments/master
└── Save to Masters/master_contracts_latest.txt

Phase 4 (80-90%): Connect WebSocket
├── wss://domain/apimarketdata/socket.io/...
└── Subscribe to instruments

Phase 5 (95-100%): Fetch Initial Data
├── GET /interactive/portfolio/positions
├── GET /interactive/orders
└── GET /interactive/orders/trades
```

### Usage in main.cpp

```cpp
LoginFlowService *loginService = new LoginFlowService();

loginService->setStatusCallback([](const QString &phase, const QString &message, int progress) {
    qDebug() << phase << message << progress << "%";
});

loginService->setErrorCallback([](const QString &phase, const QString &error) {
    qWarning() << "Error in" << phase << ":" << error;
});

loginService->setCompleteCallback([mainWindow, loginService]() {
    // Pass XTS clients to MainWindow
    mainWindow->setXTSClients(
        loginService->getMarketDataClient(),
        loginService->getInteractiveClient()
    );
    mainWindow->show();
});

loginService->executeLogin(
    mdAppKey, mdSecretKey,
    iaAppKey, iaSecretKey,
    loginID, downloadMasters,
    baseURL
);
```

## ScripBar Integration

### Current Implementation
```cpp
void ScripBar::populateSymbols(const QString &instrument)
{
    if (m_xtsClient && m_xtsClient->isLoggedIn()) {
        // Call XTS search API
        m_xtsClient->searchInstruments("RELIANCE", exchangeSegment,
            [this](bool success, const QVector<XTS::Instrument> &instruments, const QString &error) {
                if (success) {
                    QStringList symbols;
                    for (const auto &inst : instruments) {
                        symbols.append(inst.instrumentName);
                    }
                    m_symbolCombo->addItems(symbols);
                }
            }
        );
    } else {
        m_symbolCombo->addItem("Please login first");
    }
}
```

## Next Steps

### 1. Parse Master Contracts
Download masters contain pipe-delimited CSV:
```
NSEFO|37167|2|MANAPPURAM|MANAPPURAM26JAN372.5PE|OPTSTK|...
```

Need to implement:
- CSV parser for each exchange format
- Load into ScripMaster repository
- Index by symbol, expiry, strike for fast search

### 2. Implement Scrip Search from Masters
```cpp
ScripMaster::search(const QString &query, const QString &exchange, const QString &segment) {
    // Search in-memory master data
    // Filter by exchange segment
    // Return matching instruments
}
```

### 3. Update ScripBar to Use Masters
```cpp
void ScripBar::populateSymbols(const QString &instrument) {
    int exchangeSegment = getCurrentExchangeSegmentCode();
    QVector<Instrument> results = g_scripMaster->search(instrument, exchange, segment);
    
    for (const auto &inst : results) {
        m_symbolCombo->addItem(inst.symbol);
        m_instrumentCache.append(inst);  // Cache for expiry/strike population
    }
}
```

### 4. Populate Expiries and Strikes from Cache
Parse instrument names to extract:
- Expiry dates (e.g., "26JAN" → "26-Jan-2026")
- Strike prices (e.g., "372.5PE" → 372.5)
- Option types (PE/CE)

## Error Handling

### Common Issues

**1. "Not authenticated"**
- Ensure login() is called first
- Check token is stored correctly

**2. "Bad Request"**
- Verify endpoint paths
- Check base URL has correct suffix (/apimarketdata)
- Validate JSON request format

**3. "Not Found"**
- Endpoint path may be wrong
- Check if feature is available on your XTS version

**4. Network Timeout**
- Increase timeout: `QNetworkAccessManager::setTimeout()`
- Check firewall/proxy settings

## Testing

### Test Market Data API
```bash
curl -X POST https://mtrade.arhamshare.com/apimarketdata/auth/login \
  -H "Content-Type: application/json" \
  -d '{
    "appKey": "your_key",
    "secretKey": "your_secret",
    "source": "WEBAPI"
  }'
```

### Test Interactive API
```bash
curl -X POST https://mtrade.arhamshare.com/interactive/user/session \
  -H "Content-Type: application/json" \
  -d '{
    "appKey": "your_key",
    "secretKey": "your_secret",
    "source": "TWSAPI"
  }'
```

## References

- XTS Test Files: `reference_code/trading_terminal_go/cmd/test-xts-*`
- Market Data Test: `test-xts-marketdata-complete/main.go`
- Interactive Test: `test-xts-interactive-complete/main.go`
- XTS Documentation: Provided by Symphony Fintech
