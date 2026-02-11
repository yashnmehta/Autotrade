#include "SearchTokenizer.h"

#include <QRegularExpression>
#include <QDate>
#include <QStringView>

SearchTokenizer::ParsedTokens SearchTokenizer::parse(const QString& query) {
    ParsedTokens result;
    if (query.simplified().isEmpty()) return result;

    // Normalize and split tokens (uppercased)
    QStringList raw = query.simplified().toUpper().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    result.rawTokens = raw;

    QVector<ClassifiedToken> classified = classifyTokens(raw);

    // Collect symbol parts and number tokens separately
    QStringList symbolParts;
    QVector<double> numberTokens;

    for (const ClassifiedToken& ct : classified) {
        switch (ct.type) {
        case SYMBOL:
            symbolParts.append(ct.text);
            break;
        case MONTH:
            // treat as symbol-part temporarily (expiry parsing will re-evaluate)
            symbolParts.append(ct.text);
            break;
        case NUMBER:
            numberTokens.append(ct.numericValue);
            break;
        case OPTION_TYPE:
            result.optionType = static_cast<int>(ct.numericValue);
            break;
        }
    }

    // Attempt expiry parse from classified tokens (more precise)
    QString expiry = tryParseExpiryFromClassified(classified);
    if (!expiry.isEmpty()) {
        result.expiry = expiry;
        // Remove expiry-related tokens from symbolParts
        // (tryParseExpiryFromClassified favours explicit month/year tokens so symbolParts ok)
    }

    // Determine strike: choose largest numeric >= 100 (avoid treating day as strike)
    double chosenStrike = 0.0;
    for (double n : numberTokens) {
        if (n >= 100.0 && n > chosenStrike) chosenStrike = n;
    }
    if (chosenStrike > 0.0) result.strike = chosenStrike;

    // Build symbol from remaining SYMBOL tokens (join by space)
    // Remove tokens that look like month names if they were accidentally added
    QStringList filteredSymbol;
    for (const QString& p : symbolParts) {
        if (!isMonthName(p)) filteredSymbol.append(p);
    }
    if (!filteredSymbol.isEmpty()) result.symbol = filteredSymbol.join(' ');

    return result;
}

QVector<SearchTokenizer::ClassifiedToken> SearchTokenizer::classifyTokens(const QStringList& tokens) {
    QVector<ClassifiedToken> out;
    out.reserve(tokens.size());

    for (const QString& t : tokens) {
        ClassifiedToken ct;
        ct.text = t;

        bool isNum = false;
        double num = t.toDouble(&isNum);
        if (isNum) {
            ct.type = NUMBER;
            ct.numericValue = num;
            out.append(ct);
            continue;
        }

        int optType = 0;
        if (isOptionType(t, optType)) {
            ct.type = OPTION_TYPE;
            ct.numericValue = optType;
            out.append(ct);
            continue;
        }

        if (isMonthName(t)) {
            ct.type = MONTH;
            out.append(ct);
            continue;
        }

        // Compact expiry like 17FEB2026 or 17-FEB-2026
        QRegularExpression compactRe("^(\\d{1,2})\\D*([A-Z]{3})\\D*(\\d{4})$");
        QRegularExpressionMatch m = compactRe.match(t);
        if (m.hasMatch()) {
            // Convert to a sequence of classified tokens: day, month, year
            ClassifiedToken dayCt; dayCt.text = m.captured(1); dayCt.type = NUMBER; dayCt.numericValue = m.captured(1).toDouble(); out.append(dayCt);
            ClassifiedToken monCt; monCt.text = m.captured(2); monCt.type = MONTH; out.append(monCt);
            ClassifiedToken yrCt; yrCt.text = m.captured(3); yrCt.type = NUMBER; yrCt.numericValue = m.captured(3).toDouble(); out.append(yrCt);
            continue;
        }

        // Default: SYMBOL
        ct.type = SYMBOL;
        out.append(ct);
    }

    return out;
}

bool SearchTokenizer::isMonthName(const QString& token) {
    static const QSet<QString> months = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
    if (token.size() == 3 && months.contains(token)) return true;
    // Accept full month names optionally
    QString upper = token.left(3).toUpper();
    return months.contains(upper);
}

bool SearchTokenizer::isOptionType(const QString& token, int& optionTypeOut) {
    if (token == "CE" || token == "CALL" || token == "C") { optionTypeOut = 3; return true; }
    if (token == "PE" || token == "PUT" || token == "P")  { optionTypeOut = 4; return true; }
    return false;
}

QString SearchTokenizer::tryParseExpiryFromClassified(const QVector<ClassifiedToken>& classified) {
    // Collect numeric and month tokens with their positions
    struct Item { int idx; const ClassifiedToken* ct; };
    QVector<Item> months, numbers;
    for (int i = 0; i < classified.size(); ++i) {
        const ClassifiedToken& c = classified[i];
        if (c.type == MONTH) months.append({i, &c});
        else if (c.type == NUMBER) numbers.append({i, &c});
    }

    // Heuristic 1: day + month + year (any order)
    if (!months.isEmpty() && numbers.size() >= 2) {
        // Find candidate year (4-digit between 2020..2040), day (1..31)
        int yearIdx = -1; int dayIdx = -1;
        for (int i = 0; i < numbers.size(); ++i) {
            double v = numbers[i].ct->numericValue;
            if (v >= 2020 && v <= 2040 && yearIdx == -1) yearIdx = numbers[i].idx;
        }
        for (int i = 0; i < numbers.size(); ++i) {
            double v = numbers[i].ct->numericValue;
            if (v >= 1 && v <= 31 && dayIdx == -1) dayIdx = numbers[i].idx;
        }
        if (yearIdx != -1 && dayIdx != -1) {
            // month choose first month token
            QString dayStr = classified[dayIdx].text;
            QString monStr = classified[months.first().idx].text.left(3).toUpper();
            QString yearStr = QString::number(static_cast<int>(classified[yearIdx].numericValue));
            return QString("%1%2%3").arg(dayStr.rightJustified(2, '0')).arg(monStr).arg(yearStr);
        }
    }

    // Heuristic 2: month + year (e.g., FEB 2026)
    if (!months.isEmpty() && numbers.size() >= 1) {
        // find a year-like number
        for (const Item& n : numbers) {
            int v = static_cast<int>(n.ct->numericValue);
            if (v >= 2020 && v <= 2040) {
                QString monStr = classified[months.first().idx].text.left(3).toUpper();
                return QString("01%1%2").arg(monStr).arg(v); // default day=01
            }
        }
    }

    // Heuristic 3: single compact number that looks like YYYYMMDD or DDMMYYYY or DDMMMYYYY already handled earlier
    // Give up if not found
    return QString();
}
