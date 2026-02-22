#ifndef INDICATOR_ENGINE_H
#define INDICATOR_ENGINE_H

#include "data/CandleData.h"
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief Configuration for a single indicator to compute.
 */
struct IndicatorConfig {
  QString id;            // Unique ID: "RSI_14", "SMA_20"
  QString type;          // "RSI", "SMA", "EMA", "MACD", "BB", "ATR", etc.
  int period  = 14;      // Primary period
  int period2 = 0;       // Secondary period (e.g. MACD signal line)
  int period3 = 0;       // Tertiary period
  QString priceField = "close"; // "close", "high", "low", "open", "hl2", "hlc3"
  double param1 = 0.0;   // Extra param (e.g. BB stddev multiplier)
};

/**
 * @brief Computes technical indicators from candle history
 *
 * Supports MVP indicators:
 *   SMA, EMA, RSI, MACD, Bollinger Bands, ATR,
 *   Stochastic, ADX, OBV, Volume
 *
 * Usage:
 *   IndicatorEngine engine;
 *   engine.configure(indicatorConfigs);
 *   engine.addCandle(candle);
 *   double rsi = engine.value("RSI_14");
 */
class IndicatorEngine {
public:
  IndicatorEngine();

  /// Configure which indicators to compute
  void configure(const QVector<IndicatorConfig> &configs);

  /// Feed a new completed candle (call in chronological order)
  void addCandle(const ChartData::Candle &candle);

  /// Get latest computed value for an indicator
  /// Returns 0.0 if not yet available (insufficient data)
  double value(const QString &id) const;

  /// Check if indicator has enough data to produce a value
  bool isReady(const QString &id) const;

  /// Get all current indicator values
  QHash<QString, double> allValues() const;

  /// Reset all state / history
  void reset();

  /// Number of candles ingested so far
  int candleCount() const { return m_candles.size(); }

  // ────────── Static Helpers ──────────

  /// List of supported indicator types
  static QStringList supportedIndicators();

  /// Validate an indicator type string
  static bool isValidIndicator(const QString &type);

private:
  // ────────── Compute Functions ──────────
  void computeAll();
  void computeSMA(const IndicatorConfig &cfg);
  void computeEMA(const IndicatorConfig &cfg);
  void computeRSI(const IndicatorConfig &cfg);
  void computeMACD(const IndicatorConfig &cfg);
  void computeBollingerBands(const IndicatorConfig &cfg);
  void computeATR(const IndicatorConfig &cfg);
  void computeStochastic(const IndicatorConfig &cfg);
  void computeADX(const IndicatorConfig &cfg);
  void computeOBV(const IndicatorConfig &cfg);
  void computeVolume(const IndicatorConfig &cfg);

  // ────────── Price Helpers ──────────
  double getPrice(const ChartData::Candle &candle,
                  const QString &field) const;
  QVector<double> getPriceSeries(const QString &field, int count) const;

  // ────────── EMA Helper ──────────
  double computeEMAValue(const QVector<double> &data, int period,
                         const QString &stateKey);

  // ────────── Data ──────────
  QVector<IndicatorConfig> m_configs;
  QVector<ChartData::Candle> m_candles;
  QHash<QString, double> m_values;           // indicator_id → latest value
  QHash<QString, double> m_emaState;         // EMA state tracking
  QHash<QString, bool> m_ready;              // indicator_id → has enough data

  static constexpr int MAX_CANDLE_HISTORY = 500; // Keep last N candles
};

#endif // INDICATOR_ENGINE_H
