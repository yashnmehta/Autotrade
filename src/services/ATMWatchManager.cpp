#include "services/ATMWatchManager.h"
#include "repository/RepositoryManager.h"
#include "nsecm_price_store.h"
#include "nsefo_price_store.h"
#include <QDebug>
#include <QDateTime>
#include <QSet>
#include <QtConcurrent>
#include "services/MasterDataState.h"

ATMWatchManager* ATMWatchManager::s_instance = nullptr;

ATMWatchManager* ATMWatchManager::getInstance() {
    if (!s_instance) {
        s_instance = new ATMWatchManager();
    }
    return s_instance;
}

ATMWatchManager::ATMWatchManager(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ATMWatchManager::onMinuteTimer);
    m_timer->start(60000); // 1 minute

    // Also trigger calculation as soon as masters are ready
    connect(MasterDataState::getInstance(), &MasterDataState::mastersReady, this, [this]() {
        QtConcurrent::run([this]() { calculateAll(); });
    });

    // If masters are already loaded, trigger immediate calculation
    if (MasterDataState::getInstance()->areMastersLoaded()) {
        qDebug() << "[ATMWatch] Masters already loaded, triggering immediate calculation";
        QtConcurrent::run([this]() { calculateAll(); });
    }
}

void ATMWatchManager::addWatch(const QString& symbol, const QString& expiry, BasePriceSource source) {
    std::unique_lock lock(m_mutex);
    ATMConfig config;
    config.symbol = symbol;
    config.expiry = expiry;
    config.source = source;
    m_configs[symbol] = config;
    
    // Trigger immediate calculation for the new watch
    lock.unlock();
    QtConcurrent::run([this]() { calculateAll(); });
}

void ATMWatchManager::removeWatch(const QString& symbol) {
    std::unique_lock lock(m_mutex);
    m_configs.remove(symbol);
    m_results.remove(symbol);
}

QVector<ATMWatchManager::ATMInfo> ATMWatchManager::getATMWatchArray() const {
    std::shared_lock lock(m_mutex);
    QVector<ATMInfo> result;
    result.reserve(m_results.size());
    for (const auto& info : m_results) {
        result.append(info);
    }
    return result;
}

ATMWatchManager::ATMInfo ATMWatchManager::getATMInfo(const QString& symbol) const {
    std::shared_lock lock(m_mutex);
    return m_results.value(symbol);
}

void ATMWatchManager::onMinuteTimer() {
    qDebug() << "[ATMWatch] Running periodic ATM calculation...";
    QtConcurrent::run([this]() { calculateAll(); });
}

void ATMWatchManager::calculateAll() {
    std::unique_lock lock(m_mutex);
    
    auto repo = RepositoryManager::getInstance();
    if (!repo->isLoaded()) return;

    for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
        const auto& config = it.value();
        double basePrice = fetchBasePrice(config);
        
        if (basePrice <= 0) {
            qDebug() << "[ATMWatch] Could not fetch base price for" << config.symbol;
            continue;
        }
        
        // Get all option contracts for this symbol
        QVector<ContractData> allContracts = repo->getOptionChain("NSE", config.symbol);
        if (allContracts.isEmpty()) {
            qDebug() << "[ATMWatch] No contracts found for" << config.symbol;
            continue;
        }

        QVector<double> strikeList;
        QHash<double, int64_t> callMap;
        QHash<double, int64_t> putMap;
        QSet<double> uniqueStrikes;

        for (const auto& c : allContracts) {
            // Filter by expiry and ensure it's an option (instrumentType 2)
            if (c.expiryDate != config.expiry || c.instrumentType != 2) {
                // Optional: debug print first few mismatch expiries to help user
                static int mismatchCount = 0;
                if (mismatchCount < 5 && c.instrumentType == 2) {
                    qDebug() << "[ATMWatch] Expiry mismatch for" << config.symbol << "Found:" << c.expiryDate << "Target:" << config.expiry;
                    mismatchCount++;
                }
                continue;
            }
            
            if (!uniqueStrikes.contains(c.strikePrice)) {
                uniqueStrikes.insert(c.strikePrice);
                strikeList.append(c.strikePrice);
            }

            if (c.optionType == "CE") {
                callMap[c.strikePrice] = c.exchangeInstrumentID;
            } else if (c.optionType == "PE") {
                putMap[c.strikePrice] = c.exchangeInstrumentID;
            }
        }

        if (strikeList.isEmpty()) {
            qDebug() << "[ATMWatch] No strikes found for" << config.symbol << config.expiry;
            continue;
        }

        // Option 1: Nearest actual strike price
        auto result = ATMCalculator::calculateFromActualStrikes(basePrice, strikeList, config.rangeCount);
        
        if (result.isValid) {
            ATMInfo info;
            info.symbol = config.symbol;
            info.expiry = config.expiry;
            info.basePrice = basePrice;
            info.atmStrike = result.atmStrike;
            info.callToken = callMap.value(result.atmStrike, 0);
            info.putToken = putMap.value(result.atmStrike, 0);
            info.lastUpdated = QDateTime::currentDateTime();
            info.isValid = true;
            
            m_results[config.symbol] = info;
            
            qDebug() << "[ATMWatch] Updated" << config.symbol << "ATM:" << info.atmStrike 
                     << "Base:" << basePrice << "CE:" << info.callToken << "PE:" << info.putToken;
        }
    }
    
    emit atmUpdated();
}

double ATMWatchManager::fetchBasePrice(const ATMConfig& config) {
    if (config.source == BasePriceSource::Cash) {
        // Need to find asset token. We can get it from any contract of this symbol
        auto contracts = RepositoryManager::getInstance()->getOptionChain("NSE", config.symbol);
        if (contracts.isEmpty()) return 0.0;
        
        // Most F&O contracts for NIFTY share assetToken 26000
        int64_t assetToken = contracts[0].assetToken;
        if (assetToken <= 0) {
            // Fallback for some master files where assetToken might be -1 or 0
            if (config.symbol == "NIFTY") assetToken = 26000;
            else if (config.symbol == "BANKNIFTY") assetToken = 26009;
            else if (config.symbol == "FINNIFTY") assetToken = 26037;
            else if (config.symbol == "MIDCPNIFTY") assetToken = 26074;
            else return 0.0;
        }
        
        return nsecm::getGenericLtp(static_cast<uint32_t>(assetToken));
    } else {
        // Find Future LTP
        auto contracts = RepositoryManager::getInstance()->getOptionChain("NSE", config.symbol);
        for (const auto& c : contracts) {
            // instrumentType 1 = Future
            if (c.instrumentType == 1 && c.expiryDate == config.expiry) {
                auto* state = nsefo::g_nseFoPriceStore.getUnifiedState(static_cast<uint32_t>(c.exchangeInstrumentID));
                return state ? state->ltp : 0.0;
            }
        }
        
        // Fallback: If requested expiry future not found, try the nearest future?
        // For now, return 0 if exact expiry future is not found.
    }
    return 0.0;
}
