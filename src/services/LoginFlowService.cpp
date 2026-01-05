#include "services/LoginFlowService.h"
#include "services/TradingDataService.h"
#include "services/MasterDataState.h"
#include "repository/RepositoryManager.h"
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

LoginFlowService::LoginFlowService(QObject *parent)
    : QObject(parent)
    , m_mdClient(nullptr)
    , m_iaClient(nullptr)
    , m_tradingDataService(nullptr)
    , m_masterLoader(nullptr)
    , m_statusCallback(nullptr)
    , m_errorCallback(nullptr)
    , m_completeCallback(nullptr)
{
    // Create master loader worker
    m_masterLoader = new MasterLoaderWorker(this);
    
    // Connect worker signals to our handlers (thread-safe via Qt's signal/slot mechanism)
    connect(m_masterLoader, &MasterLoaderWorker::loadingProgress, 
            this, [this](int percentage, const QString& message) {
        updateStatus("masters", message, 70 + (percentage / 10)); // Map 0-100% to 70-80% overall progress
    });
    
    connect(m_masterLoader, &MasterLoaderWorker::loadingComplete,
            this, &LoginFlowService::handleMasterLoadingComplete);
    
    connect(m_masterLoader, &MasterLoaderWorker::loadingFailed,
            this, &LoginFlowService::handleMasterLoadingFailed);
}

LoginFlowService::~LoginFlowService()
{
    // Cancel any ongoing master loading operation
    if (m_masterLoader && m_masterLoader->isRunning()) {
        m_masterLoader->cancel();
        m_masterLoader->wait(3000); // Wait up to 3 seconds for clean shutdown
    }
    
    delete m_mdClient;
    delete m_iaClient;
    // m_masterLoader will be deleted automatically (parent is 'this')
}

void LoginFlowService::setStatusCallback(std::function<void(const QString&, const QString&, int)> callback)
{
    m_statusCallback = callback;
}

void LoginFlowService::setErrorCallback(std::function<void(const QString&, const QString&)> callback)
{
    m_errorCallback = callback;
}

void LoginFlowService::setCompleteCallback(std::function<void()> callback)
{
    m_completeCallback = callback;
}

void LoginFlowService::executeLogin(
    const QString &mdAppKey,
    const QString &mdSecretKey,
    const QString &iaAppKey,
    const QString &iaSecretKey,
    const QString &loginID,
    bool downloadMasters,
    const QString &baseURL,
    const QString &source)
{
    updateStatus("init", "Starting login process...", 0);

    // Create API clients with correct base URLs
    // Market Data API needs /apimarketdata suffix
    QString mdBaseURL = baseURL + "/apimarketdata";
    // Interactive API uses base URL directly
    QString iaBaseURL = baseURL;
    
    m_mdClient = new XTSMarketDataClient(mdBaseURL, mdAppKey, mdSecretKey, source, this);
    m_iaClient = new XTSInteractiveClient(iaBaseURL, iaAppKey, iaSecretKey, source, this);

    // Connect Interactive Events to TradingDataService (if available)
    // We defer actual connection because m_tradingDataService might be null here? 
    // Usually setTradingDataService is called before executeLogin.
    if (m_tradingDataService) {
         connect(m_iaClient, &XTSInteractiveClient::orderEvent, m_tradingDataService, &TradingDataService::onOrderEvent);
         connect(m_iaClient, &XTSInteractiveClient::tradeEvent, m_tradingDataService, &TradingDataService::onTradeEvent);
         connect(m_iaClient, &XTSInteractiveClient::positionEvent, m_tradingDataService, &TradingDataService::onPositionEvent);
    }

    // Phase 1: Market Data API Login
    updateStatus("md_login", "Connecting to Market Data API...", 10);
    
    m_mdClient->login([this, downloadMasters](bool success, const QString &message) {
        if (!success) {
            notifyError("md_login", message);
            return;
        }

        updateStatus("md_login", "Market Data API connected", 30);

        // Phase 2: Interactive API Login
        updateStatus("ia_login", "Connecting to Interactive API...", 40);
        
        m_iaClient->login([this, downloadMasters](bool success, const QString &message) {
            if (!success) {
                notifyError("ia_login", message);
                return;
            }

            updateStatus("ia_login", "Interactive API connected", 60);

            // Phase 3: Check shared state and handle masters accordingly
            MasterDataState* masterState = MasterDataState::getInstance();
            QString mastersDir = RepositoryManager::getMastersDirectory();
            QDir().mkpath(mastersDir);
            
            // Check if masters were already loaded by splash screen
            if (masterState->areMastersLoaded()) {
                qDebug() << "[LoginFlow] Masters already loaded by splash screen ("
                         << masterState->getContractCount() << "contracts), skipping load";
                updateStatus("masters", QString("Using cached masters (%1 contracts)")
                            .arg(masterState->getContractCount()), 80);
                
                // Emit signal for UI (even though already loaded)
                emit mastersLoaded();
                
                // Continue directly to next phase
                continueLoginAfterMasters();
                return;
            }
            
            // Check if splash screen is still loading masters
            if (masterState->isLoading()) {
                qDebug() << "[LoginFlow] Masters are being loaded by splash screen, waiting...";
                updateStatus("masters", "Waiting for master loading...", 70);
                
                // Connect to state change signal to continue when loading completes
                connect(masterState, &MasterDataState::mastersReady, 
                        this, [this](int contractCount) {
                    qDebug() << "[LoginFlow] Masters loading completed with" << contractCount << "contracts";
                    updateStatus("masters", QString("Masters ready (%1 contracts)").arg(contractCount), 80);
                    emit mastersLoaded();
                    continueLoginAfterMasters();
                }, Qt::UniqueConnection);
                
                connect(masterState, &MasterDataState::loadingError,
                        this, [this, downloadMasters, mastersDir](const QString& error) {
                    qWarning() << "[LoginFlow] Background loading failed:" << error;
                    
                    // If user requested download, proceed with download
                    if (downloadMasters) {
                        startMasterDownload(mastersDir);
                    } else {
                        // Otherwise skip and continue
                        updateStatus("masters", "No masters available (continuing)", 75);
                        continueLoginAfterMasters();
                    }
                }, Qt::UniqueConnection);
                
                return;
            }
            
            // Masters not loaded - proceed based on downloadMasters flag
            if (downloadMasters) {
                startMasterDownload(mastersDir);
            } else {
                // User doesn't want to download, but check if we should try loading from cache
                // (in case splash screen failed but files exist)
                updateStatus("masters", "Checking for cached masters...", 70);
                qDebug() << "[LoginFlow] Attempting to load from cache...";
                
                masterState->setLoadingStarted();
                m_masterLoader->loadFromCache(mastersDir);
                
                // Note: Continuation happens in handleMasterLoadingComplete()
            }
        });
    });
}

void LoginFlowService::updateStatus(const QString &phase, const QString &message, int progress)
{
    qDebug() << "[" << phase << "]" << message << "(" << progress << "%)";
    
    emit statusUpdate(phase, message, progress);
    
    if (m_statusCallback) {
        m_statusCallback(phase, message, progress);
    }
}

void LoginFlowService::notifyError(const QString &phase, const QString &error)
{
    qWarning() << "[ERROR:" << phase << "]" << error;
    
    emit errorOccurred(phase, error);
    
    if (m_errorCallback) {
        m_errorCallback(phase, error);
    }
}

void LoginFlowService::handleMasterLoadingComplete(int contractCount)
{
    qDebug() << "[LoginFlow] Master loading completed with" << contractCount << "contracts";
    updateStatus("masters", QString("Loaded %1 contracts").arg(contractCount), 80);
    
    // Update shared state
    MasterDataState* state = MasterDataState::getInstance();
    state->setMastersLoaded(contractCount);
    
    // Emit signal for UI
    emit mastersLoaded();
    
    // Continue with the rest of the login flow
    continueLoginAfterMasters();
}

void LoginFlowService::handleMasterLoadingFailed(const QString& errorMessage)
{
    qWarning() << "[LoginFlow] Master loading failed:" << errorMessage;
    updateStatus("masters", QString("Master loading failed: %1").arg(errorMessage), 75);
    
    // Update shared state
    MasterDataState* state = MasterDataState::getInstance();
    state->setLoadingFailed(errorMessage);
    
    // Continue anyway (non-fatal error - user can download later)
    continueLoginAfterMasters();
}

void LoginFlowService::startMasterDownload(const QString& mastersDir)
{
    updateStatus("masters", "Downloading master contracts...", 70);
    qDebug() << "[LoginFlow] Starting master download...";
    
    QStringList segments = {"NSEFO", "NSECM", "BSEFO", "BSECM"};
    m_mdClient->downloadMasterContracts(segments, [this, mastersDir](bool success, const QString &csvData, const QString &error) {
        if (success) {
            qDebug() << "[LoginFlow] ✓ Master download successful (size:" << csvData.size() << "bytes)";
            qDebug() << "[LoginFlow] Loading directly from memory - SKIPPING FILE I/O";
            updateStatus("masters", "Loading from memory (no I/O)...", 75);
            
            // Update shared state
            MasterDataState* state = MasterDataState::getInstance();
            state->setLoadingStarted();
            
            // OPTIMIZED: Load directly from memory - NO FILE I/O!
            // The downloaded CSV data stays in memory and is loaded directly
            // into RepositoryManager without touching disk (except for optional caching)
            m_masterLoader->loadFromMemoryOnly(csvData, true, mastersDir);
            
            // Note: Continuation happens in handleMasterLoadingComplete()
            
        } else {
            qWarning() << "[LoginFlow] Master download failed:" << error;
            updateStatus("masters", "Master download failed (continuing anyway)", 75);
            
            // Update shared state
            MasterDataState* state = MasterDataState::getInstance();
            state->setLoadingFailed(error);
            
            // Continue with login flow even if master download failed
            // (user can download later from settings)
            continueLoginAfterMasters();
        }
    });
}

void LoginFlowService::continueLoginAfterMasters()
{
    // Phase 4: Connect WebSocket
    updateStatus("websocket", "Establishing real-time connection...", 85);
    
    m_mdClient->connectWebSocket([this](bool success, const QString &message) {
        if (!success) {
            // Non-fatal: log warning but continue
            qWarning() << "[LoginFlow] WebSocket connection failed:" << message;
        }

        updateStatus("websocket", "Real-time connection established", 90);

        // Phase 5: Fetch initial data
        updateStatus("data", "Loading account data...", 92);
        
        // Fetch positions (NetWise includes carry forward and today's positions)
        m_iaClient->getPositions("NetWise", [this](bool success, const QVector<XTS::Position> &positions, const QString &message) {
            if (success) {
                qDebug() << "[LoginFlowService] Loaded" << positions.size() << "NetWise positions";
                
                // Store in TradingDataService
                if (m_tradingDataService) {
                    m_tradingDataService->setPositions(positions);
                }
            } else {
                qWarning() << "[LoginFlowService] Failed to load positions:" << message;
                // Fallback to DayWise if NetWise fails (though usually both should work)
                m_iaClient->getPositions("DayWise", [this](bool success, const QVector<XTS::Position> &positions, const QString &message) {
                     if (success && m_tradingDataService) {
                         qDebug() << "[LoginFlowService] Loaded" << positions.size() << "DayWise positions (fallback)";
                         m_tradingDataService->setPositions(positions);
                     } else {
                         qWarning() << "[LoginFlowService] Failed to load DayWise positions during fallback:" << message;
                     }
                });
            }

            // Fetch orders
            updateStatus("data", "Loading orders...", 94);
            m_iaClient->getOrders([this](bool success, const QVector<XTS::Order> &orders, const QString &message) {
                if (success) {
                    qDebug() << "[LoginFlowService] Loaded" << orders.size() << "orders";
                    if (m_tradingDataService) m_tradingDataService->setOrders(orders);
                } else {
                    qWarning() << "[LoginFlowService] Failed to load orders:" << message;
                }

                // Connect Interactive Socket
                updateStatus("ia_socket", "Connecting Interactive Socket...", 96);
                m_iaClient->connectWebSocket([this](bool success, const QString& msg){
                     if(!success) qWarning() << "[LoginFlowService] Interactive Socket Failed:" << msg;
                     else qDebug() << "[LoginFlowService] Interactive Socket Connected";
                });

                // Fetch trades
                updateStatus("data", "Loading trades...", 98);
                m_iaClient->getTrades([this](bool success, const QVector<XTS::Trade> &trades, const QString &message) {
                    if (success) {
                        qDebug() << "[LoginFlowService] Loaded" << trades.size() << "trades";
                        
                        // Store in TradingDataService
                        if (m_tradingDataService) {
                            m_tradingDataService->setTrades(trades);
                        }
                    } else {
                        qWarning() << "[LoginFlowService] Failed to load trades: success=" << success << "message=" << message;
                    }

                    // Complete!
                    updateStatus("complete", "Login successful!", 100);
                    emit loginComplete();
                    
                    if (m_completeCallback) {
                        m_completeCallback();
                    }
                });
            });
        });
    });
}
