// ============================================================================
// Data.cpp — ATMWatch data loading, tick processing, sorting, refresh
// ============================================================================
#include "views/ATMWatchWindow.h"
#include "data/PriceStoreGateway.h"
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include <QDate>
#include <QDebug>
#include <QStandardItem>
#include <QtConcurrent>

void ATMWatchWindow::updateItemWithColor(QStandardItemModel *model, int row,
                                         int col, double newValue,
                                         int precision) {
  if (!model || row < 0 || col < 0)
    return;

  QStandardItem *item = model->item(row, col);
  if (!item) {
    item = new QStandardItem();
    model->setItem(row, col, item);
  }

  double oldValue = item->text().toDouble();
  item->setText(QString::number(newValue, 'f', precision));

  if (oldValue > 0 && newValue > oldValue) {
    item->setData(1, Qt::UserRole + 1);
  } else if (oldValue > 0 && newValue < oldValue) {
    item->setData(2, Qt::UserRole + 1);
  } else {
    if (newValue != oldValue)
      item->setData(0, Qt::UserRole + 1);
  }
}

void ATMWatchWindow::refreshData() {
  FeedHandler::instance().unsubscribeAll(this);
  m_tokenToInfo.clear();
  m_symbolToRow.clear();
  m_underlyingToRow.clear();
  m_underlyingTokenToSymbol.clear();
  m_previousATMData.clear();

  m_callModel->setRowCount(0);
  m_symbolModel->setRowCount(0);
  m_putModel->setRowCount(0);

  auto atmList = ATMWatchManager::getInstance().getATMWatchArray();
  sortATMList(atmList); // Apply sorting
  FeedHandler &feed = FeedHandler::instance();

  int row = 0;
  for (const auto &info : atmList) {
    if (!info.isValid)
      continue;

    m_callModel->insertRow(row);
    m_symbolModel->insertRow(row);
    m_putModel->insertRow(row);

    m_symbolToRow[info.symbol] = row;

    // Populate Symbol Table
    m_symbolModel->setData(m_symbolModel->index(row, SYM_NAME), info.symbol);
    m_symbolModel->setData(m_symbolModel->index(row, SYM_PRICE),
                           QString::number(info.basePrice, 'f', 2));
    m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                           QString::number(info.atmStrike, 'f', 2));
    m_symbolModel->setData(m_symbolModel->index(row, SYM_EXPIRY), info.expiry);

    // Initial Data for Options
    auto setupInitialOptionData = [&](int64_t token, bool isCall) {
      if (token <= 0)
        return;
      m_tokenToInfo[token] = {info.symbol, isCall};

      // Subscribe to live feed
      feed.subscribe(2, token, this, &ATMWatchWindow::onTickUpdate);

      // Fetch current price from cache using thread-safe snapshot
      auto state = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
          2, token);
      if (state.token != 0) {

        UDP::MarketTick tick;
        tick.token = static_cast<uint32_t>(token);
        tick.ltp = state.ltp;
        tick.bids[0].price = state.bids[0].price;
        tick.asks[0].price = state.asks[0].price;
        tick.volume = state.volume;
        tick.openInterest = state.openInterest;
        onTickUpdate(tick);

        if (state.greeksCalculated) {
          if (isCall) {
            m_callModel->setData(
                m_callModel->index(row, CALL_IV),
                QString::number(state.impliedVolatility * 100.0, 'f', 2));
            m_callModel->setData(m_callModel->index(row, CALL_DELTA),
                                 QString::number(state.delta, 'f', 2));
            m_callModel->setData(m_callModel->index(row, CALL_GAMMA),
                                 QString::number(state.gamma, 'f', 4));
            m_callModel->setData(m_callModel->index(row, CALL_VEGA),
                                 QString::number(state.vega, 'f', 2));
            m_callModel->setData(m_callModel->index(row, CALL_THETA),
                                 QString::number(state.theta, 'f', 2));
          } else {
            m_putModel->setData(
                m_putModel->index(row, PUT_IV),
                QString::number(state.impliedVolatility * 100.0, 'f', 2));
            m_putModel->setData(m_putModel->index(row, PUT_DELTA),
                                QString::number(state.delta, 'f', 2));
            m_putModel->setData(m_putModel->index(row, PUT_GAMMA),
                                QString::number(state.gamma, 'f', 4));
            m_putModel->setData(m_putModel->index(row, PUT_VEGA),
                                QString::number(state.vega, 'f', 2));
            m_putModel->setData(m_putModel->index(row, PUT_THETA),
                                QString::number(state.theta, 'f', 2));
          }
        }
      }
    };

    setupInitialOptionData(info.callToken, true);
    setupInitialOptionData(info.putToken, false);

    // Subscribe to Underlying Token for Real-Time "Spot/Fut" updates
    if (info.underlyingToken > 0) {
      m_underlyingTokenToSymbol[info.underlyingToken] = info.symbol;
      feed.subscribe(1, info.underlyingToken, this,
                     &ATMWatchWindow::onTickUpdate);
      feed.subscribe(2, info.underlyingToken, this,
                     &ATMWatchWindow::onTickUpdate);
    }

    row++;
  }

  // One-time auto-fit columns to content after first data load
  if (!m_initialColumnsResized && row > 0) {
    m_callTable->resizeColumnsToContents();
    m_symbolTable->resizeColumnsToContents();
    m_putTable->resizeColumnsToContents();
    m_initialColumnsResized = true;
  }
}

void ATMWatchWindow::updateDataIncrementally() {
  // P2: Incremental updates - only change what's different, no flicker
  auto atmList = ATMWatchManager::getInstance().getATMWatchArray();
  FeedHandler &feed = FeedHandler::instance();

  // Build map of new ATM info for quick lookup
  QMap<QString, ATMWatchManager::ATMInfo> newATMData;
  for (const auto &info : atmList) {
    if (info.isValid) {
      newATMData[info.symbol] = info;
    }
  }

  // Step 1: Update existing rows and handle token changes
  for (auto it = m_symbolToRow.begin(); it != m_symbolToRow.end(); ++it) {
    const QString &symbol = it.key();
    int row = it.value();

    if (!newATMData.contains(symbol)) {
      if (m_previousATMData.contains(symbol)) {
        const auto &oldInfo = m_previousATMData[symbol];
        if (oldInfo.callToken > 0) {
          feed.unsubscribe(2, oldInfo.callToken, this);
          m_tokenToInfo.remove(oldInfo.callToken);
        }
        if (oldInfo.putToken > 0) {
          feed.unsubscribe(2, oldInfo.putToken, this);
          m_tokenToInfo.remove(oldInfo.putToken);
        }
      }
      continue;
    }

    const auto &newInfo = newATMData[symbol];
    const auto &oldInfo = m_previousATMData.value(symbol);

    bool basePriceChanged = !m_previousATMData.contains(symbol) ||
                            oldInfo.basePrice != newInfo.basePrice;
    bool atmStrikeChanged = !m_previousATMData.contains(symbol) ||
                            oldInfo.atmStrike != newInfo.atmStrike;

    if (basePriceChanged) {
      updateItemWithColor(m_symbolModel, row, SYM_PRICE, newInfo.basePrice);
    }
    if (atmStrikeChanged) {
      m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                             QString::number(newInfo.atmStrike, 'f', 2));
    }

    // Handle token changes (ATM strike moved)
    if (atmStrikeChanged && m_previousATMData.contains(symbol)) {
      if (oldInfo.callToken > 0) {
        feed.unsubscribe(2, oldInfo.callToken, this);
        m_tokenToInfo.remove(oldInfo.callToken);
      }
      if (oldInfo.putToken > 0) {
        feed.unsubscribe(2, oldInfo.putToken, this);
        m_tokenToInfo.remove(oldInfo.putToken);
      }

      auto setupTokenSubscription = [&](int64_t token, bool isCall) {
        if (token <= 0)
          return;

        m_tokenToInfo[token] = {symbol, isCall};
        feed.subscribe(2, token, this, &ATMWatchWindow::onTickUpdate);

        auto state =
            MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(2,
                                                                         token);
        if (state.token != 0) {
          UDP::MarketTick tick;
          tick.token = static_cast<uint32_t>(token);
          tick.ltp = state.ltp;
          tick.bids[0].price = state.bids[0].price;
          tick.asks[0].price = state.asks[0].price;
          tick.volume = state.volume;
          tick.openInterest = state.openInterest;
          onTickUpdate(tick);

          if (state.greeksCalculated) {
            auto model = isCall ? m_callModel : m_putModel;
            int colIV = isCall ? CALL_IV : PUT_IV;
            int colDelta = isCall ? CALL_DELTA : PUT_DELTA;
            int colGamma = isCall ? CALL_GAMMA : PUT_GAMMA;
            int colVega = isCall ? CALL_VEGA : PUT_VEGA;
            int colTheta = isCall ? CALL_THETA : PUT_THETA;

            model->setData(model->index(row, colIV),
                           QString::number(state.impliedVolatility * 100.0, 'f', 2));
            model->setData(model->index(row, colDelta),
                           QString::number(state.delta, 'f', 2));
            model->setData(model->index(row, colGamma),
                           QString::number(state.gamma, 'f', 4));
            model->setData(model->index(row, colVega),
                           QString::number(state.vega, 'f', 2));
            model->setData(model->index(row, colTheta),
                           QString::number(state.theta, 'f', 2));
          }
        }
      };

      setupTokenSubscription(newInfo.callToken, true);
      setupTokenSubscription(newInfo.putToken, false);
    }

    // Handle Underlying Token changes
    bool underlyingChanged = !m_previousATMData.contains(symbol) ||
                             oldInfo.underlyingToken != newInfo.underlyingToken;

    if (underlyingChanged && m_previousATMData.contains(symbol)) {
      if (oldInfo.underlyingToken > 0) {
        m_underlyingTokenToSymbol.remove(oldInfo.underlyingToken);
        feed.unsubscribe(1, oldInfo.underlyingToken, this);
        feed.unsubscribe(2, oldInfo.underlyingToken, this);
      }
    }
  }

  // Step 2: Add new symbols (not in previous data)
  for (const auto &newInfo : atmList) {
    if (!newInfo.isValid)
      continue;

    if (!m_symbolToRow.contains(newInfo.symbol)) {
      int row = m_symbolModel->rowCount();

      m_callModel->insertRow(row);
      m_symbolModel->insertRow(row);
      m_putModel->insertRow(row);

      m_symbolToRow[newInfo.symbol] = row;

      m_symbolModel->setData(m_symbolModel->index(row, SYM_NAME),
                             newInfo.symbol);
      updateItemWithColor(m_symbolModel, row, SYM_PRICE, newInfo.basePrice);
      m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                             QString::number(newInfo.atmStrike, 'f', 2));
      m_symbolModel->setData(m_symbolModel->index(row, SYM_EXPIRY),
                             newInfo.expiry);

      auto setupNewTokenSubscription = [&](int64_t token, bool isCall) {
        if (token <= 0)
          return;
        m_tokenToInfo[token] = {newInfo.symbol, isCall};
        feed.subscribe(2, token, this, &ATMWatchWindow::onTickUpdate);

        auto state =
            MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(2,
                                                                         token);
        if (state.token != 0) {
          UDP::MarketTick tick;
          tick.token = static_cast<uint32_t>(token);
          tick.ltp = state.ltp;
          tick.bids[0].price = state.bids[0].price;
          tick.asks[0].price = state.asks[0].price;
          tick.volume = state.volume;
          tick.openInterest = state.openInterest;
          onTickUpdate(tick);

          if (state.greeksCalculated) {
            auto model = isCall ? m_callModel : m_putModel;
            int colIV = isCall ? CALL_IV : PUT_IV;
            int colDelta = isCall ? CALL_DELTA : PUT_DELTA;
            int colGamma = isCall ? CALL_GAMMA : PUT_GAMMA;
            int colVega = isCall ? CALL_VEGA : PUT_VEGA;
            int colTheta = isCall ? CALL_THETA : PUT_THETA;

            model->setData(model->index(row, colIV),
                           QString::number(state.impliedVolatility * 100.0, 'f', 2));
            model->setData(model->index(row, colDelta),
                           QString::number(state.delta, 'f', 2));
            model->setData(model->index(row, colGamma),
                           QString::number(state.gamma, 'f', 4));
            model->setData(model->index(row, colVega),
                           QString::number(state.vega, 'f', 2));
            model->setData(model->index(row, colTheta),
                           QString::number(state.theta, 'f', 2));
          }
        }
      };

      setupNewTokenSubscription(newInfo.callToken, true);
      setupNewTokenSubscription(newInfo.putToken, false);
    }
  }

  // Step 4: Robust Reconciliation of Underlying Tokens
  QHash<int64_t, QString> desiredMap;
  for (const auto &info : atmList) {
    if (info.isValid && info.underlyingToken > 0) {
      desiredMap[info.underlyingToken] = info.symbol;
    }
  }

  QVector<int64_t> tokensToRemove;
  for (auto it = m_underlyingTokenToSymbol.begin();
       it != m_underlyingTokenToSymbol.end(); ++it) {
    int64_t token = it.key();
    if (!desiredMap.contains(token)) {
      tokensToRemove.append(token);
    }
  }

  for (int64_t token : tokensToRemove) {
    m_underlyingTokenToSymbol.remove(token);
    feed.unsubscribe(1, token, this);
    feed.unsubscribe(2, token, this);
  }

  for (auto it = desiredMap.begin(); it != desiredMap.end(); ++it) {
    int64_t token = it.key();
    QString symbol = it.value();

    if (!m_underlyingTokenToSymbol.contains(token)) {
      m_underlyingTokenToSymbol[token] = symbol;
      feed.subscribe(1, token, this, &ATMWatchWindow::onTickUpdate);
      feed.subscribe(2, token, this, &ATMWatchWindow::onTickUpdate);

      auto state = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
          1, token);
      if (state.token == 0)
        state = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
            2, token);

      if (state.token != 0) {
        int targetRow = m_symbolToRow.value(symbol, -1);
        if (targetRow >= 0 && targetRow < m_symbolModel->rowCount()) {
          updateItemWithColor(m_symbolModel, targetRow, SYM_PRICE, state.ltp);
        }
      }
    }
  }

  // Final sync of internal state
  m_previousATMData = newATMData;
}

void ATMWatchWindow::onATMUpdated() {
  updateDataIncrementally();
}

void ATMWatchWindow::onTickUpdate(const UDP::MarketTick &tick) {
  // Skip depth-only updates
  if (tick.updateType == UDP::UpdateType::DEPTH_UPDATE) {
    return;
  }

  if (!m_tokenToInfo.contains(tick.token) &&
      !m_underlyingTokenToSymbol.contains(tick.token)) {
    return;
  }

  QString symbol;

  if (m_tokenToInfo.contains(tick.token)) {
    // Case 1: Option Token Update
    const auto &info = m_tokenToInfo[tick.token];
    symbol = info.first;
    int row = m_symbolToRow.value(symbol, -1);
    if (row < 0 || row >= m_symbolModel->rowCount())
      return;

    bool isCall = info.second;
    auto model = isCall ? m_callModel : m_putModel;
    int colPrice = isCall ? CALL_LTP : PUT_LTP;

    updateItemWithColor(model, row, colPrice, tick.ltp);

    if (isCall) {
      updateItemWithColor(m_callModel, row, CALL_BID, tick.bestBid());
      updateItemWithColor(m_callModel, row, CALL_ASK, tick.bestAsk());
      m_callModel->setData(m_callModel->index(row, CALL_VOL),
                           QString::number(tick.volume));
      m_callModel->setData(m_callModel->index(row, CALL_OI),
                           QString::number(tick.openInterest));
    } else {
      updateItemWithColor(m_putModel, row, PUT_BID, tick.bestBid());
      updateItemWithColor(m_putModel, row, PUT_ASK, tick.bestAsk());
      m_putModel->setData(m_putModel->index(row, PUT_VOL),
                          QString::number(tick.volume));
      m_putModel->setData(m_putModel->index(row, PUT_OI),
                          QString::number(tick.openInterest));
    }

  } else if (m_underlyingTokenToSymbol.contains(tick.token)) {
    // Case 2: Underlying Token Update ("Spot/Fut")
    symbol = m_underlyingTokenToSymbol[tick.token];
    int row = m_symbolToRow.value(symbol, -1);
    if (row < 0 || row >= m_symbolModel->rowCount())
      return;

    updateItemWithColor(m_symbolModel, row, SYM_PRICE, tick.ltp);
  }
}

void ATMWatchWindow::onBasePriceUpdate() {
  updateBasePrices();
}

void ATMWatchWindow::updateBasePrices() {
  auto atmList = ATMWatchManager::getInstance().getATMWatchArray();

  for (const auto &info : atmList) {
    if (!info.isValid)
      continue;

    int row = m_symbolToRow.value(info.symbol, -1);
    if (row >= 0 && row < m_symbolModel->rowCount()) {
      updateItemWithColor(m_symbolModel, row, SYM_PRICE, info.basePrice);

      double currentATM =
          m_symbolModel->data(m_symbolModel->index(row, SYM_ATM)).toDouble();
      if (qAbs(currentATM - info.atmStrike) > 0.01) {
        m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                               QString::number(info.atmStrike, 'f', 2));
      }
    }
  }
}

void ATMWatchWindow::loadAllSymbols() {
  auto repo = RepositoryManager::getInstance();
  if (!repo->isLoaded()) {
    m_statusLabel->setText("Repository not loaded");
    return;
  }

  m_statusLabel->setText("Loading symbols...");

  QtConcurrent::run([this, repo]() {
    QVector<QString> optionSymbols = repo->getOptionSymbols();

    qDebug() << "[ATMWatch] Loaded" << optionSymbols.size()
             << "option-enabled symbols from cache (instant lookup)";

    QMetaObject::invokeMethod(
        &ATMWatchManager::getInstance(),
        []() { ATMWatchManager::getInstance().clearAllWatches(); },
        Qt::QueuedConnection);

    QVector<QPair<QString, QString>> watchConfigs;

    for (const QString &symbol : optionSymbols) {
      QString expiry;

      if (m_currentExpiry == "CURRENT") {
        expiry = repo->getCurrentExpiry(symbol);
      } else {
        QVector<QString> symbolsForExpiry =
            repo->getSymbolsForExpiry(m_currentExpiry);
        if (symbolsForExpiry.contains(symbol)) {
          expiry = m_currentExpiry;
        }
      }

      if (!expiry.isEmpty()) {
        watchConfigs.append(qMakePair(symbol, expiry));
      }
    }

    qDebug() << "[ATMWatch] Prepared" << watchConfigs.size() << "watch configs";

    QMetaObject::invokeMethod(
        &ATMWatchManager::getInstance(),
        [watchConfigs]() {
          ATMWatchManager::getInstance().addWatchesBatch(watchConfigs);
        },
        Qt::QueuedConnection);

    QMetaObject::invokeMethod(this, "onSymbolsLoaded", Qt::QueuedConnection,
                              Q_ARG(int, watchConfigs.size()));
  });
}

void ATMWatchWindow::populateCommonExpiries(const QString &exchange) {
  m_expiryCombo->clear();
  m_expiryCombo->addItem("Current (Nearest)", "CURRENT");

  auto repo = RepositoryManager::getInstance();
  if (!repo->isLoaded()) {
    return;
  }

  QVector<QString> expiries = repo->getAllExpiries();

  for (const QString &expiry : expiries) {
    m_expiryCombo->addItem(expiry, expiry);
  }
}

// OLD CODE BELOW - REPLACED WITH CACHE LOOKUP
#if 0
void ATMWatchWindow::populateCommonExpiries_OLD(const QString &exchange) {
  m_expiryCombo->clear();
  m_expiryCombo->addItem("Current (Nearest)", "CURRENT");

  auto repo = RepositoryManager::getInstance();
  if (!repo->isLoaded()) {
    return;
  }

  QVector<ContractData> allContracts =
      repo->getContractsBySegment(exchange, "FO");

  QSet<QString> expiries;
  for (const auto &contract : allContracts) {
    if (contract.instrumentType == 2 && !contract.expiryDate.isEmpty()) {
      expiries.insert(contract.expiryDate);
    }
  }

  QStringList expiryList = expiries.values();
  std::sort(expiryList.begin(), expiryList.end());

  for (const QString &expiry : expiryList) {
    m_expiryCombo->addItem(expiry, expiry);
  }

  qDebug() << "[ATMWatch] Populated" << expiryList.size()
           << "common expiries for" << exchange;

  m_expiryCombo->setCurrentIndex(0);
  m_currentExpiry = "CURRENT";
}
#endif

QString ATMWatchWindow::getNearestExpiry(const QString &symbol,
                                         const QString &exchange) {
  auto repo = RepositoryManager::getInstance();
  if (!repo->isLoaded()) {
    return QString();
  }

  return repo->getCurrentExpiry(symbol);
}

void ATMWatchWindow::sortATMList(QVector<ATMWatchManager::ATMInfo> &list) {
  if (list.isEmpty())
    return;

  // For call/put table sorting, pre-fetch the sort values from price store
  QHash<QString, double> sortValues;
  if (m_sortSource == SORT_CALL_TABLE || m_sortSource == SORT_PUT_TABLE) {
    for (const auto &info : list) {
      if (!info.isValid) continue;
      int64_t token = (m_sortSource == SORT_CALL_TABLE) ? info.callToken : info.putToken;
      if (token <= 0) {
        sortValues[info.symbol] = 0.0;
        continue;
      }
      auto state = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(2, token);
      double val = 0.0;
      if (state.token != 0) {
        int col = m_sortColumn;
        if (m_sortSource == SORT_CALL_TABLE) {
          if (col == CALL_IV) val = state.impliedVolatility;
          else if (col == CALL_DELTA) val = state.delta;
          else if (col == CALL_GAMMA) val = state.gamma;
          else if (col == CALL_VEGA) val = state.vega;
          else if (col == CALL_THETA) val = state.theta;
          else if (col == CALL_LTP) val = state.ltp;
          else if (col == CALL_VOL) val = state.volume;
          else if (col == CALL_OI) val = state.openInterest;
        } else {
          if (col == PUT_IV) val = state.impliedVolatility;
          else if (col == PUT_DELTA) val = state.delta;
          else if (col == PUT_GAMMA) val = state.gamma;
          else if (col == PUT_VEGA) val = state.vega;
          else if (col == PUT_THETA) val = state.theta;
          else if (col == PUT_LTP) val = state.ltp;
          else if (col == PUT_VOL) val = state.volume;
          else if (col == PUT_OI) val = state.openInterest;
        }
      }
      sortValues[info.symbol] = val;
    }
  }

  std::sort(list.begin(), list.end(),
            [this, &sortValues](const ATMWatchManager::ATMInfo &a,
                   const ATMWatchManager::ATMInfo &b) {
              bool less = false;

              if (m_sortSource == SORT_CALL_TABLE || m_sortSource == SORT_PUT_TABLE) {
                less = sortValues.value(a.symbol, 0.0) < sortValues.value(b.symbol, 0.0);
              } else {
                switch (m_sortColumn) {
                case SYM_NAME:
                  less = a.symbol < b.symbol;
                  break;
                case SYM_PRICE:
                  less = a.basePrice < b.basePrice;
                  break;
                case SYM_ATM:
                  less = a.atmStrike < b.atmStrike;
                  break;
                case SYM_EXPIRY:
                  less = a.expiry < b.expiry;
                  break;
                default:
                  less = a.symbol < b.symbol;
                  break;
                }
              }
              return (m_sortOrder == Qt::AscendingOrder) ? less : !less;
            });
}
