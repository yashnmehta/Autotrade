// Development Main - Bypasses Login for UI Testing
// ================================================
// This version skips the XTS API login and loads masters from cached files
// Use this when the XTS server is down but you want to test UI functionality
//
// To use:
// 1. Update CMakeLists.txt to use main_work.cpp instead of main.cpp
// 2. Ensure you have cached master files in build/TradingTerminal.app/Contents/MacOS/Masters/
// 3. Build and run normally

#include "app/MainWindow.h"
#include "ui/SplashScreen.h"
#include "services/TradingDataService.h"
#include "repository/RepositoryManager.h"
#include "utils/ConfigLoader.h"
#include "utils/FileLogger.h"
#include "api/XTSTypes.h"
#include "services/UdpBroadcastService.h"
#include "data/PriceStoreGateway.h"
#include "udp/UDPTypes.h"
#include "api/XTSMarketDataClient.h"
#include "api/XTSInteractiveClient.h"
#include <QApplication>
#include <QTimer>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <unordered_map>

int main(int argc, char *argv[])
{
    // Setup file logging FIRST
    setupFileLogging();
    
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);

    // Register XTS::Tick for cross-thread signals
    qRegisterMetaType<XTS::Tick>("XTS::Tick");
    
    // Register UDP types for cross-thread signals (required for Qt::QueuedConnection)
    qRegisterMetaType<UDP::MarketTick>("UDP::MarketTick");
    qRegisterMetaType<UDP::IndexTick>("UDP::IndexTick");

    // Set application metadata
    app.setApplicationName("Trading Terminal");
    app.setOrganizationName("TradingCo");
    app.setApplicationVersion("1.0.0 (Development Mode)");

    qDebug() << "========================================";
    qDebug() << "DEVELOPMENT MODE - LOGIN BYPASSED";
    qDebug() << "========================================";
    qDebug() << "Loading masters from cached files...";
    qDebug() << "XTS API login is SKIPPED";
    qDebug() << "========================================";

    // Phase 1: Show Splash Screen
    SplashScreen *splash = new SplashScreen();
    splash->showCentered();
    
    splash->setStatus("Development Mode - Loading cached data...");
    splash->setProgress(10);

    // Load masters from cache (non-blocking)
    QTimer::singleShot(500, [splash]() {
        splash->setProgress(30);
        splash->setStatus("Loading master contracts...");
        
        QString mastersDir = RepositoryManager::getMastersDirectory();
        qDebug() << "[DevMode] Masters directory:" << mastersDir;
        
        // Ensure directory exists
        QDir().mkpath(mastersDir);
        
        // Check if master files exist
        QStringList requiredFiles = {
            "contract_nsefo_latest.txt",
            "contract_nsecm_latest.txt",
            "master_contracts_latest.txt"
        };
        
        bool hasMasters = false;
        for (const QString &file : requiredFiles) {
            QString filePath = mastersDir + "/" + file;
            if (QFile::exists(filePath)) {
                qDebug() << "[DevMode] Found master file:" << file;
                hasMasters = true;
                break;
            }
        }
        
        if (!hasMasters) {
            qWarning() << "[DevMode] ⚠️ No master files found in:" << mastersDir;
            qWarning() << "[DevMode] Expected files:" << requiredFiles;
            qWarning() << "[DevMode] UI will work but symbol search will be limited";
            
            // Note: QMessageBox disabled to prevent potential memory issues in dev mode
            // QTimer::singleShot(100, [splash]() {
            //     QMessageBox::warning(
            //         splash,
            //         "No Master Files",
            //         QString("No cached master files found in:\n%1\n\n"
            //                 "The UI will work, but symbol search will be limited.\n\n"
            //                 "To get master files:\n"
            //                 "1. Run the app with XTS server online once\n"
            //                 "2. Or manually copy master files to the Masters directory")
            //             .arg(RepositoryManager::getMastersDirectory())
            //     );
            // });
        }
        
        // Load masters into RepositoryManager
        splash->setProgress(50);
        splash->setStatus("Parsing master contracts...");
        
        RepositoryManager* repo = RepositoryManager::getInstance();
        if (repo->loadAll(mastersDir)) {
            qDebug() << "[DevMode] ✅ RepositoryManager loaded successfully";
            qDebug() << "[DevMode] Total contracts loaded:" << repo->getTotalContractCount();
            splash->setProgress(70);
        } else {
            qWarning() << "[DevMode] ⚠️ Failed to load RepositoryManager (continuing anyway)";
            splash->setProgress(70);
        }
        
        // Simulate additional loading steps
        QTimer::singleShot(300, [splash]() {
            splash->setProgress(85);
            splash->setStatus("Preparing UI...");
            
            QTimer::singleShot(300, [splash]() {
                splash->setProgress(100);
                splash->setStatus("Ready!");
                
                // Close splash and show main window
                QTimer::singleShot(500, [splash]() {
                    splash->close();
                    splash->deleteLater();  // Use deleteLater() for Qt widgets (safe deletion)
                    
                    // Create and show main window directly (no login)
                    MainWindow *mainWindow = new MainWindow(nullptr);
                    
                    // Ensure window is deleted when closed (prevents memory leaks)
                    mainWindow->setAttribute(Qt::WA_DeleteOnClose);
                    
                    // 1. Create dummy XTS clients to prevent crashes
                    // Load config first
                    ConfigLoader *config = new ConfigLoader();
                     // Default config paths (similar to main.cpp logic)
                    QString appDir = QCoreApplication::applicationDirPath();
                    QStringList candidates;
                    // Fix: Add correct relative path to project root configs
                    candidates << QDir(appDir).filePath("../configs/config.ini");        // MinGW build dir
                    candidates << QDir(appDir).filePath("../../configs/config.ini");     // MSVC Debug/Release dir
                    candidates << QDir(appDir).filePath("../../../../configs/config.ini"); // macOS bundle
                    candidates << QDir(appDir).filePath("config.ini");                   // local

                    
                    QString configPath;
                    for (const QString &path : candidates) {
                        if (QFile::exists(path)) {
                            configPath = path;
                            break;
                        }
                    }
                    
                    if (!configPath.isEmpty()) {
                        config->load(configPath);
                        qDebug() << "[DevMode] Loaded config from:" << configPath;
                    } else {
                        qWarning() << "[DevMode] Could not find config.ini";
                    }
                    
                    // IMPORTANT: Set config loader on MainWindow so it can use it later
                    mainWindow->setConfigLoader(config);

                    // Create dummy clients
                    QString baseURL = config->getXTSUrl();
                    if (baseURL.isEmpty()) baseURL = "http://localhost:3000";

                    XTSMarketDataClient *mdClient = new XTSMarketDataClient(baseURL + "/apimarketdata", "DUMMY_KEY", "DUMMY_SECRET");
                    XTSInteractiveClient *iaClient = new XTSInteractiveClient(baseURL, "DUMMY_KEY", "DUMMY_SECRET");
                    
                    mainWindow->setXTSClients(mdClient, iaClient);
                    
                    // Create trading data service
                    TradingDataService *tradingDataService = new TradingDataService(mainWindow);
                    mainWindow->setTradingDataService(tradingDataService);
                    
                    // 2. Start UDP Broadcast Service
                    qDebug() << "[DevMode] Starting UDP Broadcast Service...";
                    UdpBroadcastService::Config udpConfig;
                    udpConfig.nseFoIp = config->getNSEFOMulticastIP().toStdString();
                    udpConfig.nseFoPort = config->getNSEFOPort();
                    udpConfig.nseCmIp = config->getNSECMMulticastIP().toStdString();
                    udpConfig.nseCmPort = config->getNSECMPort();
                    udpConfig.bseFoIp = config->getBSEFOMulticastIP().toStdString();
                    udpConfig.bseFoPort = config->getBSEFOPort();
                    udpConfig.bseCmIp = config->getBSECMMulticastIP().toStdString();
                    udpConfig.bseCmPort = config->getBSECMPort();
                    
                    // Auto-enable based on config presence
                    udpConfig.enableNSEFO = !udpConfig.nseFoIp.empty();
                    udpConfig.enableNSECM = !udpConfig.nseCmIp.empty();
                    udpConfig.enableBSEFO = !udpConfig.bseFoIp.empty();
                    udpConfig.enableBSECM = !udpConfig.bseCmIp.empty();
                    
                    UdpBroadcastService::instance().start(udpConfig);
                    qDebug() << "[DevMode] UDP Service started. Active:" << UdpBroadcastService::instance().isActive();

                    // 3. Initialize Distributed Price Stores
                    qDebug() << "[DevMode] Initializing Distributed Price Stores...";
                    std::vector<uint32_t> nseCmTokens, nseFoTokens, bseCmTokens, bseFoTokens;
                    
                    if (auto* r = repo->getNSECMRepository()) {
                        for (const auto& c : r->getAllContracts()) nseCmTokens.push_back(static_cast<uint32_t>(c.token));
                    }
                    if (auto* r = repo->getNSEFORepository()) {
                        for (const auto& c : r->getAllContracts()) nseFoTokens.push_back(static_cast<uint32_t>(c.token));
                    }
                    if (auto* r = repo->getBSECMRepository()) {
                        for (const auto& c : r->getAllContracts()) bseCmTokens.push_back(static_cast<uint32_t>(c.token));
                    }
                    if (auto* r = repo->getBSEFORepository()) {
                        for (const auto& c : r->getAllContracts()) bseFoTokens.push_back(static_cast<uint32_t>(c.token));
                    }
                    
                    MarketData::PriceStoreGateway::instance().initialize(nseFoTokens, nseCmTokens, bseFoTokens, bseCmTokens);
                    qDebug() << "[DevMode] ✅ Distributed Price Stores initialized successfully";

                    // Show main window
                    mainWindow->show();
                    mainWindow->raise();
                    mainWindow->activateWindow();
                    
                    qDebug() << "[DevMode] ✅ Main window shown with UDP Broadcast enabled";
                    qDebug() << "[DevMode] You can now test UI functionality";
                    qDebug() << "[DevMode] Dummy XTS clients initialized to prevent crashes";
                });
            });
        });
    });

    int exitCode = app.exec();
    
    // Cleanup file logging
    cleanupFileLogging();
    
    return exitCode;
}
