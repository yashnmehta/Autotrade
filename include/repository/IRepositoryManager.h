#ifndef IREPOSITORY_MANAGER_H
#define IREPOSITORY_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPair>
#include <cstdint>

// Forward declaration — avoids pulling in the full ContractData header
struct ContractData;

/**
 * @brief Abstract interface for the repository manager subsystem
 *
 * Provides the contract that consumers of RepositoryManager depend on,
 * enabling mock implementations in unit tests without loading real
 * master contract files.
 *
 * Production implementation: RepositoryManager
 * Test implementation: MockRepositoryManager (see tests/)
 *
 * This interface exposes the most commonly used subset of RepositoryManager's
 * public API. Methods that are only used internally (e.g., loadAll, saveProcessedCSVs)
 * are intentionally omitted.
 *
 * Usage:
 * ```cpp
 * class GreeksService {
 * public:
 *     void setRepository(IRepositoryManager* repo) { m_repo = repo; }
 * private:
 *     IRepositoryManager* m_repo = nullptr;
 * };
 * ```
 */
class IRepositoryManager
{
public:
    virtual ~IRepositoryManager() = default;

    // ═══════════════════════════════════════════════════════
    // Search
    // ═══════════════════════════════════════════════════════

    /**
     * @brief Search for scrips by symbol prefix
     */
    virtual QVector<ContractData> searchScrips(
        const QString& exchange, const QString& segment,
        const QString& series, const QString& searchText,
        int maxResults = 50) const = 0;

    /**
     * @brief Global multi-token search across all segments
     */
    virtual QVector<ContractData> searchScripsGlobal(
        const QString& searchText,
        const QString& exchangeFilter = "",
        const QString& segmentFilter = "",
        const QString& expiryFilter = "",
        int maxResults = 50) const = 0;

    // ═══════════════════════════════════════════════════════
    // Contract Lookup
    // ═══════════════════════════════════════════════════════

    /**
     * @brief Get contract by (exchangeSegmentID, token)
     */
    virtual const ContractData* getContractByToken(
        int exchangeSegmentID, int64_t token) const = 0;

    /**
     * @brief Get contract by (segmentKey, token) — e.g. ("NSEFO", 49508)
     */
    virtual const ContractData* getContractByToken(
        const QString& segmentKey, int64_t token) const = 0;

    /**
     * @brief Get contract by (exchange, segment, token)
     */
    virtual const ContractData* getContractByToken(
        const QString& exchange, const QString& segment,
        int64_t token) const = 0;

    // ═══════════════════════════════════════════════════════
    // Symbol & Expiry Data
    // ═══════════════════════════════════════════════════════

    /**
     * @brief Get unique symbol names for a segment
     */
    virtual QStringList getUniqueSymbols(
        const QString& exchange, const QString& segment,
        const QString& series = QString()) const = 0;

    /**
     * @brief Get current (nearest) expiry for a symbol
     */
    virtual QString getCurrentExpiry(const QString& symbol) const = 0;

    /**
     * @brief Get all available expiry dates
     */
    virtual QVector<QString> getAllExpiries() const = 0;

    /**
     * @brief Get expiries for a specific symbol
     */
    virtual QVector<QString> getExpiriesForSymbol(const QString& symbol) const = 0;

    // ═══════════════════════════════════════════════════════
    // ATM Watch Support
    // ═══════════════════════════════════════════════════════

    /**
     * @brief Get strikes for a symbol+expiry pair
     */
    virtual const QVector<double>& getStrikesForSymbolExpiry(
        const QString& symbol, const QString& expiry) const = 0;

    /**
     * @brief Get CE/PE token pair for a strike
     */
    virtual QPair<int64_t, int64_t> getTokensForStrike(
        const QString& symbol, const QString& expiry,
        double strike) const = 0;

    /**
     * @brief Get asset token for a symbol
     */
    virtual int64_t getAssetTokenForSymbol(const QString& symbol) const = 0;

    /**
     * @brief Get underlying price (LTP) — Cash → Future fallback
     */
    virtual double getUnderlyingPrice(
        const QString& symbol, const QString& expiry) const = 0;

    // ═══════════════════════════════════════════════════════
    // Index Data
    // ═══════════════════════════════════════════════════════

    /**
     * @brief Get index name→token mapping
     */
    virtual const QHash<QString, qint64>& getIndexNameTokenMap() const = 0;

    /**
     * @brief Get token→index name mapping (reverse)
     */
    virtual const QHash<uint32_t, QString>& getIndexTokenNameMap() const = 0;
};

#endif // IREPOSITORY_MANAGER_H
