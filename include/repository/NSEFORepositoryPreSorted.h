#ifndef NSEFOREPOSITORYPRESORTED_H
#define NSEFOREPOSITORYPRESORTED_H

#include "repository/NSEFORepository.h"
#include <QHash>
#include <QVector>

/**
 * @brief Pre-Sorted Multi-Index optimized version of NSEFORepository
 *
 * This class extends NSEFORepository with pre-sorted hash-based indexes
 * for O(log n) binary search and zero-cost sorting.
 *
 * Sort order: Expiry (by DATE) → InstrumentType → Strike → OptionType
 *
 * Benefits:
 * - O(1) hash lookup by symbol
 * - O(log n) binary search for expiry filtering
 * - Zero-cost sorting (already sorted by strike)
 * - Range extraction (copy only needed contracts)
 *
 * Performance: ~16x faster for chained filter+sort operations
 * Memory overhead: ~1-2 MB for indexes
 * Load time overhead: ~1.4s for date conversion and sorting
 *
 * Use case: UI filtering where users select symbol → expiry → view sorted
 * strikes
 */
class NSEFORepositoryPreSorted : public NSEFORepository {
public:
  NSEFORepositoryPreSorted();
  ~NSEFORepositoryPreSorted() = default;

  // Optimized method for chained filtering (symbol → expiry → sorted by strike)
  QVector<ContractData> getContractsBySymbolAndExpiry(
      const QString &symbol, const QString &expiry,
      int instrumentType = -1 // -1 = all, 1 = futures, 2 = options
  ) const override;

  // Override load methods to build indexes
  bool loadProcessedCSV(const QString &filename) override;
  bool loadFromContracts(const QVector<MasterContract> &contracts) override;
  void finalizeLoad() override;

  // Optimized filtering methods
  QVector<ContractData>
  getContractsBySeries(const QString &series) const override;
  QVector<ContractData>
  getContractsBySymbol(const QString &symbol) const override;

  /**
   * @brief Get unique symbol names (optimized O(1) access to index keys)
   * @param series Optional series filter ("OPTSTK", "FUTIDX", etc.)
   * @return List of unique symbol names
   *
   * This is significantly faster than calling getAllContracts() and filtering,
   * especially when no series filter is needed (direct key access).
   */
  QStringList getUniqueSymbols(const QString &series = QString()) const;

  /**
   * @brief Zero-copy iteration over all contracts
   * @param callback Function to call for each contract
   *
   * Delegates to base class NSEFORepository::forEachContract()
   * Provided for API consistency across all repository types.
   */
  void
  forEachContract(std::function<void(const ContractData &)> callback) const;

protected:
  // Multi-level indexes for O(1) lookups (store tokens, pre-sorted)
  QHash<QString, QVector<int64_t>>
      m_symbolIndex; // Symbol -> [tokens] (sorted by date/strike)
  QHash<QString, QVector<int64_t>>
      m_seriesIndex; // Series -> [tokens] (sorted by date/symbol/strike)
  QHash<QString, QVector<int64_t>>
      m_expiryIndex; // Expiry -> [tokens] (sorted by symbol/strike)

  // Helper to build all indexes after loading
  virtual void buildIndexes();

private:
  // Helper to sort token arrays by multi-level key (with date conversion)
  void sortIndexArrays();
};

#endif // NSEFOREPOSITORYPRESORTED_H
