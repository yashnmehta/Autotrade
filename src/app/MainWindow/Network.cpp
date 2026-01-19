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
        // ✅ CRITICAL FIX: Defer UDP start by 500ms to ensure:
        // 1. Main window is fully rendered and responsive
        // 2. UI thread has processed all pending events
        // 3. Socket initialization doesn't block the UI
        // 4. Distributed PriceStore is ready for concurrent updates
        // 5. IndicesView creation is complete (300ms delay in main.cpp)
        //
        // Increased from 100ms to 500ms to prevent thread storm during startup
        QTimer::singleShot(500, this, [this]() {
            qDebug() << "[MainWindow] Starting UDP broadcast receivers (staggered startup)...";
            startBroadcastReceiver();
        });
    } else {
        qDebug() << "[MainWindow] Auto-start broadcast disabled in settings";
    }
}
