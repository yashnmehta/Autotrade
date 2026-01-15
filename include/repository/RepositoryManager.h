#ifndef REPOSITORY_MANAGER_H
#define REPOSITORY_MANAGER_H

#include "NSEFORepository.h"
#include "NSECMRepository.h"
#include "BSEFORepository.h"
#include "BSECMRepository.h"
#include "ContractData.h"
#include <QString>
#include <QVector>
#include <QHash>
#include <memory>

/**
 * @brief Unified Repository Manager for all exchange segments
 * 
 * Provides a single interface for accessing contracts across all exchanges:
 * - NSE F&O (Futures & Options)
 * - NSE CM (Cash Market)
 * - BSE F&O (Futures & Options)
 * - BSE CM (Cash Market)
 * 
 * This replaces API-based scrip search with efficient in-memory array-based search.
 * 
 * Usage:
 * ```cpp
 * RepositoryManager* repo = RepositoryManager::getInstance();
 * repo->loadAll();
 * 
 * // Search for scrips
 * QVector<ContractData> results = repo->searchScrips("NSE", "CM", "EQUITY", "REL");
 * ```
 */
class RepositoryManager {
public:
    /**
     * @brief Get singleton instance
     */
    /**
     * @brief Get singleton instance
     */
    static RepositoryManager* getInstance();
    
    // Repository Accessors
    const NSECMRepository* getNSECMRepository() const { return m_nsecm.get(); }
    const NSEFORepository* getNSEFORepository() const { return m_nsefo.get(); }
    const BSECMRepository* getBSECMRepository() const { return m_bsecm.get(); }
    const BSEFORepository* getBSEFORepository() const { return m_bsefo.get(); }
    
    /**
     * @brief Load all master contract files
     * @param mastersPath Path to Masters directory
     * @return true if at least one segment loaded successfully
     */
    bool loadAll(const QString& mastersPath = "Masters");
    
    // ===== SEGMENT-SPECIFIC LOADING =====
    
    /**
     * @brief Load NSE F&O contracts
     * @param preferCSV If true, try CSV first before master file
     */
    bool loadNSEFO(const QString& mastersPath, bool preferCSV = true);
    
    /**
     * @brief Load NSE CM contracts
     * @param preferCSV If true, try CSV first before master file
     */
    bool loadNSECM(const QString& mastersPath, bool preferCSV = true);
    
    /**
     * @brief Load BSE F&O contracts
     * @param preferCSV If true, try CSV first before master file
     */
    bool loadBSEFO(const QString& mastersPath, bool preferCSV = true);

    /**
     * @brief Load BSE CM contracts
     * @param preferCSV If true, try CSV first before master file
     */
    bool loadBSECM(const QString& mastersPath, bool preferCSV = true);
    
    /**
     * @brief Load combined master file (all segments in one file from XTS)
     * @param filePath Path to combined master file
     * @return true if at least one segment loaded successfully
     */
    bool loadCombinedMasterFile(const QString& filePath);
    
    /**
     * @brief Load masters directly from in-memory CSV data (no file I/O)
     * @param csvData Raw CSV data containing master contracts
     * @return true if at least one segment loaded successfully
     * 
     * This method is optimized for loading freshly downloaded data directly
     * from memory without writing to disk first. It parses the CSV data
     * and populates repositories directly, avoiding file I/O overhead.
     */
    bool loadFromMemory(const QString& csvData);
    
    /**
     * @brief Save processed CSVs for faster loading
     * @param mastersPath Path to Masters directory
     * @return true if at least one segment saved successfully
     */
    bool saveProcessedCSVs(const QString& mastersPath = "Masters");
    
    // ===== SEARCH METHODS (Array-Based, No API Calls) =====
    
    /**
     * @brief Search for scrips by symbol prefix
     * @param exchange Exchange name ("NSE" or "BSE")
     * @param segment Segment type ("CM" or "FO")
     * @param series Series type ("EQUITY", "OPTSTK", "FUTIDX", etc.)
     * @param searchText Search prefix (e.g., "REL" matches "RELIANCE")
     * @param maxResults Maximum number of results (default: 50)
     * @return Vector of matching contracts
     */
    QVector<ContractData> searchScrips(
        const QString& exchange,
        const QString& segment,
        const QString& series,
        const QString& searchText,
        int maxResults = 50
    ) const;
    
    /**
     * @brief Get all scrips for a segment and series
     * @param exchange Exchange name ("NSE" or "BSE")
     * @param segment Segment type ("CM" or "FO")
     * @param series Series type ("EQUITY", "OPTSTK", "FUTIDX", etc.)
     * @return Vector of all matching contracts
     */
    QVector<ContractData> getScrips(
        const QString& exchange,
        const QString& segment,
        const QString& series
    ) const;
    
    /**
     * @brief Get contract by token using exchange segment ID
     * @param exchangeSegmentID XTS segment ID (1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO)
     * @param token Exchange instrument ID
     * @return Pointer to contract (nullptr if not found)
     */
    const ContractData* getContractByToken(
        int exchangeSegmentID,
        int64_t token
    ) const;
    
    /**
     * @brief Get contract by token
     * @param exchange Exchange name ("NSE" or "BSE")
     * @param segment Segment type ("CM" or "FO")
     * @param token Exchange instrument ID
     * @return Pointer to contract (nullptr if not found)
     */
    /**
     * @brief Get contract by token using combined segment key (e.g., "NSEFO", "NSECM")
     * @param segmentKey Combined segment name
     * @param token Exchange instrument ID
     * @return Pointer to contract (nullptr if not found)
     */
    const ContractData* getContractByToken(
        const QString& segmentKey,
        int64_t token
    ) const;

    const ContractData* getContractByToken(
        const QString& exchange,
        const QString& segment,
        int64_t token
    ) const;
    
    /**
     * @brief Get all contracts for a specific underlying symbol (F&O)
     * @param exchange Exchange name ("NSE" or "BSE")
     * @param symbol Underlying symbol (e.g., "NIFTY", "BANKNIFTY")
     * @return Vector of all F&O contracts for the symbol
     */
    QVector<ContractData> getOptionChain(
        const QString& exchange,
        const QString& symbol
    ) const;
    
    /**
     * @brief Get all contracts for a specific segment
     * @param exchange "NSE" or "BSE"
     * @param segment "CM" or "FO"
     * @return Vector of all contracts in that segment
     */
    QVector<ContractData> getContractsBySegment(const QString& exchange, const QString& segment) const;
    
    // ===== UPDATE METHODS =====
    
    /**
     * @brief Update live market data for a contract
     * @param exchange Exchange name ("NSE" or "BSE")
     * @param segment Segment type ("CM" or "FO")
     * @param token Exchange instrument ID
     * @param ltp Last traded price
     * @param volume Volume
     */
    void updateLiveData(
        const QString& exchange,
        const QString& segment,
        int64_t token,
        double ltp,
        int64_t volume
    );
    
    /**
     * @brief Update bid/ask prices
     */
    void updateBidAsk(
        const QString& exchange,
        const QString& segment,
        int64_t token,
        double bidPrice,
        double askPrice
    );
    
    /**
     * @brief Update Greeks for an option contract
     */
    void updateGreeks(
        int64_t token,
        double iv,
        double delta,
        double gamma,
        double vega,
        double theta
    );
    
    // ===== STATISTICS =====
    
    /**
     * @brief Get total loaded contracts across all segments
     */
    int getTotalContractCount() const;
    
    /**
     * @brief Get contract count per segment
     */
    struct SegmentStats {
        int nsefo;
        int nsecm;
        int bsefo;
        int bsecm;
    };
    
    SegmentStats getSegmentStats() const;
    
    /**
     * @brief Check if repositories are loaded
     */
    bool isLoaded() const;
    
    // ===== EXCHANGE SEGMENT MAPPING =====
    
    /**
     * @brief Convert exchange + segment to internal repository key
     * @param exchange "NSE" or "BSE"
     * @param segment "CM" or "FO"
     * @return Internal key like "NSECM", "NSEFO", etc.
     */
    static QString getSegmentKey(const QString& exchange, const QString& segment);
    
    /**
     * @brief Get exchange segment ID for XTS API
     * @param exchange "NSE" or "BSE"
     * @param segment "CM" or "FO"
     * @return Segment ID (1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO)
     */
    static int getExchangeSegmentID(const QString& exchange, const QString& segment);
    
    /**
     * @brief Get exchange segment name from XTS segment ID
     * @param exchangeSegmentID XTS segment ID (1=NSECM, 2=NSEFO, etc.)
     * @return Segment name like "NSECM", "NSEFO", etc.
     */
    static QString getExchangeSegmentName(int exchangeSegmentID);

    
    /**
     * @brief Get OS-specific Masters directory path
     * 
     * Returns platform-appropriate path for master data:
     * - macOS .app bundle: <app>/Masters
     * - Linux/Dev: <app>/../../../Masters or ~/TradingTerminal/Masters
     * - Creates directory if it doesn't exist
     * 
     * @return Absolute path to Masters directory
     */
    static QString getMastersDirectory();

private:
    RepositoryManager();
    ~RepositoryManager();
    
    // Singleton instance
    static RepositoryManager* s_instance;
    
    // Segment repositories
    std::unique_ptr<NSEFORepository> m_nsefo;
    std::unique_ptr<NSECMRepository> m_nsecm;
    std::unique_ptr<BSEFORepository> m_bsefo;
    std::unique_ptr<BSECMRepository> m_bsecm;
    
    bool m_loaded;
};

#endif // REPOSITORY_MANAGER_H
