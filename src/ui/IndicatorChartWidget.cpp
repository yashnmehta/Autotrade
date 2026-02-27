#include "ui/IndicatorChartWidget.h"
#include "api/transport/NativeHTTPClient.h"
#include "indicators/TALibIndicators.h"
#include "ui/GlobalSearchWidget.h"
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QSpinBox>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

IndicatorChartWidget::IndicatorChartWidget(QWidget *parent) : QWidget(parent) {
  setupUI();
  setupPriceChart();
  setupToolbar();

  // Apply dark theme
  applyDarkTheme(m_priceChart);

  qDebug() << "[IndicatorChart] Chart widget initialized";
}

IndicatorChartWidget::~IndicatorChartWidget() {
  // Cleanup is automatic (Qt parent-child ownership)
}

void IndicatorChartWidget::setupUI() {
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(0, 0, 0, 0);
  m_mainLayout->setSpacing(2);
}

void IndicatorChartWidget::setupPriceChart() {
  // Create chart
  m_priceChart = new QChart();
  m_priceChart->setTitle("Price Chart");
  m_priceChart->setAnimationOptions(
      QChart::NoAnimation); // Disable for performance
  m_priceChart->legend()->setVisible(true);
  m_priceChart->legend()->setAlignment(Qt::AlignBottom);

  // Create candlestick series
  m_candlestickSeries = new QCandlestickSeries();
  m_candlestickSeries->setName("Price");
  m_candlestickSeries->setIncreasingColor(QColor("#26a69a"));
  m_candlestickSeries->setDecreasingColor(QColor("#ef5350"));
  m_priceChart->addSeries(m_candlestickSeries);

  // Create axes
  m_axisX = new QDateTimeAxis();
  m_axisX->setFormat("dd MMM hh:mm");
  m_axisX->setTitleText("Time");
  m_priceChart->addAxis(m_axisX, Qt::AlignBottom);
  m_candlestickSeries->attachAxis(m_axisX);

  m_axisYPrice = new QValueAxis();
  m_axisYPrice->setTitleText("Price");
  m_axisYPrice->setLabelFormat("%.2f");
  m_priceChart->addAxis(m_axisYPrice, Qt::AlignLeft);
  m_candlestickSeries->attachAxis(m_axisYPrice);

  // Create chart view
  m_priceChartView = new QChartView(m_priceChart);
  m_priceChartView->setRenderHint(QPainter::Antialiasing);
  m_priceChartView->setRubberBand(
      QChartView::HorizontalRubberBand); // Enable zoom

  m_mainLayout->addWidget(m_priceChartView, 3); // Stretch factor 3 (larger)
}

void IndicatorChartWidget::setupToolbar() {
  m_toolbar = new QToolBar(this);
  m_toolbar->setMovable(false);
  m_toolbar->setFloatable(false);

  // Symbol search input
  m_toolbar->addWidget(new QLabel("Symbol: "));
  m_symbolInput = new QLineEdit();
  m_symbolInput->setPlaceholderText("Enter symbol (e.g. NIFTY, BANKNIFTY)");
  m_symbolInput->setMinimumWidth(200);
  m_symbolInput->setMaximumWidth(250);
  connect(m_symbolInput, &QLineEdit::returnPressed, this,
          &IndicatorChartWidget::onSymbolEntered);
  m_toolbar->addWidget(m_symbolInput);

  QPushButton *searchButton = new QPushButton("Search");
  searchButton->setIcon(
      style()->standardIcon(QStyle::SP_FileDialogContentsView));
  connect(searchButton, &QPushButton::clicked, this,
          &IndicatorChartWidget::onSymbolEntered);
  m_toolbar->addWidget(searchButton);

  m_globalSearchButton = new QPushButton("");
  m_globalSearchButton->setIcon(style()->standardIcon(
      QStyle::SP_ComputerIcon)); // Using computer icon as placeholder for
                                 // search/globe
  m_globalSearchButton->setToolTip("Advanced Global Search");
  connect(m_globalSearchButton, &QPushButton::clicked, this,
          &IndicatorChartWidget::onGlobalSearchClicked);
  m_toolbar->addWidget(m_globalSearchButton);

  m_toolbar->addSeparator();

  // Indicator selector
  m_toolbar->addWidget(new QLabel("Indicator:"));
  m_indicatorSelector = new QComboBox();
  m_indicatorSelector->addItem("SMA", "SMA");
  m_indicatorSelector->addItem("EMA", "EMA");
  m_indicatorSelector->addItem("Bollinger Bands", "BB");
  m_indicatorSelector->addItem("RSI", "RSI");
  m_indicatorSelector->addItem("MACD", "MACD");
  m_indicatorSelector->addItem("Stochastic", "STOCH");
  m_indicatorSelector->addItem("ATR", "ATR");
  m_indicatorSelector->addItem("CCI", "CCI");
  m_indicatorSelector->addItem("Volume", "VOLUME");
  m_toolbar->addWidget(m_indicatorSelector);

  // Add/Remove buttons
  m_addButton = new QPushButton("Add");
  m_addButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
  connect(m_addButton, &QPushButton::clicked, this,
          &IndicatorChartWidget::onAddIndicatorClicked);
  m_toolbar->addWidget(m_addButton);

  m_removeButton = new QPushButton("Remove");
  m_removeButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
  connect(m_removeButton, &QPushButton::clicked, this,
          &IndicatorChartWidget::onRemoveIndicatorClicked);
  m_toolbar->addWidget(m_removeButton);

  m_toolbar->addSeparator();

  // Zoom controls
  m_zoomInButton = new QPushButton("Zoom In");
  m_zoomInButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
  connect(m_zoomInButton, &QPushButton::clicked, this,
          &IndicatorChartWidget::onZoomInClicked);
  m_toolbar->addWidget(m_zoomInButton);

  m_zoomOutButton = new QPushButton("Zoom Out");
  m_zoomOutButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
  connect(m_zoomOutButton, &QPushButton::clicked, this,
          &IndicatorChartWidget::onZoomOutClicked);
  m_toolbar->addWidget(m_zoomOutButton);

  m_resetZoomButton = new QPushButton("Reset");
  connect(m_resetZoomButton, &QPushButton::clicked, this,
          &IndicatorChartWidget::onResetZoomClicked);
  m_toolbar->addWidget(m_resetZoomButton);

  m_mainLayout->insertWidget(0, m_toolbar); // Add at top
}

void IndicatorChartWidget::loadSymbol(const QString &symbol, int segment,
                                      qint64 token) {
  m_currentSymbol = symbol;
  m_currentSegment = segment;
  m_currentToken = token;

  // Update symbol input field
  if (m_symbolInput) {
    m_symbolInput->setText(symbol);
  }

  m_priceChart->setTitle(QString("%1 - Price Chart").arg(symbol));

  qDebug() << "[IndicatorChart] Loaded symbol:" << symbol
           << "segment:" << segment << "token:" << token;

  // Clear existing data immediately
  setCandleData(QVector<ChartData::Candle>());

  // Fetch OHLC data if XTS client is available
  if (m_xtsClient && token != 0) {
    fetchOHLCData(symbol, segment, token);
  }
}

void IndicatorChartWidget::setXTSMarketDataClient(XTSMarketDataClient *client) {
  m_xtsClient = client;
  qDebug() << "[IndicatorChart] XTS client set:" << (client != nullptr);
}

void IndicatorChartWidget::setRepositoryManager(
    RepositoryManager *repoManager) {
  m_repoManager = repoManager;
  qDebug() << "[IndicatorChart] Repository manager set:"
           << (repoManager != nullptr);
}

void IndicatorChartWidget::fetchOHLCData(const QString &symbol, int segment,
                                         qint64 token) {
  if (!m_xtsClient) {
    qWarning() << "[IndicatorChart] Cannot fetch OHLC: XTS client not set";
    return;
  }

  if (token == 0) {
    qWarning() << "[IndicatorChart] Cannot fetch OHLC: Invalid token";
    return;
  }

  QString authToken = m_xtsClient->token();
  if (authToken.isEmpty()) {
    qWarning() << "[IndicatorChart] Cannot fetch OHLC: Not logged in to XTS";
    return;
  }

  // Calculate time range (last 7 days to now)
  QDateTime now = QDateTime::currentDateTime();
  QDateTime startDt = now.addDays(-7);

  QString startTime = startDt.toString("MMM dd yyyy HHmmss");
  QString endTime = now.toString("MMM dd yyyy HHmmss");

  // Default to 5-minute candles
  int compressionSeconds = 300; // 5 minutes

  QString urlStr =
      QString("http://192.168.102.9:3000/apimarketdata/instruments/ohlc"
              "?exchangeSegment=%1&exchangeInstrumentID=%2"
              "&startTime=%3&endTime=%4&compressionValue=%5")
          .arg(segment)
          .arg(token)
          .arg(QUrl::toPercentEncoding(startTime).constData())
          .arg(QUrl::toPercentEncoding(endTime).constData())
          .arg(compressionSeconds);

  qDebug() << "[IndicatorChart] Fetching OHLC for" << symbol;
  qDebug() << "[IndicatorChart]   Token:" << token << "Segment:" << segment;
  qDebug() << "[IndicatorChart]   From:" << startTime << "To:" << endTime;
  qDebug() << "[IndicatorChart]   URL:" << urlStr;

  // Fetch in background thread
  QtConcurrent::run([this, authToken, urlStr, symbol]() {
    NativeHTTPClient client;
    client.setTimeout(30);

    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["authorization"] = authToken.toStdString();

    auto response = client.get(urlStr.toStdString(), headers);

    if (response.success && response.statusCode == 200) {
      QJsonDocument doc =
          QJsonDocument::fromJson(QByteArray::fromStdString(response.body));

      qDebug() << "[IndicatorChart] API Response for" << symbol << ":"
               << QString::fromStdString(response.body).left(500);

      if (doc.isObject()) {
        QJsonObject root = doc.object();
        QJsonObject result = root["result"].toObject();

        // Handle the backend typo: dataReponse vs dataResponse
        QString dataResponse;
        if (result.contains("dataReponse") &&
            !result["dataReponse"].toString().isEmpty()) {
          dataResponse = result["dataReponse"].toString();
        } else if (result.contains("dataResponse") &&
                   !result["dataResponse"].toString().isEmpty()) {
          dataResponse = result["dataResponse"].toString();
        }

        qDebug() << "[IndicatorChart] dataResponse found?"
                 << !dataResponse.isEmpty()
                 << "Length:" << dataResponse.length();

        if (!dataResponse.isEmpty()) {
          QVector<ChartData::Candle> candles;
          QStringList barStrings = dataResponse.split(',', Qt::SkipEmptyParts);

          for (const QString &barStr : barStrings) {
            QStringList fields = barStr.split('|', Qt::SkipEmptyParts);
            if (fields.size() >= 6) {
              ChartData::Candle candle;
              candle.timestamp = fields[0].toLongLong();
              candle.open = fields[1].toDouble();
              candle.high = fields[2].toDouble();
              candle.low = fields[3].toDouble();
              candle.close = fields[4].toDouble();
              candle.volume = fields[5].toDouble();
              candles.append(candle);
            }
          }

          // Update UI on main thread
          QMetaObject::invokeMethod(
              this,
              [this, candles, symbol]() {
                setCandleData(candles);
                qDebug() << "[IndicatorChart] Loaded" << candles.size()
                         << "candles for" << symbol;
              },
              Qt::QueuedConnection);
        } else {
          qWarning() << "[IndicatorChart] Empty OHLC response for" << symbol;
          qWarning() << "[IndicatorChart] Result keys:"
                     << result.keys().join(", ");

          // Clear existing chart if new symbol has no data
          QMetaObject::invokeMethod(
              this, [this]() { setCandleData(QVector<ChartData::Candle>()); },
              Qt::QueuedConnection);
        }
      } else {
        qWarning() << "[IndicatorChart] Invalid JSON structure from API";
      }
    } else {
      qWarning() << "[IndicatorChart] OHLC fetch failed for" << symbol << ":"
                 << response.statusCode;

      // Clear chart on error
      QMetaObject::invokeMethod(
          this, [this]() { setCandleData(QVector<ChartData::Candle>()); },
          Qt::QueuedConnection);
    }
  });
}

void IndicatorChartWidget::setCandleData(
    const QVector<ChartData::Candle> &candles) {
  m_candles = candles;
  updatePriceChart();
  recalculateAllIndicators();

  qDebug() << "[IndicatorChart] Set" << candles.size() << "candles";
}

void IndicatorChartWidget::appendCandle(const ChartData::Candle &candle) {
  m_candles.append(candle);

  // Add to candlestick series
  QCandlestickSet *set = new QCandlestickSet(candle.timestamp);
  set->setOpen(candle.open);
  set->setHigh(candle.high);
  set->setLow(candle.low);
  set->setClose(candle.close);
  m_candlestickSeries->append(set);

  // Update indicators (only recalculate affected portion)
  for (auto &info : m_indicators) {
    updateIndicator(info.name);
  }

  if (m_autoScale) {
    updateAxisRanges();
  }
}

void IndicatorChartWidget::updateCurrentCandle(
    const ChartData::Candle &candle) {
  if (m_candles.isEmpty())
    return;

  // Update last candle
  m_candles.last() = candle;

  // Update candlestick series
  auto sets = m_candlestickSeries->sets();
  if (!sets.isEmpty()) {
    QCandlestickSet *lastSet = sets.last();
    lastSet->setOpen(candle.open);
    lastSet->setHigh(candle.high);
    lastSet->setLow(candle.low);
    lastSet->setClose(candle.close);
  }

  // Update indicators
  for (auto &info : m_indicators) {
    updateIndicator(info.name);
  }
}

void IndicatorChartWidget::updatePriceChart() {
  // Clear existing data
  m_candlestickSeries->clear();

  if (m_candles.isEmpty()) {
    qWarning() << "[IndicatorChart] No candles to display";
    return;
  }

  // Determine visible range
  int startIdx = qMax(0, m_candles.size() - m_visibleCandleCount);
  int endIdx = m_candles.size();

  // Add candlesticks
  for (int i = startIdx; i < endIdx; i++) {
    const auto &candle = m_candles[i];

    QCandlestickSet *set = new QCandlestickSet(candle.timestamp);
    set->setOpen(candle.open);
    set->setHigh(candle.high);
    set->setLow(candle.low);
    set->setClose(candle.close);

    m_candlestickSeries->append(set);
  }

  updateAxisRanges();

  qDebug() << "[IndicatorChart] Updated price chart with" << (endIdx - startIdx)
           << "candles";
}

void IndicatorChartWidget::updateAxisRanges() {
  if (m_candles.isEmpty())
    return;

  int startIdx = qMax(0, m_candles.size() - m_visibleCandleCount);
  int endIdx = m_candles.size();

  // Find min/max prices in visible range
  double minPrice = std::numeric_limits<double>::max();
  double maxPrice = std::numeric_limits<double>::min();

  for (int i = startIdx; i < endIdx; i++) {
    minPrice = qMin(minPrice, m_candles[i].low);
    maxPrice = qMax(maxPrice, m_candles[i].high);
  }

  // Add 5% padding
  double padding = (maxPrice - minPrice) * 0.05;
  minPrice -= padding;
  maxPrice += padding;

  // Update axes
  m_axisYPrice->setRange(minPrice, maxPrice);

  // Update time axis
  QDateTime startTime =
      QDateTime::fromSecsSinceEpoch(m_candles[startIdx].timestamp);
  QDateTime endTime =
      QDateTime::fromSecsSinceEpoch(m_candles[endIdx - 1].timestamp);
  m_axisX->setRange(startTime, endTime);
}

void IndicatorChartWidget::addOverlayIndicator(const QString &name,
                                               const QString &type,
                                               const QVariantMap &params) {
  if (m_indicators.contains(name)) {
    qWarning() << "[IndicatorChart] Indicator" << name << "already exists";
    return;
  }

  IndicatorInfo info;
  info.name = name;
  info.type = type;
  info.params = params;
  info.isOverlay = true;

  // Create line series for overlay indicators
  if (type == "SMA" || type == "EMA" || type == "WMA") {
    info.series1 = new QLineSeries();
    info.series1->setName(name);
    m_priceChart->addSeries(info.series1);
    info.series1->attachAxis(m_axisX);
    info.series1->attachAxis(m_axisYPrice);
  } else if (type == "BB") {
    // Bollinger Bands: upper, middle, lower
    info.series1 = new QLineSeries();
    info.series1->setName(name + " Upper");
    info.series2 = new QLineSeries();
    info.series2->setName(name + " Middle");
    info.series3 = new QLineSeries();
    info.series3->setName(name + " Lower");

    m_priceChart->addSeries(info.series1);
    m_priceChart->addSeries(info.series2);
    m_priceChart->addSeries(info.series3);

    info.series1->attachAxis(m_axisX);
    info.series1->attachAxis(m_axisYPrice);
    info.series2->attachAxis(m_axisX);
    info.series2->attachAxis(m_axisYPrice);
    info.series3->attachAxis(m_axisX);
    info.series3->attachAxis(m_axisYPrice);

    // Apply styling
    info.series1->setColor(QColor("#2196f3"));
    info.series2->setColor(QColor("#9c27b0"));
    info.series3->setColor(QColor("#2196f3"));
  }

  m_indicators[name] = info;
  calculateOverlayIndicator(info);

  emit indicatorAdded(name, type);
  qDebug() << "[IndicatorChart] Added overlay indicator:" << name << "(" << type
           << ")";
}

void IndicatorChartWidget::addPanelIndicator(const QString &name,
                                             const QString &type,
                                             const QVariantMap &params) {
  if (m_indicators.contains(name)) {
    qWarning() << "[IndicatorChart] Indicator" << name << "already exists";
    return;
  }

  IndicatorInfo info;
  info.name = name;
  info.type = type;
  info.params = params;
  info.isOverlay = false;

  // Create separate chart for panel indicators
  info.chart = new QChart();
  info.chart->setTitle(name);
  info.chart->setAnimationOptions(QChart::NoAnimation);
  info.chart->legend()->setVisible(true);
  info.chart->legend()->setAlignment(Qt::AlignBottom);
  applyDarkTheme(info.chart);

  // Create series
  if (type == "RSI" || type == "CCI" || type == "ATR") {
    info.series1 = new QLineSeries();
    info.series1->setName(name);
    info.chart->addSeries(info.series1);
  } else if (type == "MACD") {
    info.series1 = new QLineSeries();
    info.series1->setName("MACD");
    info.series2 = new QLineSeries();
    info.series2->setName("Signal");

    info.chart->addSeries(info.series1);
    info.chart->addSeries(info.series2);

    info.series1->setColor(QColor("#2196f3"));
    info.series2->setColor(QColor("#f44336"));
  } else if (type == "STOCH") {
    info.series1 = new QLineSeries();
    info.series1->setName("%K");
    info.series2 = new QLineSeries();
    info.series2->setName("%D");

    info.chart->addSeries(info.series1);
    info.chart->addSeries(info.series2);

    info.series1->setColor(QColor("#2196f3"));
    info.series2->setColor(QColor("#f44336"));
  }

  // Create axes for panel chart
  QDateTimeAxis *axisX = new QDateTimeAxis();
  axisX->setFormat("dd MMM hh:mm");
  info.chart->addAxis(axisX, Qt::AlignBottom);

  QValueAxis *axisY = new QValueAxis();
  axisY->setLabelFormat("%.2f");
  info.chart->addAxis(axisY, Qt::AlignLeft);

  // Attach series to axes
  if (info.series1) {
    info.series1->attachAxis(axisX);
    info.series1->attachAxis(axisY);
  }
  if (info.series2) {
    info.series2->attachAxis(axisX);
    info.series2->attachAxis(axisY);
  }

  // Create chart view
  info.chartView = new QChartView(info.chart);
  info.chartView->setRenderHint(QPainter::Antialiasing);
  info.chartView->setRubberBand(QChartView::HorizontalRubberBand);

  m_mainLayout->addWidget(info.chartView,
                          1); // Stretch factor 1 (smaller than price chart)

  m_indicators[name] = info;
  calculatePanelIndicator(info);

  emit indicatorAdded(name, type);
  qDebug() << "[IndicatorChart] Added panel indicator:" << name << "(" << type
           << ")";
}

void IndicatorChartWidget::calculateOverlayIndicator(IndicatorInfo &info) {
  if (m_candles.isEmpty() || !TALibIndicators::isAvailable())
    return;

  // Extract price data
  auto prices = TALibIndicators::extractPrices(m_candles);

  int startIdx = qMax(0, m_candles.size() - m_visibleCandleCount);

  if (info.type == "SMA") {
    int period = info.params.value("period", 20).toInt();
    auto sma = TALibIndicators::calculateSMA(prices.closes, period);

    if (info.series1) {
      info.series1->clear();
      for (int i = startIdx; i < sma.size(); i++) {
        if (sma[i] > 0) {
          QDateTime time =
              QDateTime::fromSecsSinceEpoch(m_candles[i].timestamp);
          info.series1->append(time.toMSecsSinceEpoch(), sma[i]);
        }
      }
    }
  } else if (info.type == "EMA") {
    int period = info.params.value("period", 20).toInt();
    auto ema = TALibIndicators::calculateEMA(prices.closes, period);

    if (info.series1) {
      info.series1->clear();
      for (int i = startIdx; i < ema.size(); i++) {
        if (ema[i] > 0) {
          QDateTime time =
              QDateTime::fromSecsSinceEpoch(m_candles[i].timestamp);
          info.series1->append(time.toMSecsSinceEpoch(), ema[i]);
        }
      }
    }
  } else if (info.type == "BB") {
    int period = info.params.value("period", 20).toInt();
    double nbDevUp = info.params.value("nbDevUp", 2.0).toDouble();
    double nbDevDn = info.params.value("nbDevDn", 2.0).toDouble();

    auto bb = TALibIndicators::calculateBollingerBands(prices.closes, period,
                                                       nbDevUp, nbDevDn);

    if (info.series1 && info.series2 && info.series3) {
      info.series1->clear();
      info.series2->clear();
      info.series3->clear();

      for (int i = startIdx; i < bb.upperBand.size(); i++) {
        if (bb.middleBand[i] > 0) {
          QDateTime time =
              QDateTime::fromSecsSinceEpoch(m_candles[i].timestamp);
          qint64 msec = time.toMSecsSinceEpoch();
          info.series1->append(msec, bb.upperBand[i]);
          info.series2->append(msec, bb.middleBand[i]);
          info.series3->append(msec, bb.lowerBand[i]);
        }
      }
    }
  }
}

void IndicatorChartWidget::calculatePanelIndicator(IndicatorInfo &info) {
  if (m_candles.isEmpty() || !TALibIndicators::isAvailable())
    return;

  auto prices = TALibIndicators::extractPrices(m_candles);

  int startIdx = qMax(0, m_candles.size() - m_visibleCandleCount);

  if (info.type == "RSI") {
    int period = info.params.value("period", 14).toInt();
    auto rsi = TALibIndicators::calculateRSI(prices.closes, period);

    if (info.series1) {
      info.series1->clear();
      for (int i = startIdx; i < rsi.size(); i++) {
        if (rsi[i] > 0) {
          QDateTime time =
              QDateTime::fromSecsSinceEpoch(m_candles[i].timestamp);
          info.series1->append(time.toMSecsSinceEpoch(), rsi[i]);
        }
      }
    }

    // Set RSI axis range (0-100)
    if (info.chart) {
      auto axes = info.chart->axes(Qt::Vertical);
      if (!axes.isEmpty()) {
        QValueAxis *axisY = qobject_cast<QValueAxis *>(axes.first());
        if (axisY) {
          axisY->setRange(0, 100);
        }
      }
    }
  } else if (info.type == "MACD") {
    int fastPeriod = info.params.value("fastPeriod", 12).toInt();
    int slowPeriod = info.params.value("slowPeriod", 26).toInt();
    int signalPeriod = info.params.value("signalPeriod", 9).toInt();

    auto macd = TALibIndicators::calculateMACD(prices.closes, fastPeriod,
                                               slowPeriod, signalPeriod);

    if (info.series1 && info.series2) {
      info.series1->clear();
      info.series2->clear();

      for (int i = startIdx; i < macd.macdLine.size(); i++) {
        if (macd.macdLine[i] != 0 || macd.signalLine[i] != 0) {
          QDateTime time =
              QDateTime::fromSecsSinceEpoch(m_candles[i].timestamp);
          qint64 msec = time.toMSecsSinceEpoch();
          info.series1->append(msec, macd.macdLine[i]);
          info.series2->append(msec, macd.signalLine[i]);
        }
      }
    }
  } else if (info.type == "STOCH") {
    auto stoch = TALibIndicators::calculateStochastic(prices.highs, prices.lows,
                                                      prices.closes);

    if (info.series1 && info.series2) {
      info.series1->clear();
      info.series2->clear();

      for (int i = startIdx; i < stoch.slowK.size(); i++) {
        if (stoch.slowK[i] > 0) {
          QDateTime time =
              QDateTime::fromSecsSinceEpoch(m_candles[i].timestamp);
          qint64 msec = time.toMSecsSinceEpoch();
          info.series1->append(msec, stoch.slowK[i]);
          info.series2->append(msec, stoch.slowD[i]);
        }
      }
    }

    // Set Stochastic axis range (0-100)
    if (info.chart) {
      auto axes = info.chart->axes(Qt::Vertical);
      if (!axes.isEmpty()) {
        QValueAxis *axisY = qobject_cast<QValueAxis *>(axes.first());
        if (axisY) {
          axisY->setRange(0, 100);
        }
      }
    }
  } else if (info.type == "ATR") {
    int period = info.params.value("period", 14).toInt();
    auto atr = TALibIndicators::calculateATR(prices.highs, prices.lows,
                                             prices.closes, period);

    if (info.series1) {
      info.series1->clear();
      for (int i = startIdx; i < atr.size(); i++) {
        if (atr[i] > 0) {
          QDateTime time =
              QDateTime::fromSecsSinceEpoch(m_candles[i].timestamp);
          info.series1->append(time.toMSecsSinceEpoch(), atr[i]);
        }
      }
    }
  }
}

void IndicatorChartWidget::updateIndicator(const QString &name) {
  if (!m_indicators.contains(name))
    return;

  auto &info = m_indicators[name];

  if (info.isOverlay) {
    calculateOverlayIndicator(info);
  } else {
    calculatePanelIndicator(info);
  }
}

void IndicatorChartWidget::recalculateAllIndicators() {
  for (auto &info : m_indicators) {
    if (info.isOverlay) {
      calculateOverlayIndicator(info);
    } else {
      calculatePanelIndicator(info);
    }
  }
}

void IndicatorChartWidget::removeIndicator(const QString &name) {
  if (!m_indicators.contains(name))
    return;

  auto info = m_indicators.take(name);

  // Remove series from chart
  if (info.series1) {
    if (info.isOverlay) {
      m_priceChart->removeSeries(info.series1);
    }
    delete info.series1;
  }
  if (info.series2) {
    if (info.isOverlay) {
      m_priceChart->removeSeries(info.series2);
    }
    delete info.series2;
  }
  if (info.series3) {
    if (info.isOverlay) {
      m_priceChart->removeSeries(info.series3);
    }
    delete info.series3;
  }

  // Remove panel chart
  if (info.chartView) {
    m_mainLayout->removeWidget(info.chartView);
    delete info.chartView;
  }
  if (info.chart) {
    delete info.chart;
  }

  emit indicatorRemoved(name);
  qDebug() << "[IndicatorChart] Removed indicator:" << name;
}

void IndicatorChartWidget::clearIndicators() {
  QStringList names = m_indicators.keys();
  for (const auto &name : names) {
    removeIndicator(name);
  }
}

void IndicatorChartWidget::setAutoScale(bool enabled) {
  m_autoScale = enabled;
  if (enabled) {
    updateAxisRanges();
  }
}

void IndicatorChartWidget::setVisibleRange(int candleCount) {
  m_visibleCandleCount = candleCount;
  updatePriceChart();
  recalculateAllIndicators();
}

// ============================================================================
// SLOTS
// ============================================================================

void IndicatorChartWidget::onSymbolEntered() {
  QString symbol = m_symbolInput->text().trimmed().toUpper();
  if (symbol.isEmpty()) {
    qWarning() << "[IndicatorChart] Empty symbol entered";
    return;
  }

  qDebug() << "[IndicatorChart] Symbol change requested:" << symbol;
  emit symbolChangeRequested(symbol);
}

void IndicatorChartWidget::onAddIndicatorClicked() {
  QString type = m_indicatorSelector->currentData().toString();
  QString name = m_indicatorSelector->currentText();

  // Create parameter dialog
  QDialog dialog(this);
  dialog.setWindowTitle("Add " + name);
  QFormLayout *form = new QFormLayout(&dialog);

  QSpinBox *periodSpin = nullptr;
  QSpinBox *fastSpin = nullptr;
  QSpinBox *slowSpin = nullptr;
  QSpinBox *signalSpin = nullptr;

  QVariantMap params;

  if (type == "SMA" || type == "EMA") {
    periodSpin = new QSpinBox();
    periodSpin->setRange(1, 200);
    periodSpin->setValue(20);
    form->addRow("Period:", periodSpin);
    params["period"] = 20;
  } else if (type == "BB") {
    periodSpin = new QSpinBox();
    periodSpin->setRange(1, 200);
    periodSpin->setValue(20);
    form->addRow("Period:", periodSpin);
    params["period"] = 20;
    params["nbDevUp"] = 2.0;
    params["nbDevDn"] = 2.0;
  } else if (type == "RSI" || type == "ATR" || type == "CCI") {
    periodSpin = new QSpinBox();
    periodSpin->setRange(1, 200);
    periodSpin->setValue(14);
    form->addRow("Period:", periodSpin);
    params["period"] = 14;
  } else if (type == "MACD") {
    fastSpin = new QSpinBox();
    fastSpin->setRange(1, 100);
    fastSpin->setValue(12);
    form->addRow("Fast Period:", fastSpin);

    slowSpin = new QSpinBox();
    slowSpin->setRange(1, 100);
    slowSpin->setValue(26);
    form->addRow("Slow Period:", slowSpin);

    signalSpin = new QSpinBox();
    signalSpin->setRange(1, 100);
    signalSpin->setValue(9);
    form->addRow("Signal Period:", signalSpin);

    params["fastPeriod"] = 12;
    params["slowPeriod"] = 26;
    params["signalPeriod"] = 9;
  }

  QDialogButtonBox *buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  form->addRow(buttonBox);

  if (dialog.exec() == QDialog::Accepted) {
    // Update params from dialog
    if (periodSpin)
      params["period"] = periodSpin->value();
    if (fastSpin)
      params["fastPeriod"] = fastSpin->value();
    if (slowSpin)
      params["slowPeriod"] = slowSpin->value();
    if (signalSpin)
      params["signalPeriod"] = signalSpin->value();

    // Generate unique name
    QString uniqueName = name;
    int counter = 1;
    while (m_indicators.contains(uniqueName)) {
      uniqueName = QString("%1 (%2)").arg(name).arg(counter++);
    }

    // Add indicator
    if (type == "SMA" || type == "EMA" || type == "WMA" || type == "BB") {
      addOverlayIndicator(uniqueName, type, params);
    } else {
      addPanelIndicator(uniqueName, type, params);
    }
  }
}

void IndicatorChartWidget::onRemoveIndicatorClicked() {
  if (m_indicators.isEmpty()) {
    qDebug() << "[IndicatorChart] No indicators to remove";
    return;
  }

  // For simplicity, remove the first indicator (could add selection dialog)
  QString name = m_indicators.keys().first();
  removeIndicator(name);
}

void IndicatorChartWidget::onZoomInClicked() {
  m_visibleCandleCount = qMax(10, m_visibleCandleCount - 20);
  setVisibleRange(m_visibleCandleCount);
}

void IndicatorChartWidget::onZoomOutClicked() {
  m_visibleCandleCount = qMin(500, m_visibleCandleCount + 20);
  setVisibleRange(m_visibleCandleCount);
}

void IndicatorChartWidget::onResetZoomClicked() {
  m_visibleCandleCount = 100;
  setVisibleRange(m_visibleCandleCount);
}

// ============================================================================
// STYLING
// ============================================================================

void IndicatorChartWidget::applyDarkTheme(QChart *chart) {
  chart->setBackgroundBrush(QBrush(QColor("#131722")));
  chart->setTitleBrush(QBrush(QColor("#d1d4dc")));
  chart->setPlotAreaBackgroundBrush(QBrush(QColor("#1e222d")));
  chart->setPlotAreaBackgroundVisible(true);

  // Style legend
  if (chart->legend()) {
    chart->legend()->setLabelColor(QColor("#d1d4dc"));
    chart->legend()->setBackgroundVisible(false);
  }
}

void IndicatorChartWidget::applyAxisStyling(QAbstractAxis *axis) {
  axis->setLabelsColor(QColor("#d1d4dc"));
  axis->setTitleBrush(QBrush(QColor("#d1d4dc")));
  axis->setGridLineColor(QColor("#363c4e"));
}
void IndicatorChartWidget::onGlobalSearchClicked() {
  QDialog dialog(this);
  dialog.setWindowTitle("Global Script Search");
  dialog.setMinimumSize(800, 600);

  auto *layout = new QVBoxLayout(&dialog);
  auto *searchWidget = new GlobalSearchWidget(&dialog);
  layout->addWidget(searchWidget);

  connect(
      searchWidget, &GlobalSearchWidget::scripSelected, this,
      [this, &dialog](const ContractData &contract) {
        // Determine segment based on token range and instrument type
        int segmentInt = (contract.exchangeInstrumentID >= 11000000) ? 11 : 1;
        if (contract.strikePrice > 0 || contract.instrumentType == 1) {
          segmentInt = (contract.exchangeInstrumentID >= 11000000) ? 12 : 2;
        }

        loadSymbol(contract.name, segmentInt, contract.exchangeInstrumentID);
        dialog.accept();
      });

  dialog.exec();
}
