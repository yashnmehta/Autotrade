#include "services/ATMWatchManager.h"
#include "nsecm_price_store.h"
#include "nsefo_price_store.h"
#include "repository/RepositoryManager.h"
#include "services/MasterDataState.h"
#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <QtConcurrent>

ATMWatchManager *ATMWatchManager::s_instance = nullptr;

ATMWatchManager *ATMWatchManager::getInstance() {
  if (!s_instance) {
    s_instance = new ATMWatchManager();
  }
  return s_instance;
}

ATMWatchManager::ATMWatchManager(QObject *parent) : QObject(parent) {
  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &ATMWatchManager::onMinuteTimer);
  m_timer->start(60000); // 1 minute

  // Also trigger calculation as soon as masters are ready
  connect(MasterDataState::getInstance(), &MasterDataState::mastersReady, this,
          [this]() { QtConcurrent::run([this]() { calculateAll(); }); });

  // If masters are already loaded, trigger immediate calculation
  if (MasterDataState::getInstance()->areMastersLoaded()) {
    qDebug() << "[ATMWatch] Masters already loaded, triggering immediate "
                "calculation";
    QtConcurrent::run([this]() { calculateAll(); });
  }
}

void ATMWatchManager::addWatch(const QString &symbol, const QString &expiry,
                               BasePriceSource source) {
  std::unique_lock lock(m_mutex);
  ATMConfig config;
  config.symbol = symbol;
  config.expiry = expiry;
  config.source = source;
  m_configs[symbol] = config;
  // Note: Removed immediate calculateAll() trigger to avoid N × N complexity
  // Caller should call calculateAll() explicitly after adding all watches
}

void ATMWatchManager::addWatchesBatch(
    const QVector<QPair<QString, QString>> &configs) {
  {
    std::unique_lock lock(m_mutex);
    for (const auto &pair : configs) {
      ATMConfig config;
      config.symbol = pair.first;
      config.expiry = pair.second;
      config.source = BasePriceSource::Cash; // Default to cash
      m_configs[pair.first] = config;
    }
  }

  qDebug() << "[ATMWatch] Added" << configs.size()
           << "watches in batch, triggering calculation...";

  // Trigger ONE calculation for all watches
  QtConcurrent::run([this]() { calculateAll(); });
}

void ATMWatchManager::removeWatch(const QString &symbol) {
  std::unique_lock lock(m_mutex);
  m_configs.remove(symbol);
  m_results.remove(symbol);
}

QVector<ATMWatchManager::ATMInfo> ATMWatchManager::getATMWatchArray() const {
  std::shared_lock lock(m_mutex);
  QVector<ATMInfo> result;
  result.reserve(m_results.size());
  for (const auto &info : m_results) {
    result.append(info);
  }
  return result;
}

ATMWatchManager::ATMInfo
ATMWatchManager::getATMInfo(const QString &symbol) const {
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
  if (!repo->isLoaded())
    return;

  int successCount = 0;
  int failCount = 0;

  for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
    const auto &config = it.value();

    // Step 1: Get asset token for base price (O(1) lookup)
    int64_t assetToken = repo->getAssetTokenForSymbol(config.symbol);

    // Step 2: Fetch base price from live feed (try cash first, then future)
    double basePrice = 0.0;
    if (assetToken > 0) {
      basePrice = nsecm::getGenericLtp(static_cast<uint32_t>(assetToken));
    }

    // Step 2b: If cash LTP not available, try future LTP
    if (basePrice <= 0) {
      int64_t futureToken =
          repo->getFutureTokenForSymbolExpiry(config.symbol, config.expiry);
      if (futureToken > 0) {
        auto *state = nsefo::g_nseFoPriceStore.getUnifiedState(
            static_cast<uint32_t>(futureToken));
        if (state) {
          basePrice = state->ltp;
        }
      }
    }

    // Step 3: Get sorted strikes from cache (O(1) lookup)
    QVector<double> strikeList =
        repo->getStrikesForSymbolExpiry(config.symbol, config.expiry);

    if (strikeList.isEmpty()) {
      failCount++;
      continue;
    }

    // If still no valid base price, skip this symbol
    if (basePrice <= 0) {
      failCount++;
      continue;
    }

    // Step 4: Calculate ATM strike (pure math, no DB access)
    auto result = ATMCalculator::calculateFromActualStrikes(
        basePrice, strikeList, config.rangeCount);

    if (result.isValid) {
      // Step 5: Get CE/PE tokens from cache (O(1) lookup)
      auto tokens = repo->getTokensForStrike(config.symbol, config.expiry,
                                             result.atmStrike);

      ATMInfo info;
      info.symbol = config.symbol;
      info.expiry = config.expiry;
      info.basePrice = basePrice;
      info.atmStrike = result.atmStrike;
      info.callToken = tokens.first;
      info.putToken = tokens.second;
      info.lastUpdated = QDateTime::currentDateTime();
      info.isValid = true;

      m_results[config.symbol] = info;
      successCount++;
    } else {
      failCount++;
    }
  }

  qDebug() << "[ATMWatch] Calculation complete:" << successCount << "succeeded,"
           << failCount << "failed out of" << m_configs.size() << "symbols";

  emit atmUpdated();
}

double ATMWatchManager::fetchBasePrice(const ATMConfig &config) {
  if (config.source == BasePriceSource::Cash) {
    // Use asset token map from NSE FO Repository for fast lookup
    auto repo = RepositoryManager::getInstance();
    auto nsefoRepo = repo->getNSEFORepository();

    if (nsefoRepo) {
      // Fast O(1) lookup            // Get asset token from repository
      int64_t assetToken = nsefoRepo->getAssetToken(config.symbol);

      if (assetToken > 0) {
        double ltp = nsecm::getGenericLtp(static_cast<uint32_t>(assetToken));
        if (ltp > 0) {
          return ltp;
        }
      }
    }

    // Fallback: Try to get asset token from first contract (legacy behavior)
    auto contracts =
        RepositoryManager::getInstance()->getOptionChain("NSE", config.symbol);
    if (!contracts.isEmpty() && contracts[0].assetToken > 0) {
      return nsecm::getGenericLtp(
          static_cast<uint32_t>(contracts[0].assetToken));
    }

    // Final fallback for known indices (should not be needed if map is
    // populated)
    if (config.symbol == "NIFTY") {
      return nsecm::getGenericLtp(26000);
    } else if (config.symbol == "BANKNIFTY") {
      return nsecm::getGenericLtp(26009);
    } else if (config.symbol == "FINNIFTY") {
      return nsecm::getGenericLtp(26037);
    } else if (config.symbol == "MIDCPNIFTY") {
      return nsecm::getGenericLtp(26074);
    }

    return 0.0;
  } else {
    // Find Future LTP
    auto contracts =
        RepositoryManager::getInstance()->getOptionChain("NSE", config.symbol);
    for (const auto &c : contracts) {
      // instrumentType 1 = Future
      if (c.instrumentType == 1 && c.expiryDate == config.expiry) {
        auto *state = nsefo::g_nseFoPriceStore.getUnifiedState(
            static_cast<uint32_t>(c.exchangeInstrumentID));
        return state ? state->ltp : 0.0;
      }
    }

    // Fallback: If requested expiry future not found, try the nearest future?
    // For now, return 0 if exact expiry future is not found.
  }
  return 0.0;
}
