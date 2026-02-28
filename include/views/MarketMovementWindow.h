#ifndef MARKET_MOVEMENT_WINDOW_H
#define MARKET_MOVEMENT_WINDOW_H

#include "models/domain/WindowContext.h"
#include <QCloseEvent>
#include <QDateTime>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QString>
#include <QTableWidget>
#include <QWidget>


/**
 * @brief Lightweight candle struct used by MarketMovementWindow.
 *
 * Originally came from a removed MarketData::CandleData type.
 * Kept here so the legacy stubs compile without pulling in unrelated headers.
 */
struct CandleData {
  double open = 0;
  double high = 0;
  double low = 0;
  double close = 0;
  qint64 volume = 0;
  double average = 0;
  QDateTime timestamp;

  bool isValid() const { return open > 0 && high > 0 && low > 0 && close > 0; }
  QString getFormattedTime() const {
    return timestamp.toString("HH:mm");
  }
};

class QVBoxLayout;
class QHBoxLayout;
class QTimer;

/**
 * @brief Market Movement Window - Displays 1-minute OHLC candles from XTS API
 *
 * Fetches historical candle data directly from XTS Market Data API.
 * No UDP/CandleAggregator - pure XTS API integration.
 *
 * Features:
 * - Fetches historical OHLC from 9:00 AM to current time
 * - 7 columns: Time, Open, High, Low, Close, Volume, Average
 * - Auto-refresh every 60 seconds
 * - Manual refresh button
 * - Displays up to 500 candles
 * - Newest candles at top
 */
class MarketMovementWindow : public QWidget {
  Q_OBJECT

public:
  explicit MarketMovementWindow(QWidget *parent = nullptr);
  explicit MarketMovementWindow(const WindowContext &context,
                                QWidget *parent = nullptr);
  ~MarketMovementWindow();

  /**
   * @brief Set the instrument to display
   * @param context Window context with instrument details
   */
  void setInstrument(const WindowContext &context);

  /**
   * @brief Get current window context
   * @return Context with current instrument
   */
  WindowContext getContext() const { return m_context; }

  /**
   * @brief Check if window has valid instrument
   * @return true if instrument is set
   */
  bool hasInstrument() const { return m_context.isValid(); }

private slots:
  /**
   * @brief Handle XTS API OHLC response
   */
  void onHistoricalDataReceived(QNetworkReply *reply);

  /**
   * @brief Handle refresh button click
   */
  void onRefreshClicked();

  /**
   * @brief Legacy stubs (not used - XTS API only)
   */
  void onCandleCompleted(quint32 token, const CandleData &candle);
  void onCandleUpdated(quint32 token, const CandleData &candle);

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  void setupUI();
  void setupConnections();
  void setupTable();
  void setupNetworkManager();
  void updateHeader();
  void addCandleToTable(const CandleData &candle);
  void fetchHistoricalOHLC();
  int getSegmentFromExchange(const QString &exchange) const;

  // UI Components
  QVBoxLayout *m_mainLayout;
  QHBoxLayout *m_headerLayout;
  QLabel *m_titleLabel;
  QLabel *m_instrumentLabel;
  QLabel *m_statusLabel;
  QPushButton *m_refreshButton;
  QTableWidget *m_table;

  // Data
  WindowContext m_context;
  int m_currentSegment;
  uint32_t m_currentToken;
  QTimer *m_autoRefreshTimer;

  // Network & API
  QNetworkAccessManager *m_networkManager;
  bool m_isLoadingHistorical;
  QString m_xtsBaseUrl;
  QString m_xtsAuthToken;

  // Table columns
  enum Column {
    COL_TIME = 0,
    COL_OPEN,
    COL_HIGH,
    COL_LOW,
    COL_CLOSE,
    COL_VOLUME,
    COL_AVERAGE,
    COL_COUNT
  };
};

#endif // MARKET_MOVEMENT_WINDOW_H
