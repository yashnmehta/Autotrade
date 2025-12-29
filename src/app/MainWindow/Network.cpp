#include "app/MainWindow.h"
#include <QSettings>

void MainWindow::setupNetwork() {
    // Auto-start UDP broadcast receiver
    QSettings settings("TradingCompany", "TradingTerminal");
    if (settings.value("Network/AutoStartBroadcast", true).toBool()) {
        startBroadcastReceiver();
    }
}
