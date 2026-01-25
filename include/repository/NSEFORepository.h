#ifndef NSEFO_REPOSITORY_H
#define NSEFO_REPOSITORY_H

#include "ContractData.h"
#include <QHash>
#include <QReadWriteLock>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

/**
 * @brief NSE Futures & Options Repository with Cached Indexed Arrays
 *
 * Architecture (following Go implementation):
 * - Regular contracts (tokens 35,000 to 199,950): Direct indexed arrays
 * - Spread contracts (tokens > 10,000,000): Separate map storage
 * - Uses striped mutex (256 stripes) for optimal concurrent access
 *
 * Memory Efficiency:
 * - Array size: 164,951 slots
 * - Density: ~71.6% (85,845 contracts in 119,951 slots)
 * - Memory per contract: ~200 bytes
 * - Total memory: ~32 MB for arrays
 *
 * Performance:
 * - Regular contract lookup: O(1) with simple array access
 * - Spread contract lookup: O(1) with QHash
 * - Update operations: Protected by striped locks (256 parallel writers)
 */
class NSEFORepository {
public:
  // Token range constants (from Go implementation)
  static constexpr int64_t MIN_TOKEN = 35000;
  static constexpr int64_t MAX_TOKEN = 199950;
  static constexpr int32_t ARRAY_SIZE =
      MAX_TOKEN - MIN_TOKEN + 1; // 164,951 slots
  static constexpr int64_t SPREAD_THRESHOLD = 10000000;
  static constexpr int32_t NUM_STRIPES =
      256; // Number of mutexes for striped locking

  NSEFORepository();
  ~NSEFORepository();

  // ===== LOADING METHODS =====

  /**
   * @brief Load contracts from master file
   * @param filename Path to contract_nsefo_latest.txt
   * @return true if successful
   */
  bool loadMasterFile(const QString &filename);

  /**
   * @brief Load contracts from processed CSV file (preferred)
   * @param filename Path to nsefo_processed.csv
   * @return true if successful
   */
  virtual bool loadProcessedCSV(const QString &filename);

  /**
   * @brief Load contracts directly from parsed QVector (fastest)
   * @param contracts Pre-parsed contracts from combined master file
   * @return true if successful
   */
  virtual bool loadFromContracts(const QVector<MasterContract> &contracts);

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
  virtual void finalizeLoad();

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
   * @note Thread-safe for concurrent reads
   */
  const ContractData *getContract(int64_t token) const;

  /**
   * @brief Check if contract exists
   * @param token Exchange instrument ID
   * @return true if contract exists
   */
  bool hasContract(int64_t token) const;

  /**
   * @brief Iterate through all contracts without creating a copy
   * @param visitor Function to call for each contract
   */
  void forEachContract(std::function<void(const ContractData &)> visitor) const;

  /**
   * @brief Get all contracts (for iteration)
   * @return Vector of all valid contracts
   * @note Creates a copy, use sparingly
   */
  QVector<ContractData> getAllContracts() const;

  /**
   * @brief Get contracts by series
   * @param series Series type (OPTSTK, OPTIDX, FUTSTK, FUTIDX)
   * @return Vector of matching contracts
   */
  QVector<ContractData> getContractsBySeries(const QString &series) const;

  /**
   * @brief Get contracts by underlying symbol
   * @param symbol Underlying name (e.g., NIFTY, BANKNIFTY)
   * @return Vector of matching contracts
   */
  QVector<ContractData> getContractsBySymbol(const QString &symbol) const;

  /**
   * @brief Get contracts by underlying symbol and expiry
   * @param symbol Underlying name (e.g., NIFTY, BANKNIFTY)
   * @param expiry Expiry date string (e.g., 27JAN2026)
   * @param instrumentType Filter by instrument type (-1=all, 1=future, 2=option)
   * @return Vector of matching contracts
   */
  virtual QVector<ContractData> getContractsBySymbolAndExpiry(const QString &symbol,
                                                              const QString &expiry,
                                                              int instrumentType = -1) const;

  /**
   * @brief Get asset token by underlying symbol
   * @param symbol Underlying name (e.g., RELIANCE, NIFTY)
   * @return Asset token or -1 if not found
   */
  int64_t getAssetToken(const QString &symbol) const;

  /**
   * @brief Update asset token for a specific contract
   * @param token Exchange instrument ID
   * @param assetToken New underlying asset token
   */
  void updateAssetToken(int64_t token, int64_t assetToken);

  // ===== UPDATE METHODS =====


  // ===== METADATA =====

  /**
   * @brief Get total contract count
   */
  int32_t getTotalCount() const { return m_regularCount + m_spreadCount; }

  /**
   * @brief Get regular contract count
   */
  int32_t getRegularCount() const { return m_regularCount; }

  /**
   * @brief Get spread contract count
   */
  int32_t getSpreadCount() const { return m_spreadCount; }

  /**
   * @brief Check if repository is loaded
   */
  bool isLoaded() const { return m_loaded; }

private:
  // ===== HELPER METHODS =====

  /**
   * @brief Check if token is in regular contract range
   */
  inline bool isRegularContract(int64_t token) const {
    return token >= MIN_TOKEN && token <= MAX_TOKEN;
  }

  /**
   * @brief Convert token to array index
   */
  inline int32_t getArrayIndex(int64_t token) const {
    return static_cast<int32_t>(token - MIN_TOKEN);
  }

  /**
   * @brief Get stripe index for a given array index (for locking)
   */
  inline int32_t getStripeIndex(int32_t arrayIndex) const {
    return arrayIndex % NUM_STRIPES;
  }

  /**
   * @brief Allocate all arrays
   */
  void allocateArrays();

  // ===== DATA STORAGE =====

  // Validity bitmap (tracks which array slots are filled)
  QVector<bool> m_valid;

  // Security Master Data (6 fields)
  QVector<QString> m_name;
  QVector<QString> m_displayName;
  QVector<QString> m_description;
  QVector<QString> m_series;
  QVector<int32_t> m_lotSize;
  QVector<double> m_tickSize;

  // Trading Parameters (1 field)
  QVector<int32_t> m_freezeQty;

  // Price Bands (2 fields)
  QVector<double> m_priceBandHigh;
  QVector<double> m_priceBandLow;

  // Live Market Data (REMOVED - Use PriceStore)
  // Greeks (REMOVED - Use PriceStore)
  // Margin Data (REMOVED - Use PriceStore)

  // F&O Specific (4 fields)
  QVector<QString> m_expiryDate;
  QVector<double> m_strikePrice;
  QVector<QString> m_optionType;
  QVector<int64_t> m_assetToken;
  QVector<int32_t> m_instrumentType;

  // Spread contracts storage (tokens > 10,000,000)
  QHash<int64_t, std::shared_ptr<ContractData>> m_spreadContracts;

  // Symbol to Asset Token Map (for fast lookup of asset tokens by symbol)
  // Using QHash for Qt integration (implicit sharing) and performance
  QHash<QString, int64_t> m_symbolToAssetToken;

  // Striped mutexes for concurrent access (256 stripes)
  mutable QReadWriteLock m_mutexes[NUM_STRIPES];

  // Metadata
  int32_t m_regularCount;
  int32_t m_spreadCount;
  bool m_loaded;
};

#endif // NSEFO_REPOSITORY_H
