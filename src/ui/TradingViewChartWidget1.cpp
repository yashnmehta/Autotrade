#include "ui/TradingViewChartWidget.h"
#include "services/HistoricalDataStore.h"
#include "services/CandleAggregator.h"
#include "api/NativeHTTPClient.h"
#include "api/XTSMarketDataClient.h"
#include "repository/RepositoryManager.h"
#include "repository/ContractData.h"
#include <QVBoxLayout>
#include <QWebEngineSettings>
#include <QWebEnginePage>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QCoreApplication>
#include <QDateTime>
#include <QUrl>
#include <QUrlQuery>
#include <QtConcurrent/QtConcurrent>
#include <QDebug>

TradingViewChartWidget::TradingViewChartWidget(QWidget* parent)
    : QWidget(parent)
{
    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Create web view
    m_webView = new QWebEngineView(this);
    layout->addWidget(m_webView);
    
    // Enable developer tools (comment out in production)
    m_webView->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    m_webView->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    m_webView->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    
    // Create data bridge
    m_dataBridge = new TradingViewDataBridge(this);
    
    // Setup web channel
    setupWebChannel();
    
    // Connect signals
    connect(m_webView, &QWebEngineView::loadFinished,
            this, &TradingViewChartWidget::onLoadFinished);
    
    connect(m_dataBridge, &TradingViewDataBridge::chartReady,
            this, [this]() {
                m_chartReady = true;
                emit chartReady();
                qDebug() << "[TradingViewChart] Chart ready";
            });
    
    connect(m_dataBridge, &TradingViewDataBridge::chartClicked,
            this, &TradingViewChartWidget::chartClicked);
    
    connect(m_dataBridge, &TradingViewDataBridge::orderRequested,
            this, &TradingViewChartWidget::orderRequested);
    
    connect(m_dataBridge, &TradingViewDataBridge::historicalDataRequested,
            this, [this](const QString& symbol, int segment, const QString& resolution,
                        qint64 from, qint64 to) {
                qDebug() << "[TradingViewChart] Fetching OHLC data for" << symbol << "segment" << segment
                         << "resolution" << resolution << "from" << from << "to" << to;
                
                // Convert resolution to compressionValue (minutes)
                int compressionValue = 1;
                if (resolution == "1") compressionValue = 1;
                else if (resolution == "5") compressionValue = 5;
                else if (resolution == "15") compressionValue = 15;
                else if (resolution == "30") compressionValue = 30;
                else if (resolution == "60") compressionValue = 60;
                else if (resolution == "240") compressionValue = 240;
                else if (resolution == "D") compressionValue = 1440; // Daily
                else compressionValue = 5; // Default to 5min
                
                // Use the current token set via loadSymbol()
                int exchangeInstrumentID = m_currentToken;
                
                if (exchangeInstrumentID == 0) {
                    qWarning() << "[TradingViewChart] No token set for symbol" << symbol;
                    QMetaObject::invokeMethod(m_dataBridge, "sendHistoricalData",
                                             Qt::QueuedConnection,
                                             Q_ARG(QJsonArray, QJsonArray()));
                    return;
                }
                
                // Format timestamps for API ("MMM dd yyyy HHmmss")
                QDateTime fromDt = QDateTime::fromMSecsSinceEpoch(from, Qt::UTC);
                QDateTime toDt = QDateTime::fromMSecsSinceEpoch(to, Qt::UTC);
                QString startTime = fromDt.toString("MMM dd yyyy HHmmss");
                QString endTime = toDt.toString("MMM dd yyyy HHmmss");
                
                // Build API URL
                QString urlStr = QString(
                    "https://mtrade.arhamshare.com/apimarketdata/instruments/ohlc"
                    "?exchangeSegment=%1&exchangeInstrumentID=%2"
                    "&startTime=%3&endTime=%4&compressionValue=%5"
                ).arg(segment).arg(exchangeInstrumentID)
                 .arg(QUrl::toPercentEncoding(startTime).constData())
                 .arg(QUrl::toPercentEncoding(endTime).constData())
                 .arg(compressionValue);
                
                qDebug() << "[TradingViewChart] API URL:" << urlStr;
                
                // Get auth token from XTS client
                QString authToken;
                if (m_xtsClient) {
                    authToken = m_xtsClient->token();
                }
                
                // Make HTTP request in a separate thread to avoid blocking UI
                QtConcurrent::run([this, urlStr, authToken]() {
                    NativeHTTPClient client;
                    client.setTimeout(10);
                    
                    // Add authorization header if token available
                    std::map<std::string, std::string> headers;
                    if (!authToken.isEmpty()) {
                        headers["Authorization"] = authToken.toStdString();
                        qDebug() << "[TradingViewChart] Using auth token:" << authToken.left(20) + "...";
                    }
                    
                    auto response = client.get(urlStr.toStdString(), headers);
                    
                    QJsonArray bars;
                    if (response.success && response.statusCode == 200) {
                        // Parse JSON response
                        QJsonDocument doc = QJsonDocument::fromJson(
                            QByteArray::fromStdString(response.body)
                        );
                        
                        if (doc.isObject()) {
                            QJsonObject root = doc.object();
                            QJsonArray dataPoints = root["data"].toArray();
                            
                            for (const QJsonValue& val : dataPoints) {
                                QJsonObject dp = val.toObject();
                                QJsonObject bar;
                                
                                // Parse timestamp (format may vary)
                                QString timestamp = dp["timestamp"].toString();
                                qint64 timeMs = QDateTime::fromString(
                                    timestamp, Qt::ISODate
                                ).toMSecsSinceEpoch();
                                
                                bar["time"] = timeMs;
                                bar["open"] = dp["open"].toDouble();
                                bar["high"] = dp["high"].toDouble();
                                bar["low"] = dp["low"].toDouble();
                                bar["close"] = dp["close"].toDouble();
                                bar["volume"] = dp["volume"].toDouble(0.0);
                                
                                bars.append(bar);
                            }
                            
                            qDebug() << "[TradingViewChart] Parsed" << bars.size() << "bars from API";
                        }
                    } else {
                        qWarning() << "[TradingViewChart] API request failed:"
                                   << QString::fromStdString(response.error);
                    }
                    
                    // Send data back to JavaScript on main thread
                    QMetaObject::invokeMethod(m_dataBridge, "sendHistoricalData",
                                             Qt::QueuedConnection,
                                             Q_ARG(QJsonArray, bars));
                });
            });
    
    // Connect to CandleAggregator for real-time updates
    connect(&CandleAggregator::instance(), &CandleAggregator::candleComplete,
            this, &TradingViewChartWidget::onCandleComplete);
    
    connect(&CandleAggregator::instance(), &CandleAggregator::candleUpdate,
            this, &TradingViewChartWidget::onCandleUpdate);
    
    // Load chart HTML
    loadChartHTML();
    
    qDebug() << "[TradingViewChart] Widget created";
}

TradingViewChartWidget::~TradingViewChartWidget()
{
}

void TradingViewChartWidget::setupWebChannel()
{
    m_channel = new QWebChannel(this);
    m_channel->registerObject("dataBridge", m_dataBridge);
    m_webView->page()->setWebChannel(m_channel);
    
    qDebug() << "[TradingViewChart] Web channel setup complete";
}

void TradingViewChartWidget::loadChartHTML()
{
    // Load from resources
    QFile file(":/html/tradingview_chart.html");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString html = QString::fromUtf8(file.readAll());
        
        // Set base URL to the resources/tradingview directory so that relative paths work
        // This allows the HTML to load charting_library/charting_library.standalone.js
        QString appPath = QCoreApplication::applicationDirPath();
        QUrl baseUrl = QUrl::fromLocalFile(appPath + "/resources/tradingview/");
        
        m_webView->setHtml(html, baseUrl);
        qDebug() << "[TradingViewChart] Loading HTML from resources with base URL:" << baseUrl.toString();
    } else {
        qWarning() << "[TradingViewChart] Failed to load chart HTML from resources";
        
        // Fallback: load minimal HTML
        QString fallbackHtml = R"(
<!DOCTYPE html>
<html>
<head>
    <title>TradingView Chart</title>
    <meta charset="utf-8">
    <script src="qrc:///qtwebchannel/qwebchannel.js"></script>
    <style>
        body { margin: 0; padding: 0; background: #131722; color: white; }
        #chart_container { width: 100%; height: 100vh; }
        #error { padding: 20px; text-align: center; }
    </style>
</head>
<body>
    <div id="error">
        <h2>TradingView Chart</h2>
        <p>Chart initialization in progress...</p>
        <p><small>Library files need to be configured in resources.</small></p>
    </div>
    <script>
        new QWebChannel(qt.webChannelTransport, function(channel) {
            window.dataBridge = channel.objects.dataBridge;
            console.log("Data bridge connected");
            window.dataBridge.onChartReady();
        });
    </script>
</body>
</html>
)";
        m_webView->setHtml(fallbackHtml);
    }
}

void TradingViewChartWidget::loadSymbol(const QString& symbol, int segment,
                                       int64_t token, const QString& interval)
{
    m_currentSymbol = symbol;
    m_currentSegment = segment;
    m_currentToken = token;
    m_currentInterval = interval;
    
    if (!m_chartReady) {
        qWarning() << "[TradingViewChart] Chart not ready yet";
        return;
    }
    
    // Subscribe to real-time candle updates
    QString timeframe = interval;
    if (interval == "1") timeframe = "1m";
    else if (interval == "5") timeframe = "5m";
    else if (interval == "15") timeframe = "15m";
    else if (interval == "30") timeframe = "30m";
    else if (interval == "60") timeframe = "1h";
    else if (interval == "240") timeframe = "4h";
    else if (interval == "D") timeframe = "1d";
    
    CandleAggregator::instance().subscribeTo(symbol, segment, {timeframe});
    qDebug() << "[TradingViewChart] Subscribed to candles:" << symbol << segment << timeframe;
    
    QString script = QString(
        "if (window.widget) {"
        "  window.widget.setSymbol('%1', '%2', function() {"
        "    console.log('Symbol changed to %1');"
        "  });"
        "}"
    ).arg(symbol, interval);
    
    executeScript(script);
    
    qDebug() << "[TradingViewChart] Loading symbol:" << symbol << "interval:" << interval;
}

void TradingViewChartWidget::setInterval(const QString& interval)
{
    m_currentInterval = interval;
    
    if (!m_chartReady) {
        return;
    }
    
    QString script = QString(
        "if (window.widget) {"
        "  window.widget.setResolution('%1');"
        "}"
    ).arg(interval);
    
    executeScript(script);
}

void TradingViewChartWidget::setTheme(const QString& theme)
{
    if (!m_chartReady) {
        return;
    }
    
    QString script = QString(
        "if (window.widget) {"
        "  window.widget.changeTheme('%1');"
        "}"
    ).arg(theme.toLower());
    
    executeScript(script);
}

void TradingViewChartWidget::addIndicator(const QString& indicatorName)
{
    if (!m_chartReady) {
        return;
    }
    
    QString script = QString(
        "if (window.widget) {"
        "  window.widget.activeChart().createStudy('%1');"
        "}"
    ).arg(indicatorName);
    
    executeScript(script);
}

void TradingViewChartWidget::addOrderMarker(qint64 time, double price,
                                           const QString& text,
                                           const QString& color,
                                           const QString& shape)
{
    if (!m_chartReady) {
        return;
    }
    
    QJsonObject marker;
    marker["time"] = time * 1000; // TradingView expects milliseconds
    marker["position"] = (shape.contains("up") || shape.contains("buy")) ? "belowBar" : "aboveBar";
    marker["color"] = color;
    marker["shape"] = shape;
    marker["text"] = text;
    
    QString markerJson = QString::fromUtf8(QJsonDocument(marker).toJson(QJsonDocument::Compact));
    
    QString script = QString(
        "if (window.widget) {"
        "  window.widget.activeChart().createShape("
        "    {time: %1, price: %2},"
        "    {shape: '%3', text: '%4', fillColor: '%5'}"
        "  );"
        "}"
    ).arg(time * 1000).arg(price).arg(shape, text, color);
    
    executeScript(script);
}

void TradingViewChartWidget::executeScript(const QString& script)
{
    m_webView->page()->runJavaScript(script, [](const QVariant& result) {
        if (!result.isNull()) {
            qDebug() << "[TradingViewChart] Script result:" << result;
        }
    });
}

void TradingViewChartWidget::onCandleComplete(const QString& symbol, int segment,
                                             const QString& timeframe,
                                             const ChartData::Candle& candle)
{
    // Only update if this is the current symbol
    if (symbol != m_currentSymbol || segment != m_currentSegment) {
        return;
    }
    
    // Convert timeframe to interval
    QString interval = convertTimeframeToInterval(timeframe);
    if (interval != m_currentInterval) {
        return;
    }
    
    // Send to chart
    QJsonObject bar;
    bar["time"] = candle.timestamp * 1000;
    bar["open"] = candle.open;
    bar["high"] = candle.high;
    bar["low"] = candle.low;
    bar["close"] = candle.close;
    bar["volume"] = static_cast<double>(candle.volume);
    
    m_dataBridge->sendRealtimeBar(bar);
}

void TradingViewChartWidget::onCandleUpdate(const QString& symbol, int segment,
                                           const QString& timeframe,
                                           const ChartData::Candle& candle)
{
    // Only update if this is the current symbol/timeframe
    if (symbol != m_currentSymbol || segment != m_currentSegment) {
        return;
    }
    
    QString interval = convertTimeframeToInterval(timeframe);
    if (interval != m_currentInterval) {
        return;
    }
    
    // Send partial update
    QJsonObject bar;
    bar["time"] = candle.timestamp * 1000;
    bar["open"] = candle.open;
    bar["high"] = candle.high;
    bar["low"] = candle.low;
    bar["close"] = candle.close;
    bar["volume"] = static_cast<double>(candle.volume);
    
    m_dataBridge->sendRealtimeBar(bar);
}

void TradingViewChartWidget::onLoadFinished(bool success)
{
    if (success) {
        qDebug() << "[TradingViewChart] Page loaded successfully";
    } else {
        qWarning() << "[TradingViewChart] Page load failed";
    }
}

void TradingViewChartWidget::onJavaScriptMessage(const QString& message)
{
    // Handle messages from JavaScript
    qDebug() << "[TradingViewChart] JS Message:" << message;
    
    // Parse JSON messages if needed
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isNull() && doc.isObject()) {
        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();
        
        if (type == "error") {
            qWarning() << "[TradingViewChart] JS Error:" << obj["message"].toString();
        } else if (type == "log") {
            qDebug() << "[TradingViewChart] JS Log:" << obj["message"].toString();
        }
    }
}

QString TradingViewChartWidget::convertTimeframeToInterval(const QString& timeframe) const
{
    if (timeframe == "1m") return "1";
    if (timeframe == "5m") return "5";
    if (timeframe == "15m") return "15";
    if (timeframe == "30m") return "30";
    if (timeframe == "1h") return "60";
    if (timeframe == "4h") return "240";
    if (timeframe == "1D") return "D";
    if (timeframe == "1W") return "W";
    return "5"; // default
}

// ============================================================================
// TradingViewDataBridge Implementation
// ============================================================================

TradingViewDataBridge::TradingViewDataBridge(QObject* parent)
    : QObject(parent)
{
}

void TradingViewDataBridge::onChartReady()
{
    qDebug() << "[TradingViewDataBridge] Chart ready signal received";
    emit chartReady();
}

void TradingViewDataBridge::onChartClick(qint64 time, double price)
{
    qDebug() << "[TradingViewDataBridge] Chart clicked at" << time << price;
    emit chartClicked(time / 1000, price); // Convert from milliseconds
}

void TradingViewDataBridge::onOrderRequest(const QString& side, double price)
{
    qDebug() << "[TradingViewDataBridge] Order requested:" << side << "@" << price;
    emit orderRequested(side, price);
}

void TradingViewDataBridge::requestHistoricalData(const QString& symbol, int segment,
                                                  const QString& resolution,
                                                  qint64 from, qint64 to)
{
    qDebug() << "[TradingViewDataBridge] Historical data requested:" 
             << symbol << resolution << "from" << from << "to" << to;
    emit historicalDataRequested(symbol, segment, resolution, from / 1000, to / 1000);
}

void TradingViewDataBridge::sendHistoricalData(const QJsonArray& bars)
{
    qDebug() << "[TradingViewDataBridge] Sending" << bars.size() << "historical bars";
    emit historicalDataReady(bars);
}

void TradingViewDataBridge::sendRealtimeBar(const QJsonObject& bar)
{
    emit realtimeBarUpdate(bar);
}

void TradingViewDataBridge::sendError(const QString& error)
{
    qWarning() << "[TradingViewDataBridge] Error:" << error;
    emit errorOccurred(error);
}

void TradingViewDataBridge::searchSymbols(const QString& searchText,
                                          const QString& exchange,
                                          const QString& segment)
{
    qDebug() << "[TradingViewDataBridge] Symbol search:" << searchText
             << "exchange:" << exchange << "segment:" << segment;
    
    // Get parent widget to access RepositoryManager
    TradingViewChartWidget* widget = qobject_cast<TradingViewChartWidget*>(parent());
    if (!widget || !widget->m_repoManager) {
        qWarning() << "[TradingViewDataBridge] No RepositoryManager available";
        emit symbolSearchResults(QJsonArray());
        return;
    }
    
    // Search in repository (empty series = search all)
    QVector<ContractData> results = widget->m_repoManager->searchScrips(
        exchange, segment, "", searchText, 20  // Max 20 results
    );
    
    // Convert to JSON format for TradingView
    QJsonArray jsonResults;
    for (const ContractData& contract : results) {
        QJsonObject item;
        item["symbol"] = contract.symbol;
        item["full_name"] = QString("%1 - %2").arg(contract.symbol, contract.description);
        item["description"] = contract.description;
        item["exchange"] = contract.exchange;
        item["type"] = contract.instrumentType;
        item["token"] = static_cast<qint64>(contract.exchangeInstrumentID);
        item["segment"] = contract.exchangeSegment;
        
        jsonResults.append(item);
    }
    
    qDebug() << "[TradingViewDataBridge] Found" << jsonResults.size() << "matches";
    emit symbolSearchResults(jsonResults);
}
