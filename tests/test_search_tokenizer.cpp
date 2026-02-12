#include <QtTest>
#include "../src/search/SearchTokenizer.h"

class TestSearchTokenizer : public QObject {
    Q_OBJECT

private slots:
    // Basic symbol-only searches
    void testSymbolOnly();
    void testMultiWordSymbol();
    
    // Symbol + Strike combinations
    void testSymbolStrike();
    void testStrikeSymbol(); // reverse order
    
    // Symbol + Option Type combinations
    void testSymbolOptionType();
    void testOptionTypeSymbol(); // reverse order
    
    // Symbol + Expiry combinations
    void testSymbolExpiryShortMonth();
    void testSymbolExpiryFullDate();
    void testSymbolExpiryCompactFormat();
    void testExpirySymbol(); // reverse order
    
    // Symbol + Strike + Option Type
    void testSymbolStrikeOptionType();
    void testSymbolOptionTypeStrike();
    void testStrikeOptionTypeSymbol();
    
    // Symbol + Expiry + Strike
    void testSymbolExpiryStrike();
    void testSymbolStrikeExpiry();
    
    // Full combinations (all tokens)
    void testAllTokensStandardOrder();
    void testAllTokensMixedOrder();
    
    // Special cases
    void testSymbolWithSeries(); // "reliance EQ"
    void testCommodityExpiry(); // "gold 26feb"
    void testOnlyExpiry();
    void testOnlyStrike();
    void testOnlyOptionType();
    
    // Edge cases
    void testEmptyQuery();
    void testWhitespaceOnly();
    void testSpecialCharacters();
    void testCaseInsensitivity();
    void testMultipleNumbers(); // disambiguate day/strike/year
};

void TestSearchTokenizer::testSymbolOnly() {
    auto tokens = SearchTokenizer::parse("nifty");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QVERIFY(tokens.expiry.isEmpty());
    QCOMPARE(tokens.strike, 0.0);
    QCOMPARE(tokens.optionType, 0);
}

void TestSearchTokenizer::testMultiWordSymbol() {
    auto tokens = SearchTokenizer::parse("bank nifty");
    // Should combine as multi-word symbol (though in practice might be "BANKNIFTY")
    QVERIFY(tokens.symbol.contains("BANK"));
    QVERIFY(tokens.symbol.contains("NIFTY"));
}

void TestSearchTokenizer::testSymbolStrike() {
    auto tokens = SearchTokenizer::parse("nifty 26000");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QCOMPARE(tokens.strike, 26000.0);
    QVERIFY(tokens.expiry.isEmpty());
    QCOMPARE(tokens.optionType, 0);
}

void TestSearchTokenizer::testStrikeSymbol() {
    auto tokens = SearchTokenizer::parse("26000 nifty");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QCOMPARE(tokens.strike, 26000.0);
}

void TestSearchTokenizer::testSymbolOptionType() {
    auto tokens = SearchTokenizer::parse("nifty ce");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QCOMPARE(tokens.optionType, 3); // CE = 3
    QCOMPARE(tokens.strike, 0.0);
}

void TestSearchTokenizer::testOptionTypeSymbol() {
    auto tokens = SearchTokenizer::parse("ce nifty");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QCOMPARE(tokens.optionType, 3);
}

void TestSearchTokenizer::testSymbolExpiryShortMonth() {
    auto tokens = SearchTokenizer::parse("nifty 17feb");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QVERIFY(!tokens.expiry.isEmpty());
    QVERIFY(tokens.expiry.contains("FEB"));
    // Should parse "17feb" as a date somehow (day + month, default year)
}

void TestSearchTokenizer::testSymbolExpiryFullDate() {
    auto tokens = SearchTokenizer::parse("nifty 17feb2026");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QVERIFY(!tokens.expiry.isEmpty());
    QVERIFY(tokens.expiry.contains("17"));
    QVERIFY(tokens.expiry.contains("FEB"));
    QVERIFY(tokens.expiry.contains("2026"));
}

void TestSearchTokenizer::testSymbolExpiryCompactFormat() {
    auto tokens = SearchTokenizer::parse("nifty 17-FEB-2026");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QVERIFY(!tokens.expiry.isEmpty());
    QVERIFY(tokens.expiry.contains("17"));
    QVERIFY(tokens.expiry.contains("FEB"));
    QVERIFY(tokens.expiry.contains("2026"));
}

void TestSearchTokenizer::testExpirySymbol() {
    auto tokens = SearchTokenizer::parse("17feb2026 nifty");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QVERIFY(!tokens.expiry.isEmpty());
}

void TestSearchTokenizer::testSymbolStrikeOptionType() {
    auto tokens = SearchTokenizer::parse("nifty 26000 ce");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QCOMPARE(tokens.strike, 26000.0);
    QCOMPARE(tokens.optionType, 3);
}

void TestSearchTokenizer::testSymbolOptionTypeStrike() {
    auto tokens = SearchTokenizer::parse("nifty ce 26000");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QCOMPARE(tokens.strike, 26000.0);
    QCOMPARE(tokens.optionType, 3);
}

void TestSearchTokenizer::testStrikeOptionTypeSymbol() {
    auto tokens = SearchTokenizer::parse("26000 ce nifty");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QCOMPARE(tokens.strike, 26000.0);
    QCOMPARE(tokens.optionType, 3);
}

void TestSearchTokenizer::testSymbolExpiryStrike() {
    auto tokens = SearchTokenizer::parse("nifty 17feb 26000");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QVERIFY(!tokens.expiry.isEmpty());
    QCOMPARE(tokens.strike, 26000.0);
}

void TestSearchTokenizer::testSymbolStrikeExpiry() {
    auto tokens = SearchTokenizer::parse("nifty 26000 17feb");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QCOMPARE(tokens.strike, 26000.0);
    QVERIFY(!tokens.expiry.isEmpty());
}

void TestSearchTokenizer::testAllTokensStandardOrder() {
    auto tokens = SearchTokenizer::parse("nifty 17feb2026 26000 ce");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QVERIFY(!tokens.expiry.isEmpty());
    QCOMPARE(tokens.strike, 26000.0);
    QCOMPARE(tokens.optionType, 3);
}

void TestSearchTokenizer::testAllTokensMixedOrder() {
    auto tokens = SearchTokenizer::parse("26000 ce 17feb2026 nifty");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QVERIFY(!tokens.expiry.isEmpty());
    QCOMPARE(tokens.strike, 26000.0);
    QCOMPARE(tokens.optionType, 3);
}

void TestSearchTokenizer::testSymbolWithSeries() {
    auto tokens = SearchTokenizer::parse("reliance EQ");
    // "EQ" is a series code, but tokenizer treats it as part of symbol
    // (series filtering happens at search level, not tokenization)
    QCOMPARE(tokens.symbol, QString("RELIANCE EQ"));
}

void TestSearchTokenizer::testCommodityExpiry() {
    auto tokens = SearchTokenizer::parse("gold 26feb");
    QCOMPARE(tokens.symbol, QString("GOLD"));
    QVERIFY(!tokens.expiry.isEmpty());
    QVERIFY(tokens.expiry.contains("FEB"));
}

void TestSearchTokenizer::testOnlyExpiry() {
    auto tokens = SearchTokenizer::parse("17feb2026");
    QVERIFY(tokens.symbol.isEmpty()); // No symbol provided
    QVERIFY(!tokens.expiry.isEmpty());
}

void TestSearchTokenizer::testOnlyStrike() {
    auto tokens = SearchTokenizer::parse("26000");
    QVERIFY(tokens.symbol.isEmpty());
    QCOMPARE(tokens.strike, 26000.0);
}

void TestSearchTokenizer::testOnlyOptionType() {
    auto tokens = SearchTokenizer::parse("ce");
    QVERIFY(tokens.symbol.isEmpty());
    QCOMPARE(tokens.optionType, 3);
}

void TestSearchTokenizer::testEmptyQuery() {
    auto tokens = SearchTokenizer::parse("");
    QVERIFY(tokens.symbol.isEmpty());
    QVERIFY(tokens.expiry.isEmpty());
    QCOMPARE(tokens.strike, 0.0);
    QCOMPARE(tokens.optionType, 0);
}

void TestSearchTokenizer::testWhitespaceOnly() {
    auto tokens = SearchTokenizer::parse("   ");
    QVERIFY(tokens.symbol.isEmpty());
}

void TestSearchTokenizer::testSpecialCharacters() {
    auto tokens = SearchTokenizer::parse("nifty-50 26000");
    // Should handle hyphens in symbol names
    QVERIFY(tokens.symbol.contains("NIFTY"));
    QCOMPARE(tokens.strike, 26000.0);
}

void TestSearchTokenizer::testCaseInsensitivity() {
    auto tokens1 = SearchTokenizer::parse("NIFTY 26000 CE");
    auto tokens2 = SearchTokenizer::parse("nifty 26000 ce");
    auto tokens3 = SearchTokenizer::parse("NiFtY 26000 Ce");
    
    QCOMPARE(tokens1.symbol, tokens2.symbol);
    QCOMPARE(tokens1.symbol, tokens3.symbol);
    QCOMPARE(tokens1.strike, tokens2.strike);
    QCOMPARE(tokens1.optionType, tokens2.optionType);
}

void TestSearchTokenizer::testMultipleNumbers() {
    // Query with day (17), strike (26000), year (2026)
    auto tokens = SearchTokenizer::parse("nifty 17 feb 2026 26000 ce");
    QCOMPARE(tokens.symbol, QString("NIFTY"));
    QVERIFY(!tokens.expiry.isEmpty());
    QVERIFY(tokens.expiry.contains("17"));
    QVERIFY(tokens.expiry.contains("FEB"));
    QVERIFY(tokens.expiry.contains("2026"));
    
    // Strike should be the largest number >= 100
    QCOMPARE(tokens.strike, 26000.0);
    QCOMPARE(tokens.optionType, 3);
}

QTEST_MAIN(TestSearchTokenizer)
#include "test_search_tokenizer.moc"
