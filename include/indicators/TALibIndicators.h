#ifndef TALIBINDICATORS_H
#define TALIBINDICATORS_H

#include <QVector>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QPair>

// Forward declarations for TA-Lib (will be included in .cpp)
// This avoids exposing TA-Lib headers to the entire codebase

namespace ChartData {
struct Candle;
}

/**
 * @brief Wrapper class for TA-Lib technical indicators
 * 
 * Provides clean C++/Qt interface for TA-Lib calculations.
 * All methods are static and thread-safe.
 * Results are returned as QVector<double> for easy integration with QChart.
 */
class TALibIndicators {
public:
    /**
     * @brief Initialize TA-Lib (call once at startup)
     * @return true if initialization successful
     */
    static bool initialize();
    
    /**
     * @brief Shutdown TA-Lib (call at application exit)
     */
    static void shutdown();
    
    // ============================================================================
    // MOMENTUM INDICATORS
    // ============================================================================
    
    /**
     * @brief Calculate Relative Strength Index
     * @param closes Vector of close prices
     * @param period RSI period (default: 14)
     * @return Vector of RSI values (0-100)
     */
    static QVector<double> calculateRSI(const QVector<double>& closes, int period = 14);
    
    /**
     * @brief Calculate MACD (Moving Average Convergence Divergence)
     * @param closes Vector of close prices
     * @param fastPeriod Fast EMA period (default: 12)
     * @param slowPeriod Slow EMA period (default: 26)
     * @param signalPeriod Signal line period (default: 9)
     * @return Pair of {MACD line, Signal line, Histogram}
     */
    struct MACDResult {
        QVector<double> macdLine;
        QVector<double> signalLine;
        QVector<double> histogram;
    };
    static MACDResult calculateMACD(const QVector<double>& closes, 
                                    int fastPeriod = 12, 
                                    int slowPeriod = 26, 
                                    int signalPeriod = 9);
    
    /**
     * @brief Calculate Stochastic Oscillator
     * @return Pair of {%K line, %D line}
     */
    struct StochasticResult {
        QVector<double> slowK;
        QVector<double> slowD;
    };
    static StochasticResult calculateStochastic(const QVector<double>& highs,
                                               const QVector<double>& lows,
                                               const QVector<double>& closes,
                                               int fastKPeriod = 5,
                                               int slowKPeriod = 3,
                                               int slowDPeriod = 3);
    
    /**
     * @brief Calculate Commodity Channel Index
     */
    static QVector<double> calculateCCI(const QVector<double>& highs,
                                       const QVector<double>& lows,
                                       const QVector<double>& closes,
                                       int period = 14);
    
    /**
     * @brief Calculate Rate of Change
     */
    static QVector<double> calculateROC(const QVector<double>& closes, int period = 10);
    
    // ============================================================================
    // MOVING AVERAGES
    // ============================================================================
    
    /**
     * @brief Calculate Simple Moving Average
     */
    static QVector<double> calculateSMA(const QVector<double>& data, int period = 20);
    
    /**
     * @brief Calculate Exponential Moving Average
     */
    static QVector<double> calculateEMA(const QVector<double>& data, int period = 20);
    
    /**
     * @brief Calculate Weighted Moving Average
     */
    static QVector<double> calculateWMA(const QVector<double>& data, int period = 20);
    
    /**
     * @brief Calculate Double Exponential Moving Average (DEMA)
     */
    static QVector<double> calculateDEMA(const QVector<double>& data, int period = 20);
    
    /**
     * @brief Calculate Triple Exponential Moving Average (TEMA)
     */
    static QVector<double> calculateTEMA(const QVector<double>& data, int period = 20);
    
    /**
     * @brief Calculate Kaufman Adaptive Moving Average (KAMA)
     */
    static QVector<double> calculateKAMA(const QVector<double>& data, int period = 30);
    
    // ============================================================================
    // VOLATILITY INDICATORS
    // ============================================================================
    
    /**
     * @brief Calculate Average True Range
     */
    static QVector<double> calculateATR(const QVector<double>& highs,
                                       const QVector<double>& lows,
                                       const QVector<double>& closes,
                                       int period = 14);
    
    /**
     * @brief Calculate Bollinger Bands
     */
    struct BollingerBandsResult {
        QVector<double> upperBand;
        QVector<double> middleBand;
        QVector<double> lowerBand;
    };
    static BollingerBandsResult calculateBollingerBands(const QVector<double>& closes,
                                                        int period = 20,
                                                        double nbDevUp = 2.0,
                                                        double nbDevDn = 2.0);
    
    /**
     * @brief Calculate Standard Deviation
     */
    static QVector<double> calculateStdDev(const QVector<double>& data, int period = 20);
    
    /**
     * @brief Calculate Normalized ATR (ATR as % of price)
     */
    static QVector<double> calculateNormalizedATR(const QVector<double>& highs,
                                                  const QVector<double>& lows,
                                                  const QVector<double>& closes,
                                                  int period = 14);
    
    // ============================================================================
    // VOLUME INDICATORS
    // ============================================================================
    
    /**
     * @brief Calculate On-Balance Volume
     */
    static QVector<double> calculateOBV(const QVector<double>& closes,
                                       const QVector<double>& volumes);
    
    /**
     * @brief Calculate Chaikin A/D Line
     */
    static QVector<double> calculateAD(const QVector<double>& highs,
                                      const QVector<double>& lows,
                                      const QVector<double>& closes,
                                      const QVector<double>& volumes);
    
    /**
     * @brief Calculate Money Flow Index
     */
    static QVector<double> calculateMFI(const QVector<double>& highs,
                                       const QVector<double>& lows,
                                       const QVector<double>& closes,
                                       const QVector<double>& volumes,
                                       int period = 14);
    
    // ============================================================================
    // PATTERN RECOGNITION
    // ============================================================================
    
    /**
     * @brief Detect Doji candlestick pattern
     * @return Vector of pattern strength (-100 to +100, 0 = no pattern)
     */
    static QVector<int> detectDoji(const QVector<double>& opens,
                                   const QVector<double>& highs,
                                   const QVector<double>& lows,
                                   const QVector<double>& closes);
    
    /**
     * @brief Detect Hammer pattern
     */
    static QVector<int> detectHammer(const QVector<double>& opens,
                                     const QVector<double>& highs,
                                     const QVector<double>& lows,
                                     const QVector<double>& closes);
    
    /**
     * @brief Detect Hanging Man pattern
     */
    static QVector<int> detectHangingMan(const QVector<double>& opens,
                                         const QVector<double>& highs,
                                         const QVector<double>& lows,
                                         const QVector<double>& closes);
    
    /**
     * @brief Detect Engulfing pattern (bullish/bearish)
     */
    static QVector<int> detectEngulfing(const QVector<double>& opens,
                                        const QVector<double>& highs,
                                        const QVector<double>& lows,
                                        const QVector<double>& closes);
    
    /**
     * @brief Detect Morning Star pattern
     */
    static QVector<int> detectMorningStar(const QVector<double>& opens,
                                          const QVector<double>& highs,
                                          const QVector<double>& lows,
                                          const QVector<double>& closes);
    
    /**
     * @brief Detect Evening Star pattern
     */
    static QVector<int> detectEveningStar(const QVector<double>& opens,
                                          const QVector<double>& highs,
                                          const QVector<double>& lows,
                                          const QVector<double>& closes);
    
    /**
     * @brief Detect Shooting Star pattern
     */
    static QVector<int> detectShootingStar(const QVector<double>& opens,
                                           const QVector<double>& highs,
                                           const QVector<double>& lows,
                                           const QVector<double>& closes);
    
    // ============================================================================
    // HELPER FUNCTIONS
    // ============================================================================
    
    /**
     * @brief Extract price vectors from candle array
     */
    struct PriceVectors {
        QVector<double> opens;
        QVector<double> highs;
        QVector<double> lows;
        QVector<double> closes;
        QVector<double> volumes;
        QVector<qint64> timestamps;
    };
    static PriceVectors extractPrices(const QVector<ChartData::Candle>& candles);
    
    /**
     * @brief Check if TA-Lib is available
     */
    static bool isAvailable();
    
    /**
     * @brief Get TA-Lib version string
     */
    static QString getVersion();
    
private:
    static bool s_initialized;
    static bool s_available;
};

#endif // TALIBINDICATORS_H
