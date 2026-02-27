#ifndef ATM_CALCULATOR_H
#define ATM_CALCULATOR_H

#include <QString>
#include <QVector>
#include <algorithm>
#include <cmath>

#undef min
#undef max

/**
 * @brief Utility class for ATM (At-The-Money) calculations
 */
class ATMCalculator {
public:
  struct CalculationResult {
    double atmStrike = 0.0;
    QVector<double> strikes; // ±N strikes
    bool isValid = false;
  };

  /**
   * @brief Find ATM strike from a list of actual strike prices (Option 1)
   * @param basePrice Current underlying price (Spot or Future)
   * @param actualStrikes List of unique strikes for the symbol/expiry
   * @param rangeCount Number of strikes to include on each side of ATM
   * @return CalculationResult
   */
  static CalculationResult
  calculateFromActualStrikes(double basePrice,
                             const QVector<double> &actualStrikes,
                             int rangeCount = 0) {
    CalculationResult result;
    if (actualStrikes.isEmpty() || basePrice <= 0)
      return result;

    // Sort strikes just in case
    QVector<double> sortedStrikes = actualStrikes;
    std::sort(sortedStrikes.begin(), sortedStrikes.end());

    // Find nearest strike
    auto it =
        std::lower_bound(sortedStrikes.begin(), sortedStrikes.end(), basePrice);

    double nearest;
    int nearestIdx;
    if (it == sortedStrikes.end()) {
      nearest = sortedStrikes.last();
      nearestIdx = sortedStrikes.size() - 1;
    } else if (it == sortedStrikes.begin()) {
      nearest = sortedStrikes.first();
      nearestIdx = 0;
    } else {
      double higher = *it;
      double lower = *(it - 1);
      if ((higher - basePrice) < (basePrice - lower)) {
        nearest = higher;
        nearestIdx = std::distance(sortedStrikes.begin(), it);
      } else {
        nearest = lower;
        nearestIdx = std::distance(sortedStrikes.begin(), it) - 1;
      }
    }

    result.atmStrike = nearest;
    result.isValid = true;

    if (rangeCount > 0) {
      int startIdx = (std::max)(0, nearestIdx - rangeCount);
      int endIdx =
          (std::min)((int)sortedStrikes.size() - 1, nearestIdx + rangeCount);
      for (int i = startIdx; i <= endIdx; ++i) {
        result.strikes.append(sortedStrikes[i]);
      }
    } else {
      result.strikes.append(nearest);
    }

    return result;
  }

  /**
   * @brief Calculate ATM using a fixed strike difference (Option 2)
   * @param basePrice Current underlying price
   * @param strikeDiff The difference between two strikes (e.g., 50 for NIFTY)
   * @param rangeCount Number of strikes to include on each side
   * @return CalculationResult
   */
  static CalculationResult calculateFixedDifference(double basePrice,
                                                    double strikeDiff,
                                                    int rangeCount = 0) {
    CalculationResult result;
    if (strikeDiff <= 0 || basePrice <= 0)
      return result;

    // Calculate nearest strike
    double nearest = std::round(basePrice / strikeDiff) * strikeDiff;
    result.atmStrike = nearest;
    result.isValid = true;

    if (rangeCount > 0) {
      for (int i = -rangeCount; i <= rangeCount; ++i) {
        result.strikes.append(nearest + (i * strikeDiff));
      }
    } else {
      result.strikes.append(nearest);
    }

    return result;
  }
};

#endif // ATM_CALCULATOR_H
