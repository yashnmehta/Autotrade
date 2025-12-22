#include "app/MainWindow.h"
#include "ui/SplashScreen.h"
#include "ui/LoginWindow.h"
#include "services/LoginFlowService.h"
#include "utils/ConfigLoader.h"
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
    QApplication app(argc, argv);

    // Set application metadata
    app.setApplicationName("Trading Terminal");
    app.setOrganizationName("TradingCo");
    app.setApplicationVersion("1.0.0");

    // Phase 1: Show Splash Screen
    SplashScreen *splash = new SplashScreen();
    splash->showCentered();
    
    // Preload masters during splash (non-blocking via timer)
    splash->setStatus("Initializing...");
    splash->setProgress(10);
    splash->preloadMasters();

    // Continue with loading simulation
    QTimer splashTimer;
    int splashProgress = 50;  // Start at 50 since preload takes us there
    
    QObject::connect(&splashTimer, &QTimer::timeout, [&]() {
        splashProgress += 16;  // Adjusted increment
        if (splashProgress <= 100) {
            splash->setProgress(splashProgress);
            if (splashProgress == 66) splash->setStatus("Preparing UI...");
            else if (splashProgress == 82) splash->setStatus("Almost ready...");
            else if (splashProgress >= 98) {
                splash->setStatus("Ready!");
                splashTimer.stop();
                
                // Delay before showing login
                QTimer::singleShot(500, [&]() {
                    splash->close();
                    delete splash;
                    
                    // Phase 2: Show Login Window
                    LoginWindow *loginWindow = new LoginWindow();
                    
                    // Load credentials from config file
                    ConfigLoader *config = new ConfigLoader();
                    // Determine a list of candidate config locations that work
                    // across development builds, Linux, and macOS application bundles.
                    QString appDir = QCoreApplication::applicationDirPath();

                    QStringList candidates;
                    // 1) Standard application config location (per-platform)
                    QString appConfigDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
                    if (!appConfigDir.isEmpty()) {
                        candidates << QDir(appConfigDir).filePath("config.ini");
                    }

                    // 2) User-specific config in ~/.config/<AppName>/config.ini
                    QString homeConfig = QDir::home().filePath(QString(".config/%1/config.ini").arg(QCoreApplication::applicationName().replace(' ', "")));
                    candidates << homeConfig;

                    // 3) Common developer/project locations relative to the binary
                    // Keep the original project-relative fallback for dev builds
                    candidates << QDir(appDir).filePath("../../../../../configs/config.ini");
                    candidates << QDir(appDir).filePath("../configs/config.ini");
                    candidates << QDir(appDir).filePath("configs/config.ini");

                    // 4) macOS specific: inside app bundle Resources (if bundled)
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
                        qDebug() << "Loading credentials from config file";
                        loginWindow->setMarketDataAppKey(config->getMarketDataAppKey());
                        loginWindow->setMarketDataSecretKey(config->getMarketDataSecretKey());
                        loginWindow->setInteractiveAppKey(config->getInteractiveAppKey());
                        loginWindow->setInteractiveSecretKey(config->getInteractiveSecretKey());
                        loginWindow->setLoginID(config->getUserID());
                    } else {
                        qDebug() << "Config file not found or failed to load, using empty defaults";
                    }
                    
                    // Create login flow service
                    LoginFlowService *loginService = new LoginFlowService();
                    
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
                    
                    loginService->setCompleteCallback([loginWindow, mainWindow, loginService]() {
                        qDebug() << "✅ Login complete! Showing main window...";
                        
                        // Pass XTS clients to main window
                        mainWindow->setXTSClients(
                            loginService->getMarketDataClient(),
                            loginService->getInteractiveClient()
                        );
                        
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
                            baseURL  // Will be split into MD and IA URLs in service
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
                    
                    // Show login window
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
            }
        }
    });
    
    splashTimer.start(300); // Update every 300ms

    return app.exec();
}
