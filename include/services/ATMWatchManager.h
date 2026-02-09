#ifndef ATM_WATCH_MANAGER_H
#define ATM_WATCH_MANAGER_H

#include "repository/ContractData.h"
#include "udp/UDPTypes.h"
#include "utils/ATMCalculator.h"
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>
#include <shared_mutex>


/**
 * @brief Manages ATM (At-The-Money) calculations and watch array.
 * Calculates ATM strikes every minute based on configurable base price source.
 */
class ATMWatchManager : public QObject {
  Q_OBJECT
public:
  enum class BasePriceSource { Cash, Future };

  struct ATMConfig {
    QString symbol;
    QString expiry;
    BasePriceSource source = BasePriceSource::Cash;
    int rangeCount = 0; // ±N strikes

    ATMConfig() = default;
  };

  struct ATMInfo {
    enum class Status {
      Valid,
      PriceUnavailable,
      StrikesNotFound,
      Expired,
      CalculationError
    };

    QString symbol;
    QString expiry;
    double basePrice = 0.0;
    double atmStrike = 0.0;
    int64_t callToken = 0;
    int64_t putToken = 0;
    int64_t underlyingToken = 0; // Token for Spot/Future (based on source)
    QDateTime lastUpdated;
    bool isValid = false;
    Status status = Status::Valid;
    QString errorMessage;

    // P3: Strike range support (±N strikes)
    QVector<double> strikes; // All strikes in range (ATM ± N)
    QVector<QPair<int64_t, int64_t>>
        strikeTokens; // {callToken, putToken} for each strike

    ATMInfo() = default;
    ATMInfo(const ATMInfo &) = default;
    ATMInfo &operator=(const ATMInfo &) = default;
    ATMInfo(ATMInfo &&) = default;
    ATMInfo &operator=(ATMInfo &&) = default;
  };

  static ATMWatchManager &getInstance();

  /**
   * @brief Set default source for new watches
   */
  void setDefaultBasePriceSource(BasePriceSource source);
  BasePriceSource getDefaultBasePriceSource() const { return m_defaultSource; }

  /**
   * @brief Set strike range count (±N strikes)
   * @param count Number of strikes on each side (0 = ATM only, 5 = ATM ± 5
   * strikes)
   */
  void setStrikeRangeCount(int count);
  int getStrikeRangeCount() const { return m_defaultRangeCount; }

  /**
   * @brief Set threshold multiplier for event-driven recalculation (P3)
   * @param multiplier Multiplier of strike interval (0.25 = quarter, 0.5 =
   * half, 1.0 = full) Default is 0.5 (half the strike interval)
   */
  void setThresholdMultiplier(double multiplier);
  double getThresholdMultiplier() const { return m_thresholdMultiplier; }

  /**
   * @brief Add or update an ATM watch for a symbol
   * Note: Does NOT trigger immediate calculation. Call calculateAll()
   * explicitly.
   */
  void addWatch(const QString &symbol, const QString &expiry,
                BasePriceSource source = BasePriceSource::Cash);

  /**
   * @brief Add multiple ATM watches in batch and trigger single calculation
   * @param configs Vector of (symbol, expiry) pairs
   * This is more efficient than calling addWatch() multiple times
   */
  void addWatchesBatch(const QVector<QPair<QString, QString>> &configs);

  /**
   * @brief Remove an ATM watch
   */
  void removeWatch(const QString &symbol);

  /**
   * @brief Get current ATM info for all watched symbols
   */
  QVector<ATMInfo> getATMWatchArray() const;

  /**
   * @brief Get specific ATM info for a symbol
   */
  ATMInfo getATMInfo(const QString &symbol) const;

signals:
  /**
   * @brief Emitted when ATM calculation completes for all symbols
   */
  void atmUpdated();

  /**
   * @brief Emitted when ATM calculation fails for a symbol
   * @param symbol Symbol name
   * @param errorMessage Error description
   */
  void calculationFailed(const QString &symbol, const QString &errorMessage);

  /**
   * @brief Emitted when ATM strike changes for a symbol (P3)
   * @param symbol Symbol name
   * @param oldStrike Previous ATM strike
   * @param newStrike New ATM strike
   */
  void atmStrikeChanged(const QString &symbol, double oldStrike,
                        double newStrike);

public slots:
  // Force recalculation manually
  void calculateAll();

private slots:
  void onMinuteTimer();
  void onUnderlyingPriceUpdate(
      const UDP::MarketTick &tick); // P2: Event-driven price updates

  // calculateAll moved to public slots

private:
  ATMWatchManager(QObject *parent = nullptr);
  ~ATMWatchManager() = default;

  // Delete copy and move constructors for singleton
  ATMWatchManager(const ATMWatchManager &) = delete;
  ATMWatchManager &operator=(const ATMWatchManager &) = delete;
  ATMWatchManager(ATMWatchManager &&) = delete;
  ATMWatchManager &operator=(ATMWatchManager &&) = delete;

  double fetchBasePrice(const ATMConfig &config);
  void subscribeToUnderlyingPrices(); // P2: Subscribe to cash/future prices
  double calculateThreshold(const QString &symbol,
                            const QString &expiry); // P2: Half strike interval

  BasePriceSource m_defaultSource = BasePriceSource::Cash;
  int m_defaultRangeCount = 0; // P3: Default ±N strikes (0 = disabled)
  double m_thresholdMultiplier =
      0.5; // P3: Threshold = multiplier * strike_interval (default 0.5)
  QTimer *m_timer;
  QHash<QString, ATMConfig> m_configs;
  QHash<QString, ATMInfo> m_results;
  mutable std::shared_mutex m_mutex;

  // P2: Event-driven price tracking
  QHash<int64_t, QString> m_tokenToSymbol; // underlying token -> symbol
  QHash<QString, double>
      m_lastTriggerPrice; // symbol -> last price that triggered recalc
  QHash<QString, double>
      m_threshold; // symbol -> threshold (half strike interval)

  // P3: ATM change tracking for notifications
  QHash<QString, double> m_previousATMStrike; // symbol -> previous ATM strike
};

#endif // ATM_WATCH_MANAGER_H
