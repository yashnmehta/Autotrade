/**
 * Test program to verify master loading fixes
 * Tests the new loadFromContracts() method
 */

#include "../include/repository/RepositoryManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    qDebug() << "=== Master Loading Test ===\n";
    
    // Setup path to Masters directory  
    // For test: build/tests/test_master_loading -> need ../TradingTerminal.app/Masters
    QString mastersDir = QCoreApplication::applicationDirPath() + "/../TradingTerminal.app/Masters";
    QDir dir(mastersDir);
    if (!dir.exists()) {
        qCritical() << "Masters directory not found:" << dir.absolutePath();
        qDebug() << "Looking from:" << QCoreApplication::applicationDirPath();
        return 1;
    }
    mastersDir = dir.absolutePath();  // Normalize path
    
    qDebug() << "Masters directory:" << mastersDir;
    qDebug() << "Testing new loadFromContracts() implementation\n";
    
    // Get repository manager instance
    RepositoryManager* repo = RepositoryManager::getInstance();
    
    // Load masters
    qDebug() << "Loading masters...";
    bool success = repo->loadAll(mastersDir);
    
    if (!success) {
        qCritical() << "Failed to load masters!";
        return 1;
    }
    
    qDebug() << "\n=== Loading Results ===";
    RepositoryManager::SegmentStats stats = repo->getSegmentStats();
    qDebug() << "NSE FO contracts:" << stats.nsefo;
    qDebug() << "NSE CM contracts:" << stats.nsecm;
    qDebug() << "BSE FO contracts:" << stats.bsefo;
    qDebug() << "BSE CM contracts:" << stats.bsecm;
    
    // Check if processed CSVs were created
    QString csvDir = mastersDir + "/processed_csv";
    QDir csvDirObj(csvDir);
    
    qDebug() << "\n=== Processed CSV Files ===";
    QStringList csvFiles = csvDirObj.entryList(QStringList() << "*.csv", QDir::Files);
    for (const QString& file : csvFiles) {
        QString filePath = csvDir + "/" + file;
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly)) {
            int lineCount = 0;
            QTextStream in(&f);
            while (!in.atEnd()) {
                in.readLine();
                lineCount++;
            }
            f.close();
            qDebug() << file << ":" << (lineCount - 1) << "contracts (+" << 1 << "header)";
        }
    }
    
    qDebug() << "\n=== Test Summary ===";
    int totalContracts = repo->getTotalContractCount();
    qDebug() << "Total contracts loaded:" << totalContracts;
    
    // Expected: ~109,303 contracts (87,282 NSEFO + 8,777 NSECM + 13,244 BSECM)
    // Since BSE not implemented yet, expect: ~96,059 (87,282 + 8,777)
    if (totalContracts >= 95000) {
        qDebug() << "✅ SUCCESS: All expected contracts loaded!";
        
        // Verify NSECM has data (should be ~8,777 contracts)
        if (stats.nsecm > 8000) {
            qDebug() << "✅ NSECM Fix Verified: NSECM contracts loaded correctly!";
        } else {
            qWarning() << "⚠️ NSECM Issue: Expected ~8,777 NSECM contracts, got" << stats.nsecm;
        }
        
        return 0;
    } else {
        qCritical() << "❌ FAILED: Expected ~96,000 contracts, got" << totalContracts;
        return 1;
    }
}
