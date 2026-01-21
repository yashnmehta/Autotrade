#include "ui/ATMWatchWindow.h"
#include "data/PriceStoreGateway.h"
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include "nsecm_price_store.h"
#include "nsefo_price_store.h"
#include <QDate>
#include <QDebug>
#include <QEvent>
#include <QHeaderView>
#include <QScrollBar>
#include <QWheelEvent>
#include <QtConcurrent>
#include <QApplication>

#define OFFSET_CALL 0
#define OFFSET_SYM  6
#define OFFSET_PUT  10

ATMWatchWindow::ATMWatchWindow(QWidget *parent) : QWidget(parent), 
    m_mainModel(nullptr), m_mainProxy(nullptr) {
  setupUI();
  setupModels();
  setupConnections();

  // Initialize base price update timer (every 1 second)
  m_basePriceTimer = new QTimer(this);
  connect(m_basePriceTimer, &QTimer::timeout, this,
          &ATMWatchWindow::onBasePriceUpdate);
  m_basePriceTimer->start(1000);
}

ATMWatchWindow::~ATMWatchWindow() {
  FeedHandler::instance().unsubscribeAll(this);
}

void ATMWatchWindow::setupUI() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Filter Bar
  auto *filterBar = new QHBoxLayout();
  filterBar->setContentsMargins(10, 5, 10, 5);

  m_exchangeCombo = new QComboBox();
  m_exchangeCombo->addItems({"NSE"});
  m_exchangeCombo->setFixedWidth(100);

  m_expiryCombo = new QComboBox();
  m_expiryCombo->setFixedWidth(150);

  m_statusLabel = new QLabel("Ready");
  m_statusLabel->setStyleSheet("color: #AAAAAA;");

  filterBar->addWidget(new QLabel("Exchange:"));
  filterBar->addWidget(m_exchangeCombo);
  filterBar->addWidget(new QLabel("Expiry:"));
  filterBar->addWidget(m_expiryCombo);
  filterBar->addStretch();
  filterBar->addWidget(m_statusLabel);

  mainLayout->addLayout(filterBar);

  // Tables
  auto *tableLayout = new QHBoxLayout();
  tableLayout->setSpacing(0);

  auto setupTable = [this](QTableView *table) {
    table->setShowGrid(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // table->setAlternatingRowColors(true);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->verticalHeader()->setVisible(false); // Hide vertical headers
    table->viewport()->installEventFilter(this);
    table->setStyleSheet(
        "QTableView { background-color: #1E2A38; color: #FFFFFF; "
        "gridline-color: #2A3A50; border: 1px solid #3A4A60; }"
        "QHeaderView::section { background-color: #2A3A50; color: #FFFFFF; "
        "padding: 4px; border: 1px solid #3A4A60; font-weight: bold; }");
  };

  m_callTable = new QTableView();
  m_symbolTable = new QTableView();
  m_putTable = new QTableView();

  setupTable(m_callTable);
  setupTable(m_symbolTable);
  setupTable(m_putTable);

  // Symbols table (Middle) should show vertical scrollbar
  m_symbolTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_symbolTable->setStyleSheet(m_symbolTable->styleSheet() +
                               "QTableView { color: #FFFF00; }");

  tableLayout->addWidget(m_callTable, 2);
  tableLayout->addWidget(m_symbolTable, 1);
  tableLayout->addWidget(m_putTable, 2);

  mainLayout->addLayout(tableLayout);
  setStyleSheet("QWidget { background-color: #1E2A38; }");
}

void ATMWatchWindow::setupModels() {
  const int TOTAL_COLS = CALL_COUNT + SYM_COUNT + PUT_COUNT;
  m_mainModel = new QStandardItemModel(this);
  m_mainModel->setColumnCount(TOTAL_COLS);

  QStringList labels;
  labels << "LTP" << "Chg" << "Bid" << "Ask" << "Vol" << "OI"; // Call 0-5
  labels << "Symbol" << "Spot/Fut" << "ATM" << "Expiry";      // Sym 6-9
  labels << "Bid" << "Ask" << "LTP" << "Chg" << "Vol" << "OI"; // Put 10-15
  m_mainModel->setHorizontalHeaderLabels(labels);

  m_mainProxy = new QSortFilterProxyModel(this);
  m_mainProxy->setSourceModel(m_mainModel);
  m_mainProxy->setSortRole(Qt::EditRole);

  auto configureTable = [this](QTableView *table, int startCol, int count) {
    table->setModel(m_mainProxy);
    table->setSortingEnabled(true);
    for (int i = 0; i < m_mainModel->columnCount(); ++i) {
      if (i < startCol || i >= startCol + count) {
        table->setColumnHidden(i, true);
      } else {
        table->setColumnHidden(i, false);
      }
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  };

  configureTable(m_callTable, OFFSET_CALL, CALL_COUNT);
  configureTable(m_symbolTable, OFFSET_SYM, SYM_COUNT);
  configureTable(m_putTable, OFFSET_PUT, PUT_COUNT);
}

void ATMWatchWindow::setupConnections() {
  connect(ATMWatchManager::getInstance(), &ATMWatchManager::atmUpdated, this,
          &ATMWatchWindow::onATMUpdated);

  // Filter connections
  connect(m_exchangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ATMWatchWindow::onExchangeChanged);
  connect(m_expiryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ATMWatchWindow::onExpiryChanged);

  // Sync scrolling using the middle table as master
  connect(m_symbolTable->verticalScrollBar(), &QScrollBar::valueChanged, this,
          &ATMWatchWindow::synchronizeScrollBars);

  // Context connections (DoubleClick to open Buy/Sell)
  auto onRowDoubleClicked = [this](const QModelIndex &) {
    qDebug() << "[ATMWatch] Row double clicked";
  };
  connect(m_callTable, &QTableView::doubleClicked, onRowDoubleClicked);
  connect(m_symbolTable, &QTableView::doubleClicked, onRowDoubleClicked);
  connect(m_putTable, &QTableView::doubleClicked, onRowDoubleClicked);

  // Initialize
  m_currentExchange = "NSE";
  populateCommonExpiries(m_currentExchange);
}

void ATMWatchWindow::refreshData() {
  FeedHandler::instance().unsubscribeAll(this);
  m_tokenToInfo.clear();
  m_symbolToRow.clear();
  m_underlyingToRow.clear();

  m_mainModel->setRowCount(0);
  m_underlyingToSymbol.clear(); // Clear mapping for this refresh

  auto atmList = ATMWatchManager::getInstance()->getATMWatchArray();
  FeedHandler &feed = FeedHandler::instance();

  qDebug() << "[ATMWatch] refreshData() called. atmList size:" << atmList.size();

  int rowIdx = 0;
  for (const auto &info : atmList) {
    m_mainModel->insertRow(rowIdx);
    m_symbolToRow[info.symbol] = rowIdx;

    // Populate Symbol Table Part
    m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_SYM + SYM_NAME), info.symbol);
    m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_SYM + SYM_PRICE), info.basePrice, Qt::EditRole);
    m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_SYM + SYM_PRICE), QString::number(info.basePrice, 'f', 2), Qt::DisplayRole);
    m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_SYM + SYM_ATM), info.atmStrike, Qt::EditRole);
    m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_SYM + SYM_ATM), QString::number(info.atmStrike, 'f', 2), Qt::DisplayRole);
    m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_SYM + SYM_EXPIRY), info.expiry);

    int64_t underlyingToken = m_symbolToUnderlyingToken.value(info.symbol, 0);
    if (underlyingToken > 0) {
      m_underlyingToSymbol[underlyingToken] = info.symbol;
      
      // Ensure PriceStore is ready for this token (especially if master file missing)
      nsecm::g_nseCmPriceStore.initializeToken(static_cast<uint32_t>(underlyingToken), info.symbol.toUtf8().constData(), "", "", 0, 0, 0, 0);
      
      // Subscribe to Cash (Segment 1)
      feed.subscribe(1, static_cast<int>(underlyingToken), this, &ATMWatchWindow::onTickUpdate);
      
      if (info.symbol == "NIFTY") {
          qDebug() << "[ATMWatch] Refresh: Subscribed to NIFTY Cash/Index (26000)";
      }
    }

    // Also subscribe to Future token for Segment 2
    int64_t futureToken = RepositoryManager::getInstance()->getFutureTokenForSymbolExpiry(info.symbol, info.expiry);
    if (futureToken > 0) {
      m_underlyingToSymbol[futureToken] = info.symbol;
      feed.subscribe(2, static_cast<int>(futureToken), this, &ATMWatchWindow::onTickUpdate);
      if (info.symbol == "NIFTY") {
          qDebug() << "[ATMWatch] Refresh: Subscribed to NIFTY Future:" << futureToken;
      }
    }

    if (info.isValid) {
      // Tokens for callbacks
      m_tokenToInfo[info.callToken] = {info.symbol, true};
      m_tokenToInfo[info.putToken] = {info.symbol, false};

      // Subscribe to options
      feed.subscribe(1, static_cast<int>(info.callToken), this, &ATMWatchWindow::onTickUpdate);
      feed.subscribe(1, static_cast<int>(info.putToken), this, &ATMWatchWindow::onTickUpdate);

      // Initial Call Data
      auto *cState = nsefo::g_nseFoPriceStore.getUnifiedState(static_cast<uint32_t>(info.callToken));
      if (cState) {
        m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_CALL + CALL_LTP), cState->ltp > 0 ? cState->ltp : cState->close, Qt::EditRole);
        m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_CALL + CALL_LTP), QString::number(cState->ltp > 0 ? cState->ltp : cState->close, 'f', 2), Qt::DisplayRole);
        m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_CALL + CALL_BID), cState->bids[0].price, Qt::EditRole);
        m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_CALL + CALL_BID), QString::number(cState->bids[0].price, 'f', 2), Qt::DisplayRole);
        m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_CALL + CALL_ASK), cState->asks[0].price, Qt::EditRole);
        m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_CALL + CALL_ASK), QString::number(cState->asks[0].price, 'f', 2), Qt::DisplayRole);
      }

      // Initial Put Data
      auto *pState = nsefo::g_nseFoPriceStore.getUnifiedState(static_cast<uint32_t>(info.putToken));
      if (pState) {
        m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_PUT + PUT_LTP), pState->ltp > 0 ? pState->ltp : pState->close, Qt::EditRole);
        m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_PUT + PUT_LTP), QString::number(pState->ltp > 0 ? pState->ltp : pState->close, 'f', 2), Qt::DisplayRole);
        m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_PUT + PUT_BID), pState->bids[0].price, Qt::EditRole);
        m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_PUT + PUT_BID), QString::number(pState->bids[0].price, 'f', 2), Qt::DisplayRole);
        m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_PUT + PUT_ASK), pState->asks[0].price, Qt::EditRole);
        m_mainModel->setData(m_mainModel->index(rowIdx, OFFSET_PUT + PUT_ASK), QString::number(pState->asks[0].price, 'f', 2), Qt::DisplayRole);
      }
    }
    rowIdx++;
  }
}

void ATMWatchWindow::onATMUpdated() { refreshData(); }

void ATMWatchWindow::onTickUpdate(const UDP::MarketTick &tick) {
  // 1. Option Tick
  if (m_tokenToInfo.contains(tick.token)) {
    auto info = m_tokenToInfo[tick.token];
    int row = m_symbolToRow.value(info.first, -1);
    if (row < 0)
      return;

    if (info.second) { // Call
      m_mainModel->setData(m_mainModel->index(row, OFFSET_CALL + CALL_LTP), tick.ltp, Qt::EditRole);
      m_mainModel->setData(m_mainModel->index(row, OFFSET_CALL + CALL_LTP), QString::number(tick.ltp, 'f', 2), Qt::DisplayRole);
      
      m_mainModel->setData(m_mainModel->index(row, OFFSET_CALL + CALL_BID), tick.bestBid(), Qt::EditRole);
      m_mainModel->setData(m_mainModel->index(row, OFFSET_CALL + CALL_BID), QString::number(tick.bestBid(), 'f', 2), Qt::DisplayRole);
      
      m_mainModel->setData(m_mainModel->index(row, OFFSET_CALL + CALL_ASK), tick.bestAsk(), Qt::EditRole);
      m_mainModel->setData(m_mainModel->index(row, OFFSET_CALL + CALL_ASK), QString::number(tick.bestAsk(), 'f', 2), Qt::DisplayRole);
      
      m_mainModel->setData(m_mainModel->index(row, OFFSET_CALL + CALL_VOL), (qlonglong)tick.volume, Qt::EditRole);
      m_mainModel->setData(m_mainModel->index(row, OFFSET_CALL + CALL_OI), (qlonglong)tick.openInterest, Qt::EditRole);
    } else { // Put
      m_mainModel->setData(m_mainModel->index(row, OFFSET_PUT + PUT_LTP), tick.ltp, Qt::EditRole);
      m_mainModel->setData(m_mainModel->index(row, OFFSET_PUT + PUT_LTP), QString::number(tick.ltp, 'f', 2), Qt::DisplayRole);
      
      m_mainModel->setData(m_mainModel->index(row, OFFSET_PUT + PUT_BID), tick.bestBid(), Qt::EditRole);
      m_mainModel->setData(m_mainModel->index(row, OFFSET_PUT + PUT_BID), QString::number(tick.bestBid(), 'f', 2), Qt::DisplayRole);
      
      m_mainModel->setData(m_mainModel->index(row, OFFSET_PUT + PUT_ASK), tick.bestAsk(), Qt::EditRole);
      m_mainModel->setData(m_mainModel->index(row, OFFSET_PUT + PUT_ASK), QString::number(tick.bestAsk(), 'f', 2), Qt::DisplayRole);
      
      m_mainModel->setData(m_mainModel->index(row, OFFSET_PUT + PUT_VOL), (qlonglong)tick.volume, Qt::EditRole);
      m_mainModel->setData(m_mainModel->index(row, OFFSET_PUT + PUT_OI), (qlonglong)tick.openInterest, Qt::EditRole);
    }
    return;
  }

  // 2. Underlying Asset Tick
  QString symbol = m_underlyingToSymbol.value(tick.token);
  if (!symbol.isEmpty()) {
    if (symbol == "NIFTY") {
        qDebug() << "[ATMWatch] Received NIFTY underlying tick. LTP:" << tick.ltp;
    }
    int row = m_symbolToRow.value(symbol, -1);
    if (row >= 0) {
      m_mainModel->setData(m_mainModel->index(row, OFFSET_SYM + SYM_PRICE), tick.ltp, Qt::EditRole);
      m_mainModel->setData(m_mainModel->index(row, OFFSET_SYM + SYM_PRICE), QString::number(tick.ltp, 'f', 2), Qt::DisplayRole);
    }
  }
}

void ATMWatchWindow::synchronizeScrollBars(int value) {
  m_callTable->verticalScrollBar()->setValue(value);
  m_putTable->verticalScrollBar()->setValue(value);
}

bool ATMWatchWindow::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::Wheel) {
    if (obj == m_callTable->viewport() || obj == m_putTable->viewport()) {
      QApplication::sendEvent(m_symbolTable->viewport(), event);
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

void ATMWatchWindow::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  loadAllSymbols();
}

void ATMWatchWindow::onExchangeChanged(int index) {
  m_currentExchange = m_exchangeCombo->currentText();
  populateCommonExpiries(m_currentExchange);
  loadAllSymbols();
}

void ATMWatchWindow::onExpiryChanged(int index) {
  m_currentExpiry = m_expiryCombo->currentData().toString();
  qDebug() << "[ATMWatch] Expiry changed to:" << m_currentExpiry;
  loadAllSymbols();
}

void ATMWatchWindow::loadAllSymbols() {
  auto repo = RepositoryManager::getInstance();
  if (!repo->isLoaded()) {
    m_statusLabel->setText("Repository not loaded");
    return;
  }

  m_statusLabel->setText("Loading symbols...");
  QString currentExpiry = m_currentExpiry;

  QPointer<ATMWatchWindow> self(this);
  QtConcurrent::run([self, repo, currentExpiry]() {
    if (!self) return;

    QVector<QString> optionSymbols = repo->getOptionSymbols();
    QVector<QPair<QString, QString>> watchConfigs;

    for (const QString &symbol : optionSymbols) {
      if (!self) break;
      QString expiry;
      if (currentExpiry == "CURRENT" || currentExpiry.isEmpty()) {
        expiry = repo->getCurrentExpiry(symbol);
      } else {
        QVector<QString> symbolsForExpiry = repo->getSymbolsForExpiry(currentExpiry);
        if (symbolsForExpiry.contains(symbol)) expiry = currentExpiry;
      }

      if (!expiry.isEmpty()) watchConfigs.append(qMakePair(symbol, expiry));
    }

    if (!self) return;

    for (const auto &pair : watchConfigs) {
      int64_t assetToken = repo->getAssetTokenForSymbol(pair.first);
      int64_t futureToken = repo->getFutureTokenForSymbolExpiry(pair.first, pair.second);
      
      if (assetToken > 0) {
        // Force-initialize the token in the CM PriceStore so it accepts ticks
        nsecm::g_nseCmPriceStore.initializeToken(static_cast<uint32_t>(assetToken), pair.first.toUtf8().constData(), "", "", 0, 0, 0, 0);

        // Subscribe for real-time Cash updates
        FeedHandler::instance().subscribe(1, static_cast<int>(assetToken), self.data(), &ATMWatchWindow::onTickUpdate);
      }

      if (futureToken > 0) {
        // Subscribe for real-time Future updates
        FeedHandler::instance().subscribe(2, static_cast<int>(futureToken), self.data(), &ATMWatchWindow::onTickUpdate);
      }

      QMetaObject::invokeMethod(self.data(), [self, pair, assetToken, futureToken]() {
          if (self) {
            if (assetToken > 0) {
              self->m_symbolToUnderlyingToken[pair.first] = assetToken;
              self->m_underlyingToSymbol[assetToken] = pair.first;
            }
            if (futureToken > 0) {
              self->m_underlyingToSymbol[futureToken] = pair.first;
            }
          }
      });
    }

    QMetaObject::invokeMethod(ATMWatchManager::getInstance(), [watchConfigs]() {
      ATMWatchManager::getInstance()->addWatchesBatch(watchConfigs);
    });

    QMetaObject::invokeMethod(self.data(), "onSymbolsLoaded", Qt::QueuedConnection, Q_ARG(int, watchConfigs.size()));
  });
}

void ATMWatchWindow::populateCommonExpiries(const QString &exchange) {
  m_expiryCombo->clear();
  m_expiryCombo->addItem("Current (Nearest)", "CURRENT");

  auto repo = RepositoryManager::getInstance();
  if (!repo->isLoaded()) return;

  QVector<QString> expiries = repo->getAllExpiries();
  for (const QString &expiry : expiries) {
    m_expiryCombo->addItem(expiry, expiry);
  }
}

void ATMWatchWindow::onBasePriceUpdate() {
  updateBasePrices();
}

void ATMWatchWindow::onSymbolsLoaded(int count) {
  m_statusLabel->setText(QString("Loaded %1 symbols").arg(count));
}

void ATMWatchWindow::updateBasePrices() {
  if (!m_mainModel) return;
  auto atmList = ATMWatchManager::getInstance()->getATMWatchArray();

  for (const auto &info : atmList) {
    int row = m_symbolToRow.value(info.symbol, -1);
    if (row < 0) continue;

    double livePrice = 0.0;
    int64_t token = m_symbolToUnderlyingToken.value(info.symbol, 0);
    if (token > 0) {
      // Use getGenericLtp which handles Indices (mapping tokens to IndexStore names) correctly
      livePrice = nsecm::getGenericLtp(static_cast<uint32_t>(token));
      
      if (livePrice <= 0) {
        auto *foState = nsefo::g_nseFoPriceStore.getUnifiedState(static_cast<uint32_t>(token));
        if (foState && foState->ltp > 0) livePrice = foState->ltp;
      }
      
      if (info.symbol == "NIFTY") {
          // qDebug() << "[ATMWatch] NIFTY livePrice from generic lookup:" << livePrice;
      }
    }

    if (livePrice <= 0) livePrice = info.basePrice;

    m_mainModel->setData(m_mainModel->index(row, OFFSET_SYM + SYM_PRICE), livePrice, Qt::EditRole);
    m_mainModel->setData(m_mainModel->index(row, OFFSET_SYM + SYM_PRICE), QString::number(livePrice, 'f', 2), Qt::DisplayRole);

    double currentATM = m_mainModel->data(m_mainModel->index(row, OFFSET_SYM + SYM_ATM), Qt::EditRole).toDouble();
    if (qAbs(currentATM - info.atmStrike) > 0.01) {
      m_mainModel->setData(m_mainModel->index(row, OFFSET_SYM + SYM_ATM), info.atmStrike, Qt::EditRole);
      m_mainModel->setData(m_mainModel->index(row, OFFSET_SYM + SYM_ATM), QString::number(info.atmStrike, 'f', 2), Qt::DisplayRole);
    }
  }
}

WindowContext ATMWatchWindow::getSelectedContext() const {
  WindowContext ctx;
  ctx.sourceWindow = "ATMWatch";

  QTableView *activeTable = nullptr;
  bool isCall = false;
  bool isPut = false;

  if (m_callTable->hasFocus()) {
    activeTable = m_callTable; isCall = true;
  } else if (m_putTable->hasFocus()) {
    activeTable = m_putTable; isPut = true;
  } else if (m_symbolTable->hasFocus()) {
    activeTable = m_symbolTable;
  }

  if (!activeTable) {
    if (!m_callTable->selectionModel()->selectedIndexes().isEmpty()) { activeTable = m_callTable; isCall = true; }
    else if (!m_putTable->selectionModel()->selectedIndexes().isEmpty()) { activeTable = m_putTable; isPut = true; }
    else if (!m_symbolTable->selectionModel()->selectedIndexes().isEmpty()) { activeTable = m_symbolTable; }
  }

  if (!activeTable) return ctx;

  QModelIndex currentIndex = activeTable->currentIndex();
  if (!currentIndex.isValid()) return ctx;

  QModelIndex modelIndex = m_mainProxy->mapToSource(currentIndex);
  int row = modelIndex.row();

  QString symbol = m_mainModel->data(m_mainModel->index(row, OFFSET_SYM + SYM_NAME)).toString();
  auto atmList = ATMWatchManager::getInstance()->getATMWatchArray();

  ATMWatchManager::ATMInfo targetInfo;
  for (const auto &info : atmList) {
    if (info.symbol == symbol) {
      targetInfo = info;
      break;
    }
  }

  if (targetInfo.symbol.isEmpty()) return ctx;

  ctx.symbol = targetInfo.symbol;
  ctx.expiry = targetInfo.expiry;
  ctx.exchange = m_currentExchange;

  if (isCall) {
    ctx.token = targetInfo.callToken; ctx.optionType = "CE"; ctx.strikePrice = targetInfo.atmStrike;
    ctx.ltp = m_mainModel->data(m_mainModel->index(row, OFFSET_CALL + CALL_LTP), Qt::EditRole).toDouble();
    ctx.bid = m_mainModel->data(m_mainModel->index(row, OFFSET_CALL + CALL_BID), Qt::EditRole).toDouble();
    ctx.ask = m_mainModel->data(m_mainModel->index(row, OFFSET_CALL + CALL_ASK), Qt::EditRole).toDouble();
  } else if (isPut) {
    ctx.token = targetInfo.putToken; ctx.optionType = "PE"; ctx.strikePrice = targetInfo.atmStrike;
    ctx.ltp = m_mainModel->data(m_mainModel->index(row, OFFSET_PUT + PUT_LTP), Qt::EditRole).toDouble();
    ctx.bid = m_mainModel->data(m_mainModel->index(row, OFFSET_PUT + PUT_BID), Qt::EditRole).toDouble();
    ctx.ask = m_mainModel->data(m_mainModel->index(row, OFFSET_PUT + PUT_ASK), Qt::EditRole).toDouble();
  } else {
    ctx.token = m_symbolToUnderlyingToken.value(targetInfo.symbol, 0); ctx.optionType = "XX";
    ctx.ltp = m_mainModel->data(m_mainModel->index(row, OFFSET_SYM + SYM_PRICE), Qt::EditRole).toDouble();
  }

  return ctx;
}
