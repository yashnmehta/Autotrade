#ifdef HAVE_QTWEBENGINE
#include "ui/TradingViewChartWidget.h"
#include "api/NativeHTTPClient.h"
#include "api/XTSMarketDataClient.h"
#include "api/XTSTypes.h"
#include "repository/ContractData.h"
#include "repository/RepositoryManager.h"
#include "services/CandleAggregator.h"
#include "services/HistoricalDataStore.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
          &TradingViewChartWidget::chartClicked);

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
                      QJsonObject bar;
                      bar["time"] = fields[0].toLongLong() * 1000;
                      bar["open"] = fields[1].toDouble();
                      bar["high"] = fields[2].toDouble();
                      bar["volume"] = fields[5].toDouble();
                      bar["close"] = fields[4].toDouble();
                      bar["low"] = fields[3].toDouble();
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
    qDebug() << "[TradingViewChart] Connected to XTS tick feed";
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

    // Set base URL to the resources/tradingview directory
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

  // Construct full ticker for TradingView
  QString ticker = QString("%1_%2_%3").arg(symbol).arg(segment).arg(token);

  executeScript(QString("if (window.tvWidget) { window.tvWidget.setSymbol('%1', "
                        "'%2'); }")
                    .arg(ticker, interval));
}

bool TradingViewChartWidget::saveTemplate(const QString &templateName) {
  if (!m_chartReady) {
    qWarning() << "[TradingViewChart] Chart not ready, cannot save template";
    return false;
  }

  // Request JavaScript to save and send template data
  executeScript(QString("if (window.tvWidget) {"
                        "  window.tvWidget.save(function(data) {"
                        "    if (window.dataBridge) {"
                        "      window.dataBridge.receiveTemplateData('%1', "
                        "JSON.stringify(data));"
                        "    }"
                        "  });"
                        "}")
                    .arg(templateName));
  return true;
}

bool TradingViewChartWidget::loadTemplate(const QString &templateName) {
  if (!m_chartReady) {
    qWarning() << "[TradingViewChart] Chart not ready, cannot load template";
    return false;
  }

  // Read template from file
  QString appDataPath = QCoreApplication::applicationDirPath() + "/templates";
  QString filePath = appDataPath + "/" + templateName + ".json";

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "[TradingViewChart] Failed to load template:" << filePath;
    return false;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  if (doc.isNull() || !doc.isObject()) {
    qWarning() << "[TradingViewChart] Invalid template data:" << templateName;
    return false;
  }

  QJsonObject rootObj = doc.object();
  QJsonObject templateData = rootObj["data"].toObject();

  // Convert template data back to JSON string
  QString dataStr = QString::fromUtf8(QJsonDocument(templateData).toJson());

  // Escape quotes for JavaScript
  dataStr.replace("\"", "\\\"");
  dataStr.replace("\n", " ");

  executeScript(QString("if (window.tvWidget) {"
                        "  var data = JSON.parse(\"%1\");"
                        "  window.tvWidget.load(data);"
                        "}")
                    .arg(dataStr));

  qDebug() << "[TradingViewChart] Template loaded:" << templateName;
  return true;
}

QStringList TradingViewChartWidget::getTemplateList() const {
  QString appDataPath = QCoreApplication::applicationDirPath() + "/templates";
  QDir dir(appDataPath);

  if (!dir.exists()) {
    return QStringList();
  }

  QStringList filters;
  filters << "*.json";
  QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

  QStringList templateNames;
  for (const QFileInfo &fileInfo : files) {
    templateNames << fileInfo.baseName();
  }

  return templateNames;
}

bool TradingViewChartWidget::deleteTemplate(const QString &templateName) {
  QString appDataPath = QCoreApplication::applicationDirPath() + "/templates";
  QString filePath = appDataPath + "/" + templateName + ".json";

  QFile file(filePath);
  if (file.exists()) {
    bool success = file.remove();
    if (success) {
      qDebug() << "[TradingViewChart] Template deleted:" << templateName;
    } else {
      qWarning() << "[TradingViewChart] Failed to delete template:"
                 << templateName;
    }
    return success;
  }

  return false;
}

void TradingViewChartWidget::setInterval(const QString &interval) {
  m_currentInterval = interval;
  if (!m_chartReady)
    return;

  QString script = QString("if (window.widget) {"
                           "  window.widget.setResolution('%1');"
                           "}")
                       .arg(interval);

  executeScript(script);
}

void TradingViewChartWidget::setTheme(const QString &theme) {
  if (!m_chartReady)
    return;

  QString script = QString("if (window.widget) {"
                           "  window.widget.changeTheme('%1');"
                           "}")
                       .arg(theme.toLower());

  executeScript(script);
}

void TradingViewChartWidget::addIndicator(const QString &indicatorName) {
  if (!m_chartReady)
    return;

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
  if (!m_chartReady)
    return;

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
  if (symbol != m_currentSymbol || segment != m_currentSegment)
    return;

  QString interval = convertTimeframeToInterval(timeframe);
  if (interval != m_currentInterval)
    return;

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
  if (symbol != m_currentSymbol || segment != m_currentSegment)
    return;

  QString interval = convertTimeframeToInterval(timeframe);
  if (interval != m_currentInterval)
    return;

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
  if (static_cast<int64_t>(tick.exchangeInstrumentID) != m_currentToken)
    return;
  if (tick.exchangeSegment != m_currentSegment)
    return;

  QJsonObject bar;
  bar["time"] = tick.lastUpdateTime;
  bar["open"] = tick.open;
  bar["high"] = tick.high;
  bar["low"] = tick.low;
  bar["close"] = tick.lastTradedPrice;
  bar["volume"] = static_cast<qint64>(tick.volume);

  if (m_chartReady && m_dataBridge) {
    m_dataBridge->sendRealtimeBar(bar);
  }
}

void TradingViewChartWidget::onLoadFinished(bool success) {
  if (success)
    qDebug() << "[TradingViewChart] Page loaded successfully";
  else
    qWarning() << "[TradingViewChart] Page load failed";
}

void TradingViewChartWidget::onJavaScriptMessage(const QString &message) {
  qDebug() << "[TradingViewChart] JS Message:" << message;
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
  return "5";
}

// Data Bridge Implementation
TradingViewDataBridge::TradingViewDataBridge(QObject *parent)
    : QObject(parent) {}

void TradingViewDataBridge::onChartReady() { emit chartReady(); }

void TradingViewDataBridge::onChartClick(qint64 time, double price) {
  emit chartClicked(time / 1000, price);
}

void TradingViewDataBridge::onOrderRequest(const QString &side, double price) {
  emit orderRequested(side, price);
}

void TradingViewDataBridge::requestHistoricalData(const QString &symbol,
                                                  int segment,
                                                  const QString &resolution,
                                                  qint64 from, qint64 to,
                                                  qint64 token, int requestId) {
  emit historicalDataRequested(symbol, segment, resolution, from / 1000,
                               to / 1000, token, requestId);
}

void TradingViewDataBridge::sendHistoricalData(const QJsonArray &bars,
                                               int requestId) {
  emit historicalDataReady(bars, requestId);
}

void TradingViewDataBridge::sendRealtimeBar(const QJsonObject &bar) {
  emit realtimeBarUpdate(bar);
}

void TradingViewDataBridge::sendError(const QString &error) {
  emit errorOccurred(error);
}

void TradingViewDataBridge::searchSymbols(const QString &searchText,
                                          const QString &exchange,
                                          const QString &segment) {
  TradingViewChartWidget *widget =
      qobject_cast<TradingViewChartWidget *>(parent());
  if (!widget || !widget->m_repoManager) {
    emit symbolSearchResults(QJsonArray());
    return;
  }

  QVector<ContractData> results = widget->m_repoManager->searchScripsGlobal(
      searchText, exchange, segment, "", 20);

  QJsonArray jsonResults;
  for (const ContractData &contract : results) {
    QJsonObject item;
    QString contractExchange =
        (contract.exchangeInstrumentID >= 11000000) ? "BSE" : "NSE";
    QString contractSegment =
        (contract.strikePrice > 0 || contract.instrumentType == 1) ? "FO"
                                                                   : "CM";

    int segmentInt = (contractExchange == "NSE")
                         ? (contractSegment == "FO" ? 2 : 1)
                         : (contractSegment == "FO" ? 12 : 11);

    item["symbol"] = contract.name;
    item["description"] = contract.description;
    item["exchange"] = contractExchange;
    item["token"] = static_cast<qint64>(contract.exchangeInstrumentID);
    item["segment"] = segmentInt;
    item["ticker"] = QString("%1_%2_%3")
                         .arg(contract.name)
                         .arg(segmentInt)
                         .arg(contract.exchangeInstrumentID);

    jsonResults.append(item);
  }

  emit symbolSearchResults(jsonResults);
}

void TradingViewDataBridge::loadSymbol(const QString &symbol, int segment,
                                       qint64 token, const QString &interval) {
  TradingViewChartWidget *widget =
      qobject_cast<TradingViewChartWidget *>(parent());
  if (widget)
    widget->loadSymbol(symbol, segment, token, interval);
}

void TradingViewDataBridge::placeOrder(const QString &symbol, int segment,
                                       const QString &side, int quantity,
                                       const QString &orderType, double price,
                                       double slPrice) {
  TradingViewChartWidget *widget =
      qobject_cast<TradingViewChartWidget *>(parent());
  if (!widget || !widget->m_repoManager)
    return;

  QVector<ContractData> results =
      widget->m_repoManager->searchScripsGlobal(symbol, "NSE", "FO", "", 1);
  if (results.isEmpty())
    return;

  const ContractData &contract = results.first();
  XTS::OrderParams params;
  params.exchangeSegment = QString::number(segment);
  params.exchangeInstrumentID = contract.exchangeInstrumentID;
  params.orderSide = side.toUpper();
  params.orderQuantity = quantity;
  params.productType = "NRML";
  params.orderType = orderType.toUpper();
  params.limitPrice = price;
  params.stopPrice = slPrice;
  params.orderUniqueIdentifier =
      QString("CHART_%1").arg(QDateTime::currentMSecsSinceEpoch());

  emit widget->orderRequestedFromChart(params);
}

void TradingViewDataBridge::receiveTemplateData(const QString &templateName,
                                                const QString &templateData) {
  TradingViewChartWidget *widget =
      qobject_cast<TradingViewChartWidget *>(parent());
  if (!widget)
    return;

  // Save template data to file
  QString appDataPath = QCoreApplication::applicationDirPath() + "/templates";
  QDir dir;
  if (!dir.exists(appDataPath)) {
    dir.mkpath(appDataPath);
  }

  QString filePath = appDataPath + "/" + templateName + ".json";
  QFile file(filePath);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QJsonObject rootObj;
    rootObj["name"] = templateName;
    rootObj["timestamp"] = QDateTime::currentSecsSinceEpoch();
    rootObj["data"] = QJsonDocument::fromJson(templateData.toUtf8()).object();

    QJsonDocument doc(rootObj);
    file.write(doc.toJson());
    file.close();
    qDebug() << "[TradingViewChart] Template saved:" << templateName;
  } else {
    qWarning() << "[TradingViewChart] Failed to save template:" << filePath;
  }
}

void TradingViewDataBridge::openTemplateManager() {
  TradingViewChartWidget *widget =
      qobject_cast<TradingViewChartWidget *>(parent());
  if (!widget)
    return;

  // Call the manageChartTemplates method via the parent MainWindow
  QWidget *mainWindow = widget->window();
  if (mainWindow) {
    QMetaObject::invokeMethod(mainWindow, "manageChartTemplates",
                              Qt::QueuedConnection);
  }
}

#endif // HAVE_QTWEBENGINE
