#include "quant/TimeToExpiry.h"
#include <QDebug>
#include <algorithm>
#include <cmath>

// Static member initialization
QSet<QDate> TimeToExpiry::s_holidays;
bool TimeToExpiry::s_holidaysLoaded = false;

// ===== HIGH-LEVEL API =====

double TimeToExpiry::calendarDays(const QString &expiryDate) {
    QDate expiry = parseExpiryDate(expiryDate);
    if (!expiry.isValid()) {
        qWarning() << "[TimeToExpiry] Failed to parse expiry date:" << expiryDate;
        return 0.0001;
    }
    return calendarDays(expiry);
}

double TimeToExpiry::calendarDays(const QDate &expiry) {
    QDate today = QDate::currentDate();
    if (expiry < today) {
        return 0.0001; // Expired — return minimum
    }

    double days = static_cast<double>(today.daysTo(expiry));
    double T = days / 365.0;
    return std::max(T, 0.0001);
}

double TimeToExpiry::tradingDays(const QString &expiryDate) {
    QDate expiry = parseExpiryDate(expiryDate);
    if (!expiry.isValid()) {
        qWarning() << "[TimeToExpiry] Failed to parse expiry date:" << expiryDate;
        return 0.0001;
    }
    return tradingDays(expiry);
}

double TimeToExpiry::tradingDays(const QDate &expiry) {
    QDate today = QDate::currentDate();

    if (expiry < today) {
        return 0.0001; // Expired
    }

    // Count trading days from today to expiry
    int tDays = countTradingDays(today, expiry);

    // Add intraday component
    double intraday = intradayFraction();

    if (intraday > 0.0 && tDays > 0) {
        tDays--; // Don't double count today
    }

    // Convert to years (252 trading days per year in India)
    double T = (tDays + intraday) / 252.0;
    return std::max(T, 0.0001);
}

// ===== LOW-LEVEL UTILITIES =====

QDate TimeToExpiry::parseExpiryDate(const QString &expiryDate) {
    QDate expiry;

    // Try format: 27MAR2026
    expiry = QDate::fromString(expiryDate, "ddMMMyyyy");
    if (expiry.isValid()) return expiry;

    // Try format: 27-MAR-2026
    expiry = QDate::fromString(expiryDate, "dd-MMM-yyyy");
    if (expiry.isValid()) return expiry;

    // Try format: 2026-03-27
    expiry = QDate::fromString(expiryDate, "yyyy-MM-dd");
    if (expiry.isValid()) return expiry;

    // Try format: 27/03/2026
    expiry = QDate::fromString(expiryDate, "dd/MM/yyyy");
    if (expiry.isValid()) return expiry;

    return QDate(); // Invalid
}

int TimeToExpiry::countTradingDays(const QDate &start, const QDate &end) {
    ensureHolidaysLoaded();

    int count = 0;
    QDate current = start;

    while (current <= end) {
        if (isNSETradingDay(current)) {
            count++;
        }
        current = current.addDays(1);
    }

    return count;
}

bool TimeToExpiry::isNSETradingDay(const QDate &date) {
    ensureHolidaysLoaded();

    // Weekend check
    int dayOfWeek = date.dayOfWeek();
    if (dayOfWeek == Qt::Saturday || dayOfWeek == Qt::Sunday) {
        return false;
    }

    // Holiday check
    return !s_holidays.contains(date);
}

double TimeToExpiry::intradayFraction() {
    QTime now = QTime::currentTime();
    QTime marketClose(15, 30, 0); // NSE closes at 3:30 PM IST

    if (now >= marketClose) {
        return 0.0; // Market closed — no intraday fraction
    }

    int secondsRemaining = now.secsTo(marketClose);
    return static_cast<double>(secondsRemaining) / (24.0 * 60.0 * 60.0);
}

const QSet<QDate> &TimeToExpiry::holidays() {
    ensureHolidaysLoaded();
    return s_holidays;
}

void TimeToExpiry::setHolidays(const QSet<QDate> &holidays) {
    s_holidays = holidays;
    s_holidaysLoaded = true;
}

void TimeToExpiry::ensureHolidaysLoaded() {
    if (!s_holidaysLoaded) {
        loadDefaultHolidays();
        s_holidaysLoaded = true;
    }
}

void TimeToExpiry::loadDefaultHolidays() {
    // NSE Holidays 2026 (update annually or load from config)
    s_holidays = {
        QDate(2026, 1, 26),  // Republic Day
        QDate(2026, 3, 14),  // Holi
        QDate(2026, 3, 30),  // Good Friday
        QDate(2026, 4, 2),   // Ram Navami
        QDate(2026, 4, 14),  // Dr. Ambedkar Jayanti
        QDate(2026, 5, 1),   // Maharashtra Day
        QDate(2026, 8, 15),  // Independence Day
        QDate(2026, 8, 19),  // Janmashtami
        QDate(2026, 10, 2),  // Gandhi Jayanti
        QDate(2026, 10, 24), // Dussehra
        QDate(2026, 11, 12), // Diwali
        QDate(2026, 11, 13), // Diwali (Laxmi Pujan)
        QDate(2026, 11, 14), // Diwali (Balipratipada)
        QDate(2026, 12, 25), // Christmas
    };
}
