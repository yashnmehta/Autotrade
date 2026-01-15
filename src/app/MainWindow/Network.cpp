#include "app/MainWindow.h"
#include <QSettings>
#include <QTimer>
#include <QDebug>

void MainWindow::setupNetwork() {
    // ✅ DEFERRED START: This is called AFTER login completes and main window is visible
    qDebug() << "[MainWindow] Setting up network services...";
    
    // Auto-start UDP broadcast receiver if enabled
    QSettings settings("TradingCompany", "TradingTerminal");
    if (settings.value("Network/AutoStartBroadcast", true).toBool()) {
        // ✅ Defer UDP start by 100ms to ensure:
        // 1. Main window is fully rendered and responsive
        // 2. UI thread can process user interactions
        // 3. Socket initialization doesn't block the UI
        QTimer::singleShot(100, this, [this]() {
            qDebug() << "[MainWindow] Starting UDP broadcast receivers...";
            startBroadcastReceiver();
        });
    } else {
        qDebug() << "[MainWindow] Auto-start broadcast disabled in settings";
    }
}
