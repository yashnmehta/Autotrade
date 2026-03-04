#include "data/PriceStoreGateway.h"
#include "data/UnifiedPriceState.h"
#include "models/domain/TokenAddressBook.h"
#include "quant/ATMCalculator.h"
#include "repository/RepositoryManager.h"
#include "services/TokenSubscriptionManager.h"
#include "utils/ClipboardHelpers.h"
#include "utils/SelectionHelpers.h"
#include "views/MarketWatchWindow.h"
#include "views/helpers/MarketWatchHelpers.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSet>
#include <algorithm>

// ============================================================================
// Helper: Convert exchange string to UDP segment (eliminates 4x duplication)
// ============================================================================
static UDP::ExchangeSegment exchangeToSegment(const QString &exchange) {
  if (exchange == "NSEFO")
    return UDP::ExchangeSegment::NSEFO;
  if (exchange == "NSECM")
    return UDP::ExchangeSegment::NSECM;
  if (exchange == "BSEFO")
    return UDP::ExchangeSegment::BSEFO;
  if (exchange == "BSECM")
    return UDP::ExchangeSegment::BSECM;
  return UDP::ExchangeSegment::NSECM; // default
}

bool MarketWatchWindow::addScrip(const QString &symbol, const QString &exchange,
                                 int token) {
  if (token <= 0)
    return false;
  if (hasDuplicate(token))
    return false;

  ScripData scrip;

  qDebug() << "[MarketWatch] addScrip requested - Symbol:" << symbol
           << "Exchange:" << exchange << "Token:" << token;

  // Try to get full contract details from repository to populate missing fields
  const ContractData *contract =
      RepositoryManager::getInstance()->getContractByToken(exchange, token);
  if (contract) {
    qDebug() << "[MarketWatch] contract found in repository:"
             << contract->displayName << "Series:" << contract->series
             << "Strike:" << contract->strikePrice;

    // Prefer 'name' (Symbol) over 'displayName' (Description) for the symbol
    // field
    scrip.symbol = contract->name;
    if (scrip.symbol.isEmpty())
      scrip.symbol = contract->displayName;

    scrip.exchange = exchange;
    scrip.token = token;
    scrip.instrumentType = contract->series;
    scrip.strikePrice = contract->strikePrice;
    scrip.optionType = contract->optionType;
    scrip.seriesExpiry = contract->expiryDate;
    scrip.close =
        contract->prevClose; // Essential for change calculation if not in tick
    scrip.code = token;
  } else {
    qDebug() << "[MarketWatch] ⚠ contract NOT found in repository for"
             << exchange << token;
    scrip.symbol = symbol;
    scrip.exchange = exchange;
    scrip.token = token;
    scrip.code = token;
  }

  scrip.isBlankRow = false;

  // Determine insertion position: above current selection, or at end if none
  int insertPos = m_model->rowCount(); // default: append at end
  QModelIndexList selection = selectionModel()->selectedRows();
  if (!selection.isEmpty()) {
    // Insert above the first selected row
    int sourceRow = mapToSource(selection.first().row());
    if (sourceRow >= 0) {
      insertPos = sourceRow;
    }
  }

  m_model->insertScrip(insertPos, scrip);
  int newRow = insertPos;

  // Select the newly inserted row so the user sees it immediately
  int proxyRow = mapToProxy(newRow);
  if (proxyRow >= 0) {
    QModelIndex proxyIndex = proxyModel()->index(proxyRow, 0);
    clearSelection();
    setCurrentIndex(proxyIndex);
    selectRow(proxyRow);
    scrollTo(proxyIndex, QAbstractItemView::EnsureVisible);

    // Update stored focus to the new scrip so focusInEvent won't fight it
    m_lastFocusedToken = token;
    m_lastFocusedSymbol = scrip.symbol;
    m_suppressFocusRestore = true;
  }

  TokenSubscriptionManager::instance()->subscribe(exchange, token);

  // Subscribe to UDP ticks
  UDP::ExchangeSegment segment = exchangeToSegment(exchange);
  FeedHandler::instance().subscribeUDP(segment, token, this,
                                       &MarketWatchWindow::onUdpTickUpdate);

  // Initial Load from Distributed Store (thread-safe snapshot)
  if (m_useZeroCopyPriceCache) {
    auto data = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
        static_cast<int>(segment), token);
    if (data.token != 0 && data.ltp > 0) {
      // Update via onUdpTickUpdate for consistency
      UDP::MarketTick udpTick;
      udpTick.exchangeSegment = segment;
      udpTick.token = token;
      udpTick.ltp = data.ltp;
      udpTick.open = data.open;
      udpTick.high = data.high;
      udpTick.low = data.low;
      udpTick.prevClose = data.close;
      udpTick.volume = data.volume;
      udpTick.atp = data.avgPrice;

      // Copy best depth
      udpTick.bids[0].price = data.bids[0].price;
      udpTick.bids[0].quantity = data.bids[0].quantity;
      udpTick.asks[0].price = data.asks[0].price;
      udpTick.asks[0].quantity = data.asks[0].quantity;

      onUdpTickUpdate(udpTick);
    }
  }

  m_tokenAddressBook->addCompositeToken(exchange, "", token, newRow);
  m_tokenAddressBook->addIntKeyToken(static_cast<int>(segment), token, newRow);

  emit scripAdded(scrip.symbol, exchange, token);
  return true;
}

void MarketWatchWindow::addScrips(const QList<ScripData> &scrips) {
  if (scrips.isEmpty())
    return;

  QList<ScripData> validScrips;
  for (const auto &scripData : scrips) {
    ScripData scrip = scripData;
    if (!scrip.isBlankRow) {
      if (scrip.token <= 0 || hasDuplicate(scrip.token))
        continue;

      const ContractData *contract =
          RepositoryManager::getInstance()->getContractByToken(scrip.exchange,
                                                               scrip.token);
      if (contract) {
        scrip.symbol = contract->name;
        if (scrip.symbol.isEmpty())
          scrip.symbol = contract->displayName;
        scrip.instrumentType = contract->series;
        scrip.strikePrice = contract->strikePrice;
        scrip.optionType = contract->optionType;
        scrip.seriesExpiry = contract->expiryDate;
        scrip.close = contract->prevClose;
        scrip.code = scrip.token;
      } else {
        scrip.code = scrip.token;
      }
    }
    validScrips.append(scrip);
  }

  if (validScrips.isEmpty())
    return;

  int newRowStart = m_model->rowCount();
  m_model->addScrips(validScrips);

  for (int i = 0; i < validScrips.count(); ++i) {
    const auto &scrip = validScrips[i];
    if (scrip.isBlankRow)
      continue;

    int row = newRowStart + i;
    TokenSubscriptionManager::instance()->subscribe(scrip.exchange,
                                                    scrip.token);

    UDP::ExchangeSegment segment = exchangeToSegment(scrip.exchange);
    FeedHandler::instance().subscribeUDP(segment, scrip.token, this,
                                         &MarketWatchWindow::onUdpTickUpdate);

    if (m_useZeroCopyPriceCache) {
      auto data = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
          static_cast<int>(segment), scrip.token);
      if (data.token != 0 && data.ltp > 0) {
        UDP::MarketTick udpTick;
        udpTick.exchangeSegment = segment;
        udpTick.token = scrip.token;
        udpTick.ltp = data.ltp;
        udpTick.open = data.open;
        udpTick.high = data.high;
        udpTick.low = data.low;
        udpTick.prevClose = data.close;
        udpTick.volume = data.volume;
        udpTick.atp = data.avgPrice;
        udpTick.bids[0].price = data.bids[0].price;
        udpTick.bids[0].quantity = data.bids[0].quantity;
        udpTick.asks[0].price = data.asks[0].price;
        udpTick.asks[0].quantity = data.asks[0].quantity;
        onUdpTickUpdate(udpTick);
      }
    }

    m_tokenAddressBook->addCompositeToken(scrip.exchange, "", scrip.token, row);
    m_tokenAddressBook->addIntKeyToken(static_cast<int>(segment), scrip.token,
                                       row);
    emit scripAdded(scrip.symbol, scrip.exchange, scrip.token);
  }
}

bool MarketWatchWindow::addScripFromContract(const ScripData &contractData) {
  if (contractData.token <= 0)
    return false;
  if (hasDuplicate(contractData.token))
    return false;

  ScripData scrip = contractData;

  // Enrich from repository if crucial metadata is missing
  if (scrip.instrumentType.isEmpty() ||
      (scrip.strikePrice == 0 && scrip.optionType.isEmpty())) {
    const ContractData *contract =
        RepositoryManager::getInstance()->getContractByToken(scrip.exchange,
                                                             scrip.token);
    if (contract) {
      if (scrip.instrumentType.isEmpty())
        scrip.instrumentType = contract->series;
      if (scrip.strikePrice == 0)
        scrip.strikePrice = contract->strikePrice;
      if (scrip.optionType.isEmpty())
        scrip.optionType = contract->optionType;
      if (scrip.seriesExpiry.isEmpty())
        scrip.seriesExpiry = contract->expiryDate;
      if (scrip.close <= 0)
        scrip.close = contract->prevClose;
    }
  }

  int newRow = m_model->rowCount();
  m_model->addScrip(scrip);

  // Select the newly added row so the user sees it immediately
  {
    int proxyRow = mapToProxy(newRow);
    if (proxyRow >= 0) {
      QModelIndex proxyIndex = proxyModel()->index(proxyRow, 0);
      clearSelection();
      setCurrentIndex(proxyIndex);
      selectRow(proxyRow);
      scrollTo(proxyIndex, QAbstractItemView::EnsureVisible);

      m_lastFocusedToken = scrip.token;
      m_lastFocusedSymbol = scrip.symbol;
      m_suppressFocusRestore = true;
    }
  }

  TokenSubscriptionManager::instance()->subscribe(scrip.exchange, scrip.token);

  // Subscribe to UDP ticks
  UDP::ExchangeSegment segment = exchangeToSegment(scrip.exchange);
  FeedHandler::instance().subscribeUDP(segment, scrip.token, this,
                                       &MarketWatchWindow::onUdpTickUpdate);

  // Initial Load from Distributed Store (thread-safe snapshot)
  if (m_useZeroCopyPriceCache) {
    auto data = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
        static_cast<int>(segment), scrip.token);
    if (data.token != 0 && data.ltp > 0) {
      UDP::MarketTick udpTick;
      udpTick.exchangeSegment = segment;
      udpTick.token = scrip.token;
      udpTick.ltp = data.ltp;
      udpTick.open = data.open;
      udpTick.high = data.high;
      udpTick.low = data.low;
      udpTick.prevClose = data.close;
      udpTick.volume = data.volume;
      udpTick.atp = data.avgPrice;

      // Copy best depth
      udpTick.bids[0].price = data.bids[0].price;
      udpTick.bids[0].quantity = data.bids[0].quantity;
      udpTick.asks[0].price = data.asks[0].price;
      udpTick.asks[0].quantity = data.asks[0].quantity;

      onUdpTickUpdate(udpTick);
    }
  }

  m_tokenAddressBook->addCompositeToken(scrip.exchange, "", scrip.token,
                                        newRow);
  m_tokenAddressBook->addIntKeyToken(static_cast<int>(segment), scrip.token,
                                     newRow);

  emit scripAdded(scrip.symbol, scrip.exchange, scrip.token);

  // Set focus to the newly added scrip
  setFocusToToken(scrip.token);

  return true;
}

void MarketWatchWindow::removeScrip(int row) {
  if (row < 0 || row >= m_model->rowCount())
    return;
  const ScripData &scrip = m_model->getScripAt(row);
  if (!scrip.isBlankRow && scrip.isValid()) {
    // Clear focus state if removing the focused scrip
    if (scrip.token == m_lastFocusedToken) {
      m_lastFocusedToken = -1;
      m_lastFocusedSymbol.clear();
      qDebug() << "[MarketWatch] Cleared focus state - removed focused scrip:"
               << scrip.symbol;
    }

    TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange,
                                                      scrip.token);

    // Unsubscribe from UDP ticks
    UDP::ExchangeSegment segment = exchangeToSegment(scrip.exchange);
    FeedHandler::instance().unsubscribe(static_cast<int>(segment), scrip.token,
                                        this);

    m_tokenAddressBook->removeCompositeToken(scrip.exchange, "", scrip.token,
                                             row);
    m_tokenAddressBook->removeIntKeyToken(static_cast<int>(segment),
                                          scrip.token, row);
    emit scripRemoved(scrip.token);
  }
  m_model->removeScrip(row);
}

void MarketWatchWindow::clearAll() {
  FeedHandler::instance().unsubscribeAll(this);
  for (int row = 0; row < m_model->rowCount(); ++row) {
    const ScripData &scrip = m_model->getScripAt(row);
    if (scrip.isValid()) {
      TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange,
                                                        scrip.token);
      emit scripRemoved(scrip.token);
    }
  }
  m_tokenAddressBook->clear();
  m_model->clearAll();

  // Clear focus state since all scrips are removed
  m_lastFocusedToken = -1;
  m_lastFocusedSymbol.clear();
}

void MarketWatchWindow::insertBlankRow(int position) {
  m_model->insertBlankRow(position);

  // Select the newly inserted blank row
  int proxyRow = mapToProxy(position);
  if (proxyRow >= 0) {
    QModelIndex proxyIndex = proxyModel()->index(proxyRow, 0);
    clearSelection();
    setCurrentIndex(proxyIndex);
    selectRow(proxyRow);
    scrollTo(proxyIndex, QAbstractItemView::EnsureVisible);
  }
}

void MarketWatchWindow::deleteSelectedRows() {
  QList<int> sourceRows = SelectionHelpers::getSourceRowsDescending(
      this->selectionModel(), proxyModel());
  for (int sourceRow : sourceRows) {
    removeScrip(sourceRow);
  }
}

void MarketWatchWindow::copyToClipboard() {
  QModelIndexList selected = this->selectionModel()->selectedRows();
  if (selected.isEmpty())
    return;

  QString text;
  for (const QModelIndex &proxyIndex : selected) {
    int sourceRow = mapToSource(proxyIndex.row());
    if (sourceRow < 0)
      continue;
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    text += MarketWatchHelpers::formatScripToTSV(scrip) + "\n";
  }
  QApplication::clipboard()->setText(text);
}

void MarketWatchWindow::cutToClipboard() {
  copyToClipboard();
  deleteSelectedRows();
}

void MarketWatchWindow::pasteFromClipboard() {
  QString text = QApplication::clipboard()->text();
  if (text.isEmpty() || !ClipboardHelpers::isValidTSV(text, 3))
    return;

  int insertPosition = m_model->rowCount();
  QModelIndex currentProxyIndex = this->currentIndex();
  if (currentProxyIndex.isValid()) {
    int sourceRow = mapToSource(currentProxyIndex.row());
    if (sourceRow >= 0)
      insertPosition = sourceRow; // Insert above current selection
  }

  QList<QStringList> rows = ClipboardHelpers::parseTSV(text);
  int currentInsertPos = insertPosition;

  for (const QStringList &fields : rows) {
    QString line = fields.join('\t');
    ScripData scrip;
    if (MarketWatchHelpers::parseScripFromTSV(line, scrip) &&
        MarketWatchHelpers::isValidScrip(scrip) && !hasDuplicate(scrip.token)) {

      // Enrich from repository
      const ContractData *contract =
          RepositoryManager::getInstance()->getContractByToken(scrip.exchange,
                                                               scrip.token);
      if (contract) {
        scrip.instrumentType = contract->series;
        scrip.strikePrice = contract->strikePrice;
        scrip.optionType = contract->optionType;
        scrip.seriesExpiry = contract->expiryDate;
        if (scrip.close <= 0)
          scrip.close = contract->prevClose;
      }

      m_model->insertScrip(currentInsertPos, scrip);
      TokenSubscriptionManager::instance()->subscribe(scrip.exchange,
                                                      scrip.token);

      // Subscribe to UDP ticks
      UDP::ExchangeSegment segment = exchangeToSegment(scrip.exchange);
      FeedHandler::instance().subscribeUDP(segment, scrip.token, this,
                                           &MarketWatchWindow::onUdpTickUpdate);

      m_tokenAddressBook->addCompositeToken(scrip.exchange, "", scrip.token,
                                            currentInsertPos);
      m_tokenAddressBook->addIntKeyToken(static_cast<int>(segment), scrip.token,
                                         currentInsertPos);
      emit scripAdded(scrip.symbol, scrip.exchange, scrip.token);
      currentInsertPos++;
    }
  }
}

void MarketWatchWindow::onBuyAction() {
  QModelIndex proxyIndex = this->currentIndex();
  if (!proxyIndex.isValid())
    return;
  int sourceRow = mapToSource(proxyIndex.row());
  if (sourceRow < 0)
    return;
  const ScripData &scrip = m_model->getScripAt(sourceRow);
  if (scrip.isValid())
    emit buyRequested(scrip.symbol, scrip.token);
}

void MarketWatchWindow::onSellAction() {
  QModelIndex proxyIndex = this->currentIndex();
  if (!proxyIndex.isValid())
    return;
  int sourceRow = mapToSource(proxyIndex.row());
  if (sourceRow < 0)
    return;
  const ScripData &scrip = m_model->getScripAt(sourceRow);
  if (scrip.isValid())
    emit sellRequested(scrip.symbol, scrip.token);
}

void MarketWatchWindow::onAddScripAction() {
  // Request ScripBar focus from MainWindow instead of showing a message
  emit scripSearchRequested();
}

void MarketWatchWindow::onMakeDeltaPortfolio() {
  auto repo = RepositoryManager::getInstance();
  if (!repo->isLoaded()) {
    QMessageBox::warning(this, "Delta Portfolio", "Repository not loaded.");
    return;
  }

  // Get available expiries
  auto expiries = repo->getAllExpiries();
  if (expiries.isEmpty()) {
    QMessageBox::warning(this, "Delta Portfolio", "No expiries found.");
    return;
  }

  bool ok;
  QStringList expiryList = expiries.toList();
  QString selectedExpiry =
      QInputDialog::getItem(this, "Make Delta Portfolio",
                            "Select Expiry:", expiryList, 0, false, &ok);
  if (!ok || selectedExpiry.isEmpty()) {
    return;
  }

  // Get all unique symbols from NSEFO
  QStringList allSymbols = repo->getUniqueSymbols("NSE", "FO");

  // We are rebuilding the entire list
  clearAll();

  int addedCount = 0;
  QList<ScripData> bulkScrips;

  for (const QString &symbol : allSymbols) {
    // Check if this symbol has a future in the selected expiry using O(1) cache
    // lookup
    int64_t futureToken =
        repo->getFutureTokenForSymbolExpiry(symbol, selectedExpiry);
    if (futureToken <= 0)
      continue;

    // Retrieve underlying price
    int64_t assetToken = repo->getAssetTokenForSymbol(symbol);
    double basePrice = 0.0;

    if (assetToken > 0) {
      auto data = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
          1, static_cast<int>(assetToken));
      if (data.token != 0 && data.ltp > 0)
        basePrice = data.ltp;
    }
    if (basePrice <= 0 && futureToken > 0) {
      auto data = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
          2, futureToken);
      if (data.token != 0 && data.ltp > 0)
        basePrice = data.ltp;
    }
    if (basePrice <= 0) {
      basePrice = repo->getUnderlyingPrice(symbol, selectedExpiry);
    }

    // We strictly need to know the price to calculate ATM strikes
    if (basePrice <= 0)
      continue;

    const QVector<double> &strikeList =
        repo->getStrikesForSymbolExpiry(symbol, selectedExpiry);
    if (strikeList.isEmpty())
      continue;

    auto atmResult = ATMCalculator::calculateFromActualStrikes(
        basePrice, strikeList, 5); // 5 strikes each side
    if (!atmResult.isValid)
      continue;

    // insert blank row as a separator between symbols
    if (addedCount > 0) {
      bulkScrips.append(ScripData::createBlankRow());
    }

    if (assetToken > 0) {
      ScripData scrip;
      scrip.symbol = symbol;
      scrip.exchange = "NSECM";
      scrip.token = static_cast<int>(assetToken);
      bulkScrips.append(scrip);
    }

    ScripData futScrip;
    futScrip.symbol = symbol + " FUT";
    futScrip.exchange = "NSEFO";
    futScrip.token = static_cast<int>(futureToken);
    bulkScrips.append(futScrip);

    // Add a blank row to separate underlying from options
    bulkScrips.append(ScripData::createBlankRow());

    // Get CE/PE for each strike
    for (double strike : atmResult.strikes) {
      auto tokens = repo->getTokensForStrike(symbol, selectedExpiry, strike);

      if (tokens.first > 0) {
        ScripData ce;
        ce.symbol = symbol + " " + QString::number(strike) + " CE";
        ce.exchange = "NSEFO";
        ce.token = static_cast<int>(tokens.first);
        bulkScrips.append(ce);
      }
      if (tokens.second > 0) {
        ScripData pe;
        pe.symbol = symbol + " " + QString::number(strike) + " PE";
        pe.exchange = "NSEFO";
        pe.token = static_cast<int>(tokens.second);
        bulkScrips.append(pe);
      }
    }

    addedCount++;
  }

  addScrips(bulkScrips);
}

void MarketWatchWindow::performRowMoveByTokens(const QList<int> &tokens,
                                               int targetSourceRow) {
  if (tokens.isEmpty())
    return;
  this->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);

  QList<int> sourceRows;
  QList<ScripData> scrips;
  for (int token : tokens) {
    int row = m_model->findScripByToken(token);
    if (row >= 0) {
      sourceRows.append(row);
      scrips.append(m_model->getScripAt(row));
    }
  }
  if (sourceRows.isEmpty())
    return;

  if (sourceRows.size() == 1) {
    int sourceRow = sourceRows.first();
    const ScripData &scrip = scrips.first();
    if (sourceRow == targetSourceRow || sourceRow == targetSourceRow - 1)
      return;
    m_tokenAddressBook->onRowsRemoved(sourceRow, 1);
    m_model->moveRow(sourceRow, targetSourceRow);
    int finalPos =
        (sourceRow < targetSourceRow) ? targetSourceRow - 1 : targetSourceRow;
    m_tokenAddressBook->onRowsInserted(finalPos, 1);
    m_tokenAddressBook->addCompositeToken(scrip.exchange, "", scrip.token,
                                          finalPos);

    // Derive segment for int key (similar to addScrip logic)
    // We can optimize this by storing segment in scrip but for now simple
    // mapping is fine for move operations
    int segment = RepositoryManager::getExchangeSegmentID(
        scrip.exchange.left(3), scrip.exchange.mid(3));
    if (segment > 0) {
      m_tokenAddressBook->addIntKeyToken(segment, scrip.token, finalPos);
    }

    int proxyPos = mapToProxy(finalPos);
    if (proxyPos >= 0) {
      QModelIndex pidx = proxyModel()->index(proxyPos, 0);
      selectionModel()->select(pidx, QItemSelectionModel::ClearAndSelect |
                                         QItemSelectionModel::Rows);
      setCurrentIndex(pidx);
    }
  } else {
    QList<int> sortedRows = sourceRows;
    std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());
    int adjustment = 0;
    for (int sourceRow : sortedRows) {
      if (sourceRow < targetSourceRow)
        adjustment++;
      removeScrip(sourceRow);
    }
    int adjustedTarget = std::max(
        0, std::min(m_model->rowCount(), targetSourceRow - adjustment));
    for (int i = 0; i < scrips.size(); ++i) {
      m_model->insertScrip(adjustedTarget + i, scrips[i]);
      m_tokenAddressBook->addCompositeToken(
          scrips[i].exchange, "", scrips[i].token, adjustedTarget + i);

      int segment = RepositoryManager::getExchangeSegmentID(
          scrips[i].exchange.left(3), scrips[i].exchange.mid(3));
      if (segment > 0) {
        m_tokenAddressBook->addIntKeyToken(segment, scrips[i].token,
                                           adjustedTarget + i);
      }

      // Re-subscribe after multi-row move (removeScrip unsubscribed them)
      if (scrips[i].isValid()) {
        TokenSubscriptionManager::instance()->subscribe(scrips[i].exchange,
                                                        scrips[i].token);
        UDP::ExchangeSegment udpSeg = exchangeToSegment(scrips[i].exchange);
        FeedHandler::instance().subscribeUDP(
            udpSeg, scrips[i].token, this, &MarketWatchWindow::onUdpTickUpdate);
      }
    }
  }
}

void MarketWatchWindow::onSavePortfolio() {
  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Save Portfolio"), "",
      tr("Portfolio Files (*.json);;All Files (*)"));
  if (fileName.isEmpty())
    return;

  QList<ScripData> scrips;
  int rows = m_model->rowCount();
  for (int i = 0; i < rows; ++i) {
    scrips.append(m_model->getScripAt(i));
  }

  // Capture current visual state (widths & order) into model's profile
  GenericTableProfile currentProfile = m_model->getColumnProfile();
  captureProfileFromView(currentProfile);

  // DON'T update model during save - it triggers reset and corrupts state!
  // The profile is saved to file, model doesn't need updating here.

  if (MarketWatchHelpers::savePortfolio(fileName, scrips, currentProfile)) {
    QMessageBox::information(this, tr("Success"),
                             tr("Portfolio saved successfully."));
  } else {
    QMessageBox::critical(this, tr("Error"), tr("Failed to save portfolio."));
  }
}

void MarketWatchWindow::onLoadPortfolio() {
  QString fileName = QFileDialog::getOpenFileName(
      this, tr("Load Portfolio"), "",
      tr("Portfolio Files (*.json);;All Files (*)"));
  if (fileName.isEmpty())
    return;

  QList<ScripData> scrips;
  GenericTableProfile profile;

  // Attempt to load scrips AND profile
  if (!MarketWatchHelpers::loadPortfolio(fileName, scrips, profile)) {
    QMessageBox::critical(this, tr("Error"), tr("Failed to load portfolio."));
    return;
  }

  // Confirm before clearing
  if (m_model->rowCount() > 0) {
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        this, tr("Confirm Load"),
        tr("Loading a portfolio will clear current list. Continue?"),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::No)
      return;
  }

  clearAll();

  for (const auto &scrip : scrips) {
    if (scrip.isBlankRow) {
      insertBlankRow(m_model->rowCount());
    } else {
      addScripFromContract(scrip);
    }
  }

  // Apply the loaded profile keys to view
  if (!profile.name().isEmpty()) {
    m_model->setColumnProfile(profile);
    applyProfileToView(profile);
    qDebug() << "[MarketWatchWindow] Applied loaded profile:" << profile.name();
  }

  QMessageBox::information(this, tr("Success"),
                           tr("Portfolio loaded successfully."));
}

void MarketWatchWindow::exportPriceCacheDebug() {
  QMessageBox::information(this, "Debug",
                           "This feature is currently being refactored for the "
                           "new Distributed Price Store architecture.");
}

void MarketWatchWindow::saveState(QSettings &settings) {
  // Save Scrips
  QJsonArray scripsArray;
  int rows = m_model->rowCount();
  for (int i = 0; i < rows; ++i) {
    // Use helper to serialize scrip
    scripsArray.append(MarketWatchHelpers::scripToJson(m_model->getScripAt(i)));
  }
  settings.setValue("scrips",
                    QJsonDocument(scripsArray).toJson(QJsonDocument::Compact));

  // Save Column Profile & Geometry (delegated to helper which saves to specific
  // keys) However, WindowSettingsHelper saves to a *global* location usually
  // based on window type. For workspace-specific saving, we should save the
  // profile into THIS settings object.

  // Serialize current column profile
  settings.setValue("columnProfile",
                    QJsonDocument(m_model->getColumnProfile().toJson())
                        .toJson(QJsonDocument::Compact));
}

void MarketWatchWindow::restoreState(QSettings &settings) {
  // Restore Scrips
  if (settings.contains("scrips")) {
    QByteArray data = settings.value("scrips").toByteArray();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isArray()) {
      QJsonArray scripsArray = doc.array();
      for (const QJsonValue &val : scripsArray) {
        ScripData scrip = MarketWatchHelpers::scripFromJson(val.toObject());
        if (scrip.isBlankRow) {
          insertBlankRow(m_model->rowCount());
        } else {
          addScripFromContract(scrip);
        }
      }
    }
  }

  // Restore Column Profile
  if (settings.contains("columnProfile")) {
    QByteArray data = settings.value("columnProfile").toByteArray();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
      GenericTableProfile profile;
      profile.fromJson(doc.object());
      if (!profile.name().isEmpty()) {
        m_model->setColumnProfile(profile);
        applyProfileToView(profile); // Apply to view (widths + order)
        qDebug() << "[MarketWatchWindow] Restored column profile:"
                 << profile.name();
      } else {
        qWarning() << "[MarketWatchWindow] Failed to parse column profile from "
                      "settings";
      }
    }
  }
}

void MarketWatchWindow::captureProfileFromView(GenericTableProfile &profile) {
  QHeaderView *header = this->horizontalHeader();
  QList<int> modelColumns = m_model->getColumnProfile().visibleColumns();

  // Build a map of logical index -> column ID for currently visible columns
  QMap<int, int> logicalToColumn;
  for (int i = 0; i < modelColumns.size(); ++i) {
    logicalToColumn[i] = modelColumns[i];
  }

  QList<int> newVisibleOrder;
  QList<int> capturedVisible;

  // 1. Capture ACTUAL visual order from QHeaderView
  for (int visualIdx = 0; visualIdx < header->count(); ++visualIdx) {
    int logicalIdx = header->logicalIndex(visualIdx);

    if (logicalToColumn.contains(logicalIdx)) {
      int col = logicalToColumn[logicalIdx];

      // Capture width from the logical section
      int width = header->sectionSize(logicalIdx);
      if (width > 0) {
        profile.setColumnWidth(col, width);
      }

      // Column is visible if not hidden
      bool isHidden = header->isSectionHidden(logicalIdx);
      profile.setColumnVisible(col, !isHidden);

      if (!isHidden) {
        newVisibleOrder.append(col);
        capturedVisible.append(col);
      }
    }
  }

  // 2. Build complete column order: visible columns in visual order, then
  // hidden columns
  QList<int> completeOrder = newVisibleOrder;

  // Add hidden columns at the end, maintaining their original relative order
  QList<int> existingOrder = profile.columnOrder();
  for (int col : existingOrder) {
    if (!capturedVisible.contains(col)) {
      completeOrder.append(col);
    }
  }

  profile.setColumnOrder(completeOrder);

  qDebug() << "[captureProfileFromView] Captured" << newVisibleOrder.size()
           << "visible columns in visual order, total order size:"
           << completeOrder.size();
}

void MarketWatchWindow::applyProfileToView(const GenericTableProfile &profile) {
  QHeaderView *header = this->horizontalHeader();
  QList<int> visibleCols = profile.visibleColumns();

  // Build map of column ID -> logical index in the model
  QMap<int, int> columnToLogicalIndex;
  for (int i = 0; i < visibleCols.size(); ++i) {
    columnToLogicalIndex[visibleCols[i]] = i;
  }

  // Move sections to match profile's intended visual order
  for (int targetVisualIdx = 0; targetVisualIdx < visibleCols.size();
       ++targetVisualIdx) {
    if (targetVisualIdx >= header->count())
      break;

    int col = visibleCols[targetVisualIdx];
    int logicalIdx = columnToLogicalIndex.value(col, -1);

    if (logicalIdx >= 0) {
      // Move this logical section to the target visual position
      int currentVisualIdx = header->visualIndex(logicalIdx);
      if (currentVisualIdx != targetVisualIdx) {
        header->moveSection(currentVisualIdx, targetVisualIdx);
      }

      // Apply width
      int width = profile.columnWidth(col);
      if (width > 0) {
        header->resizeSection(logicalIdx, width);
      }

      // Ensure visible
      header->setSectionHidden(logicalIdx, false);
    }
  }

  qDebug() << "[applyProfileToView] Applied column order and widths for"
           << visibleCols.size() << "columns";
}
