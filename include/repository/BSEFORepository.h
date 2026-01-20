#ifndef BSEFO_REPOSITORY_H
#define BSEFO_REPOSITORY_H

#include "ContractData.h"
#include <QHash>
#include <QReadWriteLock>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>

/**
 * @brief BSE Futures & Options Repository with Two-Level Indexing
 *
 * Architecture:
 * - BSE FO has sparse token space (~5,626 contracts)
 * - Uses hash-based indexing like NSE CM (not dense array like NSE FO)
 * - More memory efficient for smaller contract counts
 *
 * Memory Efficiency:
 * - Hash map overhead: ~110KB for 5.6K entries
 * - Array storage: ~1.1MB for 5.6K contracts
 * - Total: ~1.2MB vs ~32MB for direct indexed approach
 */
class BSEFORepository {
public:
  BSEFORepository();
  ~BSEFORepository();

  // ===== LOADING METHODS =====

  bool loadMasterFile(const QString &filename);
  bool loadProcessedCSV(const QString &filename);
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
  void updateGreeks(int64_t token, double iv, double delta, double gamma,
                    double vega, double theta);

  // ===== STATUS METHODS =====

  int32_t getTotalCount() const { return m_contractCount; }
  bool isLoaded() const { return m_loaded; }
  void finalizeLoad();

private:
  void addContractInternal(const MasterContract &contract,
                           std::function<QString(const QString &)> intern);
  // Two-level indexing
  QHash<int64_t, int32_t> m_tokenToIndex;
  int32_t m_contractCount;

  // Static contract data
  QVector<int64_t> m_token;
  QVector<QString> m_name;
  QVector<QString> m_displayName;
  QVector<QString> m_description;
  QVector<QString> m_series;
  QVector<int32_t> m_lotSize;
  QVector<double> m_tickSize;
  QVector<QString> m_expiryDate;
  QVector<double> m_strikePrice;
  QVector<QString> m_optionType;
  QVector<int64_t> m_assetToken;
  QVector<int32_t> m_instrumentType;
  QVector<double> m_priceBandHigh;
  QVector<double> m_priceBandLow;
  QVector<int32_t> m_freezeQty;

  // Live market data
  QVector<double> m_ltp;
  QVector<double> m_open;
  QVector<double> m_high;
  QVector<double> m_low;
  QVector<double> m_close;
  QVector<double> m_prevClose;
  QVector<int64_t> m_volume;
  QVector<double> m_bidPrice;
  QVector<double> m_askPrice;

  // Greeks
  QVector<double> m_iv;
  QVector<double> m_delta;
  QVector<double> m_gamma;
  QVector<double> m_vega;
  QVector<double> m_theta;

  // Thread safety
  mutable QReadWriteLock m_mutex;
  bool m_loaded;
};

#endif // BSEFO_REPOSITORY_H
