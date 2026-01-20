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

void LoginFlowService::continueLoginAfterMasters() {
  // Phase 4: Connect WebSocket
  updateStatus("websocket", "Establishing real-time connection...", 85);

  // Connect to WebSocket status signal
  connect(
      m_mdClient, &XTSMarketDataClient::wsConnectionStatusChanged, this,
      [this](bool success, const QString &message) {
        if (!success) {
          // Non-fatal: log warning but continue
          qWarning() << "[LoginFlow] WebSocket connection failed:" << message;
        }

        updateStatus("websocket", "Real-time connection established", 90);

        // Phase 5: Fetch initial data
        updateStatus("data", "Loading account data...", 92);

        // Fetch positions based on user preference
        QString preferredView =
            PreferencesManager::instance().getPositionBookDefaultView();
        // Map "Net" -> "NetWise" to match API expected string
        if (preferredView == "Net")
          preferredView = "NetWise";

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

                // Store in TradingDataService
                if (m_tradingDataService) {
                  m_tradingDataService->setPositions(positions);
                }
              } else {
                qWarning() << "[LoginFlowService] Failed to load positions:"
                           << message;

                // Fallback logic: Toggle between NetWise and DayWise
                QString fallbackView =
                    (preferredView == "NetWise") ? "DayWise" : "NetWise";
                qWarning() << "[LoginFlowService] Attempting fallback to:"
                           << fallbackView;

                m_iaClient->getPositions(
                    fallbackView,
                    [this, fallbackView](
                        bool success, const QVector<XTS::Position> &positions,
                        const QString &message) {
                      if (success && m_tradingDataService) {
                        qDebug()
                            << "[LoginFlowService] Loaded" << positions.size()
                            << fallbackView << "positions (fallback)";
                        m_tradingDataService->setPositions(positions);
                      } else {
                        qWarning() << "[LoginFlowService] Failed to load "
                                      "positions during fallback:"
                                   << message;
                      }
                    });
              }

              // Fetch orders
              updateStatus("data", "Loading orders...", 94);
              m_iaClient->getOrders([this](bool success,
                                           const QVector<XTS::Order> &orders,
                                           const QString &message) {
                if (success) {
                  qDebug() << "[LoginFlowService] Loaded" << orders.size()
                           << "orders";
                  if (m_tradingDataService)
                    m_tradingDataService->setOrders(orders);
                } else {
                  qWarning()
                      << "[LoginFlowService] Failed to load orders:" << message;
                }

                // Connect Interactive Socket
                updateStatus("ia_socket", "Connecting Interactive Socket...",
                             96);
                m_iaClient->connectWebSocket([this](bool success,
                                                    const QString &msg) {
                  if (!success)
                    qWarning()
                        << "[LoginFlowService] Interactive Socket Failed:"
                        << msg;
                  else
                    qDebug()
                        << "[LoginFlowService] Interactive Socket Connected";
                });

                // Fetch trades
                updateStatus("data", "Loading trades...", 98);
                m_iaClient->getTrades([this](bool success,
                                             const QVector<XTS::Trade> &trades,
                                             const QString &message) {
                  // CRITICAL FIX: Network callbacks run on worker threads!
                  // Must marshal ALL GUI operations back to main thread
                  QMetaObject::invokeMethod(
                      QCoreApplication::instance(),
                      [this, success, trades, message]() {
                        if (success) {
                          qDebug() << "[LoginFlowService] Loaded"
                                   << trades.size() << "trades";

                          // Store in TradingDataService
                          if (m_tradingDataService) {
                            m_tradingDataService->setTrades(trades);
                          }
                        } else {
                          qWarning() << "[LoginFlowService] Failed to load "
                                        "trades: success="
                                     << success << "message=" << message;
                        }

                        // Complete! - This calls updateStatus which has its own
                        // thread check
                        updateStatus("complete", "Login successful!", 100);
                        emit loginComplete();

                        if (m_completeCallback) {
                          m_completeCallback();
                        }
                      },
                      Qt::QueuedConnection);
                });
              });
            });
      },
      Qt::UniqueConnection);

  // Trigger the actual WebSocket connection
  m_mdClient->connectWebSocket();
}
