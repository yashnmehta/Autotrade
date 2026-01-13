#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>
#include <QProgressBar>
#include <QLabel>

namespace Ui {
class SplashScreen;
}

class MasterLoaderWorker;

class SplashScreen : public QWidget
{
    Q_OBJECT

public:
    explicit SplashScreen(QWidget *parent = nullptr);
    ~SplashScreen();

    // Progress control
    void setProgress(int value);
    void setStatus(const QString &status);

    // Display control
    void showCentered();
    
    /**
     * @brief Preload master contracts during initialization
     * 
     * Loads master data from cache in background during splash screen,
     * making scrip search instantly available after login.
     * Uses shared state to coordinate with LoginFlowService.
     */
    void preloadMasters();
    
    /**
     * @brief Load user preferences at startup
     * 
     * Loads preferences including PriceCache mode flag that determines
     * whether to use legacy or new zero-copy architecture.
     */
    void loadPreferences();

signals:
    /**
     * @brief Emitted when splash screen is ready to close
     * 
     * Signals that all initialization tasks are complete:
     * - Config loaded
     * - Masters loaded (or determined not available)
     * - Minimum display time elapsed
     */
    void readyToClose();

private slots:
    void onMasterLoadingComplete(int contractCount);
    void onMasterLoadingFailed(const QString& errorMessage);
    void onMasterLoadingProgress(int percentage, const QString& message);
    void checkIfReadyToClose();

private:
    Ui::SplashScreen *ui;
    MasterLoaderWorker *m_masterLoader;
    
    // Completion tracking for event-driven close
    bool m_masterLoadingComplete;
    bool m_minimumTimeElapsed;
};

#endif // SPLASHSCREEN_H
