/**
 * Simplified SearchTokenizer Unit Test
 * Tests token parsing without requiring full repository infrastructure
 */

#include "SearchTokenizer.h"
#include <QCoreApplication>
#include <QDebug>
#include <iostream>

// Simple test result tracking
struct TestResults {
    int passed = 0;
    int failed = 0;
    
    void pass(const QString& testName) {
        passed++;
        qInfo() << "[PASS]" << testName;
    }
    
    void fail(const QString& testName, const QString& reason) {
        failed++;
        qWarning() << "[FAIL]" << testName << "-" << reason;
    }
    
    void summary() {
        qInfo() << "\n===== Test Summary =====";
        qInfo() << "Passed:" << passed;
        qInfo() << "Failed:" << failed;
        qInfo() << "Total: " << (passed + failed);
        qInfo() << "Success Rate:" << QString::number(100.0 * passed / (passed + failed), 'f', 1) << "%";
    }
};

TestResults results;

void testSymbolOnly() {
    auto parsed = SearchTokenizer::parse("nifty");
    
    if (parsed.symbol == "NIFTY" && parsed.strike == 0.0 && parsed.optionType == 0) {
        results.pass("Symbol only - nifty");
    } else {
        results.fail("Symbol only - nifty", "Expected symbol=NIFTY, got=" + parsed.symbol);
    }
}

void testSymbolStrike() {
    auto parsed = SearchTokenizer::parse("nifty 26000");
    
    if (parsed.symbol == "NIFTY" && qAbs(parsed.strike - 26000.0) < 0.01) {
        results.pass("Symbol + Strike - nifty 26000");
    } else {
        results.fail("Symbol + Strike", QString("Expected symbol=NIFTY, strike=26000, got symbol=%1, strike=%2").arg(parsed.symbol).arg(parsed.strike));
    }
}

void testStrikeSymbol() {
    auto parsed = SearchTokenizer::parse("26000 nifty");
    
    if (parsed.symbol == "NIFTY" && qAbs(parsed.strike - 26000.0) < 0.01) {
        results.pass("Strike + Symbol (reversed) - 26000 nifty");
    } else {
        results.fail("Strike + Symbol (reversed)", "Order should not matter");
    }
}

void testSymbolOptionType() {
    auto parsed = SearchTokenizer::parse("nifty ce");
    
    if (parsed.symbol == "NIFTY" && parsed.optionType == 3) {
        results.pass("Symbol + Option Type - nifty ce");
    } else {
        results.fail("Symbol + Option Type", QString("Expected optionType=3 (CE), got=%1").arg(parsed.optionType));
    }
}

void testOptionTypeSymbol() {
    auto parsed = SearchTokenizer::parse("ce nifty");
    
    if (parsed.symbol == "NIFTY" && parsed.optionType == 3) {
        results.pass("Option Type + Symbol (reversed) - ce nifty");
    } else {
        results.fail("Option Type + Symbol (reversed)", "Order should not matter");
    }
}

void testSymbolStrikeType() {
    auto parsed = SearchTokenizer::parse("nifty 26000 ce");
    
    if (parsed.symbol == "NIFTY" && qAbs(parsed.strike - 26000.0) < 0.01 && parsed.optionType == 3) {
        results.pass("Symbol + Strike + Type - nifty 26000 ce");
    } else {
        results.fail("Symbol + Strike + Type", "All tokens should be parsed");
    }
}

void testAllTokensMixedOrder1() {
    auto parsed = SearchTokenizer::parse("26000 ce nifty");
    
    if (parsed.symbol == "NIFTY" && qAbs(parsed.strike - 26000.0) < 0.01 && parsed.optionType == 3) {
        results.pass("Mixed order 1 - 26000 ce nifty");
    } else {
        results.fail("Mixed order 1", "Order should not matter");
    }
}

void testAllTokensMixedOrder2() {
    auto parsed = SearchTokenizer::parse("ce 26000 nifty");
    
    if (parsed.symbol == "NIFTY" && qAbs(parsed.strike - 26000.0) < 0.01 && parsed.optionType == 3) {
        results.pass("Mixed order 2 - ce 26000 nifty");
    } else {
        results.fail("Mixed order 2", "Order should not matter");
    }
}

void testAllTokensMixedOrder3() {
    auto parsed = SearchTokenizer::parse("nifty ce 26000");
    
    if (parsed.symbol == "NIFTY" && qAbs(parsed.strike - 26000.0) < 0.01 && parsed.optionType == 3) {
        results.pass("Mixed order 3 - nifty ce 26000");
    } else {
        results.fail("Mixed order 3", "Order should not matter");
    }
}

void testPutOption() {
    auto parsed = SearchTokenizer::parse("banknifty 50000 pe");
    
    if (parsed.symbol == "BANKNIFTY" && qAbs(parsed.strike - 50000.0) < 0.01 && parsed.optionType == 4) {
        results.pass("Put option - banknifty 50000 pe");
    } else {
        results.fail("Put option", QString("Expected optionType=4 (PE), got=%1").arg(parsed.optionType));
    }
}

void testExpiryShortMonth() {
    auto parsed = SearchTokenizer::parse("nifty 17feb");
    
    if (parsed.symbol == "NIFTY" && parsed.expiry.contains("FEB", Qt::CaseInsensitive)) {
        results.pass("Expiry short month - nifty 17feb");
    } else {
        results.fail("Expiry short month", "Expected expiry to contain FEB, got=" + parsed.expiry);
    }
}

void testExpiryCompact() {
    auto parsed = SearchTokenizer::parse("nifty 17feb2026");
    
    if (parsed.symbol == "NIFTY" && parsed.expiry.contains("17FEB", Qt::CaseInsensitive)) {
        results.pass("Expiry compact format - nifty 17feb2026");
    } else {
        results.fail("Expiry compact format", "Expected expiry to contain 17FEB, got=" + parsed.expiry);
    }
}

void testExpirySpaced() {
    auto parsed = SearchTokenizer::parse("nifty 17 feb 2026");
    
    if (parsed.symbol == "NIFTY" && parsed.expiry.contains("17FEB", Qt::CaseInsensitive)) {
        results.pass("Expiry spaced format - nifty 17 feb 2026");
    } else {
        results.fail("Expiry spaced format", "Expected expiry to contain 17FEB, got=" + parsed.expiry);
    }
}

void testCommodityExpiry() {
    auto parsed = SearchTokenizer::parse("gold 26feb");
    
    if (parsed.symbol == "GOLD" && parsed.expiry.contains("FEB", Qt::CaseInsensitive)) {
        results.pass("Commodity with expiry - gold 26feb");
    } else {
        results.fail("Commodity with expiry", "Failed to parse commodity + expiry");
    }
}

void testStrikeOnly() {
    auto parsed = SearchTokenizer::parse("26000");
    
    if (qAbs(parsed.strike - 26000.0) < 0.01 && parsed.symbol.isEmpty()) {
        results.pass("Strike only - 26000");
    } else {
        results.fail("Strike only", "Should parse as strike without symbol");
    }
}

void testOptionTypeOnly() {
    auto parsed = SearchTokenizer::parse("ce");
    
    if (parsed.optionType == 3 && parsed.symbol.isEmpty()) {
        results.pass("Option type only - ce");
    } else {
        results.fail("Option type only", "Should parse as option type without symbol");
    }
}

void testCaseInsensitive() {
    auto parsed1 = SearchTokenizer::parse("NIFTY");
    auto parsed2 = SearchTokenizer::parse("nifty");
    auto parsed3 = SearchTokenizer::parse("Nifty");
    
    if (parsed1.symbol == parsed2.symbol && parsed2.symbol == parsed3.symbol) {
        results.pass("Case insensitive - NIFTY/nifty/Nifty");
    } else {
        results.fail("Case insensitive", "Should handle any case");
    }
}

void testEmptyQuery() {
    auto parsed = SearchTokenizer::parse("");
    
    if (parsed.symbol.isEmpty() && parsed.strike == 0.0 && parsed.optionType == 0) {
        results.pass("Empty query");
    } else {
        results.fail("Empty query", "Should return empty result");
    }
}

void testMultiWordSymbol() {
    auto parsed = SearchTokenizer::parse("tata motors");
    
    if (parsed.symbol.contains("TATA") && parsed.symbol.contains("MOTORS")) {
        results.pass("Multi-word symbol - tata motors");
    } else {
        results.fail("Multi-word symbol", "Should combine symbol words, got=" + parsed.symbol);
    }
}

void testSeriesQuery() {
    auto parsed = SearchTokenizer::parse("reliance EQ");
    
    if (parsed.symbol.contains("RELIANCE")) {
        results.pass("Symbol with series - reliance EQ");
    } else {
        results.fail("Symbol with series", "Should parse symbol, got=" + parsed.symbol);
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    qInfo() << "\n========================================";
    qInfo() << "SearchTokenizer Unit Test Suite";
    qInfo() << "Testing Order-Independent Parsing";
    qInfo() << "========================================\n";
    
    // Run all tests
    testSymbolOnly();
    testSymbolStrike();
    testStrikeSymbol();
    testSymbolOptionType();
    testOptionTypeSymbol();
    testSymbolStrikeType();
    testAllTokensMixedOrder1();
    testAllTokensMixedOrder2();
    testAllTokensMixedOrder3();
    testPutOption();
    testExpiryShortMonth();
    testExpiryCompact();
    testExpirySpaced();
    testCommodityExpiry();
    testStrikeOnly();
    testOptionTypeOnly();
    testCaseInsensitive();
    testEmptyQuery();
    testMultiWordSymbol();
    testSeriesQuery();
    
    // Print summary
    results.summary();
    
    // Return exit code based on results
    return (results.failed == 0) ? 0 : 1;
}
