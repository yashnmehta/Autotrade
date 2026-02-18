#ifndef TRADINGVIEWCHARTWIDGET_H
#define TRADINGVIEWCHARTWIDGET_H

#include "api/XTSTypes.h" // For XTS::Tick
#include "data/CandleData.h"
#include <QString>
#ifdef HAVE_TRADINGVIEW
#include <QWebChannel>
#include <QWebEngineView>
#endif
#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class TradingViewDataBridge;
class XTSMarketDataClient;

/**
 * @brief TradingView Advanced Charts integration widget
 *
 * Embeds TradingView Charting Library using QWebEngineView with custom
 * datafeed. Connects to HistoricalDataStore and CandleAggregator for real-time
 * data.
 *
 * Features:
 * - Professional charting with 100+ indicators
 * - Drawing tools and annotations
 * - Multiple timeframes and chart types
 * - Save/load chart layouts
 * - Order placement markers
 *
 * Requirements:
 * - Qt WebEngine (Chromium)
 * - TradingView Charting Library (commercial license)
 * - ~50-100 MB memory footprint
 *
 * Usage:
 * ```cpp
 * auto* chart = new TradingViewChartWidget(this);
 * chart->loadSymbol("NIFTY", 2, "5");  // 5-minute chart
 * layout->addWidget(chart);
 * ```
 */
class TradingViewChartWidget : public QWidget {
  Q_OBJECT

public:
  explicit TradingViewChartWidget(QWidget *parent = nullptr);
  ~TradingViewChartWidget();

  /**
   * @brief Set XTS Market Data Client for API access
   */
  void setXTSMarketDataClient(class XTSMarketDataClient *client);

  /**
   * @brief Set Repository Manager for symbol search
   */
  void setRepositoryManager(class RepositoryManager *repo) {
    m_repoManager = repo;
  }

  /**
   * @brief Load a symbol on the chart
   * @param symbol Trading symbol (e.g., "NIFTY", "BANKNIFTY")
   * @param segment Exchange segment (1=NSECM, 2=NSEFO, etc.)
   * @param token Exchange instrument ID (token)
   * @param interval Chart interval ("1", "5", "15", "60", "D", "W")
   */
  void loadSymbol(const QString &symbol, int segment, int64_t token,
                  const QString &interval = "5");

  /**
   * @brief Change chart interval/timeframe
   * @param interval Interval string ("1", "5", "15", "30", "60", "D", "W")
   */
  void setInterval(const QString &interval);

  /**
   * @brief Apply a theme to the chart
   * @param theme "Light" or "Dark"
   */
  void setTheme(const QString &theme);

  /**
   * @brief Add an indicator to the chart
   * @param indicatorName TradingView indicator name (e.g., "RSI", "MACD")
   */
  void addIndicator(const QString &indicatorName);

  /**
   * @brief Add an order marker on the chart
   * @param time Unix timestamp
   * @param price Price level
   * @param text Marker text (e.g., "BUY", "SELL")
   * @param color Marker color
   * @param shape "arrow_up", "arrow_down", "circle", etc.
   */
  void addOrderMarker(qint64 time, double price, const QString &text,
                      const QString &color = "#26a69a",
                      const QString &shape = "arrow_up");

  /**
   * @brief Execute JavaScript in the chart context
   * @param script JavaScript code to execute
   */
  void executeScript(const QString &script);

  /**
   * @brief Check if chart is ready
   * @return true if TradingView widget is initialized
   */
  bool isReady() const { return m_chartReady; }

signals:
  /**
   * @brief Emitted when chart is fully loaded and ready
   */
  void chartReady();

  /**
   * @brief Emitted when user clicks on the chart
   * @param time Unix timestamp at click position
   * @param price Price at click position
   */
  void chartClicked(qint64 time, double price);

  /**
   * @brief Emitted when user requests order placement
   * @param side "BUY" or "SELL"
   * @param price Order price
   */
  void orderRequested(const QString &side, double price);

  /**
   * @brief Emitted when user places order from chart dialog
   * @param params Complete order parameters
   */
  void orderRequestedFromChart(const XTS::OrderParams &params);

public slots:
  /**
   * @brief Handle new completed candle from CandleAggregator
   * @param symbol Trading symbol
   * @param segment Exchange segment
   * @param timeframe Timeframe string
   * @param candle Completed candle
   */
  void onCandleComplete(const QString &symbol, int segment,
                        const QString &timeframe,
                        const ChartData::Candle &candle);

  /**
   * @brief Handle partial candle update (real-time)
   * @param symbol Trading symbol
   * @param segment Exchange segment
   * @param timeframe Timeframe string
   * @param candle Partial candle (current state)
   */
  void onCandleUpdate(const QString &symbol, int segment,
                      const QString &timeframe,
                      const ChartData::Candle &candle);

  /**
   * @brief Handle real-time tick updates from XTS
   * @param tick Real-time tick data
   */
  void onTickUpdate(const XTS::Tick &tick);
  
  /**
   * @brief Handle 1-minute OHLC candle from XTS WebSocket (1505 event)
   * @param candle Completed 1-minute OHLC candle
   */
  void onCandleReceived(const XTS::OHLCCandle &candle);

private slots:
  void onLoadFinished(bool success);
  void onJavaScriptMessage(const QString &message);
  void onChartClickedInternal(qint64 time, double price);

private:
  void setupWebChannel();
  void loadChartHTML();
  QString convertTimeframeToInterval(const QString &timeframe) const;

#ifdef HAVE_TRADINGVIEW
  QWebEngineView *m_webView;
  QWebChannel *m_channel;
#else
  QWidget *m_webView; // Placeholder when QtWebEngine not available
  QObject *m_channel;
#endif
  TradingViewDataBridge *m_dataBridge;

  QString m_currentSymbol;
  int m_currentSegment = 0;
  int64_t m_currentToken = 0;
  QString m_currentInterval;
  bool m_chartReady = false;

  class XTSMarketDataClient *m_xtsClient = nullptr;
  class RepositoryManager *m_repoManager = nullptr;

  friend class TradingViewDataBridge; // Allow access to m_repoManager
};

/**
 * @brief Bridge object for C++ ↔ JavaScript communication
 *
 * Exposed to JavaScript via QWebChannel for bidirectional communication.
 */
class TradingViewDataBridge : public QObject {
  Q_OBJECT

public:
  explicit TradingViewDataBridge(QObject *parent = nullptr);

  // Invokable from JavaScript
  Q_INVOKABLE void onChartReady();
  Q_INVOKABLE void onChartClick(qint64 time, double price);
  Q_INVOKABLE void onOrderRequest(const QString &side, double price);
  Q_INVOKABLE void requestHistoricalData(const QString &symbol, int segment,
                                         const QString &resolution, qint64 from,
                                         qint64 to, qint64 token,
                                         int requestId);
  Q_INVOKABLE void searchSymbols(const QString &searchText,
                                 const QString &exchange,
                                 const QString &segment);
  Q_INVOKABLE void loadSymbol(const QString &symbol, int segment, qint64 token,
                              const QString &interval);
//place order require segment id , exchange instrument id (token), order type, quantity, price, stop loss price, etc. We can create a struct to hold all these parameters and pass it as a JSON object from JavaScript to C++. For simplicity, let's define a method that takes these parameters directly for now.

  Q_INVOKABLE void placeOrder(const QString &symbol, int segment,
                             const QString &side, int quantity,
                             const QString &orderType, double price,
                             double slPrice);

public slots:
  // Called from C++ to send data to JavaScript
  void sendHistoricalData(const QJsonArray &bars, int requestId);
  void sendRealtimeBar(const QJsonObject &bar);
  void sendError(const QString &error);

signals:
  // Signals to JavaScript (via QWebChannel)
  void historicalDataReady(const QJsonArray &bars, int requestId);
  void realtimeBarUpdate(const QJsonObject &bar);
  void errorOccurred(const QString &error);
  void symbolSearchResults(const QJsonArray &results);
  void orderPlaced(const QString &orderId, const QString &status,
                  const QString &message);
  void orderFailed(const QString &error);

  // Signals to C++ (from JavaScript)
  void chartReady();
  void chartClicked(qint64 time, double price);
  void orderRequested(const QString &side, double price);
  void historicalDataRequested(const QString &symbol, int segment,
                               const QString &resolution, qint64 from,
                               qint64 to, qint64 token, int requestId);
};

#endif // TRADINGVIEWCHARTWIDGET_H
