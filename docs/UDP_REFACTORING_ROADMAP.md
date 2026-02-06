# UDP System Refactoring - Implementation Roadmap

**Project**: UDP Reader, Price Store, and Greeks Integration Overhaul  
**Start Date**: February 6, 2026  
**Status**: Planning Phase  
**Owner**: Development Team  

---

## Executive Summary

This roadmap addresses critical issues in the UDP broadcast system:
- **Race conditions** in price store reads (data corruption risk)
- **Performance waste** from unified callbacks (16x data overhead)
- **Missing integration** of enhanced messages (17201, 17202)
- **Policy mismatch** in Greeks calculation (throttled vs per-feed requirement)

**Estimated Duration**: 6-8 weeks  
**Risk Level**: High (core data path changes)  
**Business Impact**: Critical (data integrity, performance, feature completeness)

---

## Phase Overview

| Phase | Focus | Duration | Dependencies | Risk |
|-------|-------|----------|--------------|------|
| **Phase 0** | Enhanced Message Integration | 1 week | None | Medium |
| **Phase 1** | Thread-Safe Snapshot API | 2 weeks | Phase 0 | High |
| **Phase 2** | Type-Specific Callbacks | 2 weeks | Phase 1 | Medium |
| **Phase 3** | Greeks Per-Feed Policy | 1 week | Phase 1, 2 | Low |
| **Phase 4** | UI Optimization | 1-2 weeks | Phase 2, 3 | Low |

**Critical Path**: Phase 0 → Phase 1 → Phase 2 → Phase 3

---

## PHASE 0: Enhanced Message Integration (Week 1)

**Objective**: Wire enhanced messages (17201, 17202) into UdpBroadcastService callback system

**Status**: 🔴 NOT STARTED

### Tasks

#### 0.1 Register Enhanced Ticker Callback (17202)
- [ ] **Task**: Add callback registration in `UdpBroadcastService::start()`
- [ ] **File**: `src/services/UdpBroadcastService.cpp`
- [ ] **Code Location**: After line ~335 (existing callback registrations)
- [ ] **Implementation**:
  ```cpp
  // Register enhanced ticker callback (17202)
  callbacks.registerTickerCallback([this](uint32_t token) {
      handleEnhancedTickerUpdate(token);
  });
  ```
- [ ] **Testing**: Verify 17202 messages trigger Greeks calculation
- [ ] **Priority**: CRITICAL
- [ ] **Estimated Time**: 4 hours

#### 0.2 Register Enhanced Market Watch Callback (17201)
- [ ] **Task**: Add callback for enhanced market watch
- [ ] **File**: `src/services/UdpBroadcastService.cpp`
- [ ] **Implementation**:
  ```cpp
  // Register enhanced market watch callback (17201)
  callbacks.registerMarketWatchCallback([this](uint32_t token) {
      handleEnhancedMarketWatchUpdate(token);
  });
  ```
- [ ] **Priority**: HIGH
- [ ] **Estimated Time**: 4 hours

#### 0.3 Implement Handler Methods
- [ ] **Task**: Create `handleEnhancedTickerUpdate()` and `handleEnhancedMarketWatchUpdate()`
- [ ] **File**: `src/services/UdpBroadcastService.cpp`
- [ ] **Logic**: Fetch state, convert, dispatch to FeedHandler, trigger Greeks
- [ ] **Priority**: CRITICAL
- [ ] **Estimated Time**: 8 hours

#### 0.4 Testing & Validation
- [ ] **Unit Test**: Mock 17202 message, verify callback triggered
- [ ] **Integration Test**: Live UDP feed with enhanced messages
- [ ] **Greeks Test**: Confirm Greeks calculated for 17202 updates
- [ ] **Load Test**: Verify no performance degradation
- [ ] **Estimated Time**: 1 day

### Deliverables
- ✅ Enhanced messages trigger callbacks
- ✅ Greeks calculated for 17202 updates
- ✅ No silent failures for enhanced protocol symbols
- ✅ Test coverage > 80%

### Risk Mitigation
- **Risk**: Enhanced messages have different field layouts
  - **Mitigation**: Use existing converters that handle enhanced fields
- **Risk**: Performance impact from additional callbacks
  - **Mitigation**: Monitor with profiling, callbacks are already lightweight

---

## PHASE 1: Thread-Safe Snapshot API (Weeks 2-3)

**Objective**: Eliminate race conditions by implementing copy-based snapshot reads

**Status**: 🔴 NOT STARTED  
**Depends On**: Phase 0 (optional, can run parallel)

### Tasks

#### 1.1 Design Snapshot API
- [ ] **Task**: Define `getUnifiedSnapshot()` signature
- [ ] **File**: `lib/cpp_broacast_nsefo/include/nsefo_price_store.h`
- [ ] **Signature**:
  ```cpp
  [[nodiscard]] UnifiedTokenState getUnifiedSnapshot(uint32_t token) const;
  ```
- [ ] **Priority**: CRITICAL
- [ ] **Estimated Time**: 2 hours

#### 1.2 Implement NSEFO Snapshot
- [ ] **Task**: Implement snapshot in nsefo::PriceStore
- [ ] **File**: `lib/cpp_broacast_nsefo/src/nsefo_price_store.cpp`
- [ ] **Implementation**:
  ```cpp
  UnifiedTokenState PriceStore::getUnifiedSnapshot(uint32_t token) const {
      std::shared_lock lock(m_rwlock);
      size_t index = token - 35000;
      if (index >= MAX_TOKENS) return {};
      return m_unifiedStates[index]; // Copy under lock
  }
  ```
- [ ] **Priority**: CRITICAL
- [ ] **Estimated Time**: 4 hours

#### 1.3 Implement NSECM Snapshot
- [ ] **Task**: Mirror implementation for nsecm::PriceStore
- [ ] **File**: `lib/cpp_broadcast_nsecm/src/nsecm_price_store.cpp`
- [ ] **Priority**: HIGH
- [ ] **Estimated Time**: 3 hours

#### 1.4 Implement BSEFO/BSECM Snapshots
- [ ] **Task**: Implement for BSE stores
- [ ] **Files**: `lib/cpp_broadcast_bsefo/*`, `lib/cpp_broadcast_bsecm/*`
- [ ] **Priority**: MEDIUM
- [ ] **Estimated Time**: 6 hours

#### 1.5 Update UdpBroadcastService
- [ ] **Task**: Replace `getUnifiedState()` with `getUnifiedSnapshot()`
- [ ] **File**: `src/services/UdpBroadcastService.cpp`
- [ ] **Location**: Line ~400 (unified callback)
- [ ] **Change**:
  ```cpp
  // OLD: auto* data = priceStore->getUnifiedState(token);
  // NEW:
  auto data = priceStore->getUnifiedSnapshot(token);
  ```
- [ ] **Priority**: CRITICAL
- [ ] **Estimated Time**: 2 hours

#### 1.6 Update GreeksCalculationService
- [ ] **Task**: Use snapshot API
- [ ] **File**: `src/services/GreeksCalculationService.cpp`
- [ ] **Priority**: CRITICAL
- [ ] **Estimated Time**: 2 hours

#### 1.7 Update All UI Components
- [ ] **Task**: Find and replace all `getUnifiedState()` calls
- [ ] **Files**: `src/views/*`, `src/ui/*`
- [ ] **Tool**: Use grep to find all callsites
- [ ] **Priority**: HIGH
- [ ] **Estimated Time**: 1 day

#### 1.8 Testing & Validation
- [ ] **Concurrency Test**: 100 threads reading, 10 threads writing
- [ ] **Stress Test**: 100k updates/sec, verify no crashes
- [ ] **Data Integrity Test**: Verify no torn reads under load
- [ ] **Performance Test**: Measure snapshot copy overhead (expect <100ns)
- [ ] **Estimated Time**: 2 days

### Deliverables
- ✅ Zero race conditions in price reads
- ✅ All components use snapshot API
- ✅ Thread sanitizer clean
- ✅ Performance degradation < 5%

### Risk Mitigation
- **Risk**: Copy overhead impacts performance
  - **Mitigation**: 400 bytes @ 3.2 GHz = ~25 CPU cycles, negligible
  - **Fallback**: Implement reference-counted smart pointer
- **Risk**: Missing callsites cause crashes
  - **Mitigation**: Deprecate old API with `[[deprecated]]`, compiler warnings

---

## PHASE 2: Type-Specific Callbacks (Weeks 4-5)

**Objective**: Split unified callback into type-specific handlers for granular events

**Status**: 🔴 NOT STARTED  
**Depends On**: Phase 1 (snapshot API must be stable)

### Tasks

#### 2.1 Define Update Type Enum
- [ ] **Task**: Add `UpdateType` enum
- [ ] **File**: `include/data/UDPTypes.h`
- [ ] **Definition**:
  ```cpp
  enum class UpdateType : uint8_t {
      TRADE_TICK = 0,      // 7202, 17202 (LTP, volume, OI change)
      DEPTH_UPDATE = 1,    // 7208 (order book only)
      TOUCHLINE = 2,       // 7200 (BBO + basic stats)
      OI_CHANGE = 3,       // OI-only update
      FULL_SNAPSHOT = 4,   // Complete state refresh
      UNKNOWN = 255
  };
  ```
- [ ] **Priority**: HIGH
- [ ] **Estimated Time**: 1 hour

#### 2.2 Extend MarketTick Structure
- [ ] **Task**: Add fields to `UDP::MarketTick`
- [ ] **File**: `include/data/UDPTypes.h`
- [ ] **Fields**:
  ```cpp
  UpdateType updateType = UpdateType::UNKNOWN;
  uint32_t validFlags = 0; // Bitmask: LTP|BID|ASK|DEPTH|OI|VOLUME
  ```
- [ ] **Priority**: HIGH
- [ ] **Estimated Time**: 2 hours

#### 2.3 Implement Type-Specific Callbacks
- [ ] **Task**: Create separate callback lambdas for each message type
- [ ] **File**: `src/services/UdpBroadcastService.cpp`
- [ ] **Callbacks**:
  - `touchlineCallback` (7200) → `UpdateType::TOUCHLINE`
  - `tickerCallback` (7202, 17202) → `UpdateType::TRADE_TICK`
  - `depthCallback` (7208) → `UpdateType::DEPTH_UPDATE`
- [ ] **Priority**: CRITICAL
- [ ] **Estimated Time**: 1 day

#### 2.4 Update Converters
- [ ] **Task**: Set `updateType` and `validFlags` in converter functions
- [ ] **File**: `src/services/UdpBroadcastService.cpp`
- [ ] **Logic**:
  ```cpp
  tick.updateType = UpdateType::TRADE_TICK;
  tick.validFlags = VALID_LTP | VALID_VOLUME | VALID_OI;
  ```
- [ ] **Priority**: HIGH
- [ ] **Estimated Time**: 4 hours

#### 2.5 Split UI Signal Emissions
- [ ] **Task**: Add `onDepthUpdate`, `onTradeUpdate` signals to FeedHandler
- [ ] **File**: `include/services/FeedHandler.h`
- [ ] **Signals**:
  ```cpp
  signals:
      void depthUpdateReceived(const UDP::MarketTick& tick);
      void tradeUpdateReceived(const UDP::MarketTick& tick);
      void touchlineUpdateReceived(const UDP::MarketTick& tick);
  ```
- [ ] **Priority**: MEDIUM
- [ ] **Estimated Time**: 3 hours

#### 2.6 Update UI Subscribers
- [ ] **Task**: Connect to specific signals based on need
- [ ] **Files**: `src/views/MarketWatch.cpp`, `src/ui/DepthWindow.cpp`
- [ ] **Example**:
  ```cpp
  // DepthWindow only subscribes to depth updates
  connect(feedHandler, &FeedHandler::depthUpdateReceived,
          this, &DepthWindow::onDepthUpdate);
  ```
- [ ] **Priority**: MEDIUM
- [ ] **Estimated Time**: 1 day

#### 2.7 Testing & Validation
- [ ] **Unit Test**: Verify correct UpdateType for each message
- [ ] **Integration Test**: Confirm UI receives only relevant events
- [ ] **Performance Test**: Measure reduction in unnecessary UI updates
- [ ] **Regression Test**: Ensure all existing features work
- [ ] **Estimated Time**: 2 days

### Deliverables
- ✅ Type-specific callbacks for 7200, 7202, 7208, 17201, 17202
- ✅ UI components receive only relevant events
- ✅ Correct `messageType` field in MarketTick
- ✅ 60-70% reduction in unnecessary UI updates

### Risk Mitigation
- **Risk**: Breaking existing UI logic
  - **Mitigation**: Keep legacy `udpTickReceived` signal during transition
  - **Rollout**: Gradual migration, deprecate after 1 release
- **Risk**: Missed message types
  - **Mitigation**: Add telemetry for UNKNOWN updateType, alert on occurrence

---

## PHASE 3: Greeks Per-Feed Policy (Week 6)

**Objective**: Remove throttling and enable Greeks calculation on every option feed

**Status**: 🔴 NOT STARTED  
**Depends On**: Phase 1 (snapshot API), Phase 2 (type-specific callbacks)

### Tasks

#### 3.1 Add Configuration Flag
- [ ] **Task**: Add `calculateOnEveryFeed` to GreeksConfig
- [ ] **File**: `include/services/GreeksCalculationService.h`
- [ ] **Field**:
  ```cpp
  struct GreeksConfig {
      bool calculateOnEveryFeed = false; // New flag
      // ... existing fields
  };
  ```
- [ ] **Priority**: HIGH
- [ ] **Estimated Time**: 1 hour

#### 3.2 Implement Forced Calculation Path
- [ ] **Task**: Bypass throttle when `calculateOnEveryFeed = true`
- [ ] **File**: `src/services/GreeksCalculationService.cpp`
- [ ] **Logic**:
  ```cpp
  void onPriceUpdate(uint32_t token, double ltp, int exchangeSegment) {
      if (!m_config.enabled || !m_config.autoCalculate) return;
      
      if (m_config.calculateOnEveryFeed) {
          // Force calculation, skip throttle
          calculateForToken(token, exchangeSegment);
          return;
      }
      
      // Legacy throttle logic for backward compatibility
      // ...
  }
  ```
- [ ] **Priority**: CRITICAL
- [ ] **Estimated Time**: 4 hours

#### 3.3 Remove LTP > 0 Condition
- [ ] **Task**: Allow Greeks for zero-premium options
- [ ] **File**: `src/services/UdpBroadcastService.cpp`
- [ ] **Change**:
  ```cpp
  // OLD: if (greeksService.isEnabled() && data->ltp > 0)
  // NEW:
  if (greeksService.isEnabled()) {
      greeksService.onPriceUpdate(token, data.ltp, 2);
  }
  ```
- [ ] **Priority**: MEDIUM
- [ ] **Estimated Time**: 1 hour

#### 3.4 Optimize for Trade-Only Triggers
- [ ] **Task**: Only call Greeks on TRADE_TICK updates
- [ ] **File**: `src/services/UdpBroadcastService.cpp`
- [ ] **Logic**:
  ```cpp
  if (tick.updateType == UpdateType::TRADE_TICK) {
      greeksService.onPriceUpdate(token, tick.ltp, 2);
  }
  ```
- [ ] **Priority**: HIGH
- [ ] **Estimated Time**: 2 hours

#### 3.5 Config UI/Persistence
- [ ] **Task**: Add setting to config.ini and preferences dialog
- [ ] **Files**: `configs/config.ini`, `src/ui/PreferencesDialog.cpp`
- [ ] **Default**: `calculateOnEveryFeed=true` for new installs
- [ ] **Priority**: MEDIUM
- [ ] **Estimated Time**: 3 hours

#### 3.6 Testing & Validation
- [ ] **Load Test**: 5000 options @ 100 updates/sec, measure CPU
- [ ] **Accuracy Test**: Compare Greeks with throttled vs per-feed
- [ ] **Latency Test**: Measure Greeks calculation time (target <50μs)
- [ ] **Memory Test**: Monitor for leaks under sustained load
- [ ] **Estimated Time**: 2 days

### Deliverables
- ✅ Greeks calculated on every option feed
- ✅ Zero-premium options handled correctly
- ✅ No unnecessary Greeks for depth-only updates
- ✅ CPU impact < 15% under normal load

### Risk Mitigation
- **Risk**: CPU overload with 5000+ options
  - **Mitigation**: Profile and optimize Black-Scholes computation
  - **Fallback**: Implement token-level rate limiting (per-token throttle)
- **Risk**: Accuracy issues with rapid updates
  - **Mitigation**: Atomic reads of price + timestamp in snapshot

---

## PHASE 4: UI Optimization (Week 7-8)

**Objective**: Optimize UI to handle granular events efficiently

**Status**: 🔴 NOT STARTED  
**Depends On**: Phase 2 (type-specific callbacks), Phase 3 (Greeks per-feed)

### Tasks

#### 4.1 Audit UI Event Handlers
- [ ] **Task**: Identify which components need which event types
- [ ] **Files**: All in `src/views/*`, `src/ui/*`
- [ ] **Deliverable**: Component → Event Type mapping document
- [ ] **Priority**: HIGH
- [ ] **Estimated Time**: 1 day

#### 4.2 Optimize MarketWatch
- [ ] **Task**: Subscribe to trade + touchline only (skip depth)
- [ ] **File**: `src/views/MarketWatch.cpp`
- [ ] **Expected Gain**: 40-50% fewer updates
- [ ] **Priority**: HIGH
- [ ] **Estimated Time**: 4 hours

#### 4.3 Optimize DepthWindow
- [ ] **Task**: Subscribe to depth + trade only (skip touchline)
- [ ] **File**: `src/ui/DepthWindow.cpp`
- [ ] **Expected Gain**: 30% fewer updates
- [ ] **Priority**: HIGH
- [ ] **Estimated Time**: 4 hours

#### 4.4 Optimize OptionChain
- [ ] **Task**: Subscribe to trade + Greeks only
- [ ] **File**: `src/views/OptionChain.cpp`
- [ ] **Expected Gain**: 60% fewer updates (no depth processing)
- [ ] **Priority**: MEDIUM
- [ ] **Estimated Time**: 6 hours

#### 4.5 Add Update Batching
- [ ] **Task**: Batch rapid updates (10ms window) to reduce redraws
- [ ] **File**: `src/ui/MarketDataWidget.cpp`
- [ ] **Implementation**: Use QTimer::singleShot for coalescing
- [ ] **Priority**: MEDIUM
- [ ] **Estimated Time**: 1 day

#### 4.6 Profile and Measure
- [ ] **Task**: Compare UI performance before/after
- [ ] **Metrics**: FPS, repaint count, event queue depth
- [ ] **Target**: 60 FPS maintained with 1000+ symbols
- [ ] **Priority**: HIGH
- [ ] **Estimated Time**: 1 day

#### 4.7 Testing & Validation
- [ ] **Stress Test**: 2000 symbols, all updating simultaneously
- [ ] **Visual Test**: No UI jank or missed updates
- [ ] **Regression Test**: All features functional
- [ ] **Estimated Time**: 2 days

### Deliverables
- ✅ UI components optimized for event types
- ✅ 40-60% reduction in unnecessary updates
- ✅ Smooth 60 FPS under load
- ✅ No functional regressions

### Risk Mitigation
- **Risk**: Breaking existing UI behavior
  - **Mitigation**: A/B testing with feature flag
  - **Rollback**: Keep legacy path available
- **Risk**: Missing critical updates due to batching
  - **Mitigation**: Flush batch on user interaction

---

## Testing Strategy

### Unit Tests
- **Coverage Target**: 85% minimum
- **Critical Paths**: Snapshot API, callback dispatch, Greeks calculation
- **Tools**: Google Test, Qt Test framework
- **Estimated Time**: Ongoing, 2 days per phase

### Integration Tests
- **Scenarios**:
  - Live UDP feed processing (all message types)
  - Multi-exchange simultaneous updates
  - Enhanced message (17201, 17202) handling
  - Concurrent reads/writes to price store
- **Tools**: Custom UDP simulator, mock exchanges
- **Estimated Time**: 3 days

### Performance Tests
- **Benchmarks**:
  - Snapshot copy overhead (target <100ns)
  - Greeks calculation latency (target <50μs)
  - UI frame rate under load (target 60 FPS)
  - Memory footprint (target <500MB for 10k symbols)
- **Tools**: Google Benchmark, QElapsedTimer, Valgrind
- **Estimated Time**: 2 days

### Load Tests
- **Scenarios**:
  - 10,000 symbols @ 100 updates/sec
  - 1000 concurrent Greeks calculations
  - UI with 2000 visible rows
- **Duration**: 1 hour continuous load
- **Success Criteria**: No crashes, <10% CPU increase
- **Estimated Time**: 1 day

### Regression Tests
- **Scope**: All existing features
- **Execution**: Automated nightly builds
- **Tools**: CI/CD pipeline (GitHub Actions / Jenkins)
- **Estimated Time**: Ongoing

---

## Risk Assessment

### Critical Risks

| Risk | Impact | Probability | Mitigation | Contingency |
|------|--------|-------------|------------|-------------|
| Race condition in snapshot API | HIGH | MEDIUM | Extensive testing, thread sanitizer | Revert to locked reads |
| Performance degradation from copies | MEDIUM | LOW | Profiling, micro-benchmarks | Smart pointers with ref counting |
| Breaking existing UI logic | HIGH | MEDIUM | Gradual rollout, feature flags | Keep legacy signals active |
| Greeks CPU overload | MEDIUM | MEDIUM | Per-token rate limiting | Config to disable per-feed |
| Missing enhanced message types | HIGH | LOW | Comprehensive parser audit | Add telemetry for unknown types |

### Technical Debt

- **Old API Deprecation**: Remove `getUnifiedState()` after 2 releases
- **Legacy Throttling**: Keep for backward compatibility, remove in v3.0
- **Unified Callback**: Deprecate after Phase 2 complete

---

## Success Metrics

### Correctness
- ✅ Zero race conditions (thread sanitizer clean)
- ✅ Zero data corruption under load
- ✅ 100% enhanced message coverage

### Performance
- ✅ Snapshot overhead < 100ns (99th percentile)
- ✅ Greeks latency < 50μs (99th percentile)
- ✅ UI maintains 60 FPS with 2000 symbols
- ✅ Memory usage < 500MB for 10k symbols

### Completeness
- ✅ Greeks calculated on every option feed
- ✅ UI receives only relevant events
- ✅ Enhanced messages (17201, 17202) fully integrated
- ✅ All tests passing (unit, integration, performance)

### User Experience
- ✅ No visible lag or jank
- ✅ Accurate real-time Greeks
- ✅ Lower CPU usage overall (despite per-feed Greeks)

---

## Progress Tracking

### Weekly Checkpoints

**Week 1** (Feb 6-13, 2026):
- [ ] Phase 0 complete
- [ ] Enhanced messages wired and tested
- [ ] Greeks triggered for 17202 updates

**Week 2** (Feb 14-20, 2026):
- [ ] Phase 1: 50% (API design, NSEFO implementation)
- [ ] Snapshot API functional for NSEFO

**Week 3** (Feb 21-27, 2026):
- [ ] Phase 1: 100% (all stores, all callsites updated)
- [ ] Thread sanitizer clean

**Week 4** (Feb 28 - Mar 6, 2026):
- [ ] Phase 2: 50% (type enum, callbacks split)
- [ ] Type-specific callbacks functional

**Week 5** (Mar 7-13, 2026):
- [ ] Phase 2: 100% (UI signals, converters updated)
- [ ] Integration tests passing

**Week 6** (Mar 14-20, 2026):
- [ ] Phase 3: 100% (Greeks per-feed enabled)
- [ ] Performance tests passing

**Week 7-8** (Mar 21 - Apr 3, 2026):
- [ ] Phase 4: 100% (UI optimization complete)
- [ ] Final regression testing
- [ ] Production deployment

### Daily Standup Questions
1. What did you complete yesterday?
2. What are you working on today?
3. Any blockers or risks?
4. Test results from yesterday?

---

## Deployment Plan

### Staging Rollout
- **Week 7**: Deploy to staging environment
- **Duration**: 3 days monitoring
- **Criteria**: Zero crashes, all metrics green

### Production Rollout
- **Week 8**: Phased production deployment
- **Phase 1**: 10% users (1 day)
- **Phase 2**: 50% users (2 days)
- **Phase 3**: 100% users (final)
- **Rollback Criteria**: >5% crash rate, >20% performance degradation

### Monitoring
- **Metrics**: CPU usage, memory, update latency, FPS
- **Alerts**: Crash rate, error rate, performance anomalies
- **Tools**: Application Insights, custom telemetry

---

## Stakeholder Communication

### Weekly Reports
- **Audience**: Project manager, technical lead
- **Content**: Progress, risks, blockers, metrics
- **Format**: Email summary + dashboard link

### Sprint Demos
- **Frequency**: End of each phase
- **Audience**: All stakeholders
- **Content**: Live demo, metrics, Q&A

### Go/No-Go Decision
- **Date**: End of Week 7
- **Criteria**: All tests passing, no critical bugs
- **Decision Makers**: Technical lead, product owner

---

## Appendix

### Related Documents
- [UDP_PRICE_STORE_GREEKS_ANALYSIS.md](./UDP_PRICE_STORE_GREEKS_ANALYSIS.md) - Detailed technical analysis
- [POST_LOGIN_INITIALIZATION_ARCHITECTURE.md](./POST_LOGIN_INITIALIZATION_ARCHITECTURE.md) - System initialization
- [docs/exchange_docs/nse_trimm_protocol_fo.md](./exchange_docs/nse_trimm_protocol_fo.md) - Protocol specification

### Key Contacts
- **Technical Lead**: [Name]
- **QA Lead**: [Name]
- **DevOps**: [Name]

### Tools & Environment
- **IDE**: VS Code with C++ extensions
- **Build System**: CMake 3.20+
- **Compiler**: MSVC 2019 / GCC 11+ / Clang 14+
- **Testing**: Google Test, Qt Test
- **Profiling**: Visual Studio Profiler, perf
- **CI/CD**: GitHub Actions / Jenkins

---

**Document Version**: 1.0  
**Last Updated**: February 6, 2026  
**Next Review**: Weekly during implementation
