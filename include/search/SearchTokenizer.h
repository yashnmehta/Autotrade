#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QSet>

class SearchTokenizer {
public:
    struct ParsedTokens {
        QString symbol;              // "NIFTY"
        QString expiry;              // "17FEB2026" (empty if not found)
        double strike = 0.0;         // 26000.0 (0.0 if not found)
        int optionType = 0;          // 3=CE, 4=PE, 0=unknown
        QStringList rawTokens;       // Original split tokens (uppercased)
    };

    // Parse the query into ParsedTokens. Handles tokens in any order.
    static ParsedTokens parse(const QString& query);

private:
    enum TokenType {
        SYMBOL,
        NUMBER,
        OPTION_TYPE,
        MONTH
    };

    struct ClassifiedToken {
        QString text;
        TokenType type;
        double numericValue = 0.0; // for NUMBER: numeric value; for OPTION_TYPE: 3 or 4
    };

    static QVector<ClassifiedToken> classifyTokens(const QStringList& tokens);
    static bool isMonthName(const QString& token);
    static bool isOptionType(const QString& token, int& optionTypeOut);
    static QString tryParseExpiryFromClassified(const QVector<ClassifiedToken>& classified);
};
