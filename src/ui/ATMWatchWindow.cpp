#include "ui/ATMWatchWindow.h"
#include "data/PriceStoreGateway.h"
#include "models/WindowContext.h"
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include "services/GreeksCalculationService.h"
#include "ui/ATMWatchSettingsDialog.h"
#include "ui/OptionChainWindow.h"
#include "views/BuyWindow.h"
#include "views/SellWindow.h"
#include "views/SnapQuoteWindow.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDate>
#include <QDebug>
#include <QEvent>
#include <QHeaderView>
#include <QMenu>
#include <QScrollBar>
#include <QStandardItem>
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

  // Toolbar at Top
  m_toolbar = new QToolBar(this);
  m_toolbar->setStyleSheet(
      "QToolBar { background-color: #f8fafc; border: 1px solid #e2e8f0; "
      "padding: 2px; }"
      "QToolButton { background-color: #f1f5f9; color: #334155; border: 1px "
      "solid #cbd5e1; "
      "padding: 4px 8px; margin: 2px; border-radius: 4px; }"
      "QToolButton:hover { background-color: #e2e8f0; color: #0f172a; }"
      "QToolButton:pressed { background-color: #dbeafe; border-color: #3b82f6; }");

  QAction *settingsAction = m_toolbar->addAction("⚙ Settings");
  connect(settingsAction, &QAction::triggered, this,
          &ATMWatchWindow::onSettingsClicked);

  QAction *refreshAction = m_toolbar->addAction("🔄 Refresh");
  connect(refreshAction, &QAction::triggered, this,
          &ATMWatchWindow::refreshData);

  mainLayout->addWidget(m_toolbar);

  // Filter Panel at Top
  QHBoxLayout *filterLayout = new QHBoxLayout();
  filterLayout->setSpacing(10);

  // Exchange Selection
  QLabel *exchangeLabel = new QLabel("Exchange:");
  exchangeLabel->setStyleSheet("color: #475569; font-weight: bold;");
  m_exchangeCombo = new QComboBox();
  m_exchangeCombo->addItems({"NSE", "BSE"});
  m_exchangeCombo->setStyleSheet(
      "QComboBox { background-color: #ffffff; color: #0f172a; border: 1px "
      "solid #cbd5e1; padding: 4px; border-radius: 4px; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox QAbstractItemView { background-color: #ffffff; color: "
      "#0f172a; selection-background-color: #dbeafe; selection-color: #1e40af; }");
  m_exchangeCombo->setMinimumWidth(80);

  // Expiry Selection
  QLabel *expiryLabel = new QLabel("Expiry:");
  expiryLabel->setStyleSheet("color: #475569; font-weight: bold;");
  m_expiryCombo = new QComboBox();
  m_expiryCombo->addItem("Current (Nearest)", "CURRENT");
  m_expiryCombo->setStyleSheet(
      "QComboBox { background-color: #ffffff; color: #0f172a; border: 1px "
      "solid #cbd5e1; padding: 4px; border-radius: 4px; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox QAbstractItemView { background-color: #ffffff; color: "
      "#0f172a; selection-background-color: #dbeafe; selection-color: #1e40af; }");
  m_expiryCombo->setMinimumWidth(150);

  // Status Label
  m_statusLabel = new QLabel("Loading...");
  m_statusLabel->setStyleSheet("color: #64748b; font-style: italic;");

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
        "QTableView { background-color: #ffffff; color: #1e293b; "
        "gridline-color: #f1f5f9; border: 1px solid #e2e8f0; }"
        "QTableView::item:selected { background: #dbeafe; color: #1e40af; }"
        "QHeaderView::section { background-color: #f8fafc; color: #475569; "
        "padding: 4px; border: none; border-bottom: 2px solid #e2e8f0; font-weight: bold; }");
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
                               "QTableView { color: #2563eb; }");

  tableLayout->addWidget(m_callTable, 2);
  tableLayout->addWidget(m_symbolTable, 1);
  tableLayout->addWidget(m_putTable, 2);

  // Enable sorting on Symbol table header
  m_symbolTable->horizontalHeader()->setSectionsClickable(true);
  m_symbolTable->horizontalHeader()->setSortIndicatorShown(true);
  connect(m_symbolTable->horizontalHeader(), &QHeaderView::sectionClicked, this,
          &ATMWatchWindow::onHeaderClicked);

  mainLayout->addLayout(tableLayout);
  setStyleSheet("QWidget { background-color: #ffffff; }");
}

void ATMWatchWindow::setupModels() {
  m_callModel = new QStandardItemModel(this);
  m_callModel->setColumnCount(CALL_COUNT);
  m_callModel->setHorizontalHeaderLabels({"Chg", "Vol", "OI", "IV", "Delta",
                                          "Gamma", "Vega", "Theta", "LTP",
                                          "Bid", "Ask"});
  m_callTable->setModel(m_callModel);
  m_callTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

  m_callDelegate = new ATMWatchDelegate(false, this);
  m_callTable->setItemDelegate(m_callDelegate);

  m_symbolModel = new QStandardItemModel(this);
  m_symbolModel->setColumnCount(SYM_COUNT);
  m_symbolModel->setHorizontalHeaderLabels(
      {"Symbol", "Spot/Fut", "ATM", "Expiry"});
  m_symbolTable->setModel(m_symbolModel);
  m_symbolTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

  m_symbolDelegate = new ATMWatchDelegate(true, this);
  m_symbolTable->setItemDelegate(m_symbolDelegate);

  m_putModel = new QStandardItemModel(this);
  m_putModel->setColumnCount(PUT_COUNT);
  m_putModel->setHorizontalHeaderLabels({"LTP", "Bid", "Ask", "Chg", "Vol",
                                         "OI", "IV", "Delta", "Gamma", "Vega",
                                         "Theta"});
  m_putTable->setModel(m_putModel);
  m_putTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

  m_putDelegate = new ATMWatchDelegate(false, this);
  m_putTable->setItemDelegate(m_putDelegate);
}

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
    // Keep color for a short duration? Or reset?
    // For now reset if strictly equal but was different
    if (newValue != oldValue)
      item->setData(0, Qt::UserRole + 1);
  }
}

void ATMWatchWindow::setupConnections() {
  connect(&ATMWatchManager::getInstance(), &ATMWatchManager::atmUpdated, this,
          &ATMWatchWindow::onATMUpdated);

  // Greeks updates
  connect(
      &GreeksCalculationService::instance(),
      &GreeksCalculationService::greeksCalculated, this,
      [this](uint32_t token, int exchangeSegment, const GreeksResult &result) {
        if (!m_tokenToInfo.contains(token))
          return;
        auto info = m_tokenToInfo[token]; // {symbol, isCall}
        int row = m_symbolToRow.value(info.first, -1);
        if (row < 0)
          return;

        if (info.second) { // Call
          m_callModel->setData(
              m_callModel->index(row, CALL_IV),
              QString::number(result.impliedVolatility, 'f', 2));
          m_callModel->setData(m_callModel->index(row, CALL_DELTA),
                               QString::number(result.delta, 'f', 2));
          m_callModel->setData(m_callModel->index(row, CALL_GAMMA),
                               QString::number(result.gamma, 'f', 4));
          m_callModel->setData(m_callModel->index(row, CALL_VEGA),
                               QString::number(result.vega, 'f', 2));
          m_callModel->setData(m_callModel->index(row, CALL_THETA),
                               QString::number(result.theta, 'f', 2));
        } else { // Put
          m_putModel->setData(
              m_putModel->index(row, PUT_IV),
              QString::number(result.impliedVolatility, 'f', 2));
          m_putModel->setData(m_putModel->index(row, PUT_DELTA),
                              QString::number(result.delta, 'f', 2));
          m_putModel->setData(m_putModel->index(row, PUT_GAMMA),
                              QString::number(result.gamma, 'f', 4));
          m_putModel->setData(m_putModel->index(row, PUT_VEGA),
                              QString::number(result.vega, 'f', 2));
          m_putModel->setData(m_putModel->index(row, PUT_THETA),
                              QString::number(result.theta, 'f', 2));
        }
      });

  // ATM calculation error handling
  connect(&ATMWatchManager::getInstance(), &ATMWatchManager::calculationFailed,
          this, [this](const QString &symbol, const QString &errorMessage) {
            int row = m_symbolToRow.value(symbol, -1);
            if (row >= 0) {
              // Show error in ATM strike column
              m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                                     "ERROR");
              m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                                     errorMessage, Qt::ToolTipRole);

              // Set color to red
              m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                                     QColor(Qt::red), Qt::ForegroundRole);
            }
          });

  // Filter connections
  connect(m_exchangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ATMWatchWindow::onExchangeChanged);
  connect(m_expiryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ATMWatchWindow::onExpiryChanged);

  // Context menu and double-click for symbol table
  m_symbolTable->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_symbolTable, &QTableView::customContextMenuRequested, this,
          &ATMWatchWindow::onShowContextMenu);
  connect(m_symbolTable, &QTableView::doubleClicked, this,
          &ATMWatchWindow::onSymbolDoubleClicked);

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

  // Exclusive Selection Logic
  // When one table is selected, clear selection in others to avoid context
  // ambiguity
  connect(m_callTable->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, [this](const QItemSelection &selected, const QItemSelection &) {
            if (!selected.isEmpty()) {
              m_putTable->clearSelection();
              m_symbolTable->clearSelection();
            }
          });

  connect(m_putTable->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, [this](const QItemSelection &selected, const QItemSelection &) {
            if (!selected.isEmpty()) {
              m_callTable->clearSelection();
              m_symbolTable->clearSelection();
            }
          });

  connect(m_symbolTable->selectionModel(),
          &QItemSelectionModel::selectionChanged, this,
          [this](const QItemSelection &selected, const QItemSelection &) {
            if (!selected.isEmpty()) {
              m_callTable->clearSelection();
              m_putTable->clearSelection();
            }
          });
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

      // Fetch current price from cache
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
                QString::number(state.impliedVolatility, 'f', 2));
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
                QString::number(state.impliedVolatility, 'f', 2));
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
      // Try subscribing to segment 2 as well if it's a future, or just rely on
      // feed handler to handle segment
      feed.subscribe(2, info.underlyingToken, this,
                     &ATMWatchWindow::onTickUpdate);
    }

    row++;
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

    // Check if symbol still exists in new data
    if (!newATMData.contains(symbol)) {
      // Symbol removed - unsubscribe and mark for deletion
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

    // Step 1a: Update symbol table if data changed
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

    // Step 1b: Handle token changes (ATM strike moved)
    if (atmStrikeChanged && m_previousATMData.contains(symbol)) {
      // Unsubscribe from old tokens
      if (oldInfo.callToken > 0) {
        feed.unsubscribe(2, oldInfo.callToken, this);
        m_tokenToInfo.remove(oldInfo.callToken);
      }
      if (oldInfo.putToken > 0) {
        feed.unsubscribe(2, oldInfo.putToken, this);
        m_tokenToInfo.remove(oldInfo.putToken);
      }

      // Subscribe to new tokens
      auto setupTokenSubscription = [&](int64_t token, bool isCall) {
        if (token <= 0)
          return;

        m_tokenToInfo[token] = {symbol, isCall};
        feed.subscribe(2, token, this, &ATMWatchWindow::onTickUpdate);

        // Fetch current price from cache
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

          // Update Greeks if available
          if (state.greeksCalculated) {
            auto model = isCall ? m_callModel : m_putModel;
            int colIV = isCall ? CALL_IV : PUT_IV;
            int colDelta = isCall ? CALL_DELTA : PUT_DELTA;
            int colGamma = isCall ? CALL_GAMMA : PUT_GAMMA;
            int colVega = isCall ? CALL_VEGA : PUT_VEGA;
            int colTheta = isCall ? CALL_THETA : PUT_THETA;

            model->setData(model->index(row, colIV),
                           QString::number(state.impliedVolatility, 'f', 2));
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

    // Step 1c: Handle Underlying Token changes (e.g. Source switched Cash <->
    // Future)
    bool underlyingChanged = !m_previousATMData.contains(symbol) ||
                             oldInfo.underlyingToken != newInfo.underlyingToken;

    if (underlyingChanged && m_previousATMData.contains(symbol)) {
      if (oldInfo.underlyingToken > 0) {
        m_underlyingTokenToSymbol.remove(oldInfo.underlyingToken);
        feed.unsubscribe(1, oldInfo.underlyingToken, this);
        feed.unsubscribe(2, oldInfo.underlyingToken, this);
      }
      // New token subscription handled in Step 4
    }
  }

  // Step 2: Add new symbols (not in previous data)
  for (const auto &newInfo : atmList) {
    if (!newInfo.isValid)
      continue;

    if (!m_symbolToRow.contains(newInfo.symbol)) {
      // New symbol - add row using existing refreshData logic
      int row = m_symbolModel->rowCount();

      m_callModel->insertRow(row);
      m_symbolModel->insertRow(row);
      m_putModel->insertRow(row);

      m_symbolToRow[newInfo.symbol] = row;

      // Populate Symbol Table
      m_symbolModel->setData(m_symbolModel->index(row, SYM_NAME),
                             newInfo.symbol);
      updateItemWithColor(m_symbolModel, row, SYM_PRICE, newInfo.basePrice);
      m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                             QString::number(newInfo.atmStrike, 'f', 2));
      m_symbolModel->setData(m_symbolModel->index(row, SYM_EXPIRY),
                             newInfo.expiry);

      // Subscribe to tokens
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
                           QString::number(state.impliedVolatility, 'f', 2));
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
  // Instead of incremental diffs, we calculate the desired state and enforce
  // it.

  // A. Build desired map from current ATM list
  QHash<int64_t, QString> desiredMap;
  for (const auto &info : atmList) {
    if (info.isValid && info.underlyingToken > 0) {
      desiredMap[info.underlyingToken] = info.symbol;
    }
  }

  // B. Identify stale subscriptions (in current map but not in desired)
  QVector<int64_t> tokensToRemove;
  for (auto it = m_underlyingTokenToSymbol.begin();
       it != m_underlyingTokenToSymbol.end(); ++it) {
    int64_t token = it.key();
    if (!desiredMap.contains(token)) {
      tokensToRemove.append(token);
    }
  }

  // C. Unsubscribe stale tokens
  for (int64_t token : tokensToRemove) {
    m_underlyingTokenToSymbol.remove(token);
    feed.unsubscribe(1, token, this);
    feed.unsubscribe(2, token, this);
  }

  // D. Subscribe new tokens (in desired map but not in current)
  for (auto it = desiredMap.begin(); it != desiredMap.end(); ++it) {
    int64_t token = it.key();
    QString symbol = it.value();

    if (!m_underlyingTokenToSymbol.contains(token)) {
      m_underlyingTokenToSymbol[token] = symbol;
      feed.subscribe(1, token, this, &ATMWatchWindow::onTickUpdate);
      feed.subscribe(2, token, this, &ATMWatchWindow::onTickUpdate);

      // Force an initial update if available
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

  // Step 5: Final sync of internal state
  m_previousATMData = newATMData;
}

void ATMWatchWindow::onATMUpdated() {
  // P2: Use incremental updates to avoid flicker
  updateDataIncrementally();
}

void ATMWatchWindow::onTickUpdate(const UDP::MarketTick &tick) {
  // ✅ OPTIMIZATION: Skip depth-only updates (message 7208)
  // ATM Watch displays LTP, IV, Greeks - not order book depth
  // This reduces processing by ~30-40% for actively traded options
  if (tick.updateType == UDP::UpdateType::DEPTH_UPDATE) {
    return;
  }

  // Find info for this token
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

    // Throttle UI updates (limit to 10fps per row)
    // using QElapsedTimer/QTime checks if needed
    // For now, direct update

    bool isCall = info.second;
    auto model = isCall ? m_callModel : m_putModel;
    int colPrice = isCall ? CALL_LTP : PUT_LTP;

    // Update LTP
    updateItemWithColor(model, row, colPrice, tick.ltp);

    // Update other fields
    if (isCall) { // Call
      updateItemWithColor(m_callModel, row, CALL_BID, tick.bestBid());
      updateItemWithColor(m_callModel, row, CALL_ASK, tick.bestAsk());
      m_callModel->setData(m_callModel->index(row, CALL_VOL),
                           QString::number(tick.volume));
      m_callModel->setData(m_callModel->index(row, CALL_OI),
                           QString::number(tick.openInterest));
    } else { // Put
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
    // Step 1: Get option symbols from cache (instant lookup, no contract
    // filtering)
    QVector<QString> optionSymbols = repo->getOptionSymbols();

    qDebug() << "[ATMWatch] Loaded" << optionSymbols.size()
             << "option-enabled symbols from cache (instant lookup)";

    // Step 4: Subscribe to base tokens (underlying cash tokens) for all symbols
    // Note: Actual subscription happens automatically via FeedHandler
    // when ATMWatchManager calls fetchBasePrice() during calculation

    // Clear ALL ATM watches (on main thread)
    QMetaObject::invokeMethod(
        &ATMWatchManager::getInstance(),
        []() { ATMWatchManager::getInstance().clearAllWatches(); },
        Qt::QueuedConnection);

    // Step 5: Prepare watch configs for all symbols with appropriate expiry
    QVector<QPair<QString, QString>> watchConfigs; // symbol, expiry pairs

    for (const QString &symbol : optionSymbols) {
      QString expiry;

      if (m_currentExpiry == "CURRENT") {
        // Step 5.1: Get current expiry from cache (instant O(1) lookup)
        expiry = repo->getCurrentExpiry(symbol);
      } else {
        // Use user-selected expiry (verify symbol has contracts for this
        // expiry)
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

    // Step 6: Add all watches in batch (on main thread)
    QMetaObject::invokeMethod(
        &ATMWatchManager::getInstance(),
        [watchConfigs]() {
          ATMWatchManager::getInstance().addWatchesBatch(watchConfigs);
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
  auto atmList = ATMWatchManager::getInstance().getATMWatchArray();

  for (const auto &info : atmList) {
    if (!info.isValid)
      continue;

    int row = m_symbolToRow.value(info.symbol, -1);
    if (row >= 0 && row < m_symbolModel->rowCount()) {
      // Update the base price (LTP) column
      updateItemWithColor(m_symbolModel, row, SYM_PRICE, info.basePrice);

      // Also update ATM strike if it changed
      double currentATM =
          m_symbolModel->data(m_symbolModel->index(row, SYM_ATM)).toDouble();
      if (qAbs(currentATM - info.atmStrike) > 0.01) {
        m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                               QString::number(info.atmStrike, 'f', 2));
      }
    }
  }
}

void ATMWatchWindow::onSettingsClicked() {
  ATMWatchSettingsDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    // Settings applied, refresh ATM data on next calculation
    qInfo() << "[ATMWatch] Settings updated, will take effect on next ATM "
               "calculation";
  }
}

void ATMWatchWindow::onShowContextMenu(const QPoint &pos) {
  QModelIndex index = m_symbolTable->indexAt(pos);
  if (!index.isValid()) {
    return;
  }

  int row = index.row();
  QString symbol =
      m_symbolModel->data(m_symbolModel->index(row, SYM_NAME)).toString();
  QString expiry =
      m_symbolModel->data(m_symbolModel->index(row, SYM_EXPIRY)).toString();

  QMenu contextMenu(this);
  contextMenu.setStyleSheet(
      "QMenu { background-color: #ffffff; color: #1e293b; border: 1px solid "
      "#e2e8f0; border-radius: 4px; }"
      "QMenu::item { padding: 6px 16px; }"
      "QMenu::item:selected { background-color: #dbeafe; color: #1e40af; }");

  QAction *openChainAction = contextMenu.addAction("📊 Open Option Chain");
  QAction *recalculateAction = contextMenu.addAction("🔄 Recalculate ATM");
  contextMenu.addSeparator();
  QAction *copySymbolAction = contextMenu.addAction("📋 Copy Symbol");

  QAction *selectedAction =
      contextMenu.exec(m_symbolTable->viewport()->mapToGlobal(pos));

  if (selectedAction == openChainAction) {
    openOptionChain(symbol, expiry);
  } else if (selectedAction == recalculateAction) {
    // Recalculate ATM for all symbols
    ATMWatchManager::getInstance().calculateAll();
    m_statusLabel->setText("Recalculating ATM...");
  } else if (selectedAction == copySymbolAction) {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(symbol);
    m_statusLabel->setText(QString("Copied: %1").arg(symbol));
  }
}

void ATMWatchWindow::onSymbolDoubleClicked(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }

  int row = index.row();
  QString symbol =
      m_symbolModel->data(m_symbolModel->index(row, SYM_NAME)).toString();
  QString expiry =
      m_symbolModel->data(m_symbolModel->index(row, SYM_EXPIRY)).toString();

  openOptionChain(symbol, expiry);
}

void ATMWatchWindow::openOptionChain(const QString &symbol,
                                     const QString &expiry) {
  qInfo() << "[ATMWatch] Opening Option Chain for" << symbol
          << "expiry:" << expiry;

  // Create and show option chain window
  OptionChainWindow *chainWindow = new OptionChainWindow();
  chainWindow->setAttribute(Qt::WA_DeleteOnClose); // Auto-delete when closed
  chainWindow->setWindowTitle(
      QString("Option Chain - %1 (%2)").arg(symbol, expiry));
  chainWindow->setSymbol(symbol, expiry);

  // Get ATM strike from current data
  int row = m_symbolToRow.value(symbol, -1);
  if (row >= 0) {
    double atmStrike =
        m_symbolModel->data(m_symbolModel->index(row, SYM_ATM)).toDouble();
    chainWindow->setATMStrike(atmStrike);
  }

  chainWindow->show();
  chainWindow->raise();
  chainWindow->activateWindow();

  m_statusLabel->setText(QString("Opened Option Chain for %1").arg(symbol));
}

void ATMWatchWindow::onHeaderClicked(int logicalIndex) {
  // Only sort on click of Symbol table columns
  if (logicalIndex == m_sortColumn) {
    // Toggle order
    m_sortOrder = (m_sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder
                                                      : Qt::AscendingOrder;
  } else {
    // New column, default to Ascending
    m_sortColumn = logicalIndex;
    m_sortOrder = Qt::AscendingOrder;
  }

  m_symbolTable->horizontalHeader()->setSortIndicator(m_sortColumn,
                                                      m_sortOrder);
  refreshData();
}

void ATMWatchWindow::sortATMList(QVector<ATMWatchManager::ATMInfo> &list) {
  if (list.isEmpty())
    return;

  std::sort(list.begin(), list.end(),
            [this](const ATMWatchManager::ATMInfo &a,
                   const ATMWatchManager::ATMInfo &b) {
              bool less = false;
              switch (m_sortColumn) {
              case SYM_NAME: // Symbol
                less = a.symbol < b.symbol;
                break;
              case SYM_PRICE: // Spot/Fut Price (Base Price)
                less = a.basePrice < b.basePrice;
                break;
              case SYM_ATM: // ATM Strike
                less = a.atmStrike < b.atmStrike;
                break;
              case SYM_EXPIRY: // Expiry
                less = a.expiry < b.expiry;
                break;
              default:
                less = a.symbol < b.symbol;
                break;
              }
              return (m_sortOrder == Qt::AscendingOrder) ? less : !less;
            });
}

WindowContext ATMWatchWindow::getCurrentContext() {
  WindowContext context;
  context.sourceWindow = "ATMWatch";

  QTableView *activeTable = nullptr;
  if (m_callTable->hasFocus())
    activeTable = m_callTable;
  else if (m_putTable->hasFocus())
    activeTable = m_putTable;
  else if (m_symbolTable->hasFocus())
    activeTable = m_symbolTable;

  // Fallback to table with selection
  if (!activeTable) {
    if (m_callTable->selectionModel()->hasSelection())
      activeTable = m_callTable;
    else if (m_putTable->selectionModel()->hasSelection())
      activeTable = m_putTable;
    else if (m_symbolTable->selectionModel()->hasSelection())
      activeTable = m_symbolTable;
  }

  if (!activeTable)
    return context;

  QModelIndex index = activeTable->currentIndex();
  if (!index.isValid())
    return context;

  int row = index.row();
  QString symbol =
      m_symbolModel->data(m_symbolModel->index(row, SYM_NAME)).toString();

  // Retrieve ATM Info from Manager to get Tokens
  auto atmList = ATMWatchManager::getInstance().getATMWatchArray();
  ATMWatchManager::ATMInfo info;
  bool found = false;
  for (const auto &item : atmList) {
    if (item.symbol == symbol) {
      info = item;
      found = true;
      break;
    }
  }

  if (!found)
    return context;

  int64_t token = 0;
  if (activeTable == m_callTable) {
    token = info.callToken;
  } else if (activeTable == m_putTable) {
    token = info.putToken;
  } else {
    // Symbol Table (Underlying)
    token = info.underlyingToken;

    // Fix: If viewing Index Spot (Cash), switch to Future for trading context
    // because Indices themselves are not tradeable
    bool isIndex = (symbol == "NIFTY" || symbol == "BANKNIFTY" ||
                    symbol == "FINNIFTY" || symbol == "MIDCPNIFTY");

    if (isIndex) {
      // Try to find Future token if current token looks like index (or checking
      // mostly symbol) We can always try to fetch Future token for the current
      // expiry
      auto repo = RepositoryManager::getInstance();
      int64_t futToken =
          repo->getFutureTokenForSymbolExpiry(symbol, info.expiry);
      if (futToken > 0) {
        token = futToken; // Switch to Future token
        qDebug() << "[ATMWatch] Context: Switched Index Spot to Future Token:"
                 << token;
      }
    }
  }

  if (token <= 0)
    return context;

  // Fetch full contract details
  auto repo = RepositoryManager::getInstance();
  const ContractData *contractPtr = nullptr;
  QString segment = "FO";

  // Try FO first
  contractPtr = repo->getContractByToken(m_currentExchange, "FO", token);

  // If not found, try CM
  if (!contractPtr) {
    contractPtr = repo->getContractByToken(m_currentExchange, "CM", token);
    segment = "CM";
  }
  if (!contractPtr)
    return context;

  const ContractData &contract = *contractPtr;

  qDebug() << "[ATMWatch] Context Resolved - Symbol:" << contract.name
           << "Series:" << contract.series << "Type:" << contract.instrumentType
           << "Token:" << token << "Segment:" << segment;

  context.exchange = m_currentExchange;
  context.token = token;
  context.symbol = contract.name; // Use underlying symbol name (e.g. NIFTY)
  context.displayName =
      contract.description; // Full description for UI title if needed
  context.segment = segment;
  context.series = contract.series;

  // Populate Option Details
  context.optionType = contract.optionType;
  context.expiry = contract.expiryDate;
  context.strikePrice = contract.strikePrice;
  context.lotSize = contract.lotSize;
  context.tickSize = contract.tickSize;

  // ✅ IMPROVED: Use series from contract if available, otherwise smart detect
  if (!contract.series.isEmpty()) {
    context.instrumentType = contract.series;
  } else {
    bool isIndex =
        (contract.name == "NIFTY" || contract.name == "BANKNIFTY" ||
         contract.name == "FINNIFTY" || contract.name == "MIDCPNIFTY");

    if (contract.instrumentType == 1) { // Future
      context.instrumentType = isIndex ? "FUTIDX" : "FUTSTK";
    } else if (contract.instrumentType == 2) { // Option
      context.instrumentType = isIndex ? "OPTIDX" : "OPTSTK";
    } else {
      context.instrumentType = "EQ";
    }
  }

  qDebug() << "[ATMWatch] Final InstrumentType:" << context.instrumentType;

  context.expiry = contract.expiryDate;
  context.strikePrice = contract.strikePrice;
  context.optionType = contract.optionType;
  context.lotSize = contract.lotSize;
  context.tickSize = contract.tickSize;
  context.freezeQty = contract.freezeQty;

  return context;
}

void ATMWatchWindow::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_F1 || event->key() == Qt::Key_F2 ||
      event->key() == Qt::Key_F6 || event->key() == Qt::Key_Delete) {
    WindowContext context = getCurrentContext();

    if (event->key() == Qt::Key_Delete) {
      if (context.isValid()) {
        ATMWatchManager::getInstance().removeWatch(context.symbol);
        return;
      }
    }

    if (!context.isValid()) {
      // Maybe show status "Select a valid contract"
      return;
    }

    if (event->key() == Qt::Key_F1) { // Buy
      BuyWindow::getInstance(context, this)->show();
    } else if (event->key() == Qt::Key_F2) { // Sell
      SellWindow::getInstance(context, this)->show();
    } else if (event->key() == Qt::Key_F6) { // SnapQuote
      auto w = new SnapQuoteWindow(context, this);
      w->setAttribute(Qt::WA_DeleteOnClose);
      w->show();
    }
    return;
  }

  QWidget::keyPressEvent(event);
}
