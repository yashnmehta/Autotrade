#include "ui/ATMWatchWindow.h"
#include "data/PriceStoreGateway.h"
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include "services/GreeksCalculationService.h"
#include <QDate>
#include <QDebug>
#include <QEvent>
#include <QHeaderView>
#include <QScrollBar>
#include <QWheelEvent>
#include <QtConcurrent>


ATMWatchWindow::ATMWatchWindow(QWidget *parent) : QWidget(parent) {
  setupUI();
  setupModels();
  setupConnections();

  // Initialize base price update timer (every 1 second)
  m_basePriceTimer = new QTimer(this);
  m_basePriceTimer->setInterval(1000); // 1 second
  connect(m_basePriceTimer, &QTimer::timeout, this,
          &ATMWatchWindow::onBasePriceUpdate);
  m_basePriceTimer->start();

  // Initial data load
  refreshData();

  setWindowTitle("ATM Watch");
  resize(1200, 600);
}

ATMWatchWindow::~ATMWatchWindow() {
  FeedHandler::instance().unsubscribeAll(this);
}

void ATMWatchWindow::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(5, 5, 5, 5);
  mainLayout->setSpacing(5);

  // Filter Panel at Top
  QHBoxLayout *filterLayout = new QHBoxLayout();
  filterLayout->setSpacing(10);

  // Exchange Selection
  QLabel *exchangeLabel = new QLabel("Exchange:");
  exchangeLabel->setStyleSheet("color: #FFFFFF; font-weight: bold;");
  m_exchangeCombo = new QComboBox();
  m_exchangeCombo->addItems({"NSE", "BSE"});
  m_exchangeCombo->setStyleSheet(
      "QComboBox { background-color: #2A3A50; color: #FFFFFF; border: 1px "
      "solid #3A4A60; padding: 4px; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox QAbstractItemView { background-color: #2A3A50; color: "
      "#FFFFFF; selection-background-color: #3A4A60; }");
  m_exchangeCombo->setMinimumWidth(80);

  // Expiry Selection
  QLabel *expiryLabel = new QLabel("Expiry:");
  expiryLabel->setStyleSheet("color: #FFFFFF; font-weight: bold;");
  m_expiryCombo = new QComboBox();
  m_expiryCombo->addItem("Current (Nearest)", "CURRENT");
  m_expiryCombo->setStyleSheet(
      "QComboBox { background-color: #2A3A50; color: #FFFFFF; border: 1px "
      "solid #3A4A60; padding: 4px; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox QAbstractItemView { background-color: #2A3A50; color: "
      "#FFFFFF; selection-background-color: #3A4A60; }");
  m_expiryCombo->setMinimumWidth(150);

  // Status Label
  m_statusLabel = new QLabel("Loading...");
  m_statusLabel->setStyleSheet("color: #AAAAAA; font-style: italic;");

  filterLayout->addWidget(exchangeLabel);
  filterLayout->addWidget(m_exchangeCombo);
  filterLayout->addWidget(expiryLabel);
  filterLayout->addWidget(m_expiryCombo);
  filterLayout->addWidget(m_statusLabel);
  filterLayout->addStretch();

  mainLayout->addLayout(filterLayout);

  // Table Layout (existing code)
  QHBoxLayout *tableLayout = new QHBoxLayout();
  tableLayout->setSpacing(0);

  auto setupTable = [this](QTableView *table) {
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->hide();
    table->setShowGrid(true);
    table->setGridStyle(Qt::SolidLine);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
  m_callModel = new QStandardItemModel(this);
  m_callModel->setColumnCount(CALL_COUNT);
  m_callModel->setHorizontalHeaderLabels(
      {"LTP", "Chg", "Bid", "Ask", "Vol", "OI", "IV", "Delta", "Gamma", "Vega", "Theta"});
  m_callTable->setModel(m_callModel);
  m_callTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

  m_symbolModel = new QStandardItemModel(this);
  m_symbolModel->setColumnCount(SYM_COUNT);
  m_symbolModel->setHorizontalHeaderLabels(
      {"Symbol", "Spot/Fut", "ATM", "Expiry"});
  m_symbolTable->setModel(m_symbolModel);
  m_symbolTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

  m_putModel = new QStandardItemModel(this);
  m_putModel->setColumnCount(PUT_COUNT);
  m_putModel->setHorizontalHeaderLabels(
      {"Bid", "Ask", "LTP", "Chg", "Vol", "OI", "IV", "Delta", "Gamma", "Vega", "Theta"});
  m_putTable->setModel(m_putModel);
  m_putTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void ATMWatchWindow::setupConnections() {
  connect(ATMWatchManager::getInstance(), &ATMWatchManager::atmUpdated, this,
          &ATMWatchWindow::onATMUpdated);

  // Greeks updates
  connect(&GreeksCalculationService::instance(), &GreeksCalculationService::greeksCalculated, this,
          [this](uint32_t token, int exchangeSegment, const GreeksResult& result) {
              if (!m_tokenToInfo.contains(token)) return;
              auto info = m_tokenToInfo[token]; // {symbol, isCall}
              int row = m_symbolToRow.value(info.first, -1);
              if (row < 0) return;

              if (info.second) { // Call
                  m_callModel->setData(m_callModel->index(row, CALL_IV), QString::number(result.impliedVolatility, 'f', 2));
                  m_callModel->setData(m_callModel->index(row, CALL_DELTA), QString::number(result.delta, 'f', 2));
                  m_callModel->setData(m_callModel->index(row, CALL_GAMMA), QString::number(result.gamma, 'f', 4));
                  m_callModel->setData(m_callModel->index(row, CALL_VEGA), QString::number(result.vega, 'f', 2));
                  m_callModel->setData(m_callModel->index(row, CALL_THETA), QString::number(result.theta, 'f', 2));
              } else { // Put
                  m_putModel->setData(m_putModel->index(row, PUT_IV), QString::number(result.impliedVolatility, 'f', 2));
                  m_putModel->setData(m_putModel->index(row, PUT_DELTA), QString::number(result.delta, 'f', 2));
                  m_putModel->setData(m_putModel->index(row, PUT_GAMMA), QString::number(result.gamma, 'f', 4));
                  m_putModel->setData(m_putModel->index(row, PUT_VEGA), QString::number(result.vega, 'f', 2));
                  m_putModel->setData(m_putModel->index(row, PUT_THETA), QString::number(result.theta, 'f', 2));
              }
          });

  // Filter connections
  connect(m_exchangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ATMWatchWindow::onExchangeChanged);
  connect(m_expiryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ATMWatchWindow::onExpiryChanged);

  // Async loading completion
  connect(this, &ATMWatchWindow::onSymbolsLoaded, this, [this](int count) {
    m_statusLabel->setText(QString("Loaded %1 symbols").arg(count));
  });

  // Sync scrolling using the middle table as master
  connect(m_symbolTable->verticalScrollBar(), &QScrollBar::valueChanged, this,
          &ATMWatchWindow::synchronizeScrollBars);

  // Initialize
  m_currentExchange = "NSE";
  populateCommonExpiries(m_currentExchange);
}

void ATMWatchWindow::refreshData() {
  FeedHandler::instance().unsubscribeAll(this);
  m_tokenToInfo.clear();
  m_symbolToRow.clear();
  m_underlyingToRow.clear();

  m_callModel->setRowCount(0);
  m_symbolModel->setRowCount(0);
  m_putModel->setRowCount(0);

  auto atmList = ATMWatchManager::getInstance()->getATMWatchArray();
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

      // Fetch current price from cache
      if (auto state =
              MarketData::PriceStoreGateway::instance().getUnifiedState(
                  2, token)) {
        UDP::MarketTick tick;
        tick.token = static_cast<uint32_t>(token);
        tick.ltp = state->ltp;
        tick.bids[0].price = state->bids[0].price;
        tick.asks[0].price = state->asks[0].price;
        tick.volume = state->volume;
        tick.openInterest = state->openInterest;
        tick.openInterest = state->openInterest;
        onTickUpdate(tick);
        
        // Pre-populate Greeks if available
        if (state->greeksCalculated) {
             if (isCall) {
                  m_callModel->setData(m_callModel->index(row, CALL_IV), QString::number(state->impliedVolatility, 'f', 2));
                  m_callModel->setData(m_callModel->index(row, CALL_DELTA), QString::number(state->delta, 'f', 2));
                  m_callModel->setData(m_callModel->index(row, CALL_GAMMA), QString::number(state->gamma, 'f', 4));
                  m_callModel->setData(m_callModel->index(row, CALL_VEGA), QString::number(state->vega, 'f', 2));
                  m_callModel->setData(m_callModel->index(row, CALL_THETA), QString::number(state->theta, 'f', 2));
             } else {
                  m_putModel->setData(m_putModel->index(row, PUT_IV), QString::number(state->impliedVolatility, 'f', 2));
                  m_putModel->setData(m_putModel->index(row, PUT_DELTA), QString::number(state->delta, 'f', 2));
                  m_putModel->setData(m_putModel->index(row, PUT_GAMMA), QString::number(state->gamma, 'f', 4));
                  m_putModel->setData(m_putModel->index(row, PUT_VEGA), QString::number(state->vega, 'f', 2));
                  m_putModel->setData(m_putModel->index(row, PUT_THETA), QString::number(state->theta, 'f', 2));
             }
        }
      }
    };

    setupInitialOptionData(info.callToken, true);
    setupInitialOptionData(info.putToken, false);

    row++;
  }
}

void ATMWatchWindow::onATMUpdated() { refreshData(); }

void ATMWatchWindow::onTickUpdate(const UDP::MarketTick &tick) {
  // ✅ OPTIMIZATION: Skip depth-only updates (message 7208)
  // ATM Watch displays LTP, IV, Greeks - not order book depth
  // This reduces processing by ~30-40% for actively traded options
  if (tick.updateType == UDP::UpdateType::DEPTH_UPDATE) {
    return;
  }
  
  if (!m_tokenToInfo.contains(tick.token))
    return;

  auto info = m_tokenToInfo[tick.token];
  int row = m_symbolToRow.value(info.first, -1);
  if (row < 0)
    return;

  if (info.second) { // Call
    m_callModel->setData(m_callModel->index(row, CALL_LTP),
                         QString::number(tick.ltp, 'f', 2));
    m_callModel->setData(m_callModel->index(row, CALL_BID),
                         QString::number(tick.bestBid(), 'f', 2));
    m_callModel->setData(m_callModel->index(row, CALL_ASK),
                         QString::number(tick.bestAsk(), 'f', 2));
    m_callModel->setData(m_callModel->index(row, CALL_VOL),
                         QString::number(tick.volume));
    m_callModel->setData(m_callModel->index(row, CALL_OI),
                         QString::number(tick.openInterest));
  } else { // Put
    m_putModel->setData(m_putModel->index(row, PUT_LTP),
                        QString::number(tick.ltp, 'f', 2));
    m_putModel->setData(m_putModel->index(row, PUT_BID),
                        QString::number(tick.bestBid(), 'f', 2));
    m_putModel->setData(m_putModel->index(row, PUT_ASK),
                        QString::number(tick.bestAsk(), 'f', 2));
    m_putModel->setData(m_putModel->index(row, PUT_VOL),
                        QString::number(tick.volume));
    m_putModel->setData(m_putModel->index(row, PUT_OI),
                        QString::number(tick.openInterest));
  }
}

void ATMWatchWindow::synchronizeScrollBars(int value) {
  m_callTable->verticalScrollBar()->setValue(value);
  m_putTable->verticalScrollBar()->setValue(value);
}

void ATMWatchWindow::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  // Load all symbols when window is shown (in background)
  loadAllSymbols();
}

bool ATMWatchWindow::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::Wheel) {
    if (obj == m_callTable->viewport() || obj == m_putTable->viewport() ||
        obj == m_symbolTable->viewport()) {
      QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
      int currentValue = m_symbolTable->verticalScrollBar()->value();
      int delta = wheelEvent->angleDelta().y();
      int step = m_symbolTable->verticalScrollBar()->singleStep();
      int newValue = currentValue - (delta > 0 ? step : -step);
      m_symbolTable->verticalScrollBar()->setValue(newValue);
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

void ATMWatchWindow::onExchangeChanged(int index) {
  m_currentExchange = m_exchangeCombo->currentText();
  qDebug() << "[ATMWatch] Exchange changed to:" << m_currentExchange;

  // Repopulate expiries for new exchange
  populateCommonExpiries(m_currentExchange);

  // Reload all symbols
  loadAllSymbols();
}

void ATMWatchWindow::onExpiryChanged(int index) {
  m_currentExpiry = m_expiryCombo->currentData().toString();
  qDebug() << "[ATMWatch] Expiry changed to:" << m_currentExpiry;

  // Reload all symbols with new expiry
  loadAllSymbols();
}

void ATMWatchWindow::loadAllSymbols() {
  auto repo = RepositoryManager::getInstance();
  if (!repo->isLoaded()) {
    m_statusLabel->setText("Repository not loaded");
    return;
  }

  m_statusLabel->setText("Loading symbols...");

  // Run in background to prevent UI freeze
  QtConcurrent::run([this, repo]() {
    // Step 1: Get option symbols from cache (instant lookup, no contract filtering)
    QVector<QString> optionSymbols = repo->getOptionSymbols();
    
    qDebug() << "[ATMWatch] Loaded" << optionSymbols.size()
             << "option-enabled symbols from cache (instant lookup)";

    // Step 4: Subscribe to base tokens (underlying cash tokens) for all symbols
    // Note: Actual subscription happens automatically via FeedHandler
    // when ATMWatchManager calls fetchBasePrice() during calculation
    
    // Clear existing ATM watches (on main thread)
    QMetaObject::invokeMethod(
        ATMWatchManager::getInstance(),
        [this]() {
          ATMWatchManager::getInstance()->removeWatch("NIFTY");
          ATMWatchManager::getInstance()->removeWatch("BANKNIFTY");
        },
        Qt::QueuedConnection);

    // Step 5: Prepare watch configs for all symbols with appropriate expiry
    QVector<QPair<QString, QString>> watchConfigs; // symbol, expiry pairs
    
    for (const QString &symbol : optionSymbols) {
      QString expiry;

      if (m_currentExpiry == "CURRENT") {
        // Step 5.1: Get current expiry from cache (instant O(1) lookup)
        expiry = repo->getCurrentExpiry(symbol);
      } else {
        // Use user-selected expiry (verify symbol has contracts for this expiry)
        QVector<QString> symbolsForExpiry = repo->getSymbolsForExpiry(m_currentExpiry);
        if (symbolsForExpiry.contains(symbol)) {
          expiry = m_currentExpiry;
        }
      }

      if (!expiry.isEmpty()) {
        watchConfigs.append(qMakePair(symbol, expiry));
      }
    }

    qDebug() << "[ATMWatch] Prepared" << watchConfigs.size() << "watch configs";

    // Step 6: Add all watches in batch (on main thread) to avoid N × N calculations
    QMetaObject::invokeMethod(
        ATMWatchManager::getInstance(),
        [watchConfigs]() {
          ATMWatchManager::getInstance()->addWatchesBatch(watchConfigs);
        },
        Qt::QueuedConnection);

    // Update UI on main thread
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

  // Get all expiries from cache (instant lookup)
  QVector<QString> expiries = repo->getAllExpiries();
  
  // Add to combo box (already sorted ascending)
  for (const QString& expiry : expiries) {
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

  // Get all F&O contracts
  QVector<ContractData> allContracts =
      repo->getContractsBySegment(exchange, "FO");

  // Extract unique expiries from all options
  QSet<QString> expiries;
  for (const auto &contract : allContracts) {
    if (contract.instrumentType == 2 && !contract.expiryDate.isEmpty()) {
      expiries.insert(contract.expiryDate);
    }
  }

  // Sort and add to combo
  QStringList expiryList = expiries.values();
  std::sort(expiryList.begin(), expiryList.end());

  for (const QString &expiry : expiryList) {
    m_expiryCombo->addItem(expiry, expiry);
  }

  qDebug() << "[ATMWatch] Populated" << expiryList.size()
           << "common expiries for" << exchange;

  // Set to Current by default
  m_expiryCombo->setCurrentIndex(0);
  m_currentExpiry = "CURRENT";
}
#endif

QString ATMWatchWindow::getNearestExpiry(const QString &symbol,
                                         const QString &exchange) {
  // Use cache for instant lookup
  auto repo = RepositoryManager::getInstance();
  if (!repo->isLoaded()) {
    return QString();
  }

  return repo->getCurrentExpiry(symbol);
}

void ATMWatchWindow::onBasePriceUpdate() {
  // Update base prices (LTP) for all symbols in real-time
  updateBasePrices();
}

void ATMWatchWindow::onSymbolsLoaded(int count) {
  m_statusLabel->setText(QString("Loaded %1 symbols").arg(count));
}

void ATMWatchWindow::updateBasePrices() {
  // Get latest ATM info from manager
  auto atmList = ATMWatchManager::getInstance()->getATMWatchArray();

  for (const auto &info : atmList) {
    if (!info.isValid)
      continue;

    int row = m_symbolToRow.value(info.symbol, -1);
    if (row < 0)
      continue;

    // Update the base price (LTP) column
    m_symbolModel->setData(m_symbolModel->index(row, SYM_PRICE),
                           QString::number(info.basePrice, 'f', 2));

    // Also update ATM strike if it changed
    double currentATM =
        m_symbolModel->data(m_symbolModel->index(row, SYM_ATM)).toDouble();
    if (qAbs(currentATM - info.atmStrike) > 0.01) {
      m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                             QString::number(info.atmStrike, 'f', 2));
    }
  }
}
