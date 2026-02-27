#ifndef LOGINFLOWSERVICE_H
#define LOGINFLOWSERVICE_H

#include "api/xts/XTSMarketDataClient.h"
#include "api/xts/XTSInteractiveClient.h"
#include "api/xts/XTSTypes.h"
#include "services/MasterLoaderWorker.h"
#include <QObject>
#include <QTimer>
#include <QVector>
#include <functional>

class LoginFlowService : public QObject
{
    Q_OBJECT

public:
    // ------------------------------------------------------------------
    // Describes which REST fetch(es) failed so the UI can show detail
    // ------------------------------------------------------------------
    struct FetchError {
        bool positionsFailed = false;
        bool ordersFailed    = false;
        bool tradesFailed    = false;
        QStringList messages;           ///< one entry per failed fetch

        bool anyFailed() const {
            return positionsFailed || ordersFailed || tradesFailed;
        }
        QString summary() const { return messages.join("\n"); }
    };

    explicit LoginFlowService(QObject *parent = nullptr);
    ~LoginFlowService();

    // Execute complete login flow
    void executeLogin(
        const QString &mdAppKey,
        const QString &mdSecretKey,
        const QString &iaAppKey,
        const QString &iaSecretKey,
        const QString &loginID,
        bool downloadMasters,
        const QString &baseURL,
        const QString &source = "WEBAPI"
    );

    /**
     * @brief Re-fetch all account data (positions, orders, trades) without
     *        re-logging in.  Used by the "Retry" path after a fetch failure.
     *        The IA WebSocket stays connected; events continue to buffer while
     *        the fresh REST snapshots are downloaded.
     */
    void retryDataFetch();

    // Getters for API clients
    XTSMarketDataClient*  getMarketDataClient()  const { return m_mdClient; }
    XTSInteractiveClient* getInteractiveClient() const { return m_iaClient; }

    // Set trading data service for storing fetched data
    void setTradingDataService(class TradingDataService* service) {
        m_tradingDataService = service;
    }

    // Status callbacks
    void setStatusCallback(std::function<void(const QString&, const QString&, int)> cb);
    void setErrorCallback (std::function<void(const QString&, const QString&)> cb);
    void setCompleteCallback(std::function<void()> cb);

    /**
     * @brief Called when one or more REST fetches fail after retries are
     *        exhausted.  Provides a FetchError struct so the UI can show
     *        exactly which fetches failed and offer a Retry button.
     */
    void setFetchErrorCallback(std::function<void(const FetchError&)> cb);

signals:
    void statusUpdate(const QString &phase, const QString &message, int progress);
    void errorOccurred(const QString &phase, const QString &error);
    void loginComplete();
    void mastersLoaded();

    /// Emitted when REST data sync completes (even partially) so the UI
    /// can reflect the actual data quality.
    void dataSyncComplete(bool allSucceeded, FetchError errors);

private:
    // ── Core flow helpers ───────────────────────────────────────────────
    void updateStatus(const QString &phase, const QString &message, int progress);
    void notifyError  (const QString &phase, const QString &error);

    void handleMasterLoadingComplete(int contractCount);
    void handleMasterLoadingFailed(const QString& errorMessage);
    void continueLoginAfterMasters();
    void startMastersPhase();
    void startMasterDownload(const QString& mastersDir);

    // ── Snapshot + replay internals ─────────────────────────────────────
    /**
     * @brief Start the IA WebSocket immediately so no events are ever lost,
     *        but queue them in the event buffer until the REST snapshot is ready.
     */
    void connectIASocketAndBuffer();

    /**
     * @brief Fetch all three REST snapshots.  While fetching, live events
     *        continue to arrive and are queued in the buffer structs below.
     *        On completion, applySnapshotAndReplay() is called.
     */
    void fetchAllRestSnapshots();

    /**
     * @brief Apply the completed REST snapshots to TradingDataService and
     *        then replay any buffered live events on top, in arrival order.
     *        This guarantees zero event loss regardless of REST latency.
     */
    void applySnapshotAndReplay();

    /**
     * @brief Start (or restart) the per-fetch timeout timer.
     *        If all three REST calls don't complete within the window,
     *        fetchErrorCallback is invoked with the partial FetchError.
     */
    void startFetchTimeout();
    void cancelFetchTimeout();
    void onFetchTimeout();

    // ── API clients ─────────────────────────────────────────────────────
    XTSMarketDataClient  *m_mdClient  = nullptr;
    XTSInteractiveClient *m_iaClient  = nullptr;
    TradingDataService   *m_tradingDataService = nullptr;
    MasterLoaderWorker   *m_masterLoader = nullptr;

    // ── Login state ─────────────────────────────────────────────────────
    bool m_mdLoggedIn    = false;
    bool m_iaLoggedIn    = false;
    bool m_downloadMasters = false;
    bool m_loginCompleted  = false;   ///< guard: completeCallback fires once only

    // ── Snapshot + replay buffers ────────────────────────────────────────
    // REST snapshots (written once, from the callback thread)
    QVector<XTS::Position> m_snapshotPositions;
    QVector<XTS::Order>    m_snapshotOrders;
    QVector<XTS::Trade>    m_snapshotTrades;

    // Completion flags for the three REST fetches
    bool m_positionsFetched = false;
    bool m_ordersFetched    = false;
    bool m_tradesFetched    = false;

    // Live-event buffers — events that arrive while REST is in flight
    // These are replayed on top of the snapshot once it is complete.
    QVector<XTS::Position> m_bufferedPositionEvents;
    QVector<XTS::Order>    m_bufferedOrderEvents;
    QVector<XTS::Trade>    m_bufferedTradeEvents;

    // Whether the IA socket is connected and events are being buffered
    bool m_iaSocketConnected = false;
    bool m_snapshotApplied   = false;   ///< true after applySnapshotAndReplay()

    // Per-fetch error tracking
    FetchError m_fetchError;

    // ── Timeout guard ────────────────────────────────────────────────────
    static constexpr int kFetchTimeoutMs = 20000;  ///< 20 s per fetch cycle
    QTimer *m_fetchTimeoutTimer = nullptr;

    // ── Saved login credentials for retry ────────────────────────────────
    QString m_savedBaseURL;
    QString m_savedSource;

    // ── Callbacks ────────────────────────────────────────────────────────
    std::function<void(const QString&, const QString&, int)> m_statusCallback;
    std::function<void(const QString&, const QString&)>      m_errorCallback;
    std::function<void()>                                    m_completeCallback;
    std::function<void(const FetchError&)>                   m_fetchErrorCallback;
};

Q_DECLARE_METATYPE(LoginFlowService::FetchError)

#endif // LOGINFLOWSERVICE_H
