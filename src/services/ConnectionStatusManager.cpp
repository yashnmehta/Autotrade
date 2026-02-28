#include "services/ConnectionStatusManager.h"
#include "api/xts/XTSMarketDataClient.h"
#include "api/xts/XTSInteractiveClient.h"
#include "services/UdpBroadcastService.h"
#include "services/XTSFeedBridge.h"
#include "services/FeedHandler.h"
#include "core/ExchangeSegment.h"
#include "repository/RepositoryManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════════
// Utility functions
// ═══════════════════════════════════════════════════════════════════════

QString connectionStateToString(ConnectionState state) {
    switch (state) {
        case ConnectionState::Disconnected: return "Disconnected";
        case ConnectionState::Connecting:   return "Connecting";
        case ConnectionState::Connected:    return "Connected";
        case ConnectionState::Reconnecting: return "Reconnecting";
        case ConnectionState::Error:        return "Error";
    }
    return "Unknown";
}

QString connectionIdToLabel(ConnectionId id) {
    switch (id) {
        case ConnectionId::XTS_MARKET_DATA: return "XTS MD";
        case ConnectionId::XTS_INTERACTIVE: return "XTS IA";
        case ConnectionId::UDP_NSEFO:       return "NSEFO";
        case ConnectionId::UDP_NSECM:       return "NSECM";
        case ConnectionId::UDP_BSEFO:       return "BSEFO";
        case ConnectionId::UDP_BSECM:       return "BSECM";
    }
    return "?";
}

QString connectionStateColor(ConnectionState state) {
    switch (state) {
        case ConnectionState::Connected:    return "#16a34a";  // Green
        case ConnectionState::Connecting:
        case ConnectionState::Reconnecting: return "#f59e0b";  // Amber
        case ConnectionState::Disconnected: return "#94a3b8";  // Slate
        case ConnectionState::Error:        return "#dc2626";  // Red
    }
    return "#94a3b8";
}

// ═══════════════════════════════════════════════════════════════════════
// ConnectionInfo helpers
// ═══════════════════════════════════════════════════════════════════════

QString ConnectionInfo::uptimeString() const {
    if (!connectedSince.isValid() || state != ConnectionState::Connected)
        return QStringLiteral("\xe2\x80\x94"); // em-dash

    qint64 secs = connectedSince.secsTo(QDateTime::currentDateTime());
    if (secs < 60)    return QString("%1s").arg(secs);
    if (secs < 3600)  return QString("%1m %2s").arg(secs / 60).arg(secs % 60);
    return QString("%1h %2m").arg(secs / 3600).arg((secs % 3600) / 60);
}

// ═══════════════════════════════════════════════════════════════════════
// Singleton
// ═══════════════════════════════════════════════════════════════════════

ConnectionStatusManager& ConnectionStatusManager::instance() {
    static ConnectionStatusManager s_instance;
    return s_instance;
}

ConnectionStatusManager::ConnectionStatusManager()
    : QObject(nullptr)
{
    initializeConnections();

    // Stats refresh timer — computes packets/sec every 1 second
    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(1000);
    connect(m_statsTimer, &QTimer::timeout, this, &ConnectionStatusManager::refreshStats);
    m_statsTimer->start();

    qDebug() << "[ConnectionStatusManager] Initialized — 6 connections, primary=UDP";
}

ConnectionStatusManager::~ConnectionStatusManager() {
    // Disconnect all incoming signals to prevent use-after-free during
    // static destruction. UdpBroadcastService may outlive this manager.
    disconnect();
    if (m_statsTimer) m_statsTimer->stop();
}

// ═══════════════════════════════════════════════════════════════════════
// Initialization
// ═══════════════════════════════════════════════════════════════════════

void ConnectionStatusManager::initializeConnections() {
    auto add = [this](ConnectionId id, const QString& name) {
        ConnectionInfo info;
        info.id = id;
        info.displayName = name;
        info.state = ConnectionState::Disconnected;
        m_connections[id] = info;
    };

    add(ConnectionId::XTS_MARKET_DATA, "XTS Market Data");
    add(ConnectionId::XTS_INTERACTIVE, "XTS Interactive");
    add(ConnectionId::UDP_NSEFO,       "NSE F&O UDP");
    add(ConnectionId::UDP_NSECM,       "NSE Cash UDP");
    add(ConnectionId::UDP_BSEFO,       "BSE F&O UDP");
    add(ConnectionId::UDP_BSECM,       "BSE Cash UDP");
}

// ═══════════════════════════════════════════════════════════════════════
// Query API
// ═══════════════════════════════════════════════════════════════════════

ConnectionInfo ConnectionStatusManager::getConnectionInfo(ConnectionId id) const {
    QMutexLocker lock(&m_mutex);
    return m_connections.value(id);
}

QList<ConnectionInfo> ConnectionStatusManager::getAllConnections() const {
    QMutexLocker lock(&m_mutex);
    return m_connections.values();
}

int ConnectionStatusManager::connectedCount() const {
    QMutexLocker lock(&m_mutex);
    int count = 0;
    for (const auto& info : m_connections) {
        if (info.isConnected()) ++count;
    }
    return count;
}

int ConnectionStatusManager::udpConnectedCount() const {
    QMutexLocker lock(&m_mutex);
    int count = 0;
    for (const auto& info : m_connections) {
        if (info.isUDP() && info.isConnected()) ++count;
    }
    return count;
}

int ConnectionStatusManager::udpTotalCount() const {
    QMutexLocker lock(&m_mutex);
    int count = 0;
    for (const auto& info : m_connections) {
        // Count UDP connections that are not Disconnected (i.e. attempted)
        if (info.isUDP() && info.state != ConnectionState::Disconnected)
            ++count;
    }
    return count;
}

double ConnectionStatusManager::totalUdpPacketsPerSec() const {
    QMutexLocker lock(&m_mutex);
    double total = 0;
    for (const auto& info : m_connections) {
        if (info.isUDP()) total += info.packetsPerSec;
    }
    return total;
}

QString ConnectionStatusManager::udpSummaryString() const {
    int connected = udpConnectedCount();
    int total = udpTotalCount();
    double pps = totalUdpPacketsPerSec();

    QString rateStr;
    if (pps >= 1000.0)
        rateStr = QString("%1k/s").arg(pps / 1000.0, 0, 'f', 1);
    else
        rateStr = QString("%1/s").arg(static_cast<int>(pps));

    if (total == 0)
        return "UDP: Off";

    return QString("UDP: %1/%2 (%3)").arg(connected).arg(total).arg(rateStr);
}

QString ConnectionStatusManager::xtsSummaryString() const {
    QMutexLocker lock(&m_mutex);
    auto mdState = m_connections.value(ConnectionId::XTS_MARKET_DATA).state;
    auto iaState = m_connections.value(ConnectionId::XTS_INTERACTIVE).state;

    auto dot = [](ConnectionState s) -> QString {
        if (s == ConnectionState::Connected)    return QStringLiteral("\xe2\x97\x8f"); // ●
        if (s == ConnectionState::Connecting ||
            s == ConnectionState::Reconnecting) return QStringLiteral("\xe2\x97\x90"); // ◐
        return QStringLiteral("\xe2\x97\x8b"); // ○
    };

    return QString("MD %1  IA %2").arg(dot(mdState)).arg(dot(iaState));
}

// ═══════════════════════════════════════════════════════════════════════
// Primary Data Source
// ═══════════════════════════════════════════════════════════════════════

PrimaryDataSource ConnectionStatusManager::primarySource() const {
    return m_primarySource.load(std::memory_order_acquire);
}

QString ConnectionStatusManager::primarySourceLabel() const {
    return isUdpPrimary() ? "Hybrid (UDP)" : "XTS Only";
}

QString ConnectionStatusManager::feedModeString() const {
    return primarySourceLabel();
}

bool ConnectionStatusManager::isUdpPrimary() const {
    return m_primarySource.load(std::memory_order_acquire) == PrimaryDataSource::UDP_PRIMARY;
}

void ConnectionStatusManager::setDefaultPrimarySource(PrimaryDataSource source) {
    m_primarySource.store(source, std::memory_order_release);

    // Set XTSFeedBridge mode accordingly (no migration at startup)
    FeedMode bridgeMode = (source == PrimaryDataSource::XTS_PRIMARY)
                          ? FeedMode::XTS_ONLY : FeedMode::HYBRID;
    XTSFeedBridge::instance().setFeedMode(bridgeMode);

    qDebug() << "[ConnectionStatusManager] Default primary source:"
             << (source == PrimaryDataSource::UDP_PRIMARY ? "UDP" : "XTS");

    emit primarySourceChanged(source);
    emit feedModeChanged(primarySourceLabel());
}

void ConnectionStatusManager::setFeedMode(const QString& mode) {
    PrimaryDataSource source = PrimaryDataSource::UDP_PRIMARY;
    if (mode == "xts_only" || mode == "xtsonly" || mode == "websocket" || mode == "XTS Only") {
        source = PrimaryDataSource::XTS_PRIMARY;
    }
    setDefaultPrimarySource(source);
}

void ConnectionStatusManager::switchPrimarySource(PrimaryDataSource source,
                                                   bool startStopUdp) {
    PrimaryDataSource oldSource = m_primarySource.load(std::memory_order_acquire);
    if (oldSource == source) {
        qDebug() << "[ConnectionStatusManager] Already using requested primary source";
        return;
    }

    qDebug() << "═══════════════════════════════════════════════════════";
    qDebug() << "[ConnectionStatusManager] SWITCHING primary data source:";
    qDebug() << "   FROM:" << (oldSource == PrimaryDataSource::UDP_PRIMARY ? "UDP" : "XTS");
    qDebug() << "   TO:  " << (source == PrimaryDataSource::UDP_PRIMARY ? "UDP" : "XTS");
    qDebug() << "═══════════════════════════════════════════════════════";

    emit migrationProgress("Switching data source...");

    // Step 1: Update the bridge FIRST so new subscriptions go to the right place
    FeedMode bridgeMode = (source == PrimaryDataSource::XTS_PRIMARY)
                          ? FeedMode::XTS_ONLY : FeedMode::HYBRID;
    XTSFeedBridge::instance().setFeedMode(bridgeMode);

    // Step 2: Store new state IMMEDIATELY (so new subscriptions route correctly)
    m_primarySource.store(source, std::memory_order_release);

    emit primarySourceChanged(source);
    emit feedModeChanged(primarySourceLabel());

    // Step 3: Optionally start/stop UDP receivers
    if (startStopUdp) {
        if (source == PrimaryDataSource::XTS_PRIMARY) {
            qDebug() << "[ConnectionStatusManager] UDP receivers kept running (standby)";
        }
    }

    // Step 4: Migrate existing subscriptions ASYNCHRONOUSLY
    // ──────────────────────────────────────────────────────────────────
    // CRITICAL: Migration MUST be deferred to let the UI event loop breathe.
    //
    // Why? reRegisterAllTokens() calls bridge.requestSubscribe() for every
    // active token. Each subscribe triggers an async REST call via
    // QtConcurrent::run(). The subscriptionCompleted signal is delivered
    // to XTSFeedBridge on the UI thread. If we block the UI thread here
    // in a synchronous loop, those signals can never be delivered, causing
    // a deadlock (UI thread waiting → REST thread signal waiting → UI thread).
    //
    // By deferring with QTimer::singleShot(0), we:
    //   1. Return control to the event loop immediately
    //   2. Let pending signals (subscriptionCompleted, UI repaints) process
    //   3. Then start migration in small batches that yield to the event loop
    // ──────────────────────────────────────────────────────────────────
    QTimer::singleShot(0, this, [this, oldSource, source]() {
        migrateSubscriptions(oldSource, source);
        emit migrationProgress("Data source switched successfully");
        qDebug() << "[ConnectionStatusManager] Switch complete";
    });
}

// ═══════════════════════════════════════════════════════════════════════
// Subscription Migration — the critical runtime handoff
// ═══════════════════════════════════════════════════════════════════════

void ConnectionStatusManager::migrateSubscriptions(PrimaryDataSource oldSource,
                                                    PrimaryDataSource newSource) {
    auto& feedHandler = FeedHandler::instance();
    auto& bridge = XTSFeedBridge::instance();

    // Get snapshot of active tokens FIRST (short lock, then release)
    auto activeTokens = feedHandler.getActiveTokens();
    size_t totalSubs = activeTokens.size();
    qDebug() << "[Migration] Active FeedHandler subscriptions:" << totalSubs;

    if (newSource == PrimaryDataSource::XTS_PRIMARY) {
        // ──────────────────────────────────────────────────────────────
        // UDP → XTS: Subscribe active tokens via XTS REST (BULK)
        // ──────────────────────────────────────────────────────────────
        // The bridge is now in XTS_ONLY mode (set in switchPrimarySource).
        // Use bulkSubscribeAll to subscribe all tokens in a single API call
        // per segment. This is much faster than batched individual calls.

        const int XTS_LIMIT = bridge.config().maxTotalSubscriptions;
        
        if (totalSubs == 0) {
            emit migrationProgress("No tokens to migrate");
            return;
        }

        // Warn if we have more tokens than XTS can handle
        if (totalSubs > static_cast<size_t>(XTS_LIMIT)) {
            qWarning() << "[Migration] ⚠ Active tokens (" << totalSubs
                       << ") exceed XTS limit (" << XTS_LIMIT << ")";
            qWarning() << "[Migration] Only first" << XTS_LIMIT
                       << "tokens will receive live data via XTS";
            emit migrationProgress(
                QString("⚠ %1 tokens exceed XTS limit of %2 — excess tokens dropped")
                    .arg(totalSubs).arg(XTS_LIMIT));
        } else {
            emit migrationProgress(
                QString("Subscribing %1 tokens via XTS REST API (bulk)...").arg(totalSubs));
        }

        // Bulk subscribe all tokens in single API calls (one per segment)
        // Also add NSE CM index tokens so IndicesView stays live in XTS-only mode
        RepositoryManager* repo = RepositoryManager::getInstance();
        if (repo) {
            const auto& indexTokenMap = repo->getIndexTokenNameMap();
            const int nsecmSeg = static_cast<int>(ExchangeSegment::NSECM);
            for (auto it = indexTokenMap.constBegin(); it != indexTokenMap.constEnd(); ++it) {
                uint32_t idxToken = it.key();
                // Only append if not already present (avoid duplicates)
                bool found = std::any_of(activeTokens.begin(), activeTokens.end(),
                    [&](const std::pair<int,uint32_t>& p){ return p.first == nsecmSeg && p.second == idxToken; });
                if (!found) {
                    activeTokens.push_back({nsecmSeg, idxToken});
                }
            }
            qDebug() << "[Migration] Added" << indexTokenMap.size()
                     << "NSE CM index tokens for IndicesView. Total:" << activeTokens.size();
        }

        int subscribed = bridge.bulkSubscribeAll(activeTokens);
        
        qDebug() << "[Migration] UDP→XTS complete:" << subscribed
                 << "tokens subscribed via bulk API";
        emit migrationProgress(
            QString("Migration complete — %1 tokens subscribed via XTS").arg(subscribed));

    } else {
        // ──────────────────────────────────────────────────────────────
        // XTS → UDP: Unsubscribe from XTS REST, let UDP take over
        // ──────────────────────────────────────────────────────────────
        // UDP already receives ALL multicast data for all instruments.
        // We just need to unsubscribe from XTS REST to free the 500 cap.
        // But keep 1505 candle subscriptions alive for chart windows.
        //
        // unsubscribeAllExceptCandles() releases its lock before REST calls,
        // so this is safe on the UI thread.

        emit migrationProgress("Migrating to UDP — freeing XTS subscriptions...");

        bridge.unsubscribeAllExceptCandles();

        qDebug() << "[Migration] XTS→UDP: Unsubscribed non-candle tokens from XTS REST";
    }
}

// NOTE: migrateBatchToXTS is kept for backward compatibility but no longer used
// The bulk approach in migrateSubscriptions is preferred.
void ConnectionStatusManager::migrateBatchToXTS(
    std::shared_ptr<std::vector<std::pair<int, uint32_t>>> tokens,
    size_t startIdx, size_t totalCount)
{
    // Bail if the user switched back while we were migrating
    if (m_primarySource.load(std::memory_order_acquire) != PrimaryDataSource::XTS_PRIMARY) {
        qDebug() << "[Migration] Aborted — primary source changed during migration";
        emit migrationProgress("Migration cancelled");
        return;
    }

    auto& bridge = XTSFeedBridge::instance();

    // Process a batch of tokens (50 at a time to match XTS bridge batch size)
    static constexpr size_t BATCH_SIZE = 50;
    size_t endIdx = std::min(startIdx + BATCH_SIZE, tokens->size());

    for (size_t i = startIdx; i < endIdx; ++i) {
        const auto& [segment, token] = (*tokens)[i];
        bridge.requestSubscribe(token, segment);
    }

    qDebug() << "[Migration] Queued batch" << startIdx << "-" << endIdx
             << "of" << totalCount << "tokens";

    emit migrationProgress(
        QString("Subscribing tokens: %1 / %2").arg(endIdx).arg(totalCount));

    // If more tokens remain, schedule the next batch via event loop
    if (endIdx < tokens->size()) {
        QTimer::singleShot(0, this, [this, tokens, endIdx, totalCount]() {
            migrateBatchToXTS(tokens, endIdx, totalCount);
        });
    } else {
        auto stats = bridge.getStats();
        qDebug() << "[Migration] UDP→XTS complete: Queued" << totalCount
                 << "tokens (pending:" << stats.totalPending
                 << " active:" << stats.totalSubscribed << ")";
        emit migrationProgress(
            QString("Migration complete — %1 tokens queued for XTS subscription")
                .arg(totalCount));
    }
}

// ═══════════════════════════════════════════════════════════════════════
// State Update API
// ═══════════════════════════════════════════════════════════════════════

void ConnectionStatusManager::setState(ConnectionId id, ConnectionState state,
                                        const QString& errorMessage) {
    // Guard: during static destruction (exit / __cxa_finalize), QApplication
    // is already gone. Emitting signals at this point delivers them to
    // destroyed widgets (e.g. ConnectionBarWidget) → SIGSEGV.
    if (!QCoreApplication::instance()) return;

    ConnectionInfo snapshot;
    bool changed = false;
    bool overallChanged = false;

    {
        QMutexLocker lock(&m_mutex);
        auto it = m_connections.find(id);
        if (it == m_connections.end()) return;

        ConnectionState oldState = it->state;
        if (oldState == state && it->errorMessage == errorMessage)
            return;

        it->state = state;
        it->errorMessage = errorMessage;

        if (state == ConnectionState::Connected && oldState != ConnectionState::Connected) {
            it->connectedSince = QDateTime::currentDateTime();
        } else if (state != ConnectionState::Connected) {
            it->connectedSince = QDateTime();
        }

        snapshot = *it;
        changed = true;

        bool anyConnected = false;
        for (const auto& info : m_connections) {
            if (info.isConnected()) { anyConnected = true; break; }
        }
        if (anyConnected != m_wasAnyConnected) {
            m_wasAnyConnected = anyConnected;
            overallChanged = true;
        }
    }

    if (changed) {
        qDebug() << "[ConnectionStatusManager]"
                 << connectionIdToLabel(id) << "->"
                 << connectionStateToString(state)
                 << (errorMessage.isEmpty() ? "" : QString(" (%1)").arg(errorMessage));
        emit stateChanged(id, state, snapshot);
    }

    if (overallChanged) {
        emit overallStatusChanged(m_wasAnyConnected);
    }
}

void ConnectionStatusManager::setAddress(ConnectionId id, const QString& address) {
    QMutexLocker lock(&m_mutex);
    auto it = m_connections.find(id);
    if (it != m_connections.end()) {
        it->address = address;
    }
}

void ConnectionStatusManager::recordActivity(ConnectionId id, uint64_t packetsDelta) {
    int idx = static_cast<int>(id);
    if (idx >= 0 && idx < NUM_CONNECTIONS) {
        m_packetCounters[idx].fetch_add(packetsDelta, std::memory_order_relaxed);
    }
    // lastActivity update — acceptable race (no mutex on hot path)
    {
        QMutexLocker lock(&m_mutex);
        auto it = m_connections.find(id);
        if (it != m_connections.end()) {
            it->lastActivity = QDateTime::currentDateTime();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Stats Refresh (1-second timer)
// ═══════════════════════════════════════════════════════════════════════

void ConnectionStatusManager::refreshStats() {
    {
        QMutexLocker lock(&m_mutex);
        for (int i = 0; i < NUM_CONNECTIONS; ++i) {
            ConnectionId cid = static_cast<ConnectionId>(i);
            auto it = m_connections.find(cid);
            if (it == m_connections.end()) continue;

            uint64_t current = m_packetCounters[i].load(std::memory_order_relaxed);
            uint64_t delta = current - m_lastPacketSnapshot[i];
            m_lastPacketSnapshot[i] = current;

            it->totalPackets = current;
            it->packetsPerSec = static_cast<double>(delta);
        }
    }
    emit statsUpdated();
}

// ═══════════════════════════════════════════════════════════════════════
// XTS Client Wiring
// ═══════════════════════════════════════════════════════════════════════

void ConnectionStatusManager::wireXTSMarketDataClient(XTSMarketDataClient* client) {
    if (!client) return;

    connect(client, &XTSMarketDataClient::wsConnectionStatusChanged,
            this, [this](bool connected, const QString& error) {
        if (connected) {
            setState(ConnectionId::XTS_MARKET_DATA, ConnectionState::Connected);
        } else if (error.isEmpty()) {
            setState(ConnectionId::XTS_MARKET_DATA, ConnectionState::Disconnected);
        } else {
            setState(ConnectionId::XTS_MARKET_DATA, ConnectionState::Error, error);
        }
    });

    connect(client, &XTSMarketDataClient::loginCompleted,
            this, [this](bool success, const QString& error) {
        if (success) {
            setState(ConnectionId::XTS_MARKET_DATA, ConnectionState::Connecting);
        } else {
            setState(ConnectionId::XTS_MARKET_DATA, ConnectionState::Error, error);
        }
    });

    connect(client, &XTSMarketDataClient::errorOccurred,
            this, [this](const QString& error) {
        auto info = getConnectionInfo(ConnectionId::XTS_MARKET_DATA);
        if (info.state != ConnectionState::Connected) {
            setState(ConnectionId::XTS_MARKET_DATA, ConnectionState::Error, error);
        }
    });

    // Sampled tick activity
    connect(client, &XTSMarketDataClient::tickReceived,
            this, [this](const XTS::Tick&) {
        recordActivity(ConnectionId::XTS_MARKET_DATA);
    }, Qt::DirectConnection);

    qDebug() << "[ConnectionStatusManager] Wired XTS Market Data client";
}

void ConnectionStatusManager::wireXTSInteractiveClient(XTSInteractiveClient* client) {
    if (!client) return;

    connect(client, &XTSInteractiveClient::connectionStatusChanged,
            this, [this](bool connected) {
        setState(ConnectionId::XTS_INTERACTIVE,
                 connected ? ConnectionState::Connected : ConnectionState::Disconnected);
    });

    connect(client, &XTSInteractiveClient::loginCompleted,
            this, [this](bool success, const QString& error) {
        if (success) {
            setState(ConnectionId::XTS_INTERACTIVE, ConnectionState::Connecting);
        } else {
            setState(ConnectionId::XTS_INTERACTIVE, ConnectionState::Error, error);
        }
    });

    connect(client, &XTSInteractiveClient::errorOccurred,
            this, [this](const QString& error) {
        auto info = getConnectionInfo(ConnectionId::XTS_INTERACTIVE);
        if (info.state != ConnectionState::Connected) {
            setState(ConnectionId::XTS_INTERACTIVE, ConnectionState::Error, error);
        }
    });

    connect(client, &XTSInteractiveClient::orderEvent,
            this, [this](const XTS::Order&) {
        recordActivity(ConnectionId::XTS_INTERACTIVE);
    }, Qt::DirectConnection);

    connect(client, &XTSInteractiveClient::tradeEvent,
            this, [this](const XTS::Trade&) {
        recordActivity(ConnectionId::XTS_INTERACTIVE);
    }, Qt::DirectConnection);

    qDebug() << "[ConnectionStatusManager] Wired XTS Interactive client";
}

// ═══════════════════════════════════════════════════════════════════════
// UDP Wiring
// ═══════════════════════════════════════════════════════════════════════

namespace {
    ConnectionId exchangeSegToConnectionId(ExchangeSegment seg) {
        switch (seg) {
            case ExchangeSegment::NSEFO: return ConnectionId::UDP_NSEFO;
            case ExchangeSegment::NSECM: return ConnectionId::UDP_NSECM;
            case ExchangeSegment::BSEFO: return ConnectionId::UDP_BSEFO;
            case ExchangeSegment::BSECM: return ConnectionId::UDP_BSECM;
            default: return ConnectionId::UDP_NSEFO;
        }
    }
}

void ConnectionStatusManager::wireUdpBroadcastService() {
    auto& udp = UdpBroadcastService::instance();

    connect(&udp, &UdpBroadcastService::receiverStatusChanged,
            this, [this](ExchangeSegment receiver, bool active) {
        ConnectionId cid = exchangeSegToConnectionId(receiver);
        setState(cid, active ? ConnectionState::Connected : ConnectionState::Disconnected);
    });

    connect(&udp, &UdpBroadcastService::statusChanged,
            this, [this](bool active) {
        if (!active) {
            setState(ConnectionId::UDP_NSEFO, ConnectionState::Disconnected);
            setState(ConnectionId::UDP_NSECM, ConnectionState::Disconnected);
            setState(ConnectionId::UDP_BSEFO, ConnectionState::Disconnected);
            setState(ConnectionId::UDP_BSECM, ConnectionState::Disconnected);
        }
    });

    // Tick activity tracking (maps exchange segment → connection ID)
    connect(&udp, &UdpBroadcastService::udpTickReceived,
            this, [this](const UDP::MarketTick& tick) {
        ConnectionId cid;
        switch (tick.exchangeSegment) {
            case UDP::ExchangeSegment::NSEFO: cid = ConnectionId::UDP_NSEFO; break;
            case UDP::ExchangeSegment::NSECM: cid = ConnectionId::UDP_NSECM; break;
            case UDP::ExchangeSegment::BSEFO: cid = ConnectionId::UDP_BSEFO; break;
            case UDP::ExchangeSegment::BSECM: cid = ConnectionId::UDP_BSECM; break;
            default: return;
        }
        recordActivity(cid);
    }, Qt::DirectConnection);

    qDebug() << "[ConnectionStatusManager] Wired UdpBroadcastService";
}
