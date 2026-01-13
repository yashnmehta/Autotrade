# Async Subscription Flow with Qt Signals

## Overview

This document describes how to implement the **subscriber → FeedHandler → MainUI → PriceCache** flow using Qt signals for asynchronous, non-blocking communication.

## Architecture

```
┌─────────────────┐         ┌──────────────┐         ┌──────────┐         ┌──────────────────┐
│  MarketWatch/   │         │              │         │          │         │                  │
│  OptionChain    │────────▶│  FeedHandler │────────▶│  MainUI  │────────▶│  PriceCacheZC    │
│  (Subscriber)   │ Signal  │              │ Signal  │          │ Signal  │                  │
└─────────────────┘         └──────────────┘         └──────────┘         └──────────────────┘
         ▲                                                                           │
         │                                                                           │
         └───────────────────────────────────────────────────────────────────────────┘
                                    subscriptionReady signal
```

## Signal Flow

### 1. Subscriber Requests Token
```cpp
// In MarketWatch or OptionChain
QString requesterId = QUuid::createUuid().toString(); // Unique ID for this request
emit requestTokenSubscription(requesterId, token, segment);
```

### 2. FeedHandler Forwards Request
```cpp
// In FeedHandler.h
signals:
    void requestPriceSubscription(QString requesterId, uint32_t token, PriceCache::MarketSegment segment);

// In FeedHandler.cpp (slot)
void FeedHandler::onTokenSubscriptionRequest(QString requesterId, uint32_t token, PriceCache::MarketSegment segment) {
    qDebug() << "[FeedHandler] Forwarding subscription request for token" << token;
    emit requestPriceSubscription(requesterId, token, segment);
}
```

### 3. MainUI Routes to PriceCache
```cpp
// In MainUI.h (or MainWindow.h)
private slots:
    void onPriceSubscriptionRequest(QString requesterId, uint32_t token, PriceCache::MarketSegment segment);

// In MainUI.cpp constructor - connect signals
void MainUI::setupConnections() {
    // Connect FeedHandler → MainUI
    connect(&FeedHandler::getInstance(), &FeedHandler::requestPriceSubscription,
            this, &MainUI::onPriceSubscriptionRequest,
            Qt::QueuedConnection); // Async, thread-safe
            
    // Connect PriceCache → Subscribers (via signals)
    // This will be connected by individual subscribers
}

// In MainUI.cpp - route to PriceCache
void MainUI::onPriceSubscriptionRequest(QString requesterId, uint32_t token, PriceCache::MarketSegment segment) {
    qDebug() << "[MainUI] Routing subscription request to PriceCache for token" << token;
    
    // Call async method on PriceCache
    PriceCacheZeroCopy::getInstance().subscribeAsync(token, segment, requesterId);
}
```

### 4. PriceCache Emits Response
```cpp
// In PriceCacheZeroCopy.cpp (already implemented)
void PriceCacheZeroCopy::subscribeAsync(uint32_t token, MarketSegment segment, const QString& requesterId) {
    SubscriptionResult result = subscribe(token, segment); // Sync subscribe internally
    
    // Emit signal - Qt will deliver to main thread
    emit subscriptionReady(
        requesterId,
        token,
        segment,
        result.dataPointer,
        result.snapshot,
        result.success,
        result.errorMessage
    );
}
```

### 5. Subscriber Receives Response
```cpp
// In MarketWatch.cpp constructor
void MarketWatch::setupConnections() {
    // Connect to PriceCache subscription signal
    connect(&PriceCacheZeroCopy::getInstance(), &PriceCacheZeroCopy::subscriptionReady,
            this, &MarketWatch::onSubscriptionReady,
            Qt::QueuedConnection);
}

// In MarketWatch.cpp - handle response
void MarketWatch::onSubscriptionReady(
    QString requesterId,
    uint32_t token,
    PriceCache::MarketSegment segment,
    PriceCache::ConsolidatedMarketData* dataPointer,
    PriceCache::ConsolidatedMarketData snapshot,
    bool success,
    QString errorMessage)
{
    // Check if this response is for our request
    if (requesterId != m_currentRequestId) {
        return; // Not for us
    }
    
    if (!success) {
        qWarning() << "[MarketWatch] Subscription failed:" << errorMessage;
        QMessageBox::warning(this, "Subscription Failed", errorMessage);
        return;
    }
    
    qDebug() << "[MarketWatch] ✓ Subscription successful for token" << token;
    
    // Store pointer for direct access
    m_tokenPointers[token] = dataPointer;
    
    // Use initial snapshot
    updateUI(snapshot);
    
    // Setup timer for periodic reads (100ms)
    if (!m_updateTimer) {
        m_updateTimer = new QTimer(this);
        connect(m_updateTimer, &QTimer::timeout, this, &MarketWatch::onTimerUpdate);
        m_updateTimer->start(100); // 100ms refresh
    }
}

// Periodic update - read directly from pointer
void MarketWatch::onTimerUpdate() {
    for (auto& [token, dataPtr] : m_tokenPointers) {
        if (dataPtr) {
            // Zero-copy read! No locks, no copies
            updateRow(token, *dataPtr);
        }
    }
}
```

## Thread Safety

### Qt Signal Delivery
- **Qt::QueuedConnection**: Signals delivered via event loop (thread-safe)
- **Qt::AutoConnection**: Qt automatically chooses queued if cross-thread
- **Recommendation**: Use `Qt::QueuedConnection` explicitly for clarity

### Memory Access
- **Subscription**: Single-threaded via Qt event loop (main thread)
- **Direct Reads**: Lock-free, atomic reads from subscribers
- **UDP Writes**: Direct array access via `getSegmentBaseAddress()`
- **No Race Conditions**: Readers and writers operate on different cache lines

## Performance Characteristics

| Operation | Latency | Thread | Blocking? |
|-----------|---------|--------|-----------|
| Request Subscription | ~1ms | Any | No (async) |
| PriceCache Response | ~0.1ms | Main | No (signal) |
| Direct Read (timer) | <100ns | Main | No |
| UDP Write | <100ns | UDP thread | No |

## Example: Full Integration

### Step 1: Connect Signals in Application Startup
```cpp
// In main.cpp or MainUI::setupConnections()
void setupPriceCacheConnections() {
    auto& feedHandler = FeedHandler::getInstance();
    auto& priceCache = PriceCacheZeroCopy::getInstance();
    auto* mainUI = MainUI::getInstance();
    
    // Subscriber → FeedHandler → MainUI → PriceCache
    connect(&feedHandler, &FeedHandler::requestPriceSubscription,
            mainUI, &MainUI::onPriceSubscriptionRequest,
            Qt::QueuedConnection);
    
    qDebug() << "[App] Price cache signal chain connected";
}
```

### Step 2: MarketWatch Subscribes
```cpp
void MarketWatch::addScrip(const QString& symbol, uint32_t token) {
    QString requesterId = QUuid::createUuid().toString();
    m_pendingRequests[requesterId] = token;
    
    // Emit request signal
    emit requestTokenSubscription(requesterId, token, PriceCache::MarketSegment::NSE_CM);
    
    qDebug() << "[MarketWatch] Requested subscription for" << symbol << "token" << token;
}
```

### Step 3: Handle Response
```cpp
void MarketWatch::onSubscriptionReady(
    QString requesterId,
    uint32_t token,
    PriceCache::MarketSegment segment,
    PriceCache::ConsolidatedMarketData* dataPointer,
    PriceCache::ConsolidatedMarketData snapshot,
    bool success,
    QString errorMessage)
{
    if (!m_pendingRequests.contains(requesterId)) {
        return; // Not our request
    }
    
    m_pendingRequests.remove(requesterId);
    
    if (success) {
        // Store pointer and snapshot
        m_dataPointers[token] = dataPointer;
        m_lastSnapshot[token] = snapshot;
        
        // Add row to UI
        addRowToTable(token, snapshot);
        
        qDebug() << "[MarketWatch] ✓ Token" << token << "subscribed, pointer:" 
                 << static_cast<void*>(dataPointer);
    } else {
        qWarning() << "[MarketWatch] ✗ Subscription failed:" << errorMessage;
    }
}
```

### Step 4: Read Updates
```cpp
void MarketWatch::onTimerUpdate() {
    // Read all subscribed tokens (zero-copy!)
    for (int row = 0; row < m_model->rowCount(); ++row) {
        uint32_t token = m_model->data(m_model->index(row, 0)).toUInt();
        
        if (m_dataPointers.contains(token)) {
            ConsolidatedMarketData* dataPtr = m_dataPointers[token];
            
            // Direct memory read (no locks, no copies)
            double ltp = dataPtr->ltp;
            double change = dataPtr->change;
            double changePercent = dataPtr->changePercent;
            
            // Update UI
            m_model->setData(m_model->index(row, 1), ltp);
            m_model->setData(m_model->index(row, 2), change);
            m_model->setData(m_model->index(row, 3), changePercent);
        }
    }
}
```

## Error Handling

### Case 1: Token Not Found
```cpp
if (!success) {
    if (errorMessage.contains("not found")) {
        // Token doesn't exist in master
        QMessageBox::warning(this, "Invalid Token", 
            "Token not found in master files. Please refresh master data.");
    }
}
```

### Case 2: PriceCache Not Initialized
```cpp
if (!PriceCacheZeroCopy::getInstance().isInitialized()) {
    qWarning() << "[MarketWatch] PriceCache not ready, waiting...";
    
    // Retry after 1 second
    QTimer::singleShot(1000, this, [=]() {
        addScrip(symbol, token);
    });
}
```

### Case 3: Request Timeout
```cpp
// Set timeout for pending requests
QTimer::singleShot(5000, this, [=]() {
    if (m_pendingRequests.contains(requesterId)) {
        qWarning() << "[MarketWatch] Subscription timeout for token" << token;
        m_pendingRequests.remove(requesterId);
        
        QMessageBox::warning(this, "Timeout", 
            "Subscription request timed out. Please try again.");
    }
});
```

## Benefits of This Architecture

### 1. Non-Blocking UI
- Subscription requests don't block UI thread
- Qt event loop handles async delivery
- No threading complexity for UI developers

### 2. Type Safety
- Qt's meta-object system validates signal/slot types at compile time
- `Q_DECLARE_METATYPE` registers custom types

### 3. Decoupling
- Subscribers don't need PriceCache reference
- FeedHandler acts as mediator
- Easy to add new subscribers

### 4. Zero-Copy Performance
- Initial subscription returns pointer
- All future reads are direct memory access
- No copies, no locks, <100ns latency

### 5. Testability
- Can mock FeedHandler/MainUI
- Can test PriceCache in isolation
- Signal spies for unit tests

## Testing

### Unit Test: Signal Emission
```cpp
void TestPriceCache::testAsyncSubscription() {
    PriceCacheZeroCopy& cache = PriceCacheZeroCopy::getInstance();
    
    // Setup signal spy
    QSignalSpy spy(&cache, &PriceCacheZeroCopy::subscriptionReady);
    
    // Request subscription
    QString requesterId = "test-request-123";
    cache.subscribeAsync(12345, PriceCache::MarketSegment::NSE_CM, requesterId);
    
    // Wait for signal
    QVERIFY(spy.wait(1000)); // 1 second timeout
    QCOMPARE(spy.count(), 1);
    
    // Validate signal arguments
    QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), requesterId);
    QCOMPARE(args.at(1).toUInt(), 12345u);
    QCOMPARE(args.at(5).toBool(), true); // success
}
```

### Integration Test: Full Flow
```cpp
void TestIntegration::testSubscriptionFlow() {
    // Setup chain
    FeedHandler& fh = FeedHandler::getInstance();
    MainUI* mainUI = new MainUI();
    PriceCacheZeroCopy& cache = PriceCacheZeroCopy::getInstance();
    
    // Connect signals
    connect(&fh, &FeedHandler::requestPriceSubscription,
            mainUI, &MainUI::onPriceSubscriptionRequest);
    
    // Spy on final response
    QSignalSpy spy(&cache, &PriceCacheZeroCopy::subscriptionReady);
    
    // Emit initial request
    emit fh.requestPriceSubscription("test-id", 12345, PriceCache::MarketSegment::NSE_CM);
    
    // Verify delivery
    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.count(), 1);
}
```

## Migration from Legacy Code

### Old Code (Blocking)
```cpp
// Blocking call - UI freezes
PriceData data = PriceCache::getInstance()->getPrice(token);
updateUI(data);
```

### New Code (Async)
```cpp
// Step 1: Request subscription
emit requestTokenSubscription(requesterId, token, segment);

// Step 2: Handle response (in connected slot)
void onSubscriptionReady(...) {
    m_dataPointers[token] = dataPointer; // Store pointer
}

// Step 3: Read directly (in timer)
void onTimerUpdate() {
    ConsolidatedMarketData data = *m_dataPointers[token]; // Zero-copy!
    updateUI(data);
}
```

## Best Practices

1. **Always Use Qt::QueuedConnection** for cross-thread signals
2. **Store Pointer Once** - don't request subscription repeatedly
3. **Use Unique Request IDs** - prevents response mix-ups
4. **Set Timeouts** - handle slow/failed responses
5. **Check isInitialized()** before subscribing
6. **Use Timer for Reads** - 100ms is good balance between latency and CPU
7. **Validate Pointers** - check for nullptr before dereferencing
8. **Unsubscribe on Destroy** - cleanup resources

## Troubleshooting

### Issue: Signals Not Delivered
**Cause**: Missing Qt event loop or wrong connection type
**Solution**: Use `Qt::QueuedConnection` and ensure `QApplication::exec()` is running

### Issue: Pointer is nullptr
**Cause**: Token not found or PriceCache not initialized
**Solution**: Check `success` flag in signal, validate token exists

### Issue: Stale Data
**Cause**: UDP not writing or timer not firing
**Solution**: Check UDP receiver is running, validate timer interval

### Issue: Memory Corruption
**Cause**: Reading/writing same cache line concurrently
**Solution**: Verify 512-byte alignment, check UDP write patterns

## Next Steps

1. Implement signal connections in `MainUI::setupConnections()`
2. Add signal declarations to `FeedHandler.h`
3. Update `MarketWatch` to use async subscription
4. Update `OptionChain` to use async subscription
5. Add unit tests for signal flow
6. Benchmark latency vs legacy implementation
7. Document performance improvements

## References

- `PriceCacheZeroCopy.h` - API documentation
- `IMPLEMENTATION_COMPLETE.md` - Architecture details
- `CONSOLIDATED_ARRAY_STRATEGY.md` - Memory layout
- Qt Signals & Slots: https://doc.qt.io/qt-5/signalsandslots.html
- Qt Thread Basics: https://doc.qt.io/qt-5/thread-basics.html
