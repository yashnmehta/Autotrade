#ifndef CHARTDATAFEED_H
#define CHARTDATAFEED_H

/**
 * @file ChartDataFeed.h
 * @brief TradingView custom datafeed bridge for the ChartsWindow.
 *
 * Handles:
 *   - Historical OHLC data retrieval via XTS REST API
 *   - Real-time candle accumulation from UDP ticks (FeedHandler)
 *   - IST-aligned candle boundary calculations
 *   - 50ms batched GUI updates (prevents QWebChannel IPC overload)
 *   - Proper subscribeBars/unsubscribeBars lifecycle
 *
 * Thread-safety contract:
 *   - ChartDataFeed lives on the GUI thread.
 *   - All public slots arrive via QWebChannel (Qt::QueuedConnection).
 *   - UDP ticks arrive via FeedHandler with Qt::DirectConnection.
 *   - m_rwLock guards: m_currentToken, m_currentSegment,
 *     m_currentSymbol, m_currentExchange, m_subscribers.
 *   - XTS OHLC HTTP fetch runs in a background thread;
 *     results are marshalled back via Qt::QueuedConnection.
 */

#include "api/xts/XTSTypes.h"
#include "udp/UDPTypes.h"
#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QReadWriteLock>
#include <QHash>
#include <QMap>
#include <QTimer>

class XTSMarketDataClient;

class ChartDataFeed : public QObject {
    Q_OBJECT

public:
    explicit ChartDataFeed(QObject *parent = nullptr);
    ~ChartDataFeed();

    /// Provide the XTS market-data client so getBars() can call
    /// the real /instruments/ohlc endpoint.
    void setMarketDataClient(XTSMarketDataClient *client);

public slots:
    // ── Called via QWebChannel from JavaScript ──
    void resolveSymbol(const QString &symbolName, const QString &callbackId);
    void getBars(const QString &symbolInfoStr, const QString &resolution,
                 long long fromTs, long long toTs, const QString &callbackId);
    void subscribeBars(const QString &symbolInfoStr, const QString &resolution,
                       const QString &subscriberUID);
    void unsubscribeBars(const QString &subscriberUID);
    void setCurrentToken(int64_t token, const QString &symbol, const QString &exchange);
    void notifyVisibleRangeChanged(double fromMs, double toMs);
    void placeOrderRequest(const QJsonObject &order);

signals:
    void symbolResolved(const QString &callbackId, const QJsonObject &symbolInfo);
    void barsReceived(const QString &callbackId, const QJsonArray &bars, bool noData);
    void realtimeUpdate(const QString &subscriberUID, const QJsonObject &bar);
    void visibleRangeChanged(double fromMs, double toMs);
    void orderFromChartRequested(const QJsonObject &order);

private slots:
    /// Receives parsed OHLC bars from the background HTTP thread.
    void onOHLCVBarsReady(const QString &internalCbId,
                          const QJsonArray &bars, bool noData);

    /// Event-driven live tick receiver — called on every UDP tick.
    void onUdpTickReceived(const UDP::MarketTick &tick);

    /// Timer slot to batch and emit GUI updates (every 50ms).
    void onUpdateTimer();

private:
    static QString resolutionToCompression(const QString &resolution);
    static int     resolutionToSeconds(const QString &resolution);
    static QString utcSecsToISTString(long long utcSecs);
    bool parseSymbolInfo(const QString &symbolInfoStr, int &segment, int64_t &token) const;
    void resetCandleAccumulator(const QString &uid, qint64 barStartSec = 0);

    XTSMarketDataClient *m_marketDataClient = nullptr;

    mutable QReadWriteLock m_rwLock;

    // Current MAIN chart subscription context (fallback)
    int64_t  m_currentToken   = 0;
    int      m_currentSegment = 1;
    QString  m_currentSymbol;
    QString  m_currentExchange;

    // ── Candle accumulator per subscriber ──
    struct SubscriberInfo {
        int      segment;
        uint32_t token;
        int      resolutionSecs;
        bool     isHistorySeeded;
        qint64   candleBarStartSec;
        double   candleOpen;
        double   candleHigh;
        double   candleLow;
        double   candleClose;
        qint64   candleVolume;      // Total volume to emit (Base + Delta)
        qint64   candleBaseVolume;  // Volume from historical API for this candle
        qint64   candleVolumePrev;  // Cumulative day-volume at start of live tracking
        bool     isVolumeSeeded;
        bool     hasPendingUpdate;
        QJsonArray completedBars;   // Bars completed since last timer tick
    };

    QHash<QString, SubscriberInfo> m_subscribers;

    // ── History seeding (last bar from getBars → subscribeBars) ──
    struct HistorySeed {
        qint64 startSec;
        qint64 volume;
        double open, high, low, close;
    };
    QHash<QString, HistorySeed> m_historySeeds;

    // ── Pending OHLC HTTP requests ──
    struct PendingRequest {
        QString tvCallbackId;
        int     segment;
        uint32_t token;
        int     resolutionSecs;
    };
    QHash<QString, PendingRequest> m_pendingBars;

    QTimer *m_updateTimer = nullptr;
};

#endif // CHARTDATAFEED_H
