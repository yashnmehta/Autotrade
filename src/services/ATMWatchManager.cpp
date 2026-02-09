#include "services/ATMWatchManager.h"
#include "nsecm_price_store.h"
#include "nsefo_price_store.h"
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include "services/MasterDataState.h"
#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <QtConcurrent>
#include <atomic>

ATMWatchManager &ATMWatchManager::getInstance() {
  static ATMWatchManager instance;
  return instance;
}

ATMWatchManager::ATMWatchManager(QObject *parent) : QObject(parent) {
  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &ATMWatchManager::onMinuteTimer);
  m_timer->start(60000); // P2: 60 seconds - timer now acts as backup,
                         // event-driven is primary

  // Also trigger calculation as soon as masters are ready
  connect(MasterDataState::getInstance(), &MasterDataState::mastersReady, this,
          [this]() {
            QtConcurrent::run([this]() {
              calculateAll();
              subscribeToUnderlyingPrices(); // P2: Subscribe after first
                                             // calculation
            });
          });

  // If masters are already loaded, trigger immediate calculation
  /* if (MasterDataState::getInstance()->areMastersLoaded()) {
    qDebug() << "[ATMWatch] Masters already loaded, triggering immediate "
                "calculation";
    QtConcurrent::run([this]() { calculateAll(); });
  } */
}

void ATMWatchManager::addWatch(const QString &symbol, const QString &expiry,
                               BasePriceSource source) {
  std::unique_lock lock(m_mutex);
  ATMConfig config;
  config.symbol = symbol;
  config.expiry = expiry;
  config.source = source;
  config.rangeCount = m_defaultRangeCount; // P3: Use default range
  m_configs[symbol] = config;
}

void ATMWatchManager::setDefaultBasePriceSource(BasePriceSource source) {
  std::unique_lock lock(m_mutex);
  m_defaultSource = source;

  // Update all existing configs with new source
  for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
    it.value().source = source;
  }

  // Trigger recalculation to fetch new underlying tokens
  qDebug() << "[ATMWatch] Base price source set to"
           << (source == BasePriceSource::Cash ? "Cash" : "Future")
           << "- triggering recalculation";
  QtConcurrent::run([this]() { calculateAll(); });
}

void ATMWatchManager::setStrikeRangeCount(int count) {
  std::unique_lock lock(m_mutex);
  m_defaultRangeCount = count;

  // Update all existing configs with new range count
  for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
    it.value().rangeCount = count;
  }

  lock.unlock();

  // Trigger recalculation to fetch new strike ranges
  if (count > 0) {
    qDebug() << "[ATMWatch] Strike range set to ±" << count
             << "strikes - triggering recalculation";
    QtConcurrent::run([this]() { calculateAll(); });
  }
}

void ATMWatchManager::setThresholdMultiplier(double multiplier) {
  if (multiplier <= 0 || multiplier > 1.0) {
    qDebug() << "[ATMWatch] Invalid threshold multiplier:" << multiplier
             << "(must be 0 < m <= 1.0)";
    return;
  }

  std::unique_lock lock(m_mutex);
  m_thresholdMultiplier = multiplier;

  // Recalculate all thresholds with new multiplier
  for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
    const auto &config = it.value();
    double threshold = calculateThreshold(config.symbol, config.expiry);
    m_threshold[config.symbol] = threshold;
  }

  lock.unlock();
  qDebug() << "[ATMWatch] Threshold multiplier set to" << multiplier;
}

void ATMWatchManager::addWatchesBatch(
    const QVector<QPair<QString, QString>> &configs) {
  {
    std::unique_lock lock(m_mutex);
    for (const auto &pair : configs) {
      ATMConfig config;
      config.symbol = pair.first;
      config.expiry = pair.second;
      config.source = m_defaultSource;         // Use the default source
      config.rangeCount = m_defaultRangeCount; // P3: Use default range
      m_configs[pair.first] = config;
    }
  }

  /* qDebug() << "[ATMWatch] Added" << configs.size()
           << "watches in batch, triggering calculation..."; */

  // Trigger ONE calculation for all watches
  QtConcurrent::run([this]() { calculateAll(); });
}

void ATMWatchManager::removeWatch(const QString &symbol) {
  std::unique_lock lock(m_mutex);
  m_configs.remove(symbol);
  m_results.remove(symbol);
}

void ATMWatchManager::clearAllWatches() {
  std::unique_lock lock(m_mutex);
  m_configs.clear();
  m_results.clear();
  m_tokenToSymbol.clear();
  m_lastTriggerPrice.clear();
  m_threshold.clear();
  m_previousATMStrike.clear();
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
  // qDebug() << "[ATMWatch] Running periodic ATM calculation...";
  QtConcurrent::run([this]() { calculateAll(); });
}

void ATMWatchManager::calculateAll() {
  // P2: Prevent concurrent calculations to reduce CPU load
  static std::atomic<bool> isCalculating{false};
  if (isCalculating.exchange(true)) {
    return;
  }

  // Use RAII to ensure flag is reset even on early return
  struct Guard {
    std::atomic<bool> &flag;
    ~Guard() { flag.store(false); }
  } guard{isCalculating};

  std::unique_lock lock(m_mutex);

  auto repo = RepositoryManager::getInstance();
  if (!repo->isLoaded())
    return;

  int successCount = 0;
  int failCount = 0;
  QVector<ATMInfo> failedSymbols;
  QVector<QPair<QString, QPair<double, double>>>
      atmChanges; // P3: Track ATM changes {symbol, {old, new}}

  for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
    const auto &config = it.value();

    // Initialize or get existing info
    ATMInfo &info = m_results[config.symbol];
    info.symbol = config.symbol;
    info.expiry = config.expiry;
    info.isValid = false; // Reset to false until calculation succeeds
    info.status = ATMInfo::Status::Valid;
    info.errorMessage.clear();

    // Unified Step 1 & 2: Get Underlying Price (Cash -> Future Fallback)
    double basePrice = 0.0;
    int64_t underlyingToken = 0;

    if (config.source == BasePriceSource::Cash) {
      if (auto nsefoRepo = repo->getNSEFORepository()) {
        underlyingToken = nsefoRepo->getAssetToken(config.symbol);
      }
      // Fallback logic for basePrice is handled in fetchBasePrice-like logic
      // below
      basePrice = repo->getUnderlyingPrice(config.symbol, config.expiry);
    } else {
      // Future
      auto contracts = repo->getOptionChain("NSE", config.symbol);
      for (const auto &c : contracts) {
        if (c.instrumentType == 1 && c.expiryDate == config.expiry) {
          underlyingToken = c.exchangeInstrumentID;
          break;
        }
      }
      basePrice = repo->getUnderlyingPrice(config.symbol, config.expiry);
    }

    if (config.symbol == "RELIANCE") {
      qDebug() << "[ATMWatchManager] RELIANCE Calculated - Source:"
               << (config.source == BasePriceSource::Cash ? "Cash" : "Future")
               << "UnderlyingToken:" << underlyingToken
               << "BasePrice:" << basePrice;
    }

    // Step 3: Get sorted strikes from cache (O(1) lookup)
    const QVector<double> &strikeList =
        repo->getStrikesForSymbolExpiry(config.symbol, config.expiry);

    if (strikeList.isEmpty()) {
      info.status = ATMInfo::Status::StrikesNotFound;
      info.errorMessage = "No strikes found for " + config.expiry;
      failedSymbols.append(info);
      failCount++;
      continue;
    }

    // If still no valid base price, we can't calculate ATM strike
    if (basePrice <= 0) {
      info.status = ATMInfo::Status::PriceUnavailable;
      info.errorMessage = "Underlying price unavailable";
      failedSymbols.append(info);
      if (config.symbol == "NIFTY") {
        qDebug() << "[ATMWatch] ERROR: NIFTY base price is 0 for expiry:"
                 << config.expiry;
      }
      failCount++;
      continue;
    }

    // Step 4: Calculate ATM strike (pure math, no DB access)
    auto result = ATMCalculator::calculateFromActualStrikes(
        basePrice, strikeList, config.rangeCount);

    if (result.isValid) {
      // Step 5: Get CE/PE tokens for ATM strike (O(1) lookup)
      auto tokens = repo->getTokensForStrike(config.symbol, config.expiry,
                                             result.atmStrike);

      info.basePrice = basePrice;
      info.atmStrike = result.atmStrike;
      info.callToken = tokens.first;
      info.putToken = tokens.second;
      info.underlyingToken = underlyingToken;
      info.lastUpdated = QDateTime::currentDateTime();
      info.isValid = true;

      // P3: Store strike range and fetch tokens for all strikes
      info.strikes = result.strikes;
      info.strikeTokens.clear();
      for (const double strike : result.strikes) {
        auto strikeTokenPair =
            repo->getTokensForStrike(config.symbol, config.expiry, strike);
        info.strikeTokens.append(strikeTokenPair);
      }

      // P3: Detect ATM strike changes
      double previousStrike = m_previousATMStrike.value(config.symbol, 0.0);
      if (previousStrike > 0 && previousStrike != result.atmStrike) {
        atmChanges.append({config.symbol, {previousStrike, result.atmStrike}});
        qDebug() << "[ATMWatch]" << config.symbol
                 << "ATM changed:" << previousStrike << "->"
                 << result.atmStrike;
      }
      m_previousATMStrike[config.symbol] = result.atmStrike;

      successCount++;
    } else {
      failCount++;
    }
  }

  /* qDebug() << "[ATMWatch] Calculation complete:" << successCount <<
     "succeeded,"
           << failCount << "failed out of" << m_configs.size() << "symbols.
     Results size:" << m_results.size(); */

  emit atmUpdated();

  // Emit errors for failed symbols (outside lock to avoid deadlock)
  lock.unlock();
  for (const auto &failedInfo : failedSymbols) {
    emit calculationFailed(failedInfo.symbol, failedInfo.errorMessage);
  }

  // P3: Emit ATM change notifications
  for (const auto &change : atmChanges) {
    emit atmStrikeChanged(change.first, change.second.first,
                          change.second.second);
  }
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

    // Final fallback for known indices
    // Now handled by RepositoryManager mapping from Index Master
    return 0.0;
  } else {
    // Find Future LTP
    auto contracts =
        RepositoryManager::getInstance()->getOptionChain("NSE", config.symbol);
    for (const auto &c : contracts) {
      // instrumentType 1 = Future
      if (c.instrumentType == 1 && c.expiryDate == config.expiry) {
        auto state = nsefo::g_nseFoPriceStore.getUnifiedSnapshot(
            static_cast<uint32_t>(c.exchangeInstrumentID));
        return (state.token != 0) ? state.ltp : 0.0;
      }
    }

    // Fallback: If requested expiry future not found, try the nearest future?
    // For now, return 0 if exact expiry future is not found.
  }
  return 0.0;
}

// P2: Subscribe to underlying price updates for event-driven recalculation
void ATMWatchManager::subscribeToUnderlyingPrices() {
  std::unique_lock lock(m_mutex);
  auto repo = RepositoryManager::getInstance();
  auto nsefoRepo = repo->getNSEFORepository();
  if (!nsefoRepo)
    return;

  FeedHandler &feed = FeedHandler::instance();

  for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
    const auto &config = it.value();

    // Get underlying token based on source preference
    int64_t underlyingToken = 0;
    if (config.source == BasePriceSource::Cash) {
      underlyingToken = nsefoRepo->getAssetToken(config.symbol);
      if (underlyingToken > 0) {
        // Subscribe to NSECM (exchange segment 1)
        m_tokenToSymbol[underlyingToken] = config.symbol;
        feed.subscribe(1, underlyingToken, this,
                       &ATMWatchManager::onUnderlyingPriceUpdate);

        // Initialize threshold
        double threshold = calculateThreshold(config.symbol, config.expiry);
        m_threshold[config.symbol] = threshold;
        m_lastTriggerPrice[config.symbol] =
            nsecm::getGenericLtp(static_cast<uint32_t>(underlyingToken));

        qDebug() << "[ATMWatch] Subscribed to" << config.symbol
                 << "token:" << underlyingToken << "threshold:" << threshold;
      }
    } else {
      // Future - find the contract
      auto contracts = repo->getOptionChain("NSE", config.symbol);
      for (const auto &c : contracts) {
        if (c.instrumentType == 1 && c.expiryDate == config.expiry) {
          underlyingToken = c.exchangeInstrumentID;
          m_tokenToSymbol[underlyingToken] = config.symbol;
          // Subscribe to NSEFO (exchange segment 2)
          feed.subscribe(2, underlyingToken, this,
                         &ATMWatchManager::onUnderlyingPriceUpdate);

          double threshold = calculateThreshold(config.symbol, config.expiry);
          m_threshold[config.symbol] = threshold;
          auto state = nsefo::g_nseFoPriceStore.getUnifiedSnapshot(
              static_cast<uint32_t>(underlyingToken));
          m_lastTriggerPrice[config.symbol] = state.ltp;

          qDebug() << "[ATMWatch] Subscribed to" << config.symbol << "Future"
                   << "token:" << underlyingToken << "threshold:" << threshold;
          break;
        }
      }
    }
  }
}

// P2: Calculate threshold (half of strike interval)
double ATMWatchManager::calculateThreshold(const QString &symbol,
                                           const QString &expiry) {
  auto repo = RepositoryManager::getInstance();
  const QVector<double> &strikes =
      repo->getStrikesForSymbolExpiry(symbol, expiry);

  if (strikes.size() < 2) {
    return 50.0; // Default fallback
  }

  // Calculate strike interval from first two strikes
  double strikeInterval = strikes[1] - strikes[0];

  // P3: Threshold = multiplier * strike_interval (default 0.5 = half)
  return strikeInterval * m_thresholdMultiplier;
}

// P2: Event-driven price update handler
void ATMWatchManager::onUnderlyingPriceUpdate(const UDP::MarketTick &tick) {
  double newPrice = tick.ltp;

  // Find which symbol this token belongs to
  QString symbol;
  {
    std::shared_lock lock(m_mutex);
    auto it = m_tokenToSymbol.find(tick.token);
    if (it == m_tokenToSymbol.end())
      return;
    symbol = it.value();
  }

  // Check if price crossed threshold
  std::shared_lock lock(m_mutex);
  double lastPrice = m_lastTriggerPrice.value(symbol, 0.0);
  double threshold = m_threshold.value(symbol, 50.0);
  lock.unlock();

  if (lastPrice == 0.0) {
    // First price, just store it
    std::unique_lock wlock(m_mutex);
    m_lastTriggerPrice[symbol] = newPrice;
    return;
  }

  // Check if price moved beyond threshold
  double priceDelta = qAbs(newPrice - lastPrice);

  if (priceDelta >= threshold) {
    // Price crossed threshold - trigger recalculation
    qDebug() << "[ATMWatch]" << symbol << "price moved" << priceDelta
             << "(threshold:" << threshold << ") - triggering recalculation";

    // Update last trigger price
    {
      std::unique_lock wlock(m_mutex);
      m_lastTriggerPrice[symbol] = newPrice;
    }

    // Trigger async recalculation
    QtConcurrent::run([this]() { calculateAll(); });
  }
}
