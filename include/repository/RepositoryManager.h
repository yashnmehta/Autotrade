#ifndef REPOSITORY_MANAGER_H
#define REPOSITORY_MANAGER_H

#include "BSECMRepository.h"
#include "BSEFORepository.h"
#include "ContractData.h"
#include "NSECMRepository.h"
#include "NSEFORepository.h"
#include <QObject>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVector>
#include <QReadWriteLock>
#include <memory>
#include <shared_mutex>

// Forward declare price stores
namespace nsefo {
class PriceStore;
class IndexStore;
} // namespace nsefo
namespace nsecm {
class PriceStore;
class IndexStore;
} // namespace nsecm
namespace bse {
class PriceStore;
class IndexStore;
} // namespace bse

/**
 * @brief Unified Repository Manager for all exchange segments
 *
 * Provides a single interface for accessing contracts across all exchanges:
 * - NSE F&O (Futures & Options)
 * - NSE CM (Cash Market)
 * - BSE F&O (Futures & Options)
 * - BSE CM (Cash Market)
 *
 * This replaces API-based scrip search with efficient in-memory array-based
 * search.
 *
 * Usage:
 * ```cpp
 * RepositoryManager* repo = RepositoryManager::getInstance();
 * repo->loadAll();
 *
 * // Search for scrips
 * QVector<ContractData> results = repo->searchScrips("NSE", "CM", "EQUITY",
 * "REL");
 * ```
 */
class RepositoryManager : public QObject {
  Q_OBJECT
public:
  /**
   * @brief Get singleton instance
   */
  /**
   * @brief Get singleton instance
   */
  static RepositoryManager *getInstance();

  // Repository Accessors
  const NSECMRepository *getNSECMRepository() const { return m_nsecm.get(); }
  const NSEFORepository *getNSEFORepository() const { return m_nsefo.get(); }
  const BSECMRepository *getBSECMRepository() const { return m_bsecm.get(); }
  const BSEFORepository *getBSEFORepository() const { return m_bsefo.get(); }

  /**
   * @brief Load all master contract files
   * @param mastersPath Path to Masters directory
   * @return true if at least one segment loaded successfully
   */
  bool loadAll(const QString &mastersPath = "Masters");

  // ===== SEGMENT-SPECIFIC LOADING =====

  /**
   * @brief Load NSE F&O contracts
   * @param preferCSV If true, try CSV first before master file
   */
  bool loadNSEFO(const QString &mastersPath, bool preferCSV = true);

  /**
   * @brief Load NSE CM contracts
   * @param preferCSV If true, try CSV first before master file
   */
  bool loadNSECM(const QString &mastersPath, bool preferCSV = true);

  /**
   * @brief Load BSE F&O contracts
   * @param preferCSV If true, try CSV first before master file
   */
  bool loadBSEFO(const QString &mastersPath, bool preferCSV = true);

  /**
   * @brief Load BSE CM contracts
   * @param preferCSV If true, try CSV first before master file
   */
  bool loadBSECM(const QString &mastersPath, bool preferCSV = true);
  
  /**
   * @brief Load NSE Index Master to populate Asset Tokens for Indices
   */
  bool loadIndexMaster(const QString &mastersPath);
  
  /**
   * @brief Get the index name to token map
   * @return Hash map of index names to their tokens
   */
  const QHash<QString, int64_t>& getIndexNameTokenMap() const;
  
  /**
   * @brief Resolve asset tokens for index derivatives (where field 14 = -1)
   * 
   * Index options and futures have UnderlyingInstrumentId = -1 in the master file.
   * This function resolves them to actual index tokens by looking up the index
   * master data loaded from nse_cm_index_master.csv.
   * 
   * Must be called AFTER both NSECM and NSEFO repositories are loaded.
   * 
   * Impact: Without this, Greeks calculation fails for ~15,000 index option contracts.
   */
  void resolveIndexAssetTokens();

  /**
   * @brief Load combined master file (all segments in one file from XTS)
   * @param filePath Path to combined master file
   * @return true if at least one segment loaded successfully
   */
  bool loadCombinedMasterFile(const QString &filePath);

  /**
   * @brief Load masters directly from in-memory CSV data (no file I/O)
   * @param csvData Raw CSV data containing master contracts
   * @return true if at least one segment loaded successfully
   *
   * This method is optimized for loading freshly downloaded data directly
   * from memory without writing to disk first. It parses the CSV data
   * and populates repositories directly, avoiding file I/O overhead.
   */
  bool loadFromMemory(const QString &csvData);

  /**
   * @brief Save processed CSVs for faster loading
   * @param mastersPath Path to Masters directory
   * @return true if at least one segment saved successfully
   */
  bool saveProcessedCSVs(const QString &mastersPath = "Masters");

  /**
   * @brief Initialize distributed price stores with contract master tokens
   *
   * Call this after loading contract masters to populate valid tokens
   * in the UDP reader thread-local price stores. This enables:
   * - NSE FO: Mark valid token slots in indexed array
   * - Others: Pre-populate valid token sets for validation
   */
  void initializeDistributedStores();

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
  QVector<ContractData> searchScrips(const QString &exchange,
                                     const QString &segment,
                                     const QString &series,
                                     const QString &searchText,
                                     int maxResults = 50) const;

  /**
   * @brief Get all scrips for a segment and series
   * @param exchange Exchange name ("NSE" or "BSE")
   * @param segment Segment type ("CM" or "FO")
   * @param series Series type ("EQUITY", "OPTSTK", "FUTIDX", etc.)
   * @return Vector of all matching contracts
   */
  QVector<ContractData> getScrips(const QString &exchange,
                                  const QString &segment,
                                  const QString &series) const;

  /**
   * @brief Get contract by token using exchange segment ID
   * @param exchangeSegmentID XTS segment ID (1=NSECM, 2=NSEFO, 11=BSECM,
   * 12=BSEFO)
   * @param token Exchange instrument ID
   * @return Pointer to contract (nullptr if not found)
   */
  const ContractData *getContractByToken(int exchangeSegmentID,
                                         int64_t token) const;

  /**
   * @brief Get contract by token
   * @param exchange Exchange name ("NSE" or "BSE")
   * @param segment Segment type ("CM" or "FO")
   * @param token Exchange instrument ID
   * @return Pointer to contract (nullptr if not found)
   */
  /**
   * @brief Get contract by token using combined segment key (e.g., "NSEFO",
   * "NSECM")
   * @param segmentKey Combined segment name
   * @param token Exchange instrument ID
   * @return Pointer to contract (nullptr if not found)
   */
  const ContractData *getContractByToken(const QString &segmentKey,
                                         int64_t token) const;

  const ContractData *getContractByToken(const QString &exchange,
                                         const QString &segment,
                                         int64_t token) const;

  /**
   * @brief Get all contracts for a specific underlying symbol (F&O)
   * @param exchange Exchange name ("NSE" or "BSE")
   * @param symbol Underlying symbol (e.g., "NIFTY", "BANKNIFTY")
   * @return Vector of all F&O contracts for the symbol
   */
  QVector<ContractData> getOptionChain(const QString &exchange,
                                       const QString &symbol) const;

  // ===== EXPIRY CACHE API (ATM Watch Optimization) =====

  /**
   * @brief Get all option-enabled symbols (OPTSTK InstrumentType=2)
   * @return Vector of symbols that have option contracts
   *
   * This is a fast O(1) cache lookup instead of filtering 100K+ contracts.
   * Cache is built during master load in buildExpiryCache().
   */
  QVector<QString> getOptionSymbols() const;

  /**
   * @brief Get all symbols for a specific expiry date
   * @param expiry Expiry date (e.g., "30JAN26")
   * @return Vector of symbols that have options expiring on this date
   */
  QVector<QString> getSymbolsForExpiry(const QString &expiry) const;

  /**
   * @brief Get current (nearest) expiry for a symbol
   * @param symbol Symbol name (e.g., "NIFTY")
   * @return Nearest expiry date or empty string if not found
   */
  QString getCurrentExpiry(const QString &symbol) const;

  /**
   * @brief Get all available expiry dates (sorted ascending)
   * @return Vector of all available expiry dates (sorted ascending)
   */
  QVector<QString> getAllExpiries() const;

  QVector<QString> getOptionSymbolsFromArray() const;
  QVector<QString> getUniqueExpiryOfStockOption() const;

  // New methods for ATM Watch
  QVector<QString> getAllUniqueSymbolOfStockOption() const;
  QVector<QString>
  getCurrentExpiryOfAllStockOption(const QVector<QString> &symbolList) const;
  QString getNearestExpiry(const QVector<QString> &expiryList) const;
  QVector<QString> getExpiriesForSymbol(const QString &symbol) const;

  // ATM Watch Optimization: O(1) lookups for strikes and tokens
  const QVector<double> &getStrikesForSymbolExpiry(const QString &symbol,
                                                   const QString &expiry) const;
  QPair<int64_t, int64_t> getTokensForStrike(const QString &symbol,
                                             const QString &expiry,
                                             double strike) const;
  int64_t getAssetTokenForSymbol(const QString &symbol) const;
  int64_t getFutureTokenForSymbolExpiry(const QString &symbol,
                                        const QString &expiry) const;
  
  /**
   * @brief Get underlying price (LTP) using unified logic (Cash -> Future fallback)
   * @param symbol Symbol (e.g., "NIFTY")
   * @param expiry Expiry Date (e.g., "30JAN2025")
   * @return double LTP (0.0 if not found)
   */
  double getUnderlyingPrice(const QString &symbol, const QString &expiry) const;
  
  QString getSymbolForFutureToken(int64_t token) const;
  
  // Debug helper
  void dumpFutureTokenMap(const QString& filepath) const;

  /**
   * @brief Get all contracts for a specific segment
   * @param exchange "NSE" or "BSE"
   * @param segment "CM" or "FO"
   * @return Vector of all contracts in that segment
   */
  QVector<ContractData> getContractsBySegment(const QString &exchange,
                                              const QString &segment) const;

  // ===== UPDATE METHODS =====

  /**
   * @brief Update live market data for a contract
   * @param exchange Exchange name ("NSE" or "BSE")
   * @param segment Segment type ("CM" or "FO")
   * @param token Exchange instrument ID
   * @param ltp Last traded price
   * @param volume Volume
   */
  void updateLiveData(const QString &exchange, const QString &segment,
                      int64_t token, double ltp, int64_t volume);

  /**
   * @brief Update bid/ask prices
   */
  void updateBidAsk(const QString &exchange, const QString &segment,
                    int64_t token, double bidPrice, double askPrice);

  /**
   * @brief Update Greeks for an option contract
   */
  void updateGreeks(int64_t token, double iv, double delta, double gamma,
                    double vega, double theta);

  /**
   * @brief Update asset tokens in NSEFO from index master mapping
   */
  void updateIndexAssetTokens();
  
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
  static QString getSegmentKey(const QString &exchange, const QString &segment);

  /**
   * @brief Get exchange segment ID for XTS API
   * @param exchange "NSE" or "BSE"
   * @param segment "CM" or "FO"
   * @return Segment ID (1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO)
   */
  static int getExchangeSegmentID(const QString &exchange,
                                  const QString &segment);

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
  
signals:
  void mastersLoaded();
  void loadingError(const QString& title, const QStringList& details);
  
  /**
   * @brief Emitted when all repositories have been loaded and initialized
   * 
   * This signal indicates that:
   * - Master files have been parsed
   * - Index master integrated
   * - Asset tokens resolved
   * - UDP mappings initialized
   * 
   * UDP readers should wait for this signal before starting to avoid
   * race conditions with index name resolution.
   */
  void repositoryLoaded();

private:
  RepositoryManager();
  ~RepositoryManager();
  
  // Thread safety for concurrent access (protects all public getters)
  mutable QReadWriteLock m_repositoryLock;

  /**
   * @brief Build caches for efficient symbol/expiry/strike lookups.
   *
   * Strategy:
   * 1. Iterates through all NSE FO contracts once during startup (in finalizeLoad()).
   * 2. Populates m_symbolExpiryStrikes: Map of (Symbol|Expiry) -> sorted unique strikes.
   * 3. Populates m_strikeToTokens: Map of (Symbol|Expiry|Strike) -> (CallToken, PutToken).
   * 4. Populates m_symbolExpiryFutureToken: Map of (Symbol|Expiry) -> Future Token.
   * 5. Populates m_symbolToAssetToken: Map of Symbol -> Cash Asset Token.
   *
   * This pre-computation moves the O(N) filtering cost to startup, enabling O(1) 
   * lookups for real-time ATM Watch calculations and UI rendering. This keeps 
   * the runtime loops extremamente lightweight (sub-microsecond lookups) even 
   * with 100K+ contracts.
   */
  void buildExpiryCache();

  // Singleton instance
  static RepositoryManager *s_instance;

  // Segment repositories
  std::unique_ptr<NSEFORepository> m_nsefo;
  std::unique_ptr<NSECMRepository> m_nsecm;
  std::unique_ptr<BSEFORepository> m_bsefo;
  std::unique_ptr<BSECMRepository> m_bsecm;

  bool m_loaded;

  // ===== EXPIRY CACHE (ATM Watch Optimization) =====
  // Pre-processed data for fast option symbol and expiry lookups
  // Built once during master load, used by ATM Watch for instant rendering

  QHash<QString, QVector<QString>>
      m_expiryToSymbols; // "30JAN26" -> ["NIFTY", "BANKNIFTY", ...]
  QHash<QString, QString> m_symbolToCurrentExpiry; // "NIFTY" -> "30JAN26"
  QSet<QString> m_optionSymbols;  // {"NIFTY", "BANKNIFTY", "RELIANCE", ...}
  // Cache for sorted expiries to avoid re-sorting on every access
  QVector<QString> m_sortedExpiries;
  
  // Temporary storage for index contracts before merging
  QVector<ContractData> m_indexContracts;

  // ATM Watch Optimization: Pre-built strike and token caches
  // Key: "SYMBOL|EXPIRY" -> sorted list of strikes
  QHash<QString, QVector<double>> m_symbolExpiryStrikes;
  // Key: "SYMBOL|EXPIRY|STRIKE" -> (CE token, PE token)
  QHash<QString, QPair<int64_t, int64_t>> m_strikeToTokens;
  // Symbol -> asset token (for cash price lookup)
  QHash<QString, int64_t> m_symbolToAssetToken;
  // Index Name -> Token (Loaded from nse_cm_index_master.csv)
  QHash<QString, int64_t> m_indexNameTokenMap;
  // Key: "SYMBOL|EXPIRY" -> future token (for future price lookup when cash not
  // available)
  QHash<QString, int64_t> m_symbolExpiryFutureToken;
  // Key: Future Token -> Symbol (Reverse mapping)
  QHash<int64_t, QString> m_futureTokenToSymbol;
  // Mutex for reader-writer locking of key caches
  mutable std::shared_mutex m_expiryCacheMutex;
};

#endif // REPOSITORY_MANAGER_H
