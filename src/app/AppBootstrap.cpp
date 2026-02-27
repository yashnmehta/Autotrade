#include "app/AppBootstrap.h"

#include "api/xts/XTSTypes.h"
#include "app/MainWindow.h"
#include "core/ExchangeSegment.h"
#include "core/WindowCacheManager.h"
#include "data/CandleData.h"
#include "services/CandleAggregator.h"
#include "services/GreeksCalculationService.h"
#include "services/LoginFlowService.h"
#include "services/TradingDataService.h"
#include "udp/UDPTypes.h"
#include "ui/LoginWindow.h"
#include "ui/SplashScreen.h"
#include "utils/ConfigLoader.h"
#include "utils/FileLogger.h"
#include "utils/LicenseManager.h"
#include "utils/PreferencesManager.h"
#ifdef HAVE_TALIB
#include "indicators/TALibIndicators.h"
#endif

#include <QApplication>
#include <QAbstractButton>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <cstdio>

// ═══════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════════════════

AppBootstrap::AppBootstrap(QApplication *app, QObject *parent)
    : QObject(parent)
    , m_app(app)
    , m_config(nullptr)
    , m_splash(nullptr)
    , m_loginWindow(nullptr)
    , m_loginService(nullptr)
    , m_tradingDataService(nullptr)
    , m_mainWindow(nullptr)
{
}

AppBootstrap::~AppBootstrap()
{
    cleanup();
}

// ═══════════════════════════════════════════════════════════════════════════
// Main entry point
// ═══════════════════════════════════════════════════════════════════════════

int AppBootstrap::run()
{
    fprintf(stderr, "[Bootstrap] Starting application...\n");
    fflush(stderr);

    // ── Synchronous init ─────────────────────────────────────────────────
    setupFileLogging();
    registerMetaTypes();
    initializeTALib();
    setAppMetadata();

    // ── Splash-driven init ───────────────────────────────────────────────
    showSplashScreen();           // shows splash
    loadConfiguration();          // loads config.ini during splash
    checkLicense();               // may exit the app if license fails

    // Pre-load masters (async, fires readyToClose when done)
    m_splash->setStatus("Initializing...");
    m_splash->preloadMasters();

    // When splash is ready, transition to login
    connect(m_splash, &SplashScreen::readyToClose, this, &AppBootstrap::onSplashReady);

    // ── Run event loop ───────────────────────────────────────────────────
    int exitCode = m_app->exec();

    cleanup();
    return exitCode;
}

// ═══════════════════════════════════════════════════════════════════════════
// Register Qt meta-types for cross-thread signals
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::registerMetaTypes()
{
    fprintf(stderr, "[Bootstrap] Registering MetaTypes...\n");
    fflush(stderr);

    // XTS types
    qRegisterMetaType<XTS::Tick>("XTS::Tick");
    qRegisterMetaType<XTS::Order>("XTS::Order");
    qRegisterMetaType<XTS::Trade>("XTS::Trade");
    qRegisterMetaType<XTS::Position>("XTS::Position");
    qRegisterMetaType<QVector<XTS::Order>>("QVector<XTS::Order>");
    qRegisterMetaType<QVector<XTS::Trade>>("QVector<XTS::Trade>");
    qRegisterMetaType<QVector<XTS::Position>>("QVector<XTS::Position>");

    // UDP broadcast types
    qRegisterMetaType<UDP::MarketTick>("UDP::MarketTick");
    qRegisterMetaType<UDP::IndexTick>("UDP::IndexTick");
    qRegisterMetaType<UDP::CircuitLimitTick>("UDP::CircuitLimitTick");
    qRegisterMetaType<ExchangeSegment>("ExchangeSegment");
    qRegisterMetaType<ExchangeSegment>("ExchangeReceiver");  // Type alias used in UdpBroadcastService signals

    // Greeks & chart types
    qRegisterMetaType<uint32_t>("uint32_t");
    qRegisterMetaType<GreeksResult>("GreeksResult");
    qRegisterMetaType<ChartData::Candle>("ChartData::Candle");
    qRegisterMetaType<ChartData::Timeframe>("ChartData::Timeframe");

    // License & login flow
    qRegisterMetaType<LicenseManager::CheckResult>("LicenseManager::CheckResult");
    qRegisterMetaType<LoginFlowService::FetchError>("LoginFlowService::FetchError");
}

// ═══════════════════════════════════════════════════════════════════════════
// TA-Lib initialisation
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::initializeTALib()
{
#ifdef HAVE_TALIB
    fprintf(stderr, "[Bootstrap] Initializing TA-Lib...\n");
    fflush(stderr);
    if (TALibIndicators::initialize()) {
        qDebug() << "[Bootstrap] TA-Lib initialized:" << TALibIndicators::getVersion();
    } else {
        qWarning() << "[Bootstrap] TA-Lib initialization failed.";
    }
#else
    qDebug() << "[Bootstrap] TA-Lib not available (compiled without HAVE_TALIB).";
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// Application metadata
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::setAppMetadata()
{
    m_app->setApplicationName("Trading Terminal");
    m_app->setOrganizationName("TradingCo");
    m_app->setApplicationVersion("1.0.0");
}

// ═══════════════════════════════════════════════════════════════════════════
// Splash screen
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::showSplashScreen()
{
    m_splash = new SplashScreen();
    m_splash->showCentered();
    m_splash->setStatus("Loading configuration...");
    m_splash->setProgress(5);
}

// ═══════════════════════════════════════════════════════════════════════════
// Configuration loading
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::loadConfiguration()
{
    fprintf(stderr, "[Bootstrap] Loading configuration...\n");
    fflush(stderr);

    m_config = new ConfigLoader();
    QString appDir = QCoreApplication::applicationDirPath();

    fprintf(stderr, "[Bootstrap] Application directory: %s\n", qPrintable(appDir));
    fflush(stderr);

    QStringList candidates;
    // 1) MSVC build config (build_msvc/Debug or build_msvc/Release)
    candidates << QDir(appDir).filePath("../../configs/config.ini");
    // 2) MinGW build config (build/)
    candidates << QDir(appDir).filePath("../configs/config.ini");
    // 3) Same directory as executable
    candidates << QDir(appDir).filePath("configs/config.ini");
    // 4) Standard application config location (per-platform)
    QString appConfigDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!appConfigDir.isEmpty()) {
        candidates << QDir(appConfigDir).filePath("config.ini");
    }
    // 5) User-specific config
    QString homeConfig = QDir::home().filePath(
        QString(".config/%1/config.ini")
            .arg(QCoreApplication::applicationName().replace(' ', "")));
    candidates << homeConfig;
    // 6) macOS bundle or other deep nesting
    candidates << QDir(appDir).filePath("../../../../configs/config.ini");
    candidates << QDir(appDir).filePath("../../../../../configs/config.ini");
    // 7) macOS specific: inside app bundle Resources
    candidates << QDir(appDir).filePath("../Resources/config.ini");

    m_configPath.clear();
    qWarning() << "[CONFIG] Starting config search...";
    qWarning() << "[CONFIG] Application directory:" << appDir;

    for (const QString &cand : candidates) {
        QString abs = QFileInfo(cand).absoluteFilePath();
        fprintf(stderr, "[Bootstrap] Checking: %s\n", qPrintable(abs));
        fflush(stderr);
        if (QFile::exists(abs)) {
            fprintf(stderr, "[Bootstrap] ✓ FOUND: %s\n", qPrintable(abs));
            fflush(stderr);
            if (m_config->load(abs)) {
                m_configPath = abs;
                fprintf(stderr, "[Bootstrap] ✓ Successfully loaded config\n");
                fflush(stderr);
                qWarning() << "[CONFIG] ✓ Loaded from:" << abs;
                break;
            } else {
                qWarning() << "[CONFIG] ✗ Found but failed to load:" << abs;
            }
        }
    }

    if (!m_configPath.isEmpty()) {
        qWarning() << "[CONFIG] Default client:" << m_config->getDefaultClient();
        qWarning() << "[CONFIG] User ID:" << m_config->getUserID();
        m_splash->setStatus("Configuration loaded");
    } else {
        qWarning() << "[CONFIG] ⚠ Config file not found, using defaults";
        m_splash->setStatus("Using default configuration");
    }
    m_splash->setProgress(10);
}

// ═══════════════════════════════════════════════════════════════════════════
// License check
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::checkLicense()
{
    fprintf(stderr, "[Bootstrap] Running license check...\n");
    fflush(stderr);

    m_splash->setStatus("Verifying license...");
    m_splash->setProgress(12);

    LicenseManager &licenseManager = LicenseManager::instance();
    licenseManager.initialize(m_config);

    LicenseManager::CheckResult licResult = licenseManager.checkLicense();

    if (!licResult.valid) {
        qCritical() << "[Bootstrap] ❌ License check FAILED:" << licResult.reason;
        m_splash->setStatus("License check failed");
        m_splash->setProgress(15);

        QTimer::singleShot(300, [this, licResult]() {
            QMessageBox msgBox;
            msgBox.setWindowTitle("License Error");
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setText("This application is not licensed to run on this machine.");
            msgBox.setInformativeText(licResult.reason);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.exec();

            m_splash->close();
            m_app->quit();
        });
        // This will block in app.exec() and the timer above will quit.
        // The caller (run()) will return the exit code.
        return;
    }

    qDebug() << "[Bootstrap] ✅ License check passed."
             << (licResult.isTrial ? "(Trial mode)" : "(Full license)")
             << (licResult.expiresAt.isValid()
                     ? QString("| Expires: %1").arg(licResult.expiresAt.toString(Qt::ISODate))
                     : QString("| Perpetual"));

    m_splash->setStatus("License verified");
    m_splash->setProgress(15);
}

// ═══════════════════════════════════════════════════════════════════════════
// Splash → Login transition
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::onSplashReady()
{
    qDebug() << "[Bootstrap] Splash screen ready, showing login window...";

    m_splash->close();
    m_splash->deleteLater();
    m_splash = nullptr;

    showLoginWindow();
}

// ═══════════════════════════════════════════════════════════════════════════
// Login window setup
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::showLoginWindow()
{
    m_loginWindow = new LoginWindow();

    // Populate credentials from config
    if (!m_configPath.isEmpty()) {
        qDebug() << "[Bootstrap] Populating login from config";
        m_loginWindow->setMarketDataAppKey(m_config->getMarketDataAppKey());
        m_loginWindow->setMarketDataSecretKey(m_config->getMarketDataSecretKey());
        m_loginWindow->setInteractiveAppKey(m_config->getInteractiveAppKey());
        m_loginWindow->setInteractiveSecretKey(m_config->getInteractiveSecretKey());
        m_loginWindow->setLoginID(m_config->getUserID());
    }

    // Create services
    m_loginService = new LoginFlowService();
    m_tradingDataService = new TradingDataService();
    m_loginService->setTradingDataService(m_tradingDataService);

    // Create main window (hidden)
    m_mainWindow = new MainWindow(nullptr);
    m_mainWindow->hide();

    // Wire masters-loaded → ScripBar refresh
    connect(m_loginService, &LoginFlowService::mastersLoaded, m_mainWindow,
            [this]() {
                qDebug() << "[Bootstrap] Masters loaded, refreshing ScripBar...";
                m_mainWindow->refreshScripBar();
            });

    setupLoginCallbacks();

    // Show login as modal dialog
    m_loginWindow->showCentered();
    int result = m_loginWindow->exec();

    if (result == QDialog::Rejected) {
        qDebug() << "[Bootstrap] Login cancelled by user";
        m_loginWindow->deleteLater();
        m_loginWindow = nullptr;
        if (m_mainWindow) {
            m_mainWindow->close();
            m_mainWindow->deleteLater();
            m_mainWindow = nullptr;
        }
        QApplication::quit();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Login callbacks wiring
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::setupLoginCallbacks()
{
    // Status callback
    m_loginService->setStatusCallback(
        [this](const QString &phase, const QString &message, int progress) {
            if (phase == "md_login") {
                m_loginWindow->setMDStatus(progress >= 30 ? "✓ Connected" : "Connecting...", false);
            } else if (phase == "ia_login") {
                m_loginWindow->setIAStatus(progress >= 60 ? "✓ Connected" : "Connecting...", false);
            }
        });

    // Error callback
    m_loginService->setErrorCallback(
        [this](const QString &phase, const QString &error) {
            if (phase == "md_login") {
                m_loginWindow->setMDStatus(QString("✗ Error: %1").arg(error), true);
                m_loginWindow->enableLoginButton();
            } else if (phase == "ia_login") {
                m_loginWindow->setIAStatus(QString("✗ Error: %1").arg(error), true);
                m_loginWindow->enableLoginButton();
            }
        });

    // Complete callback
    m_loginService->setCompleteCallback([this]() { onLoginComplete(); });

    // Fetch-error callback
    m_loginService->setFetchErrorCallback(
        [this](const LoginFlowService::FetchError &err) { onFetchError(err); });

    // Login button
    m_loginWindow->setOnLoginClicked([this]() { onLoginClicked(); });

    // Continue button
    m_loginWindow->setOnContinueClicked([this]() { onContinueClicked(); });
}

// ═══════════════════════════════════════════════════════════════════════════
// Login complete → wire services, show continue
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::onLoginComplete()
{
    qDebug() << "[Bootstrap] ✅ Login complete! Wiring services...";

    CandleAggregator::instance().initialize(true);
    qDebug() << "[Bootstrap] CandleAggregator initialized";

    m_mainWindow->setXTSClients(m_loginService->getMarketDataClient(),
                                m_loginService->getInteractiveClient());
    m_mainWindow->setTradingDataService(m_tradingDataService);
    m_mainWindow->setConfigLoader(m_config);

    m_loginWindow->showContinueButton();
    m_loginWindow->enableLoginButton();
}

// ═══════════════════════════════════════════════════════════════════════════
// Data-sync error handling
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::onFetchError(const LoginFlowService::FetchError &err)
{
    QStringList failedItems;
    if (err.positionsFailed) failedItems << "Positions";
    if (err.ordersFailed)    failedItems << "Orders";
    if (err.tradesFailed)    failedItems << "Trades";

    QString failedStr = failedItems.join(", ");
    QString detail    = err.summary();

    qWarning() << "[Bootstrap] Data sync failed for:" << failedStr;

    QMessageBox msgBox(m_loginWindow);
    msgBox.setWindowTitle("Data Sync Incomplete");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(QString("<b>Could not load: %1</b>").arg(failedStr));
    msgBox.setInformativeText(
        detail + "\n\n"
        "You can <b>Retry</b> to fetch the data again,\n"
        "or <b>Continue</b> to open the terminal with partial data\n"
        "(missing data will appear empty until the next refresh).");

    QPushButton *retryBtn    = msgBox.addButton("Retry", QMessageBox::AcceptRole);
    QPushButton *continueBtn = msgBox.addButton("Continue Anyway", QMessageBox::DestructiveRole);
    Q_UNUSED(continueBtn);
    msgBox.setDefaultButton(retryBtn);
    msgBox.exec();

    if (qobject_cast<QPushButton*>(msgBox.clickedButton()) == retryBtn) {
        m_loginWindow->setMDStatus("Retrying data sync...", false);
        m_loginWindow->setIAStatus("Retrying data sync...", false);
        m_loginService->retryDataFetch();
    } else {
        // Continue with partial data
        CandleAggregator::instance().initialize(true);
        m_mainWindow->setXTSClients(m_loginService->getMarketDataClient(),
                                    m_loginService->getInteractiveClient());
        m_mainWindow->setTradingDataService(m_tradingDataService);
        m_mainWindow->setConfigLoader(m_config);
        m_loginWindow->showContinueButton();
        m_loginWindow->enableLoginButton();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Login button clicked
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::onLoginClicked()
{
    qDebug() << "[Bootstrap] Login button clicked";

    QString mdAppKey    = m_loginWindow->getMarketDataAppKey();
    QString mdSecretKey = m_loginWindow->getMarketDataSecretKey();
    QString iaAppKey    = m_loginWindow->getInteractiveAppKey();
    QString iaSecretKey = m_loginWindow->getInteractiveSecretKey();
    QString loginID     = m_loginWindow->getLoginID();
    bool downloadMasters = m_loginWindow->shouldDownloadMasters();

    m_loginWindow->disableLoginButton();
    m_loginWindow->setMDStatus("Connecting...", false);
    m_loginWindow->setIAStatus("Connecting...", false);

    QString baseURL = m_config->getXTSUrl();
    if (baseURL.isEmpty()) {
        baseURL = "https://ttblaze.iifl.com";
    }

    m_loginService->executeLogin(mdAppKey, mdSecretKey, iaAppKey, iaSecretKey,
                                 loginID, downloadMasters, baseURL,
                                 m_config->getSource());
}

// ═══════════════════════════════════════════════════════════════════════════
// Continue button clicked → show MainWindow
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::onContinueClicked()
{
    qDebug() << "[Bootstrap] Continue clicked — showing main window";

    if (!m_mainWindow) {
        qCritical() << "[Bootstrap] ERROR: MainWindow is NULL!";
        return;
    }

    // Show main window FIRST to prevent Qt auto-quit
    m_mainWindow->show();
    m_mainWindow->raise();
    m_mainWindow->activateWindow();

    // Close login AFTER main window is visible
    m_loginWindow->accept();

    // Defer workspace loading to prevent blocking during dialog close
    QTimer::singleShot(10, m_mainWindow, [this]() {
        qDebug() << "[Bootstrap] Loading workspace...";

        QString defaultWorkspace = PreferencesManager::instance().getDefaultWorkspace();
        if (!defaultWorkspace.isEmpty() && defaultWorkspace != "Default") {
            if (m_mainWindow->loadWorkspaceByName(defaultWorkspace)) {
                qDebug() << "[Bootstrap] Loaded workspace:" << defaultWorkspace;
            } else {
                qWarning() << "[Bootstrap] Workspace failed:" << defaultWorkspace;
            }
        }

        // Create IndicesView & window cache after workspace
        QTimer::singleShot(50, m_mainWindow, [this]() {
            qDebug() << "[Bootstrap] Creating IndicesView...";
            if (!m_mainWindow->hasIndicesView()) {
                m_mainWindow->createIndicesView();
            }
            WindowCacheManager::instance().initialize(m_mainWindow);
        });
    });

    m_loginWindow->deleteLater();
    m_loginWindow = nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════
// Cleanup
// ═══════════════════════════════════════════════════════════════════════════

void AppBootstrap::cleanup()
{
#ifdef HAVE_TALIB
    TALibIndicators::shutdown();
    qDebug() << "[Bootstrap] TA-Lib shut down.";
#endif
    cleanupFileLogging();
}
