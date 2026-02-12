#ifndef INDICATORCHARTWIDGET_H
#define INDICATORCHARTWIDGET_H

#include "api/XTSMarketDataClient.h"
#include "repository/RepositoryManager.h"
#include "services/CandleAggregator.h"
#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QChart>
#include <QChartView>
#include <QComboBox>
#include <QDateTimeAxis>
#include <QHash>
#include <QLineEdit>
#include <QLineSeries>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>
#include <QValueAxis>
#include <QWidget>


using namespace QtCharts;

/**
 * @brief Widget for displaying price charts with technical indicators
 *
 * Features:
 * - Candlestick price chart
 * - Multiple indicator overlays (MA, Bollinger Bands, etc.)
 * - Separate indicator panels (RSI, MACD, etc.)
 * - Real-time updates
 * - Zoom and pan
 * - Indicator customization toolbar
 */
class IndicatorChartWidget : public QWidget {
  Q_OBJECT

public:
  explicit IndicatorChartWidget(QWidget *parent = nullptr);
  ~IndicatorChartWidget();

  /**
   * @brief Load symbol and display chart
   */
  void loadSymbol(const QString &symbol, int segment, qint64 token);

  /**
   * @brief Set candle data and refresh chart
   */
  void setCandleData(const QVector<ChartData::Candle> &candles);

  /**
   * @brief Add a new candle (real-time update)
   */
  void appendCandle(const ChartData::Candle &candle);

  /**
   * @brief Update current candle (real-time)
   */
  void updateCurrentCandle(const ChartData::Candle &candle);

  /**
   * @brief Add overlay indicator (displays on price chart)
   */
  void addOverlayIndicator(const QString &name, const QString &type,
                           const QVariantMap &params);

  /**
   * @brief Add panel indicator (displays in separate panel below)
   */
  void addPanelIndicator(const QString &name, const QString &type,
                         const QVariantMap &params);

  /**
   * @brief Remove indicator by name
   */
  void removeIndicator(const QString &name);

  /**
   * @brief Clear all indicators
   */
  void clearIndicators();

  /**
   * @brief Enable/disable auto-scaling
   */
  void setAutoScale(bool enabled);

  /**
   * @brief Set visible candle range (for zooming)
   */
  void setVisibleRange(int candleCount);

  /**
   * @brief Set XTS Market Data Client for OHLC fetching
   */
  void setXTSMarketDataClient(XTSMarketDataClient *client);

  /**
   * @brief Set Repository Manager for symbol lookup
   */
  void setRepositoryManager(RepositoryManager *repoManager);

signals:
  void indicatorAdded(const QString &name, const QString &type);
  void indicatorRemoved(const QString &name);
  void candleClicked(qint64 timestamp, double price);
  void symbolChangeRequested(const QString &symbol);

private slots:
  void onSymbolEntered();
  void onAddIndicatorClicked();
  void onRemoveIndicatorClicked();
  void onZoomInClicked();
  void onZoomOutClicked();
  void onResetZoomClicked();
  void onGlobalSearchClicked();

private:
  struct IndicatorInfo {
    QString name;
    QString type;
    QVariantMap params;
    QLineSeries *series1 = nullptr;
    QLineSeries *series2 = nullptr;
    QLineSeries *series3 = nullptr;
    QChart *chart = nullptr; // For panel indicators
    QChartView *chartView = nullptr;
    bool isOverlay = true;
  };

  // UI Components
  QVBoxLayout *m_mainLayout;
  QToolBar *m_toolbar;
  QLineEdit *m_symbolInput;
  QComboBox *m_indicatorSelector;
  QPushButton *m_addButton;
  QPushButton *m_removeButton;
  QPushButton *m_zoomInButton;
  QPushButton *m_zoomOutButton;
  QPushButton *m_resetZoomButton;
  QPushButton *m_globalSearchButton;

  // Main price chart
  QChart *m_priceChart;
  QChartView *m_priceChartView;
  QCandlestickSeries *m_candlestickSeries;
  QDateTimeAxis *m_axisX;
  QValueAxis *m_axisYPrice;

  // Data storage
  QVector<ChartData::Candle> m_candles;
  QString m_currentSymbol;
  int m_currentSegment = 0;
  qint64 m_currentToken = 0;

  // Indicators
  QHash<QString, IndicatorInfo> m_indicators;

  // Settings
  bool m_autoScale = true;
  int m_visibleCandleCount = 100; // Default: show last 100 candles

  // External services
  XTSMarketDataClient *m_xtsClient = nullptr;
  RepositoryManager *m_repoManager = nullptr;

  // Helper methods
  void setupUI();
  void setupPriceChart();
  void setupToolbar();
  void updatePriceChart();
  void updateIndicator(const QString &name);
  void recalculateAllIndicators();
  void updateAxisRanges();
  QDateTime indexToDateTime(int index) const;
  int dateTimeToIndex(const QDateTime &dt) const;

  // Data fetching
  void fetchOHLCData(const QString &symbol, int segment, qint64 token);

  // Indicator calculation helpers
  void calculateOverlayIndicator(IndicatorInfo &info);
  void calculatePanelIndicator(IndicatorInfo &info);

  // Chart styling
  void applyDarkTheme(QChart *chart);
  void applyAxisStyling(QAbstractAxis *axis);
};

#endif // INDICATORCHARTWIDGET_H
