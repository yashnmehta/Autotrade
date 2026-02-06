#ifndef DATE_UTILS_H
#define DATE_UTILS_H

#include <QDate>
#include <QString>

/**
 * @brief Utility class for date parsing and formatting
 *
 * Centralizes date parsing logic used across repositories.
 * Ensures consistent DDMMMYYYY format and QDate conversion.
 */
class DateUtils {
public:
  /**
   * @brief Parse expiry date from various formats to standardized DDMMMYYYY
   *
   * Handles multiple input formats:
   * - ISO format: "2024-12-26T00:00:00"
   * - YYYYMMDD: "20241226"
   * - Already formatted: "26DEC2024"
   *
   * @param input Raw date string from master file/CSV
   * @param outDDMMMYYYY Output in DDMMMYYYY format (e.g., "26DEC2024")
   * @param outDate Output as QDate for sorting/comparison
   * @param outTimeToExpiry Output as time to expiry in years (for Greeks)
   * @return true if parsing successful, false otherwise
   *
   * Example:
   * ```cpp
   * QString formatted;
   * QDate date;
   * double tte;
   * if (DateUtils::parseExpiryDate("2024-12-26T00:00:00", formatted, date,
   * tte)) {
   *   // formatted = "26DEC2024"
   *   // date = QDate(2024, 12, 26)
   *   // tte = (days to expiry) / 365.0
   * }
   * ```
   */
  static bool parseExpiryDate(const QString &input, QString &outDDMMMYYYY,
                              QDate &outDate, double &outTimeToExpiry);

  /**
   * @brief Validate if string is in DDMMMYYYY format
   * @param date String to validate
   * @return true if format is DDMMMYYYY (e.g., "26DEC2024")
   */
  static bool isValidDDMMMYYYY(const QString &date);

private:
  // Month names for conversion
  static const QStringList MONTH_NAMES;
};

#endif // DATE_UTILS_H
