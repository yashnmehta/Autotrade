#include "utils/DateUtils.h"
#include <QDate>
#include <QDebug>
#include <QRegularExpression>


// Month names for DDMMMYYYY format
const QStringList DateUtils::MONTH_NAMES = {"",    "JAN", "FEB", "MAR", "APR",
                                            "MAY", "JUN", "JUL", "AUG", "SEP",
                                            "OCT", "NOV", "DEC"};

bool DateUtils::parseExpiryDate(const QString &input, QString &outDDMMMYYYY,
                                QDate &outDate, double &outTimeToExpiry) {
  if (input.isEmpty() || input == "N/A") {
    outDDMMMYYYY = "";
    outDate = QDate();
    outTimeToExpiry = 0.0;
    return false;
  }

  QString year, month, day;

  // ===== FORMAT 1: ISO format with 'T' (e.g., "2024-12-26T00:00:00") =====
  int tIdx = input.indexOf('T');
  if (tIdx != -1) {
    int d1 = input.indexOf('-');
    int d2 = input.indexOf('-', d1 + 1);
    if (d1 != -1 && d2 != -1 && d2 < tIdx) {
      year = input.mid(0, d1);
      month = input.mid(d1 + 1, d2 - d1 - 1);
      day = input.mid(d2 + 1, tIdx - d2 - 1);
    }
  }
  // ===== FORMAT 2: YYYYMMDD (e.g., "20241226") =====
  else if (input.length() == 8 && input.at(0).isDigit()) {
    year = input.mid(0, 4);
    month = input.mid(4, 2);
    day = input.mid(6, 2);
  }
  // ===== FORMAT 3: YYYY-MM-DD (e.g., "2024-12-26") =====
  else if (input.contains('-')) {
    QStringList parts = input.split('-');
    if (parts.size() == 3) {
      // Check if it's YYYY-MM-DD or DD-MM-YYYY
      if (parts[0].length() == 4) {
        // YYYY-MM-DD
        year = parts[0];
        month = parts[1];
        day = parts[2];
      } else {
        // DD-MM-YYYY
        day = parts[0];
        month = parts[1];
        year = parts[2];
      }
    }
  }
  // ===== FORMAT 4: DD/MM/YYYY (e.g., "26/12/2024") =====
  else if (input.contains('/')) {
    QStringList parts = input.split('/');
    if (parts.size() == 3) {
      day = parts[0];
      month = parts[1];
      year = parts[2];
    }
  }
  // ===== FORMAT 5: Already in DDMMMYYYY (e.g., "26DEC2024") =====
  else if (input.length() >= 9 && input.at(0).isDigit() &&
           input.at(2).isLetter()) {
    // Already in DDMMMYYYY format
    outDDMMMYYYY = input;

    // Parse to QDate for validation
    day = input.mid(0, 2);
    QString monthStr = input.mid(2, 3).toUpper();
    year = input.mid(5);

    int monthNum = MONTH_NAMES.indexOf(monthStr);
    if (monthNum >= 1 && monthNum <= 12) {
      outDate = QDate(year.toInt(), monthNum, day.toInt());

      if (outDate.isValid()) {
        // Calculate timeToExpiry
        QDate today = QDate::currentDate();
        if (outDate >= today) {
          int daysToExpiry = today.daysTo(outDate);
          outTimeToExpiry = daysToExpiry / 365.0;
        } else {
          outTimeToExpiry = 0.0; // Expired
        }
        return true;
      }
    }

    // Invalid DDMMMYYYY format
    outDDMMMYYYY = "";
    outDate = QDate();
    outTimeToExpiry = 0.0;
    return false;
  }

  // ===== Convert to DDMMMYYYY =====
  if (!year.isEmpty() && !month.isEmpty() && !day.isEmpty()) {
    int monthNum = month.toInt();
    if (monthNum >= 1 && monthNum <= 12) {
      // Ensure day has leading zero
      if (day.length() == 1) {
        day = "0" + day;
      }

      outDDMMMYYYY = day + MONTH_NAMES[monthNum] + year;
      outDate = QDate(year.toInt(), monthNum, day.toInt());

      // Validate QDate
      if (!outDate.isValid()) {
        qWarning() << "[DateUtils] Invalid date parsed:" << input
                   << "-> Year:" << year << "Month:" << monthNum
                   << "Day:" << day;
        outDDMMMYYYY = "";
        outDate = QDate();
        outTimeToExpiry = 0.0;
        return false;
      }

      // Calculate timeToExpiry: (expiry_date - trade_date) / 365.0
      QDate today = QDate::currentDate();
      if (outDate >= today) {
        int daysToExpiry = today.daysTo(outDate);
        outTimeToExpiry = daysToExpiry / 365.0;
      } else {
        outTimeToExpiry = 0.0; // Expired or invalid
      }

      return true;
    }
  }

  // Parsing failed
  qWarning() << "[DateUtils] Failed to parse date:" << input;
  outDDMMMYYYY = input; // Fallback to original
  outDate = QDate();
  outTimeToExpiry = 0.0;
  return false;
}

bool DateUtils::isValidDDMMMYYYY(const QString &date) {
  // Format: DDMMMYYYY (e.g., "26DEC2024")
  // DD = 2 digits, MMM = 3 letters, YYYY = 4 digits
  QRegularExpression regex("^\\d{2}[A-Z]{3}\\d{4}$");
  return regex.match(date).hasMatch();
}
