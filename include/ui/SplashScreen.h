#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>
#include <QProgressBar>
#include <QLabel>

namespace Ui {
class SplashScreen;
}

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
     */
    void preloadMasters();

private:
    Ui::SplashScreen *ui;
};

#endif // SPLASHSCREEN_H
