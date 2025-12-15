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

private:
    Ui::SplashScreen *ui;
};

#endif // SPLASHSCREEN_H
