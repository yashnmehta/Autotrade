/**
 * Global Search Feature Test Script
 * Tests flexible multi-token parsing with various query patterns
 */

#include "repository/RepositoryManager.h"
#include "search/SearchTokenizer.h"
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QTextStream>
#include <iostream>

// ANSI color codes for terminal output
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

class GlobalSearchTester {
public:
    GlobalSearchTester() {
        m_repoManager = RepositoryManager::getInstance();
    }
    
    bool initialize() {
        qInfo() << "\n" << COLOR_BOLD << "=== Initializing Repository Manager ===" << COLOR_RESET;
        
        // Load master files
        bool success = m_repoManager->loadAllMasterFiles();
        
        if (success) {
            qInfo() << COLOR_GREEN << "✓ Repository loaded successfully" << COLOR_RESET;
        } else {
            qWarning() << COLOR_YELLOW << "⚠ Repository load failed" << COLOR_RESET;
        }
        
        return success;
    }
    
    void runTestSuite() {
        qInfo() << "\n" << COLOR_BOLD << "=== Global Search Test Suite ===" << COLOR_RESET;
        qInfo() << "Testing flexible multi-token parsing (order-independent)\n";
        
        // Test cases with expected characteristics
        struct TestCase {
            QString query;
            QString description;
            int expectedMinResults;
            QString expectedSymbol;
        };
        
        QVector<TestCase> testCases = {
            // Symbol only
            {"nifty", "Symbol only - NIFTY", 100, "NIFTY"},
            {"reliance", "Symbol only - RELIANCE", 10, "RELIANCE"},
            {"banknifty", "Symbol only - BANKNIFTY", 50, "BANKNIFTY"},
            
            // Symbol + Strike (any order)
            {"nifty 26000", "Symbol + Strike (standard order)", 5, "NIFTY"},
            {"26000 nifty", "Strike + Symbol (reversed order)", 5, "NIFTY"},
            {"50000 banknifty", "Strike + Symbol - BANKNIFTY", 5, "BANKNIFTY"},
            
            // Symbol + Option Type (any order)
            {"nifty ce", "Symbol + Option Type (CE)", 50, "NIFTY"},
            {"ce nifty", "Option Type + Symbol (reversed)", 50, "NIFTY"},
            {"banknifty pe", "Symbol + Option Type (PE)", 50, "BANKNIFTY"},
            
            // Symbol + Strike + Type (any order)
            {"nifty 26000 ce", "Symbol + Strike + Type", 2, "NIFTY"},
            {"26000 ce nifty", "Strike + Type + Symbol", 2, "NIFTY"},
            {"ce 26000 nifty", "Type + Strike + Symbol", 2, "NIFTY"},
            {"nifty ce 26000", "Symbol + Type + Strike", 2, "NIFTY"},
            
            // Symbol + Expiry (flexible formats)
            {"nifty 17feb", "Symbol + Expiry (short month)", 5, "NIFTY"},
            {"nifty 17feb2026", "Symbol + Expiry (compact)", 5, "NIFTY"},
            {"nifty 17 feb 2026", "Symbol + Expiry (spaced)", 5, "NIFTY"},
            {"gold 26feb", "Commodity + Expiry", 2, "GOLD"},
            
            // Symbol + Expiry + Strike (various orders)
            {"nifty 17feb 26000", "Symbol + Expiry + Strike", 2, "NIFTY"},
            {"nifty 26000 17feb", "Symbol + Strike + Expiry", 2, "NIFTY"},
            {"26000 nifty 17feb", "Strike + Symbol + Expiry", 2, "NIFTY"},
            
            // All tokens (any order)
            {"nifty 17feb 26000 ce", "All tokens (standard)", 1, "NIFTY"},
            {"26000 ce nifty 17feb", "All tokens (mixed order)", 1, "NIFTY"},
            {"ce 26000 17feb nifty", "All tokens (type first)", 1, "NIFTY"},
            
            // Series/Segment
            {"reliance EQ", "Symbol + Series", 1, "RELIANCE"},
            {"tata motors", "Multi-word symbol", 5, "TATA"},
            
            // Edge cases
            {"26000", "Strike only", 5, ""},
            {"ce", "Option type only", 50, ""},
            {"17feb", "Expiry only", 5, ""},
        };
        
        int passCount = 0;
        int failCount = 0;
        
        for (int i = 0; i < testCases.size(); ++i) {
            const TestCase& test = testCases[i];
            
            qInfo() << "\n" << COLOR_CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << COLOR_RESET;
            qInfo() << COLOR_BOLD << "Test" << (i + 1) << "/" << testCases.size() 
                    << ":" << COLOR_RESET << test.description;
            qInfo() << COLOR_BLUE << "Query:" << COLOR_RESET << "\"" << test.query << "\"";
            
            bool testPassed = testQuery(test.query, test.expectedMinResults, test.expectedSymbol);
            
            if (testPassed) {
                passCount++;
                qInfo() << COLOR_GREEN << "✓ PASS" << COLOR_RESET;
            } else {
                failCount++;
                qInfo() << COLOR_YELLOW << "✗ FAIL" << COLOR_RESET;
            }
        }
        
        // Summary
        qInfo() << "\n" << COLOR_BOLD << "=== Test Summary ===" << COLOR_RESET;
        qInfo() << "Total Tests:" << testCases.size();
        qInfo() << COLOR_GREEN << "Passed:" << passCount << COLOR_RESET;
        if (failCount > 0) {
            qInfo() << COLOR_YELLOW << "Failed:" << failCount << COLOR_RESET;
        }
        qInfo() << "Pass Rate:" << QString::number(100.0 * passCount / testCases.size(), 'f', 1) << "%\n";
    }
    
    bool testQuery(const QString& query, int expectedMinResults, const QString& expectedSymbol) {
        QElapsedTimer timer;
        timer.start();
        
        // Parse tokens first to show what the tokenizer extracted
        auto parsed = SearchTokenizer::parse(query);
        
        qInfo() << COLOR_CYAN << "Parsed Tokens:" << COLOR_RESET;
        qInfo() << "  Symbol:" << (parsed.symbol.isEmpty() ? "(none)" : parsed.symbol);
        qInfo() << "  Expiry:" << (parsed.expiry.isEmpty() ? "(none)" : parsed.expiry);
        qInfo() << "  Strike:" << (parsed.strike > 0 ? QString::number(parsed.strike) : "(none)");
        qInfo() << "  Option Type:" << (parsed.optionType == 3 ? "CE" : 
                                        parsed.optionType == 4 ? "PE" : "(none)");
        
        // Perform search
        QVector<ContractData> results = m_repoManager->searchScripsGlobal(
            query, "", "", "", 20  // Get top 20 results
        );
        
        qint64 elapsed = timer.nsecsElapsed() / 1000;  // microseconds
        
        qInfo() << COLOR_CYAN << "Search Results:" << COLOR_RESET << results.size() << "found";
        qInfo() << "Time:" << QString::number(elapsed / 1000.0, 'f', 2) << "ms";
        
        // Display top results
        int displayCount = qMin(results.size(), 10);
        if (displayCount > 0) {
            qInfo() << "\n" << COLOR_BOLD << "Top" << displayCount << "Results:" << COLOR_RESET;
            for (int i = 0; i < displayCount; ++i) {
                const ContractData& contract = results[i];
                
                QString resultLine = QString("  %1. %2")
                    .arg(i + 1, 2)
                    .arg(formatContract(contract));
                
                qInfo().noquote() << resultLine;
            }
        } else {
            qInfo() << COLOR_YELLOW << "  (No results found)" << COLOR_RESET;
        }
        
        // Validate results
        bool hasMinResults = results.size() >= expectedMinResults;
        bool hasExpectedSymbol = expectedSymbol.isEmpty() || 
                                 (results.size() > 0 && 
                                  results[0].name.contains(expectedSymbol, Qt::CaseInsensitive));
        
        if (!hasMinResults) {
            qInfo() << COLOR_YELLOW << "⚠ Expected at least" << expectedMinResults 
                    << "results, got" << results.size() << COLOR_RESET;
        }
        
        if (!expectedSymbol.isEmpty() && !hasExpectedSymbol) {
            qInfo() << COLOR_YELLOW << "⚠ Expected top result to contain symbol:" 
                    << expectedSymbol << COLOR_RESET;
        }
        
        return hasMinResults && hasExpectedSymbol;
    }
    
    void interactiveMode() {
        qInfo() << "\n" << COLOR_BOLD << "=== Interactive Search Mode ===" << COLOR_RESET;
        qInfo() << "Enter search queries to test (or 'quit' to exit)\n";
        
        QTextStream input(stdin);
        
        while (true) {
            std::cout << COLOR_BOLD << "Search> " << COLOR_RESET;
            std::cout.flush();
            
            QString query = input.readLine().trimmed();
            
            if (query.isEmpty()) {
                continue;
            }
            
            if (query.toLower() == "quit" || query.toLower() == "exit" || query.toLower() == "q") {
                qInfo() << "\nExiting interactive mode...";
                break;
            }
            
            qInfo() << "";
            testQuery(query, 0, "");
            qInfo() << "";
        }
    }
    
private:
    QString formatContract(const ContractData& contract) {
        QString exchange = (contract.exchangeInstrumentID >= 11000000) ? "BSE" : "NSE";
        QString segment = (contract.strikePrice > 0 || contract.instrumentType == 1) ? "FO" : "CM";
        
        QString result = QString("%1 · %2 %3")
            .arg(contract.name)
            .arg(exchange)
            .arg(segment);
        
        // Add F&O details
        if (segment == "FO") {
            if (contract.strikePrice > 0) {
                // Options
                QString optType = contract.optionType.contains("C", Qt::CaseInsensitive) ? "CE" : "PE";
                result += QString(" · %1 %2").arg(contract.strikePrice, 0, 'f', 2).arg(optType);
            } else {
                // Futures
                result += " · FUT";
            }
            
            if (!contract.expiryDate.isEmpty()) {
                result += QString(" · Exp: %1").arg(contract.expiryDate.left(7));
            }
        } else {
            // Cash market
            if (!contract.series.isEmpty()) {
                result += QString(" · %1").arg(contract.series);
            }
        }
        
        result += QString(" (Token: %1)").arg(contract.exchangeInstrumentID);
        
        return result;
    }
    
    RepositoryManager* m_repoManager;
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    qInfo() << COLOR_BOLD << "\n╔════════════════════════════════════════════════════════╗";
    qInfo() << "║  Global Search Feature Test Script                    ║";
    qInfo() << "║  Flexible Multi-Token Parsing                         ║";
    qInfo() << "╚════════════════════════════════════════════════════════╝" << COLOR_RESET << "\n";
    
    GlobalSearchTester tester;
    
    // Initialize repository
    if (!tester.initialize()) {
        qCritical() << "Failed to initialize repository. Exiting...";
        return 1;
    }
    
    // Check command line arguments
    QStringList args = app.arguments();
    
    bool interactive = args.contains("--interactive") || args.contains("-i");
    bool autoTest = args.contains("--test") || args.contains("-t") || args.size() == 1;
    
    if (autoTest && !interactive) {
        // Run automated test suite
        tester.runTestSuite();
    }
    
    if (interactive || (!autoTest)) {
        // Run interactive mode
        tester.interactiveMode();
    }
    
    return 0;
}
