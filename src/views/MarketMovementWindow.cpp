#include "views/MarketMovementWindow.h"
#include "utils/ConfigLoader.h"
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>


// Helper function to load config
static void loadXTSConfig(QString &baseUrl, QString &authToken) {
  ConfigLoader config;
  if (config.load("configs/config.ini")) {
    baseUrl = config.getXTSUrl();
    authToken = config.getMDToken();
    qDebug() << "[MarketMovementWindow] XTS Config loaded - URL:" << baseUrl;
  } else {
    qWarning()
        << "[MarketMovementWindow] Failed to load config, using defaults";
    baseUrl = "http://192.168.102.9:3000";
  }
}

MarketMovementWindow::MarketMovementWindow(QWidget *parent)
    : QWidget(parent), m_mainLayout(nullptr), m_headerLayout(nullptr),
      m_titleLabel(nullptr), m_instrumentLabel(nullptr), m_statusLabel(nullptr),
      m_table(nullptr), m_currentSegment(0), m_currentToken(0),
      m_networkManager(nullptr), m_isLoadingHistorical(false) {
  loadXTSConfig(m_xtsBaseUrl, m_xtsAuthToken);
  setupUI();
  setupConnections();
  setupNetworkManager();
}

MarketMovementWindow::MarketMovementWindow(const WindowContext &context,
                                           QWidget *parent)
    : QWidget(parent), m_mainLayout(nullptr), m_headerLayout(nullptr),
      m_titleLabel(nullptr), m_instrumentLabel(nullptr), m_statusLabel(nullptr),
      m_table(nullptr), m_context(context), m_currentSegment(0),
      m_currentToken(0), m_networkManager(nullptr),
      m_isLoadingHistorical(false) {
  loadXTSConfig(m_xtsBaseUrl, m_xtsAuthToken);
  setupUI();
  setupConnections();
  setupNetworkManager();

  // Set instrument and subscribe to candle updates
  if (m_context.isValid()) {
    m_currentSegment = getSegmentFromExchange(m_context.exchange);
    m_currentToken = m_context.token;
    updateHeader();

    // Fetch historical OHLC data from XTS API
    fetchHistoricalOHLC();
  }
}

MarketMovementWindow::~MarketMovementWindow() {
  // Cleanup network manager
  if (m_networkManager) {
    m_networkManager->deleteLater();
  }
}

void MarketMovementWindow::setupUI() {
  // Main layout
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(10, 10, 10, 10);
  m_mainLayout->setSpacing(10);

  // Header layout
  m_headerLayout = new QHBoxLayout();

  m_titleLabel = new QLabel("Market Movement for OPTSTK", this);
  QFont titleFont = m_titleLabel->font();
  titleFont.setBold(true);
  titleFont.setPointSize(12);
  m_titleLabel->setFont(titleFont);

  m_instrumentLabel = new QLabel("No instrument selected", this);
  m_instrumentLabel->setStyleSheet("color: #888888;");

  m_statusLabel = new QLabel("", this);
  m_statusLabel->setStyleSheet("color: #4ec9b0; font-style: italic;");

  // Refresh button
  m_refreshButton = new QPushButton("Refresh", this);
  m_refreshButton->setStyleSheet("QPushButton {"
                                 "    background-color: #0e639c;"
                                 "    color: white;"
                                 "    border: none;"
                                 "    padding: 5px 15px;"
                                 "    border-radius: 3px;"
                                 "}"
                                 "QPushButton:hover {"
                                 "    background-color: #1177bb;"
                                 "}"
                                 "QPushButton:pressed {"
                                 "    background-color: #0d5a8f;"
                                 "}");
  connect(m_refreshButton, &QPushButton::clicked, this,
          &MarketMovementWindow::onRefreshClicked);

  m_headerLayout->addWidget(m_titleLabel);
  m_headerLayout->addWidget(m_statusLabel);
  m_headerLayout->addStretch();
  m_headerLayout->addWidget(m_refreshButton);
  m_headerLayout->addWidget(m_instrumentLabel);

  m_mainLayout->addLayout(m_headerLayout);

  // Create table
  setupTable();

  m_mainLayout->addWidget(m_table);

  setLayout(m_mainLayout);
}

void MarketMovementWindow::setupTable() {
  m_table = new QTableWidget(0, COL_COUNT, this);

  // Set column headers
  QStringList headers;
  headers << "Time" << "Open" << "High" << "Low" << "Close"
          << "Volume" << "Average";
  m_table->setHorizontalHeaderLabels(headers);

  // Table styling
  m_table->setStyleSheet("QTableWidget {"
                         "    background-color: #1e1e1e;"
                         "    color: #ffffff;"
                         "    gridline-color: #3e3e42;"
                         "    border: 1px solid #3e3e42;"
                         "}"
                         "QTableWidget::item {"
                         "    padding: 5px;"
                         "    border-bottom: 1px solid #2d2d30;"
                         "}"
                         "QTableWidget::item:selected {"
                         "    background-color: #094771;"
                         "}"
                         "QHeaderView::section {"
                         "    background-color: #2d2d30;"
                         "    color: #ffffff;"
                         "    padding: 6px;"
                         "    border: 1px solid #3e3e42;"
                         "    font-weight: bold;"
                         "}");

  // Table properties
  m_table->setAlternatingRowColors(false); // Disable to avoid white backgrounds
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setStretchLastSection(true);

  // Ensure all rows have dark background
  m_table->setStyleSheet(m_table->styleSheet() +
                         "QTableWidget::item { background-color: #1e1e1e; }");

  // Column widths
  m_table->setColumnWidth(COL_TIME, 120);
  m_table->setColumnWidth(COL_OPEN, 100);
  m_table->setColumnWidth(COL_HIGH, 100);
  m_table->setColumnWidth(COL_LOW, 100);
  m_table->setColumnWidth(COL_CLOSE, 100);
  m_table->setColumnWidth(COL_VOLUME, 120);
  m_table->setColumnWidth(COL_AVERAGE, 120);
}

void MarketMovementWindow::setupConnections() {
  // Auto-refresh every 60 seconds (configurable)
  m_autoRefreshTimer = new QTimer(this);
  connect(m_autoRefreshTimer, &QTimer::timeout, this, [this]() {
    if (m_currentToken > 0 && !m_isLoadingHistorical) {
      qDebug()
          << "[MarketMovementWindow] Auto-refreshing OHLC data from XTS API";
      fetchHistoricalOHLC();
    }
  });
  m_autoRefreshTimer->start(60000); // Refresh every 60 seconds
}

void MarketMovementWindow::setInstrument(const WindowContext &context) {
  if (!context.isValid()) {
    qDebug() << "[MarketMovementWindow] Invalid context provided";
    return;
  }

  // Update context
  m_context = context;
  m_currentSegment = getSegmentFromExchange(m_context.exchange);
  m_currentToken = m_context.token;

  // Update header
  updateHeader();

  // Clear table
  m_table->setRowCount(0);

  // Fetch historical OHLC data from XTS API
  qDebug() << "[MarketMovementWindow] Fetching OHLC for token" << m_currentToken
           << "segment" << m_currentSegment;
  fetchHistoricalOHLC();
}

void MarketMovementWindow::updateHeader() {
  if (!m_context.isValid()) {
    m_instrumentLabel->setText("No instrument selected");
    return;
  }

  QString instrumentText =
      QString("%1 - %2 %3")
          .arg(m_context.symbol)
          .arg(m_context.expiry.isEmpty() ? "" : m_context.expiry)
          .arg(m_context.strikePrice > 0
                   ? QString::number(m_context.strikePrice) + " " +
                         m_context.optionType
                   : "");

  m_instrumentLabel->setText(instrumentText);
  m_instrumentLabel->setStyleSheet("color: #4ec9b0; font-weight: bold;");
}

int MarketMovementWindow::getSegmentFromExchange(
    const QString &exchange) const {
  if (exchange == "NSECM")
    return 1;
  if (exchange == "NSEFO")
    return 2;
  if (exchange == "BSECM")
    return 11;
  if (exchange == "BSEFO")
    return 12;
  return 2; // Default to NSEFO
}

void MarketMovementWindow::onRefreshClicked() {
  if (m_currentToken > 0 && !m_isLoadingHistorical) {
    qDebug() << "[MarketMovementWindow] Manual refresh triggered";
    fetchHistoricalOHLC();
  } else if (m_isLoadingHistorical) {
    qDebug() << "[MarketMovementWindow] Refresh skipped - already loading";
  } else {
    qDebug()
        << "[MarketMovementWindow] Refresh skipped - no instrument selected";
  }
}

// Removed: CandleAggregator integration - using XTS API only
void MarketMovementWindow::onCandleCompleted(quint32 token,
                                             const CandleData &candle) {
  qDebug()
      << "[MarketMovementWindow] Candle completed signal received for token"
      << token << "(current:" << m_currentToken << ")"
      << "Time:" << candle.getFormattedTime();

  // Only process candles for our token
  if (token != m_currentToken) {
    return;
  }

  qDebug() << "[MarketMovementWindow] Candle completed for token:" << token
           << "Time:" << candle.getFormattedTime();

  // Check if this candle already exists in the table (by time)
  QString candleTime = candle.getFormattedTime();
  for (int row = 0; row < m_table->rowCount(); ++row) {
    QTableWidgetItem *timeItem = m_table->item(row, COL_TIME);
    if (timeItem && timeItem->text() == candleTime) {
      // Candle already exists, don't add duplicate
      return;
    }
  }

  // Add new candle at top
  addCandleToTable(candle);

  // Limit to 10 rows
  while (m_table->rowCount() > 10) {
    m_table->removeRow(10);
  }
}

void MarketMovementWindow::onCandleUpdated(quint32 token,
                                           const CandleData &candle) {
  // Only process candles for our token
  if (token != m_currentToken) {
    return;
  }

  qDebug() << "[MarketMovementWindow] Candle updated for token" << token
           << "Time:" << candle.getFormattedTime() << "OHLC:" << candle.open
           << candle.high << candle.low << candle.close;

  QString candleTime = candle.getFormattedTime();

  // Check if this candle already exists in the table
  if (m_table->rowCount() > 0) {
    QTableWidgetItem *timeItem = m_table->item(0, COL_TIME);

    if (timeItem && timeItem->text() == candleTime) {
      // Update existing row 0
      m_table->item(0, COL_HIGH)->setText(QString::number(candle.high, 'f', 2));
      m_table->item(0, COL_LOW)->setText(QString::number(candle.low, 'f', 2));
      m_table->item(0, COL_CLOSE)
          ->setText(QString::number(candle.close, 'f', 2));
      m_table->item(0, COL_VOLUME)->setText(QString::number(candle.volume));
      m_table->item(0, COL_AVERAGE)
          ->setText(QString::number(candle.average, 'f', 2));
      return;
    }
  }

  // New minute - add new row at top
  addCandleToTable(candle);

  // Limit to 500 rows (full trading day + buffer)
  while (m_table->rowCount() > 500) {
    m_table->removeRow(500);
  }
}

void MarketMovementWindow::addCandleToTable(const CandleData &candle) {
  if (!candle.isValid()) {
    return;
  }

  // Insert at top (row 0) so newest data appears first
  m_table->insertRow(0);

  // Dark background color for all items
  QColor darkBg(30, 30, 30);

  // Time
  QTableWidgetItem *timeItem = new QTableWidgetItem(candle.getFormattedTime());
  timeItem->setBackground(QBrush(darkBg));
  m_table->setItem(0, COL_TIME, timeItem);

  // OHLC
  QTableWidgetItem *openItem =
      new QTableWidgetItem(QString::number(candle.open, 'f', 2));
  openItem->setBackground(QBrush(darkBg));
  m_table->setItem(0, COL_OPEN, openItem);

  QTableWidgetItem *highItem =
      new QTableWidgetItem(QString::number(candle.high, 'f', 2));
  highItem->setBackground(QBrush(darkBg));
  m_table->setItem(0, COL_HIGH, highItem);

  QTableWidgetItem *lowItem =
      new QTableWidgetItem(QString::number(candle.low, 'f', 2));
  lowItem->setBackground(QBrush(darkBg));
  m_table->setItem(0, COL_LOW, lowItem);

  QTableWidgetItem *closeItem =
      new QTableWidgetItem(QString::number(candle.close, 'f', 2));
  closeItem->setBackground(QBrush(darkBg));
  m_table->setItem(0, COL_CLOSE, closeItem);

  // Volume and Average
  QTableWidgetItem *volumeItem =
      new QTableWidgetItem(QString::number(candle.volume));
  volumeItem->setBackground(QBrush(darkBg));
  m_table->setItem(0, COL_VOLUME, volumeItem);

  QTableWidgetItem *avgItem =
      new QTableWidgetItem(QString::number(candle.average, 'f', 2));
  avgItem->setBackground(QBrush(darkBg));
  m_table->setItem(0, COL_AVERAGE, avgItem);

  // Center align all cells
  for (int col = 0; col < COL_COUNT; ++col) {
    if (m_table->item(0, col)) {
      m_table->item(0, col)->setTextAlignment(Qt::AlignCenter);
    }
  }
}

void MarketMovementWindow::setupNetworkManager() {
  m_networkManager = new QNetworkAccessManager(this);
  connect(m_networkManager, &QNetworkAccessManager::finished, this,
          &MarketMovementWindow::onHistoricalDataReceived);
}

void MarketMovementWindow::fetchHistoricalOHLC() {
  if (m_currentToken == 0 || m_currentSegment == 0) {
    qDebug() << "[MarketMovementWindow] Invalid token or segment for "
                "historical fetch";
    return;
  }

  if (m_isLoadingHistorical) {
    qDebug() << "[MarketMovementWindow] Already loading historical data";
    return;
  }

  m_isLoadingHistorical = true;
  m_statusLabel->setText("Loading historical data...");

  // Calculate time range: 9:00 AM today to now
  QDateTime now = QDateTime::currentDateTime();
  QDateTime startTime = QDateTime(now.date(), QTime(9, 0, 0));

  // If current time is before 9 AM, use yesterday
  if (now.time() < QTime(9, 0, 0)) {
    startTime = startTime.addDays(-1);
  }

  // Format times for XTS API: "MMM dd yyyy HHmmss" (e.g., "Feb 26 2026 090000")
  QString startTimeStr = startTime.toString("MMM dd yyyy HHmmss");
  QString endTimeStr = now.toString("MMM dd yyyy HHmmss");

  qDebug() << "[MarketMovementWindow] Fetching OHLC from" << startTimeStr
           << "to" << endTimeStr;
  qDebug() << "[MarketMovementWindow] Token:" << m_currentToken
           << "Segment:" << m_currentSegment;

  // Build XTS API URL - matches curl command exactly
  QString endpoint = QString("/marketdata/instruments/ohlc");

  QUrl url(m_xtsBaseUrl + endpoint);
  QUrlQuery query;
  query.addQueryItem("exchangeSegment", QString::number(m_currentSegment));
  query.addQueryItem("exchangeInstrumentID", QString::number(m_currentToken));
  query.addQueryItem("startTime", startTimeStr);
  query.addQueryItem("endTime", endTimeStr);
  query.addQueryItem("compressionValue", "1"); // 1 minute candles (default)
  url.setQuery(query);

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  // CRITICAL: No "Bearer " prefix - just raw token (matches curl command
  // exactly)
  if (!m_xtsAuthToken.isEmpty()) {
    request.setRawHeader("Authorization", m_xtsAuthToken.toUtf8());
    qDebug() << "[MarketMovementWindow] Auth token set (first 30 chars):"
             << m_xtsAuthToken.left(30);
  } else {
    qWarning() << "[MarketMovementWindow] No auth token available!";
  }

  qDebug() << "[MarketMovementWindow] Request URL:" << url.toString();

  m_networkManager->get(request);
}

void MarketMovementWindow::onHistoricalDataReceived(QNetworkReply *reply) {
  m_isLoadingHistorical = false;
  m_statusLabel->setText("");

  if (reply->error() != QNetworkReply::NoError) {
    QString errorMsg = QString("Failed to fetch historical data: %1")
                           .arg(reply->errorString());
    qDebug() << "[MarketMovementWindow]" << errorMsg;
    m_statusLabel->setText("Error loading data");
    m_statusLabel->setStyleSheet("color: #f48771; font-style: italic;");

    qDebug()
        << "[MarketMovementWindow] XTS API error - no fallback data available";

    reply->deleteLater();
    return;
  }

  // Parse JSON response
  QByteArray responseData = reply->readAll();
  qDebug() << "[MarketMovementWindow] Received response:"
           << responseData.left(500);

  QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
  if (!jsonDoc.isObject()) {
    qDebug() << "[MarketMovementWindow] Invalid JSON response";
    m_statusLabel->setText("Invalid data format");
    m_statusLabel->setStyleSheet("color: #f48771; font-style: italic;");
    reply->deleteLater();
    return;
  }

  QJsonObject jsonObj = jsonDoc.object();

  // XTS API response format: {"type": "success", "result": {"dataReponse":
  // [...]}}
  if (!jsonObj.contains("result") || jsonObj["type"].toString() != "success") {
    qDebug() << "[MarketMovementWindow] API returned error or no result";
    m_statusLabel->setText("No data available");
    m_statusLabel->setStyleSheet("color: #f48771; font-style: italic;");
    reply->deleteLater();
    return;
  }

  QJsonObject result = jsonObj["result"].toObject();
  QJsonArray dataArray = result["dataReponse"].toArray();

  if (dataArray.isEmpty()) {
    qDebug() << "[MarketMovementWindow] No candle data in response";
    m_statusLabel->setText("No candles found");
    m_statusLabel->setStyleSheet("color: #888888; font-style: italic;");
    reply->deleteLater();
    return;
  }

  qDebug() << "[MarketMovementWindow] Processing" << dataArray.size()
           << "candles";

  // Clear existing table
  m_table->setRowCount(0);

  // Parse and add candles (API returns oldest first, we want newest first)
  QList<CandleData> candles;
  for (const QJsonValue &value : dataArray) {
    QJsonObject candleObj = value.toObject();

    CandleData candle;
    // XTS API fields: Timestamp, Open, High, Low, Close, Volume, OpenInterest
    QString timestamp = candleObj["Timestamp"].toString();
    candle.open = candleObj["Open"].toDouble();
    candle.high = candleObj["High"].toDouble();
    candle.low = candleObj["Low"].toDouble();
    candle.close = candleObj["Close"].toDouble();
    candle.volume = candleObj["Volume"].toInt();

    // Calculate average
    if (candle.volume > 0) {
      candle.average = (candle.high + candle.low + candle.close) / 3.0;
    } else {
      candle.average = candle.close;
    }

    // Parse timestamp - XTS format might be Unix timestamp or ISO format
    QDateTime dateTime;
    bool isUnixTimestamp = false;
    qint64 unixTime = timestamp.toLongLong(&isUnixTimestamp);

    if (isUnixTimestamp && unixTime > 0) {
      dateTime = QDateTime::fromSecsSinceEpoch(unixTime);
    } else {
      dateTime = QDateTime::fromString(timestamp, Qt::ISODate);
    }

    if (!dateTime.isValid()) {
      qDebug() << "[MarketMovementWindow] Invalid timestamp:" << timestamp;
      continue;
    }

    candle.timestamp = dateTime;
    candles.append(candle);
  }

  // Add candles in reverse order (newest first)
  for (int i = candles.size() - 1; i >= 0; --i) {
    addCandleToTable(candles[i]);
  }

  m_statusLabel->setText(QString("Loaded %1 candles").arg(candles.size()));
  m_statusLabel->setStyleSheet("color: #4ec9b0; font-style: italic;");

  qDebug() << "[MarketMovementWindow] Successfully loaded" << candles.size()
           << "historical candles";

  // Auto-clear status after 3 seconds
  QTimer::singleShot(3000, this, [this]() { m_statusLabel->setText(""); });

  reply->deleteLater();
}
