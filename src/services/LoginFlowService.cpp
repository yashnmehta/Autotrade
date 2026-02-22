#include "services/LoginFlowService.h"
#include "data/PriceStoreGateway.h"
#include "repository/RepositoryManager.h"
#include "services/MasterDataState.h"
#include "services/TradingDataService.h"
#include "utils/PreferencesManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>

LoginFlowService::LoginFlowService(QObject *parent)
    : QObject(parent), m_mdClient(nullptr), m_iaClient(nullptr),
      m_tradingDataService(nullptr), m_masterLoader(nullptr),
      m_statusCallback(nullptr), m_errorCallback(nullptr),
      m_completeCallback(nullptr) {
  // Create master loader worker
  m_masterLoader = new MasterLoaderWorker(this);

  // Connect worker signals to our handlers (thread-safe via Qt's signal/slot
  // mechanism)
  connect(m_masterLoader, &MasterLoaderWorker::loadingProgress, this,
          [this](int percentage, const QString &message) {
            updateStatus("masters", message,
                         70 + (percentage /
                               10)); // Map 0-100% to 70-80% overall progress
          });

  connect(m_masterLoader, &MasterLoaderWorker::loadingComplete, this,
          &LoginFlowService::handleMasterLoadingComplete);

  connect(m_masterLoader, &MasterLoaderWorker::loadingFailed, this,
          &LoginFlowService::handleMasterLoadingFailed);
}

LoginFlowService::~LoginFlowService() {
  // Cancel any ongoing master loading operation
  if (m_masterLoader && m_masterLoader->isRunning()) {
    m_masterLoader->cancel();
    m_masterLoader->wait(3000); // Wait up to 3 seconds for clean shutdown
  }

  delete m_mdClient;
  delete m_iaClient;
  // m_masterLoader will be deleted automatically (parent is 'this')
}

void LoginFlowService::setStatusCallback(
    std::function<void(const QString &, const QString &, int)> callback) {
  m_statusCallback = callback;
}

void LoginFlowService::setErrorCallback(
    std::function<void(const QString &, const QString &)> callback) {
  m_errorCallback = callback;
}

void LoginFlowService::setCompleteCallback(std::function<void()> callback) {
  m_completeCallback = callback;
}

void LoginFlowService::setFetchErrorCallback(
    std::function<void(const FetchError &)> callback) {
  m_fetchErrorCallback = callback;
}

void LoginFlowService::startMastersPhase() {
  // Phase 3: Check shared state and handle masters accordingly
  MasterDataState *masterState = MasterDataState::getInstance();

  QString mastersDir = RepositoryManager::getMastersDirectory();

  QDir().mkpath(mastersDir);

  // Check if splash screen is still loading masters
  if (masterState->isLoading()) {
    qDebug()
        << "[LoginFlow] Masters are being loaded by splash screen, waiting...";
    updateStatus("masters", "Waiting for master loading...", 70);

    // Connect to state change signal to continue when loading completes
    connect(
        masterState, &MasterDataState::mastersReady, this,
        [this](int contractCount) {
          // PRIORITY: If user requested download, discard background load and
          // download fresh
          if (m_downloadMasters) {
            qDebug() << "[LoginFlow] Background load complete, but user "
                        "requested download. Proceeding to download.";
            QString mastersDir = RepositoryManager::getMastersDirectory();
            startMasterDownload(mastersDir);
            return;
          }

          qDebug() << "[LoginFlow] Masters loading completed with"
                   << contractCount << "contracts";
          updateStatus(
              "masters",
              QString("Masters ready (%1 contracts)").arg(contractCount), 80);
          emit mastersLoaded();
          continueLoginAfterMasters();
        },
        Qt::UniqueConnection);

    connect(
        masterState, &MasterDataState::loadingError, this,
        [this](const QString &error) {
          qWarning() << "[LoginFlow] Background loading failed:" << error;

          // If user requested download, proceed with download
          if (m_downloadMasters) {
            QString mastersDir = RepositoryManager::getMastersDirectory();
            startMasterDownload(mastersDir);
          } else {
            // Otherwise skip and continue
            updateStatus("masters", "No masters available (continuing)", 75);
            continueLoginAfterMasters();
          }
        },
        Qt::UniqueConnection);

    return;
  }

  // PRIORITY: User requested download
  if (m_downloadMasters) {
    startMasterDownload(mastersDir);
    return;
  }

  // Check if masters were already loaded by splash screen (and we didn't want
  // to download)
  if (masterState->areMastersLoaded()) {
    qDebug() << "[LoginFlow] Masters already loaded by splash screen ("
             << masterState->getContractCount() << "contracts), skipping load";
    updateStatus("masters",
                 QString("Using cached masters (%1 contracts)")
                     .arg(masterState->getContractCount()),
                 80);

    // Emit signal for UI (even though already loaded)
    emit mastersLoaded();

    // Continue directly to next phase
    continueLoginAfterMasters();
    return;
  }

  // Masters not loaded and no download requested -> try loading from cache
  updateStatus("masters", "Checking for cached masters...", 70);
  qDebug() << "[LoginFlow] Attempting to load from cache...";

  masterState->setLoadingStarted();
  m_masterLoader->loadFromCache(mastersDir);
}

void LoginFlowService::executeLogin(
    const QString &mdAppKey, const QString &mdSecretKey,
    const QString &iaAppKey, const QString &iaSecretKey, const QString &loginID,
    bool downloadMasters, const QString &baseURL, const QString &source) {
  m_downloadMasters = downloadMasters;
  m_mdLoggedIn = false;
  m_iaLoggedIn = false;

  updateStatus("init", "Starting login process...", 0);

  // Cleanup previous clients if any to avoid leaks and stale connections
  if (m_mdClient) {
    m_mdClient->disconnectWebSocket();
    delete m_mdClient;
    m_mdClient = nullptr;
  }
  if (m_iaClient) {
    m_iaClient->disconnectWebSocket();
    delete m_iaClient;
    m_iaClient = nullptr;
  }

  // Create API clients with correct base URLs
  QString mdBaseURL = baseURL + "/apimarketdata";
  QString iaBaseURL = baseURL;

  m_mdClient =
      new XTSMarketDataClient(mdBaseURL, mdAppKey, mdSecretKey, source, this);
  m_iaClient =
      new XTSInteractiveClient(iaBaseURL, iaAppKey, iaSecretKey, source, this);

  // Connect Interactive Events to TradingDataService
  if (m_tradingDataService) {
    connect(m_iaClient, &XTSInteractiveClient::orderEvent, m_tradingDataService,
            &TradingDataService::onOrderEvent);
    connect(m_iaClient, &XTSInteractiveClient::tradeEvent, m_tradingDataService,
            &TradingDataService::onTradeEvent);
    connect(m_iaClient, &XTSInteractiveClient::positionEvent,
            m_tradingDataService, &TradingDataService::onPositionEvent);
  }

  // Connect MD login signal
  connect(
      m_mdClient, &XTSMarketDataClient::loginCompleted, this,
      [this](bool success, const QString &message) {
        if (!success) {
          notifyError("md_login", message);
          return;
        }
        m_mdLoggedIn = true;
        updateStatus("md_login", "Market Data API connected", 30);

        if (m_iaLoggedIn) {
          startMastersPhase();
        } else {
          // Still waiting for IA
          updateStatus("ia_login", "Waiting for Interactive API...", 40);
        }
      },
      Qt::UniqueConnection);

  // Connect IA login signal
  connect(
      m_iaClient, &XTSInteractiveClient::loginCompleted, this,
      [this](bool success, const QString &message) {
        if (!success) {
          notifyError("ia_login", message);
          return;
        }
        m_iaLoggedIn = true;
        updateStatus("ia_login", "Interactive API connected", 60);

        if (m_mdLoggedIn) {
          startMastersPhase();
        } else {
          // Still waiting for MD
          updateStatus("md_login", "Waiting for Market Data API...", 50);
        }
      },
      Qt::UniqueConnection);

  // Trigger the actual login
  updateStatus("md_login", "Connecting to Market Data API...", 10);
  updateStatus("ia_login", "Connecting to Interactive API...", 20);

  m_mdClient->login();
  m_iaClient->login();
}

#include <QThread> // Added for thread safety check

void LoginFlowService::updateStatus(const QString &phase,
                                    const QString &message, int progress) {
  // CRITICAL FIX: Ensure UI updates happen on the main thread
  // Network callbacks execute on worker threads and MUST NOT call GUI code
  // directly
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this,
        [this, phase, message, progress]() {
          updateStatus(phase, message, progress);
        },
        Qt::QueuedConnection);
    return;
  }

  qDebug() << "[" << phase << "]" << message << "(" << progress << "%)";

  emit statusUpdate(phase, message, progress);

  if (m_statusCallback) {
    m_statusCallback(phase, message, progress);
  }
}

void LoginFlowService::notifyError(const QString &phase, const QString &error) {
  // CRITICAL FIX: Ensure error notifications happen on the main thread
  // Network callbacks execute on worker threads and MUST NOT call GUI code
  // directly
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, phase, error]() { notifyError(phase, error); },
        Qt::QueuedConnection);
    return;
  }

  qWarning() << "[ERROR:" << phase << "]" << error;

  emit errorOccurred(phase, error);

  if (m_errorCallback) {
    m_errorCallback(phase, error);
  }
}

void LoginFlowService::handleMasterLoadingComplete(int contractCount) {
  qDebug() << "[LoginFlow] Master loading completed with" << contractCount
           << "contracts";
  updateStatus("masters", QString("Loaded %1 contracts").arg(contractCount),
               80);

  // Update shared state
  MasterDataState *state = MasterDataState::getInstance();
  state->setMastersLoaded(contractCount);

  // ========== RE-INITIALIZE PRICECACHE WITH FRESH MASTERS ==========
  // Users may download updated master data in login flow with new tokens,
  // delisted scrips, or lot size changes. Re-initialize zero-copy cache
  // to use the fresh token mappings.
  qDebug() << "[LoginFlow] Re-initializing PriceCacheZeroCopy with fresh "
              "master data...";

  RepositoryManager *repo = RepositoryManager::getInstance();

  // ==================================================================
  // NEW ARCHITECTURE: Initialize Distributed Price Stores
  // ==================================================================
  std::vector<uint32_t> nseCmTokenList, nseFoTokenList, bseCmTokenList,
      bseFoTokenList;

  const auto &nseCmContracts = repo->getContractsBySegment("NSE", "CM");
  for (const auto &contract : nseCmContracts)
    nseCmTokenList.push_back(contract.exchangeInstrumentID);

  const auto &nseFoContracts = repo->getContractsBySegment("NSE", "FO");
  for (const auto &contract : nseFoContracts)
    nseFoTokenList.push_back(contract.exchangeInstrumentID);

  const auto &bseCmContracts = repo->getContractsBySegment("BSE", "CM");
  for (const auto &contract : bseCmContracts)
    bseCmTokenList.push_back(contract.exchangeInstrumentID);

  const auto &bseFoContracts = repo->getContractsBySegment("BSE", "FO");
  for (const auto &contract : bseFoContracts)
    bseFoTokenList.push_back(contract.exchangeInstrumentID);

  MarketData::PriceStoreGateway::instance().initialize(
      nseFoTokenList, nseCmTokenList, bseFoTokenList, bseCmTokenList);

  qDebug() << "[LoginFlow] ✓ Distributed Price Stores initialized via Gateway";
  qDebug() << "    NSE CM:" << nseCmTokenList.size() << "tokens";
  qDebug() << "    NSE FO:" << nseFoTokenList.size() << "tokens";
  qDebug() << "    BSE CM:" << bseCmTokenList.size() << "tokens";
  qDebug() << "    BSE FO:" << bseFoTokenList.size() << "tokens";
  // ==================================================================

  // Emit signal for UI
  emit mastersLoaded();

  // Continue with the rest of the login flow
  continueLoginAfterMasters();
}

void LoginFlowService::handleMasterLoadingFailed(const QString &errorMessage) {
  qWarning() << "[LoginFlow] Master loading failed:" << errorMessage;
  updateStatus("masters",
               QString("Master loading failed: %1").arg(errorMessage), 75);

  // Update shared state
  MasterDataState *state = MasterDataState::getInstance();
  state->setLoadingFailed(errorMessage);

  // Continue anyway (non-fatal error - user can download later)
  continueLoginAfterMasters();
}

void LoginFlowService::startMasterDownload(const QString &mastersDir) {
  updateStatus("masters", "Downloading master contracts...", 70);
  qDebug() << "[LoginFlow] Starting master download...";

  QStringList segments = {"NSEFO", "NSECM", "BSEFO", "BSECM"};

  // Connect to download completed signal
  connect(
      m_mdClient, &XTSMarketDataClient::masterContractsDownloaded, this,
      [this, mastersDir](bool success, const QString &csvData,
                         const QString &error) {
        if (success) {
          qDebug() << "[LoginFlow] ✓ Master download successful (size:"
                   << csvData.size() << "bytes)";
          qDebug()
              << "[LoginFlow] Loading directly from memory - SKIPPING FILE I/O";
          updateStatus("masters", "Loading from memory (no I/O)...", 75);

          // Update shared state
          MasterDataState *state = MasterDataState::getInstance();
          state->setLoadingStarted();

          // OPTIMIZED: Load directly from memory - NO FILE I/O!
          // The downloaded CSV data stays in memory and is loaded directly
          // into RepositoryManager without touching disk (except for optional
          // caching)
          m_masterLoader->loadFromMemoryOnly(csvData, true, mastersDir);

          // Note: Continuation happens in handleMasterLoadingComplete()

        } else {
          qWarning() << "[LoginFlow] Master download failed:" << error;
          updateStatus("masters", "Master download failed (continuing anyway)",
                       75);

          // Update shared state
          MasterDataState *state = MasterDataState::getInstance();
          state->setLoadingFailed(error);

          // Continue with login flow even if master download failed
          // (user can download later from settings)
          continueLoginAfterMasters();
        }
      },
      Qt::UniqueConnection);

  // Trigger the actual download
  m_mdClient->downloadMasterContracts(segments);
}

// ═══════════════════════════════════════════════════════════════════════════
// SNAPSHOT + REPLAY  (data sync after both logins complete)
//
// Problem this solves
// ───────────────────
// REST snapshots (positions/orders/trades) can take hundreds of milliseconds
// on a slow connection or when the book is large.  During that window the
// Interactive WebSocket is not yet connected, so any live order/trade/position
// events that the broker sends ARE LOST FOREVER.
//
// The correct industry-standard solution is the "snapshot + replay" pattern:
//
//   1. Connect IA WebSocket immediately → events flow in and are BUFFERED.
//   2. Fire all three REST snapshot requests in parallel.
//   3. When ALL snapshots have arrived, apply them to TradingDataService
//      as the authoritative baseline.
//   4. Replay every buffered live event on top of the snapshot.
//      Because live events are deltas (upserts), replaying them in arrival
//      order converges to the correct final state.
//   5. From this point forward, live events go directly to TradingDataService
//      (no more buffering needed).
//
// Result: Zero event loss, correct final state, regardless of REST latency.
// ═══════════════════════════════════════════════════════════════════════════

void LoginFlowService::continueLoginAfterMasters() {
  // ─────────────────────────────────────────────────────────────────────────
  // Phase 4: Connect Market Data WebSocket
  // ─────────────────────────────────────────────────────────────────────────
  updateStatus("websocket", "Establishing real-time connection...", 85);

  connect(
      m_mdClient, &XTSMarketDataClient::wsConnectionStatusChanged, this,
      [this](bool success, const QString &message) {
        if (!success) {
          qWarning() << "[LoginFlow] MD WebSocket connection failed:" << message;
          updateStatus("websocket_error",
                       QString("WebSocket error: %1").arg(message), 85);
          return;
        }

        updateStatus("websocket", "Real-time connection established", 90);

        // Step 1 — connect IA socket FIRST so we never miss an event
        connectIASocketAndBuffer();

        // Step 2 — fire REST snapshots in parallel (buffering is now active)
        fetchAllRestSnapshots();
      },
      Qt::UniqueConnection);

  m_mdClient->connectWebSocket();
}

// ─────────────────────────────────────────────────────────────────────────
// connectIASocketAndBuffer
//
// Opens the Interactive WebSocket immediately.  Until applySnapshotAndReplay()
// is called, every incoming event is appended to the in-memory buffer instead
// of being written directly to TradingDataService.  This means ZERO events
// are lost even when the REST snapshot takes several seconds to arrive.
// ─────────────────────────────────────────────────────────────────────────
void LoginFlowService::connectIASocketAndBuffer() {
  m_bufferedPositionEvents.clear();
  m_bufferedOrderEvents.clear();
  m_bufferedTradeEvents.clear();
  m_snapshotApplied   = false;
  m_iaSocketConnected = false;

  updateStatus("ia_socket", "Connecting Interactive Socket...", 91);

  if (m_tradingDataService) {
    // While m_snapshotApplied == false, route events into buffers.
    connect(m_iaClient, &XTSInteractiveClient::orderEvent, this,
            [this](const XTS::Order &order) {
              if (!m_snapshotApplied) {
                m_bufferedOrderEvents.append(order);
                qDebug() << "[LoginFlow] Buffered order event:"
                         << order.appOrderID;
              }
              // After snapshot, TradingDataService's own slot handles it
              // (wired in executeLogin).
            },
            Qt::UniqueConnection);

    connect(m_iaClient, &XTSInteractiveClient::tradeEvent, this,
            [this](const XTS::Trade &trade) {
              if (!m_snapshotApplied) {
                m_bufferedTradeEvents.append(trade);
                qDebug() << "[LoginFlow] Buffered trade event:"
                         << trade.executionID;
              }
            },
            Qt::UniqueConnection);

    connect(m_iaClient, &XTSInteractiveClient::positionEvent, this,
            [this](const XTS::Position &pos) {
              if (!m_snapshotApplied) {
                m_bufferedPositionEvents.append(pos);
                qDebug() << "[LoginFlow] Buffered position event:"
                         << pos.exchangeInstrumentID;
              }
            },
            Qt::UniqueConnection);
  }

  m_iaClient->connectWebSocket([this](bool success, const QString &msg) {
    if (!success) {
      qWarning() << "[LoginFlow] Interactive Socket failed:" << msg;
      // Non-fatal — trading still works for manual orders, continue anyway
    } else {
      qDebug() << "[LoginFlow] Interactive Socket connected — "
                  "buffering live events until REST snapshot is ready";
      m_iaSocketConnected = true;
    }
  });
}

// ─────────────────────────────────────────────────────────────────────────
// fetchAllRestSnapshots
//
// Fires all three REST requests IN PARALLEL.  A timeout timer is started.
// As each response arrives its m_*Fetched flag is set.  Once all three are
// done (or the timeout fires) applySnapshotAndReplay() takes over.
// ─────────────────────────────────────────────────────────────────────────
void LoginFlowService::fetchAllRestSnapshots() {
  m_positionsFetched  = false;
  m_ordersFetched     = false;
  m_tradesFetched     = false;
  m_fetchError        = FetchError{};
  m_snapshotPositions.clear();
  m_snapshotOrders.clear();
  m_snapshotTrades.clear();

  startFetchTimeout();

  updateStatus("data", "Syncing positions, orders & trades...", 93);

  // ── Positions ────────────────────────────────────────────────────────
  QString preferredView =
      PreferencesManager::instance().getPositionBookDefaultView();
  if (preferredView == "Net") preferredView = "NetWise";

  auto doFetchPositions = [this](const QString &view) {
    m_iaClient->getPositions(
        view,
        [this, view](bool ok, const QVector<XTS::Position> &positions,
                     const QString &msg) {
          QMetaObject::invokeMethod(this, [this, ok, view, positions, msg]() {
            if (ok) {
              qDebug() << "[LoginFlow] REST positions OK:" << positions.size()
                       << "(" << view << ")";
              m_snapshotPositions = positions;
              m_positionsFetched  = true;
              applySnapshotAndReplay();
            } else {
              // One automatic fallback: toggle NetWise ↔ DayWise
              QString fallback = (view == "NetWise") ? "DayWise" : "NetWise";
              qWarning() << "[LoginFlow] Positions failed (" << view
                         << "), trying fallback:" << fallback;
              m_iaClient->getPositions(
                  fallback,
                  [this](bool ok2, const QVector<XTS::Position> &p2,
                         const QString &msg2) {
                    QMetaObject::invokeMethod(this, [this, ok2, p2, msg2]() {
                      if (ok2) {
                        qDebug() << "[LoginFlow] REST positions fallback OK:"
                                 << p2.size();
                        m_snapshotPositions = p2;
                      } else {
                        qWarning()
                            << "[LoginFlow] Positions fallback failed:" << msg2;
                        m_fetchError.positionsFailed = true;
                        m_fetchError.messages
                            << QString("Positions: %1").arg(msg2);
                      }
                      m_positionsFetched = true;
                      applySnapshotAndReplay();
                    }, Qt::QueuedConnection);
                  });
            }
          }, Qt::QueuedConnection);
        });
  };
  doFetchPositions(preferredView);

  // ── Orders (parallel) ────────────────────────────────────────────────
  m_iaClient->getOrders([this](bool ok, const QVector<XTS::Order> &orders,
                                const QString &msg) {
    QMetaObject::invokeMethod(this, [this, ok, orders, msg]() {
      if (ok) {
        qDebug() << "[LoginFlow] REST orders OK:" << orders.size();
        m_snapshotOrders = orders;
      } else {
        qWarning() << "[LoginFlow] REST orders failed:" << msg;
        m_fetchError.ordersFailed = true;
        m_fetchError.messages << QString("Orders: %1").arg(msg);
      }
      m_ordersFetched = true;
      applySnapshotAndReplay();
    }, Qt::QueuedConnection);
  });

  // ── Trades (parallel) ────────────────────────────────────────────────
  m_iaClient->getTrades([this](bool ok, const QVector<XTS::Trade> &trades,
                                const QString &msg) {
    QMetaObject::invokeMethod(this, [this, ok, trades, msg]() {
      if (ok) {
        qDebug() << "[LoginFlow] REST trades OK:" << trades.size();
        m_snapshotTrades = trades;
      } else {
        qWarning() << "[LoginFlow] REST trades failed:" << msg;
        m_fetchError.tradesFailed = true;
        m_fetchError.messages << QString("Trades: %1").arg(msg);
      }
      m_tradesFetched = true;
      applySnapshotAndReplay();
    }, Qt::QueuedConnection);
  });
}

// ─────────────────────────────────────────────────────────────────────────
// applySnapshotAndReplay
//
// Called after EACH REST response lands.  Does nothing until all three are
// done (or the timeout marks remaining ones as failed).
//
//   1. Store REST snapshots in TradingDataService.
//   2. Replay buffered live events (in arrival order) on top.
//   3. Set m_snapshotApplied = true — future events go directly to service.
//   4. Emit dataSyncComplete so the UI can reflect data quality.
//   5a. If all succeeded → fire completeCallback ("Continue" button shown).
//   5b. If any failed    → fire fetchErrorCallback so UI shows Retry dialog.
// ─────────────────────────────────────────────────────────────────────────
void LoginFlowService::applySnapshotAndReplay() {
  if (!m_positionsFetched || !m_ordersFetched || !m_tradesFetched)
    return;                     // not all done yet — wait for the others

  if (m_snapshotApplied) return; // guard against double-call (timeout race)
  m_snapshotApplied = true;

  cancelFetchTimeout();

  qDebug() << "[LoginFlow] ✅ All REST snapshots received. Applying baseline...";
  qDebug() << "[LoginFlow]   positions=" << m_snapshotPositions.size()
           << " orders=" << m_snapshotOrders.size()
           << " trades=" << m_snapshotTrades.size();

  if (m_tradingDataService) {
    // ── 1. Apply REST baselines ───────────────────────────────────────
    m_tradingDataService->setPositions(m_snapshotPositions);
    m_tradingDataService->setOrders(m_snapshotOrders);
    m_tradingDataService->setTrades(m_snapshotTrades);

    // ── 2. Replay buffered live events (arrived while REST was in flight)
    int rPos = m_bufferedPositionEvents.size();
    int rOrd = m_bufferedOrderEvents.size();
    int rTrd = m_bufferedTradeEvents.size();

    for (const auto &p : std::as_const(m_bufferedPositionEvents))
      m_tradingDataService->onPositionEvent(p);
    for (const auto &o : std::as_const(m_bufferedOrderEvents))
      m_tradingDataService->onOrderEvent(o);
    for (const auto &t : std::as_const(m_bufferedTradeEvents))
      m_tradingDataService->onTradeEvent(t);

    qDebug() << "[LoginFlow] Replay done —"
             << rPos << "position," << rOrd << "order,"
             << rTrd << "trade events replayed on top of snapshot";
  }

  // ── 3. Clear buffers — no longer needed ──────────────────────────────
  m_bufferedPositionEvents.clear();
  m_bufferedOrderEvents.clear();
  m_bufferedTradeEvents.clear();

  // ── 4. Notify UI about sync quality ──────────────────────────────────
  bool allOk = !m_fetchError.anyFailed();
  emit dataSyncComplete(allOk, m_fetchError);

  if (!allOk) {
    // ── 5b. One or more fetches failed — show Retry dialog ───────────
    qWarning() << "[LoginFlow] ⚠ Partial sync failure:" << m_fetchError.summary();
    updateStatus("data_error",
                 QString("Data sync incomplete: %1").arg(m_fetchError.summary()),
                 95);
    if (m_fetchErrorCallback) {
      m_fetchErrorCallback(m_fetchError);
      // completeCallback is NOT called here — the user must Retry or
      // explicitly choose to proceed via the dialog in main.cpp.
      return;
    }
    // If no fetchErrorCallback registered, fall through and complete anyway
    // (backward-compatible behaviour for callers that don't handle it yet)
    qWarning() << "[LoginFlow] No fetchErrorCallback registered — "
                  "proceeding with partial data";
  }

  // ── 5a. All succeeded (or no error handler) — complete ───────────────
  if (!m_loginCompleted) {
    m_loginCompleted = true;
    updateStatus("complete", "Login successful!", 100);
    emit loginComplete();
    if (m_completeCallback) m_completeCallback();
  }
}

// ─────────────────────────────────────────────────────────────────────────
// retryDataFetch
//
// Called when the user clicks "Retry" after a partial fetch failure.
// The IA WebSocket is already connected; we re-arm the buffer and
// re-fire all three REST requests.
// ─────────────────────────────────────────────────────────────────────────
void LoginFlowService::retryDataFetch() {
  qDebug() << "[LoginFlow] User requested retry — re-fetching all data...";

  m_snapshotApplied = false;
  m_loginCompleted  = false;
  m_bufferedPositionEvents.clear();
  m_bufferedOrderEvents.clear();
  m_bufferedTradeEvents.clear();

  updateStatus("data", "Retrying data sync...", 90);

  fetchAllRestSnapshots(); // re-fires all three REST calls + new timeout guard
}

// ─────────────────────────────────────────────────────────────────────────
// Timeout helpers
// ─────────────────────────────────────────────────────────────────────────
void LoginFlowService::startFetchTimeout() {
  cancelFetchTimeout();
  m_fetchTimeoutTimer = new QTimer(this);
  m_fetchTimeoutTimer->setSingleShot(true);
  connect(m_fetchTimeoutTimer, &QTimer::timeout,
          this, &LoginFlowService::onFetchTimeout);
  m_fetchTimeoutTimer->start(kFetchTimeoutMs);
  qDebug() << "[LoginFlow] Fetch timeout armed:" << kFetchTimeoutMs << "ms";
}

void LoginFlowService::cancelFetchTimeout() {
  if (m_fetchTimeoutTimer) {
    m_fetchTimeoutTimer->stop();
    m_fetchTimeoutTimer->deleteLater();
    m_fetchTimeoutTimer = nullptr;
  }
}

void LoginFlowService::onFetchTimeout() {
  m_fetchTimeoutTimer = nullptr; // already fired

  if (m_snapshotApplied) return; // race: completed just before timeout

  qWarning() << "[LoginFlow] ⏱ Fetch timeout after" << kFetchTimeoutMs
             << "ms! positions=" << m_positionsFetched
             << " orders=" << m_ordersFetched
             << " trades=" << m_tradesFetched;

  const QString timeoutMsg =
      QString("timed out after %1s").arg(kFetchTimeoutMs / 1000);

  if (!m_positionsFetched) {
    m_positionsFetched = true;
    m_fetchError.positionsFailed = true;
    m_fetchError.messages << "Positions: " + timeoutMsg;
  }
  if (!m_ordersFetched) {
    m_ordersFetched = true;
    m_fetchError.ordersFailed = true;
    m_fetchError.messages << "Orders: " + timeoutMsg;
  }
  if (!m_tradesFetched) {
    m_tradesFetched = true;
    m_fetchError.tradesFailed = true;
    m_fetchError.messages << "Trades: " + timeoutMsg;
  }

  applySnapshotAndReplay(); // all flags now true → will proceed
}

          updateStatus("websocket_error",
                       QString("WebSocket error: %1").arg(message), 85);
          return; // STOP HERE — no point fetching data without a live feed
        }

        updateStatus("websocket", "Real-time connection established", 90);

        // ─────────────────────────────────────────────────────────────────
        // Phase 5: Fetch REST snapshots BEFORE connecting Interactive socket
        //
        // ORDERING CONTRACT (critical for correctness):
        //   getPositions → getOrders → getTrades → connectWebSocket(IA)
        //
        // Connecting the Interactive socket LAST ensures that every live
        // on_order / on_trade / on_position event arrives AFTER the full
        // REST snapshot is already stored in TradingDataService.  Reversing
        // this order causes the first live event to be upserted into an empty
        // collection, then immediately overwritten when the REST callback fires.
        // ─────────────────────────────────────────────────────────────────

        updateStatus("data", "Loading positions...", 91);

        // --- Fetch positions ---
        QString preferredView =
            PreferencesManager::instance().getPositionBookDefaultView();
        if (preferredView == "Net")
          preferredView = "NetWise"; // map UI label → API string

        qDebug() << "[LoginFlowService] Fetching positions with preference:"
                 << preferredView;

        m_iaClient->getPositions(
            preferredView,
            [this, preferredView](bool success,
                                  const QVector<XTS::Position> &positions,
                                  const QString &message) {
              if (success) {
                qDebug() << "[LoginFlowService] Loaded" << positions.size()
                         << preferredView << "positions";
                if (m_tradingDataService)
                  m_tradingDataService->setPositions(positions);
              } else {
                qWarning() << "[LoginFlowService] Failed to load positions:"
                           << message;

                // Fallback: toggle NetWise ↔ DayWise
                QString fallbackView =
                    (preferredView == "NetWise") ? "DayWise" : "NetWise";
                qWarning() << "[LoginFlowService] Attempting fallback to:"
                           << fallbackView;

                m_iaClient->getPositions(
                    fallbackView,
                    [this, fallbackView](bool success,
                                        const QVector<XTS::Position> &positions,
                                        const QString &message) {
                      if (success && m_tradingDataService) {
                        qDebug()
                            << "[LoginFlowService] Loaded" << positions.size()
                            << fallbackView << "positions (fallback)";
                        m_tradingDataService->setPositions(positions);
                      } else {
                        qWarning()
                            << "[LoginFlowService] Positions fallback failed:"
                            << message;
                      }
                    });
              }

              // --- Fetch orders ---
              updateStatus("data", "Loading orders...", 93);
              m_iaClient->getOrders([this](bool success,
                                           const QVector<XTS::Order> &orders,
                                           const QString &message) {
                if (success) {
                  qDebug() << "[LoginFlowService] Loaded" << orders.size()
                           << "orders";
                  if (m_tradingDataService)
                    m_tradingDataService->setOrders(orders);
                } else {
                  qWarning() << "[LoginFlowService] Failed to load orders:"
                             << message;
                }

                // --- Fetch trades ---
                updateStatus("data", "Loading trades...", 96);
                m_iaClient->getTrades([this](bool success,
                                             const QVector<XTS::Trade> &trades,
                                             const QString &message) {
                  // Network callbacks run on worker threads — marshal to main
                  QMetaObject::invokeMethod(
                      QCoreApplication::instance(),
                      [this, success, trades, message]() {
                        if (success) {
                          qDebug() << "[LoginFlowService] Loaded"
                                   << trades.size() << "trades";
                          if (m_tradingDataService)
                            m_tradingDataService->setTrades(trades);
                        } else {
                          qWarning()
                              << "[LoginFlowService] Failed to load trades:"
                              << message;
                        }

                        // ─────────────────────────────────────────────────
                        // Phase 6: Connect Interactive WebSocket LAST
                        //
                        // All REST snapshots (positions, orders, trades) are
                        // now stored.  Only NOW do we open the live stream so
                        // incoming events safely upsert into a populated store.
                        // ─────────────────────────────────────────────────
                        updateStatus("ia_socket",
                                     "Connecting Interactive Socket...", 98);
                        m_iaClient->connectWebSocket(
                            [this](bool success, const QString &msg) {
                              if (!success) {
                                qWarning() << "[LoginFlowService] Interactive "
                                              "Socket Failed:"
                                           << msg;
                              } else {
                                qDebug() << "[LoginFlowService] Interactive "
                                            "Socket Connected";
                              }

                              // Complete regardless of IA socket result —
                              // trading is still usable for manual orders
                              updateStatus("complete", "Login successful!", 100);
                              emit loginComplete();
                              if (m_completeCallback)
                                m_completeCallback();
                            });
                      },
                      Qt::QueuedConnection);
                });
              });
            });

        // ── Startup timeout guard ─────────────────────────────────────────
        // If any REST callback never fires (network timeout, server error),
        // the login flow would stall forever.  This safety timer ensures the
        // complete callback fires after at most 20 seconds.
        QTimer *timeoutGuard = new QTimer(this);
        timeoutGuard->setSingleShot(true);
        connect(timeoutGuard, &QTimer::timeout, this, [this, timeoutGuard]() {
          timeoutGuard->deleteLater();
          // Only fire if login hasn't completed yet
          if (m_completeCallback) {
            qWarning() << "[LoginFlowService] ⚠ Data fetch timeout — "
                          "proceeding with partial data. "
                          "Positions/orders/trades may be incomplete.";
            updateStatus("complete", "Login complete (partial data)", 100);
            emit loginComplete();
            m_completeCallback();
            m_completeCallback = nullptr; // ensure it fires only once
          }
        });
        timeoutGuard->start(20000); // 20-second safety net
      },
      Qt::UniqueConnection);

  // Trigger the actual MD WebSocket connection
  m_mdClient->connectWebSocket();
}
