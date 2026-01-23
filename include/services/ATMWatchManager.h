#ifndef ATM_WATCH_MANAGER_H
#define ATM_WATCH_MANAGER_H

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <shared_mutex>
#include "utils/ATMCalculator.h"
#include "repository/ContractData.h"

/**
 * @brief Manages ATM (At-The-Money) calculations and watch array.
 * Calculates ATM strikes every minute based on configurable base price source.
 */
class ATMWatchManager : public QObject {
    Q_OBJECT
public:
    enum class BasePriceSource {
        Cash,
        Future
    };

    struct ATMConfig {
        QString symbol;
        QString expiry;
        BasePriceSource source = BasePriceSource::Cash;
        int rangeCount = 0; // ±N strikes
        
        ATMConfig() = default;
    };

    struct ATMInfo {
        QString symbol;
        QString expiry;
        double basePrice = 0.0;
        double atmStrike = 0.0;
        int64_t callToken = 0;
        int64_t putToken = 0;
        QDateTime lastUpdated;
        bool isValid = false;
        
        ATMInfo() = default;
        ATMInfo(const ATMInfo&) = default;
        ATMInfo& operator=(const ATMInfo&) = default;
        ATMInfo(ATMInfo&&) = default;
        ATMInfo& operator=(ATMInfo&&) = default;
    };

    static ATMWatchManager* getInstance();

    /**
     * @brief Set default source for new watches
     */
    void setDefaultBasePriceSource(BasePriceSource source);
    BasePriceSource getDefaultBasePriceSource() const { return m_defaultSource; }

    /**
     * @brief Add or update an ATM watch for a symbol
     * Note: Does NOT trigger immediate calculation. Call calculateAll() explicitly.
     */
    void addWatch(const QString& symbol, const QString& expiry, BasePriceSource source = BasePriceSource::Cash);
    
    /**
     * @brief Add multiple ATM watches in batch and trigger single calculation
     * @param configs Vector of (symbol, expiry) pairs
     * This is more efficient than calling addWatch() multiple times
     */
    void addWatchesBatch(const QVector<QPair<QString, QString>>& configs);
    
    /**
     * @brief Remove an ATM watch
     */
    void removeWatch(const QString& symbol);

    /**
     * @brief Get current ATM info for all watched symbols
     */
    QVector<ATMInfo> getATMWatchArray() const;

    /**
     * @brief Get specific ATM info for a symbol
     */
    ATMInfo getATMInfo(const QString& symbol) const;

signals:
    /**
     * @brief Emitted when ATM calculation completes for all symbols
     */
    void atmUpdated();

public slots:
    // Force recalculation manually
    void calculateAll();

private slots:
    void onMinuteTimer();

    // calculateAll moved to public slots

private:
    ATMWatchManager(QObject* parent = nullptr);
    static ATMWatchManager* s_instance;

    double fetchBasePrice(const ATMConfig& config);
    
    BasePriceSource m_defaultSource = BasePriceSource::Cash;
    QTimer* m_timer;
    QHash<QString, ATMConfig> m_configs;
    QHash<QString, ATMInfo> m_results;
    mutable std::shared_mutex m_mutex;
};

#endif // ATM_WATCH_MANAGER_H
