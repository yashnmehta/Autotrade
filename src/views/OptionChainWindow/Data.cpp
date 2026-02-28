// ============================================================================
// Data.cpp — OptionChain data loading, tick processing, symbol/expiry
//            population, strike data updates, column mapping
// ============================================================================
#include "views/OptionChainWindow.h"
#include "data/PriceStoreGateway.h"
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include "services/GreeksCalculationService.h"
#include <QDate>
#include <QDebug>
#include <QScrollBar>
#include <QSet>
#include <QStandardItem>
#include <QTimer>
#include <algorithm>
#include <cmath>

// ============================================================================
// Column ID → Put-table model-column mapping
// ============================================================================
int OptionChainWindow::putColumnIndex(int colId)
{
    static const int map[] = {
        /* OI          0 */ 15,
        /* CHNG_IN_OI  1 */ 14,
        /* VOLUME      2 */ 13,
        /* IV          3 */  6,
        /* BID_IV      4 */  7,
        /* ASK_IV      5 */  8,
        /* DELTA       6 */  9,
        /* GAMMA       7 */ 10,
        /* VEGA        8 */ 11,
        /* THETA       9 */ 12,
        /* LTP        10 */  5,
        /* CHNG       11 */  4,
        /* BID_QTY    12 */  0,
        /* BID        13 */  1,
        /* ASK        14 */  2,
        /* ASK_QTY    15 */  3,
    };
    if (colId < 0 || colId >= 16) return -1;
    return map[colId];
}

void OptionChainWindow::captureColumnWidths()
{
    for (int colId = 0; colId < 16; ++colId) {
        int callIdx = colId + 1;
        if (callIdx < m_callTable->model()->columnCount()) {
            int w = m_callTable->columnWidth(callIdx);
            if (w > 0) m_columnProfile.setColumnWidth(colId, w);
        }
    }
}

void OptionChainWindow::updateStrikeData(double strike,
                                         const OptionStrikeData &data) {
  m_strikeData[strike] = data;

  int row = m_strikes.indexOf(strike);
  if (row < 0)
    return;

  auto updateItemWithColor = [](QStandardItem *item, double newValue,
                                int precision = 2) {
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
  };

  // Update call data
  m_callModel->item(row, CALL_OI)->setText(QString::number(data.callOI));
  m_callModel->item(row, CALL_CHNG_IN_OI)
      ->setText(QString::number(data.callChngInOI));
  m_callModel->item(row, CALL_VOLUME)
      ->setText(QString::number(data.callVolume));
  m_callModel->item(row, CALL_IV)
      ->setText(QString::number(data.callIV * 100.0, 'f', 2));
  m_callModel->item(row, CALL_BID_IV)
      ->setText(QString::number(data.callBidIV * 100.0, 'f', 2));
  m_callModel->item(row, CALL_ASK_IV)
      ->setText(QString::number(data.callAskIV * 100.0, 'f', 2));
  m_callModel->item(row, CALL_DELTA)
      ->setText(QString::number(data.callDelta, 'f', 2));
  m_callModel->item(row, CALL_GAMMA)
      ->setText(QString::number(data.callGamma, 'f', 4));
  m_callModel->item(row, CALL_VEGA)
      ->setText(QString::number(data.callVega, 'f', 2));
  m_callModel->item(row, CALL_THETA)
      ->setText(QString::number(data.callTheta, 'f', 2));

  updateItemWithColor(m_callModel->item(row, CALL_LTP), data.callLTP);

  m_callModel->item(row, CALL_CHNG)
      ->setText(QString::number(data.callChng, 'f', 2));
  m_callModel->item(row, CALL_BID_QTY)
      ->setText(QString::number(data.callBidQty));

  updateItemWithColor(m_callModel->item(row, CALL_BID), data.callBid);
  updateItemWithColor(m_callModel->item(row, CALL_ASK), data.callAsk);

  m_callModel->item(row, CALL_ASK_QTY)
      ->setText(QString::number(data.callAskQty));

  // Update put data
  m_putModel->item(row, PUT_BID_QTY)->setText(QString::number(data.putBidQty));

  updateItemWithColor(m_putModel->item(row, PUT_BID), data.putBid);
  updateItemWithColor(m_putModel->item(row, PUT_ASK), data.putAsk);

  m_putModel->item(row, PUT_ASK_QTY)->setText(QString::number(data.putAskQty));
  m_putModel->item(row, PUT_CHNG)
      ->setText(QString::number(data.putChng, 'f', 2));

  updateItemWithColor(m_putModel->item(row, PUT_LTP), data.putLTP);
  m_putModel->item(row, PUT_IV)->setText(QString::number(data.putIV * 100.0, 'f', 2));
  m_putModel->item(row, PUT_BID_IV)
      ->setText(QString::number(data.putBidIV * 100.0, 'f', 2));
  m_putModel->item(row, PUT_ASK_IV)
      ->setText(QString::number(data.putAskIV * 100.0, 'f', 2));
  m_putModel->item(row, PUT_DELTA)
      ->setText(QString::number(data.putDelta, 'f', 2));
  m_putModel->item(row, PUT_GAMMA)
      ->setText(QString::number(data.putGamma, 'f', 4));
  m_putModel->item(row, PUT_VEGA)
      ->setText(QString::number(data.putVega, 'f', 2));
  m_putModel->item(row, PUT_THETA)
      ->setText(QString::number(data.putTheta, 'f', 2));
  m_putModel->item(row, PUT_VOLUME)->setText(QString::number(data.putVolume));
  m_putModel->item(row, PUT_CHNG_IN_OI)
      ->setText(QString::number(data.putChngInOI));
  m_putModel->item(row, PUT_OI)->setText(QString::number(data.putOI));
}

void OptionChainWindow::clearData() {
  m_callModel->removeRows(0, m_callModel->rowCount());
  m_strikeModel->removeRows(0, m_strikeModel->rowCount());
  m_putModel->removeRows(0, m_putModel->rowCount());
  m_strikeData.clear();
  m_strikes.clear();
}

void OptionChainWindow::setStrikeRange(double minStrike, double maxStrike,
                                       double interval) {
  clearData();

  for (double strike = minStrike; strike <= maxStrike; strike += interval) {
    m_strikes.append(strike);
  }
}

void OptionChainWindow::setATMStrike(double atmStrike) {
  m_atmStrike = atmStrike;
  highlightATMStrike();
}

void OptionChainWindow::refreshData() {
  FeedHandler::instance().unsubscribeAll(this);
  clearData();
  m_tokenToStrike.clear();

  QString symbol = m_symbolCombo->currentText();
  QString expiry = m_expiryCombo->currentText();

  if (symbol.isEmpty())
    return;

  m_currentSymbol = symbol;
  m_currentExpiry = expiry;

  RepositoryManager *repo = RepositoryManager::getInstance();

  int exchangeSegment = 2; // NSEFO
  QVector<ContractData> contracts = repo->getOptionChain("NSE", symbol);
  if (contracts.isEmpty()) {
    contracts = repo->getOptionChain("BSE", symbol);
    exchangeSegment = 12; // BSEFO
  }

  m_exchangeSegment = exchangeSegment;

  QMap<double, ContractData> callContracts;
  QMap<double, ContractData> putContracts;
  QSet<double> strikes;

  for (const auto &contract : contracts) {
    if (!expiry.isEmpty() && contract.expiryDate != expiry)
      continue;

    strikes.insert(contract.strikePrice);
    if (contract.optionType == "CE") {
      callContracts[contract.strikePrice] = contract;
    } else if (contract.optionType == "PE") {
      putContracts[contract.strikePrice] = contract;
    }
  }

  if (strikes.isEmpty()) {
    return;
  }

  QList<double> sortedStrikes = strikes.values();
  std::sort(sortedStrikes.begin(), sortedStrikes.end());
  m_strikes = sortedStrikes;

  QList<QList<QStandardItem *>> callRows;
  QList<QList<QStandardItem *>> putRows;
  QList<QStandardItem *> strikeRows;

  FeedHandler &feed = FeedHandler::instance();

  for (double strike : sortedStrikes) {
    OptionStrikeData data;
    data.strikePrice = strike;

    if (callContracts.contains(strike)) {
      data.callToken = callContracts[strike].exchangeInstrumentID;

      feed.subscribe(exchangeSegment, data.callToken, this,
                     &OptionChainWindow::onTickUpdate);

      m_tokenToStrike[data.callToken] = strike;

      auto unifiedState =
          MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
              exchangeSegment, data.callToken);
      if (unifiedState.token != 0) {
        if (unifiedState.ltp > 0) {
          data.callLTP = unifiedState.ltp;
          if (unifiedState.close > 0)
            data.callChng = unifiedState.ltp - unifiedState.close;
        }
        if (unifiedState.bids[0].price > 0)
          data.callBid = unifiedState.bids[0].price;
        if (unifiedState.asks[0].price > 0)
          data.callAsk = unifiedState.asks[0].price;
        if (unifiedState.bids[0].quantity > 0)
          data.callBidQty = unifiedState.bids[0].quantity;
        if (unifiedState.asks[0].quantity > 0)
          data.callAskQty = unifiedState.asks[0].quantity;
        if (unifiedState.volume > 0)
          data.callVolume = unifiedState.volume;
        if (unifiedState.openInterest > 0)
          data.callOI = (int)unifiedState.openInterest;

        if (unifiedState.greeksCalculated) {
          data.callIV = unifiedState.impliedVolatility;
          data.callDelta = unifiedState.delta;
          data.callGamma = unifiedState.gamma;
          data.callVega = unifiedState.vega;
          data.callTheta = unifiedState.theta;
        }
      }
    }

    if (putContracts.contains(strike)) {
      data.putToken = putContracts[strike].exchangeInstrumentID;

      feed.subscribe(exchangeSegment, data.putToken, this,
                     &OptionChainWindow::onTickUpdate);

      m_tokenToStrike[data.putToken] = strike;

      auto unifiedState =
          MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
              exchangeSegment, data.putToken);
      if (unifiedState.token != 0) {
        if (unifiedState.ltp > 0) {
          data.putLTP = unifiedState.ltp;
          if (unifiedState.close > 0)
            data.putChng = unifiedState.ltp - unifiedState.close;
        }
        if (unifiedState.bids[0].price > 0)
          data.putBid = unifiedState.bids[0].price;
        if (unifiedState.asks[0].price > 0)
          data.putAsk = unifiedState.asks[0].price;
        if (unifiedState.bids[0].quantity > 0)
          data.putBidQty = (int)unifiedState.bids[0].quantity;
        if (unifiedState.asks[0].quantity > 0)
          data.putAskQty = (int)unifiedState.asks[0].quantity;
        if (unifiedState.volume > 0)
          data.putVolume = (int)unifiedState.volume;
        if (unifiedState.openInterest > 0)
          data.putOI = (int)unifiedState.openInterest;

        if (unifiedState.greeksCalculated) {
          data.putIV = unifiedState.impliedVolatility;
          data.putDelta = unifiedState.delta;
          data.putGamma = unifiedState.gamma;
          data.putVega = unifiedState.vega;
          data.putTheta = unifiedState.theta;
        }
      }
    }

    m_strikeData[strike] = data;

    // --- Create Visual Items ---
    auto createItem = [](double val, int precision = 2) {
      return new QStandardItem(val == 0 ? "0"
                                        : QString::number(val, 'f', precision));
    };
    auto createIntItem = [](int64_t val) {
      return new QStandardItem(val == 0 ? "0" : QString::number(val));
    };

    // Call Row
    QList<QStandardItem *> cRow;
    QStandardItem *cbCall = new QStandardItem("");
    cbCall->setCheckable(true);

    cRow << cbCall << createIntItem(data.callOI)
         << createIntItem(data.callChngInOI) << createIntItem(data.callVolume)
         << createItem(data.callIV * 100.0) << createItem(data.callBidIV * 100.0)
         << createItem(data.callAskIV * 100.0) << createItem(data.callDelta, 2)
         << createItem(data.callGamma, 4) << createItem(data.callVega, 2)
         << createItem(data.callTheta, 2) << createItem(data.callLTP)
         << createItem(data.callChng) << createIntItem(data.callBidQty)
         << createItem(data.callBid) << createItem(data.callAsk)
         << createIntItem(data.callAskQty);

    for (int i = 1; i < cRow.size(); ++i)
      cRow[i]->setTextAlignment(Qt::AlignCenter);
    callRows.append(cRow);

    // Strike Row
    QStandardItem *sItem = new QStandardItem(QString::number(strike, 'f', 2));
    sItem->setTextAlignment(Qt::AlignCenter);
    strikeRows.append(sItem);

    // Put Row
    QList<QStandardItem *> pRow;
    QStandardItem *cbPut = new QStandardItem("");
    cbPut->setCheckable(true);

    pRow << createIntItem(data.putBidQty) << createItem(data.putBid)
         << createItem(data.putAsk) << createIntItem(data.putAskQty)
         << createItem(data.putChng) << createItem(data.putLTP)
         << createItem(data.putIV * 100.0) << createItem(data.putBidIV * 100.0)
         << createItem(data.putAskIV * 100.0) << createItem(data.putDelta, 2)
         << createItem(data.putGamma, 4) << createItem(data.putVega, 2)
         << createItem(data.putTheta, 2) << createIntItem(data.putVolume)
         << createIntItem(data.putChngInOI) << createIntItem(data.putOI)
         << cbPut;

    for (int i = 0; i < pRow.size() - 1; ++i)
      pRow[i]->setTextAlignment(Qt::AlignCenter);
    putRows.append(pRow);
  }

  // Batch Insert to Views
  m_callTable->setUpdatesEnabled(false);
  m_strikeTable->setUpdatesEnabled(false);
  m_putTable->setUpdatesEnabled(false);

  for (const auto &row : callRows)
    m_callModel->appendRow(row);
  for (auto item : strikeRows)
    m_strikeModel->appendRow(item);
  for (const auto &row : putRows)
    m_putModel->appendRow(row);

  m_callTable->setUpdatesEnabled(true);
  m_strikeTable->setUpdatesEnabled(true);
  m_putTable->setUpdatesEnabled(true);

  // Auto-fit columns to content (one-time, user can still resize after)
  m_callTable->resizeColumnsToContents();
  m_putTable->resizeColumnsToContents();

  // Set ATM
  if (!m_strikes.isEmpty()) {
    m_atmStrike = m_strikes[m_strikes.size() / 2];
    highlightATMStrike();

    int row = m_strikes.indexOf(m_atmStrike);
    if (row >= 0) {
      QTimer::singleShot(0, this, [this, row]() {
        QModelIndex strikeIdx = m_strikeModel->index(row, 0);
        if (strikeIdx.isValid()) {
          m_strikeTable->scrollTo(strikeIdx,
                                  QAbstractItemView::PositionAtCenter);
          int val = m_strikeTable->verticalScrollBar()->value();
          m_callTable->verticalScrollBar()->setValue(val);
          m_putTable->verticalScrollBar()->setValue(val);
        }
      });
    }
  }

  updateTableColors();

  qDebug() << "[OptionChainWindow] Loaded" << m_strikeData.size()
           << "strikes for" << m_currentSymbol << m_currentExpiry;
  qDebug() << "[OptionChainWindow] Greeks will be calculated on tick updates";
}

void OptionChainWindow::onTickUpdate(const UDP::MarketTick &tick) {
  if (tick.updateType == UDP::UpdateType::DEPTH_UPDATE) {
    return;
  }

  if (!m_tokenToStrike.contains(tick.token))
    return;

  double strike = m_tokenToStrike[tick.token];
  OptionStrikeData &data = m_strikeData[strike];

  bool isCall = (tick.token == (uint32_t)data.callToken);

  if (isCall) {
    if (tick.ltp > 0) {
      data.callLTP = tick.ltp;
      if (tick.prevClose > 0) {
        data.callChng = tick.ltp - tick.prevClose;
      }
    }

    if (tick.bids[0].price > 0)
      data.callBid = tick.bids[0].price;
    if (tick.asks[0].price > 0)
      data.callAsk = tick.asks[0].price;
    if (tick.bids[0].quantity > 0)
      data.callBidQty = (int)tick.bids[0].quantity;
    if (tick.asks[0].quantity > 0)
      data.callAskQty = (int)tick.asks[0].quantity;

    if (tick.volume > 0)
      data.callVolume = (int)tick.volume;
    if (tick.openInterest > 0)
      data.callOI = (int)tick.openInterest;

  } else {
    if (tick.ltp > 0) {
      data.putLTP = tick.ltp;
      if (tick.prevClose > 0) {
        data.putChng = tick.ltp - tick.prevClose;
      }
    }

    if (tick.bids[0].price > 0)
      data.putBid = tick.bids[0].price;
    if (tick.asks[0].price > 0)
      data.putAsk = tick.asks[0].price;
    if (tick.bids[0].quantity > 0)
      data.putBidQty = (int)tick.bids[0].quantity;
    if (tick.asks[0].quantity > 0)
      data.putAskQty = (int)tick.asks[0].quantity;

    if (tick.volume > 0)
      data.putVolume = (int)tick.volume;
    if (tick.openInterest > 0)
      data.putOI = (int)tick.openInterest;
  }

  updateStrikeData(strike, data);

  GreeksCalculationService::instance().onPriceUpdate(tick.token, tick.ltp,
                                                     m_exchangeSegment);
}

void OptionChainWindow::populateSymbols() {
  const QSignalBlocker blocker(m_symbolCombo);
  m_symbolCombo->clear();

  RepositoryManager *repo = RepositoryManager::getInstance();
  QSet<QString> symbols;

  QVector<ContractData> indices = repo->getScrips("NSE", "FO", "FUTIDX");
  for (const auto &contract : indices) {
    symbols.insert(contract.name);
  }

  QVector<ContractData> stocks = repo->getScrips("NSE", "FO", "FUTSTK");
  for (const auto &contract : stocks) {
    symbols.insert(contract.name);
  }

  if (symbols.isEmpty()) {
    QVector<ContractData> bseIndices = repo->getScrips("BSE", "FO", "FUTIDX");
    for (const auto &contract : bseIndices)
      symbols.insert(contract.name);
  }

  QStringList sortedSymbols = symbols.values();
  std::sort(sortedSymbols.begin(), sortedSymbols.end());

  m_symbolCombo->addItems(sortedSymbols);
  m_symbolCombo->setEditable(true);
  m_symbolCombo->setInsertPolicy(QComboBox::NoInsert);

  int index = m_symbolCombo->findText("NIFTY");
  if (index >= 0) {
    m_symbolCombo->setCurrentIndex(index);
  } else if (!sortedSymbols.isEmpty()) {
    m_symbolCombo->setCurrentIndex(0);
  }

  if (m_symbolCombo->count() > 0) {
    QString currentSym = m_symbolCombo->currentText();
    m_currentSymbol = currentSym;
    m_titleLabel->setText(currentSym);
    populateExpiries(currentSym);
  }
}

void OptionChainWindow::populateExpiries(const QString &symbol) {
  const QSignalBlocker blocker(m_expiryCombo);
  m_expiryCombo->clear();
  if (symbol.isEmpty())
    return;

  RepositoryManager *repo = RepositoryManager::getInstance();

  QVector<ContractData> contracts = repo->getOptionChain("NSE", symbol);
  if (contracts.isEmpty()) {
    contracts = repo->getOptionChain("BSE", symbol);
  }

  QSet<QString> expirySet;
  for (const auto &contract : contracts) {
    if (!contract.expiryDate.isEmpty()) {
      expirySet.insert(contract.expiryDate);
    }
  }

  if (expirySet.isEmpty())
    return;

  QList<QDate> sortedDates;
  QMap<QDate, QString> dateToString;

  for (const QString &exp : expirySet) {
    QString cleanExp = exp;
    if (cleanExp.length() >= 7) {
      int yearIdx = cleanExp.length() - 4;
      if (yearIdx > 0) {
        QString dayPart = cleanExp.left(yearIdx - 3);
        QString monthPart = cleanExp.mid(yearIdx - 3, 3);
        QString yearPart = cleanExp.mid(yearIdx);

        if (monthPart.length() == 3) {
          monthPart = monthPart.at(0).toUpper() + monthPart.mid(1).toLower();
        }

        QString parseable = dayPart + monthPart + yearPart;
        QDate d = QDate::fromString(parseable, "dMMMyyyy");

        if (d.isValid()) {
          sortedDates.append(d);
          dateToString[d] = exp;
        } else {
          qDebug() << "Failed to parse expiry:" << exp
                   << "Cleaned:" << parseable;
        }
      }
    }
  }

  std::stable_sort(sortedDates.begin(), sortedDates.end(),
                   [](const QDate &a, const QDate &b) { return a < b; });

  for (const QDate &d : sortedDates) {
    m_expiryCombo->addItem(dateToString[d]);
  }

  if (m_expiryCombo->count() > 0) {
    m_expiryCombo->setCurrentIndex(0);
    m_currentExpiry = m_expiryCombo->currentText();
  }
}
