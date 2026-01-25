#include "repository/NSEFORepositoryPreSorted.h"
#include <QDebug>
#include <QDate>
#include <algorithm>

NSEFORepositoryPreSorted::NSEFORepositoryPreSorted() : NSEFORepository() {
}

bool NSEFORepositoryPreSorted::loadProcessedCSV(const QString &filename) {
    // Call parent implementation to load data into arrays
    bool success = NSEFORepository::loadProcessedCSV(filename);
    
    if (success) {
        qDebug() << "[PreSorted] Building multi-level indexes...";
        buildIndexes();
        qDebug() << "[PreSorted] Indexes built:"
                 << "Series:" << m_seriesIndex.size()
                 << "Symbols:" << m_symbolIndex.size()
                 << "Expiries:" << m_expiryIndex.size();
    }
    
    return success;
}

bool NSEFORepositoryPreSorted::loadFromContracts(const QVector<MasterContract> &contracts) {
    // Call parent implementation
    bool success = NSEFORepository::loadFromContracts(contracts);
    
    if (success) {
        buildIndexes();
    }
    
    return success;
}

// Helper: Convert expiry string to QDate for proper chronological comparison
static QDate expiryToDate(const QString& expiry) {
    // Format: "27JAN2026" -> QDate(2026, 1, 27)
    if (expiry.length() < 9) return QDate();
    
    QString day = expiry.left(2);
    QString month = expiry.mid(2, 3).toUpper();
    QString year = expiry.right(4);
    
    static QHash<QString, int> monthMap = {
        {"JAN", 1}, {"FEB", 2}, {"MAR", 3}, {"APR", 4},
        {"MAY", 5}, {"JUN", 6}, {"JUL", 7}, {"AUG", 8},
        {"SEP", 9}, {"OCT", 10}, {"NOV", 11}, {"DEC", 12}
    };
    
    int monthNum = monthMap.value(month, 0);
    if (monthNum == 0) return QDate();
    
    return QDate(year.toInt(), monthNum, day.toInt());
}

void NSEFORepositoryPreSorted::buildIndexes() {
    // Clear existing indexes
    m_seriesIndex.clear();
    m_symbolIndex.clear();
    m_expiryIndex.clear();
    
    // Reserve space for common case
    m_seriesIndex.reserve(10);      // ~4-6 series types
    m_symbolIndex.reserve(500);     // ~200-300 unique symbols
    m_expiryIndex.reserve(100);     // ~50-80 unique expiries
    
    // Build indexes by getting all contracts from parent
    QVector<ContractData> allContracts = getAllContracts();
    
    for (const auto& contract : allContracts) {
        int64_t token = contract.exchangeInstrumentID;
        
        // Index by series
        m_seriesIndex[contract.series].append(token);
        
        // Index by symbol
        m_symbolIndex[contract.name].append(token);
        
        // Index by expiry
        m_expiryIndex[contract.expiryDate].append(token);
    }
    
    qDebug() << "[PreSorted] Sorting index arrays by date...";
    sortIndexArrays();
    qDebug() << "[PreSorted] Arrays sorted!";
}

void NSEFORepositoryPreSorted::sortIndexArrays() {
    // Pre-compute date values for all contracts to avoid repeated conversions
    QHash<int64_t, QDate> dateCache;
    
    // Sort each symbol's token array by: Expiry (DATE) → InstrumentType → Strike → OptionType
    for (auto it = m_symbolIndex.begin(); it != m_symbolIndex.end(); ++it) {
        QVector<int64_t>& tokens = it.value();
        
        std::sort(tokens.begin(), tokens.end(), 
                  [this](int64_t tokenA, int64_t tokenB) {
                      const ContractData* a = getContract(tokenA);
                      const ContractData* b = getContract(tokenB);
                      
                      if (!a || !b) return tokenA < tokenB;
                      
                      // Primary: Expiry (by pre-parsed DATE)
                      if (a->expiryDate_dt != b->expiryDate_dt)
                          return a->expiryDate_dt < b->expiryDate_dt;
                      
                      // Secondary: InstrumentType (Futures=1, Options=2, Spreads=4)
                      if (a->instrumentType != b->instrumentType)
                          return a->instrumentType < b->instrumentType;
                      
                      // Tertiary: Strike Price (lowest first)
                      if (qAbs(a->strikePrice - b->strikePrice) > 0.001)
                          return a->strikePrice < b->strikePrice;
                      
                      // Quaternary: OptionType (CE before PE)
                      return a->optionType < b->optionType;
                  });
    }
    
    // Also sort series index
    for (auto it = m_seriesIndex.begin(); it != m_seriesIndex.end(); ++it) {
        QVector<int64_t>& tokens = it.value();
        
        std::sort(tokens.begin(), tokens.end(), 
                  [this](int64_t tokenA, int64_t tokenB) {
                      const ContractData* a = getContract(tokenA);
                      const ContractData* b = getContract(tokenB);
                      
                      if (!a || !b) return tokenA < tokenB;
                      
                      if (a->expiryDate_dt != b->expiryDate_dt)
                          return a->expiryDate_dt < b->expiryDate_dt;
                      if (a->name != b->name)
                          return a->name < b->name;
                      if (a->instrumentType != b->instrumentType)
                          return a->instrumentType < b->instrumentType;
                      if (qAbs(a->strikePrice - b->strikePrice) > 0.001)
                          return a->strikePrice < b->strikePrice;
                      return a->optionType < b->optionType;
                  });
    }
    
    // Sort expiry index by symbol → strike
    for (auto it = m_expiryIndex.begin(); it != m_expiryIndex.end(); ++it) {
        QVector<int64_t>& tokens = it.value();
        
        std::sort(tokens.begin(), tokens.end(), 
                  [this](int64_t tokenA, int64_t tokenB) {
                      const ContractData* a = getContract(tokenA);
                      const ContractData* b = getContract(tokenB);
                      
                      if (!a || !b) return tokenA < tokenB;
                      
                      if (a->name != b->name)
                          return a->name < b->name;
                      if (a->instrumentType != b->instrumentType)
                          return a->instrumentType < b->instrumentType;
                      if (qAbs(a->strikePrice - b->strikePrice) > 0.001)
                          return a->strikePrice < b->strikePrice;
                      return a->optionType < b->optionType;
                  });
    }
}

QVector<ContractData> NSEFORepositoryPreSorted::getContractsBySymbolAndExpiry(
    const QString &symbol, 
    const QString &expiry,
    int instrumentType) const {
    
    QVector<ContractData> results;
    
    // Step 1: O(1) hash lookup for symbol
    auto it = m_symbolIndex.find(symbol);
    if (it == m_symbolIndex.end()) {
        return results;
    }
    
    const QVector<int64_t>& tokens = it.value();
    QDate targetDate = expiryToDate(expiry);
    
    // Step 2: O(log n) binary search to find first contract with this expiry
    auto lower = std::lower_bound(
        tokens.begin(),
        tokens.end(),
        targetDate,
        [this](int64_t token, const QDate& target) {
            const ContractData* c = getContract(token);
            if (!c) return true;
            return c->expiryDate_dt < target;
        }
    );
    
    // Step 3: Scan forward while expiry matches (contracts are sorted by expiry first)
    for (auto it = lower; it != tokens.end(); ++it) {
        const ContractData* contract = getContract(*it);
        if (!contract) continue;
        
        // Stop when we hit a different expiry (since array is sorted by date)
        if (contract->expiryDate_dt != targetDate) {
            break;
        }
        
        // Filter by instrument type if specified
        if (instrumentType == -1 || contract->instrumentType == instrumentType) {
            results.append(*contract);
        }
    }
    
    // Results are already sorted by strike price - no sorting needed!
    return results;
}
