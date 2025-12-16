# Native WebSocket Implementation Strategy

**Date**: December 16, 2025  
**Purpose**: Replace Qt wrappers with native C++ for zero-latency WebSocket communication

---

## 🎯 Objective

Based on Go implementation (`trading_terminal_go/internal/services/xts/marketdata.go`), implement **native C++ WebSocket** without Qt overhead for time-critical market data operations.

---

## 📊 Performance Impact

### Qt Wrappers Overhead

**QWebSocket**:
- Qt event loop processing: ~500-2000 ns per message
- Signal/slot mechanism: ~100-300 ns per emit
- QObject overhead: ~50-100 ns per instance
- Total: **~650-2400 ns per tick**

**QTimer**:
- Event loop scheduling: ~200-500 ns per fire
- Signal/slot: ~100-300 ns
- Total: **~300-800 ns per heartbeat**

**QNetworkAccessManager**:
- Event loop: ~500-1000 ns per request
- Total: **~500-1000 ns per REST call**

### Impact on Trading

For **10,000 ticks/second**:
- Qt overhead: 6.5 - 24 μs/second
- **Annual waste: 205 - 756 seconds/year**
- Native C++: < 1 μs/second
- **Savings: 200+ seconds/year**

---

## 🔍 Go Implementation Analysis

### Key Components (from Go reference)

```go
// internal/services/xts/marketdata.go

// 1. Native WebSocket (gorilla/websocket)
conn, _, err := websocket.DefaultDialer.Dial(wsURL, headers)

// 2. Manual Engine.IO/Socket.IO parsing
func (c *MarketDataClient) handleMessage(message string) {
    if strings.HasPrefix(message, "0") {
        // Engine.IO open (handshake)
    } else if strings.HasPrefix(message, "2") {
        // Ping - respond with pong
        c.sendPong()
    } else if strings.HasPrefix(message, "3") {
        // Pong received
        c.lastPongTime = time.Now()
    } else if strings.HasPrefix(message, "42") {
        // Socket.IO event
        c.parseSocketIOEvent(message[2:])
    }
}

// 3. Native heartbeat (time.Ticker)
func (c *MarketDataClient) heartbeat() {
    ticker := time.NewTicker(25 * time.Second)
    for range ticker.C {
        c.conn.WriteMessage(websocket.TextMessage, []byte("2"))
        c.lastPingTime = time.Now()
    }
}

// 4. Connection health monitoring
func (c *MarketDataClient) GetConnectionHealth() string {
    timeSinceLastPong := time.Since(c.lastPongTime)
    if timeSinceLastPong < 40*time.Second {
        return "HEALTHY"
    } else if timeSinceLastPong < 60*time.Second {
        return "DEGRADED"
    }
    return "UNHEALTHY"
}
```

---

## 🚀 C++ Native Implementation Plan

### Phase 1: Library Selection

**Option 1: websocketpp** (Recommended)
- Header-only library
- Similar to gorilla/websocket
- Zero external dependencies (except Boost.Asio)
- Mature and well-tested

```cpp
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
```

**Option 2: Boost.Beast**
- Part of Boost 1.70+
- Modern C++14/17
- Better performance than websocketpp
- More complex API

```cpp
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
```

**Option 3: libwebsockets**
- Pure C library
- Lowest level control
- Most complex integration

### Phase 2: Protocol Implementation

Based on Go reference, implement **Engine.IO v3 + Socket.IO v2/v3**:

```cpp
class NativeWebSocketClient {
private:
    void parseMessage(const std::string& message) {
        if (message[0] == '0') {
            // Engine.IO open - parse handshake JSON
            handleHandshake(message.substr(1));
        }
        else if (message[0] == '2') {
            // Ping from server - send pong
            sendPong();
        }
        else if (message[0] == '3') {
            // Pong from server - update timestamp
            m_lastPong = std::chrono::system_clock::now();
        }
        else if (message.starts_with("42")) {
            // Socket.IO event - parse JSON array
            parseSocketIOEvent(message.substr(2));
        }
    }
    
    void sendPing() {
        m_wsClient->send("2");  // Engine.IO ping
        m_lastPing = std::chrono::system_clock::now();
    }
    
    void sendPong() {
        m_wsClient->send("3");  // Engine.IO pong
    }
    
    void sendSocketIOConnect() {
        m_wsClient->send("40/");  // Connect to root namespace
    }
};
```

### Phase 3: Heartbeat with std::thread

Replace QTimer with native thread (like Go's goroutine):

```cpp
void NativeWebSocketClient::startHeartbeat() {
    m_heartbeatRunning = true;
    m_heartbeatThread = std::thread([this]() {
        while (m_heartbeatRunning) {
            sendPing();
            
            // Sleep for 25 seconds (XTS heartbeat interval)
            std::this_thread::sleep_for(std::chrono::seconds(25));
        }
    });
}

void NativeWebSocketClient::stopHeartbeat() {
    m_heartbeatRunning = false;
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }
}
```

### Phase 4: Connection Health

Implement health monitoring (from Go reference):

```cpp
std::string NativeWebSocketClient::getHealthStatus() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    auto now = std::chrono::system_clock::now();
    auto timeSinceLastPong = now - m_lastPong;
    
    if (timeSinceLastPong < std::chrono::seconds(40)) {
        return "HEALTHY";
    } else if (timeSinceLastPong < std::chrono::seconds(60)) {
        return "DEGRADED";
    } else if (m_connected) {
        return "UNHEALTHY";
    }
    return "DISCONNECTED";
}

NativeWebSocketClient::ConnectionStats NativeWebSocketClient::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return {
        .connected = m_connected.load(),
        .reconnectCount = m_reconnectAttempts.load(),
        .messagesReceived = m_messagesReceived.load(),
        .lastPing = m_lastPing,
        .lastPong = m_lastPong,
        .subscriptions = static_cast<int>(m_subscriptions.size())
    };
}
```

---

## 📋 Migration Plan

### Step 1: Benchmark Qt vs Native

Run `benchmark_qt_vs_native` to measure actual overhead:

```bash
cd build
./benchmark_qt_vs_native
```

Expected results:
- Qt event loop: 500-2000 ns
- Native C++: 10-50 ns
- **Qt is 10-40x slower**

### Step 2: Add Native WebSocket Library

**Using websocketpp** (recommended for compatibility):

```cmake
# CMakeLists.txt
find_package(Boost REQUIRED COMPONENTS system)
find_package(websocketpp REQUIRED)

target_link_libraries(TradingTerminal PRIVATE
    websocketpp::websocketpp
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
)
```

### Step 3: Implement NativeWebSocketClient

Based on header file created, implement full client:
- Connection management
- Engine.IO/Socket.IO parsing
- Heartbeat with std::thread
- Health monitoring

### Step 4: Create Adapter Layer

Keep Qt for UI, use native for data:

```cpp
class XTSMarketDataClient {
private:
    // Use native WebSocket for market data
    NativeWebSocketClient* m_nativeWS;
    
    // Keep Qt for UI signals
    Q_OBJECT
signals:
    void tickReceived(const XTS::Tick& tick);
    
private:
    void onNativeMessage(const std::string& message) {
        // Parse tick
        XTS::Tick tick = parseTickData(message);
        
        // Emit Qt signal for UI (minimal Qt usage)
        emit tickReceived(tick);
    }
};
```

### Step 5: Benchmark Improvement

After migration, measure latency:

```bash
# Before (Qt)
Tick processing: ~2000 ns average

# After (Native)
Tick processing: ~100 ns average

# Improvement: 20x faster
```

---

## 🎯 Success Criteria

1. **Latency**: < 100 ns per tick (vs Qt's 650-2400 ns)
2. **Throughput**: 100,000+ ticks/second (vs Qt's 10,000-50,000)
3. **CPU Usage**: < 5% at 10,000 ticks/sec (vs Qt's 10-20%)
4. **Memory**: Stable at runtime (no Qt overhead)
5. **Reliability**: Same or better than Qt (based on Go reference)

---

## 📚 References

### Go Implementation
- `trading_terminal_go/internal/services/xts/marketdata.go`
- `trading_terminal_go/docs/WEBSOCKET_ENHANCEMENT_SUMMARY.md`
- `trading_terminal_go/docs/WEBSOCKET_MONITORING_GUIDE.md`

### C++ Libraries
- **websocketpp**: https://github.com/zaphoyd/websocketpp
- **Boost.Beast**: https://www.boost.org/doc/libs/1_84_0/libs/beast/doc/html/index.html
- **libwebsockets**: https://libwebsockets.org/

### Protocol Specs
- **Engine.IO v3**: https://github.com/socketio/engine.io-protocol
- **Socket.IO v2/v3**: https://socket.io/docs/v4/socket-io-protocol/

---

## 🚨 Important Notes

### When to Use Qt
- **UI components**: QWidget, QTableView, etc.
- **UI threading**: QMetaObject::invokeMethod for thread-safe UI updates
- **User interactions**: Signals/slots for button clicks, etc.

### When to Use Native C++
- **WebSocket communication**: Market data ticks
- **Timers**: Heartbeat, reconnection
- **Network I/O**: Non-UI REST API calls
- **Data processing**: Tick parsing, calculations
- **Memory operations**: Lock-free queues, ring buffers

---

## ⚡ Next Steps

1. **Run benchmark** to quantify Qt overhead
2. **Choose WebSocket library** (recommend websocketpp)
3. **Implement NativeWebSocketClient** based on Go reference
4. **Create adapter** to bridge native backend with Qt UI
5. **Test and measure** latency improvement
6. **Gradual migration**: WebSocket first, then timers, then REST API

---

**Goal**: Achieve **< 100 ns latency** for tick processing while maintaining Qt for UI.
