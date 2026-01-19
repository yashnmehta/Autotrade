#include "ui/SplashScreen.h"
#include "ui_SplashScreen.h"
#include "repository/RepositoryManager.h"
#include "services/MasterLoaderWorker.h"
#include "services/MasterDataState.h"
#include "data/PriceStoreGateway.h"
#include "utils/PreferencesManager.h"
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include "utils/MemoryProfiler.h"

SplashScreen::SplashScreen(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , ui(new Ui::SplashScreen)
    , m_masterLoader(nullptr)
    , m_masterLoadingComplete(false)
    , m_minimumTimeElapsed(false)
{
    ui->setupUi(this);
    
    // Make widget semi-transparent background
    setAttribute(Qt::WA_TranslucentBackground, false);
    
    // Set initial values
    ui->progressBar->setValue(0);
    ui->labelStatus->setText("Initializing...");
    
    // Load preferences first (including PriceCache mode)
    loadPreferences();
    
    // Create master loader worker for async loading
    m_masterLoader = new MasterLoaderWorker(this);
    
    // Connect signals from worker
    connect(m_masterLoader, &MasterLoaderWorker::loadingProgress,
            this, &SplashScreen::onMasterLoadingProgress);
    connect(m_masterLoader, &MasterLoaderWorker::loadingComplete,
            this, &SplashScreen::onMasterLoadingComplete);
    connect(m_masterLoader, &MasterLoaderWorker::loadingFailed,
            this, &SplashScreen::onMasterLoadingFailed);
    
    // Setup minimum display time (1.5 seconds)
    QTimer::singleShot(1500, this, [this]() {
        qDebug() << "[SplashScreen] Minimum display time elapsed";
        m_minimumTimeElapsed = true;
        checkIfReadyToClose();
    });
}

SplashScreen::~SplashScreen()
{
    // Cancel any ongoing loading operation
    if (m_masterLoader && m_masterLoader->isRunning()) {
        m_masterLoader->cancel();
        m_masterLoader->wait(2000);
    }
    
    delete ui;
}

void SplashScreen::setProgress(int value)
{
    ui->progressBar->setValue(value);
    // Qt will update UI automatically on next event loop iteration
}

void SplashScreen::setStatus(const QString &status)
{
    ui->labelStatus->setText(status);
    // Qt will update UI automatically on next event loop iteration
}

void SplashScreen::showCentered()
{
    show();
    
    // Center on screen
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
    
    // Qt will update UI automatically
}

void SplashScreen::preloadMasters()
{
    // Check if masters are already loaded or loading
    MasterDataState* state = MasterDataState::getInstance();
    
    if (state->areMastersLoaded()) {
        qDebug() << "[SplashScreen] Masters already loaded, skipping preload";
        setStatus("Master contracts ready");
        setProgress(100);
        m_masterLoadingComplete = true;
        checkIfReadyToClose();
        return;
    }
    
    if (state->isLoading()) {
        qDebug() << "[SplashScreen] Masters already loading, skipping duplicate load";
        setStatus("Loading master contracts...");
        setProgress(30);
        return;
    }
    
    // Check if cached master files exist
    QString mastersDir = RepositoryManager::getMastersDirectory();
    QString masterFile = mastersDir + "/master_contracts_latest.txt";
    
    if (!QFile::exists(masterFile)) {
        qDebug() << "[SplashScreen] No cached masters found at:" << masterFile;
        setStatus("Ready");
        setProgress(100);
        state->setLoadState(MasterDataState::LoadState::LoadFailed);
        m_masterLoadingComplete = true;
        checkIfReadyToClose();
        return;
    }
    
    // Start async loading from cache
    setStatus("Loading master contracts...");
    setProgress(30);
    
    qDebug() << "[SplashScreen] Starting async master loading from cache...";
    state->setLoadingStarted();
    
    // Load asynchronously using worker thread
    m_masterLoader->loadFromCache(mastersDir);
    
    // Fallback timeout: If loading takes more than 5 seconds, close anyway
    QTimer::singleShot(5000, this, [this]() {
        if (!m_masterLoadingComplete) {
            qWarning() << "[SplashScreen] Master loading timeout - closing splash anyway";
            m_masterLoadingComplete = true;
            setStatus("Ready");
            setProgress(100);
            checkIfReadyToClose();
        }
    });
}

void SplashScreen::onMasterLoadingProgress(int percentage, const QString& message)
{
    // Update UI with loading progress
    setStatus(message);
    // Map 0-100% loading to 30-100% splash progress
    int splashProgress = 30 + (percentage * 70 / 100);
    setProgress(splashProgress);
    
    qDebug() << "[SplashScreen] Loading progress:" << percentage << "%" << message;
}

void SplashScreen::onMasterLoadingComplete(int contractCount)
{
    qDebug() << "[SplashScreen] ✓ Masters preloaded successfully with" << contractCount << "contracts";
    
    // Update shared state
    MasterDataState* state = MasterDataState::getInstance();
    state->setMastersLoaded(contractCount);
    
    // Update UI
    setStatus(QString("Loaded %1 contracts").arg(contractCount));
    setProgress(100);
    
    auto stats = RepositoryManager::getInstance()->getSegmentStats();
    qDebug() << "[SplashScreen]   NSE F&O:" << stats.nsefo << "contracts";
    qDebug() << "[SplashScreen]   NSE CM:" << stats.nsecm << "contracts";
    qDebug() << "[SplashScreen]   BSE F&O:" << stats.bsefo << "contracts";
    qDebug() << "[SplashScreen]   BSE CM:" << stats.bsecm << "contracts";
    
    // ===================================================================
    // Initialize new PriceCache (zero-copy) if enabled
    // ===================================================================
    PreferencesManager& prefs = PreferencesManager::instance();
    bool useLegacy = true; // FORCED FALSE FOR MEASUREMENT
    
    // Log baseline RAM usage
    MemoryProfiler::logSnapshot("Baseline (Masters Loaded)");

    if (!useLegacy) {
        qDebug() << "[SplashScreen] ========================================";
        qDebug() << "[SplashScreen] Initializing Distributed Price Stores...";
        qDebug() << "[SplashScreen] ========================================";
        
        setStatus(QString("Initializing background price stores..."));
        setProgress(90);
        
        // Build token lists from repository
        RepositoryManager* repo = RepositoryManager::getInstance();
        
        // NSE CM token list
        std::vector<uint32_t> nseCmTokens;
        const auto& nseCmContracts = repo->getContractsBySegment("NSE", "CM");
        for (const auto& contract : nseCmContracts) {
            nseCmTokens.push_back(contract.exchangeInstrumentID);
        }
        
        // NSE FO token list
        std::vector<uint32_t> nseFoTokens;
        const auto& nseFoContracts = repo->getContractsBySegment("NSE", "FO");
        for (const auto& contract : nseFoContracts) {
            nseFoTokens.push_back(contract.exchangeInstrumentID);
        }
        
        // BSE CM token list
        std::vector<uint32_t> bseCmTokens;
        const auto& bseCmContracts = repo->getContractsBySegment("BSE", "CM");
        for (const auto& contract : bseCmContracts) {
            bseCmTokens.push_back(contract.exchangeInstrumentID);
        }
        
        // BSE FO token list
        std::vector<uint32_t> bseFoTokens;
        const auto& bseFoContracts = repo->getContractsBySegment("BSE", "FO");
        for (const auto& contract : bseFoContracts) {
            bseFoTokens.push_back(contract.exchangeInstrumentID);
        }
        
        // Initialize via Gateway
        MarketData::PriceStoreGateway::instance().initialize(
            nseFoTokens,
            nseCmTokens,
            bseFoTokens,
            bseCmTokens
        );
        
        qDebug() << "[SplashScreen] \u2713 Distributed Stores initialized successfully";
        qDebug() << "[SplashScreen] ========================================";
        
        // Log RAM after store allocation
        MemoryProfiler::logSnapshot("Distributed Stores Active");
        
        setStatus("Market data stores ready!");
    } else {
        MemoryProfiler::logSnapshot("Legacy Mode Active");
    }

    
    // Mark loading complete and check if ready to close
    m_masterLoadingComplete = true;
    checkIfReadyToClose();
}

void SplashScreen::onMasterLoadingFailed(const QString& errorMessage)
{
    qWarning() << "[SplashScreen] ⚠ Master loading failed:" << errorMessage;
    
    // Update shared state
    MasterDataState* state = MasterDataState::getInstance();
    state->setLoadingFailed(errorMessage);
    
    // Update UI
    setStatus("Ready");
    setProgress(100);
    
    // Mark loading complete (even though failed) and check if ready to close
    m_masterLoadingComplete = true;
    checkIfReadyToClose();
}

void SplashScreen::checkIfReadyToClose()
{
    // Only close when BOTH conditions are met:
    // 1. Master loading complete (or not needed/failed)
    // 2. Minimum display time elapsed (prevents flashing)
    
    if (m_masterLoadingComplete && m_minimumTimeElapsed) {
        qDebug() << "[SplashScreen] All conditions met - ready to close";
        setStatus("Ready!");
        
        // Small delay for visual polish (let user see "Ready!" message)
        QTimer::singleShot(300, this, [this]() {
            emit readyToClose();
        });
    } else {
        qDebug() << "[SplashScreen] Waiting for conditions - master loading:" 
                 << m_masterLoadingComplete << "minimum time:" << m_minimumTimeElapsed;
    }
}

void SplashScreen::loadPreferences()
{
    setStatus("Loading preferences...");
    setProgress(10);
    
    // Access PreferencesManager singleton
    PreferencesManager& prefs = PreferencesManager::instance();
    
    // Load PriceCache mode flag
    bool useLegacy = prefs.getUseLegacyPriceCache();
    
    qDebug() << "[SplashScreen] ========================================";
    qDebug() << "[SplashScreen] Preferences Loaded";
    qDebug() << "[SplashScreen] ========================================";
    qDebug() << "[SplashScreen] PriceCache Mode:" << (useLegacy ? "LEGACY (current)" : "NEW (zero-copy)");
    
    if (!useLegacy) {
        qDebug() << "[SplashScreen] ⚡ NEW ARCHITECTURE ENABLED ⚡";
        qDebug() << "[SplashScreen]   - Zero-copy memory arrays";
        qDebug() << "[SplashScreen]   - Direct UDP writes";
        qDebug() << "[SplashScreen]   - Pointer-based subscriptions";
        qDebug() << "[SplashScreen]   - Hybrid notification system";
    } else {
        qDebug() << "[SplashScreen] 📦 Using current implementation";
    }
    qDebug() << "[SplashScreen] ========================================";
    
    setStatus("Preferences loaded");
    setProgress(20);
}

