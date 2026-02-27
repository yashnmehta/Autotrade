#ifndef MARKETWATCHHELPERS_H
#define MARKETWATCHHELPERS_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include "models/MarketWatchColumnProfile.h"
#include "models/GenericTableProfile.h"

// Forward declaration
struct ScripData;

/**
 * @brief Helper functions for Market Watch window operations
 * 
 * Provides utilities for parsing, formatting, and validating scrip data
 * specific to market watch operations.
 */
class MarketWatchHelpers
{
public:
    /**
     * @brief Parse a single TSV line into ScripData
     * @param line TSV line with format: Symbol\tExchange\tToken\tLTP\t...
     * @param scrip Output ScripData structure
     * @return true if parsing succeeded, false if invalid
     * 
     * Expected format: Symbol\tExchange\tToken\tLTP\tChange\t%Change\tVolume\tBid\tAsk\tHigh\tLow\tOpen
     */
    static bool parseScripFromTSV(const QString &line, ScripData &scrip);
    
    /**
     * @brief Format ScripData to TSV line
     * @param scrip ScripData to format
     * @return TSV formatted string
     */
    static QString formatScripToTSV(const ScripData &scrip);
    
    /**
     * @brief Validate scrip data
     * @param scrip ScripData to validate
     * @return true if scrip has valid symbol, exchange, and token
     */
    static bool isValidScrip(const ScripData &scrip);
    
    /**
     * @brief Check if line represents a blank/separator row
     * @param line TSV line to check
     * @return true if line is a separator (e.g., "────")
     */
    static bool isBlankLine(const QString &line);
    
    /**
     * @brief Extract token from TSV line
     * @param line TSV line
     * @return Token ID, or -1 if invalid
     */
    static int extractToken(const QString &line);

    /**
     * @brief Save portfolio to file
     * @param filename Path to save file
     * @param scrips List of scrips to save
     * @param profile Optional column profile to save
     * @return true if successful
     */
    static bool savePortfolio(const QString &filename, const QList<ScripData> &scrips, const GenericTableProfile &profile = GenericTableProfile());

    /**
     * @brief Load portfolio from file
     * @param filename Path to load file
     * @param scrips Output list of scrips
     * @return true if successful
     */
    /**
     * @brief Convert ScripData to JSON object
     */
    static QJsonObject scripToJson(const ScripData &scrip);

    /**
     * @brief Create ScripData from JSON object
     */
    static ScripData scripFromJson(const QJsonObject &json);

    static bool loadPortfolio(const QString &filename, QList<ScripData> &scrips, GenericTableProfile &profile);
};

#endif // MARKETWATCHHELPERS_H
