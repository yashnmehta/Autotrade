#ifndef CONNECTIONSTATUSMANAGER_H
#define CONNECTIONSTATUSMANAGER_H

#include <QObject>
#include <QMap>
#include <QElapsedTimer>
#include <QTimer>
#include <QMutex>
#include <QDateTime>
#include <atomic>
#include <memory>
#include <vector>
#include <utility>

class XTSMarketDataClient;
class XTSInteractiveClient;

// ═══════════════════════════════════════════════════════════════════════
// Enums — shared by all connection-related UI components
// ═══════════════════════════════════════════════════════════════════════

/**
 * @brief Identifies each managed connection endpoint
 * 
 * There are exactly 6 connections tracked by the trading terminal:
 * - 2 XTS WebSocket connections (MD + IA, always-on)
 * - 4 UDP multicast receivers (NSE FO/CM, BSE FO/CM, started per config)
 */
enum class ConnectionId {
    XTS_MARKET_DATA,    // XTS Market Data WebSocket  (ALWAYS connected)
    XTS_INTERACTIVE,    // XTS Interactive (Order) WebSocket
    UDP_NSEFO,          // NSE F&O UDP multicast receiver
    UDP_NSECM,          // NSE Cash Market UDP multicast receiver
    UDP_BSEFO,          // BSE F&O UDP multicast receiver
    UDP_BSECM           // BSE Cash Market UDP multicast receiver
};

/**
 * @brief Connection lifecycle state
 */
enum class ConnectionState {
    Disconnected,       // Not started or explicitly stopped
    Connecting,         // Attempting initial connection
    Connected,          // Active and receiving data
    Reconnecting,       // Lost connection, attempting recovery
    Error               // Failed — see errorMessage
};

/**
 * @brief Which service is the PRIMARY source for live tick data
 *
 * Architecture:
 *   - XTS MD WebSocket is ALWAYS connected.
 *   - It always provides 1505 (candle) data for chart windows.
 *   The "primary source" only controls who provides touchline/depth/LTP ticks.
 *
 *   UDP_PRIMARY   (default for office / co-located):
 *     UDP multicast → FeedHandler.  XTS REST subscribes only for 1505 candles.
 *
 *   XTS_PRIMARY   (internet / home user, or UDP failure):
 *     XTS REST subscribe (1501/1502) → XTS WS ticks → FeedHandler.
 *     UDP receivers may still run but are NOT the tick source.
 *
 * Switching at runtime triggers subscription migration:
 *   UDP→XTS: Subscribe all active FeedHandler tokens via XTSFeedBridge REST
 *   XTS→UDP: Unsubscribe those tokens from XTS REST (free cap), ensure
 *            UDP filtering includes them, data flows automatically.
 */
enum class PrimaryDataSource {
    UDP_PRIMARY,    // UDP multicast is the primary tick provider
    XTS_PRIMARY     // XTS WebSocket is the primary tick provider
};

// ═══════════════════════════════════════════════════════════════════════
// ConnectionInfo — snapshot of a single connection
// ═══════════════════════════════════════════════════════════════════════

struct ConnectionInfo {
    ConnectionId    id;
    ConnectionState state       = ConnectionState::Disconnected;
    QString         displayName;        // "XTS MD", "UDP NSEFO", etc.
    QString         errorMessage;       // Non-empty when state == Error
    QString         address;            // IP:port or WebSocket URL
    QDateTime       connectedSince;     // Timestamp of last Connected transition
    QDateTime       lastActivity;       // Timestamp of last data received

    // Live statistics (updated by stats refresh timer)
    uint64_t        totalPackets   = 0; // Lifetime packet/message count
    double          packetsPerSec  = 0; // Rolling average (1-second window)
    double          latencyMs      = 0; // Estimated latency (if measurable)

    // Derived convenience
    bool isConnected() const { return state == ConnectionState::Connected; }
    bool isUDP()       const {
        return id == ConnectionId::UDP_NSEFO || id == ConnectionId::UDP_NSECM ||
               id == ConnectionId::UDP_BSEFO || id == ConnectionId::UDP_BSECM;
    }
    bool isXTS()       const {
        return id == ConnectionId::XTS_MARKET_DATA || id == ConnectionId::XTS_INTERACTIVE;
    }

    QString uptimeString() const;       // "2h 15m" or "—"
};

// ═══════════════════════════════════════════════════════════════════════
// ConnectionStatusManager — singleton service
// ═══════════════════════════════════════════════════════════════════════

/**
 * @brief Aggregates connection state + manages primary data source switching
 *
 * Two responsibilities:
 *   1. Track connection state for all 6 endpoints (status indicators)
 *   2. Manage primary data source (UDP vs XTS) with subscription migration
 *
 * KEY ARCHITECTURE:
 *   XTS MD WebSocket is ALWAYS connected.
 *   It always provides 1505 (candle) data for chart windows.
 *   The "primary source" only controls who provides touchline/depth/LTP ticks.
 *
 *   When switching primary source at runtime, this manager orchestrates
 *   the subscription migration between UDP and XTS so there is no data gap.
 */
class ConnectionStatusManager : public QObject
{
    Q_OBJECT

public:
    static ConnectionStatusManager& instance();

    // ─── Query API ───────────────────────────────────────────────────

    ConnectionInfo getConnectionInfo(ConnectionId id) const;
    QList<ConnectionInfo> getAllConnections() const;
    int connectedCount() const;
    int udpConnectedCount() const;
    int udpTotalCount() const;
    double totalUdpPacketsPerSec() const;
    QString udpSummaryString() const;
    QString xtsSummaryString() const;

    // ─── Primary Data Source ─────────────────────────────────────────

    /** @brief Current primary data source */
    PrimaryDataSource primarySource() const;

    /** @brief Human-readable label: "Hybrid (UDP)" or "XTS Only" */
    QString primarySourceLabel() const;

    /** @brief Legacy alias — returns "Hybrid (UDP)" or "XTS Only" for ConnectionBarWidget */
    QString feedModeString() const;

    /** @brief Is UDP the primary tick provider? */
    bool isUdpPrimary() const;

    /**
     * @brief Switch primary data source at RUNTIME
     *
     * This is the main entry point for runtime switching.  It:
     *   1. Updates XTSFeedBridge feed mode
     *   2. Migrates all active FeedHandler subscriptions
     *   3. Starts/stops UDP receivers if needed
     *   4. Emits primarySourceChanged()
     *
     * Safe to call from UI thread (migration is batched + async).
     *
     * @param source New primary data source
     * @param startStopUdp If true, also start/stop UDP receivers.
     *                     If false, only change subscription routing.
     */
    void switchPrimarySource(PrimaryDataSource source, bool startStopUdp = false);

    /**
     * @brief Set default primary source from config (NO migration, called at startup)
     */
    void setDefaultPrimarySource(PrimaryDataSource source);

    // ─── Feed Mode (legacy string-based API for UI compatibility) ────

    /** @brief Set feed mode from string: "hybrid" or "xts_only" */
    void setFeedMode(const QString& mode);

    // ─── State Update API (called by services, NOT by UI) ────────────

    void setState(ConnectionId id, ConnectionState state,
                  const QString& errorMessage = QString());
    void setAddress(ConnectionId id, const QString& address);
    void recordActivity(ConnectionId id, uint64_t packetsDelta = 1);

    // ─── XTS Client Wiring ───────────────────────────────────────────

    void wireXTSMarketDataClient(XTSMarketDataClient* client);
    void wireXTSInteractiveClient(XTSInteractiveClient* client);
    void wireUdpBroadcastService();

signals:
    void stateChanged(ConnectionId id, ConnectionState newState,
                      const ConnectionInfo& info);
    void statsUpdated();
    void primarySourceChanged(PrimaryDataSource newSource);

    /** @brief Emitted when feed mode label changes (for ConnectionBarWidget) */
    void feedModeChanged(const QString& modeLabel);

    void overallStatusChanged(bool anyConnected);

    /**
     * @brief Emitted during subscription migration with progress info
     */
    void migrationProgress(const QString& message);

private:
    ConnectionStatusManager();
    ~ConnectionStatusManager();
    ConnectionStatusManager(const ConnectionStatusManager&) = delete;
    ConnectionStatusManager& operator=(const ConnectionStatusManager&) = delete;

    void initializeConnections();
    void refreshStats();

    /**
     * @brief Migrate all active FeedHandler subscriptions to the new primary
     *
     * UDP → XTS: Collect all active tokens from FeedHandler, push them
     *            through XTSFeedBridge::requestSubscribe() (batched + rate-limited)
     * XTS → UDP: Unsubscribe those tokens from XTS REST to free the cap.
     *            UDP already receives all multicast data — just ensure the
     *            token filter (UdpBroadcastService::subscribeToken) is in place.
     *
     * NOTE: This is called asynchronously from switchPrimarySource() via
     *       QTimer::singleShot(0) to avoid blocking the UI event loop.
     */
    void migrateSubscriptions(PrimaryDataSource oldSource, PrimaryDataSource newSource);

    /**
     * @brief Queue the next batch of tokens for XTS subscription
     *
     * Called recursively via QTimer::singleShot(0) to process tokens in
     * small batches (50 at a time), yielding to the event loop between
     * each batch so the UI stays responsive and REST response signals
     * can be delivered.
     */
    void migrateBatchToXTS(std::shared_ptr<std::vector<std::pair<int, uint32_t>>> tokens,
                           size_t startIdx, size_t totalCount);

    // ─── State ───────────────────────────────────────────────────────
    mutable QMutex m_mutex;
    QMap<ConnectionId, ConnectionInfo> m_connections;

    static constexpr int NUM_CONNECTIONS = 6;
    std::atomic<uint64_t> m_packetCounters[NUM_CONNECTIONS] = {};
    uint64_t m_lastPacketSnapshot[NUM_CONNECTIONS] = {};

    QTimer* m_statsTimer = nullptr;
    std::atomic<PrimaryDataSource> m_primarySource{PrimaryDataSource::UDP_PRIMARY};
    bool m_wasAnyConnected = false;
};

// ═══════════════════════════════════════════════════════════════════════
// Utility functions
// ═══════════════════════════════════════════════════════════════════════

QString connectionStateToString(ConnectionState state);
QString connectionIdToLabel(ConnectionId id);
QString connectionStateColor(ConnectionState state);

#endif // CONNECTIONSTATUSMANAGER_H
