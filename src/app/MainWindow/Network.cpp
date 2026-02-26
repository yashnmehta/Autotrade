#include "app/MainWindow.h"
#include "services/GreeksCalculationService.h"
#include "services/XTSFeedBridge.h"
#include "repository/RepositoryManager.h"
#include "utils/ConfigLoader.h"
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QDebug>

void MainWindow::setupNetwork() {
    // ✅ DEFERRED START: This is called AFTER login completes and main window is visible
    qDebug() << "[MainWindow] Setting up network services...";

    // Check feed mode — skip UDP entirely in XTS_ONLY mode
    if (m_configLoader && m_configLoader->getFeedMode() == "xts_only") {
        qDebug() << "[MainWindow] Feed mode is XTS_ONLY — skipping UDP broadcast receivers";
        qDebug() << "[MainWindow] All price data will come through XTS WebSocket";
        return;
    }
    
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
            // Initialize Greeks Calculation Service
            // Needs to be done before UDP starts feeding it data
            qDebug() << "[MainWindow] Initializing Greeks Calculation Service...";
            auto& greeksService = GreeksCalculationService::instance();
            greeksService.loadConfiguration();
            greeksService.setRepositoryManager(RepositoryManager::getInstance());
            
            qDebug() << "[MainWindow] Starting UDP broadcast receivers (staggered startup)...";
            startBroadcastReceiver();
        });
    } else {
        qDebug() << "[MainWindow] Auto-start broadcast disabled in settings";
    }
}

void MainWindow::initializeXTSFeedBridge() {
    if (!m_configLoader) return;

    auto& bridge = XTSFeedBridge::instance();

    // 1. Inject the XTS market data client
    bridge.setMarketDataClient(m_xtsMarketDataClient);

    // 2. Load rate-limit config from config.ini
    XTSFeedBridge::Config cfg;
    cfg.maxTotalSubscriptions = m_configLoader->getFeedMaxTotalSubscriptions();
    cfg.maxRestCallsPerSec    = m_configLoader->getFeedMaxRestCallsPerSec();
    cfg.batchSize             = m_configLoader->getFeedBatchSize();
    cfg.batchIntervalMs       = m_configLoader->getFeedBatchIntervalMs();
    cfg.cooldownOnRateLimitMs = m_configLoader->getFeedCooldownOnRateLimitMs();
    bridge.setConfig(cfg);

    // 3. Determine feed mode from config
    QString modeStr = m_configLoader->getFeedMode();
    FeedMode mode = FeedMode::HYBRID;
    if (modeStr == "xts_only" || modeStr == "xtsonly" || modeStr == "websocket") {
        mode = FeedMode::XTS_ONLY;
    }
    bridge.setFeedMode(mode);

    // 4. If XTS_ONLY, skip UDP startup (it will fail anyway)
    if (mode == FeedMode::XTS_ONLY) {
        qDebug() << "[MainWindow] Feed mode: XTS_ONLY — UDP receivers will NOT be started";
        qDebug() << "[MainWindow] All market data will come through XTS WebSocket";
    } else {
        qDebug() << "[MainWindow] Feed mode: HYBRID — UDP + XTS WebSocket";
    }

    // 5. Connect stats signal for status bar updates
    connect(&bridge, &XTSFeedBridge::subscriptionStatsChanged,
            this, [this](int subscribed, int pending, int capacity) {
        if (m_statusBar) {
            m_statusBar->showMessage(
                QString("XTS Subs: %1 active, %2 pending (cap: %3)")
                    .arg(subscribed).arg(pending).arg(capacity), 3000);
        }
    });

    connect(&bridge, &XTSFeedBridge::rateLimitHit,
            this, [this](int cooldownMs) {
        if (m_statusBar) {
            m_statusBar->showMessage(
                QString("⚠ XTS rate limit hit — pausing %1s")
                    .arg(cooldownMs / 1000), cooldownMs);
        }
    });

    bridge.dumpStats();
}
