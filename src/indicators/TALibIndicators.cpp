
#include "indicators/TALibIndicators.h"
#include "services/CandleAggregator.h"
#include <QDebug>

// Try to include TA-Lib - if not available, provide stub implementations
#ifdef HAVE_TALIB
#include <ta_libc.h>
#endif

bool TALibIndicators::s_initialized = false;
bool TALibIndicators::s_available = false;

bool TALibIndicators::initialize() {
#ifdef HAVE_TALIB
  TA_RetCode retCode = TA_Initialize();
  if (retCode == TA_SUCCESS) {
    s_initialized = true;
    s_available = true;
    qDebug() << "[TALib] Initialized successfully. Version:" << getVersion();
    return true;
  } else {
    qWarning() << "[TALib] Initialization failed with code:" << retCode;
    s_available = false;
    return false;
  }
#else
  qWarning() << "[TALib] Not compiled with TA-Lib support. Indicator "
                "calculations will return empty results.";
  qWarning() << "[TALib] To enable: Install TA-Lib and add -DHAVE_TALIB to "
                "CMakeLists.txt";
  s_available = false;
  return false;
#endif
}

void TALibIndicators::shutdown() {
#ifdef HAVE_TALIB
  if (s_initialized) {
    TA_Shutdown();
    s_initialized = false;
    qDebug() << "[TALib] Shutdown complete";
  }
#endif
}

bool TALibIndicators::isAvailable() { return s_available; }

QString TALibIndicators::getVersion() {
#ifdef HAVE_TALIB
  return QString("TA-Lib %1.%2.%3")
      .arg(TA_GetVersionMajor())
      .arg(TA_GetVersionMinor())
      .arg(TA_GetVersionBuild());
#else
  return "TA-Lib not available";
#endif
}

// ============================================================================
// MOMENTUM INDICATORS
// ============================================================================

QVector<double> TALibIndicators::calculateRSI(const QVector<double> &closes,
                                              int period) {
  int size = closes.size();
  QVector<double> rsi(size, 0.0);

  if (!s_available || size < period) {
    return rsi;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode = TA_RSI(0,                  // startIdx
                              size - 1,           // endIdx
                              closes.constData(), // input array
                              period,             // time period
                              &outBegIdx,         // output begin index
                              &outNBElement,      // output element count
                              outReal             // output array
  );

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      rsi[outBegIdx + i] = outReal[i];
    }
    qDebug() << "[TALib] RSI calculated:" << outNBElement << "values";
  } else {
    qWarning() << "[TALib] RSI calculation failed with code:" << retCode;
  }

  delete[] outReal;
#endif

  return rsi;
}

TALibIndicators::MACDResult
TALibIndicators::calculateMACD(const QVector<double> &closes, int fastPeriod,
                               int slowPeriod, int signalPeriod) {

  int size = closes.size();
  MACDResult result;
  result.macdLine.resize(size);
  result.macdLine.fill(0.0);
  result.signalLine.resize(size);
  result.signalLine.fill(0.0);
  result.histogram.resize(size);
  result.histogram.fill(0.0);

  if (!s_available || size < slowPeriod) {
    return result;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outMACD = new double[size];
  double *outMACDSignal = new double[size];
  double *outMACDHist = new double[size];

  TA_RetCode retCode = TA_MACD(
      0, size - 1, closes.constData(), fastPeriod, slowPeriod, signalPeriod,
      &outBegIdx, &outNBElement, outMACD, outMACDSignal, outMACDHist);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      result.macdLine[outBegIdx + i] = outMACD[i];
      result.signalLine[outBegIdx + i] = outMACDSignal[i];
      result.histogram[outBegIdx + i] = outMACDHist[i];
    }
    qDebug() << "[TALib] MACD calculated:" << outNBElement << "values";
  } else {
    qWarning() << "[TALib] MACD calculation failed with code:" << retCode;
  }

  delete[] outMACD;
  delete[] outMACDSignal;
  delete[] outMACDHist;
#endif

  return result;
}

TALibIndicators::StochasticResult TALibIndicators::calculateStochastic(
    const QVector<double> &highs, const QVector<double> &lows,
    const QVector<double> &closes, int fastKPeriod, int slowKPeriod,
    int slowDPeriod) {

  int size = closes.size();
  StochasticResult result;
  result.slowK.resize(size);
  result.slowK.fill(0.0);
  result.slowD.resize(size);
  result.slowD.fill(0.0);

  if (!s_available || size < fastKPeriod) {
    return result;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outSlowK = new double[size];
  double *outSlowD = new double[size];

  TA_RetCode retCode =
      TA_STOCH(0, size - 1, highs.constData(), lows.constData(),
               closes.constData(), fastKPeriod, slowKPeriod,
               TA_MAType_SMA, // Slow %K MA type
               slowDPeriod,
               TA_MAType_SMA, // Slow %D MA type
               &outBegIdx, &outNBElement, outSlowK, outSlowD);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      result.slowK[outBegIdx + i] = outSlowK[i];
      result.slowD[outBegIdx + i] = outSlowD[i];
    }
    qDebug() << "[TALib] Stochastic calculated:" << outNBElement << "values";
  }

  delete[] outSlowK;
  delete[] outSlowD;
#endif

  return result;
}

QVector<double> TALibIndicators::calculateCCI(const QVector<double> &highs,
                                              const QVector<double> &lows,
                                              const QVector<double> &closes,
                                              int period) {

  int size = closes.size();
  QVector<double> cci(size, 0.0);

  if (!s_available || size < period) {
    return cci;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode =
      TA_CCI(0, size - 1, highs.constData(), lows.constData(),
             closes.constData(), period, &outBegIdx, &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      cci[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return cci;
}

QVector<double> TALibIndicators::calculateROC(const QVector<double> &closes,
                                              int period) {
  int size = closes.size();
  QVector<double> roc(size, 0.0);

  if (!s_available || size < period) {
    return roc;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode = TA_ROC(0, size - 1, closes.constData(), period,
                              &outBegIdx, &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      roc[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return roc;
}

// ============================================================================
// MOVING AVERAGES
// ============================================================================

QVector<double> TALibIndicators::calculateSMA(const QVector<double> &data,
                                              int period) {
  int size = data.size();
  QVector<double> sma(size, 0.0);

  if (!s_available || size < period) {
    return sma;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode = TA_SMA(0, size - 1, data.constData(), period, &outBegIdx,
                              &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      sma[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return sma;
}

QVector<double> TALibIndicators::calculateEMA(const QVector<double> &data,
                                              int period) {
  int size = data.size();
  QVector<double> ema(size, 0.0);

  if (!s_available || size < period) {
    return ema;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode = TA_EMA(0, size - 1, data.constData(), period, &outBegIdx,
                              &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      ema[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return ema;
}

QVector<double> TALibIndicators::calculateWMA(const QVector<double> &data,
                                              int period) {
  int size = data.size();
  QVector<double> wma(size, 0.0);

  if (!s_available || size < period) {
    return wma;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode = TA_WMA(0, size - 1, data.constData(), period, &outBegIdx,
                              &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      wma[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return wma;
}

QVector<double> TALibIndicators::calculateDEMA(const QVector<double> &data,
                                               int period) {
  int size = data.size();
  QVector<double> dema(size, 0.0);

  if (!s_available || size < period) {
    return dema;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode = TA_DEMA(0, size - 1, data.constData(), period,
                               &outBegIdx, &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      dema[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return dema;
}

QVector<double> TALibIndicators::calculateTEMA(const QVector<double> &data,
                                               int period) {
  int size = data.size();
  QVector<double> tema(size, 0.0);

  if (!s_available || size < period) {
    return tema;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode = TA_TEMA(0, size - 1, data.constData(), period,
                               &outBegIdx, &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      tema[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return tema;
}

QVector<double> TALibIndicators::calculateKAMA(const QVector<double> &data,
                                               int period) {
  int size = data.size();
  QVector<double> kama(size, 0.0);

  if (!s_available || size < period) {
    return kama;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode = TA_KAMA(0, size - 1, data.constData(), period,
                               &outBegIdx, &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      kama[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return kama;
}

// ============================================================================
// VOLATILITY INDICATORS
// ============================================================================

QVector<double> TALibIndicators::calculateATR(const QVector<double> &highs,
                                              const QVector<double> &lows,
                                              const QVector<double> &closes,
                                              int period) {

  int size = closes.size();
  QVector<double> atr(size, 0.0);

  if (!s_available || size < period) {
    return atr;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode =
      TA_ATR(0, size - 1, highs.constData(), lows.constData(),
             closes.constData(), period, &outBegIdx, &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      atr[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return atr;
}

TALibIndicators::BollingerBandsResult TALibIndicators::calculateBollingerBands(
    const QVector<double> &closes, int period, double nbDevUp, double nbDevDn) {

  int size = closes.size();
  BollingerBandsResult result;
  result.upperBand.resize(size);
  result.upperBand.fill(0.0);
  result.middleBand.resize(size);
  result.middleBand.fill(0.0);
  result.lowerBand.resize(size);
  result.lowerBand.fill(0.0);

  if (!s_available || size < period) {
    return result;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outUpper = new double[size];
  double *outMiddle = new double[size];
  double *outLower = new double[size];

  TA_RetCode retCode =
      TA_BBANDS(0, size - 1, closes.constData(), period, nbDevUp, nbDevDn,
                TA_MAType_SMA, // MA type for middle band
                &outBegIdx, &outNBElement, outUpper, outMiddle, outLower);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      result.upperBand[outBegIdx + i] = outUpper[i];
      result.middleBand[outBegIdx + i] = outMiddle[i];
      result.lowerBand[outBegIdx + i] = outLower[i];
    }
  }

  delete[] outUpper;
  delete[] outMiddle;
  delete[] outLower;
#endif

  return result;
}

QVector<double> TALibIndicators::calculateStdDev(const QVector<double> &data,
                                                 int period) {
  int size = data.size();
  QVector<double> stddev(size, 0.0);

  if (!s_available || size < period) {
    return stddev;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode = TA_STDDEV(0, size - 1, data.constData(), period,
                                 1.0, // Number of deviations
                                 &outBegIdx, &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      stddev[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return stddev;
}

QVector<double> TALibIndicators::calculateNormalizedATR(
    const QVector<double> &highs, const QVector<double> &lows,
    const QVector<double> &closes, int period) {

  QVector<double> atr = calculateATR(highs, lows, closes, period);
  QVector<double> natr(atr.size(), 0.0);

  // Normalize: (ATR / Close) * 100
  for (int i = period; i < atr.size(); i++) {
    if (closes[i] > 0) {
      natr[i] = (atr[i] / closes[i]) * 100.0;
    }
  }

  return natr;
}

// ============================================================================
// VOLUME INDICATORS
// ============================================================================

QVector<double> TALibIndicators::calculateOBV(const QVector<double> &closes,
                                              const QVector<double> &volumes) {

  int size = closes.size();
  QVector<double> obv(size, 0.0);

  if (!s_available || size < 2) {
    return obv;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode =
      TA_OBV(0, size - 1, closes.constData(), volumes.constData(), &outBegIdx,
             &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      obv[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return obv;
}

QVector<double> TALibIndicators::calculateAD(const QVector<double> &highs,
                                             const QVector<double> &lows,
                                             const QVector<double> &closes,
                                             const QVector<double> &volumes) {

  int size = closes.size();
  QVector<double> ad(size, 0.0);

  if (!s_available || size < 1) {
    return ad;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode = TA_AD(0, size - 1, highs.constData(), lows.constData(),
                             closes.constData(), volumes.constData(),
                             &outBegIdx, &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      ad[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return ad;
}

QVector<double> TALibIndicators::calculateMFI(const QVector<double> &highs,
                                              const QVector<double> &lows,
                                              const QVector<double> &closes,
                                              const QVector<double> &volumes,
                                              int period) {

  int size = closes.size();
  QVector<double> mfi(size, 0.0);

  if (!s_available || size < period) {
    return mfi;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  double *outReal = new double[size];

  TA_RetCode retCode = TA_MFI(0, size - 1, highs.constData(), lows.constData(),
                              closes.constData(), volumes.constData(), period,
                              &outBegIdx, &outNBElement, outReal);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      mfi[outBegIdx + i] = outReal[i];
    }
  }

  delete[] outReal;
#endif

  return mfi;
}

// ============================================================================
// PATTERN RECOGNITION
// ============================================================================

QVector<int> TALibIndicators::detectDoji(const QVector<double> &opens,
                                         const QVector<double> &highs,
                                         const QVector<double> &lows,
                                         const QVector<double> &closes) {

  int size = closes.size();
  QVector<int> pattern(size, 0);

  if (!s_available || size < 1) {
    return pattern;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  int *outInteger = new int[size];

  TA_RetCode retCode = TA_CDLDOJI(
      0, size - 1, opens.constData(), highs.constData(), lows.constData(),
      closes.constData(), &outBegIdx, &outNBElement, outInteger);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      pattern[outBegIdx + i] = outInteger[i];
    }
  }

  delete[] outInteger;
#endif

  return pattern;
}

QVector<int> TALibIndicators::detectHammer(const QVector<double> &opens,
                                           const QVector<double> &highs,
                                           const QVector<double> &lows,
                                           const QVector<double> &closes) {

  int size = closes.size();
  QVector<int> pattern(size, 0);

  if (!s_available || size < 1) {
    return pattern;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  int *outInteger = new int[size];

  TA_RetCode retCode = TA_CDLHAMMER(
      0, size - 1, opens.constData(), highs.constData(), lows.constData(),
      closes.constData(), &outBegIdx, &outNBElement, outInteger);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      pattern[outBegIdx + i] = outInteger[i];
    }
  }

  delete[] outInteger;
#endif

  return pattern;
}

QVector<int> TALibIndicators::detectHangingMan(const QVector<double> &opens,
                                               const QVector<double> &highs,
                                               const QVector<double> &lows,
                                               const QVector<double> &closes) {

  int size = closes.size();
  QVector<int> pattern(size, 0);

  if (!s_available || size < 1) {
    return pattern;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  int *outInteger = new int[size];

  TA_RetCode retCode = TA_CDLHANGINGMAN(
      0, size - 1, opens.constData(), highs.constData(), lows.constData(),
      closes.constData(), &outBegIdx, &outNBElement, outInteger);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      pattern[outBegIdx + i] = outInteger[i];
    }
  }

  delete[] outInteger;
#endif

  return pattern;
}

QVector<int> TALibIndicators::detectEngulfing(const QVector<double> &opens,
                                              const QVector<double> &highs,
                                              const QVector<double> &lows,
                                              const QVector<double> &closes) {

  int size = closes.size();
  QVector<int> pattern(size, 0);

  if (!s_available || size < 2) {
    return pattern;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  int *outInteger = new int[size];

  TA_RetCode retCode = TA_CDLENGULFING(
      0, size - 1, opens.constData(), highs.constData(), lows.constData(),
      closes.constData(), &outBegIdx, &outNBElement, outInteger);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      pattern[outBegIdx + i] = outInteger[i];
    }
  }

  delete[] outInteger;
#endif

  return pattern;
}

QVector<int> TALibIndicators::detectMorningStar(const QVector<double> &opens,
                                                const QVector<double> &highs,
                                                const QVector<double> &lows,
                                                const QVector<double> &closes) {

  int size = closes.size();
  QVector<int> pattern(size, 0);

  if (!s_available || size < 3) {
    return pattern;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  int *outInteger = new int[size];

  TA_RetCode retCode =
      TA_CDLMORNINGSTAR(0, size - 1, opens.constData(), highs.constData(),
                        lows.constData(), closes.constData(),
                        0.3, // Penetration (0.0 - 1.0)
                        &outBegIdx, &outNBElement, outInteger);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      pattern[outBegIdx + i] = outInteger[i];
    }
  }

  delete[] outInteger;
#endif

  return pattern;
}

QVector<int> TALibIndicators::detectEveningStar(const QVector<double> &opens,
                                                const QVector<double> &highs,
                                                const QVector<double> &lows,
                                                const QVector<double> &closes) {

  int size = closes.size();
  QVector<int> pattern(size, 0);

  if (!s_available || size < 3) {
    return pattern;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  int *outInteger = new int[size];

  TA_RetCode retCode =
      TA_CDLEVENINGSTAR(0, size - 1, opens.constData(), highs.constData(),
                        lows.constData(), closes.constData(),
                        0.3, // Penetration
                        &outBegIdx, &outNBElement, outInteger);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      pattern[outBegIdx + i] = outInteger[i];
    }
  }

  delete[] outInteger;
#endif

  return pattern;
}

QVector<int> TALibIndicators::detectShootingStar(
    const QVector<double> &opens, const QVector<double> &highs,
    const QVector<double> &lows, const QVector<double> &closes) {

  int size = closes.size();
  QVector<int> pattern(size, 0);

  if (!s_available || size < 1) {
    return pattern;
  }

#ifdef HAVE_TALIB
  int outBegIdx, outNBElement;
  int *outInteger = new int[size];

  TA_RetCode retCode = TA_CDLSHOOTINGSTAR(
      0, size - 1, opens.constData(), highs.constData(), lows.constData(),
      closes.constData(), &outBegIdx, &outNBElement, outInteger);

  if (retCode == TA_SUCCESS) {
    for (int i = 0; i < outNBElement; i++) {
      pattern[outBegIdx + i] = outInteger[i];
    }
  }

  delete[] outInteger;
#endif

  return pattern;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

TALibIndicators::PriceVectors
TALibIndicators::extractPrices(const QVector<ChartData::Candle> &candles) {

  PriceVectors prices;
  prices.opens.reserve(candles.size());
  prices.highs.reserve(candles.size());
  prices.lows.reserve(candles.size());
  prices.closes.reserve(candles.size());
  prices.volumes.reserve(candles.size());
  prices.timestamps.reserve(candles.size());

  for (const auto &candle : candles) {
    prices.opens.append(candle.open);
    prices.highs.append(candle.high);
    prices.lows.append(candle.low);
    prices.closes.append(candle.close);
    prices.volumes.append(candle.volume);
    prices.timestamps.append(candle.timestamp);
  }

  return prices;
}
