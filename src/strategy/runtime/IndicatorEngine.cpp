#include "strategy/runtime/IndicatorEngine.h"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <numeric>

IndicatorEngine::IndicatorEngine() {}

void IndicatorEngine::configure(const QVector<IndicatorConfig> &configs) {
  m_configs = configs;
  reset();
}

void IndicatorEngine::reset() {
  m_candles.clear();
  m_values.clear();
  m_emaState.clear();
  m_ready.clear();
}

void IndicatorEngine::addCandle(const ChartData::Candle &candle) {
  m_candles.append(candle);

  // Trim history to prevent unbounded growth
  if (m_candles.size() > MAX_CANDLE_HISTORY) {
    m_candles.remove(0, m_candles.size() - MAX_CANDLE_HISTORY);
  }

  computeAll();
}

double IndicatorEngine::value(const QString &id) const {
  return m_values.value(id, 0.0);
}

bool IndicatorEngine::isReady(const QString &id) const {
  return m_ready.value(id, false);
}

QHash<QString, double> IndicatorEngine::allValues() const { return m_values; }

QStringList IndicatorEngine::supportedIndicators() {
  return {"SMA",  "EMA",    "RSI",  "MACD", "BB",
          "ATR",  "STOCH",  "ADX",  "OBV",  "VOLUME"};
}

bool IndicatorEngine::isValidIndicator(const QString &type) {
  return supportedIndicators().contains(type.toUpper());
}

// ═══════════════════════════════════════════════════════════
// COMPUTE ALL
// ═══════════════════════════════════════════════════════════

void IndicatorEngine::computeAll() {
  for (const IndicatorConfig &cfg : m_configs) {
    QString type = cfg.type.toUpper();
    if (type == "SMA")
      computeSMA(cfg);
    else if (type == "EMA")
      computeEMA(cfg);
    else if (type == "RSI")
      computeRSI(cfg);
    else if (type == "MACD")
      computeMACD(cfg);
    else if (type == "BB")
      computeBollingerBands(cfg);
    else if (type == "ATR")
      computeATR(cfg);
    else if (type == "STOCH")
      computeStochastic(cfg);
    else if (type == "ADX")
      computeADX(cfg);
    else if (type == "OBV")
      computeOBV(cfg);
    else if (type == "VOLUME")
      computeVolume(cfg);
  }
}

// ═══════════════════════════════════════════════════════════
// PRICE HELPERS
// ═══════════════════════════════════════════════════════════

double IndicatorEngine::getPrice(const ChartData::Candle &candle,
                                 const QString &field) const {
  if (field == "open")
    return candle.open;
  if (field == "high")
    return candle.high;
  if (field == "low")
    return candle.low;
  if (field == "close")
    return candle.close;
  if (field == "hl2")
    return (candle.high + candle.low) / 2.0;
  if (field == "hlc3")
    return (candle.high + candle.low + candle.close) / 3.0;
  return candle.close; // default
}

QVector<double> IndicatorEngine::getPriceSeries(const QString &field,
                                                int count) const {
  QVector<double> result;
  int start = std::max(0, m_candles.size() - count);
  for (int i = start; i < m_candles.size(); ++i) {
    result.append(getPrice(m_candles[i], field));
  }
  return result;
}

// ═══════════════════════════════════════════════════════════
// SMA - Simple Moving Average
// ═══════════════════════════════════════════════════════════

void IndicatorEngine::computeSMA(const IndicatorConfig &cfg) {
  int period = cfg.period;
  if (m_candles.size() < period) {
    m_ready[cfg.id] = false;
    return;
  }

  double sum = 0.0;
  for (int i = m_candles.size() - period; i < m_candles.size(); ++i) {
    sum += getPrice(m_candles[i], cfg.priceField);
  }

  m_values[cfg.id] = sum / period;
  m_ready[cfg.id] = true;
}

// ═══════════════════════════════════════════════════════════
// EMA - Exponential Moving Average
// ═══════════════════════════════════════════════════════════

double IndicatorEngine::computeEMAValue(const QVector<double> &data,
                                        int period,
                                        const QString &stateKey) {
  if (data.isEmpty())
    return 0.0;

  double multiplier = 2.0 / (period + 1);

  if (!m_emaState.contains(stateKey)) {
    // Initialize with SMA of first 'period' values
    if (data.size() < period)
      return 0.0;
    double sma = 0.0;
    for (int i = 0; i < period; ++i)
      sma += data[i];
    sma /= period;
    m_emaState[stateKey] = sma;

    // Apply EMA for remaining values
    double ema = sma;
    for (int i = period; i < data.size(); ++i) {
      ema = (data[i] - ema) * multiplier + ema;
    }
    m_emaState[stateKey] = ema;
    return ema;
  }

  // Incremental: apply only latest value
  double prevEma = m_emaState[stateKey];
  double latest = data.last();
  double ema = (latest - prevEma) * multiplier + prevEma;
  m_emaState[stateKey] = ema;
  return ema;
}

void IndicatorEngine::computeEMA(const IndicatorConfig &cfg) {
  if (m_candles.size() < cfg.period) {
    m_ready[cfg.id] = false;
    return;
  }

  QVector<double> prices = getPriceSeries(cfg.priceField, m_candles.size());
  m_values[cfg.id] = computeEMAValue(prices, cfg.period, cfg.id);
  m_ready[cfg.id] = true;
}

// ═══════════════════════════════════════════════════════════
// RSI - Relative Strength Index
// ═══════════════════════════════════════════════════════════

void IndicatorEngine::computeRSI(const IndicatorConfig &cfg) {
  int period = cfg.period;
  if (m_candles.size() < period + 1) {
    m_ready[cfg.id] = false;
    return;
  }

  QVector<double> prices = getPriceSeries(cfg.priceField, m_candles.size());

  // Calculate gains and losses
  QString avgGainKey = cfg.id + "_avgGain";
  QString avgLossKey = cfg.id + "_avgLoss";

  if (!m_emaState.contains(avgGainKey)) {
    // First calculation: simple average
    double sumGain = 0.0, sumLoss = 0.0;
    for (int i = 1; i <= period && i < prices.size(); ++i) {
      double change = prices[i] - prices[i - 1];
      if (change > 0)
        sumGain += change;
      else
        sumLoss += std::abs(change);
    }
    m_emaState[avgGainKey] = sumGain / period;
    m_emaState[avgLossKey] = sumLoss / period;

    // Continue with Wilder's smoothing for remaining data
    for (int i = period + 1; i < prices.size(); ++i) {
      double change = prices[i] - prices[i - 1];
      double gain = change > 0 ? change : 0.0;
      double loss = change < 0 ? std::abs(change) : 0.0;
      m_emaState[avgGainKey] =
          (m_emaState[avgGainKey] * (period - 1) + gain) / period;
      m_emaState[avgLossKey] =
          (m_emaState[avgLossKey] * (period - 1) + loss) / period;
    }
  } else {
    // Incremental: Wilder's smoothing with latest candle
    double change = prices.last() - prices[prices.size() - 2];
    double gain = change > 0 ? change : 0.0;
    double loss = change < 0 ? std::abs(change) : 0.0;
    m_emaState[avgGainKey] =
        (m_emaState[avgGainKey] * (period - 1) + gain) / period;
    m_emaState[avgLossKey] =
        (m_emaState[avgLossKey] * (period - 1) + loss) / period;
  }

  double avgGain = m_emaState[avgGainKey];
  double avgLoss = m_emaState[avgLossKey];

  if (avgLoss < 1e-10) {
    m_values[cfg.id] = 100.0; // No losses = RSI is 100
  } else {
    double rs = avgGain / avgLoss;
    m_values[cfg.id] = 100.0 - (100.0 / (1.0 + rs));
  }
  m_ready[cfg.id] = true;
}

// ═══════════════════════════════════════════════════════════
// MACD - Moving Average Convergence Divergence
// ═══════════════════════════════════════════════════════════

void IndicatorEngine::computeMACD(const IndicatorConfig &cfg) {
  int fastPeriod = cfg.period > 0 ? cfg.period : 12;
  int slowPeriod = cfg.period2 > 0 ? cfg.period2 : 26;
  int signalPeriod = cfg.period3 > 0 ? cfg.period3 : 9;

  if (m_candles.size() < slowPeriod + signalPeriod) {
    m_ready[cfg.id] = false;
    m_ready[cfg.id + "_SIGNAL"] = false;
    m_ready[cfg.id + "_HIST"] = false;
    return;
  }

  QVector<double> prices = getPriceSeries(cfg.priceField, m_candles.size());

  double fastEMA = computeEMAValue(prices, fastPeriod, cfg.id + "_fast");
  double slowEMA = computeEMAValue(prices, slowPeriod, cfg.id + "_slow");
  double macdLine = fastEMA - slowEMA;

  m_values[cfg.id] = macdLine;
  m_ready[cfg.id] = true;

  // Signal line: EMA of MACD line
  // We need historical MACD values - approximate with current
  QString signalKey = cfg.id + "_SIGNAL";
  QString signalEmaKey = cfg.id + "_signalEma";

  double multiplier = 2.0 / (signalPeriod + 1);
  if (!m_emaState.contains(signalEmaKey)) {
    m_emaState[signalEmaKey] = macdLine;
  } else {
    m_emaState[signalEmaKey] =
        (macdLine - m_emaState[signalEmaKey]) * multiplier +
        m_emaState[signalEmaKey];
  }

  double signalLine = m_emaState[signalEmaKey];
  m_values[signalKey] = signalLine;
  m_ready[signalKey] = true;

  // Histogram
  QString histKey = cfg.id + "_HIST";
  m_values[histKey] = macdLine - signalLine;
  m_ready[histKey] = true;
}

// ═══════════════════════════════════════════════════════════
// BOLLINGER BANDS
// ═══════════════════════════════════════════════════════════

void IndicatorEngine::computeBollingerBands(const IndicatorConfig &cfg) {
  int period = cfg.period > 0 ? cfg.period : 20;
  double stddevMult = cfg.param1 > 0 ? cfg.param1 : 2.0;

  if (m_candles.size() < period) {
    m_ready[cfg.id + "_UPPER"] = false;
    m_ready[cfg.id + "_MIDDLE"] = false;
    m_ready[cfg.id + "_LOWER"] = false;
    return;
  }

  QVector<double> prices = getPriceSeries(cfg.priceField, period);

  // Middle band (SMA)
  double sum = 0.0;
  for (double p : prices)
    sum += p;
  double middle = sum / period;

  // Standard deviation
  double variance = 0.0;
  for (double p : prices) {
    double diff = p - middle;
    variance += diff * diff;
  }
  double stddev = std::sqrt(variance / period);

  m_values[cfg.id + "_UPPER"] = middle + stddevMult * stddev;
  m_values[cfg.id + "_MIDDLE"] = middle;
  m_values[cfg.id + "_LOWER"] = middle - stddevMult * stddev;
  m_values[cfg.id] = middle; // Default: middle band

  m_ready[cfg.id + "_UPPER"] = true;
  m_ready[cfg.id + "_MIDDLE"] = true;
  m_ready[cfg.id + "_LOWER"] = true;
  m_ready[cfg.id] = true;
}

// ═══════════════════════════════════════════════════════════
// ATR - Average True Range
// ═══════════════════════════════════════════════════════════

void IndicatorEngine::computeATR(const IndicatorConfig &cfg) {
  int period = cfg.period > 0 ? cfg.period : 14;
  if (m_candles.size() < period + 1) {
    m_ready[cfg.id] = false;
    return;
  }

  QString atrKey = cfg.id + "_atrState";

  if (!m_emaState.contains(atrKey)) {
    // Initial ATR: average of first 'period' true ranges
    double sum = 0.0;
    for (int i = 1; i <= period; ++i) {
      double tr1 = m_candles[i].high - m_candles[i].low;
      double tr2 = std::abs(m_candles[i].high - m_candles[i - 1].close);
      double tr3 = std::abs(m_candles[i].low - m_candles[i - 1].close);
      sum += std::max({tr1, tr2, tr3});
    }
    m_emaState[atrKey] = sum / period;

    // Wilder smoothing for remaining
    for (int i = period + 1; i < m_candles.size(); ++i) {
      double tr1 = m_candles[i].high - m_candles[i].low;
      double tr2 = std::abs(m_candles[i].high - m_candles[i - 1].close);
      double tr3 = std::abs(m_candles[i].low - m_candles[i - 1].close);
      double tr = std::max({tr1, tr2, tr3});
      m_emaState[atrKey] =
          (m_emaState[atrKey] * (period - 1) + tr) / period;
    }
  } else {
    // Incremental
    int n = m_candles.size();
    double tr1 = m_candles[n - 1].high - m_candles[n - 1].low;
    double tr2 = std::abs(m_candles[n - 1].high - m_candles[n - 2].close);
    double tr3 = std::abs(m_candles[n - 1].low - m_candles[n - 2].close);
    double tr = std::max({tr1, tr2, tr3});
    m_emaState[atrKey] =
        (m_emaState[atrKey] * (period - 1) + tr) / period;
  }

  m_values[cfg.id] = m_emaState[atrKey];
  m_ready[cfg.id] = true;
}

// ═══════════════════════════════════════════════════════════
// STOCHASTIC OSCILLATOR
// ═══════════════════════════════════════════════════════════

void IndicatorEngine::computeStochastic(const IndicatorConfig &cfg) {
  int kPeriod = cfg.period > 0 ? cfg.period : 14;
  int dPeriod = cfg.period2 > 0 ? cfg.period2 : 3;

  if (m_candles.size() < kPeriod) {
    m_ready[cfg.id] = false;
    m_ready[cfg.id + "_K"] = false;
    m_ready[cfg.id + "_D"] = false;
    return;
  }

  // %K = (Close - Lowest Low) / (Highest High - Lowest Low) * 100
  double lowestLow = std::numeric_limits<double>::max();
  double highestHigh = std::numeric_limits<double>::lowest();
  int start = m_candles.size() - kPeriod;
  for (int i = start; i < m_candles.size(); ++i) {
    lowestLow = std::min(lowestLow, m_candles[i].low);
    highestHigh = std::max(highestHigh, m_candles[i].high);
  }

  double range = highestHigh - lowestLow;
  double kValue =
      range > 1e-10
          ? ((m_candles.last().close - lowestLow) / range * 100.0)
          : 50.0;

  m_values[cfg.id + "_K"] = kValue;
  m_values[cfg.id] = kValue; // Default: %K
  m_ready[cfg.id + "_K"] = true;
  m_ready[cfg.id] = true;

  // %D = SMA of %K over dPeriod (simplified: use EMA state)
  QString dKey = cfg.id + "_D";
  QString dStateKey = cfg.id + "_dState";
  double multiplier = 2.0 / (dPeriod + 1);

  if (!m_emaState.contains(dStateKey)) {
    m_emaState[dStateKey] = kValue;
  } else {
    m_emaState[dStateKey] =
        (kValue - m_emaState[dStateKey]) * multiplier + m_emaState[dStateKey];
  }

  m_values[dKey] = m_emaState[dStateKey];
  m_ready[dKey] = true;
}

// ═══════════════════════════════════════════════════════════
// ADX - Average Directional Index
// ═══════════════════════════════════════════════════════════

void IndicatorEngine::computeADX(const IndicatorConfig &cfg) {
  int period = cfg.period > 0 ? cfg.period : 14;
  if (m_candles.size() < period * 2) {
    m_ready[cfg.id] = false;
    return;
  }

  // Simplified ADX using Wilder smoothing
  QString plusDmKey = cfg.id + "_plusDM";
  QString minusDmKey = cfg.id + "_minusDM";
  QString trKey = cfg.id + "_tr";
  QString adxKey = cfg.id + "_adx";

  int n = m_candles.size();

  if (!m_emaState.contains(adxKey)) {
    // Initialize
    double sumPlusDM = 0, sumMinusDM = 0, sumTR = 0;
    for (int i = 1; i <= period && i < n; ++i) {
      double upMove = m_candles[i].high - m_candles[i - 1].high;
      double downMove = m_candles[i - 1].low - m_candles[i].low;

      double plusDM = (upMove > downMove && upMove > 0) ? upMove : 0;
      double minusDM = (downMove > upMove && downMove > 0) ? downMove : 0;

      double tr1 = m_candles[i].high - m_candles[i].low;
      double tr2 = std::abs(m_candles[i].high - m_candles[i - 1].close);
      double tr3 = std::abs(m_candles[i].low - m_candles[i - 1].close);

      sumPlusDM += plusDM;
      sumMinusDM += minusDM;
      sumTR += std::max({tr1, tr2, tr3});
    }

    m_emaState[plusDmKey] = sumPlusDM;
    m_emaState[minusDmKey] = sumMinusDM;
    m_emaState[trKey] = sumTR;

    // Smooth and compute DI for remaining data
    double smoothPlusDM = sumPlusDM, smoothMinusDM = sumMinusDM,
           smoothTR = sumTR;
    double adxSum = 0;
    int adxCount = 0;
    for (int i = period + 1; i < n; ++i) {
      double upMove = m_candles[i].high - m_candles[i - 1].high;
      double downMove = m_candles[i - 1].low - m_candles[i].low;
      double plusDM = (upMove > downMove && upMove > 0) ? upMove : 0;
      double minusDM = (downMove > upMove && downMove > 0) ? downMove : 0;
      double tr1 = m_candles[i].high - m_candles[i].low;
      double tr2 = std::abs(m_candles[i].high - m_candles[i - 1].close);
      double tr3 = std::abs(m_candles[i].low - m_candles[i - 1].close);
      double tr = std::max({tr1, tr2, tr3});

      smoothPlusDM = smoothPlusDM - (smoothPlusDM / period) + plusDM;
      smoothMinusDM = smoothMinusDM - (smoothMinusDM / period) + minusDM;
      smoothTR = smoothTR - (smoothTR / period) + tr;

      double plusDI = smoothTR > 0 ? (smoothPlusDM / smoothTR * 100) : 0;
      double minusDI = smoothTR > 0 ? (smoothMinusDM / smoothTR * 100) : 0;
      double diSum = plusDI + minusDI;
      double dx = diSum > 0 ? (std::abs(plusDI - minusDI) / diSum * 100) : 0;
      adxSum += dx;
      adxCount++;
    }

    m_emaState[plusDmKey] = smoothPlusDM;
    m_emaState[minusDmKey] = smoothMinusDM;
    m_emaState[trKey] = smoothTR;
    m_emaState[adxKey] = adxCount > 0 ? adxSum / adxCount : 0;
  } else {
    // Incremental
    double upMove = m_candles[n - 1].high - m_candles[n - 2].high;
    double downMove = m_candles[n - 2].low - m_candles[n - 1].low;
    double plusDM = (upMove > downMove && upMove > 0) ? upMove : 0;
    double minusDM = (downMove > upMove && downMove > 0) ? downMove : 0;
    double tr1 = m_candles[n - 1].high - m_candles[n - 1].low;
    double tr2 = std::abs(m_candles[n - 1].high - m_candles[n - 2].close);
    double tr3 = std::abs(m_candles[n - 1].low - m_candles[n - 2].close);
    double tr = std::max({tr1, tr2, tr3});

    m_emaState[plusDmKey] =
        m_emaState[plusDmKey] - (m_emaState[plusDmKey] / period) + plusDM;
    m_emaState[minusDmKey] =
        m_emaState[minusDmKey] - (m_emaState[minusDmKey] / period) + minusDM;
    m_emaState[trKey] =
        m_emaState[trKey] - (m_emaState[trKey] / period) + tr;

    double smoothTR = m_emaState[trKey];
    double plusDI =
        smoothTR > 0 ? (m_emaState[plusDmKey] / smoothTR * 100) : 0;
    double minusDI =
        smoothTR > 0 ? (m_emaState[minusDmKey] / smoothTR * 100) : 0;
    double diSum = plusDI + minusDI;
    double dx =
        diSum > 0 ? (std::abs(plusDI - minusDI) / diSum * 100) : 0;

    // Smooth ADX (Wilder)
    m_emaState[adxKey] =
        (m_emaState[adxKey] * (period - 1) + dx) / period;
  }

  m_values[cfg.id] = m_emaState[adxKey];
  m_ready[cfg.id] = true;
}

// ═══════════════════════════════════════════════════════════
// OBV - On Balance Volume
// ═══════════════════════════════════════════════════════════

void IndicatorEngine::computeOBV(const IndicatorConfig &cfg) {
  if (m_candles.size() < 2) {
    m_ready[cfg.id] = false;
    return;
  }

  QString obvKey = cfg.id + "_obv";

  if (!m_emaState.contains(obvKey)) {
    // Compute from scratch
    double obv = 0.0;
    for (int i = 1; i < m_candles.size(); ++i) {
      if (m_candles[i].close > m_candles[i - 1].close)
        obv += m_candles[i].volume;
      else if (m_candles[i].close < m_candles[i - 1].close)
        obv -= m_candles[i].volume;
      // If equal, OBV unchanged
    }
    m_emaState[obvKey] = obv;
  } else {
    // Incremental
    int n = m_candles.size();
    if (m_candles[n - 1].close > m_candles[n - 2].close)
      m_emaState[obvKey] += m_candles[n - 1].volume;
    else if (m_candles[n - 1].close < m_candles[n - 2].close)
      m_emaState[obvKey] -= m_candles[n - 1].volume;
  }

  m_values[cfg.id] = m_emaState[obvKey];
  m_ready[cfg.id] = true;
}

// ═══════════════════════════════════════════════════════════
// VOLUME - Current volume indicator
// ═══════════════════════════════════════════════════════════

void IndicatorEngine::computeVolume(const IndicatorConfig &cfg) {
  if (m_candles.isEmpty()) {
    m_ready[cfg.id] = false;
    return;
  }

  m_values[cfg.id] = static_cast<double>(m_candles.last().volume);
  m_ready[cfg.id] = true;

  // Also compute average volume if period is set
  if (cfg.period > 0 && m_candles.size() >= cfg.period) {
    double sum = 0.0;
    int start = m_candles.size() - cfg.period;
    for (int i = start; i < m_candles.size(); ++i) {
      sum += m_candles[i].volume;
    }
    m_values[cfg.id + "_AVG"] = sum / cfg.period;
    m_ready[cfg.id + "_AVG"] = true;
  }
}
