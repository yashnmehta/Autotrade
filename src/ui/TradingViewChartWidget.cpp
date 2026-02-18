#include "ui/TradingViewChartWidget.h"
#include "api/NativeHTTPClient.h"
#include "api/XTSMarketDataClient.h"
#include "api/XTSTypes.h"
#include "models/WindowContext.h"
#include "repository/ContractData.h"
#include "repository/RepositoryManager.h"
#include "services/CandleAggregator.h"
#include "services/HistoricalDataStore.h"
#include "views/BuyWindow.h"
#include "views/SellWindow.h"
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QtConcurrent/QtConcurrent>

TradingViewChartWidget::TradingViewChartWidget(QWidget *parent)
    : QWidget(parent) {
  // Create layout
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  // Create web view
  m_webView = new QWebEngineView(this);
  layout->addWidget(m_webView);

  // Enable developer tools (comment out in production)
  m_webView->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,
                                      true);
  m_webView->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled,
                                      true);
  m_webView->settings()->setAttribute(
      QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);

  // Create data bridge
  m_dataBridge = new TradingViewDataBridge(this);

  // Setup web channel
  setupWebChannel();

  // Connect signals
  connect(m_webView, &QWebEngineView::loadFinished, this,
          &TradingViewChartWidget::onLoadFinished);

  connect(m_dataBridge, &TradingViewDataBridge::chartReady, this, [this]() {
    m_chartReady = true;
    emit chartReady();
    qDebug() << "[TradingViewChart] Chart ready";
  });

  connect(m_dataBridge, &TradingViewDataBridge::chartClicked, this,
          &TradingViewChartWidget::onChartClickedInternal);

  connect(m_dataBridge, &TradingViewDataBridge::orderRequested, this,
          &TradingViewChartWidget::orderRequested);

  connect(
      m_dataBridge, &TradingViewDataBridge::historicalDataRequested, this,
      [this](const QString &symbol, int segment, const QString &resolution,
             qint64 from, qint64 to, qint64 token, int requestId) {
        qWarning() << "[TradingViewChart] Fetching OHLC data for" << symbol
                   << "segment" << segment << "resolution" << resolution
                   << "from" << from << "to" << to << "token" << token
                   << "requestId" << requestId;

        // Convert resolution to compressionValue (minutes)
        int compressionValue = 1;
        if (resolution == "1")
          compressionValue = 1;
        else if (resolution == "5")
          compressionValue = 5;
        else if (resolution == "15")
          compressionValue = 15;
        else if (resolution == "30")
          compressionValue = 30;
        else if (resolution == "60")
          compressionValue = 60;
        else if (resolution == "240")
          compressionValue = 240;
        else if (resolution == "D")
          compressionValue = 1440; // Daily
        else
          compressionValue = 5; // Default to 5min

        // Use the token passed as parameter (fixes race condition)
        int exchangeInstrumentID = token;

        if (exchangeInstrumentID == 0) {
          qWarning() << "[TradingViewChart] No token provided for symbol"
                     << symbol;
          QMetaObject::invokeMethod(
              m_dataBridge, "sendHistoricalData", Qt::QueuedConnection,
              Q_ARG(QJsonArray, QJsonArray()), Q_ARG(int, requestId));
          return;
        }

        // Format timestamps for API ("MMM dd yyyy HHmmss")
        // NOTE: We use qint64 from/to as SECONDS since they were divided by
        // 1000 in requestHistoricalData
        QDateTime fromDt = QDateTime::fromSecsSinceEpoch(from, Qt::LocalTime);
        QDateTime toDt = QDateTime::fromSecsSinceEpoch(to, Qt::LocalTime);

        // Ensure toDt is not later than "now" to avoid future data requests
        QDateTime now = QDateTime::currentDateTime();
        if (toDt > now)
          toDt = now;

        QString startTime = fromDt.toString("MMM dd yyyy HHmmss");
        QString endTime = toDt.toString("MMM dd yyyy HHmmss");

        int compressionSeconds = compressionValue * 60;
        QString baseUrl = "http://192.168.102.9:3000/";
        QString urlStr = baseUrl + "apimarketdata/instruments/ohlc";
        QString urlStrfull =
            urlStr + QString("?exchangeSegment=%1&exchangeInstrumentID=%2"
                             "&startTime=%3&endTime=%4&compressionValue=%5")
                         .arg(segment)
                         .arg(exchangeInstrumentID)
                         .arg(QUrl::toPercentEncoding(startTime).constData())
                         .arg(QUrl::toPercentEncoding(endTime).constData())
                         .arg(compressionSeconds);

        qWarning() << "[TradingViewChart] OHLC Request [" << startTime
                   << "] to [" << endTime << "] | Res:" << resolution;

        // Get auth token from XTS client
        QString authToken;
        if (m_xtsClient) {
          authToken = m_xtsClient->token();
          qWarning() << "[TradingViewChart] Auth token available:"
                     << !authToken.isEmpty() << "Length:" << authToken.length();
        } else {
          qWarning() << "[TradingViewChart] XTS client not available!";
        }

        // Make HTTP request in a separate thread to avoid blocking UI
        QtConcurrent::run([this, authToken, requestId, from, to,
                           exchangeInstrumentID, segment, compressionSeconds,
                           resolution]() {
          qDebug()
              << "[TradingViewChart] Starting iterative OHLC fetch for request"
              << requestId;

          QJsonArray finalBars;
          qint64 searchTo = to;
          qint64 searchFrom = from;
          int daysExtended = 0;
          const int MIN_CANDLES = 150;
          const int MAX_EXTENSION_DAYS = 7;

          NativeHTTPClient client;
          client.setTimeout(30);

          std::map<std::string, std::string> headers;
          headers["Content-Type"] = "application/json";
          if (!authToken.isEmpty()) {
            headers["authorization"] = authToken.toStdString();
          }

          while (finalBars.size() < MIN_CANDLES &&
                 daysExtended <= MAX_EXTENSION_DAYS) {
            QDateTime fromDt =
                QDateTime::fromSecsSinceEpoch(searchFrom, Qt::LocalTime);
            QDateTime toDt =
                QDateTime::fromSecsSinceEpoch(searchTo, Qt::LocalTime);
            QString startTime = fromDt.toString("MMM dd yyyy HHmmss");
            QString endTime = toDt.toString("MMM dd yyyy HHmmss");

            QString urlStr =
                QString(
                    "http://192.168.102.9:3000/apimarketdata/instruments/ohlc"
                    "?exchangeSegment=%1&exchangeInstrumentID=%2"
                    "&startTime=%3&endTime=%4&compressionValue=%5")
                    .arg(segment)
                    .arg(exchangeInstrumentID)
                    .arg(QUrl::toPercentEncoding(startTime).constData())
                    .arg(QUrl::toPercentEncoding(endTime).constData())
                    .arg(compressionSeconds);

            qWarning() << "[TradingViewChart] Fetching batch:" << startTime
                       << "to" << endTime
                       << "(Current bars:" << finalBars.size() << ")";

            auto response = client.get(urlStr.toStdString(), headers);

            if (response.success && response.statusCode == 200) {
              QJsonDocument doc = QJsonDocument::fromJson(
                  QByteArray::fromStdString(response.body));
              if (doc.isObject()) {
                QJsonObject root = doc.object();
                QJsonObject result = root["result"].toObject();
                QString dataResponse = result["dataReponse"].toString();
                if (dataResponse.isEmpty())
                  dataResponse = result["dataResponse"].toString();

                if (!dataResponse.isEmpty()) {
                  QStringList barStrings =
                      dataResponse.split(',', Qt::SkipEmptyParts);
                  QJsonArray batchBars;
                  for (const QString &barStr : barStrings) {
                    QStringList fields = barStr.split('|', Qt::SkipEmptyParts);
                    if (fields.size() >= 6) {
                      qint64 timestamp = fields[0].toLongLong();
                      
                      // CRITICAL: Skip bars with invalid timestamps
                      // Prevents "time order violation" errors in TradingView
                      if (timestamp == 0 || timestamp < 946684800) {
                        qWarning() << "[TradingViewChart] Skipping bar with invalid timestamp:"
                                   << timestamp << "Raw:" << fields[0];
                        continue;
                      }
                      
                      QJsonObject bar;
                      bar["time"] = timestamp * 1000; // Convert to milliseconds
                      bar["open"] = fields[1].toDouble();
                      bar["high"] = fields[2].toDouble();
                      bar["low"] = fields[3].toDouble();
                      bar["close"] = fields[4].toDouble();
                      bar["volume"] = fields[5].toDouble();
                      batchBars.append(bar);
                    }
                  }

                  // Prepend new bars (older data) to the front
                  for (int i = batchBars.size() - 1; i >= 0; --i) {
                    finalBars.insert(0, batchBars[i]);
                  }
                }
              }
            } else {
              qWarning() << "[TradingViewChart] Batch fetch failed:"
                         << response.statusCode;
              break; // Stop if API errors out
            }

            if (finalBars.size() >= MIN_CANDLES)
              break;

            // Extend back by another day
            daysExtended++;
            searchTo = searchFrom;
            searchFrom -= (24 * 60 * 60); // 1 day back
          }

          // Remove duplicates and sort by time
          QMap<qint64, QJsonObject> uniqueBars;
          for (const QJsonValue &value : finalBars) {
            QJsonObject bar = value.toObject();
            uniqueBars.insert(bar["time"].toVariant().toLongLong(), bar);
          }

          finalBars = QJsonArray(); // Clear and rebuild
          QList<qint64> sortedTimes = uniqueBars.keys();
          std::sort(sortedTimes.begin(), sortedTimes.end());

          for (qint64 time : sortedTimes) {
            finalBars.append(uniqueBars.value(time));
          }

          qDebug() << "[TradingViewChart] Sending final result:"
                   << finalBars.size() << "bars";
          QMetaObject::invokeMethod(
              m_dataBridge, "sendHistoricalData", Qt::QueuedConnection,
              Q_ARG(QJsonArray, finalBars), Q_ARG(int, requestId));
        });
      });

  // Connect to CandleAggregator for real-time updates
  connect(&CandleAggregator::instance(), &CandleAggregator::candleComplete,
          this, &TradingViewChartWidget::onCandleComplete);

  connect(&CandleAggregator::instance(), &CandleAggregator::candleUpdate, this,
          &TradingViewChartWidget::onCandleUpdate);

  // Connect to XTS Market Data Client for real-time ticks (will be set later)
  // Connection happens after setXTSMarketDataClient() is called

  // Load chart HTML
  loadChartHTML();

  qDebug() << "[TradingViewChart] Widget created";
}

TradingViewChartWidget::~TradingViewChartWidget() {
  // Unsubscribe from current token if still subscribed
  if (m_xtsClient && m_xtsClient->isLoggedIn() && m_currentToken > 0) {
    m_xtsClient->unsubscribe(QVector<int64_t>{m_currentToken},
                             m_currentSegment);
  }
}

void TradingViewChartWidget::setXTSMarketDataClient(
    XTSMarketDataClient *client) {
  m_xtsClient = client;

  // Connect to tick updates for real-time data
  if (m_xtsClient) {
    connect(m_xtsClient, &XTSMarketDataClient::tickReceived, this,
            &TradingViewChartWidget::onTickUpdate, Qt::UniqueConnection);
    
    // Connect to OHLC candle updates (1505 WebSocket events)
    connect(m_xtsClient, &XTSMarketDataClient::candleReceived, this,
            &TradingViewChartWidget::onCandleReceived, Qt::UniqueConnection);
    
    qDebug() << "[TradingViewChart] Connected to XTS tick and candle feeds";
  }
}

void TradingViewChartWidget::setupWebChannel() {
  m_channel = new QWebChannel(this);
  m_channel->registerObject("dataBridge", m_dataBridge);
  m_webView->page()->setWebChannel(m_channel);

  qDebug() << "[TradingViewChart] Web channel setup complete";
}

void TradingViewChartWidget::loadChartHTML() {
  // Load from resources
  QFile file(":/html/tradingview_chart.html");
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QString html = QString::fromUtf8(file.readAll());

    // Set base URL to the resources/tradingview directory so that relative
    // paths work This allows the HTML to load
    // charting_library/charting_library.standalone.js
    QString appPath = QCoreApplication::applicationDirPath();
    QUrl baseUrl = QUrl::fromLocalFile(appPath + "/resources/tradingview/");

    m_webView->setHtml(html, baseUrl);
    qDebug() << "[TradingViewChart] Loading HTML from resources with base URL:"
             << baseUrl.toString();
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

void TradingViewChartWidget::loadSymbol(const QString &symbol, int segment,
                                        int64_t token,
                                        const QString &interval) {
  m_currentSymbol = symbol;
  m_currentSegment = segment;
  m_currentToken = token;
  m_currentInterval = interval;

  // Subscribe to real-time data for this token
  if (m_xtsClient && m_xtsClient->isLoggedIn()) {
    qDebug() << "[TradingViewChart] Subscribing to token:" << token
             << "segment:" << segment;
    m_xtsClient->subscribe(QVector<int64_t>{token}, segment);
  }

  if (!m_chartReady) {
    qWarning() << "[TradingViewChart] Chart not ready yet";
    return;
  }

  // Subscribe to real-time candle updates
  QString timeframe = interval;
  if (interval == "1")
    timeframe = "1m";
  else if (interval == "5")
    timeframe = "5m";
  else if (interval == "15")
    timeframe = "15m";
  else if (interval == "30")
    timeframe = "30m";
  else if (interval == "60")
    timeframe = "1h";
  else if (interval == "240")
    timeframe = "4h";
  else if (interval == "D")
    timeframe = "1d";

  CandleAggregator::instance().subscribeTo(symbol, segment, {timeframe});
  qDebug() << "[TradingViewChart] Subscribed to candles:" << symbol << segment
           << timeframe;

  // Construct full ticker for TradingView to enable token extraction
  QString ticker = QString("%1_%2_%3").arg(symbol).arg(segment).arg(token);

  QString script = QString("if (window.widget) {"
                           "  window.widget.setSymbol('%1', '%2', function() {"
                           "    console.log('Symbol changed to %1');"
                           "  });"
                           "}")
                       .arg(ticker, interval);

  executeScript(script);

  qDebug() << "[TradingViewChart] Loading symbol:" << ticker
           << "interval:" << interval;
}

void TradingViewChartWidget::setInterval(const QString &interval) {
  m_currentInterval = interval;

  if (!m_chartReady) {
    return;
  }

  QString script = QString("if (window.widget) {"
                           "  window.widget.setResolution('%1');"
                           "}")
                       .arg(interval);

  executeScript(script);
}

void TradingViewChartWidget::setTheme(const QString &theme) {
  if (!m_chartReady) {
    return;
  }

  QString script = QString("if (window.widget) {"
                           "  window.widget.changeTheme('%1');"
                           "}")
                       .arg(theme.toLower());

  executeScript(script);
}

void TradingViewChartWidget::addIndicator(const QString &indicatorName) {
  if (!m_chartReady) {
    return;
  }

  QString script = QString("if (window.widget) {"
                           "  window.widget.activeChart().createStudy('%1');"
                           "}")
                       .arg(indicatorName);

  executeScript(script);
}

void TradingViewChartWidget::addOrderMarker(qint64 time, double price,
                                            const QString &text,
                                            const QString &color,
                                            const QString &shape) {
  if (!m_chartReady) {
    return;
  }

  QJsonObject marker;
  marker["time"] = time * 1000; // TradingView expects milliseconds
  marker["position"] =
      (shape.contains("up") || shape.contains("buy")) ? "belowBar" : "aboveBar";
  marker["color"] = color;
  marker["shape"] = shape;
  marker["text"] = text;

  QString markerJson =
      QString::fromUtf8(QJsonDocument(marker).toJson(QJsonDocument::Compact));

  QString script = QString("if (window.widget) {"
                           "  window.widget.activeChart().createShape("
                           "    {time: %1, price: %2},"
                           "    {shape: '%3', text: '%4', fillColor: '%5'}"
                           "  );"
                           "}")
                       .arg(time * 1000)
                       .arg(price)
                       .arg(shape, text, color);

  executeScript(script);
}

void TradingViewChartWidget::executeScript(const QString &script) {
  m_webView->page()->runJavaScript(script, [](const QVariant &result) {
    if (!result.isNull()) {
      qDebug() << "[TradingViewChart] Script result:" << result;
    }
  });
}

void TradingViewChartWidget::onCandleComplete(const QString &symbol,
                                              int segment,
                                              const QString &timeframe,
                                              const ChartData::Candle &candle) {
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

void TradingViewChartWidget::onCandleUpdate(const QString &symbol, int segment,
                                            const QString &timeframe,
                                            const ChartData::Candle &candle) {
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

void TradingViewChartWidget::onTickUpdate(const XTS::Tick &tick) {
  // Only process ticks for our current token
  if (static_cast<int64_t>(tick.exchangeInstrumentID) != m_currentToken) {
    return;
  }

  if (tick.exchangeSegment != m_currentSegment) {
    return;
  }

  // Send real-time tick as partial bar update to TradingView
  // This provides instant feedback while CandleAggregator builds proper candles
  QJsonObject bar;
  bar["time"] = tick.lastUpdateTime; // XTS provides milliseconds
  bar["open"] = tick.open;
  bar["high"] = tick.high;
  bar["low"] = tick.low;
  bar["close"] = tick.lastTradedPrice;
  bar["volume"] = static_cast<qint64>(tick.volume);

  // Send to chart if ready
  if (m_chartReady && m_dataBridge) {
    m_dataBridge->sendRealtimeBar(bar);
  }
}

void TradingViewChartWidget::onCandleReceived(const XTS::OHLCCandle &candle) {
  // Only process candles for our current token
  if (candle.exchangeInstrumentID != m_currentToken) {
    return;
  }

  if (candle.exchangeSegment != m_currentSegment) {
    return;
  }

  // CRITICAL: Validate timestamp before sending to TradingView
  // Invalid timestamps (0 or < year 2000) cause time order violations
  if (candle.barTime == 0 || candle.barTime < 946684800) {
    qWarning() << "[TradingViewChart] Invalid candle timestamp:" << candle.barTime
               << "for token" << candle.exchangeInstrumentID << "- skipping";
    return;
  }

  // Convert XTS candle to TradingView format
  // XTS barTime is in seconds (Unix timestamp), TradingView expects milliseconds
  QJsonObject bar;
  bar["time"] = candle.barTime * 1000;
  bar["open"] = candle.open;
  bar["high"] = candle.high;
  bar["low"] = candle.low;
  bar["close"] = candle.close;
  bar["volume"] = candle.barVolume;

  qDebug() << "[TradingViewChart] Received 1-min candle for token"
           << candle.exchangeInstrumentID << "at" << candle.barTime
           << "OHLC:" << candle.open << candle.high << candle.low << candle.close
           << "Volume:" << candle.barVolume;

  // Send to chart if ready
  if (m_chartReady && m_dataBridge) {
    m_dataBridge->sendRealtimeBar(bar);
  }
}

void TradingViewChartWidget::onLoadFinished(bool success) {
  if (success) {
    qDebug() << "[TradingViewChart] Page loaded successfully";
  } else {
    qWarning() << "[TradingViewChart] Page load failed";
  }
}

void TradingViewChartWidget::onChartClickedInternal(qint64 time, double price) {
  // Re-emit original signal for external listeners
  emit chartClicked(time, price);
  
  // If no symbol is loaded, ignore
  if (m_currentSymbol.isEmpty() || m_currentToken == 0) {
    qDebug() << "[TradingViewChart] Chart clicked but no symbol loaded";
    return;
  }
  
  qDebug() << "[TradingViewChart] Chart clicked at price" << price 
           << "for symbol" << m_currentSymbol;
  
  // Create context menu for order placement
  QMenu menu(this);
  menu.setStyleSheet("QMenu { font-size: 11pt; padding: 5px; }");
  
  QAction *buyAction = menu.addAction(QString("Buy %1 @ %2")
                                       .arg(m_currentSymbol)
                                       .arg(price, 0, 'f', 2));
  buyAction->setIcon(QIcon(":/icons/buy.png"));
  
  QAction *sellAction = menu.addAction(QString("Sell %1 @ %2")
                                        .arg(m_currentSymbol)
                                        .arg(price, 0, 'f', 2));
  sellAction->setIcon(QIcon(":/icons/sell.png"));
  
  menu.addSeparator();
  QAction *cancelAction = menu.addAction("Cancel");
  
  // Show menu at cursor position
  QAction *selectedAction = menu.exec(QCursor::pos());
  
  if (!selectedAction || selectedAction == cancelAction) {
    return;
  }
  
  // Create WindowContext with clicked price
  WindowContext context;
  context.sourceWindow = "TradingViewChart";
  context.symbol = m_currentSymbol;
  context.token = m_currentToken;
  context.segment = QString::number(m_currentSegment);
  
  // Set clicked price as both LTP and price for order
  context.ltp = price;
  context.bid = price;
  context.ask = price;
  
  // Get contract details from repository if available
  if (m_repoManager) {
    // Try to find the contract in the repository to get exchange, series, etc.
    // For now, we'll use basic info
    if (m_currentSegment == 1) {
      context.exchange = "NSECM";
    } else if (m_currentSegment == 2) {
      context.exchange = "NSEFO";
    } else if (m_currentSegment == 11) {
      context.exchange = "BSECM";
    } else if (m_currentSegment == 12) {
      context.exchange = "BSEFO";
    } else {
      context.exchange = "NSECM"; // Default
    }
  }
  
  // Show appropriate order window
  if (selectedAction == buyAction) {
    BuyWindow *buyWin = BuyWindow::getInstance(context, this);
    buyWin->show();
    buyWin->raise();
    buyWin->activateWindow();
  } else if (selectedAction == sellAction) {
    SellWindow *sellWin = SellWindow::getInstance(context, this);
    sellWin->show();
    sellWin->raise();
    sellWin->activateWindow();
  }
}

void TradingViewChartWidget::onJavaScriptMessage(const QString &message) {
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

QString TradingViewChartWidget::convertTimeframeToInterval(
    const QString &timeframe) const {
  if (timeframe == "1m")
    return "1";
  if (timeframe == "5m")
    return "5";
  if (timeframe == "15m")
    return "15";
  if (timeframe == "30m")
    return "30";
  if (timeframe == "1h")
    return "60";
  if (timeframe == "4h")
    return "240";
  if (timeframe == "1D")
    return "D";
  if (timeframe == "1W")
    return "W";
  return "5"; // default
}

// ============================================================================
// TradingViewDataBridge Implementation
// ============================================================================

TradingViewDataBridge::TradingViewDataBridge(QObject *parent)
    : QObject(parent) {}

void TradingViewDataBridge::onChartReady() {
  qDebug() << "[TradingViewDataBridge] Chart ready signal received";
  emit chartReady();
}

void TradingViewDataBridge::onChartClick(qint64 time, double price) {
  qDebug() << "[TradingViewDataBridge] Chart clicked at" << time << price;
  emit chartClicked(time / 1000, price); // Convert from milliseconds
}

void TradingViewDataBridge::onOrderRequest(const QString &side, double price) {
  qDebug() << "[TradingViewDataBridge] Order requested:" << side << "@"
           << price;
  emit orderRequested(side, price);
}

void TradingViewDataBridge::requestHistoricalData(const QString &symbol,
                                                  int segment,
                                                  const QString &resolution,
                                                  qint64 from, qint64 to,
                                                  qint64 token, int requestId) {
  qDebug() << "[TradingViewDataBridge] Historical data requested:" << symbol
           << resolution << "from" << from << "to" << to << "token:" << token
           << "requestId:" << requestId;
  emit historicalDataRequested(symbol, segment, resolution, from / 1000,
                               to / 1000, token, requestId);
}

void TradingViewDataBridge::sendHistoricalData(const QJsonArray &bars,
                                               int requestId) {
  qDebug() << "[TradingViewDataBridge] ========== SENDING HISTORICAL DATA "
              "==========";
  qDebug() << "[TradingViewDataBridge] Bar count:" << bars.size()
           << "requestId:" << requestId;

  if (bars.size() > 0) {
    qDebug() << "[TradingViewDataBridge] First bar:" << bars[0];
    qDebug() << "[TradingViewDataBridge] Last bar:" << bars[bars.size() - 1];
  } else {
    qWarning() << "[TradingViewDataBridge] ⚠️ Empty bars array - chart will "
                  "show 'No data'!";
  }

  qDebug() << "[TradingViewDataBridge] Emitting historicalDataReady signal...";
  emit historicalDataReady(bars, requestId);
  qDebug() << "[TradingViewDataBridge] ✅ Signal emitted to JavaScript";
}

void TradingViewDataBridge::sendRealtimeBar(const QJsonObject &bar) {
  emit realtimeBarUpdate(bar);
}

void TradingViewDataBridge::sendError(const QString &error) {
  qWarning() << "[TradingViewDataBridge] Error:" << error;
  emit errorOccurred(error);
}

void TradingViewDataBridge::searchSymbols(const QString &searchText,
                                          const QString &exchange,
                                          const QString &segment) {
  QElapsedTimer timer;
  timer.start();

  qDebug() << "";
  qDebug() << "═══════════════════════════════════════════════════════";
  qDebug() << "[SYMBOL SEARCH] Query:" << searchText
           << "| Exchange:" << exchange << "| Segment:" << segment;
  qDebug() << "═══════════════════════════════════════════════════════";

  // Get parent widget to access RepositoryManager
  TradingViewChartWidget *widget =
      qobject_cast<TradingViewChartWidget *>(parent());
  if (!widget || !widget->m_repoManager) {
    qWarning() << "[SYMBOL SEARCH] ✗ No RepositoryManager available";
    emit symbolSearchResults(QJsonArray());
    return;
  }

  // Use the new fuzzy global search for much better results
  QVector<ContractData> results = widget->m_repoManager->searchScripsGlobal(
      searchText, exchange, segment, "", 20);

  qDebug() << "[SEARCH RESULTS] Found" << results.size()
           << "results using Global Search";
  qDebug() << "";

  // Convert to JSON format for TradingView
  QJsonArray jsonResults;
  for (int i = 0; i < results.size(); ++i) {
    const ContractData &contract = results[i];
    QJsonObject item;

    // Identify exchange and segment from contract data
    QString contractExchange =
        (contract.exchangeInstrumentID >= 11000000) ? "BSE" : "NSE";
    QString contractSegment =
        (contract.strikePrice > 0 || contract.instrumentType == 1) ? "FO"
                                                                   : "CM";

    int segmentInt = 1; // Default NSE CM
    if (contractExchange == "NSE") {
      segmentInt = (contractSegment == "FO") ? 2 : 1;
    } else {
      segmentInt = (contractSegment == "FO") ? 12 : 11;
    }

    // Build display name and description for better UX
    QString displayName;
    QString description;
    QString searchDisplayText;
    QString tvType = "stock"; // fallback

    if (contractSegment == "FO") {
      // For F&O contracts, show strike/type/expiry clearly
      if (contract.strikePrice > 0) {
        // Options: "BANKNIFTY 24000 CE"
        QString optType = "CE";
        if (contract.optionType.contains("P", Qt::CaseInsensitive)) {
          optType = "PE";
        }
        displayName = QString("%1 %2 %3")
                          .arg(contract.name)
                          .arg(contract.strikePrice, 0, 'f',
                               1) // 1 dec place for cleaner look
                          .arg(optType);
        description = QString("%1 %2 | %3")
                          .arg(contract.name)
                          .arg(contract.strikePrice, 0, 'f', 1)
                          .arg(contract.expiryDate);
        searchDisplayText =
            QString("%1 · %2 %3 · %4")
                .arg(contract.name,
                     QString::number(contract.strikePrice, 'f', 1), optType,
                     contract.expiryDate);
        tvType = "options";
      } else {
        // Futures: "BANKNIFTY FUT"
        displayName = QString("%1 FUT").arg(contract.name);
        description =
            QString("%1 | Exp: %2").arg(displayName).arg(contract.expiryDate);
        searchDisplayText =
            QString("%1 · FUT · %2").arg(contract.name, contract.expiryDate);
        tvType = "futures";
      }
    } else {
      // Cash market: "RELIANCE EQ"
      displayName = contract.name;
      description = QString("%1 | %2").arg(contract.name, contract.series);
      searchDisplayText =
          QString("%1 · %2 · %3")
              .arg(contract.name, contractExchange, contract.series);
      tvType = "stock";
    }

    item["symbol"] = displayName;
    item["full_name"] = searchDisplayText;
    item["description"] = description;
    item["exchange"] = contractExchange;
    item["type"] = tvType; // Standarized for TradingView filters
    item["token"] = static_cast<qint64>(contract.exchangeInstrumentID);
    item["segment"] = segmentInt;
    item["ticker"] = QString("%1_%2_%3")
                         .arg(contract.name)
                         .arg(segmentInt)
                         .arg(contract.exchangeInstrumentID);
    item["expiry"] = contract.expiryDate;
    item["strike"] = contract.strikePrice;
    item["optionType"] = contract.optionType;

    jsonResults.append(item);
  }

  // Log detailed results
  qDebug() << "[SEARCH SUGGESTIONS]";
  for (int i = 0; i < qMin(jsonResults.size(), 10); ++i) {
    QJsonObject obj = jsonResults[i].toObject();
    QString logLine = QString("  %1. %2 (Token: %3)")
                          .arg(i + 1, 2)
                          .arg(obj["full_name"].toString())
                          .arg(obj["token"].toVariant().toLongLong());
    qDebug().noquote() << logLine;
  }
  if (jsonResults.size() > 10) {
    qDebug() << QString("  ... and %1 more").arg(jsonResults.size() - 10);
  }

  qint64 totalTime = timer.nsecsElapsed() / 1000; // microseconds
  qDebug() << "";
  qDebug() << QString("[SEARCH COMPLETE] Total time: %1 μs (%2 ms)")
                  .arg(totalTime)
                  .arg(totalTime / 1000.0, 0, 'f', 2);
  qDebug() << "═══════════════════════════════════════════════════════";
  qDebug() << "";

  emit symbolSearchResults(jsonResults);
}

void TradingViewDataBridge::loadSymbol(const QString &symbol, int segment,
                                       qint64 token, const QString &interval) {
  qDebug() << "[TradingViewDataBridge] loadSymbol called from JavaScript:"
           << "symbol=" << symbol << "segment=" << segment << "token=" << token
           << "interval=" << interval;

  // Get parent widget and call its loadSymbol method
  TradingViewChartWidget *widget =
      qobject_cast<TradingViewChartWidget *>(parent());
  if (widget) {
    widget->loadSymbol(symbol, segment, token, interval);
  } else {
    qWarning() << "[TradingViewDataBridge] Failed to get parent widget";
  }
}

void TradingViewDataBridge::placeOrder(const QString &symbol, int segment,
                                       const QString &side, int quantity,
                                       const QString &orderType, double price,
                                       double slPrice) {
  qDebug() << "[TradingViewDataBridge] placeOrder called from JavaScript:";
  qDebug() << "  Symbol:" << symbol << "| Segment:" << segment;
  qDebug() << "  Side:" << side << "| Qty:" << quantity;
  qDebug() << "  Type:" << orderType << "| Price:" << price << "| SL:" << slPrice;

  // Get parent widget to access RepositoryManager and emit to MainWindow
  TradingViewChartWidget *widget =
      qobject_cast<TradingViewChartWidget *>(parent());
  
  if (!widget || !widget->m_repoManager) {
    qWarning() << "[TradingViewDataBridge] No parent widget or RepositoryManager";
    emit orderFailed("Chart widget not initialized");
    return;
  }

  // Resolve token for the symbol if not provided
  qint64 token = 0;
  QString exchange = (segment == 1 || segment == 2) ? "NSE" : "BSE";
  QString segmentName = (segment == 2 || segment == 12) ? "FO" : "CM";
  
  // Search for the symbol to get token
  QVector<ContractData> results = 
      widget->m_repoManager->searchScripsGlobal(symbol, exchange, segmentName, "", 1);
  
  if (results.isEmpty()) {
    QString error = QString("Symbol '%1' not found in %2 %3")
                        .arg(symbol, exchange, segmentName);
    qWarning() << "[TradingViewDataBridge]" << error;
    emit orderFailed(error);
    return;
  }

  const ContractData &contract = results.first();
  token = contract.exchangeInstrumentID;

  qDebug() << "[TradingViewDataBridge] Resolved token:" << token
           << "for" << contract.name;

  // Build OrderParams struct
  XTS::OrderParams params;
  params.exchangeSegment = QString::number(segment);
  params.exchangeInstrumentID = token;
  params.orderSide = side.toUpper(); // "BUY" or "SELL"
  params.orderQuantity = quantity;
  params.productType = "NRML"; // Default to NRML (can be MIS/CNC)
  params.disclosedQuantity = 0;
  params.timeInForce = "DAY";
  
  // Set order type and prices
  if (orderType.toUpper() == "MARKET") {
    params.orderType = "MARKET";
    params.limitPrice = 0;
    params.stopPrice = 0;
  } else if (orderType.toUpper() == "LIMIT") {
    params.orderType = "LIMIT";
    params.limitPrice = price;
    params.stopPrice = 0;
  } else if (orderType.toUpper() == "SL" || orderType.toUpper() == "STOPLIMIT") {
    params.orderType = "STOPLIMIT";
    params.limitPrice = price;
    params.stopPrice = slPrice;
  } else if (orderType.toUpper() == "SL-M" || orderType.toUpper() == "STOPMARKET") {
    params.orderType = "STOPMARKET";
    params.limitPrice = 0;
    params.stopPrice = slPrice;
  } else {
    emit orderFailed("Unknown order type: " + orderType);
    return;
  }

  params.orderUniqueIdentifier = 
      QString("CHART_%1_%2").arg(symbol).arg(QDateTime::currentMSecsSinceEpoch());

  // Emit signal to parent widget which will forward to MainWindow
  emit widget->orderRequestedFromChart(params);
  
  qDebug() << "[TradingViewDataBridge] Order request emitted to MainWindow";
}
