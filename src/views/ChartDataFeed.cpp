/**
 * @file ChartDataFeed.cpp
 * @brief TradingView custom datafeed bridge implementation.
 *
 * Adapted from reference implementation with:
 *   - Proper IST-aligned candle accumulator
 *   - 50ms batched GUI updates
 *   - Async OHLC fetching via NativeHTTPClient
 *   - Volume delta computation
 *   - FeedHandler-based live ticks
 */

#include "views/ChartDataFeed.h"
#include "api/xts/XTSMarketDataClient.h"
#include "api/transport/NativeHTTPClient.h"
#include "services/FeedHandler.h"
#include "repository/RepositoryManager.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QUrl>
#include <QtConcurrent>
#include <atomic>

// Internal serial number for unique per-request callback IDs.
static std::atomic<uint64_t> g_ohlcvSerial{0};

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────
ChartDataFeed::ChartDataFeed(QObject *parent)
    : QObject(parent)
{
    // Start an update timer to debounce chart updates at 50ms
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &ChartDataFeed::onUpdateTimer);
    m_updateTimer->start(50);

    qDebug() << "[ChartDataFeed] Created";
}

ChartDataFeed::~ChartDataFeed()
{
    // Unsubscribe all FeedHandler connections
    FeedHandler::instance().unsubscribeAll(this);
    qDebug() << "[ChartDataFeed] Destroyed";
}

// ─────────────────────────────────────────────────────────────────────────────
// Public setters
// ─────────────────────────────────────────────────────────────────────────────
void ChartDataFeed::setMarketDataClient(XTSMarketDataClient *client)
{
    m_marketDataClient = client;
    if (m_marketDataClient)
        qDebug() << "[ChartDataFeed] Market data client attached";
}

// ─────────────────────────────────────────────────────────────────────────────
// setCurrentToken — called from GUI thread when user picks a stock
// ─────────────────────────────────────────────────────────────────────────────
void ChartDataFeed::setCurrentToken(int64_t token,
                                    const QString &symbol,
                                    const QString &exchange)
{
    int segment = exchange.isEmpty() ? 1 : exchange.toInt();
    if (segment <= 0) segment = 1;

    QWriteLocker wl(&m_rwLock);

    if (m_currentToken == token && m_currentSegment == segment &&
        m_currentSymbol == symbol && m_currentExchange == exchange)
        return;

    qDebug() << "[ChartDataFeed] Token changed:" << m_currentToken << "->" << token
             << "symbol:" << symbol;

    m_currentToken    = token;
    m_currentSegment  = segment;
    m_currentSymbol   = symbol;
    m_currentExchange = exchange;
}

// ─────────────────────────────────────────────────────────────────────────────
// resolveSymbol — called via QWebChannel (GUI thread)
// ─────────────────────────────────────────────────────────────────────────────
void ChartDataFeed::resolveSymbol(const QString &symbolName,
                                   const QString &callbackId)
{
    qDebug() << "[ChartDataFeed] Resolving symbol:" << symbolName;

    QString exchangeName = "NSE";
    {
        QReadLocker rl(&m_rwLock);
        if      (m_currentExchange == "1")  exchangeName = "NSE";
        else if (m_currentExchange == "2")  exchangeName = "NSE";
        else if (m_currentExchange == "11") exchangeName = "BSE";
        else if (m_currentExchange == "12") exchangeName = "BSE";
        else if (!m_currentExchange.isEmpty()) exchangeName = m_currentExchange;
    }

    QJsonObject symbolInfo;
    symbolInfo["name"]                  = symbolName;
    symbolInfo["ticker"]                = symbolName;
    symbolInfo["description"]           = symbolName;
    symbolInfo["type"]                  = "stock";
    symbolInfo["session"]               = "0915-1530";
    symbolInfo["timezone"]              = "Asia/Kolkata";
    symbolInfo["minmov"]                = 5;
    symbolInfo["pricescale"]            = 100;
    symbolInfo["has_intraday"]          = true;
    symbolInfo["has_daily"]             = true;
    symbolInfo["intraday_multipliers"]  = QJsonArray({"1", "5", "15", "30", "60"});
    symbolInfo["supported_resolutions"] = QJsonArray({"1", "5", "15", "30", "60", "1D"});
    symbolInfo["volume_precision"]      = 0;
    symbolInfo["data_status"]           = "streaming";
    symbolInfo["format"]                = "price";
    symbolInfo["exchange"]              = exchangeName;
    symbolInfo["listed_exchange"]       = exchangeName;
    symbolInfo["currency_code"]         = "INR";

    emit symbolResolved(callbackId, symbolInfo);
}

// ─────────────────────────────────────────────────────────────────────────────
// getBars — called via QWebChannel (GUI thread). Fully async.
// ─────────────────────────────────────────────────────────────────────────────
void ChartDataFeed::getBars(const QString &symbolInfoStr,
                             const QString &resolution,
                             long long fromTs, long long toTs,
                             const QString &callbackId)
{
    qDebug() << "[ChartDataFeed] getBars:" << symbolInfoStr << "res:" << resolution
             << "from:" << fromTs << "to:" << toTs;

    int64_t tokenToFetch = 0;
    int     segment      = 1;

    if (!parseSymbolInfo(symbolInfoStr, segment, tokenToFetch)) {
        QReadLocker rl(&m_rwLock);
        tokenToFetch = m_currentToken;
        segment      = m_currentSegment;
    }

    if (!m_marketDataClient || tokenToFetch == 0) {
        qDebug() << "[ChartDataFeed] getBars: no client or token — returning noData";
        emit barsReceived(callbackId, QJsonArray(), true);
        return;
    }

    QString startStr    = utcSecsToISTString(fromTs);
    QString endStr      = utcSecsToISTString(toTs);
    QString compression = resolutionToCompression(resolution);

    uint64_t serial    = g_ohlcvSerial.fetch_add(1, std::memory_order_relaxed);
    QString internalId = QStringLiteral("ohlcv_%1").arg(serial);

    PendingRequest req;
    req.tvCallbackId   = callbackId;
    req.segment        = segment;
    req.token          = static_cast<uint32_t>(tokenToFetch);
    req.resolutionSecs = resolutionToSeconds(resolution);
    m_pendingBars.insert(internalId, req);

    // Get auth token
    QString authToken;
    if (m_marketDataClient)
        authToken = m_marketDataClient->token();

    int compressionSeconds = compression.toInt();
    int segCopy = segment;
    int64_t tokenCopy = tokenToFetch;

    // Fetch OHLC data asynchronously to avoid blocking GUI
    QtConcurrent::run([this, authToken, internalId, fromTs, toTs,
                       segCopy, tokenCopy, compressionSeconds]() {
        NativeHTTPClient client;
        client.setTimeout(15);

        std::map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json";
        if (!authToken.isEmpty())
            headers["authorization"] = authToken.toStdString();

        QJsonArray finalBars;
        qint64 searchTo   = toTs;
        qint64 searchFrom = fromTs;
        int daysExtended   = 0;
        const int MIN_CANDLES = 150;
        const int MAX_EXTENSION_DAYS = 7;

        while (finalBars.size() < MIN_CANDLES && daysExtended <= MAX_EXTENSION_DAYS) {
            QString startStr = utcSecsToISTString(searchFrom);
            QString endStr   = utcSecsToISTString(searchTo);

            QString urlStr = QString(
                "http://192.168.102.9:3000/apimarketdata/instruments/ohlc"
                "?exchangeSegment=%1&exchangeInstrumentID=%2"
                "&startTime=%3&endTime=%4&compressionValue=%5")
                .arg(segCopy)
                .arg(tokenCopy)
                .arg(QString::fromUtf8(QUrl::toPercentEncoding(startStr)))
                .arg(QString::fromUtf8(QUrl::toPercentEncoding(endStr)))
                .arg(compressionSeconds);

            auto response = client.get(urlStr.toStdString(), headers);
            if (!response.success || response.statusCode != 200) {
                qWarning() << "[ChartDataFeed] OHLC batch failed:" << response.statusCode;
                break;
            }

            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
            if (doc.isObject()) {
                QJsonObject result = doc.object()["result"].toObject();
                QString dataResponse = result["dataReponse"].toString();
                if (dataResponse.isEmpty())
                    dataResponse = result["dataResponse"].toString();

                if (!dataResponse.isEmpty()) {
                    QStringList barStrings = dataResponse.split(',', Qt::SkipEmptyParts);
                    QJsonArray batchBars;
                    for (const QString &barStr : barStrings) {
                        QStringList fields = barStr.split('|', Qt::SkipEmptyParts);
                        if (fields.size() >= 6) {
                            qint64 ts = fields[0].toLongLong();
                            if (ts <= 946684800) continue; // skip invalid timestamps

                            QJsonObject bar;
                            bar["time"]   = static_cast<double>(ts) * 1000.0;
                            bar["open"]   = fields[1].toDouble();
                            bar["high"]   = fields[2].toDouble();
                            bar["low"]    = fields[3].toDouble();
                            bar["close"]  = fields[4].toDouble();
                            bar["volume"] = fields[5].toDouble();
                            batchBars.append(bar);
                        }
                    }
                    for (int i = batchBars.size() - 1; i >= 0; --i)
                        finalBars.insert(0, batchBars[i]);
                }
            }

            if (finalBars.size() >= MIN_CANDLES) break;
            daysExtended++;
            searchTo   = searchFrom;
            searchFrom -= 86400; // 1 day back
        }

        // Deduplicate and sort by time
        QMap<qint64, QJsonObject> uniqueBars;
        for (const QJsonValue &v : finalBars) {
            QJsonObject bar = v.toObject();
            uniqueBars.insert(static_cast<qint64>(bar["time"].toDouble()), bar);
        }
        QJsonArray sorted;
        for (auto it = uniqueBars.cbegin(); it != uniqueBars.cend(); ++it)
            sorted.append(it.value());

        bool noData = sorted.isEmpty();

        // Marshal back to GUI thread
        QMetaObject::invokeMethod(this, "onOHLCVBarsReady", Qt::QueuedConnection,
            Q_ARG(QString, internalId),
            Q_ARG(QJsonArray, sorted),
            Q_ARG(bool, noData));
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// onOHLCVBarsReady — slot, always on GUI thread (Qt::QueuedConnection)
// ─────────────────────────────────────────────────────────────────────────────
void ChartDataFeed::onOHLCVBarsReady(const QString &internalCbId,
                                      const QJsonArray &bars,
                                      bool noData)
{
    auto it = m_pendingBars.find(internalCbId);
    if (it == m_pendingBars.end()) {
        qDebug() << "[ChartDataFeed] Stale internalId:" << internalCbId;
        return;
    }

    const PendingRequest req = it.value();
    m_pendingBars.erase(it);

    qDebug() << "[ChartDataFeed] Emitting" << bars.size() << "bars for callbackId:" << req.tvCallbackId;

    // Seed history for live candle continuity
    if (!bars.isEmpty()) {
        QJsonObject lastBar = bars.last().toObject();
        qint64 lastBarSec    = static_cast<qint64>(lastBar["time"].toDouble() / 1000.0);
        qint64 lastBarVolume = static_cast<qint64>(lastBar["volume"].toDouble());

        QWriteLocker wl(&m_rwLock);
        QString seedKey = QString("%1|%2|%3").arg(req.segment).arg(req.token).arg(req.resolutionSecs);
        m_historySeeds.insert(seedKey, {
            lastBarSec, lastBarVolume,
            lastBar["open"].toDouble(), lastBar["high"].toDouble(),
            lastBar["low"].toDouble(),  lastBar["close"].toDouble()
        });
    }

    emit barsReceived(req.tvCallbackId, bars, noData);
}

// ─────────────────────────────────────────────────────────────────────────────
// subscribeBars / unsubscribeBars
// ─────────────────────────────────────────────────────────────────────────────
void ChartDataFeed::subscribeBars(const QString &symbolInfoStr,
                                   const QString &resolution,
                                   const QString &subscriberUID)
{
    qDebug() << "[ChartDataFeed] subscribeBars UID:" << subscriberUID << "res:" << resolution;

    int segment = 1;
    int64_t token = 0;

    if (!parseSymbolInfo(symbolInfoStr, segment, token)) {
        QReadLocker rl(&m_rwLock);
        segment = m_currentSegment;
        token   = m_currentToken;
    }

    int resSecs = resolutionToSeconds(resolution);
    bool requiresSubscription = true;

    {
        QWriteLocker wl(&m_rwLock);

        // Check if another subscriber already listens to this token
        for (auto it = m_subscribers.begin(); it != m_subscribers.end(); ++it) {
            if (it.value().segment == segment && it.value().token == static_cast<uint32_t>(token)) {
                requiresSubscription = false;
                break;
            }
        }

        // Seed from last historical bar for seamless transition
        QString seedKey = QString("%1|%2|%3").arg(segment).arg(token).arg(resSecs);
        HistorySeed seed = m_historySeeds.value(seedKey, {0, 0, 0.0, 0.0, 0.0, 0.0});

        SubscriberInfo info;
        info.segment         = segment;
        info.token           = static_cast<uint32_t>(token);
        info.resolutionSecs  = resSecs;
        info.isHistorySeeded = true;
        info.candleBarStartSec = seed.startSec;
        info.candleOpen      = seed.open;
        info.candleHigh      = seed.high;
        info.candleLow       = seed.low;
        info.candleClose     = seed.close;
        info.candleVolume    = seed.volume;
        info.candleBaseVolume = seed.volume;
        info.candleVolumePrev = 0;
        info.isVolumeSeeded  = false;
        info.hasPendingUpdate = false;
        info.completedBars   = QJsonArray();

        m_subscribers.insert(subscriberUID, info);
    }

    if (token != 0 && requiresSubscription) {
        FeedHandler::instance().subscribe(
            segment,
            static_cast<uint32_t>(token),
            this,
            &ChartDataFeed::onUdpTickReceived);
        qDebug() << "[ChartDataFeed] Subscribed FeedHandler for token:" << token;
    }
}

void ChartDataFeed::unsubscribeBars(const QString &subscriberUID)
{
    qDebug() << "[ChartDataFeed] unsubscribeBars UID:" << subscriberUID;

    int segment = 0;
    uint32_t token = 0;
    bool tokenStillUsed = false;

    {
        QWriteLocker wl(&m_rwLock);
        if (m_subscribers.contains(subscriberUID)) {
            segment = m_subscribers[subscriberUID].segment;
            token   = m_subscribers[subscriberUID].token;
            m_subscribers.remove(subscriberUID);

            for (auto it = m_subscribers.begin(); it != m_subscribers.end(); ++it) {
                if (it.value().segment == segment && it.value().token == token) {
                    tokenStillUsed = true;
                    break;
                }
            }
        }
    }

    if (token != 0 && !tokenStillUsed) {
        FeedHandler::instance().unsubscribe(segment, token, this);
        qDebug() << "[ChartDataFeed] Unsubscribed FeedHandler for token:" << token;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// onUdpTickReceived — instant live tick from FeedHandler
// ─────────────────────────────────────────────────────────────────────────────
void ChartDataFeed::onUdpTickReceived(const UDP::MarketTick &tick)
{
    if (tick.ltp <= 0) return;

    const double   ltp       = tick.ltp;
    const qint64   cumVolume = static_cast<qint64>(tick.volume);
    const qint64   nowSec    = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    const uint32_t token     = tick.token;
    const int      segment   = static_cast<int>(tick.exchangeSegment);

    QWriteLocker wl(&m_rwLock);

    for (auto it = m_subscribers.begin(); it != m_subscribers.end(); ++it) {
        SubscriberInfo &info = it.value();

        if (info.token != token || info.segment != segment) continue;
        if (!info.isHistorySeeded) continue;

        const qint64 periodSec = static_cast<qint64>(info.resolutionSecs);

        // ── IST-aligned candle boundary calculation ──
        constexpr qint64 IST_OFFSET_SECS = 5 * 3600 + 30 * 60;
        qint64 shiftedNow = nowSec + IST_OFFSET_SECS;
        qint64 currentBoundarySec = (shiftedNow / periodSec) * periodSec - IST_OFFSET_SECS;

        if (info.candleBarStartSec == 0) {
            // First tick ever (no history seed)
            info.candleBarStartSec = currentBoundarySec;
            info.candleVolumePrev  = cumVolume;
            info.candleBaseVolume  = 0;
            info.isVolumeSeeded    = true;
        } else if (currentBoundarySec > info.candleBarStartSec) {
            // Candle roll-over: emit completed bar
            if (info.candleOpen > 0.0) {
                QJsonObject completedBar;
                completedBar["time"]   = static_cast<double>(info.candleBarStartSec) * 1000.0;
                completedBar["open"]   = info.candleOpen;
                completedBar["high"]   = info.candleHigh;
                completedBar["low"]    = info.candleLow;
                completedBar["close"]  = info.candleClose;
                completedBar["volume"] = static_cast<double>(info.candleVolume);

                info.completedBars.append(completedBar);
            }

            // Reset for new candle
            info.candleBarStartSec = currentBoundarySec;
            info.candleVolumePrev  = cumVolume;
            info.candleBaseVolume  = 0;
            info.isVolumeSeeded    = true;
            info.candleOpen   = 0.0;
            info.candleHigh   = 0.0;
            info.candleLow    = 0.0;
            info.candleClose  = 0.0;
            info.candleVolume = 0;
            info.hasPendingUpdate = false;
        }

        if (!info.isVolumeSeeded) {
            info.candleVolumePrev = cumVolume;
            info.isVolumeSeeded   = true;
        }

        // ── Update OHLC ──
        if (info.candleOpen == 0.0) {
            info.candleOpen = ltp;
            info.candleHigh = ltp;
            info.candleLow  = ltp;
        } else {
            if (ltp > info.candleHigh) info.candleHigh = ltp;
            if (ltp < info.candleLow)  info.candleLow  = ltp;
        }
        info.candleClose = ltp;

        // ── Volume delta computation ──
        qint64 delta = cumVolume - info.candleVolumePrev;
        if (delta < 0) delta = 0;
        info.candleVolume = info.candleBaseVolume + delta;

        info.hasPendingUpdate = true;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// onUpdateTimer — 50ms batched emission to prevent IPC overload
// ─────────────────────────────────────────────────────────────────────────────
void ChartDataFeed::onUpdateTimer()
{
    struct PendingEmit { QString uid; QJsonObject bar; };
    QVector<PendingEmit> pending;

    {
        QWriteLocker wl(&m_rwLock);
        for (auto it = m_subscribers.begin(); it != m_subscribers.end(); ++it) {
            SubscriberInfo &info = it.value();

            // Emit completed bars first
            for (const QJsonValue &v : info.completedBars)
                pending.append({it.key(), v.toObject()});
            info.completedBars = QJsonArray();

            // Emit live candle update
            if (info.hasPendingUpdate) {
                QJsonObject liveBar;
                liveBar["time"]   = static_cast<double>(info.candleBarStartSec) * 1000.0;
                liveBar["open"]   = info.candleOpen;
                liveBar["high"]   = info.candleHigh;
                liveBar["low"]    = info.candleLow;
                liveBar["close"]  = info.candleClose;
                liveBar["volume"] = static_cast<double>(info.candleVolume);

                pending.append({it.key(), liveBar});
                info.hasPendingUpdate = false;
            }
        }
    }

    for (const PendingEmit &pe : pending)
        emit realtimeUpdate(pe.uid, pe.bar);
}

// ─────────────────────────────────────────────────────────────────────────────
// parseSymbolInfo
// ─────────────────────────────────────────────────────────────────────────────
bool ChartDataFeed::parseSymbolInfo(const QString &symbolInfoStr,
                                     int &segment, int64_t &token) const
{
    QString ticker = symbolInfoStr;

    // TradingView may pass a full JSON string
    QJsonDocument doc = QJsonDocument::fromJson(symbolInfoStr.toUtf8());
    if (!doc.isNull() && doc.isObject()) {
        QJsonObject obj = doc.object();
        ticker = obj.value("ticker").toString();
        if (ticker.isEmpty())
            ticker = obj.value("name").toString();
    }

    // Match against current symbol
    {
        QReadLocker rl(&m_rwLock);
        if (ticker == m_currentSymbol && m_currentToken != 0) {
            segment = m_currentSegment;
            token   = m_currentToken;
            return true;
        }
    }

    // Try "segment_token" format (e.g. "1_2885")
    if (ticker.contains('_')) {
        QStringList parts = ticker.split('_');
        if (parts.size() == 2) {
            bool okSeg, okTok;
            int parsedSeg    = parts[0].toInt(&okSeg);
            int64_t parsedTok = parts[1].toLongLong(&okTok);
            if (okSeg && okTok && parsedTok > 0) {
                segment = parsedSeg;
                token   = parsedTok;
                return true;
            }
        }
    }

    // Try "EXCHANGE:SYMBOL" or just "SYMBOL" via RepositoryManager
    QString symbolName = ticker;
    QString exchange   = "NSE";
    if (ticker.contains(':')) {
        QStringList parts = ticker.split(':');
        if (parts.size() == 2) {
            exchange   = parts[0];
            symbolName = parts[1];
        }
    }

    if (RepositoryManager *repo = RepositoryManager::getInstance()) {
        int64_t foundToken = repo->getAssetTokenForSymbol(symbolName);
        if (foundToken > 0) {
            segment = (exchange == "BSE") ? 11 : 1;
            token   = foundToken;
            return true;
        }
    }

    return false;
}

void ChartDataFeed::resetCandleAccumulator(const QString &uid, qint64 barStartSec)
{
    QWriteLocker wl(&m_rwLock);
    if (!m_subscribers.contains(uid)) return;

    SubscriberInfo &info = m_subscribers[uid];
    info.candleBarStartSec = barStartSec;
    info.candleOpen        = 0.0;
    info.candleHigh        = 0.0;
    info.candleLow         = 0.0;
    info.candleClose       = 0.0;
    info.candleVolume      = 0;
    info.candleBaseVolume  = 0;
    info.candleVolumePrev  = 0;
    info.isVolumeSeeded    = false;
    info.hasPendingUpdate  = false;
    info.completedBars     = QJsonArray();
}

// ─────────────────────────────────────────────────────────────────────────────
// Static helpers
// ─────────────────────────────────────────────────────────────────────────────
QString ChartDataFeed::resolutionToCompression(const QString &resolution)
{
    if (resolution == "1")   return "60";
    if (resolution == "5")   return "300";
    if (resolution == "15")  return "900";
    if (resolution == "30")  return "1800";
    if (resolution == "60")  return "3600";
    if (resolution == "1D")  return "86400";
    bool ok;
    int mins = resolution.toInt(&ok);
    return ok ? QString::number(mins * 60) : "60";
}

int ChartDataFeed::resolutionToSeconds(const QString &resolution)
{
    if (resolution == "1")   return 60;
    if (resolution == "5")   return 300;
    if (resolution == "15")  return 900;
    if (resolution == "30")  return 1800;
    if (resolution == "60")  return 3600;
    if (resolution == "1D")  return 86400;
    bool ok;
    int mins = resolution.toInt(&ok);
    return ok ? mins * 60 : 60;
}

QString ChartDataFeed::utcSecsToISTString(long long utcSecs)
{
    constexpr qint64 IST_OFFSET_SECS = 5 * 3600 + 30 * 60;
    qint64 istSecs = static_cast<qint64>(utcSecs) + IST_OFFSET_SECS;
    QDateTime dt = QDateTime::fromSecsSinceEpoch(istSecs, Qt::UTC);
    return dt.toString("MMM dd yyyy HHmmss");
}

void ChartDataFeed::notifyVisibleRangeChanged(double fromMs, double toMs)
{
    emit visibleRangeChanged(fromMs, toMs);
}

void ChartDataFeed::placeOrderRequest(const QJsonObject &order)
{
    qDebug() << "[ChartDataFeed] Order request from chart:" << order;
    emit orderFromChartRequested(order);
}
