#include "ui/SplashScreen.h"
#include "ui_SplashScreen.h"
#include <QApplication>
#include <QScreen>

SplashScreen::SplashScreen(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , ui(new Ui::SplashScreen)
{
    ui->setupUi(this);
    
    // Make widget semi-transparent background
    setAttribute(Qt::WA_TranslucentBackground, false);
    
    // Set initial values
    ui->progressBar->setValue(0);
    ui->labelStatus->setText("Initializing...");
}

SplashScreen::~SplashScreen()
{
    delete ui;
}

void SplashScreen::setProgress(int value)
{
    ui->progressBar->setValue(value);
    QApplication::processEvents(); // Force UI update
}

void SplashScreen::setStatus(const QString &status)
{
    ui->labelStatus->setText(status);
    QApplication::processEvents(); // Force UI update
}

void SplashScreen::showCentered()
{
    show();
    
    // Center on screen
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
    
    QApplication::processEvents();
}
