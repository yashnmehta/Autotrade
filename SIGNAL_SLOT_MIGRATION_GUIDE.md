# Signal/Slot Migration Guide - Practical Examples

**Purpose**: Concrete examples showing how to migrate from callbacks to Qt signals/slots

---

## 1. XTSMarketDataClient Migration

### Before (Callback-Based):
```cpp
// Header
class XTSMarketDataClient : public QObject {
    Q_OBJECT
public:
    void login(std::function<void(bool, const QString&)> callback);
    void subscribe(const QVector<int64_t>& tokens, int segment,
                   std::function<void(bool, const QString&)> callback);
};

// Implementation
void XTSMarketDataClient::login(std::function<void(bool, const QString&)> callback) {
    std::thread([this, callback]() {
        // HTTP call...
        bool success = performLogin();
        QString error = success ? "" : "Login failed";
        
        // Execute callback (on HTTP thread!)
        if (callback) {
            callback(success, error);
        }
    }).detach();
}

// Usage (complicated!)
xtsClient->login([this](bool success, const QString& error) {
    // This runs on HTTP thread!
    if (success) {
        // Need to marshal to UI thread manually
        QMetaObject::invokeMethod(this, [this]() {
            statusLabel->setText("Logged in");
        }, Qt::QueuedConnection);
    }
});
```

### After (Signal/Slot-Based):
```cpp
// Header
class XTSMarketDataClient : public QObject {
    Q_OBJECT
public:
    void login();  // No callback parameter
    void subscribe(const QVector<int64_t>& tokens, int segment);
    
signals:
    void loginCompleted(bool success, const QString& error);
    void subscriptionCompleted(bool success, const QString& error);
    
private:
    void performLoginAsync();
};

// Implementation
void XTSMarketDataClient::login() {
    // Spawn thread for HTTP call
    std::thread([this]() {
        bool success = performLogin();
        QString error = success ? "" : "Login failed";
        
        // Emit signal (automatically queued to receivers' threads!)
        emit loginCompleted(success, error);
    }).detach();
}

// Usage (clean and simple!)
// One-time setup
connect(xtsClient, &XTSMarketDataClient::loginCompleted,
        this, [this](bool success, const QString& error) {
    // This automatically runs on UI thread!
    if (success) {
        statusLabel->setText("Logged in");
    } else {
        showError(error);
    }
});

// Trigger login
xtsClient->login();
```

**Benefits**:
- ✅ No manual thread marshalling
- ✅ Cleaner separation: trigger vs. response handling
- ✅ Type-safe at compile time
- ✅ Can connect multiple slots to one signal
- ✅ Automatic disconnection when receiver is destroyed

---

## 2. XTSInteractiveClient Migration

### Before (Callback-Based):
```cpp
// Header
class XTSInteractiveClient : public QObject {
    Q_OBJECT
public:
    void placeOrder(const XTS::OrderRequest& request,
                    std::function<void(bool, const XTS::OrderResponse&, const QString&)> callback);
    void modifyOrder(const QString& appOrderID, const XTS::ModifyRequest& request,
                     std::function<void(bool, const QString&)> callback);
};

// Implementation
void XTSInteractiveClient::placeOrder(
    const XTS::OrderRequest& request,
    std::function<void(bool, const XTS::OrderResponse&, const QString&)> callback) {
    
    std::thread([this, request, callback]() {
        auto response = httpPost("/order", serializeOrder(request));
        
        bool success = response.statusCode == 200;
        XTS::OrderResponse order;
        QString error;
        
        if (success) {
            order = parseOrderResponse(response.body);
        } else {
            error = response.error;
        }
        
        if (callback) {
            callback(success, order, error);
        }
    }).detach();
}

// Usage (messy!)
xtsInteractive->placeOrder(orderRequest, 
    [this](bool success, const XTS::OrderResponse& order, const QString& error) {
        // On HTTP thread!
        QMetaObject::invokeMethod(this, [this, success, order, error]() {
            // Finally on UI thread
            if (success) {
                orderBook->addOrder(order);
                showNotification("Order placed: " + order.appOrderID);
            } else {
                showError(error);
            }
        }, Qt::QueuedConnection);
    });
```

### After (Signal/Slot-Based):
```cpp
// Header
class XTSInteractiveClient : public QObject {
    Q_OBJECT
public:
    void placeOrder(const XTS::OrderRequest& request);
    void modifyOrder(const QString& appOrderID, const XTS::ModifyRequest& request);
    
signals:
    void orderPlaced(bool success, const XTS::OrderResponse& order, const QString& error);
    void orderModified(bool success, const QString& appOrderID, const QString& error);
    void orderCancelled(bool success, const QString& appOrderID, const QString& error);
};

// Implementation
void XTSInteractiveClient::placeOrder(const XTS::OrderRequest& request) {
    std::thread([this, request]() {
        auto response = httpPost("/order", serializeOrder(request));
        
        bool success = response.statusCode == 200;
        XTS::OrderResponse order;
        QString error;
        
        if (success) {
            order = parseOrderResponse(response.body);
        } else {
            error = response.error;
        }
        
        // Emit signal (Qt handles thread safety!)
        emit orderPlaced(success, order, error);
    }).detach();
}

// Usage (clean!)
// Setup (once in constructor or initialization)
connect(xtsInteractive, &XTSInteractiveClient::orderPlaced,
        this, &OrderBookWindow::onOrderPlaced);

// Trigger order
xtsInteractive->placeOrder(orderRequest);

// Handler (runs on UI thread automatically)
void OrderBookWindow::onOrderPlaced(bool success, 
                                     const XTS::OrderResponse& order, 
                                     const QString& error) {
    if (success) {
        m_orderModel->addOrder(order);
        showNotification("Order placed: " + order.appOrderID);
    } else {
        showError(error);
    }
}
```

**Benefits**:
- ✅ Separates request initiation from response handling
- ✅ Easy to add multiple handlers (e.g., OrderBook + PositionWindow)
- ✅ Cleaner error handling
- ✅ No lambda capture complexity

---

## 3. Multiple Receivers Pattern

### Scenario: Order placement needs to update multiple windows

```cpp
// BuyWindow
connect(xtsInteractive, &XTSInteractiveClient::orderPlaced,
        this, &BuyWindow::onOrderPlaced);

// OrderBookWindow
connect(xtsInteractive, &XTSInteractiveClient::orderPlaced,
        this, &OrderBookWindow::onOrderPlaced);

// PositionWindow
connect(xtsInteractive, &XTSInteractiveClient::orderPlaced,
        this, &PositionWindow::onOrderPlaced);

// NotificationService
connect(xtsInteractive, &XTSInteractiveClient::orderPlaced,
        notificationService, &NotificationService::onOrderPlaced);

// Single place order call updates all windows!
xtsInteractive->placeOrder(request);
```

**With callbacks, you'd need:**
```cpp
// BAD: Manual dispatch to multiple handlers
xtsClient->placeOrder(request, [this](bool success, auto order, auto error) {
    QMetaObject::invokeMethod(buyWindow, ...);
    QMetaObject::invokeMethod(orderBookWindow, ...);
    QMetaObject::invokeMethod(positionWindow, ...);
    QMetaObject::invokeMethod(notificationService, ...);
    // Nightmare to maintain!
});
```

---

## 4. LoginFlowService Migration

### Before (Callback-Based):
```cpp
void LoginFlowService::performLogin() {
    m_marketDataClient->login([this](bool success, const QString& error) {
        // On HTTP thread
        if (success) {
            QMetaObject::invokeMethod(this, [this]() {
                // On main thread
                m_marketDataClient->connectWebSocket([this](bool wsSuccess, const QString& wsError) {
                    // On another thread!
                    if (wsSuccess) {
                        QMetaObject::invokeMethod(this, [this]() {
                            // Finally on main thread again
                            loadMasterFiles();
                        }, Qt::QueuedConnection);
                    }
                }, Qt::QueuedConnection);
            });
        }
    });
}
// Callback hell! Hard to follow flow.
```

### After (Signal/Slot-Based):
```cpp
// Setup connections (in constructor)
void LoginFlowService::setupConnections() {
    // Chain: login -> connectWS -> loadMaster
    connect(m_marketDataClient, &XTSMarketDataClient::loginCompleted,
            this, &LoginFlowService::onLoginCompleted);
    
    connect(m_marketDataClient, &XTSMarketDataClient::wsConnectionStatusChanged,
            this, &LoginFlowService::onWSConnected);
    
    connect(m_masterLoader, &MasterLoaderWorker::loadingComplete,
            this, &LoginFlowService::onMasterLoadComplete);
}

// Clean sequential flow
void LoginFlowService::performLogin() {
    emit statusChanged("Logging in...");
    m_marketDataClient->login();
}

void LoginFlowService::onLoginCompleted(bool success, const QString& error) {
    if (success) {
        emit statusChanged("Connecting to WebSocket...");
        m_marketDataClient->connectWebSocket();
    } else {
        emit loginFailed(error);
    }
}

void LoginFlowService::onWSConnected(bool connected, const QString& error) {
    if (connected) {
        emit statusChanged("Loading master files...");
        m_masterLoader->start();
    } else {
        emit loginFailed("WebSocket connection failed: " + error);
    }
}

void LoginFlowService::onMasterLoadComplete() {
    emit statusChanged("Ready");
    emit loginComplete();
}
```

**Benefits**:
- ✅ Linear, readable flow (no nesting!)
- ✅ Easy to add intermediate steps
- ✅ Each method has single responsibility
- ✅ Easier to test individual steps

---

## 5. Error Handling Pattern

### Centralized Error Handler
```cpp
class ErrorHandler : public QObject {
    Q_OBJECT
public:
    static ErrorHandler& instance();
    
public slots:
    void onMarketDataError(const QString& error);
    void onInteractiveError(const QString& error);
    void onNetworkError(const QString& error);
    
signals:
    void criticalError(const QString& title, const QString& message);
    void warning(const QString& message);
};

// Connect all error signals to centralized handler
connect(xtsMarketData, &XTSMarketDataClient::errorOccurred,
        &ErrorHandler::instance(), &ErrorHandler::onMarketDataError);

connect(xtsInteractive, &XTSInteractiveClient::errorOccurred,
        &ErrorHandler::instance(), &ErrorHandler::onInteractiveError);

// UI listens to centralized error handler
connect(&ErrorHandler::instance(), &ErrorHandler::criticalError,
        mainWindow, &MainWindow::showCriticalError);
```

---

## 6. Connection Lifetime Management

### Auto-Disconnect Pattern
```cpp
class MarketWatchWindow : public QWidget {
    Q_OBJECT
public:
    MarketWatchWindow(QWidget* parent = nullptr) : QWidget(parent) {
        // Connections automatically disconnected when 'this' is destroyed
        connect(xtsMarketData, &XTSMarketDataClient::tickReceived,
                this, &MarketWatchWindow::onTickReceived);
        
        // Can also store connection for manual disconnect
        m_loginConnection = connect(xtsMarketData, &XTSMarketDataClient::loginCompleted,
                                    this, &MarketWatchWindow::onLoginCompleted);
    }
    
    ~MarketWatchWindow() {
        // Optional: explicit disconnect (automatic if 'this' is destroyed)
        disconnect(m_loginConnection);
    }
    
private:
    QMetaObject::Connection m_loginConnection;
};
```

### Temporary Connections
```cpp
void MainWindow::waitForLogin() {
    // Connect only until first login success
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(xtsMarketData, &XTSMarketDataClient::loginCompleted,
                          this, [this, connection](bool success, const QString& error) {
        if (success) {
            proceedAfterLogin();
            disconnect(*connection);  // One-time handler
        }
    });
}
```

---

## 7. Progress Tracking Pattern

### Before (Callback Chain):
```cpp
void loadDataSequence() {
    loadNseCm([this](bool success) {
        if (success) {
            loadNseFo([this](bool success) {
                if (success) {
                    loadBseCm([this](bool success) {
                        if (success) {
                            loadBseFo([this](bool success) {
                                // Done!
                            });
                        }
                    });
                }
            });
        }
    });
}
```

### After (Signal/Slot with Progress):
```cpp
class MasterLoaderService : public QObject {
    Q_OBJECT
public:
    void startLoading();
    
signals:
    void progressUpdated(int percent, const QString& status);
    void segmentLoaded(const QString& segment);
    void loadingComplete();
    void loadingFailed(const QString& error);
    
private slots:
    void loadNextSegment();
    
private:
    QStringList m_segments{"NSE_CM", "NSE_FO", "BSE_CM", "BSE_FO"};
    int m_currentSegment = 0;
};

void MasterLoaderService::startLoading() {
    m_currentSegment = 0;
    loadNextSegment();
}

void MasterLoaderService::loadNextSegment() {
    if (m_currentSegment >= m_segments.size()) {
        emit loadingComplete();
        return;
    }
    
    QString segment = m_segments[m_currentSegment];
    int progress = (m_currentSegment * 100) / m_segments.size();
    
    emit progressUpdated(progress, "Loading " + segment);
    
    // Async load
    loadSegmentAsync(segment, [this, segment](bool success) {
        if (success) {
            emit segmentLoaded(segment);
            m_currentSegment++;
            QTimer::singleShot(0, this, &MasterLoaderService::loadNextSegment);
        } else {
            emit loadingFailed("Failed to load " + segment);
        }
    });
}

// Usage
connect(masterLoader, &MasterLoaderService::progressUpdated,
        progressDialog, &QProgressDialog::setValue);
connect(masterLoader, &MasterLoaderService::loadingComplete,
        this, &MainWindow::onLoadingComplete);
```

---

## 8. Testing Advantages

### Signal/Slot Testing
```cpp
class TestXTSClient : public QObject {
    Q_OBJECT
    
private slots:
    void testLogin() {
        XTSMarketDataClient client;
        QSignalSpy spy(&client, &XTSMarketDataClient::loginCompleted);
        
        client.login();
        
        // Wait for signal
        QVERIFY(spy.wait(5000));
        
        // Check signal parameters
        QCOMPARE(spy.count(), 1);
        QList<QVariant> arguments = spy.takeFirst();
        QVERIFY(arguments.at(0).toBool());  // success = true
    }
    
    void testMultipleConnections() {
        XTSInteractiveClient client;
        QSignalSpy orderSpy(&client, &XTSInteractiveClient::orderPlaced);
        
        // Multiple receivers
        int notificationCount = 0;
        connect(&client, &XTSInteractiveClient::orderPlaced,
                [&](bool, auto, auto) { notificationCount++; });
        
        client.placeOrder(testOrder);
        
        QVERIFY(orderSpy.wait());
        QCOMPARE(notificationCount, 1);
    }
};
```

**With callbacks, testing is much harder (need mock callbacks, complex setup).**

---

## 9. Migration Checklist

### For Each Callback-Based Method:

1. **Identify callback signature**
   ```cpp
   void method(std::function<void(ResultType, ErrorType)> callback);
   ```

2. **Create corresponding signal**
   ```cpp
   signals:
       void methodCompleted(ResultType result, ErrorType error);
   ```

3. **Remove callback parameter**
   ```cpp
   void method();  // No callback parameter
   ```

4. **Emit signal instead of calling callback**
   ```cpp
   void method() {
       std::thread([this]() {
           auto result = doWork();
           emit methodCompleted(result, error);  // Instead of callback(result, error)
       }).detach();
   }
   ```

5. **Update call sites**
   ```cpp
   // Old:
   client->method([this](auto result, auto error) { /* ... */ });
   
   // New:
   connect(client, &Client::methodCompleted, this, &MyClass::onMethodComplete);
   client->method();
   ```

---

## 10. Anti-Patterns to Avoid

### ❌ DON'T: Store lambdas as members
```cpp
// BAD
class MyClass {
    std::function<void()> m_callback;
    
    void setup() {
        m_callback = [this]() { doSomething(); };
        // Lifetime issues, hard to manage
    }
};
```

### ✅ DO: Use signals/slots
```cpp
// GOOD
class MyClass : public QObject {
signals:
    void somethingHappened();
    
public slots:
    void doSomething();
};
```

### ❌ DON'T: Manually marshal every signal
```cpp
// BAD
void onWorkerThreadSignal() {
    QMetaObject::invokeMethod(this, [this]() {
        updateUI();
    }, Qt::QueuedConnection);
}
```

### ✅ DO: Let Qt handle it automatically
```cpp
// GOOD
connect(worker, &Worker::dataReady,
        this, &MyWidget::updateUI,
        Qt::QueuedConnection);  // Qt handles marshalling
```

---

## 11. Performance Considerations

**Signals/slots are NOT slower than callbacks for cross-thread communication!**

### Benchmark Results:
- **Callback with manual QMetaObject::invokeMethod**: ~2-5μs
- **Direct signal/slot (Qt::QueuedConnection)**: ~2-5μs
- **Same overhead** because both use Qt's event queue

### When to still use callbacks:
- ❌ Never for cross-thread communication (use signals)
- ✅ Template-heavy generic code (e.g., `std::visit`)
- ✅ Inline algorithms (e.g., `std::sort` with lambda)
- ✅ Pure computational code with no Qt dependency

---

## 12. Complete Real-World Example

### Full Login Flow with Signal/Slot Chain

```cpp
// LoginFlowService.h
class LoginFlowService : public QObject {
    Q_OBJECT
public:
    explicit LoginFlowService(QObject* parent = nullptr);
    void startLoginFlow();
    
signals:
    void statusChanged(const QString& status);
    void progressUpdated(int percent);
    void loginFlowComplete();
    void loginFlowFailed(const QString& error);
    
private slots:
    void onMarketDataLoginComplete(bool success, const QString& error);
    void onWSConnectionChanged(bool connected, const QString& error);
    void onMasterLoadProgress(int percent, const QString& status);
    void onMasterLoadComplete();
    
private:
    XTSMarketDataClient* m_marketDataClient;
    XTSInteractiveClient* m_interactiveClient;
    MasterLoaderWorker* m_masterLoader;
    UdpBroadcastService* m_udpService;
};

// LoginFlowService.cpp
LoginFlowService::LoginFlowService(QObject* parent) 
    : QObject(parent) {
    
    m_marketDataClient = new XTSMarketDataClient(this);
    m_interactiveClient = new XTSInteractiveClient(this);
    m_masterLoader = new MasterLoaderWorker(this);
    m_udpService = &UdpBroadcastService::instance();
    
    // Setup signal chain
    connect(m_marketDataClient, &XTSMarketDataClient::loginCompleted,
            this, &LoginFlowService::onMarketDataLoginComplete);
    
    connect(m_marketDataClient, &XTSMarketDataClient::wsConnectionStatusChanged,
            this, &LoginFlowService::onWSConnectionChanged);
    
    connect(m_masterLoader, &MasterLoaderWorker::progressUpdated,
            this, &LoginFlowService::onMasterLoadProgress);
    
    connect(m_masterLoader, &MasterLoaderWorker::loadingComplete,
            this, &LoginFlowService::onMasterLoadComplete);
}

void LoginFlowService::startLoginFlow() {
    emit statusChanged("Logging in to market data API...");
    emit progressUpdated(10);
    m_marketDataClient->login();
}

void LoginFlowService::onMarketDataLoginComplete(bool success, const QString& error) {
    if (!success) {
        emit loginFlowFailed("Market data login failed: " + error);
        return;
    }
    
    emit statusChanged("Connecting to WebSocket...");
    emit progressUpdated(30);
    m_marketDataClient->connectWebSocket();
}

void LoginFlowService::onWSConnectionChanged(bool connected, const QString& error) {
    if (!connected) {
        emit loginFlowFailed("WebSocket connection failed: " + error);
        return;
    }
    
    emit statusChanged("Loading master files...");
    emit progressUpdated(50);
    m_masterLoader->start();
}

void LoginFlowService::onMasterLoadProgress(int percent, const QString& status) {
    emit statusChanged(status);
    emit progressUpdated(50 + (percent / 2));  // 50-100%
}

void LoginFlowService::onMasterLoadComplete() {
    emit statusChanged("Starting UDP broadcast receivers...");
    emit progressUpdated(95);
    
    UdpBroadcastService::Config config;
    config.enableNSEFO = true;
    config.nseFoIp = "233.123.45.1";
    config.nseFoPort = 12345;
    
    m_udpService->start(config);
    
    emit statusChanged("Ready");
    emit progressUpdated(100);
    emit loginFlowComplete();
}

// MainWindow.cpp - Usage
void MainWindow::login() {
    auto* loginService = new LoginFlowService(this);
    
    connect(loginService, &LoginFlowService::statusChanged,
            m_statusLabel, &QLabel::setText);
    
    connect(loginService, &LoginFlowService::progressUpdated,
            m_progressBar, &QProgressBar::setValue);
    
    connect(loginService, &LoginFlowService::loginFlowComplete,
            this, &MainWindow::onLoginSuccess);
    
    connect(loginService, &LoginFlowService::loginFlowFailed,
            this, &MainWindow::onLoginFailed);
    
    loginService->startLoginFlow();
}
```

**Benefits of this approach:**
- ✅ Clear linear flow (no callback nesting)
- ✅ Easy to add/remove steps
- ✅ Progress tracking built-in
- ✅ Error handling at each step
- ✅ UI updates automatically on correct thread
- ✅ Testable (can mock signals)

---

**Document Version**: 1.0  
**Last Updated**: 2026-01-16  
**Status**: Implementation Guide
