# Action Items Roadmap - Trading Terminal C++

**Purpose**: Prioritized implementation plan for all findings  
**Timeline**: 3-month detailed plan + 6-month strategic outlook  
**Last Updated**: February 8, 2026

---

## 📊 Quick Reference

### Priority Legend

| Priority | Timeframe | Severity | Examples |
|----------|-----------|----------|----------|
| **P0** 🔴 | 1-3 days | Critical | Security breaches, data corruption |
| **P1** 🟠 | 1 week | High | Race conditions, major bugs |
| **P2** 🟡 | 2-4 weeks | Medium | Memory leaks, performance issues |
| **P3** 🔵 | 1-3 months | Low | Tech debt, quality improvements |
| **P4** 🟣 | 3-6 months | Strategic | Architecture enhancements |

### Issue Count by Priority

- **P0**: 3 issues (Security)
- **P1**: 4 issues (Thread safety, data integrity)
- **P2**: 5 issues (Memory, configuration)
- **P3**: 8 issues (Code quality)
- **P4**: 3 issues (Architecture)

**Total**: 23 issues

---

## 🚨 Week 1: P0 - CRITICAL (Days 1-3)

### Day 1: Emergency Security Response

#### SEC-001: Rotate Exposed API Credentials
**Time**: 2 hours  
**Owner**: Security Lead  
**Blocked by**: None  
**Blocks**: SEC-002, SEC-003

**Tasks**:
1. [ ] Log into XTS broker portal
2. [ ] Identify all exposed credentials:
   ```
   - marketdata_appkey: 0ad2e5b5745f883b275594
   - marketdata_secretkey: Hbem321@hG
   - interactive_appkey: a36a44c17d008920af5719
   - interactive_secretkey: Tiyv603$Pm
   - PLUS 3 historical sets in comments
   ```
3. [ ] Revoke ALL exposed keys immediately
4. [ ] Generate new credentials
5. [ ] Store new credentials in Windows Credential Manager

**Validation**:
```bash
# Verify old credentials no longer work
curl -H "Authorization: <old_key>" https://api.endpoint
# Expected: 401 Unauthorized
```

**Rollback Plan**: Keep new credentials ready, but don't revoke old until new tested.

---

#### SEC-002: Damage Assessment
**Time**: 3 hours  
**Owner**: Security Lead  
**Blocked by**: None  
**Blocks**: None

**Tasks**:
1. [ ] Contact XTS support for API access logs
   ```
   Request logs for:
   - Last 90 days
   - All API keys found in config.ini
   - Group by IP address
   ```
2. [ ] Compare with legitimate usage patterns
3. [ ] Document any unauthorized access
4. [ ] File incident report if breach detected

**Output**: `INCIDENT_REPORT_2026-02-08.md` (if needed)

---

#### SEC-003: Move Credentials to Secure Storage
**Time**: 4 hours  
**Owner**: Developer  
**Blocked by**: SEC-001 (need new credentials)  
**Blocks**: None

**Implementation**:

```cpp
// Step 1: Create SecureCredentialStore utility
// File: include/utils/SecureCredentialStore.h
// [See CRITICAL_SECURITY_ISSUES.md for full implementation]

// Step 2: Update ConfigLoader
QString ConfigLoader::getApiKey() {
    // Priority: Windows Credential Manager > Env Var > Config File
    QString key = SecureCredentialStore::getCredential("marketdata_appkey");
    if (!key.isEmpty()) return key;
    
    key = qgetenv("XTS_MD_APPKEY");
    if (!key.isEmpty()) return key;
    
    // Fallback (log warning)
    qWarning() << "[SECURITY] Using insecure file-based credentials!";
    QSettings settings("configs/config.ini", QSettings::IniFormat);
    return settings.value("CREDENTIALS/marketdata_appkey", "").toString();
}

// Step 3: Setup utility
// File: tools/setup_credentials.cpp
// [See CRITICAL_SECURITY_ISSUES.md for implementation]
```

**Testing**:
```powershell
# Test 1: Setup credentials
.\setup_credentials.exe
# Enter new credentials

# Test 2: Verify storage
cmdkey /list | Select-String "TradingTerminal"

# Test 3: Launch app
.\TradingTerminal.exe
# Should load credentials from Windows Credential Manager
```

**Git Hygiene**:
```bash
# Remove config.ini from git
git rm --cached configs/config.ini
echo "configs/config.ini" >> .gitignore

# Create template
cp configs/config.ini configs/config.ini.template
# Manually replace secrets with ${ENV:VAR_NAME}

git add configs/config.ini.template .gitignore
git commit -m "security: Move credentials to environment/secure storage"
```

---

### Days 2-3: Verify & Document

#### SEC-004: Security Audit
**Time**: 4 hours  
**Owner**: Security Lead

**Checklist**:
- [ ] No credentials in `configs/config.ini`
- [ ] `config.ini` added to `.gitignore`
- [ ] `config.ini.template` created with placeholders
- [ ] Credentials loaded from Windows Credential Manager
- [ ] Fallback to environment variables works
- [ ] Warning logged if using file-based credentials
- [ ] Old credentials disabled in XTS portal
- [ ] Team trained on new process

**Deliverable**: `SECURITY_AUDIT_CHECKLIST.md`

---

## 🟠 Week 2: P1 - HIGH Priority

### RACE-001: Replace getUnifiedState() with getUnifiedSnapshot()

**Time**: 2 days  
**Owner**: Core Developer  
**Blocked by**: None  
**Impact**: Fixes data corruption in Greeks and UI

---

#### Day 1: Audit Code

**Step 1: Find All Usages**
```bash
cd /path/to/trading_terminal_cpp
grep -rn "getUnifiedState" --include="*.cpp" --include="*.h" > audit_unified_state.txt

# Example output:
# src/services/UdpBroadcastService.cpp:445: auto* data = nsefo::g_nseFoPriceStore.getUnifiedState(token);
# src/ui/OptionChainWindow.cpp:890: const auto* state = nsefo::g_nseFoPriceStore.getUnifiedState(token);
```

**Step 2: Categorize**
```markdown
### Unsafe Usages (NEEDS FIX)
1. src/services/UdpBroadcastService.cpp:445
2. src/ui/OptionChainWindow.cpp:890
3. src/services/ATMWatchManager.cpp:201

### Safe Usages (Already using snapshot)
1. src/services/GreeksCalculationService.cpp:250 ✅
```

---

#### Day 2: Implement Fixes

**Pattern Replacement**:

```cpp
// File 1: src/services/UdpBroadcastService.cpp:445
// ❌ BEFORE
auto touchlineCallback = [this](int32_t token) {
    const auto *data = nsefo::g_nseFoPriceStore.getUnifiedState(token);
    if (!data) return;
    
    UDP::MarketTick tick = convertNseFoUnified(*data);
    emit udpTickReceived(tick);
};

// ✅ AFTER
auto touchlineCallback = [this](int32_t token) {
    auto snapshot = nsefo::g_nseFoPriceStore.getUnifiedSnapshot(token);
    if (snapshot.token == 0) return;  // Not found
    
    UDP::MarketTick tick = convertNseFoUnified(snapshot);
    emit udpTickReceived(tick);
};
```

**Testing**:
```cpp
// After each fix, test:
1. Launch app
2. Subscribe to token
3. Verify no crashes
4. Check Greeks calculation still works
5. Run with ThreadSanitizer (if available)
```

---

#### Day 2.5: Deprecate Unsafe API

```cpp
// lib/cpp_broacast_nsefo/include/nsefo_price_store.h

/**
 * @deprecated Use getUnifiedSnapshot() for thread-safe access
 * 
 * This method returns a raw pointer that becomes invalid after the lock
 * is released, creating race conditions. Use getUnifiedSnapshot() instead,
 * which returns a copy under lock.
 * 
 * This method will be removed in version 2.0.
 */
[[deprecated("Use getUnifiedSnapshot() for thread-safe access")]]
const UnifiedTokenState* getUnifiedState(uint32_t token) const;
```

**Compiler Flags**:
```cmake
# CMakeLists.txt
if(MSVC)
    add_compile_options(/W4 /WX)  # Treat warnings as errors
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

# Enable deprecation warnings as errors
add_compile_options(-Werror=deprecated-declarations)
```

---

### SINGLE-001: Fix Singleton Thread Safety

**Time**: 1 day  
**Owner**: Core Developer  
**Blocked by**: None

---

#### Files to Fix

1. **RepositoryManager** (`include/repository/RepositoryManager.h:64`)
2. **MasterDataState** (`include/services/MasterDataState.h:40`)
3. **ATMWatchManager** (`include/services/ATMWatchManager.h:52`)

---

#### Implementation (Recommended: Meyer's Singleton)

**Before**:
```cpp
// ❌ UNSAFE
class RepositoryManager {
public:
    static RepositoryManager* getInstance() {
        if (!s_instance) {
            s_instance = new RepositoryManager();
        }
        return s_instance;
    }
private:
    static RepositoryManager* s_instance;
};

RepositoryManager* RepositoryManager::s_instance = nullptr;
```

**After**:
```cpp
// ✅ THREAD-SAFE (Meyer's Singleton)
class RepositoryManager {
public:
    static RepositoryManager& getInstance() {
        static RepositoryManager instance;
        return instance;
    }
    
    // Delete copy/move
    RepositoryManager(const RepositoryManager&) = delete;
    RepositoryManager& operator=(const RepositoryManager&) = delete;
    RepositoryManager(RepositoryManager&&) = delete;
    RepositoryManager& operator=(RepositoryManager&&) = delete;
    
private:
    RepositoryManager() = default;
    ~RepositoryManager() = default;
};
```

**Update All Callsites**:
```cpp
// ❌ BEFORE (pointer)
RepositoryManager* repo = RepositoryManager::getInstance();
auto* contract = repo->getContract(token, segment);

// ✅ AFTER (reference)
RepositoryManager& repo = RepositoryManager::getInstance();
auto* contract = repo.getContract(token, segment);
```

**Testing**:
```cpp
// tests/test_singleton_thread_safety.cpp
TEST(SingletonTest, ConcurrentInit) {
    std::vector<std::thread> threads;
    std::set<RepositoryManager*> instances;
    std::mutex mtx;
    
    // Spawn 100 threads trying to get instance simultaneously
    for (int i = 0; i < 100; i++) {
        threads.emplace_back([&]() {
            RepositoryManager& inst = RepositoryManager::getInstance();
            std::lock_guard<std::mutex> lock(mtx);
            instances.insert(&inst);
        });
    }
    
    for (auto& t : threads) t.join();
    
    // All should get SAME instance
    EXPECT_EQ(instances.size(), 1);
}
```

---

### GREEK-001: Add Error States to GreeksResult

**Time**: 1 day  
**Owner**: Domain Expert  
**Blocked by**: None

---

#### Enhancement

**Current**:
```cpp
struct GreeksResult {
    double impliedVolatility = 0.0;
    double delta = 0.0;
    // ... other fields
    int64_t calculationTimestamp = 0;
};

// Problem: Can't distinguish between:
// - Greeks = 0 (calculated correctly)
// - Greeks calculation failed (should show N/A)
```

**Proposed**:
```cpp
struct GreeksResult {
    enum class Status {
        Success,        // Calculated successfully
        Failed,         // Calculation failed (error in inputs)
        Pending,        // Not yet calculated
        Stale           // Cached value too old
    };
    
    Status status = Status::Pending;
    QString errorMessage;  // Only if status == Failed
    
    // Existing fields...
    double impliedVolatility = 0.0;
    double delta = 0.0;
    double gamma = 0.0;
    double vega = 0.0;
    double theta = 0.0;
    double spotPrice = 0.0;
    double strikePrice = 0.0;
    double timeToExpiry = 0.0;
    double theoreticalPrice = 0.0;
    int64_t calculationTimestamp = 0;
    
    // Validation details
    bool ivConverged = false;
    int ivIterations = 0;
};
```

**Update Calculation Service**:
```cpp
GreeksResult GreeksCalculationService::calculateForToken(uint32_t token, int exchangeSegment) {
    GreeksResult result;
    result.status = GreeksResult::Status::Pending;
    
    // Validation
    if (underlyingPrice <= 0) {
        result.status = GreeksResult::Status::Failed;
        result.errorMessage = "Underlying price unavailable";
        emit calculationFailed(token, exchangeSegment, result.errorMessage);
        return result;
    }
    
    if (T <= 0) {
        result.status = GreeksResult::Status::Failed;
        result.errorMessage = "Option expired";
        emit calculationFailed(token, exchangeSegment, result.errorMessage);
        return result;
    }
    
    // Calculate...
    result.impliedVolatility = ivResult.iv;
    result.status = GreeksResult::Status::Success;
    result.errorMessage = "";
    
    return result;
}
```

**Update UI (OptionChainWindow)**:
```cpp
void OptionChainWindow::onGreeksCalculated(uint32_t token, int segment, const GreeksResult& result) {
    int row = findRowForToken(token);
    if (row < 0) return;
    
    switch (result.status) {
        case GreeksResult::Status::Success:
            m_model->setData(row, COL_IV, QString::number(result.impliedVolatility, 'f', 2));
            m_model->setData(row, COL_DELTA, QString::number(result.delta, 'f', 4));
            // ...
            break;
            
        case GreeksResult::Status::Failed:
            m_model->setData(row, COL_IV, "N/A");
            m_model->setData(row, COL_DELTA, "N/A");
            // Set tooltip with error message
            m_tableView->setToolTip(row, COL_IV, result.errorMessage);
            break;
            
        case GreeksResult::Status::Pending:
            m_model->setData(row, COL_IV, "...");
            m_model->setData(row, COL_DELTA, "...");
            break;
            
        case GreeksResult::Status::Stale:
            // Grey out stale values
            m_model->setData(row, COL_IV, 
                QString::number(result.impliedVolatility, 'f', 2));
            m_model->setForeground(row, COL_IV, QColor(128, 128, 128));  // Grey
            break;
    }
}
```

---

## 🟡 Weeks 3-4: P2 - MEDIUM Priority

### MEM-001: Refactor main.cpp Memory Ownership

**Time**: 3 days  
**Owner**: Senior Developer

---

#### Targets

1. **SplashScreen** - Convert to smart pointer
2. **ConfigLoader** - Convert to smart pointer or stack
3. **LoginWindow** - Add `Qt::WA_DeleteOnClose`
4. **MainWindow** - Already correct, document

---

#### Implementation

**File**: `src/main.cpp`

**Before** (Lines 68-140):
```cpp
SplashScreen *splash = new SplashScreen();
splash->showCentered();

ConfigLoader *config = new ConfigLoader();
config->loadFile("...");

LoginWindow *loginWindow = new LoginWindow();
if (loginResult == SUCCESS) {
    delete loginWindow;
}

MainWindow *mainWindow = new MainWindow(nullptr);
mainWindow->setAttribute(Qt::WA_DeleteOnClose);
mainWindow->show();
```

**After**:
```cpp
// Splash screen (short-lived, non-parented widget)
auto splash = std::make_unique<SplashScreen>();
splash->showCentered();
splash->setStatus("Loading...");

// Config loader (not a QObject)
auto config = std::make_unique<ConfigLoader>();
config->loadFile("configs/config.ini");

// Login window (Qt-managed, self-deleting)
auto loginWindow = new LoginWindow();  // OK - Qt manages
loginWindow->setAttribute(Qt::WA_DeleteOnClose);
int loginResult = loginWindow->exec();

// Main window (Qt-managed, self-deleting)
auto mainWindow = new MainWindow(nullptr);  // OK - Qt manages
mainWindow->setAttribute(Qt::WA_DeleteOnClose);
mainWindow->show();

return app.exec();
// ✅ splash and config auto-deleted when out of scope
```

---

### CONFIG-001: Standardize base_price_mode

**Time**: 1 day  
**Owner**: Business Analyst + Developer

---

#### Issue

```ini
[ATM_WATCH]
base_price_mode = future  # Uses future for strike selection

[GREEKS_CALCULATION]
base_price_mode = cash    # Uses spot for Greeks
```

**Question**: Is this intentional or bug?

---

#### Investigation

**Interview Questions**:
1. **Trading Strategy**: Do users trade based on spot or futures?
2. **ATM Definition**: Should ATM strike be based on spot or future?
3. **Greeks Standard**: Black-Scholes typically uses spot - any reason to deviate?

**Recommendation**:

**Option A - Intentional** (Document):
```ini
# ATM strike selection uses FUTURE price
# Rationale: F&O traders use futures as reference
[ATM_WATCH]
base_price_mode = future

# Greeks calculation uses SPOT price
# Rationale: Standard Black-Scholes assumption
[GREEKS_CALCULATION]
base_price_mode = cash
```

**Option B - Bug** (Fix):
```ini
# Use consistent pricing basis
[ATM_WATCH]
base_price_mode = future

[GREEKS_CALCULATION]
base_price_mode = future  # ← Changed to match
```

---

### UDP-001: Add Packet Loss Detection

**Time**: 2 days  
**Owner**: Network Developer

---

#### Implementation

**File**: `lib/cpp_broadcast_nsefo/src/multicast_receiver.cpp`

**Add**:
```cpp
class PacketLossMonitor {
public:
    void onPacketReceived(uint32_t seqNo) {
        if (m_lastSeqNo > 0 && seqNo != m_lastSeqNo + 1) {
            uint32_t lost = seqNo - m_lastSeqNo - 1;
            m_totalLost += lost;
            
            qWarning() << "[UDP] Packet loss detected:"
                       << "Expected:" << (m_lastSeqNo + 1)
                       << "Got:" << seqNo
                       << "Lost:" << lost;
            
            emit packetLossDetected(lost, seqNo);
        }
        
        m_lastSeqNo = seqNo;
        m_totalReceived++;
    }
    
    double getLossRate() const {
        if (m_totalReceived == 0) return 0.0;
        return (double)m_totalLost / (m_totalReceived + m_totalLost);
    }
    
signals:
    void packetLossDetected(uint32_t lostCount, uint32_t currentSeq);
    
private:
    uint32_t m_lastSeqNo = 0;
    uint64_t m_totalReceived = 0;
    uint64_t m_totalLost = 0;
};
```

**UI Alert**:
```cpp
// MainWindow status bar
connect(&udpService, &UdpService::packetLossDetected,
        [this](uint32_t lost, uint32_t seq) {
            if (lost > 10) {  // Only alert if significant loss
                QString msg = QString("⚠ UDP packet loss: %1 packets").arg(lost);
                m_statusBar->showMessage(msg, 5000);  // 5 seconds
            }
        });
```

---

## 🔵 Month 2: P3 - LOW Priority (Code Quality)

### DEBUG-001: Standardize Logging

**Time**: 1 week  
**Owner**: Core Developer

See [ARCHITECTURE_RECOMMENDATIONS.md](ARCHITECTURE_RECOMMENDATIONS.md#recommendation-3-structured-logging)

**Steps**:
1. Create `include/utils/Logging.h` with Qt categories
2. Replace all `qDebug() << "[Component]..."` with category logging
3. Add config.ini `[LOGGING]` section
4. Document usage in `CODING_STANDARDS.md`

---

### ARCH-001: Reduce Singleton Count

**Time**: 2 weeks  
**Owner**: Architect + Team

**Current Singletons** (10):
1. GreeksCalculationService ✅ (Keep - global service)
2. UdpBroadcastService ✅ (Keep - global service)
3. FeedHandler ✅ (Keep - global service)
4. RepositoryManager ⚠️ (Consider DI)
5. MasterDataState ⚠️ (Consider DI)
6. ATMWatchManager ⚠️ (Consider DI)
7. WindowManager ✅ (Keep - UI coordination)
8. PreferencesManager ✅ (Keep - global state)
9. SoundManager ✅ (Keep - global resource)
10. ConfigLoader ❌ (Should NOT be singleton)

**Action**:
1. Document why each singleton is necessary
2. Convert 3-4 to dependency injection
3. Add testing benefits

---

## 🟣 Months 3-6: P4 - STRATEGIC

### ARCH-002: Implement Dependency Injection

See [ARCHITECTURE_RECOMMENDATIONS.md](ARCHITECTURE_RECOMMENDATIONS.md#recommendation-2-dependency-injection)

**Timeline**: 2 months  
**Benefits**: Testability, loose coupling

---

### ARCH-003: Comprehensive Testing

**Timeline**: 3 months  
**Target**: 60% code coverage

**Framework**: Google Test + Google Mock

```cmake
# CMakeLists.txt
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(test_greeks tests/test_greeks.cpp)
target_link_libraries(test_greeks gtest_main gmock_main)
```

**Test Categories**:
1. Unit tests (Greeks calculation, IV solver)
2. Integration tests (UDP → Price Store → Greeks → UI)
3. Thread safety tests (Stress, race detection)
4. Performance tests (Latency, throughput)

---

### ARCH-004: Lock-Free Price Store

**Timeline**: 1 month (if profiling shows need)  
**Prerequisite**: Performance profiling

See [ARCHITECTURE_RECOMMENDATIONS.md](ARCHITECTURE_RECOMMENDATIONS.md#recommendation-5-lock-free-optimizations)

---

## 📊 Progress Tracking

### Dashboard (Update Weekly)

```markdown
| Priority | Total | Completed | In Progress | Blocked | Not Started |
|----------|-------|-----------|-------------|---------|-------------|
| P0       | 3     | 0         | 0           | 0       | 3           |
| P1       | 4     | 0         | 0           | 0       | 4           |
| P2       | 5     | 0         | 0           | 0       | 5           |
| P3       | 8     | 0         | 0           | 0       | 8           |
| P4       | 3     | 0         | 0           | 0       | 3           |
| **Total**| **23**| **0**     | **0**       | **0**   | **23**      |
```

### Gantt Chart (High-Level)

```
Week:      1    2    3    4    5-8   9-12  13-24
───────────────────────────────────────────────────
P0 SEC  [████]
P1 RACE      [████]
P1 SINGLE    [██]
P1 GREEK      [██]
P2 MEM           [████]
P2 CONFIG        [██]
P2 UDP             [████]
P3 DEBUG                [████████]
P3 ARCH                      [████████████]
P4 TEST                              [████████████████]
```

---

## 📋 Success Metrics

### Week 1 (P0 Complete)
- ✅ All API credentials rotated
- ✅ No credentials in git repository
- ✅ Credentials loaded from secure storage
- ✅ Security audit checklist completed

### Month 1 (P0 + P1 Complete)
- ✅ No race conditions (verified by ThreadSanitizer)
- ✅ All singletons thread-safe
- ✅ Greeks error states implemented
- ✅ Zero `[[deprecated]]` warnings in build

### Month 2 (P0 + P1 + P2 Complete)
- ✅ Zero memory leaks (verified by Valgrind/Dr. Memory)
- ✅ Packet loss detection working
- ✅ Configuration standardized and documented

### Month 6 (All Complete)
- ✅ 60%+ unit test coverage
- ✅ Dependency injection implemented
- ✅ Structured logging throughout codebase
- ✅ Performance benchmarks documented

---

## 🚀 Getting Started

### Today (Right Now)

1. **Read** all analysis documents:
   - [CODEBASE_ANALYSIS_REPORT.md](CODEBASE_ANALYSIS_REPORT.md)
   - [CRITICAL_SECURITY_ISSUES.md](CRITICAL_SECURITY_ISSUES.md)
   - [THREAD_SAFETY_AUDIT.md](THREAD_SAFETY_AUDIT.md)

2. **Assess** team capacity and assign owners

3. **Start** with Day 1 tasks (credential rotation)

4. **Schedule** daily standups for Week 1 (P0 work)

---

## 📞 Support

For questions about specific issues:
- **Security**: See [CRITICAL_SECURITY_ISSUES.md](CRITICAL_SECURITY_ISSUES.md)
- **Thread Safety**: See [THREAD_SAFETY_AUDIT.md](THREAD_SAFETY_AUDIT.md)
- **Memory**: See [MEMORY_MANAGEMENT_REVIEW.md](MEMORY_MANAGEMENT_REVIEW.md)
- **Architecture**: See [ARCHITECTURE_RECOMMENDATIONS.md](ARCHITECTURE_RECOMMENDATIONS.md)

---

**Document Version**: 1.0  
**Last Updated**: February 8, 2026  
**Next Review**: Weekly during P0/P1, monthly thereafter
