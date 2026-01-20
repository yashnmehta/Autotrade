#ifndef BSECM_REPOSITORY_H
#define BSECM_REPOSITORY_H

#include "ContractData.h"
#include <QHash>
#include <QReadWriteLock>
#include <QString>
#include <QVector>
#include <functional>

/**
 * @brief BSE Cash Market Repository with Two-Level Indexing
 *
 * Architecture (following NSE CM implementation):
 * Level 1: Token → Index mapping (QHash with ~13K entries)
 * Level 2: Parallel arrays (compact, no gaps)
 *
 * This approach is optimal for CM segments where:
 * - Token space is sparse (not suitable for direct indexing)
 * - Contract count is relatively large (~13,244 contracts)
 * - Memory efficiency is important
 *
 * Memory Efficiency:
 * - No wasted space (only allocated slots are filled)
 * - Hash map overhead: ~260KB for 13K entries
 * - Array storage: ~2.6MB for 13K contracts
 * - Total: ~2.9MB vs ~100MB+ for direct indexed approach
 */
class BSECMRepository {
public:
  BSECMRepository();
  ~BSECMRepository();

  // ===== LOADING METHODS =====

  /**
   * @brief Load contracts from master file
   * @param filename Path to contract_bsecm_latest.txt
   * @return true if successful
   */
  bool loadMasterFile(const QString &filename);

  /**
   * @brief Load contracts from processed CSV file (preferred)
   * @param filename Path to bsecm_processed.csv
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
   * @brief Save contracts to processed CSV file
   * @param filename Path to save CSV file
   * @return true if successful
   */
  bool saveProcessedCSV(const QString &filename) const;

  // ===== QUERY METHODS =====

  const ContractData *getContract(int64_t token) const;
  bool hasContract(int64_t token) const;
  QVector<ContractData> getAllContracts() const;
  QVector<ContractData> getContractsBySeries(const QString &series) const;
  QVector<ContractData> getContractsBySymbol(const QString &symbol) const;
  
  /**
   * @brief Iterate over all contracts (Zero-Copy)
   * @param callback Function called for each contract
   */
  void forEachContract(std::function<void(const ContractData &)> callback) const;

  // ===== UPDATE METHODS =====

  void updateLiveData(int64_t token, double ltp, double open, double high,
                      double low, double close, double prevClose,
                      int64_t volume);

  // ===== STATUS METHODS =====

  int32_t getTotalCount() const { return m_contractCount; }
  bool isLoaded() const { return m_loaded; }
  void finalizeLoad();
private:
  void addContractInternal(const MasterContract &contract,
                           std::function<QString(const QString &)> intern);


private:
  // Two-level indexing
  QHash<int64_t, int32_t> m_tokenToIndex; // Token → Array Index
  int32_t m_contractCount;

  // Static contract data (parallel arrays)
  QVector<int64_t> m_token;
  QVector<QString> m_name;
  QVector<QString> m_displayName;
  QVector<QString> m_description;
  QVector<QString> m_series;
  QVector<int32_t> m_lotSize;
  QVector<double> m_tickSize;
  QVector<double> m_priceBandHigh;
  QVector<double> m_priceBandLow;

  // Live market data (parallel arrays)
  QVector<double> m_ltp;
  QVector<double> m_open;
  QVector<double> m_high;
  QVector<double> m_low;
  QVector<double> m_close;
  QVector<double> m_prevClose;
  QVector<int64_t> m_volume;

  // Thread safety
  mutable QReadWriteLock m_mutex;
  bool m_loaded;
};

#endif // BSECM_REPOSITORY_H
