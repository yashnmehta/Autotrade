#ifndef APPBOOTSTRAP_H
#define APPBOOTSTRAP_H

#include <QObject>
#include <QString>
#include "services/LoginFlowService.h"

class QApplication;
class ConfigLoader;
class TradingDataService;
class MainWindow;
class SplashScreen;
class LoginWindow;

/**
 * @brief Application bootstrap controller
 *
 * Orchestrates the entire startup sequence:
 *   1. Register Qt meta-types
 *   2. Initialize logging, TA-Lib
 *   3. Show splash, load config, check license
 *   4. Pre-load master files
 *   5. Show login window, run login flow
 *   6. Wire services, show main window
 *
 * Extracted from main.cpp to keep the entry point minimal
 * and make the boot sequence testable / modifiable.
 */
class AppBootstrap : public QObject {
    Q_OBJECT

public:
    explicit AppBootstrap(QApplication *app, QObject *parent = nullptr);
    ~AppBootstrap();

    /**
     * @brief Run the full bootstrap sequence.
     * @return Application exit code (from QApplication::exec())
     */
    int run();

private:
    // ── Bootstrap phases ─────────────────────────────────────────────────
    void registerMetaTypes();
    void initializeTALib();
    void setAppMetadata();

    // Config & license (synchronous during splash)
    void loadConfiguration();
    void checkLicense();

    // Splash → Login → MainWindow (event-driven)
    void showSplashScreen();
    void onSplashReady();
    void showLoginWindow();
    void setupLoginCallbacks();
    void onLoginComplete();
    void onFetchError(const LoginFlowService::FetchError &err);
    void onLoginClicked();
    void onContinueClicked();

    // Cleanup
    void cleanup();

    // ── Members ──────────────────────────────────────────────────────────
    QApplication   *m_app;
    ConfigLoader   *m_config;
    QString         m_configPath;

    SplashScreen       *m_splash;
    LoginWindow        *m_loginWindow;
    LoginFlowService   *m_loginService;
    TradingDataService *m_tradingDataService;
    MainWindow         *m_mainWindow;
};

#endif // APPBOOTSTRAP_H
