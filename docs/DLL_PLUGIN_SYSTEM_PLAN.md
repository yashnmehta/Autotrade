# DLL Plugin System - Implementation Plan

**Feature:** Client-Side Strategy Plugins without Source Code Disclosure  
**Date:** February 10, 2026  
**Priority:** HIGH (IP protection for clients)  
**Estimated Effort:** 4-6 weeks

---

## Overview

Enable clients to develop proprietary trading strategies as compiled DLL plugins that run within the trading terminal without sharing source code. This provides:

**Client Benefits:**
- Protect intellectual property (algorithms remain secret)
- Distribute strategies to customers without source disclosure
- Rapid development with provided SDK
- Full access to platform capabilities (market data, orders, positions)

**Platform Benefits:**
- Extensible ecosystem
- Third-party strategy marketplace potential
- No recompilation of main application
- Isolated plugin execution (crash protection)

---

## Architecture Design

### Component Diagram

```
┌──────────────────────────────────────────────────────────────┐
│              TRADING TERMINAL (Main Application)             │
│  ┌────────────────────────────────────────────────────────┐  │
│  │              PluginManager (Core)                      │  │
│  │  - Discovers .dll files                                │  │
│  │  - Validates signatures                                │  │
│  │  - Loads via QLibrary                                  │  │
│  │  - Creates sandbox process                             │  │
│  │  - Manages lifecycle                                   │  │
│  └─────────────────────┬──────────────────────────────────┘  │
│                        │                                     │
│  ┌─────────────────────▼──────────────────────────────────┐  │
│  │         StrategyPluginAPI (Stable Interface)           │  │
│  │  - IStrategy (pure virtual)                            │  │
│  │  - IStrategyContext (callbacks)                        │  │
│  │  - Data structures (MarketTick, Order, Position)      │  │
│  │  - Version: 1.0.0                                      │  │
│  └─────────────────────┬──────────────────────────────────┘  │
│                        │                                     │
│         ┌──────────────┴──────────────┐                      │
│         │                             │                      │
│  ┌──────▼─────────┐          ┌───────▼────────┐             │
│  │  In-Process    │          │  Out-of-Process │             │
│  │  Plugin Loader │          │  Plugin Host    │             │
│  │  (QLibrary)    │          │  (IPC via       │             │
│  │                │          │   QLocalSocket) │             │
│  └────────────────┘          └─────────────────┘             │
└──────────────────────────────────────────────────────────────┘
                        │
                        │ Loads
                        ▼
┌──────────────────────────────────────────────────────────────┐
│           CLIENT DLL (MyPrivateStrategy.dll)                 │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  class MyStrategy : public IStrategy {                 │  │
│  │      void init(IStrategyContext*) override;            │  │
│  │      void onTick(const MarketTick&) override;         │  │
│  │      bool shouldEnter() override;                      │  │
│  │      // ... proprietary logic ...                      │  │
│  │  };                                                     │  │
│  │                                                         │  │
│  │  extern "C" __declspec(dllexport)                      │  │
│  │  IStrategy* createStrategy() {                         │  │
│  │      return new MyStrategy();                          │  │
│  │  }                                                      │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Define Stable Plugin API

### Core Interface (StrategyPluginAPI.h)

```cpp
// StrategyPluginAPI.h - Distributed to clients via SDK
// Version: 1.0.0

#ifndef STRATEGY_PLUGIN_API_H
#define STRATEGY_PLUGIN_API_H

#include <cstdint>

#ifdef _WIN32
    #ifdef STRATEGY_API_EXPORT
        #define STRATEGY_API __declspec(dllexport)
    #else
        #define STRATEGY_API __declspec(dllimpport)
    #endif
#else
    #define STRATEGY_API
#endif

namespace StrategyPlugin {

// Forward declarations
class IStrategyContext;

//============================================================================
// Data Structures
//============================================================================

struct MarketTick {
    int64_t token;
    int64_t timestamp;  // Unix epoch milliseconds
    double ltp;         // Last traded price
    double open;
    double high;
    double low;
    double close;
    int64_t volume;
    double bidPrice;
    double askPrice;
    int64_t bidQty;
    int64_t askQty;
};

struct Candle {
    int64_t timestamp;  // Unix epoch seconds
    double open;
    double high;
    double low;
    double close;
    int64_t volume;
};

struct Order {
    int64_t orderId;
    char symbol[32];
    char side[8];       // "BUY" or "SELL"
    int quantity;
    double price;
    char orderType[16]; // "LIMIT", "MARKET", "SL", "SL-M"
    char status[16];    // "PENDING", "OPEN", "COMPLETE", "REJECTED", "CANCELLED"
    int64_t timestamp;
};

struct Position {
    char symbol[32];
    char side[8];       // "BUY" or "SELL"
    int quantity;
    double entryPrice;
    double currentPrice;
    double pnl;
    double unrealizedPnl;
    int64_t openTime;
};

struct IndicatorValue {
    char name[32];
    double value;
    int64_t timestamp;
};

//============================================================================
// Strategy Context Interface (Platform → Plugin)
//============================================================================

class STRATEGY_API IStrategyContext {
public:
    virtual ~IStrategyContext() = default;
    
    // Market data access
    virtual double getLTP(const char* symbol) const = 0;
    virtual bool getCurrentTick(const char* symbol, MarketTick* outTick) const = 0;
    virtual int getCandles(const char* symbol, const char* timeframe, 
                          Candle* outCandles, int maxCount) const = 0;
    
    // Indicator access
    virtual double getIndicator(const char* symbol, const char* indicatorName) const = 0;
    virtual int getIndicatorHistory(const char* symbol, const char* indicatorName,
                                   IndicatorValue* outValues, int maxCount) const = 0;
    
    // Order management
    virtual int64_t placeOrder(const char* symbol, const char* side, 
                              int quantity, double price, const char* orderType) = 0;
    virtual bool modifyOrder(int64_t orderId, int newQuantity, double newPrice) = 0;
    virtual bool cancelOrder(int64_t orderId) = 0;
    virtual bool getOrder(int64_t orderId, Order* outOrder) const = 0;
    
    // Position management
    virtual int getPositions(const char* symbol, Position* outPositions, int maxCount) const = 0;
    virtual bool getPosition(int64_t positionId, Position* outPosition) const = 0;
    
    // Account info
    virtual double getAvailableMargin() const = 0;
    virtual double getUsedMargin() const = 0;
    virtual double getTotalPnL() const = 0;
    
    // Logging
    virtual void logDebug(const char* message) = 0;
    virtual void logInfo(const char* message) = 0;
    virtual void logWarning(const char* message) = 0;
    virtual void logError(const char* message) = 0;
    
    // Utility
    virtual int64_t getCurrentTimestamp() const = 0;
    virtual bool isMarketOpen() const = 0;
    virtual const char* getSegmentName(int segment) const = 0;
};

//============================================================================
// Strategy Interface (Plugin → Platform)
//============================================================================

class STRATEGY_API IStrategy {
public:
    virtual ~IStrategy() = default;
    
    // Metadata
    virtual const char* name() const = 0;
    virtual const char* version() const = 0;
    virtual const char* author() const = 0;
    virtual const char* description() const = 0;
    
    // Lifecycle
    virtual void init(IStrategyContext* context) = 0;
    virtual void shutdown() = 0;
    
    // Market data callbacks
    virtual void onTick(const MarketTick& tick) = 0;
    virtual void onCandle(const Candle& candle) = 0;
    
    // Trading logic
    virtual bool shouldEnterLong() = 0;
    virtual bool shouldEnterShort() = 0;
    virtual bool shouldExit(const Position& position) = 0;
    virtual bool shouldModifyStopLoss(const Position& position, double* newSL) = 0;
    virtual bool shouldModifyTarget(const Position& position, double* newTarget) = 0;
    
    // Risk management
    virtual int calculateQuantity() = 0;
    virtual double calculateStopLoss(double entryPrice, const char* side) = 0;
    virtual double calculateTarget(double entryPrice, const char* side) = 0;
    
    // Order callbacks
    virtual void onOrderUpdate(const Order& order) = 0;
    virtual void onPositionUpdate(const Position& position) = 0;
    
    // Error handling
    virtual void onError(const char* errorMessage) = 0;
};

//============================================================================
// Plugin Entry Point (must be exported by client DLL)
//============================================================================

extern "C" {
    // Create strategy instance (called by platform)
    typedef IStrategy* (*CreateStrategyFunc)();
    STRATEGY_API IStrategy* createStrategy();
    
    // Destroy strategy instance (called by platform)
    typedef void (*DestroyStrategyFunc)(IStrategy*);
    STRATEGY_API void destroyStrategy(IStrategy* strategy);
    
    // Get API version (compatibility check)
    typedef int (*GetAPIVersionFunc)();
    STRATEGY_API int getAPIVersion();
}

// Current API version (increment on breaking changes)
#define STRATEGY_API_VERSION 1

} // namespace StrategyPlugin

#endif // STRATEGY_PLUGIN_API_H
```

---

## Phase 2: Plugin Manager Implementation

### PluginManager Class

```cpp
// include/core/PluginManager.h
class PluginManager : public QObject {
    Q_OBJECT
public:
    static PluginManager& instance();
    
    // Discovery
    void scanPluginsDirectory(const QString& directory);
    QStringList getAvailablePlugins() const;
    
    // Loading
    bool loadPlugin(const QString& pluginPath, QString& errorMsg);
    bool unloadPlugin(const QString& pluginId);
    bool reloadPlugin(const QString& pluginId);
    
    // Execution
    bool startPlugin(const QString& pluginId, const QString& symbol, int segment);
    bool stopPlugin(const QString& pluginId);
    
    // Query
    PluginInfo getPluginInfo(const QString& pluginId) const;
    QVector<PluginInfo> getAllPlugins() const;
    bool isPluginLoaded(const QString& pluginId) const;
    bool isPluginRunning(const QString& pluginId) const;
    
signals:
    void pluginLoaded(const QString& pluginId);
    void pluginUnloaded(const QString& pluginId);
    void pluginStarted(const QString& pluginId);
    void pluginStopped(const QString& pluginId);
    void pluginError(const QString& pluginId, const QString& error);
    
private:
    PluginManager();
    
    bool validatePlugin(const QString& path, QString& errorMsg);
    bool checkAPIVersion(QLibrary& library);
    bool verifySignature(const QString& path);
    
    struct PluginInstance {
        QString id;
        QString path;
        QLibrary* library;
        StrategyPlugin::IStrategy* strategy;
        StrategyContextImpl* context;
        bool isRunning;
        QDateTime loadedAt;
    };
    
    QHash<QString, PluginInstance*> m_plugins;
    QMutex m_mutex;
};

struct PluginInfo {
    QString id;
    QString name;
    QString version;
    QString author;
    QString description;
    QString path;
    bool isLoaded;
    bool isRunning;
    bool isSigned;
    QDateTime loadedAt;
};
```

### Plugin Loading Implementation

```cpp
// src/core/PluginManager.cpp

bool PluginManager::loadPlugin(const QString& pluginPath, QString& errorMsg) {
    QMutexLocker locker(&m_mutex);
    
    // Step 1: Validate file exists
    if (!QFile::exists(pluginPath)) {
        errorMsg = "Plugin file not found: " + pluginPath;
        return false;
    }
    
    // Step 2: Verify digital signature (security)
    if (!verifySignature(pluginPath)) {
        errorMsg = "Plugin signature verification failed";
        return false;
    }
    
    // Step 3: Load DLL
    QLibrary* library = new QLibrary(pluginPath);
    if (!library->load()) {
        errorMsg = "Failed to load plugin: " + library->errorString();
        delete library;
        return false;
    }
    
    // Step 4: Check API version compatibility
    if (!checkAPIVersion(*library)) {
        errorMsg = "Incompatible API version";
        library->unload();
        delete library;
        return false;
    }
    
    // Step 5: Resolve createStrategy function
    auto createFunc = (StrategyPlugin::CreateStrategyFunc)
        library->resolve("createStrategy");
    if (!createFunc) {
        errorMsg = "Plugin does not export createStrategy function";
        library->unload();
        delete library;
        return false;
    }
    
    // Step 6: Create strategy instance
    StrategyPlugin::IStrategy* strategy = nullptr;
    try {
        strategy = createFunc();
        if (!strategy) {
            errorMsg = "createStrategy returned null";
            library->unload();
            delete library;
            return false;
        }
    } catch (const std::exception& e) {
        errorMsg = QString("Exception in createStrategy: %1").arg(e.what());
        library->unload();
        delete library;
        return false;
    }
    
    // Step 7: Create strategy context
    StrategyContextImpl* context = new StrategyContextImpl();
    
    // Step 8: Initialize strategy
    try {
        strategy->init(context);
    } catch (const std::exception& e) {
        errorMsg = QString("Exception in init: %1").arg(e.what());
        delete strategy;
        delete context;
        library->unload();
        delete library;
        return false;
    }
    
    // Step 9: Store plugin instance
    QString pluginId = QFileInfo(pluginPath).baseName();
    PluginInstance* instance = new PluginInstance{
        .id = pluginId,
        .path = pluginPath,
        .library = library,
        .strategy = strategy,
        .context = context,
        .isRunning = false,
        .loadedAt = QDateTime::currentDateTime()
    };
    
    m_plugins[pluginId] = instance;
    
    emit pluginLoaded(pluginId);
    qInfo() << "Plugin loaded:" << pluginId;
    
    return true;
}

bool PluginManager::checkAPIVersion(QLibrary& library) {
    auto getVersionFunc = (StrategyPlugin::GetAPIVersionFunc)
        library.resolve("getAPIVersion");
    
    if (!getVersionFunc) {
        qWarning() << "Plugin does not export getAPIVersion";
        return false;
    }
    
    int pluginVersion = getVersionFunc();
    if (pluginVersion != STRATEGY_API_VERSION) {
        qWarning() << "Plugin API version mismatch:"
                  << "Expected" << STRATEGY_API_VERSION
                  << "Got" << pluginVersion;
        return false;
    }
    
    return true;
}

bool PluginManager::verifySignature(const QString& path) {
    // Windows: Use WinVerifyTrust API
    // Linux: Use GPG signature verification
    
#ifdef _WIN32
    // Verify Authenticode signature
    WINTRUST_FILE_INFO fileInfo;
    memset(&fileInfo, 0, sizeof(fileInfo));
    fileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath = reinterpret_cast<LPCWSTR>(path.utf16());
    
    WINTRUST_DATA trustData;
    memset(&trustData, 0, sizeof(trustData));
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    
    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status = WinVerifyTrust(NULL, &policyGUID, &trustData);
    
    return (status == ERROR_SUCCESS);
#else
    // For development, skip verification (in production, implement GPG check)
    qWarning() << "Signature verification not implemented for this platform";
    return true;  // Allow unsigned plugins in dev mode
#endif
}
```

---

## Phase 3: Strategy Context Implementation

### Context Bridge (Platform → Plugin)

```cpp
// include/strategy/StrategyContextImpl.h
class StrategyContextImpl : public StrategyPlugin::IStrategyContext {
public:
    StrategyContextImpl();
    ~StrategyContextImpl() override;
    
    void setSymbol(const QString& symbol, int segment);
    
    // IStrategyContext implementation
    double getLTP(const char* symbol) const override;
    bool getCurrentTick(const char* symbol, StrategyPlugin::MarketTick* outTick) const override;
    int getCandles(const char* symbol, const char* timeframe, 
                  StrategyPlugin::Candle* outCandles, int maxCount) const override;
    
    double getIndicator(const char* symbol, const char* indicatorName) const override;
    int getIndicatorHistory(const char* symbol, const char* indicatorName,
                           StrategyPlugin::IndicatorValue* outValues, int maxCount) const override;
    
    int64_t placeOrder(const char* symbol, const char* side, 
                      int quantity, double price, const char* orderType) override;
    bool modifyOrder(int64_t orderId, int newQuantity, double newPrice) override;
    bool cancelOrder(int64_t orderId) override;
    bool getOrder(int64_t orderId, StrategyPlugin::Order* outOrder) const override;
    
    int getPositions(const char* symbol, StrategyPlugin::Position* outPositions, int maxCount) const override;
    bool getPosition(int64_t positionId, StrategyPlugin::Position* outPosition) const override;
    
    double getAvailableMargin() const override;
    double getUsedMargin() const override;
    double getTotalPnL() const override;
    
    void logDebug(const char* message) override;
    void logInfo(const char* message) override;
    void logWarning(const char* message) override;
    void logError(const char* message) override;
    
    int64_t getCurrentTimestamp() const override;
    bool isMarketOpen() const override;
    const char* getSegmentName(int segment) const override;
    
private:
    QString m_symbol;
    int m_segment;
    mutable QMutex m_mutex;
    
    // Convert internal types to plugin API types
    void convertTick(const UDP::MarketTick& internal, StrategyPlugin::MarketTick* external) const;
    void convertCandle(const Candle& internal, StrategyPlugin::Candle* external) const;
    void convertOrder(const Order& internal, StrategyPlugin::Order* external) const;
    void convertPosition(const Position& internal, StrategyPlugin::Position* external) const;
};
```

### Implementation Example

```cpp
double StrategyContextImpl::getLTP(const char* symbol) const {
    QString qSymbol = QString::fromUtf8(symbol);
    return PriceStore::instance().getLTP(qSymbol, m_segment);
}

int64_t StrategyContextImpl::placeOrder(const char* symbol, const char* side,
                                        int quantity, double price, 
                                        const char* orderType) {
    QString qSymbol = QString::fromUtf8(symbol);
    QString qSide = QString::fromUtf8(side);
    QString qOrderType = QString::fromUtf8(orderType);
    
    // Validate parameters
    if (quantity <= 0) {
        logError("Invalid quantity: must be > 0");
        return -1;
    }
    
    if (price < 0) {
        logError("Invalid price: must be >= 0");
        return -1;
    }
    
    // Place order via OrderManager
    try {
        qint64 orderId = OrderManager::instance().placeOrder(
            qSymbol, m_segment, qSide, quantity, price, qOrderType, "NRML"
        );
        
        logInfo(QString("Order placed: ID=%1, %2 %3 @ %4")
            .arg(orderId).arg(qSide).arg(quantity).arg(price));
        
        return orderId;
    } catch (const std::exception& e) {
        logError(QString("Order failed: %1").arg(e.what()));
        return -1;
    }
}

int StrategyContextImpl::getCandles(const char* symbol, const char* timeframe,
                                    StrategyPlugin::Candle* outCandles, 
                                    int maxCount) const {
    QString qSymbol = QString::fromUtf8(symbol);
    QString qTimeframe = QString::fromUtf8(timeframe);
    
    // Get candles from historical store
    QVector<Candle> candles = HistoricalDataStore::instance().getRecentCandles(
        qSymbol, m_segment, qTimeframe, maxCount
    );
    
    // Convert to plugin API format
    int count = std::min(candles.size(), maxCount);
    for (int i = 0; i < count; ++i) {
        convertCandle(candles[i], &outCandles[i]);
    }
    
    return count;
}

void StrategyContextImpl::convertTick(const UDP::MarketTick& internal,
                                      StrategyPlugin::MarketTick* external) const {
    external->token = internal.token;
    external->timestamp = internal.timestamp;
    external->ltp = internal.ltp;
    external->open = internal.open;
    external->high = internal.high;
    external->low = internal.low;
    external->close = internal.close;
    external->volume = internal.volume;
    external->bidPrice = internal.bidPrice;
    external->askPrice = internal.askPrice;
    external->bidQty = internal.bidQty;
    external->askQty = internal.askQty;
}
```

---

## Phase 4: Plugin Sandboxing & Security

### Out-of-Process Plugin Host

```cpp
// src/core/PluginHost.cpp (separate process)
int main(int argc, char* argv[]) {
    if (argc < 3) {
        qCritical() << "Usage: PluginHost <plugin_path> <ipc_name>";
        return 1;
    }
    
    QString pluginPath = argv[1];
    QString ipcName = argv[2];
    
    // Load plugin in isolated process
    QLibrary library(pluginPath);
    if (!library.load()) {
        qCritical() << "Failed to load plugin:" << library.errorString();
        return 1;
    }
    
    auto createFunc = (StrategyPlugin::CreateStrategyFunc)
        library.resolve("createStrategy");
    if (!createFunc) {
        qCritical() << "createStrategy not found";
        return 1;
    }
    
    StrategyPlugin::IStrategy* strategy = createFunc();
    if (!strategy) {
        qCritical() << "createStrategy returned null";
        return 1;
    }
    
    // Set up IPC communication with main process
    QLocalSocket ipcSocket;
    ipcSocket.connectToServer(ipcName);
    if (!ipcSocket.waitForConnected(5000)) {
        qCritical() << "IPC connection failed";
        return 1;
    }
    
    // Create context that communicates via IPC
    StrategyContextIPC* context = new StrategyContextIPC(&ipcSocket);
    
    // Initialize strategy
    strategy->init(context);
    
    // Main event loop - process IPC commands
    while (ipcSocket.state() == QLocalSocket::ConnectedState) {
        if (ipcSocket.waitForReadyRead(100)) {
            QByteArray data = ipcSocket.readAll();
            processIPCCommand(strategy, context, data, &ipcSocket);
        }
    }
    
    // Cleanup
    strategy->shutdown();
    delete strategy;
    delete context;
    library.unload();
    
    return 0;
}

void processIPCCommand(StrategyPlugin::IStrategy* strategy,
                      StrategyContextIPC* context,
                      const QByteArray& data,
                      QLocalSocket* socket) {
    QDataStream stream(data);
    QString command;
    stream >> command;
    
    if (command == "onTick") {
        StrategyPlugin::MarketTick tick;
        stream >> tick.token >> tick.timestamp >> tick.ltp;
        // ... read other fields
        strategy->onTick(tick);
    }
    else if (command == "shouldEnterLong") {
        bool result = strategy->shouldEnterLong();
        QByteArray response;
        QDataStream responseStream(&response, QIODevice::WriteOnly);
        responseStream << QString("result") << result;
        socket->write(response);
    }
    // ... other commands
}
```

### Resource Limits

```cpp
// Apply resource limits to plugin process (Windows)
void applyResourceLimits(HANDLE processHandle) {
    // CPU time limit: 10% of one core
    JOBOBJECT_CPU_RATE_CONTROL_INFORMATION cpuLimit;
    cpuLimit.ControlFlags = JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | 
                            JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
    cpuLimit.CpuRate = 1000;  // 10% (in 0.01% units)
    
    SetInformationJobObject(jobHandle, JobObjectCpuRateControlInformation,
                            &cpuLimit, sizeof(cpuLimit));
    
    // Memory limit: 100 MB
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION memLimit;
    memset(&memLimit, 0, sizeof(memLimit));
    memLimit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    memLimit.ProcessMemoryLimit = 100 * 1024 * 1024;  // 100 MB
    
    SetInformationJobObject(jobHandle, JobObjectExtendedLimitInformation,
                            &memLimit, sizeof(memLimit));
}
```

---

## Phase 5: Client SDK & Documentation

### SDK Package Contents

```
StrategySDK/
├── include/
│   └── StrategyPluginAPI.h      # Core API header
├── lib/
│   ├── StrategyPluginAPI.lib     # Import library (Windows)
│   └── libStrategyPluginAPI.so   # Shared library (Linux)
├── examples/
│   ├── SimpleMA/
│   │   ├── SimpleMAStrategy.h
│   │   ├── SimpleMAStrategy.cpp
│   │   └── CMakeLists.txt
│   ├── RSI/
│   │   ├── RSIStrategy.h
│   │   ├── RSIStrategy.cpp
│   │   └── CMakeLists.txt
│   └── MacdCrossover/
│       ├── MacdStrategy.h
│       ├── MacdStrategy.cpp
│       └── CMakeLists.txt
├── docs/
│   ├── API_Reference.md
│   ├── Quick_Start_Guide.md
│   ├── Best_Practices.md
│   └── FAQ.md
├── tools/
│   ├── SignTool.exe              # DLL signing utility
│   └── PluginValidator.exe       # Pre-deployment validator
└── CMakeLists.txt                # Root CMake for examples
```

### Example: Simple MA Strategy (Client Code)

```cpp
// SimpleMAStrategy.h
#ifndef SIMPLE_MA_STRATEGY_H
#define SIMPLE_MA_STRATEGY_H

#include <StrategyPluginAPI.h>
#include <vector>
#include <string>

class SimpleMAStrategy : public StrategyPlugin::IStrategy {
public:
    SimpleMAStrategy();
    ~SimpleMAStrategy() override;
    
    // Metadata
    const char* name() const override { return "Simple MA Crossover"; }
    const char* version() const override { return "1.0.0"; }
    const char* author() const override { return "Client Name"; }
    const char* description() const override { 
        return "20/50 SMA crossover strategy"; 
    }
    
    // Lifecycle
    void init(StrategyPlugin::IStrategyContext* context) override;
    void shutdown() override;
    
    // Market data
    void onTick(const StrategyPlugin::MarketTick& tick) override;
    void onCandle(const StrategyPlugin::Candle& candle) override;
    
    // Trading logic
    bool shouldEnterLong() override;
    bool shouldEnterShort() override;
    bool shouldExit(const StrategyPlugin::Position& position) override;
    bool shouldModifyStopLoss(const StrategyPlugin::Position& position, double* newSL) override;
    bool shouldModifyTarget(const StrategyPlugin::Position& position, double* newTarget) override;
    
    // Risk management
    int calculateQuantity() override;
    double calculateStopLoss(double entryPrice, const char* side) override;
    double calculateTarget(double entryPrice, const char* side) override;
    
    // Callbacks
    void onOrderUpdate(const StrategyPlugin::Order& order) override;
    void onPositionUpdate(const StrategyPlugin::Position& position) override;
    void onError(const char* errorMessage) override;
    
private:
    StrategyPlugin::IStrategyContext* m_context;
    std::vector<double> m_closePrices;
    std::string m_signal;
    
    double calculateSMA(int period) const;
};

#endif
```

```cpp
// SimpleMAStrategy.cpp
#include "SimpleMAStrategy.h"
#include <algorithm>
#include <numeric>
#include <sstream>

SimpleMAStrategy::SimpleMAStrategy()
    : m_context(nullptr)
{
}

SimpleMAStrategy::~SimpleMAStrategy() {
}

void SimpleMAStrategy::init(StrategyPlugin::IStrategyContext* context) {
    m_context = context;
    m_context->logInfo("Simple MA Strategy initialized");
    
    // Subscribe to 5-minute candles
    m_closePrices.reserve(100);
}

void SimpleMAStrategy::shutdown() {
    m_context->logInfo("Simple MA Strategy shutting down");
}

void SimpleMAStrategy::onTick(const StrategyPlugin::MarketTick& tick) {
    // Not used in this strategy (we use candles)
}

void SimpleMAStrategy::onCandle(const StrategyPlugin::Candle& candle) {
    m_closePrices.push_back(candle.close);
    
    // Keep only last 100 candles
    if (m_closePrices.size() > 100) {
        m_closePrices.erase(m_closePrices.begin());
    }
    
    // Need at least 50 candles for SMA50
    if (m_closePrices.size() < 50) {
        return;
    }
    
    double sma20 = calculateSMA(20);
    double sma50 = calculateSMA(50);
    
    // Get previous SMAs (from previous candle)
    double prevSMA20 = 0.0;
    double prevSMA50 = 0.0;
    if (m_closePrices.size() > 50) {
        // Calculate from previous state (exclude last price)
        auto temp = m_closePrices.back();
        m_closePrices.pop_back();
        prevSMA20 = calculateSMA(20);
        prevSMA50 = calculateSMA(50);
        m_closePrices.push_back(temp);
    }
    
    // Bullish crossover
    if (prevSMA20 <= prevSMA50 && sma20 > sma50) {
        m_signal = "LONG";
        m_context->logInfo("Bullish MA crossover detected");
    }
    // Bearish crossover
    else if (prevSMA20 >= prevSMA50 && sma20 < sma50) {
        m_signal = "SHORT";
        m_context->logInfo("Bearish MA crossover detected");
    }
}

bool SimpleMAStrategy::shouldEnterLong() {
    if (m_signal != "LONG") return false;
    
    // Check no open positions
    StrategyPlugin::Position positions[10];
    int count = m_context->getPositions("", positions, 10);
    
    return (count == 0);
}

bool SimpleMAStrategy::shouldEnterShort() {
    if (m_signal != "SHORT") return false;
    
    // Check no open positions
    StrategyPlugin::Position positions[10];
    int count = m_context->getPositions("", positions, 10);
    
    return (count == 0);
}

bool SimpleMAStrategy::shouldExit(const StrategyPlugin::Position& position) {
    if (m_closePrices.size() < 50) return false;
    
    double sma20 = calculateSMA(20);
    double sma50 = calculateSMA(50);
    
    // Exit long if SMA20 crosses below SMA50
    if (std::string(position.side) == "BUY" && sma20 < sma50) {
        return true;
    }
    
    // Exit short if SMA20 crosses above SMA50
    if (std::string(position.side) == "SELL" && sma20 > sma50) {
        return true;
    }
    
    return false;
}

bool SimpleMAStrategy::shouldModifyStopLoss(const StrategyPlugin::Position& position,
                                             double* newSL) {
    return false;  // Not implemented in this strategy
}

bool SimpleMAStrategy::shouldModifyTarget(const StrategyPlugin::Position& position,
                                           double* newTarget) {
    return false;  // Not implemented in this strategy
}

int SimpleMAStrategy::calculateQuantity() {
    return 1;  // Fixed quantity
}

double SimpleMAStrategy::calculateStopLoss(double entryPrice, const char* side) {
    double atr = m_context->getIndicator("", "ATR_14");
    
    if (std::string(side) == "BUY") {
        return entryPrice - (2.0 * atr);
    } else {
        return entryPrice + (2.0 * atr);
    }
}

double SimpleMAStrategy::calculateTarget(double entryPrice, const char* side) {
    double atr = m_context->getIndicator("", "ATR_14");
    
    if (std::string(side) == "BUY") {
        return entryPrice + (3.0 * atr);
    } else {
        return entryPrice - (3.0 * atr);
    }
}

void Simple MAStrategy::onOrderUpdate(const StrategyPlugin::Order& order) {
    std::ostringstream oss;
    oss << "Order update: ID=" << order.orderId 
        << " Status=" << order.status;
    m_context->logInfo(oss.str().c_str());
}

void SimpleMAStrategy::onPositionUpdate(const StrategyPlugin::Position& position) {
    std::ostringstream oss;
    oss << "Position update: " << position.symbol
        << " PnL=" << position.pnl;
    m_context->logInfo(oss.str().c_str());
}

void SimpleMAStrategy::onError(const char* errorMessage) {
    m_context->logError(errorMessage);
}

double SimpleMAStrategy::calculateSMA(int period) const {
    if (m_closePrices.size() < static_cast<size_t>(period)) {
        return 0.0;
    }
    
    double sum = std::accumulate(
        m_closePrices.end() - period,
        m_closePrices.end(),
        0.0
    );
    
    return sum / period;
}

//=============================================================================
// DLL Export Functions
//=============================================================================

extern "C" {
    STRATEGY_API StrategyPlugin::IStrategy* createStrategy() {
        return new SimpleMAStrategy();
    }
    
    STRATEGY_API void destroyStrategy(StrategyPlugin::IStrategy* strategy) {
        delete strategy;
    }
    
    STRATEGY_API int getAPIVersion() {
        return STRATEGY_API_VERSION;
    }
}
```

### CMakeLists.txt for Client Plugin

```cmake
cmake_minimum_required(VERSION 3.15)
project(SimpleMAStrategy)

set(CMAKE_CXX_STANDARD 17)

# Find StrategySDK
find_path(STRATEGY_SDK_INCLUDE_DIR StrategyPluginAPI.h
    PATHS $ENV{STRATEGY_SDK_PATH}/include
)

# Create plugin DLL
add_library(SimpleMAStrategy SHARED
    SimpleMAStrategy.h
    SimpleMAStrategy.cpp
)

target_include_directories(SimpleMAStrategy PRIVATE
    ${STRATEGY_SDK_INCLUDE_DIR}
)

# Export symbols
target_compile_definitions(SimpleMAStrategy PRIVATE
    STRATEGY_API_EXPORT
)

# Install
install(TARGETS SimpleMAStrategy
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
)
```

---

## Phase 6: Testing & Validation

### Plugin Validator Tool

```cpp
// tools/PluginValidator/main.cpp
int main(int argc, char* argv[]) {
    if (argc < 2) {
        qInfo() << "Usage: PluginValidator <plugin.dll>";
        return 1;
    }
    
    QString pluginPath = argv[1];
    
    qInfo() << "Validating plugin:" << pluginPath;
    qInfo() << "==========================================";
    
    // Test 1: File exists
    if (!QFile::exists(pluginPath)) {
        qCritical() << "❌ File not found";
        return 1;
    }
    qInfo() << "✅ File exists";
    
    // Test 2: Load DLL
    QLibrary library(pluginPath);
    if (!library.load()) {
        qCritical() << "❌ Failed to load:" << library.errorString();
        return 1;
    }
    qInfo() << "✅ DLL loaded successfully";
    
    // Test 3: Check exports
    auto getVersionFunc = (StrategyPlugin::GetAPIVersionFunc)
        library.resolve("getAPIVersion");
    if (!getVersionFunc) {
        qCritical() << "❌ Missing export: getAPIVersion";
        library.unload();
        return 1;
    }
    qInfo() << "✅ getAPIVersion found";
    
    auto createFunc = (StrategyPlugin::CreateStrategyFunc)
        library.resolve("createStrategy");
    if (!createFunc) {
        qCritical() << "❌ Missing export: createStrategy";
        library.unload();
        return 1;
    }
    qInfo() << "✅ createStrategy found";
    
    auto destroyFunc = (StrategyPlugin::DestroyStrategyFunc)
        library.resolve("destroyStrategy");
    if (!destroyFunc) {
        qCritical() << "❌ Missing export: destroyStrategy";
        library.unload();
        return 1;
    }
    qInfo() << "✅ destroyStrategy found";
    
    // Test 4: API version
    int version = getVersionFunc();
    if (version != STRATEGY_API_VERSION) {
        qCritical() << "❌ API version mismatch:"
                    << "Expected" << STRATEGY_API_VERSION
                    << "Got" << version;
        library.unload();
        return 1;
    }
    qInfo() << "✅ API version compatible:" << version;
    
    // Test 5: Create instance
    StrategyPlugin::IStrategy* strategy = createFunc();
    if (!strategy) {
        qCritical() << "❌ createStrategy returned null";
        library.unload();
        return 1;
    }
    qInfo() << "✅ Strategy instance created";
    
    // Test 6: Metadata
    qInfo() << "Strategy Name:" << strategy->name();
    qInfo() << "Version:" << strategy->version();
    qInfo() << "Author:" << strategy->author();
    qInfo() << "Description:" << strategy->description();
    
    // Test 7: Destroy instance
    destroyFunc(strategy);
    qInfo() << "✅ Strategy instance destroyed";
    
    // Cleanup
    library.unload();
    qInfo() << "✅ DLL unloaded";
    
    qInfo() << "==========================================";
    qInfo() << "✅ All tests passed!";
    
    return 0;
}
```

---

## Security Considerations

### DLL Signing (Windows)

```bash
# Sign DLL with Authenticode certificate
signtool sign /f MyCert.pfx /p MyPassword /t http://timestamp.digicert.com SimpleMAStrategy.dll

# Verify signature
signtool verify /pa SimpleMAStrategy.dll
```

### Whitelist Approach

```cpp
// Only load plugins from approved directory
QSettings settings;
QString pluginDir = settings.value("pluginDirectory", 
    QCoreApplication::applicationDirPath() + "/plugins").toString();

// Prevent loading from arbitrary paths
if (!pluginPath.startsWith(pluginDir)) {
    qWarning() << "Plugin must be in approved directory:" << pluginDir;
    return false;
}
```

### API Restrictions

```cpp
// Limit order quantities
int64_t StrategyContextImpl::placeOrder(const char* symbol, const char* side,
                                        int quantity, double price, 
                                        const char* orderType) {
    // Enforce maximum quantity per order
    const int MAX_QTY = 100;
    if (quantity > MAX_QTY) {
        logError(QString("Quantity %1 exceeds limit %2").arg(quantity).arg(MAX_QTY));
        return -1;
    }
    
    // Enforce rate limiting (max 10 orders per second)
    if (!m_rateLimiter.checkLimit()) {
        logError("Order rate limit exceeded");
        return -1;
    }
    
    // ... rest of implementation
}
```

---

## Deployment Checklist

- [ ] StrategyPluginAPI.h finalized (v1.0.0)
- [ ] PluginManager implemented with loading/unloading
- [ ] StrategyContextImpl with all API functions
- [ ] DLL signature verification (Windows/Linux)
- [ ] Out-of-process plugin host (optional, for sandboxing)
- [ ] Resource limits applied (CPU, memory)
- [ ] SDK package created with examples
- [ ] API documentation (Doxygen)
- [ ] Quick start guide for clients
- [ ] PluginValidator tool
- [ ] SignTool integration
- [ ] Testing with sample plugins
- [ ] Performance benchmarks

---

## Estimated Timeline

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| **Phase 1** | 1 week | Stable API header v1.0.0 |
| **Phase 2** | 1 week | PluginManager with loading |
| **Phase 3** | 1 week | StrategyContextImpl complete |
| **Phase 4** | 1 week | Sandboxing + security |
| **Phase 5** | 1 week | SDK package + examples |
| **Phase 6** | 1 week | Testing + validation tools |

**Total:** 6 weeks

---

## Maintenance & Versioning

### API Versioning Strategy

- **v1.0.0:** Initial release
- **v1.x.x:** Backwards-compatible additions (new functions)
- **v2.0.0:** Breaking changes (function signature changes)

### Deprecation Policy

When deprecating API functions:
1. Mark as `[[deprecated]]` in header
2. Provide migration guide
3. Support for 2 major versions
4. Remove in 3rd major version

Example:
```cpp
// Deprecated in v2.0.0, will be removed in v3.0.0
[[deprecated("Use getIndicatorEx instead")]]
virtual double getIndicator(const char* name) const = 0;
```

---

## Future Enhancements

1. **Hot Reload:** Reload plugin without restarting terminal
2. **Plugin Marketplace:** Distribute plugins via online store
3. **Encrypted Plugins:** AES encryption for additional IP protection
4. **Multi-Language Support:** Python/C#/Rust plugins via FFI
5. **GPU Acceleration:** CUDA/OpenCL for ML-based strategies
6. **Cloud Deployment:** Run plugins on remote servers

---

**Status:** Ready for Implementation  
**Risk Level:** MEDIUM (requires careful security design)  
**Client Demand:** HIGH (IP protection critical)  
**Expected Impact:** VERY HIGH (enables ecosystem)
