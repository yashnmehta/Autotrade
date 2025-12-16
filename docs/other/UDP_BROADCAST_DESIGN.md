# UDP Broadcast Market Data Provider - Design Document

## Overview

Add support for direct Exchange UDP broadcast for ultra-low latency market data, alongside existing XTS API support. Provides configurable hybrid approach.

**Date**: 16 December 2025  
**Status**: Design Phase

---

## Architecture

### Provider Hierarchy

```
                    IMarketDataProvider
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
XTSMarketDataProvider  UDPBroadcastProvider  HybridMarketDataProvider
  (Socket.IO)           (UDP Multicast)       (Smart Routing)
        │                   │                   │
        │                   │                   │
   XTS Server          Exchange Feed      Routes to UDP or XTS
```

### Routing Logic (Hybrid Mode)

```
Exchange Segment → Provider Selection
─────────────────────────────────────
NSECM (1)  → UDP (if available) → Fallback to XTS
NSEFO (2)  → UDP (if available) → Fallback to XTS
NSECD (13) → UDP (if available) → Fallback to XTS
BSECM (11) → UDP (if available) → Fallback to XTS
BSEFO (12) → UDP (if available) → Fallback to XTS
BSECD (61) → UDP (if available) → Fallback to XTS
MCXFO (51) → XTS ONLY (UDP not developed yet)
```

---

## Configuration

### config.json Format

```json
{
  "marketData": {
    "mode": "hybrid",  // "xts" | "udp" | "hybrid"
    
    "xts": {
      "baseURL": "http://192.168.102.9:3000/apimarketdata",
      "enabled": true
    },
    
    "udp": {
      "enabled": true,
      "exchanges": {
        "NSE": {
          "enabled": true,
          "multicastGroup": "239.1.1.1",
          "port": 9001,
          "protocol": "binary"  // or "fix"
        },
        "BSE": {
          "enabled": true,
          "multicastGroup": "239.1.1.2",
          "port": 9002,
          "protocol": "binary"
        },
        "MCX": {
          "enabled": false,
          "note": "UDP broadcast not yet developed for MCX"
        }
      }
    },
    
    "hybrid": {
      "preferUDP": true,
      "fallbackToXTS": true,
      "udpTimeout": 5000  // ms - switch to XTS if no UDP data
    }
  }
}
```

---

## Implementation

### 1. IMarketDataProvider Interface (Enhanced)

```cpp
// include/api/IMarketDataProvider.h
enum class ProviderType {
    XTS,
    UDP,
    Hybrid
};

struct ProviderCapabilities {
    bool supportsREST;
    bool supportsWebSocket;
    bool supportsUDP;
    QVector<int> supportedExchanges;  // Exchange segments
    int averageLatencyMs;
};

class IMarketDataProvider : public QObject {
    Q_OBJECT
public:
    virtual ~IMarketDataProvider() = default;
    
    // Identification
    virtual ProviderType type() const = 0;
    virtual QString name() const = 0;
    virtual ProviderCapabilities capabilities() const = 0;
    
    // Connection
    virtual void connect(const QJsonObject &config,
                        std::function<void(bool)> callback) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    
    // Subscription
    virtual void subscribe(const QVector<int64_t> &tokens,
                          int exchangeSegment,
                          std::function<void(bool, const QJsonArray&)> callback) = 0;
    virtual void unsubscribe(const QVector<int64_t> &tokens,
                            std::function<void(bool)> callback) = 0;
    
    // Quote retrieval
    virtual void getQuote(int64_t token,
                         int exchangeSegment,
                         std::function<void(bool, const Quote&)> callback) = 0;
    
signals:
    void tickReceived(const Quote &quote);
    void connectionStateChanged(bool connected);
    void errorOccurred(const QString &error);
    void latencyMeasured(int latencyMs);  // NEW - for monitoring
};
```

---

### 2. UDPBroadcastProvider Implementation

```cpp
// include/api/UDPBroadcastProvider.h
#include <QUdpSocket>
#include <QHostAddress>
#include "IMarketDataProvider.h"

class UDPBroadcastProvider : public IMarketDataProvider {
    Q_OBJECT
public:
    explicit UDPBroadcastProvider(QObject *parent = nullptr);
    ~UDPBroadcastProvider() override;
    
    // IMarketDataProvider implementation
    ProviderType type() const override { return ProviderType::UDP; }
    QString name() const override { return "Exchange UDP Broadcast"; }
    ProviderCapabilities capabilities() const override;
    
    void connect(const QJsonObject &config,
                std::function<void(bool)> callback) override;
    void disconnect() override;
    bool isConnected() const override { return m_connected; }
    
    void subscribe(const QVector<int64_t> &tokens, int exchangeSegment,
                  std::function<void(bool, const QJsonArray&)> callback) override;
    void unsubscribe(const QVector<int64_t> &tokens,
                    std::function<void(bool)> callback) override;
    
    void getQuote(int64_t token, int exchangeSegment,
                 std::function<void(bool, const Quote&)> callback) override;
    
private slots:
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    
private:
    // Configuration per exchange
    struct ExchangeConfig {
        bool enabled;
        QHostAddress multicastGroup;
        quint16 port;
        QString protocol;  // "binary" or "fix"
    };
    
    QMap<int, ExchangeConfig> m_exchangeConfigs;  // ExchangeSegment -> Config
    QMap<int, QUdpSocket*> m_sockets;  // ExchangeSegment -> Socket
    QSet<int64_t> m_subscribedTokens;
    QMap<int64_t, int> m_tokenToExchange;  // Token -> ExchangeSegment mapping
    bool m_connected;
    
    // Parsing methods
    Quote parseNSEPacket(const QByteArray &data);
    Quote parseBSEPacket(const QByteArray &data);
    
    // UDP setup
    bool initUDPSocket(int exchangeSegment, const ExchangeConfig &config);
    void joinMulticastGroup(QUdpSocket *socket, const QHostAddress &group);
    
    // Token filtering
    bool isTokenSubscribed(int64_t token) const;
    int getExchangeForToken(int64_t token) const;
};
```

```cpp
// src/api/UDPBroadcastProvider.cpp
void UDPBroadcastProvider::connect(const QJsonObject &config,
                                   std::function<void(bool)> callback) {
    qDebug() << "[UDPBroadcast] Initializing...";
    
    QJsonObject udpConfig = config["udp"].toObject();
    QJsonObject exchanges = udpConfig["exchanges"].toObject();
    
    // Setup NSE
    if (exchanges.contains("NSE")) {
        QJsonObject nseConfig = exchanges["NSE"].toObject();
        if (nseConfig["enabled"].toBool()) {
            ExchangeConfig cfg;
            cfg.enabled = true;
            cfg.multicastGroup = QHostAddress(nseConfig["multicastGroup"].toString());
            cfg.port = nseConfig["port"].toInt();
            cfg.protocol = nseConfig["protocol"].toString();
            
            m_exchangeConfigs[1] = cfg;  // NSECM
            m_exchangeConfigs[2] = cfg;  // NSEFO
            m_exchangeConfigs[13] = cfg; // NSECD
            
            if (!initUDPSocket(1, cfg)) {
                callback(false);
                return;
            }
        }
    }
    
    // Setup BSE
    if (exchanges.contains("BSE")) {
        QJsonObject bseConfig = exchanges["BSE"].toObject();
        if (bseConfig["enabled"].toBool()) {
            ExchangeConfig cfg;
            cfg.enabled = true;
            cfg.multicastGroup = QHostAddress(bseConfig["multicastGroup"].toString());
            cfg.port = bseConfig["port"].toInt();
            cfg.protocol = bseConfig["protocol"].toString();
            
            m_exchangeConfigs[11] = cfg;  // BSECM
            m_exchangeConfigs[12] = cfg;  // BSEFO
            m_exchangeConfigs[61] = cfg;  // BSECD
            
            if (!initUDPSocket(11, cfg)) {
                callback(false);
                return;
            }
        }
    }
    
    m_connected = true;
    qDebug() << "[UDPBroadcast] Connected to" << m_sockets.size() << "exchange feeds";
    callback(true);
}

bool UDPBroadcastProvider::initUDPSocket(int exchangeSegment, const ExchangeConfig &config) {
    QUdpSocket *socket = new QUdpSocket(this);
    
    // Bind to port
    if (!socket->bind(QHostAddress::AnyIPv4, config.port, QUdpSocket::ShareAddress)) {
        qCritical() << "[UDPBroadcast] Failed to bind to port" << config.port;
        delete socket;
        return false;
    }
    
    // Join multicast group
    if (!socket->joinMulticastGroup(config.multicastGroup)) {
        qCritical() << "[UDPBroadcast] Failed to join multicast group" << config.multicastGroup;
        delete socket;
        return false;
    }
    
    connect(socket, &QUdpSocket::readyRead, this, &UDPBroadcastProvider::onReadyRead);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QUdpSocket::error),
            this, &UDPBroadcastProvider::onError);
    
    m_sockets[exchangeSegment] = socket;
    qDebug() << "[UDPBroadcast] Listening on" << config.multicastGroup << ":" << config.port;
    return true;
}

void UDPBroadcastProvider::onReadyRead() {
    QUdpSocket *socket = qobject_cast<QUdpSocket*>(sender());
    if (!socket) return;
    
    while (socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(socket->pendingDatagramSize());
        
        QHostAddress sender;
        quint16 senderPort;
        socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        
        // Measure latency (packet should have timestamp)
        auto receiveTime = QDateTime::currentMSecsSinceEpoch();
        
        // Determine exchange from socket
        int exchangeSegment = m_sockets.key(socket, -1);
        if (exchangeSegment == -1) continue;
        
        // Parse based on exchange
        Quote quote;
        if (exchangeSegment >= 1 && exchangeSegment <= 13) {
            quote = parseNSEPacket(datagram);
        } else if (exchangeSegment >= 11 && exchangeSegment <= 61) {
            quote = parseBSEPacket(datagram);
        }
        
        // Filter: only emit if token is subscribed
        if (isTokenSubscribed(quote.token)) {
            // Calculate latency
            auto packetTime = quote.timestamp;  // From packet
            int latency = receiveTime - packetTime;
            emit latencyMeasured(latency);
            
            emit tickReceived(quote);
        }
    }
}

void UDPBroadcastProvider::subscribe(const QVector<int64_t> &tokens,
                                     int exchangeSegment,
                                     std::function<void(bool, const QJsonArray&)> callback) {
    // UDP is broadcast - we just add tokens to filter list
    for (int64_t token : tokens) {
        m_subscribedTokens.insert(token);
        m_tokenToExchange[token] = exchangeSegment;
    }
    
    qDebug() << "[UDPBroadcast] Subscribed" << tokens.size() 
             << "tokens for exchange" << exchangeSegment;
    
    // No initial snapshot from UDP (broadcast only has live data)
    callback(true, QJsonArray());
}

Quote UDPBroadcastProvider::parseNSEPacket(const QByteArray &data) {
    // NSE binary packet format (adjust based on actual protocol)
    Quote quote;
    
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // Example parsing (actual format depends on NSE specification)
    quint32 token;
    quint64 timestamp;
    qint32 ltp;  // Price in paise
    qint32 volume;
    qint32 bidPrice, askPrice;
    
    stream >> token >> timestamp >> ltp >> volume >> bidPrice >> askPrice;
    
    quote.token = token;
    quote.timestamp = timestamp;
    quote.ltp = ltp / 100.0;  // Convert paise to rupees
    quote.volume = volume;
    quote.bidPrice = bidPrice / 100.0;
    quote.askPrice = askPrice / 100.0;
    
    return quote;
}

Quote UDPBroadcastProvider::parseBSEPacket(const QByteArray &data) {
    // BSE binary packet format (adjust based on actual protocol)
    // Similar to NSE but may have different field order/sizes
    return parseNSEPacket(data);  // Placeholder
}
```

---

### 3. HybridMarketDataProvider Implementation

```cpp
// include/api/HybridMarketDataProvider.h
class HybridMarketDataProvider : public IMarketDataProvider {
    Q_OBJECT
public:
    explicit HybridMarketDataProvider(QObject *parent = nullptr);
    ~HybridMarketDataProvider() override;
    
    // Set underlying providers
    void setProviders(UDPBroadcastProvider *udp, XTSMarketDataProvider *xts);
    
    // IMarketDataProvider implementation
    ProviderType type() const override { return ProviderType::Hybrid; }
    QString name() const override { return "Hybrid (UDP + XTS)"; }
    
    void connect(const QJsonObject &config,
                std::function<void(bool)> callback) override;
    void subscribe(const QVector<int64_t> &tokens, int exchangeSegment,
                  std::function<void(bool, const QJsonArray&)> callback) override;
    
private:
    UDPBroadcastProvider *m_udpProvider;
    XTSMarketDataProvider *m_xtsProvider;
    QJsonObject m_config;
    
    // Routing decision
    bool shouldUseUDP(int exchangeSegment) const;
    IMarketDataProvider* selectProvider(int exchangeSegment) const;
    
    // Fallback management
    QMap<int64_t, QTimer*> m_fallbackTimers;  // Token -> Timeout timer
    void startFallbackTimer(int64_t token, int exchangeSegment);
    void switchToXTSFallback(int64_t token, int exchangeSegment);
};
```

```cpp
// src/api/HybridMarketDataProvider.cpp
void HybridMarketDataProvider::subscribe(const QVector<int64_t> &tokens,
                                         int exchangeSegment,
                                         std::function<void(bool, const QJsonArray&)> callback) {
    IMarketDataProvider *provider = selectProvider(exchangeSegment);
    
    qDebug() << "[Hybrid] Routing exchange" << exchangeSegment 
             << "to" << provider->name();
    
    if (provider == m_udpProvider) {
        // Subscribe via UDP
        provider->subscribe(tokens, exchangeSegment, callback);
        
        // Start fallback timer (switch to XTS if no data within timeout)
        if (m_config["hybrid"]["fallbackToXTS"].toBool()) {
            for (int64_t token : tokens) {
                startFallbackTimer(token, exchangeSegment);
            }
        }
    } else {
        // Subscribe via XTS
        provider->subscribe(tokens, exchangeSegment, callback);
    }
}

IMarketDataProvider* HybridMarketDataProvider::selectProvider(int exchangeSegment) const {
    // Check config mode
    QString mode = m_config["marketData"]["mode"].toString();
    
    if (mode == "xts") {
        return m_xtsProvider;
    } else if (mode == "udp") {
        return shouldUseUDP(exchangeSegment) ? m_udpProvider : m_xtsProvider;
    } else {  // hybrid
        bool preferUDP = m_config["hybrid"]["preferUDP"].toBool();
        bool udpAvailable = shouldUseUDP(exchangeSegment) && 
                           m_udpProvider->isConnected();
        
        if (preferUDP && udpAvailable) {
            return m_udpProvider;
        } else {
            return m_xtsProvider;
        }
    }
}

bool HybridMarketDataProvider::shouldUseUDP(int exchangeSegment) const {
    // NSE and BSE have UDP, MCX doesn't (not yet developed)
    switch (exchangeSegment) {
        case 1:   // NSECM
        case 2:   // NSEFO
        case 13:  // NSECD
        case 11:  // BSECM
        case 12:  // BSEFO
        case 61:  // BSECD
            return true;
        case 51:  // MCXFO - UDP not available
        default:
            return false;
    }
}

void HybridMarketDataProvider::startFallbackTimer(int64_t token, int exchangeSegment) {
    int timeout = m_config["hybrid"]["udpTimeout"].toInt(5000);
    
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(timeout);
    
    connect(timer, &QTimer::timeout, this, [this, token, exchangeSegment]() {
        qWarning() << "[Hybrid] No UDP data for token" << token 
                   << "- switching to XTS fallback";
        switchToXTSFallback(token, exchangeSegment);
    });
    
    // Connect to UDP tick signal to cancel timer
    connect(m_udpProvider, &IMarketDataProvider::tickReceived, 
            this, [this, token, timer](const Quote &quote) {
        if (quote.token == token && timer->isActive()) {
            timer->stop();
            m_fallbackTimers.remove(token);
            delete timer;
        }
    });
    
    m_fallbackTimers[token] = timer;
    timer->start();
}

void HybridMarketDataProvider::switchToXTSFallback(int64_t token, int exchangeSegment) {
    // Subscribe via XTS as fallback
    m_xtsProvider->subscribe({token}, exchangeSegment, [token](bool ok, const QJsonArray&) {
        if (ok) {
            qDebug() << "[Hybrid] Fallback to XTS successful for token" << token;
        } else {
            qWarning() << "[Hybrid] Fallback to XTS failed for token" << token;
        }
    });
}
```

---

## Usage

### Application Initialization

```cpp
// main.cpp or MainWindow.cpp
void MainWindow::initializeMarketData() {
    // Load config
    QFile configFile("config.json");
    configFile.open(QIODevice::ReadOnly);
    QJsonObject config = QJsonDocument::fromJson(configFile.readAll()).object();
    
    QString mode = config["marketData"]["mode"].toString();
    
    if (mode == "xts") {
        // XTS only
        m_provider = new XTSMarketDataProvider(this);
    } else if (mode == "udp") {
        // UDP only
        m_provider = new UDPBroadcastProvider(this);
    } else {  // hybrid
        // Create both providers
        UDPBroadcastProvider *udp = new UDPBroadcastProvider(this);
        XTSMarketDataProvider *xts = new XTSMarketDataProvider(this);
        
        // Create hybrid provider
        HybridMarketDataProvider *hybrid = new HybridMarketDataProvider(this);
        hybrid->setProviders(udp, xts);
        
        m_provider = hybrid;
    }
    
    // Create service with selected provider
    m_marketDataService = new MarketDataService(m_provider, this);
    
    // Connect provider
    m_provider->connect(config, [this](bool success) {
        if (success) {
            qDebug() << "Market data provider connected:" << m_provider->name();
            emit marketDataReady();
        } else {
            qCritical() << "Failed to connect market data provider";
        }
    });
}
```

---

## Testing Strategy

### 1. Unit Tests

```cpp
TEST(UDPBroadcastProvider, ParseNSEPacket) {
    UDPBroadcastProvider provider;
    
    // Create mock packet
    QByteArray packet = createMockNSEPacket(12345, 25960.0, 1000);
    
    Quote quote = provider.parseNSEPacket(packet);
    
    EXPECT_EQ(quote.token, 12345);
    EXPECT_DOUBLE_EQ(quote.ltp, 25960.0);
    EXPECT_EQ(quote.volume, 1000);
}

TEST(HybridProvider, RoutingLogic) {
    HybridMarketDataProvider hybrid;
    
    // NSE should route to UDP
    EXPECT_EQ(hybrid.selectProvider(2), udpProvider);
    
    // MCX should route to XTS
    EXPECT_EQ(hybrid.selectProvider(51), xtsProvider);
}

TEST(HybridProvider, FallbackToXTS) {
    // Test that after timeout without UDP data, switches to XTS
}
```

### 2. Integration Tests

```cpp
TEST_F(MarketDataIntegrationTest, HybridMode_NSE_UsesUDP) {
    // Configure hybrid mode
    QJsonObject config = loadTestConfig("hybrid");
    
    // Initialize
    initializeMarketData(config);
    
    // Subscribe to NSE instrument
    addInstrumentToWatch(createNSEInstrument());
    
    // Verify UDP provider was used
    EXPECT_TRUE(udpProviderWasUsed());
}

TEST_F(MarketDataIntegrationTest, HybridMode_MCX_UsesXTS) {
    // Subscribe to MCX instrument
    addInstrumentToWatch(createMCXInstrument());
    
    // Verify XTS provider was used (MCX UDP not available)
    EXPECT_TRUE(xtsProviderWasUsed());
}
```

### 3. Performance Tests

```cpp
TEST_F(PerformanceTest, UDP_Latency) {
    // Measure UDP packet receipt to tick emission latency
    // Expected: <1ms
}

TEST_F(PerformanceTest, XTS_vs_UDP_Comparison) {
    // Compare latency and throughput
    // UDP should be significantly faster
}
```

---

## Deployment

### Phase 1: XTS Only (Current)
- Deploy with `"mode": "xts"`
- Baseline for comparison

### Phase 2: UDP Development
- Implement UDPBroadcastProvider
- Test with NSE/BSE feeds
- Benchmark latency

### Phase 3: Hybrid Rollout
- Deploy with `"mode": "hybrid"`
- Monitor fallback frequency
- Tune timeout values

### Phase 4: Production
- Full hybrid mode for NSE/BSE
- XTS for MCX (until UDP developed)
- Auto-fallback on UDP issues

---

## Future Enhancements

### MCX UDP Support
When MCX UDP broadcast becomes available:

```cpp
// Update configuration
"MCX": {
  "enabled": true,
  "multicastGroup": "239.1.1.3",
  "port": 9003,
  "protocol": "binary"
}

// Update routing in HybridProvider
case 51:  // MCXFO - UDP now available
    return true;
```

### Advanced Features
- Packet loss detection and reporting
- Automatic provider health monitoring
- Dynamic provider switching based on latency
- Multi-path redundancy (multiple UDP streams)

---

**Document Version**: 1.0  
**Status**: Design Complete - Ready for Implementation  
**Next Steps**: Implement UDPBroadcastProvider after XTS integration stable
