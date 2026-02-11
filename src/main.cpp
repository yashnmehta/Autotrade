#include "api/XTSTypes.h" // For XTS::Tick
#include "app/MainWindow.h"
#include "core/WindowCacheManager.h"
#include "data/CandleData.h" // For ChartData::Candle
#include "services/CandleAggregator.h"
#include "services/GreeksCalculationService.h" // For GreeksResult
#include "services/LoginFlowService.h"
#include "services/TradingDataService.h"
#include "udp/UDPTypes.h" // For UDP::MarketTick, UDP::IndexTick
#include "ui/LoginWindow.h"
#include "ui/SplashScreen.h"
#include "utils/ConfigLoader.h"
#include "utils/FileLogger.h"         // File logging
#include "utils/PreferencesManager.h" // Preferences
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <cstdio>

int main(int argc, char *argv[]) {
  fprintf(stderr, "[Main] Starting application...\n");
  fflush(stderr);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

  fprintf(stderr, "[Main] Creating QApplication...\n");
  fflush(stderr);
  QApplication app(argc, argv);

  fprintf(stderr, "[Main] Setting up File Logging...\n");
  fflush(stderr);
  // Setup file logging after QApplication is created
  setupFileLogging();

  fprintf(stderr, "[Main] Registering MetaTypes...\n");
  fflush(stderr);
  // Register XTS types for cross-thread signals (CRITICAL for socket events!)
  qRegisterMetaType<XTS::Tick>("XTS::Tick");
  qRegisterMetaType<XTS::Order>("XTS::Order");
  qRegisterMetaType<XTS::Trade>("XTS::Trade");
  qRegisterMetaType<XTS::Position>("XTS::Position");
  qRegisterMetaType<QVector<XTS::Order>>("QVector<XTS::Order>");
  qRegisterMetaType<QVector<XTS::Trade>>("QVector<XTS::Trade>");
  qRegisterMetaType<QVector<XTS::Position>>("QVector<XTS::Position>");

  // Register UDP types for broadcast signals
  qRegisterMetaType<UDP::MarketTick>("UDP::MarketTick");
  qRegisterMetaType<UDP::IndexTick>("UDP::IndexTick");
  qRegisterMetaType<UDP::CircuitLimitTick>("UDP::CircuitLimitTick");

  // Register Greeks types for OptionChain updates (CRITICAL for Greeks
  // display!)
  qRegisterMetaType<uint32_t>("uint32_t");
  qRegisterMetaType<GreeksResult>("GreeksResult");
  
  // Register Chart/Candle types for charting system
  qRegisterMetaType<ChartData::Candle>("ChartData::Candle");
  qRegisterMetaType<ChartData::Timeframe>("ChartData::Timeframe");

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

  fprintf(stderr, "[Main] Loading configuration...\n");
  fflush(stderr);

  ConfigLoader *config = new ConfigLoader();
  QString appDir = QCoreApplication::applicationDirPath();

  fprintf(stderr, "[Main] Application directory: %s\n", qPrintable(appDir));
  fflush(stderr);

  QStringList candidates;
  // 1) MSVC build config (build_msvc/Debug or build_msvc/Release)
  candidates << QDir(appDir).filePath("../../configs/config.ini");
  
  // 2) MinGW build config (build/)
  candidates << QDir(appDir).filePath("../configs/config.ini");
  
  // 3) Same directory as executable
  candidates << QDir(appDir).filePath("configs/config.ini");

  // 4) Standard application config location (per-platform)
  QString appConfigDir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  if (!appConfigDir.isEmpty()) {
    candidates << QDir(appConfigDir).filePath("config.ini");
  }

  // 5) User-specific config in ~/.config/<AppName>/config.ini
  QString homeConfig = QDir::home().filePath(
      QString(".config/%1/config.ini")
          .arg(QCoreApplication::applicationName().replace(' ', "")));
  candidates << homeConfig;

  // 6) macOS bundle or other deep nesting
  candidates << QDir(appDir).filePath("../../../../configs/config.ini");
  candidates << QDir(appDir).filePath("../../../../../configs/config.ini");

  // 7) macOS specific: inside app bundle Resources (if bundled)
  candidates << QDir(appDir).filePath("../Resources/config.ini");

  QString foundPath;
  qWarning() << "[CONFIG] Starting config search...";
  qWarning() << "[CONFIG] Application directory:" << appDir;
  
  fprintf(stderr, "[Main] Searching for config.ini...\n");
  fflush(stderr);
  
  for (const QString &cand : candidates) {
    QString abs = QFileInfo(cand).absoluteFilePath();
    fprintf(stderr, "[Main] Checking: %s\n", qPrintable(abs));
    fflush(stderr);
    qWarning() << "[CONFIG] Checking:" << abs;
    if (QFile::exists(abs)) {
      fprintf(stderr, "[Main] ✓ FOUND: %s\n", qPrintable(abs));
      fflush(stderr);
      qWarning() << "[CONFIG] ✓ Found config at:" << abs;
      if (config->load(abs)) {
        foundPath = abs;
        fprintf(stderr, "[Main] ✓ Successfully loaded config\n");
        fflush(stderr);
        qWarning() << "[CONFIG] ✓ Successfully loaded config from:" << abs;
        break;
      } else {
        fprintf(stderr, "[Main] ✗ Failed to load config\n");
        fflush(stderr);
        qWarning() << "[CONFIG] ✗ Found config but failed to load:" << abs;
      }
    }
  }

  if (!foundPath.isEmpty()) {
    qWarning() << "[CONFIG] ✅ Configuration loaded successfully from:" << foundPath;
    qWarning() << "[CONFIG] Default client:" << config->getDefaultClient();
    qWarning() << "[CONFIG] User ID:" << config->getUserID();
    qWarning() << "[CONFIG] XTS URL:" << config->getXTSUrl();
    splash->setStatus("Configuration loaded");
  } else {
    qWarning() << "[CONFIG] ⚠ Config file not found, using defaults";
    splash->setStatus("Using default configuration");
  }
  splash->setProgress(10);

  // Preload masters during splash (event-driven, non-blocking)
  splash->setStatus("Initializing...");
  splash->preloadMasters();

  // EVENT-DRIVEN APPROACH: Wait for splash to signal ready instead of using
  // timer The splash screen will emit readyToClose() when:
  // 1. Master loading complete (or not needed/failed/timeout)
  // 2. Minimum display time elapsed (1.5 seconds)
  QObject::connect(
      splash, &SplashScreen::readyToClose, [splash, config, foundPath]() {
        qDebug()
            << "[Main] Splash screen ready to close, showing login window...";

        splash->close();
        splash->deleteLater();

        // Phase 2: Show Login Window
        LoginWindow *loginWindow = new LoginWindow();

        // Config already loaded during splash screen - just populate UI
        if (!foundPath.isEmpty()) {
          qDebug() << "Populating login window with credentials from config";
          loginWindow->setMarketDataAppKey(config->getMarketDataAppKey());
          loginWindow->setMarketDataSecretKey(config->getMarketDataSecretKey());
          loginWindow->setInteractiveAppKey(config->getInteractiveAppKey());
          loginWindow->setInteractiveSecretKey(
              config->getInteractiveSecretKey());
          loginWindow->setLoginID(config->getUserID());
        }

        // Create login flow service
        LoginFlowService *loginService = new LoginFlowService();

        // Create trading data service
        TradingDataService *tradingDataService = new TradingDataService();

        // Wire trading data service to login service
        loginService->setTradingDataService(tradingDataService);

        // Setup status callbacks
        loginService->setStatusCallback([loginWindow](const QString &phase,
                                                      const QString &message,
                                                      int progress) {
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

        loginService->setErrorCallback([loginWindow](const QString &phase,
                                                     const QString &error) {
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
        QObject::connect(
            loginService, &LoginFlowService::mastersLoaded, mainWindow,
            [mainWindow]() {
              qDebug()
                  << "[Main] Masters loaded, refreshing ScripBar symbols...";
              mainWindow->refreshScripBar();
            });

        loginService->setCompleteCallback([loginWindow, mainWindow,
                                           loginService, tradingDataService,
                                           config]() {
          qDebug() << "✅ Login complete! Showing main window...";

          // Initialize CandleAggregator for real-time chart updates
          CandleAggregator::instance().initialize(true);
          qDebug() << "[Main] CandleAggregator initialized";

          // Pass XTS clients to main window
          mainWindow->setXTSClients(loginService->getMarketDataClient(),
                                    loginService->getInteractiveClient());

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
          loginService->executeLogin(mdAppKey, mdSecretKey, iaAppKey,
                                     iaSecretKey, loginID, downloadMasters,
                                     baseURL, config->getSource());
        });

        // Setup continue button callback
        loginWindow->setOnContinueClicked([loginWindow, mainWindow]() {
          qDebug() << "Continue button clicked - showing main window";

          if (mainWindow != nullptr) {
            // ✅ CRITICAL FIX: Show main window FIRST to prevent Qt auto-quit
            // Qt quits if no windows visible after last window closes
            qDebug() << "Showing main window immediately...";
            mainWindow->show();
            mainWindow->raise();
            mainWindow->activateWindow();

            // ✅ Close dialog AFTER main window is visible
            loginWindow->accept();

            // ✅ Defer workspace loading to prevent blocking during dialog
            // close This runs after modal event loop has fully exited
            QTimer::singleShot(10, mainWindow, [mainWindow]() {
              qDebug() << "Loading workspace after dialog fully closed...";

              // Load Default Workspace
              QString defaultWorkspace =
                  PreferencesManager::instance().getDefaultWorkspace();
              if (!defaultWorkspace.isEmpty() &&
                  defaultWorkspace != "Default") {
                if (mainWindow->loadWorkspaceByName(defaultWorkspace)) {
                  qDebug() << "Loaded default workspace:" << defaultWorkspace;
                } else {
                  qWarning() << "Default workspace not found or failed to load:"
                             << defaultWorkspace;
                }
              }

              // Create IndicesView after workspace is loaded
              QTimer::singleShot(50, mainWindow, [mainWindow]() {
                qDebug()
                    << "[Main] Creating IndicesView after workspace loaded...";
                if (!mainWindow->hasIndicesView()) {
                  mainWindow->createIndicesView();
                }

                // Initialize window cache for fast Buy/Sell opening (~10ms)
                WindowCacheManager::instance().initialize(mainWindow);
              });
            });

            // Cleanup login window
            loginWindow->deleteLater();
          } else {
            qCritical() << "ERROR: MainWindow pointer is NULL!";
          }
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
