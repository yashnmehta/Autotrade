#ifndef TIME_TO_EXPIRY_H
#define TIME_TO_EXPIRY_H

#include <QDate>
#include <QSet>
#include <QString>
#include <QTime>

/**
 * @brief Time-to-expiry calculator for Indian equity derivatives
 *
 * Provides accurate T (time in years) computation for Black-Scholes
 * and other pricing models, using NSE trading-day calendar.
 *
 * Two modes:
 *  - Calendar days: T = days / 365.0  (simple, for UI calculators)
 *  - Trading days:  T = tradingDays / 252.0  (accurate, for live Greeks)
 *
 * Usage:
 * @code
 *   // Quick calendar-day calculation
 *   double T = TimeToExpiry::calendarDays("27MAR2026");
 *
 *   // Accurate trading-day calculation with intraday fraction
 *   double T = TimeToExpiry::tradingDays("27MAR2026");
 *
 *   // From QDate
 *   double T = TimeToExpiry::tradingDays(QDate(2026, 3, 27));
 * @endcode
 */
class TimeToExpiry {
public:
    // ===== HIGH-LEVEL API =====

    /**
     * @brief Calendar-day based T (simple: days / 365.0)
     * @param expiryDate Expiry date string (DDMMMYYYY, DD-MMM-YYYY, or YYYY-MM-DD)
     * @return Time to expiry in years, minimum 0.0001
     */
    static double calendarDays(const QString &expiryDate);

    /**
     * @brief Calendar-day based T from QDate
     */
    static double calendarDays(const QDate &expiry);

    /**
     * @brief Trading-day based T (accurate: tradingDays / 252.0)
     *
     * Excludes weekends and NSE holidays. Adds intraday fraction
     * if market is currently open.
     *
     * @param expiryDate Expiry date string (DDMMMYYYY, DD-MMM-YYYY, or YYYY-MM-DD)
     * @return Time to expiry in years, minimum 0.0001
     */
    static double tradingDays(const QString &expiryDate);

    /**
     * @brief Trading-day based T from QDate
     */
    static double tradingDays(const QDate &expiry);

    // ===== LOW-LEVEL UTILITIES =====

    /**
     * @brief Parse expiry date from string
     *
     * Tries formats: DDMMMYYYY, DD-MMM-YYYY, YYYY-MM-DD
     * @return Parsed QDate (check isValid())
     */
    static QDate parseExpiryDate(const QString &expiryDate);

    /**
     * @brief Count trading days between two dates (inclusive)
     *
     * Excludes weekends (Sat/Sun) and NSE holidays.
     */
    static int countTradingDays(const QDate &start, const QDate &end);

    /**
     * @brief Check if a date is an NSE trading day
     *
     * Returns false for weekends and NSE holidays.
     */
    static bool isNSETradingDay(const QDate &date);

    /**
     * @brief Get intraday fraction of the current trading day remaining
     *
     * Returns fraction of day remaining until market close (15:30 IST).
     * Returns 0 if market is closed.
     */
    static double intradayFraction();

    /**
     * @brief Get/reload NSE holiday set
     *
     * Call setHolidays() to override the default built-in holiday list.
     */
    static const QSet<QDate> &holidays();
    static void setHolidays(const QSet<QDate> &holidays);

private:
    static QSet<QDate> s_holidays;
    static bool s_holidaysLoaded;

    static void ensureHolidaysLoaded();
    static void loadDefaultHolidays();
};

#endif // TIME_TO_EXPIRY_H
