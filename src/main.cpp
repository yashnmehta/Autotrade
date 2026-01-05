#include "app/MainWindow.h"
#include "ui/SplashScreen.h"
#include "ui/LoginWindow.h"
#include "services/LoginFlowService.h"
#include "services/TradingDataService.h"
#include "utils/ConfigLoader.h"
#include "utils/FileLogger.h"  // File logging
#include "api/XTSTypes.h"  // For XTS::Tick
#include <QApplication>
#include <QThread>
#include <QTimer>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>

int main(int argc, char *argv[])
{
    // Setup file logging FIRST (before any qDebug calls)
    setupFileLogging();
    
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);

    
    // Register XTS types for cross-thread signals (CRITICAL for socket events!)
    qRegisterMetaType<XTS::Tick>("XTS::Tick");
    qRegisterMetaType<XTS::Order>("XTS::Order");
    qRegisterMetaType<XTS::Trade>("XTS::Trade");
    qRegisterMetaType<XTS::Position>("XTS::Position");
    qRegisterMetaType<QVector<XTS::Order>>("QVector<XTS::Order>");
    qRegisterMetaType<QVector<XTS::Trade>>("QVector<XTS::Trade>");
    qRegisterMetaType<QVector<XTS::Position>>("QVector<XTS::Position>");

    // Set application metadata
    app.setApplicationName("Trading Terminal");
    app.setOrganizationName("TradingCo");
    app.setApplicationVersion("1.0.0");

    // Phase 1: Show Splash Screen
    SplashScreen *splash = new SplashScreen();
    splash->showCentered();
    
    // OPTIMIZATION: Load config DURING splash screen (not after)
    splash->setStatus("Loading configuration...");
    splash->setProgress(5);
    QApplication::processEvents();
    
    ConfigLoader *config = new ConfigLoader();
    QString appDir = QCoreApplication::applicationDirPath();
    
    QStringList candidates;
    // 1) Workspace config (highest priority for development)
    candidates << QDir(appDir).filePath("../../../../configs/config.ini");
    
    // 2) Standard application config location (per-platform)
    QString appConfigDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!appConfigDir.isEmpty()) {
        candidates << QDir(appConfigDir).filePath("config.ini");
    }

    // 3) User-specific config in ~/.config/<AppName>/config.ini
    QString homeConfig = QDir::home().filePath(QString(".config/%1/config.ini").arg(QCoreApplication::applicationName().replace(' ', "")));
    candidates << homeConfig;

    // 4) Common developer/project locations relative to the binary
    candidates << QDir(appDir).filePath("../../../../../configs/config.ini");
    candidates << QDir(appDir).filePath("../configs/config.ini");
    candidates << QDir(appDir).filePath("configs/config.ini");

    // 5) macOS specific: inside app bundle Resources (if bundled)
    candidates << QDir(appDir).filePath("../Resources/config.ini");

    QString foundPath;
    for (const QString &cand : candidates) {
        QString abs = QFileInfo(cand).absoluteFilePath();
        qDebug() << "Checking config candidate:" << abs;
        if (QFile::exists(abs)) {
            qDebug() << "Found config at:" << abs;
            if (config->load(abs)) {
                foundPath = abs;
                break;
            } else {
                qWarning() << "Found config but failed to load:" << abs;
            }
        }
    }

    if (!foundPath.isEmpty()) {
        qDebug() << "✓ Config loaded from:" << foundPath;
        splash->setStatus("Configuration loaded");
    } else {
        qDebug() << "Config file not found, using defaults";
        splash->setStatus("Using default configuration");
    }
    splash->setProgress(10);
    QApplication::processEvents();
    
    // Preload masters during splash (event-driven, non-blocking)
    splash->setStatus("Initializing...");
    splash->preloadMasters();

    // EVENT-DRIVEN APPROACH: Wait for splash to signal ready instead of using timer
    // The splash screen will emit readyToClose() when:
    // 1. Master loading complete (or not needed/failed/timeout)
    // 2. Minimum display time elapsed (1.5 seconds)
    QObject::connect(splash, &SplashScreen::readyToClose, [splash, config, foundPath]() {
        qDebug() << "[Main] Splash screen ready to close, showing login window...";
        
        splash->close();
        delete splash;
        
        // Phase 2: Show Login Window
                    LoginWindow *loginWindow = new LoginWindow();
                    
                    // Config already loaded during splash screen - just populate UI
                    if (!foundPath.isEmpty()) {
                        qDebug() << "Populating login window with credentials from config";
                        loginWindow->setMarketDataAppKey(config->getMarketDataAppKey());
                        loginWindow->setMarketDataSecretKey(config->getMarketDataSecretKey());
                        loginWindow->setInteractiveAppKey(config->getInteractiveAppKey());
                        loginWindow->setInteractiveSecretKey(config->getInteractiveSecretKey());
                        loginWindow->setLoginID(config->getUserID());
                    }
                    
                    // Create login flow service
                    LoginFlowService *loginService = new LoginFlowService();
                    
                    // Create trading data service
                    TradingDataService *tradingDataService = new TradingDataService();
                    
                    // Wire trading data service to login service
                    loginService->setTradingDataService(tradingDataService);
                    
                    // Setup status callbacks
                    loginService->setStatusCallback([loginWindow](const QString &phase, const QString &message, int progress) {
                        if (phase == "md_login") {
                            if (progress >= 30) {
                                loginWindow->setMDStatus("✓ Connected", false);
                            } else {
                                loginWindow->setMDStatus("Connecting...", false);
                            }
                        } else if (phase == "ia_login") {
                            if (progress >= 60) {
                                loginWindow->setIAStatus("✓ Connected", false);
                            } else {
                                loginWindow->setIAStatus("Connecting...", false);
                            }
                        }
                    });
                    
                    loginService->setErrorCallback([loginWindow](const QString &phase, const QString &error) {
                        if (phase == "md_login") {
                            loginWindow->setMDStatus(QString("✗ Error: %1").arg(error), true);
                            loginWindow->enableLoginButton();
                        } else if (phase == "ia_login") {
                            loginWindow->setIAStatus(QString("✗ Error: %1").arg(error), true);
                            loginWindow->enableLoginButton();
                        }
                    });
                    
                    // Create main window (but don't show yet)
                    // Use new with QApplication as parent to ensure proper lifecycle
                    MainWindow *mainWindow = new MainWindow(nullptr);
                    mainWindow->hide(); // Explicitly hide initially
                    
                    // Connect mastersLoaded signal to refresh ScripBar
                    QObject::connect(loginService, &LoginFlowService::mastersLoaded, mainWindow, [mainWindow]() {
                        qDebug() << "[Main] Masters loaded, refreshing ScripBar symbols...";
                        mainWindow->refreshScripBar();
                    });
                    
                    loginService->setCompleteCallback([loginWindow, mainWindow, loginService, tradingDataService, config]() {
                        qDebug() << "✅ Login complete! Showing main window...";
                        
                        // Pass XTS clients to main window
                        mainWindow->setXTSClients(
                            loginService->getMarketDataClient(),
                            loginService->getInteractiveClient()
                        );
                        
                        // Pass trading data service to main window
                        mainWindow->setTradingDataService(tradingDataService);
                        
                        // Pass config loader to main window
                        mainWindow->setConfigLoader(config);
                        
                        // Show continue button
                        loginWindow->showContinueButton();
                        loginWindow->enableLoginButton();
                    });
                    
                    // Setup login button callback
                    loginWindow->setOnLoginClicked([loginWindow, loginService, config]() {
                        qDebug() << "Login button clicked";
                        
                        // Get credentials
                        QString mdAppKey = loginWindow->getMarketDataAppKey();
                        QString mdSecretKey = loginWindow->getMarketDataSecretKey();
                        QString iaAppKey = loginWindow->getInteractiveAppKey();
                        QString iaSecretKey = loginWindow->getInteractiveSecretKey();
                        QString loginID = loginWindow->getLoginID();
                        bool downloadMasters = loginWindow->shouldDownloadMasters();
                        
                        // Disable button during login
                        loginWindow->disableLoginButton();
                        loginWindow->setMDStatus("Connecting...", false);
                        loginWindow->setIAStatus("Connecting...", false);
                        
                        // Get base URL from config or use default
                        QString baseURL = config->getXTSUrl();
                        if (baseURL.isEmpty()) {
                            baseURL = "https://ttblaze.iifl.com";
                        }
                        
                        // Market Data API needs /apimarketdata suffix
                        QString mdBaseURL = baseURL + "/apimarketdata";
                        // Interactive API uses base URL directly
                        QString iaBaseURL = baseURL;
                        
                        // Execute login flow
                        loginService->executeLogin(
                            mdAppKey, mdSecretKey,
                            iaAppKey, iaSecretKey,
                            loginID, downloadMasters,
                            baseURL,
                            config->getSource()
                        );
                    });
                    
                    // Setup continue button callback
                    loginWindow->setOnContinueClicked([loginWindow, mainWindow]() {
                        qDebug() << "Continue button clicked - showing main window";
                        
                        // First show the main window
                        if (mainWindow != nullptr) {
                            qDebug() << "MainWindow pointer is valid, showing window...";
                            mainWindow->show();
                            mainWindow->raise();
                            mainWindow->activateWindow();
                        } else {
                            qCritical() << "ERROR: MainWindow pointer is NULL!";
                        }
                        
                        // Then close and delete login window
                        loginWindow->accept();
                        loginWindow->deleteLater(); // Use deleteLater instead of delete for safer cleanup
                    });
                    
                    // Show login window centered
                    loginWindow->showCentered();
                    
                    // Show login window as modal dialog
                    int result = loginWindow->exec();
                    
                    if (result == QDialog::Rejected) {
                        // User clicked Exit or closed window
                        qDebug() << "Login cancelled by user";
                        loginWindow->deleteLater();
                        if (mainWindow != nullptr) {
                            mainWindow->close();
                            mainWindow->deleteLater();
                        }
                        QApplication::quit();
                    }
    });

    int exitCode = app.exec();
    
    // Cleanup file logging
    cleanupFileLogging();
    
    return exitCode;
}
