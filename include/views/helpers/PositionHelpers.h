#ifndef POSITIONHELPERS_H
#define POSITIONHELPERS_H

#include <QString>
#include <QStringList>
#include <QList>

#include "models/qt/PositionModel.h"

// Forward declaration if needed, but we included the header
// struct PositionData;

/**
 * @brief Helper functions for PositionData window operations
 * 
 * Provides utilities for parsing, formatting, and validating position data
 * specific to position/P&L operations.
 */
class PositionHelpers
{
public:
    /**
     * @brief Parse a single TSV line into PositionData structure
     * @param line TSV line with format: Symbol\tSeriesExpiry\tBuyQty\tSellQty\t...
     * @param position Output PositionData structure
     * @return true if parsing succeeded, false if invalid
     * 
     * Expected format: Symbol\tSeriesExpiry\tBuyQty\tSellQty\tNetPrice\tMarkPrice\tMTM\tMargin\tBuyValue\tSellValue\tExchange\tSegment\tUser\tClient\tPeriodicity
     */
    static bool parsePositionFromTSV(const QString &line, PositionData &position);
    
    /**
     * @brief Format PositionData to TSV line
     * @param position PositionData to format
     * @return TSV formatted string
     */
    static QString formatPositionToTSV(const PositionData &position);
    
    /**
     * @brief Validate position data
     * @param position PositionData to validate
     * @return true if position has valid symbol
     */
    static bool isValidPosition(const PositionData &position);
    
    /**
     * @brief Calculate net quantity (Buy - Sell)
     * @param position PositionData data
     * @return Net quantity (positive for long, negative for short)
     */
    static int calculateNetQty(const PositionData &position);
    
    /**
     * @brief Calculate total P&L for multiple positions
     * @param positions List of positions
     * @return Total MTM gain/loss
     */
    static double calculateTotalPnL(const QList<PositionData> &positions);
    
    /**
     * @brief Calculate total margin for multiple positions
     * @param positions List of positions
     * @return Total margin requirement
     */
    static double calculateTotalMargin(const QList<PositionData> &positions);
    
    /**
     * @brief Format P&L value with color indicator
     * @param pnl P&L value
     * @return Formatted string with sign (e.g., "+1,234.56" or "-567.89")
     */
    static QString formatPnL(double pnl);
    
    /**
     * @brief Check if position is square-off (Buy == Sell)
     * @param position PositionData to check
     * @return true if net position is zero
     */
    static bool isSquaredOff(const PositionData &position);
};

#endif // POSITIONHELPERS_H
