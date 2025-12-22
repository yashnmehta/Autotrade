#include "ui/SplashScreen.h"
#include "ui_SplashScreen.h"
#include "repository/RepositoryManager.h"
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QDebug>

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

void SplashScreen::preloadMasters()
{
    // Load cached masters in the background during splash screen
    // This makes scrip search instantly available after login
    
    setStatus("Loading master contracts...");
    setProgress(30);
    
    // Use QTimer to allow UI to update before blocking operation
    QTimer::singleShot(100, this, [this]() {
        QString mastersDir = RepositoryManager::getMastersDirectory();
        RepositoryManager* repo = RepositoryManager::getInstance();
        
        if (repo->loadAll(mastersDir)) {
            qDebug() << "[SplashScreen] ✓ Masters preloaded successfully";
            auto stats = repo->getSegmentStats();
            qDebug() << "[SplashScreen]   NSE F&O:" << stats.nsefo << "contracts";
            qDebug() << "[SplashScreen]   NSE CM:" << stats.nsecm << "contracts";
            setStatus("Master contracts loaded");
        } else {
            qDebug() << "[SplashScreen] ⚠ No cached masters found (will download on login)";
            setStatus("Masters will download after login");
        }
        
        setProgress(50);
    });
}
