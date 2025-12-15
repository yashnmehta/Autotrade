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

    // Simulate initial loading
    QTimer splashTimer;
    int splashProgress = 0;
    
    QObject::connect(&splashTimer, &QTimer::timeout, [&]() {
        splashProgress += 20;
        if (splashProgress <= 100) {
            splash->setProgress(splashProgress);
            if (splashProgress == 20) splash->setStatus("Loading configuration...");
            else if (splashProgress == 40) splash->setStatus("Initializing components...");
            else if (splashProgress == 60) splash->setStatus("Preparing UI...");
            else if (splashProgress == 80) splash->setStatus("Almost ready...");
            else if (splashProgress == 100) {
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
                    // Get application directory and construct config path
                    QString appDir = QCoreApplication::applicationDirPath();
                    QString configPath = appDir + "/../../configs/config.ini"; // Go up from .app/Contents/MacOS
                    
                    qDebug() << "Looking for config at:" << configPath;
                    if (QFile::exists(configPath) && config->load(configPath)) {
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
                    MainWindow *mainWindow = new MainWindow();
                    
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
                        
                        // Execute login flow
                        loginService->executeLogin(
                            mdAppKey, mdSecretKey,
                            iaAppKey, iaSecretKey,
                            loginID, downloadMasters,
                            baseURL
                        );
                    });
                    
                    // Setup continue button callback
                    loginWindow->setOnContinueClicked([loginWindow, mainWindow]() {
                        qDebug() << "Continue button clicked - showing main window";
                        loginWindow->accept();
                        delete loginWindow;
                        mainWindow->show();
                    });
                    
                    // Show login window
                    int result = loginWindow->exec();
                    
                    if (result == QDialog::Rejected) {
                        // User clicked Exit or closed window
                        qDebug() << "Login cancelled by user";
                        delete loginWindow;
                        delete mainWindow;
                        QApplication::quit();
                    }
                });
            }
        }
    });
    
    splashTimer.start(300); // Update every 300ms

    return app.exec();
}
