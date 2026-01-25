#ifndef NSECM_REPOSITORY_H
#define NSECM_REPOSITORY_H

#include "ContractData.h"
#include <QHash>
#include <QReadWriteLock>
#include <QString>
#include <QVector>
#include <functional>

/**
 * @brief NSE Cash Market Repository with Two-Level Indexing
 *
 * Architecture (following Go implementation):
 * Level 1: Token → Index mapping (QHash with ~2.5K entries)
 * Level 2: Parallel arrays (compact, no gaps)
 *
 * This approach is optimal for CM segments where:
 * - Token space is sparse (not suitable for direct indexing)
 * - Contract count is relatively small (~2,500 contracts)
 * - Memory efficiency is important
 *
 * Memory Efficiency:
 * - No wasted space (only allocated slots are filled)
 * - Hash map overhead: ~50KB for 2.5K entries
 * - Array storage: ~500KB for 2.5K contracts
 * - Total: ~550KB vs ~32MB for direct indexed approach
 *
 * Performance:
 * - Lookup: O(1) hash + O(1) array access
 * - Update: O(1) hash + O(1) array access
 * - Iteration: O(n) where n = actual contract count
 */
class NSECMRepository {
public:
  NSECMRepository();
  ~NSECMRepository();

  // ===== LOADING METHODS =====

  /**
   * @brief Load contracts from master file
   * @param filename Path to contract_nsecm_latest.txt
   * @return true if successful
   */
  bool loadMasterFile(const QString &filename);

  /**
   * @brief Load contracts from processed CSV file (preferred)
   * @param filename Path to nsecm_processed.csv
   * @return true if successful
   */
  bool loadProcessedCSV(const QString &filename);

  /**
   * @brief Load contracts directly from parsed QVector (fastest)
   * @param contracts Pre-parsed contracts from combined master file
   * @return true if successful
   */
  bool loadFromContracts(const QVector<MasterContract> &contracts);
  
  /**
   * @brief Append contracts (for index master integration)
   */
  void appendContracts(const QVector<ContractData>& contracts);

  /**
   * @brief Prepare repository for streaming load (clears data)
   */
  void prepareForLoad();

  /**
   * @brief Add a single contract during streaming load
   * @param contract Parsed master contract
   * @param internFunc String interning function
   */
  void addContract(const MasterContract &contract,
                   std::function<QString(const QString &)> internFunc);

  /**
   * @brief Finalize loading after streaming (marks repository as loaded)
   */
  void finalizeLoad();

  /**
   * @brief Save contracts to processed CSV file
   * @param filename Path to save CSV file
   * @return true if successful
   */
  bool saveProcessedCSV(const QString &filename) const;

  // ===== QUERY METHODS =====

  /**
   * @brief Get contract data by token
   * @param token Exchange instrument ID
   * @return ContractData pointer (nullptr if not found)
   */
  const ContractData *getContract(int64_t token) const;

  /**
   * @brief Check if contract exists
   * @param token Exchange instrument ID
   * @return true if contract exists
   */
  bool hasContract(int64_t token) const;

  /**
   * @brief Get all contracts (for iteration)
   * @return Vector of all contracts
   */
  QVector<ContractData> getAllContracts() const;

  /**
   * @brief Get contracts by series
   * @param series Series type (EQ, BE, etc.)
   * @return Vector of matching contracts
   */
  QVector<ContractData> getContractsBySeries(const QString &series) const;

  /**
   * @brief Get contracts by symbol name
   * @param symbol Stock symbol (e.g., RELIANCE, TCS)
   * @return Vector of matching contracts
   */
  QVector<ContractData> getContractsBySymbol(const QString &symbol) const;
  
  /**
   * @brief Iterate over all contracts (Zero-Copy)
   * @param callback Function called for each contract
   */
  void forEachContract(std::function<void(const ContractData &)> callback) const;

  // ===== UPDATE METHODS =====

  /**
   * @brief Update live market data for a contract
   * @param token Exchange instrument ID
   * @param ltp Last traded price
   * @param volume Volume
   */
  void updateLiveData(int64_t token, double ltp, int64_t volume);

  /**
   * @brief Update bid/ask prices (from WebSocket)
   * @param token Exchange instrument ID
   * @param bidPrice Best bid price
   * @param askPrice Best ask price
   */
  void updateBidAsk(int64_t token, double bidPrice, double askPrice);

  /**
   * @brief Update OHLC data
   * @param token Exchange instrument ID
   * @param open Open price
   * @param high High price
   * @param low Low price
   * @param close Close price
   */
  void updateOHLC(int64_t token, double open, double high, double low,
                  double close);

  // ===== METADATA =====

  /**
   * @brief Get total contract count
   */
  int32_t getTotalCount() const { return m_contractCount; }

  /**
   * @brief Check if repository is loaded
   */
  bool isLoaded() const { return m_loaded; }
  
  /**
   * @brief Get index name to token mapping
   */
  QHash<QString, int64_t> getIndexNameTokenMap() const;
  
  /**
   * @brief Build index name to token mapping after load
   */
  void buildIndexNameMap();

private:
  /**
   * @brief Allocate all parallel arrays
   */
  void allocateArrays(int32_t count);

  /**
   * @brief Get array index for a token (returns -1 if not found)
   */
  int32_t getIndex(int64_t token) const;

  // ===== DATA STORAGE =====

private:
  void addContractInternal(const MasterContract &contract,
                           std::function<QString(const QString &)> intern);

  // Level 1: Token → Index mapping
  QHash<int64_t, int32_t> m_tokenToIndex;

  // Level 2: Parallel arrays (compact, no gaps)
  QVector<int64_t>
      m_token; // Token for each contract (needed for reverse lookup)

  // Security Master Data (4 fields)
  QVector<QString> m_name;        // Symbol/stock name
  QVector<QString> m_displayName; // Display name
  QVector<QString> m_description; // Description
  QVector<QString> m_series;      // Series (EQ, BE, etc.)

  // Trading Parameters (3 fields)
  QVector<int32_t> m_lotSize;   // Lot size
  QVector<double> m_tickSize;   // Tick size
  QVector<int32_t> m_freezeQty; // Freeze quantity

  // Price Bands (2 fields)
  QVector<double> m_priceBandHigh; // Upper circuit limit
  QVector<double> m_priceBandLow;  // Lower circuit limit

  // Live Market Data (7 fields)
  QVector<double> m_ltp;       // Last traded price
  QVector<double> m_open;      // Open price
  QVector<double> m_high;      // High price
  QVector<double> m_low;       // Low price
  QVector<double> m_close;     // Close price
  QVector<double> m_prevClose; // Previous close
  QVector<int64_t> m_volume;   // Volume

  // WebSocket-only fields (not in CSV, populated by live updates)
  QVector<double> m_bidPrice; // Best bid price
  QVector<double> m_askPrice; // Best ask price

  // Metadata
  int32_t m_contractCount;
  bool m_loaded;
  mutable QReadWriteLock m_mutex;
  
  // Index name -> Token mapping for indices
  QHash<QString, int64_t> m_indexNameToToken;
};

#endif // NSECM_REPOSITORY_H
