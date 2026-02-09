# Architecture Recommendations - Trading Terminal C++

**Focus**: Long-term maintainability, scalability, testability  
**Priority**: 🔵 **P3-P4** (Not urgent, but important for growth)  
**Timeframe**: 2-6 months

---

## 📊 Executive Summary

Your current architecture is **functional and well-documented**, but opportunities exist for improvement. This document proposes enhancements for:

1. **Testability** - Add dependency injection, mock interfaces
2. **Configuration** - Separate concerns, enable environment-specific configs
3. **Modularity** - Reduce singleton proliferation, improve boundaries
4. **Observability** - Structured logging, health monitoring
5. **Performance** - Lock-free optimizations for hot paths

**Guiding Principle**: Incremental improvement, not rewrite. These are evolutionary changes.

---

## 🏗️ Current Architecture Assessment

### Strengths ✅

1. **Layer Separation**: Clear UI → Service → Repository → Data Store hierarchy
2. **Thread Architecture**: Well-designed UDP → Qt Signal → UI flow
3. **Documentation**: Excellent markdown docs explaining design decisions
4. **Recent Improvements**: Greeks implementation shows good iterative development

### Pain Points ⚠️

1. **Singleton Proliferation**: 10+ global singletons create hidden dependencies
2. **Configuration Management**: Monolithic `config.ini` mixes concerns
3. **Testing Gaps**: No unit tests, hard to test isolated components
4. **Lock Contention**: Potential bottleneck in high-frequency ticks

---

## 🎯 Recommendation 1: Configuration Management

### Current State

**Problem**: Single `config.ini` contains:
- Security-critical credentials
- Infrastructure settings (UDP, API)
- Business logic (Greeks, ATM Watch)
- UI preferences

**Issues**:
- Can't commit file to git (contains secrets)
- No environment-specific overrides (dev/staging/prod)
- No validation or schema
- Manual parsing, error-prone

---

### Proposed Architecture

**Structure**:
```
configs/
├── app.ini                    # Application settings (gitignored)
├── app.ini.template           # Template with placeholders (committed)
├── environments/
│   ├── development.ini        # Dev overrides
│   ├── staging.ini            # Staging overrides
│   └── production.ini         # Prod overrides
├── schemas/
│   └── config.schema.json     # JSON schema for validation
└── secrets/
    └── .env                   # Environment variables (gitignored)
```

**Example - app.ini.template**:
```ini
# Trading Terminal Configuration Template
# Copy to app.ini and fill in values

[XTS]
# API Configuration
url = ${XTS_URL:http://localhost:3000}
disable_ssl = ${XTS_DISABLE_SSL:true}
broadcastmode = ${XTS_BROADCAST_MODE:Partial}

[UDP]
# UDP Multicast Settings
nse_fo_multicast_ip = ${NSE_FO_IP:233.1.2.5}
nse_fo_port = ${NSE_FO_PORT:34331}

[GREEKS_CALCULATION]
enabled = ${GREEKS_ENABLED:true}
risk_free_rate = ${RISK_FREE_RATE:0.065}
base_price_mode = ${BASE_PRICE_MODE:cash}
```

**Environment-Specific Override**:
```ini
# configs/environments/production.ini
[XTS]
url = https://mtrade.arhamshare.com
disable_ssl = false
```

---

### Implementation

**Step 1: Create ConfigManager**

```cpp
// include/utils/ConfigManager.h
class ConfigManager {
public:
    static ConfigManager& instance();
    
    // Load with priority: CLI args > Environment > Env-specific > Default
    void load(const QString& environment = "development");
    
    // Type-safe getters
    QString getString(const QString& section, const QString& key, 
                     const QString& defaultValue = "") const;
    int getInt(const QString& section, const QString& key, int defaultValue = 0) const;
    double getDouble(const QString& section, const QString& key, double defaultValue = 0.0) const;
    bool getBool(const QString& section, const QString& key, bool defaultValue = false) const;
    
    // Validation
    bool validate(const QString& schemaPath);
    QStringList getValidationErrors() const;
    
    // Hot reload
    void reload();
    void watchForChanges(bool enable = true);
    
signals:
    void configChanged(const QString& section, const QString& key);
    
private:
    ConfigManager() = default;
    QString resolveValue(const QString& value) const;  // Expand ${VAR:default}
    QString m_environment;
    QSettings m_settings;
};
```

**Step 2: Environment Variable Expansion**

```cpp
// src/utils/ConfigManager.cpp
QString ConfigManager::resolveValue(const QString& value) const {
    static QRegularExpression re(R"(\$\{([^:}]+):?([^}]*)\})");
    
    QString result = value;
    QRegularExpressionMatchIterator it = re.globalMatch(value);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString varName = match.captured(1);
        QString defaultVal = match.captured(2);
        
        // Try environment variable first
        QString envValue = qgetenv(varName.toUtf8());
        QString replacement = envValue.isEmpty() ? defaultVal : envValue;
        
        result.replace(match.captured(0), replacement);
    }
    
    return result;
}

QString ConfigManager::getString(const QString& section, 
                                const QString& key, 
                                const QString& defaultValue) const {
    m_settings.beginGroup(section);
    QString value = m_settings.value(key, defaultValue).toString();
    m_settings.endGroup();
    
    return resolveValue(value);
}
```

**Step 3: Migration Path**

```cpp
// Old code
QSettings settings("configs/config.ini", QSettings::IniFormat);
QString url = settings.value("XTS/url", "").toString();

// New code
ConfigManager& config = ConfigManager::instance();
config.load(qgetenv("APP_ENV"));  // "development", "staging", "production"
QString url = config.getString("XTS", "url");
```

---

## 🎯 Recommendation 2: Dependency Injection

### Current State

**Problem**: Tight coupling via singletons

```cpp
// Current code - tightly coupled
void GreeksCalculationService::calculateForToken(uint32_t token) {
    // ❌ Direct dependency on global singleton
    RepositoryManager* repo = RepositoryManager::getInstance();
    auto* contract = repo->getContract(token);
    
    // ❌ Another global dependency
    double price = nsefo::g_nseFoPriceStore.getUnifiedSnapshot(token).ltp;
}

// Problems:
// - Hard to unit test (can't mock RepositoryManager)
// - Hidden dependencies (not visible in constructor)
// - Initialization order matters
```

---

### Proposed Architecture

**Dependency Injection via Constructor**:

```cpp
// include/services/GreeksCalculationService.h
class IContractRepository {
public:
    virtual ~IContractRepository() = default;
    virtual ContractData* getContract(uint32_t token, int segment) = 0;
};

class IPriceProvider {
public:
    virtual ~IPriceProvider() = default;
    virtual double getPrice(uint32_t token, int segment) = 0;
};

class GreeksCalculationService : public QObject {
public:
    // ✅ Explicit dependencies
    GreeksCalculationService(IContractRepository* contractRepo,
                            IPriceProvider* priceProvider,
                            QObject* parent = nullptr);
    
    void calculateForToken(uint32_t token, int segment);
    
private:
    IContractRepository* m_contractRepo;  // Injected
    IPriceProvider* m_priceProvider;      // Injected
};
```

**Implementation**:

```cpp
// Concrete implementations
class RepositoryManagerAdapter : public IContractRepository {
public:
    ContractData* getContract(uint32_t token, int segment) override {
        return RepositoryManager::getInstance()->getContract(token, segment);
    }
};

class PriceStoreAdapter : public IPriceProvider {
public:
    double getPrice(uint32_t token, int segment) override {
        if (segment == 2) {  // NSE FO
            auto snap = nsefo::g_nseFoPriceStore.getUnifiedSnapshot(token);
            return snap.ltp;
        }
        // ... other segments
        return 0.0;
    }
};

// Wiring
GreeksCalculationService setupGreeksService() {
    auto contractRepo = new RepositoryManagerAdapter();
    auto priceProvider = new PriceStoreAdapter();
    
    return GreeksCalculationService(contractRepo, priceProvider);
}
```

**Testing Benefits**:

```cpp
// tests/test_greeks_service.cpp
class MockContractRepository : public IContractRepository {
public:
    MOCK_METHOD(ContractData*, getContract, (uint32_t, int), (override));
};

class MockPriceProvider : public IPriceProvider {
public:
    MOCK_METHOD(double, getPrice, (uint32_t, int), (override));
};

TEST(GreeksService, CalculatesIVCorrectly) {
    MockContractRepository mockRepo;
    MockPriceProvider mockPriceProvider;
    
    // Setup expectations
    EXPECT_CALL(mockRepo, getContract(42546, 2))
        .WillOnce(Return(createMockContract()));
    EXPECT_CALL(mockPriceProvider, getPrice(42546, 2))
        .WillOnce(Return(100.50));
    
    // Test
    GreeksCalculationService service(&mockRepo, &mockPriceProvider);
    auto result = service.calculateForToken(42546, 2);
    
    EXPECT_NEAR(result.impliedVolatility, 0.20, 0.01);
}
```

---

## 🎯 Recommendation 3: Structured Logging

### Current State

**Problem**: Inconsistent logging

```cpp
// Found in codebase:
qDebug() << "[GreeksDebug] IV:" << iv;  // Sometimes enabled
// qDebug() << "[UdpBroadcast] Tick received";  // Commented out
qWarning() << "Error loading file";  // No context
qCritical() << "[Security] Missing credentials";  // Good!
```

**Issues**:
- No severity levels (all mixed)
- Hard to filter logs
- No structured fields (can't parse programmatically)
- Commented-out debug statements

---

### Proposed Architecture

**Qt Logging Categories**:

```cpp
// include/utils/Logging.h
#ifndef LOGGING_H
#define LOGGING_H

#include <QLoggingCategory>

// Declare categories
Q_DECLARE_LOGGING_CATEGORY(greeks)
Q_DECLARE_LOGGING_CATEGORY(udp)
Q_DECLARE_LOGGING_CATEGORY(api)
Q_DECLARE_LOGGING_CATEGORY(security)
Q_DECLARE_LOGGING_CATEGORY(performance)

// Convenience macros
#define LOG_GREEKS_DEBUG qCDebug(greeks)
#define LOG_GREEKS_INFO qCInfo(greeks)
#define LOG_GREEKS_WARN qCWarning(greeks)

#define LOG_UDP_DEBUG qCDebug(udp)
#define LOG_API_ERROR qCCritical(api)
#define LOG_SECURITY_CRITICAL qCCritical(security)

#endif
```

**Implementation**:

```cpp
// src/utils/Logging.cpp
#include "utils/Logging.h"

Q_LOGGING_CATEGORY(greeks, "greeks.calculation")
Q_LOGGING_CATEGORY(udp, "udp.receiver")
Q_LOGGING_CATEGORY(api, "api.xts")
Q_LOGGING_CATEGORY(security, "security")
Q_LOGGING_CATEGORY(performance, "performance")
```

**Usage**:

```cpp
// Before (inconsistent)
qDebug() << "[GreeksDebug] Calculating IV for token:" << token;
// qDebug() << "[GreeksDebug] Result:" << iv;  // Commented!

// After (structured)
LOG_GREEKS_DEBUG << "Calculating IV"
                 << "token=" << token
                 << "segment=" << segment;
LOG_GREEKS_INFO << "IV calculated"
                << "token=" << token
                << "iv=" << iv
                << "iterations=" << iterations;
```

**Configuration** (Enable/disable at runtime):

```cpp
// main.cpp
void setupLogging() {
    // Load from config.ini
    QSettings settings("configs/config.ini", QSettings::IniFormat);
    settings.beginGroup("LOGGING");
    
    bool greeksDebug = settings.value("greeks_debug", false).toBool();
    bool udpDebug = settings.value("udp_debug", false).toBool();
    
    // Set category filters
    QString rules;
    if (greeksDebug) rules += "greeks.calculation.debug=true\n";
    if (udpDebug) rules += "udp.receiver.debug=true\n";
    
    // Always enable warnings and errors
    rules += "*.warning=true\n";
    rules += "*.critical=true\n";
    
    QLoggingCategory::setFilterRules(rules);
}
```

**Environment Variable Override**:
```powershell
# Enable all debug logging
$env:QT_LOGGING_RULES="*.debug=true"

# Or selective
$env:QT_LOGGING_RULES="greeks.calculation.debug=true;udp.receiver.debug=true"
```

---

## 🎯 Recommendation 4: Health Monitoring

### Current State

**Problem**: No runtime health visibility

- No way to check if UDP feeds are healthy
- Can't detect if Greeks calculation stuck
- No alerts for degraded performance

---

### Proposed Architecture

```cpp
// include/utils/HealthMonitor.h
class HealthMonitor : public QObject {
    Q_OBJECT
public:
    enum class Status { Healthy, Degraded, Critical };
    
    struct ComponentHealth {
        QString name;
        Status status;
        QString message;
        QDateTime lastUpdate;
    };
    
    static HealthMonitor& instance();
    
    // Register components
    void registerComponent(const QString& name);
    
    // Update health
    void setHealthy(const QString& component);
    void setDegraded(const QString& component, const QString& reason);
    void setCritical(const QString& component, const QString& reason);
    
    // Heartbeat (call frequently)
    void heartbeat(const QString& component);
    
    // Query
    Status getOverallStatus() const;
    QList<ComponentHealth> getComponentStatus() const;
    
    // Export
    QString toJson() const;
    
signals:
    void statusChanged(const QString& component, Status status);
    void overallStatusChanged(Status status);
    
private:
    HealthMonitor() = default;
    void checkHeartbeats();  // Timer checks for stale heartbeats
    
    QHash<QString, ComponentHealth> m_components;
    QTimer* m_heartbeatChecker;
};
```

**Usage**:

```cpp
// In UDP receiver
void NseFoReader::receivePackets() {
    HealthMonitor::instance().registerComponent("udp.nsefo");
    
    while (running) {
        int bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes > 0) {
            HealthMonitor::instance().heartbeat("udp.nsefo");
            processPacket(buffer, bytes);
        } else {
            HealthMonitor::instance().setDegraded("udp.nsefo", 
                "No packets received for 5 seconds");
        }
    }
}

// In Greeks service
void GreeksCalculationService::calculateForToken(uint32_t token) {
    HealthMonitor::instance().registerComponent("greeks.calculation");
    HealthMonitor::instance().heartbeat("greeks.calculation");
    
    try {
        // ... calculation ...
        HealthMonitor::instance().setHealthy("greeks.calculation");
    } catch (...) {
        HealthMonitor::instance().setCritical("greeks.calculation", 
            "Exception during calculation");
    }
}
```

**UI Integration**:

```cpp
// Status bar indicator
QLabel* statusLabel = new QLabel(statusBar);
connect(&HealthMonitor::instance(), &HealthMonitor::overallStatusChanged,
        [statusLabel](HealthMonitor::Status status) {
            switch (status) {
                case HealthMonitor::Status::Healthy:
                    statusLabel->setText("● Healthy");
                    statusLabel->setStyleSheet("color: green;");
                    break;
                case HealthMonitor::Status::Degraded:
                    statusLabel->setText("⚠ Degraded");
                    statusLabel->setStyleSheet("color: orange;");
                    break;
                case HealthMonitor::Status::Critical:
                    statusLabel->setText("✖ Critical");
                    statusLabel->setStyleSheet("color: red;");
                    break;
            }
        });
```

---

## 🎯 Recommendation 5: Lock-Free Optimizations

### Current State

**Hot Path**: UDP Tick → Price Store Update → Greeks Calculation

**Potential Bottleneck**: Mutex contention when 1000s of ticks/second

---

### Proposed Optimization

**Double-Buffering for Price Store**:

```cpp
// include/data/LockFreePriceStore.h
template<typename T>
class DoubleBufferedStore {
public:
    // Writer: No lock, writes to back buffer
    void write(const T& data) {
        auto& backBuffer = getBackBuffer();
        backBuffer = data;
        
        // Atomic swap
        m_currentIndex.fetch_xor(1, std::memory_order_release);
    }
    
    // Reader: No lock, reads from front buffer
    T read() const {
        auto& frontBuffer = getFrontBuffer();
        return frontBuffer;  // Copy
    }
    
private:
    std::array<T, 2> m_buffers;
    std::atomic<int> m_currentIndex{0};
    
    T& getBackBuffer() { return m_buffers[m_currentIndex.load()]; }
    const T& getFrontBuffer() const { return m_buffers[m_currentIndex.load() ^ 1]; }
};
```

**Benefits**:
- ✅ Zero locks in hot path
- ✅ Readers never block writers
- ✅ Microsecond latency

**Trade-offs**:
- ⚠️ Readers may see slightly stale data (1 tick old)
- ⚠️ More complex code
- ⚠️ Only suitable for single-writer scenarios

**When to Apply**: Only if profiling shows mutex contention as bottleneck.

---

## 📊 Migration Roadmap

### Phase 1: Foundation (Month 1)
- [ ] Implement ConfigManager with environment support
- [ ] Migrate credentials to separate file/env vars
- [ ] Add Qt logging categories
- [ ] Document new patterns

### Phase 2: Quality (Month 2-3)
- [ ] Add health monitoring system
- [ ] Create mock interfaces for repositories
- [ ] Write first unit tests (target: 20% coverage)
- [ ] Add CI/CD with basic tests

### Phase 3: Performance (Month 4-6)
- [ ] Profile hot paths with VTune/perf
- [ ] Apply lock-free optimizations if needed
- [ ] Benchmark before/after
- [ ] Document performance characteristics

---

## 📚 References

- **Dependency Injection**: Martin Fowler - [Inversion of Control Containers](https://martinfowler.com/articles/injection.html)
- **Qt Logging**: [Qt Documentation - Logging Categories](https://doc.qt.io/qt-5/qloggingcategory.html)
- **Lock-Free Programming**: Anthony Williams - [C++ Concurrency in Action](https://www.manning.com/books/c-plus-plus-concurrency-in-action-second-edition)
- **Config Management**: [12-Factor App Methodology](https://12factor.net/config)

---

**Document Version**: 1.0  
**Last Updated**: February 8, 2026  
**Next Review**: Quarterly
