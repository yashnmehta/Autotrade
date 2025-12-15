#ifndef POSITIONHELPERS_H
#define POSITIONHELPERS_H

#include <QString>
#include <QStringList>
#include <QList>

// Forward declaration
struct Position;

/**
 * @brief Helper functions for Position window operations
 * 
 * Provides utilities for parsing, formatting, and validating position data
 * specific to position/P&L operations.
 */
class PositionHelpers
{
public:
    /**
     * @brief Parse a single TSV line into Position structure
     * @param line TSV line with format: Symbol\tSeriesExpiry\tBuyQty\tSellQty\t...
     * @param position Output Position structure
     * @return true if parsing succeeded, false if invalid
     * 
     * Expected format: Symbol\tSeriesExpiry\tBuyQty\tSellQty\tNetPrice\tMarkPrice\tMTM\tMargin\tBuyValue\tSellValue\tExchange\tSegment\tUser\tClient\tPeriodicity
     */
    static bool parsePositionFromTSV(const QString &line, Position &position);
    
    /**
     * @brief Format Position to TSV line
     * @param position Position to format
     * @return TSV formatted string
     */
    static QString formatPositionToTSV(const Position &position);
    
    /**
     * @brief Validate position data
     * @param position Position to validate
     * @return true if position has valid symbol
     */
    static bool isValidPosition(const Position &position);
    
    /**
     * @brief Calculate net quantity (Buy - Sell)
     * @param position Position data
     * @return Net quantity (positive for long, negative for short)
     */
    static int calculateNetQty(const Position &position);
    
    /**
     * @brief Calculate total P&L for multiple positions
     * @param positions List of positions
     * @return Total MTM gain/loss
     */
    static double calculateTotalPnL(const QList<Position> &positions);
    
    /**
     * @brief Calculate total margin for multiple positions
     * @param positions List of positions
     * @return Total margin requirement
     */
    static double calculateTotalMargin(const QList<Position> &positions);
    
    /**
     * @brief Format P&L value with color indicator
     * @param pnl P&L value
     * @return Formatted string with sign (e.g., "+1,234.56" or "-567.89")
     */
    static QString formatPnL(double pnl);
    
    /**
     * @brief Check if position is square-off (Buy == Sell)
     * @param position Position to check
     * @return true if net position is zero
     */
    static bool isSquaredOff(const Position &position);
};

#endif // POSITIONHELPERS_H
