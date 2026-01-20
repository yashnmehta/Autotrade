#include "ui/OptionChainWindow.h"
#include <QDate>
#include <QDebug>
#include <QGroupBox>
#include <QHeaderView>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QWheelEvent>
#include <cmath>

#include "data/PriceStoreGateway.h"
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include "services/TokenSubscriptionManager.h"
#include "utils/MemoryProfiler.h"
#include <QTimer>

// ============================================================================
// OptionChainDelegate Implementation
// ============================================================================

OptionChainDelegate::OptionChainDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void OptionChainDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
  // For checkbox columns, use default rendering
  QString headerText =
      index.model()->headerData(index.column(), Qt::Horizontal).toString();
  if (headerText.isEmpty() && index.column() == 0) {
    // This is a checkbox column (empty header in first or last column)
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }
  if (headerText.isEmpty() &&
      index.column() == index.model()->columnCount() - 1) {
    // This is a checkbox column (empty header in first or last column)
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  painter->save();

  // Get the value and determine color
  QString text = index.data(Qt::DisplayRole).toString();
  QColor bgColor = QColor("#2A3A50"); // Default background
  QColor textColor = Qt::white;

  // Column-specific background colors

  // Check for dynamic tick update color first
  int direction = index.data(Qt::UserRole + 1).toInt();
  if (direction == 1) {
    bgColor = QColor("#0000FF"); // Blue for Up
  } else if (direction == 2) {
    bgColor = QColor("#FF0000"); // Red for Down
  } // else transparent
  else {
    bgColor = QColor("transparent");
  }

  // Check for numeric values indicating change
  bool isChangeColumn = false;

  if (headerText.contains("Chng") || headerText.contains("Change")) {
    isChangeColumn = true;
    bool ok;
    double value = text.toDouble(&ok);

    if (ok && value != 0.0) {
      if (value > 0) {
        textColor = QColor("#00FF00"); // Green for positive
      } else {
        textColor = QColor("#FF4444"); // Red for negative
      }
    }
  }

  // Draw background
  if (option.state & QStyle::State_Selected) {
    bgColor = QColor("#3A5A70"); // Highlighted selection
  }

  painter->fillRect(option.rect, bgColor);

  // Draw text
  painter->setPen(textColor);
  painter->drawText(option.rect, Qt::AlignCenter, text);

  painter->restore();
}

QSize OptionChainDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const {
  return QStyledItemDelegate::sizeHint(option, index);
}

// ============================================================================
// OptionChainWindow Implementation
// ============================================================================

OptionChainWindow::OptionChainWindow(QWidget *parent)
    : QWidget(parent), m_symbolCombo(nullptr), m_expiryCombo(nullptr),
      m_refreshButton(nullptr), m_calculatorButton(nullptr),
      m_titleLabel(nullptr), m_callTable(nullptr), m_strikeTable(nullptr),
      m_putTable(nullptr), m_callModel(nullptr), m_strikeModel(nullptr),
      m_putModel(nullptr), m_callDelegate(nullptr), m_putDelegate(nullptr),
      m_atmStrike(0.0), m_selectedCallRow(-1), m_selectedPutRow(-1) {
  setupUI();
  setupModels();
  setupConnections();

  // Populate symbols (silently, without triggering partial refreshes)
  populateSymbols();

  // One explicit refresh to load initial data
  refreshData();

  setWindowTitle("Option Chain");
  resize(1600, 800);
}

OptionChainWindow::~OptionChainWindow() {}

void OptionChainWindow::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(10, 10, 10, 10);
  mainLayout->setSpacing(10);

  // ========================================================================
  // Header Section
  // ========================================================================
  QHBoxLayout *headerLayout = new QHBoxLayout();
  headerLayout->setSpacing(10);

  // Title
  m_titleLabel = new QLabel("NIFTY");
  m_titleLabel->setStyleSheet(
      "QLabel { font-size: 18px; font-weight: bold; color: #FFFFFF; }");
  headerLayout->addWidget(m_titleLabel);

  headerLayout->addStretch();

  // Symbol selection
  QLabel *symbolLabel = new QLabel("Symbol:");
  symbolLabel->setStyleSheet("QLabel { color: #FFFFFF; }");
  headerLayout->addWidget(symbolLabel);

  m_symbolCombo = new QComboBox();
  m_symbolCombo->setMinimumWidth(120);
  m_symbolCombo->setStyleSheet(
      "QComboBox { background: #2A3A50; color: #FFFFFF; border: 1px solid "
      "#3A4A60; padding: 5px; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox::down-arrow { image: url(none); }");
  headerLayout->addWidget(m_symbolCombo);

  // Expiry selection
  QLabel *expiryLabel = new QLabel("Expiry:");
  expiryLabel->setStyleSheet("QLabel { color: #FFFFFF; }");
  headerLayout->addWidget(expiryLabel);

  m_expiryCombo = new QComboBox();
  m_expiryCombo->setMinimumWidth(120);
  m_expiryCombo->setStyleSheet(
      "QComboBox { background: #2A3A50; color: #FFFFFF; border: 1px solid "
      "#3A4A60; padding: 5px; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox::down-arrow { image: url(none); }");
  headerLayout->addWidget(m_expiryCombo);

  // Refresh button
  m_refreshButton = new QPushButton("Refresh");
  m_refreshButton->setStyleSheet(
      "QPushButton { background: #2A5A80; color: #FFFFFF; border: none; "
      "padding: 6px 15px; border-radius: 3px; }"
      "QPushButton:hover { background: #3A6A90; }"
      "QPushButton:pressed { background: #1A4A70; }");
  headerLayout->addWidget(m_refreshButton);

  // Calculator button
  m_calculatorButton = new QPushButton("View Calculators");
  m_calculatorButton->setStyleSheet(
      "QPushButton { background: #2A5A80; color: #FFFFFF; border: none; "
      "padding: 6px 15px; border-radius: 3px; }"
      "QPushButton:hover { background: #3A6A90; }"
      "QPushButton:pressed { background: #1A4A70; }");
  headerLayout->addWidget(m_calculatorButton);

  mainLayout->addLayout(headerLayout);

  // ========================================================================
  // Table Section - Horizontal Splitter
  // ========================================================================
  QHBoxLayout *tableLayout = new QHBoxLayout();
  tableLayout->setSpacing(0);
  tableLayout->setContentsMargins(0, 0, 0, 0);

  // Call Table (Left)
  m_callTable = new QTableView();
  m_callTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_callTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_callTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_callTable->verticalHeader()->hide();
  m_callTable->setAlternatingRowColors(false);
  m_callTable->setShowGrid(true);
  m_callTable->setGridStyle(Qt::SolidLine);
  m_callTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_callTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_callTable->viewport()->installEventFilter(this);
  m_callTable->setStyleSheet("QTableView {"
                             "   background-color: #1E2A38;"
                             "   color: #FFFFFF;"
                             "   gridline-color: #2A3A50;"
                             "   border: 1px solid #3A4A60;"
                             "}"
                             "QTableView::item {"
                             "   padding: 5px;"
                             "}"
                             "QHeaderView::section {"
                             "   background-color: #2A3A50;"
                             "   color: #FFFFFF;"
                             "   padding: 6px;"
                             "   border: 1px solid #3A4A60;"
                             "   font-weight: bold;"
                             "}");
  tableLayout->addWidget(m_callTable, 4);

  // Strike Table (Center)
  m_strikeTable = new QTableView();
  m_strikeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_strikeTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_strikeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_strikeTable->verticalHeader()->hide();
  m_strikeTable->setAlternatingRowColors(false);
  m_strikeTable->setShowGrid(true);
  m_strikeTable->setGridStyle(Qt::SolidLine);
  m_strikeTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_strikeTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_strikeTable->viewport()->installEventFilter(this);
  m_strikeTable->setStyleSheet("QTableView {"
                               "   background-color: #2A3A50;"
                               "   color: #FFFF00;"
                               "   gridline-color: #3A4A60;"
                               "   border: 1px solid #3A4A60;"
                               "   font-weight: bold;"
                               "   font-size: 13px;"
                               "}"
                               "QTableView::item {"
                               "   padding: 5px;"
                               "}"
                               "QHeaderView::section {"
                               "   background-color: #3A4A60;"
                               "   color: #FFFFFF;"
                               "   padding: 6px;"
                               "   border: 1px solid #4A5A70;"
                               "   font-weight: bold;"
                               "}");
  tableLayout->addWidget(m_strikeTable, 1);

  // Put Table (Right)
  m_putTable = new QTableView();
  m_putTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_putTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_putTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_putTable->verticalHeader()->hide();
  m_putTable->setAlternatingRowColors(false);
  m_putTable->setShowGrid(true);
  m_putTable->setGridStyle(Qt::SolidLine);
  m_putTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_putTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_putTable->viewport()->installEventFilter(this);
  m_putTable->setStyleSheet("QTableView {"
                            "   background-color: #1E2A38;"
                            "   color: #FFFFFF;"
                            "   gridline-color: #2A3A50;"
                            "   border: 1px solid #3A4A60;"
                            "}"
                            "QTableView::item {"
                            "   padding: 5px;"
                            "}"
                            "QHeaderView::section {"
                            "   background-color: #2A3A50;"
                            "   color: #FFFFFF;"
                            "   padding: 6px;"
                            "   border: 1px solid #3A4A60;"
                            "   font-weight: bold;"
                            "}");
  tableLayout->addWidget(m_putTable, 4);

  mainLayout->addLayout(tableLayout);

  // Set main widget style
  setStyleSheet("QWidget { background-color: #1E2A38; }");
}

void OptionChainWindow::setupModels() {
  // ========================================================================
  // Call Model
  // ========================================================================
  m_callModel = new QStandardItemModel(this);
  m_callModel->setColumnCount(CALL_COLUMN_COUNT);
  m_callModel->setHorizontalHeaderLabels({"", "OI", "Chng in OI", "Volume",
                                          "IV", "LTP", "Chng", "BID QTY", "BID",
                                          "ASK", "ASK QTY"});

  m_callTable->setModel(m_callModel);

  // Set column widths
  m_callTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_callTable->setColumnWidth(0, 30); // Checkbox column
  m_callTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);

  // Custom delegate for color coding
  m_callDelegate = new OptionChainDelegate(this);
  m_callTable->setItemDelegate(m_callDelegate);

  // ========================================================================
  // Strike Model
  // ========================================================================
  m_strikeModel = new QStandardItemModel(this);
  m_strikeModel->setColumnCount(1);
  m_strikeModel->setHorizontalHeaderLabels({"Strike"});

  m_strikeTable->setModel(m_strikeModel);
  m_strikeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

  // ========================================================================
  // Put Model
  // ========================================================================
  m_putModel = new QStandardItemModel(this);
  m_putModel->setColumnCount(PUT_COLUMN_COUNT);
  m_putModel->setHorizontalHeaderLabels({"BID QTY", "BID", "ASK", "ASK QTY",
                                         "Chng", "LTP", "IV", "Volume",
                                         "Chng in OI", "OI", ""});

  m_putTable->setModel(m_putModel);

  // Set column widths
  m_putTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_putTable->setColumnWidth(PUT_COLUMN_COUNT - 1, 30); // Checkbox column
  m_putTable->horizontalHeader()->setSectionResizeMode(PUT_COLUMN_COUNT - 1,
                                                       QHeaderView::Fixed);

  // Custom delegate for color coding
  m_putDelegate = new OptionChainDelegate(this);
  m_putTable->setItemDelegate(m_putDelegate);
}

void OptionChainWindow::setupConnections() {
  // Header controls
  connect(m_symbolCombo, &QComboBox::currentTextChanged, this,
          &OptionChainWindow::onSymbolChanged);
  connect(m_expiryCombo, &QComboBox::currentTextChanged, this,
          &OptionChainWindow::onExpiryChanged);
  connect(m_refreshButton, &QPushButton::clicked, this,
          &OptionChainWindow::onRefreshClicked);
  connect(m_calculatorButton, &QPushButton::clicked, this,
          &OptionChainWindow::onCalculatorClicked);

  // Table interactions
  connect(m_callTable, &QTableView::clicked, this,
          &OptionChainWindow::onCallTableClicked);
  connect(m_putTable, &QTableView::clicked, this,
          &OptionChainWindow::onPutTableClicked);
  connect(m_strikeTable, &QTableView::clicked, this,
          &OptionChainWindow::onStrikeTableClicked);

  // Synchronize scroll bars - Only use strike table as master
  connect(m_strikeTable->verticalScrollBar(), &QScrollBar::valueChanged, this,
          &OptionChainWindow::synchronizeScrollBars);

  connect(this, &OptionChainWindow::refreshRequested, this,
          &OptionChainWindow::refreshData);
}

void OptionChainWindow::setSymbol(const QString &symbol,
                                  const QString &expiry) {
  m_currentSymbol = symbol;
  m_currentExpiry = expiry;

  m_symbolCombo->setCurrentText(symbol);
  m_expiryCombo->setCurrentText(expiry);
  m_titleLabel->setText(symbol);

  // Refresh data
  emit refreshRequested();
}

void OptionChainWindow::updateStrikeData(double strike,
                                         const OptionStrikeData &data) {
  m_strikeData[strike] = data;

  // Find the row for this strike
  int row = m_strikes.indexOf(strike);
  if (row < 0)
    return;

  // Helper lambda to update item with color direction
  auto updateItemWithColor = [](QStandardItem *item, double newValue,
                                int precision = 2) {
    double oldValue = item->text().toDouble();
    item->setText(QString::number(newValue, 'f', precision));

    // Store direction: 1 for Up (Blue), 2 for Down (Red), 0 for No
    // Change/Neutral
    if (oldValue > 0 && newValue > oldValue) {
      item->setData(1, Qt::UserRole + 1);
    } else if (oldValue > 0 && newValue < oldValue) {
      item->setData(2, Qt::UserRole + 1);
    } else {
      // Keep previous color if equal, or reset? Let's keep previous for
      // flickering effect, or reset. Usually reset to neutral if equal, but
      // market data usually flickers. If equal, maybe we don't change the data
      // role, so it keeps the last state? Or better, 0 for neutral.
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
      ->setText(QString::number(data.callIV, 'f', 2));

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
  m_putModel->item(row, PUT_IV)->setText(QString::number(data.putIV, 'f', 2));
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

void OptionChainWindow::onSymbolChanged(const QString &symbol) {
  if (symbol == m_currentSymbol)
    return;

  m_currentSymbol = symbol;
  m_titleLabel->setText(symbol);

  // Update expiries for the new symbol
  populateExpiries(symbol);

  emit refreshRequested();
}

void OptionChainWindow::onExpiryChanged(const QString &expiry) {
  if (expiry == m_currentExpiry)
    return;

  m_currentExpiry = expiry;
  emit refreshRequested();
}

void OptionChainWindow::onRefreshClicked() { emit refreshRequested(); }

void OptionChainWindow::onTradeClicked() {
  // Determine which option is selected
  if (m_selectedCallRow >= 0) {
    double strike = getStrikeAtRow(m_selectedCallRow);
    emit tradeRequested(m_currentSymbol, m_currentExpiry, strike, "CE");
  } else if (m_selectedPutRow >= 0) {
    double strike = getStrikeAtRow(m_selectedPutRow);
    emit tradeRequested(m_currentSymbol, m_currentExpiry, strike, "PE");
  }
}

void OptionChainWindow::onCalculatorClicked() {
  emit calculatorRequested(m_currentSymbol, m_currentExpiry, 0.0, "");
}

void OptionChainWindow::onCallTableClicked(const QModelIndex &index) {
  int row = index.row();
  int col = index.column();

  // Handle checkbox column (column 0)
  if (col == 0) {
    QStandardItem *item = m_callModel->item(row, 0);
    if (item) {
      bool isChecked = item->checkState() == Qt::Checked;
      item->setCheckState(isChecked ? Qt::Unchecked : Qt::Checked);
    }
    return;
  }

  // Regular row click - select only call
  m_selectedCallRow = row;
  m_callTable->selectRow(row);
  m_putTable->clearSelection();

  qDebug() << "Call selected at strike:" << getStrikeAtRow(row);
}

void OptionChainWindow::onPutTableClicked(const QModelIndex &index) {
  int row = index.row();
  int col = index.column();

  // Handle checkbox column (last column)
  if (col == PUT_COLUMN_COUNT - 1) {
    QStandardItem *item = m_putModel->item(row, PUT_COLUMN_COUNT - 1);
    if (item) {
      bool isChecked = item->checkState() == Qt::Checked;
      item->setCheckState(isChecked ? Qt::Unchecked : Qt::Checked);
    }
    return;
  }

  // Regular row click - select only put
  m_selectedPutRow = row;
  m_putTable->selectRow(row);
  m_callTable->clearSelection();

  qDebug() << "Put selected at strike:" << getStrikeAtRow(row);
}

void OptionChainWindow::onStrikeTableClicked(const QModelIndex &index) {
  int row = index.row();

  // Strike click - select both call and put for the same strike
  m_selectedCallRow = row;
  m_selectedPutRow = row;

  m_callTable->selectRow(row);
  m_putTable->selectRow(row);
  m_strikeTable->selectRow(row);

  qDebug() << "Strike selected:" << getStrikeAtRow(row)
           << "- Both Call and Put selected";
}

bool OptionChainWindow::eventFilter(QObject *obj, QEvent *event) {
  // Handle wheel events on all table viewports for synchronized scrolling
  if (event->type() == QEvent::Wheel) {
    if (obj == m_callTable->viewport() || obj == m_putTable->viewport() ||
        obj == m_strikeTable->viewport()) {

      QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);

      // Get current scroll position
      int currentValue = m_strikeTable->verticalScrollBar()->value();
      int delta = wheelEvent->angleDelta().y();
      int step = m_strikeTable->verticalScrollBar()->singleStep();

      // Calculate new position
      int newValue = currentValue - (delta > 0 ? step : -step);

      // Apply to all tables
      m_strikeTable->verticalScrollBar()->setValue(newValue);
      m_callTable->verticalScrollBar()->setValue(newValue);
      m_putTable->verticalScrollBar()->setValue(newValue);

      return true; // Event handled
    }
  }

  return QWidget::eventFilter(obj, event);
}

void OptionChainWindow::synchronizeScrollBars(int value) {
  // Strike table is the master - sync call and put tables to it
  // No need to block signals since only strike table emits
  m_callTable->verticalScrollBar()->setValue(value);
  m_putTable->verticalScrollBar()->setValue(value);
}

void OptionChainWindow::highlightATMStrike() {
  // Find ATM row
  int atmRow = -1;
  for (int i = 0; i < m_strikes.size(); ++i) {
    if (m_strikes[i] == m_atmStrike) {
      atmRow = i;
      break;
    }
  }

  if (atmRow < 0)
    return;

  // Highlight the ATM strike row with different background
  QColor atmColor("#3A5A70");

  for (int col = 0; col < m_callModel->columnCount(); ++col) {
    QStandardItem *item = m_callModel->item(atmRow, col);
    if (item) {
      item->setBackground(atmColor);
    }
  }

  QStandardItem *strikeItem = m_strikeModel->item(atmRow, 0);
  if (strikeItem) {
    strikeItem->setBackground(QColor("#4A6A80"));
    strikeItem->setForeground(QColor("#FFFF00"));
  }

  for (int col = 0; col < m_putModel->columnCount(); ++col) {
    QStandardItem *item = m_putModel->item(atmRow, col);
    if (item) {
      item->setBackground(atmColor);
    }
  }
}

void OptionChainWindow::updateTableColors() {
  // This method can be called after data updates to refresh colors
  m_callTable->viewport()->update();
  m_putTable->viewport()->update();
}

QModelIndex OptionChainWindow::getStrikeIndex(int row) const {
  return m_strikeModel->index(row, 0);
}

double OptionChainWindow::getStrikeAtRow(int row) const {
  if (row >= 0 && row < m_strikes.size()) {
    return m_strikes[row];
  }
  return 0.0;
}

void OptionChainWindow::refreshData() {
  // Clear existing data and subscriptions
  MemoryProfiler::logSnapshot("OptionChain: Pre-Refresh");

  FeedHandler::instance().unsubscribeAll(this);
  clearData();
  m_tokenToStrike.clear();

  // Get parameters
  QString symbol = m_symbolCombo->currentText();
  QString expiry = m_expiryCombo->currentText();

  if (symbol.isEmpty())
    return;

  m_currentSymbol = symbol;
  m_currentExpiry = expiry;

  RepositoryManager *repo = RepositoryManager::getInstance();

  // Query contracts - try NSE first
  int exchangeSegment = 2; // NSEFO
  QVector<ContractData> contracts = repo->getOptionChain("NSE", symbol);
  if (contracts.isEmpty()) {
    contracts = repo->getOptionChain("BSE", symbol);
    exchangeSegment = 12; // BSEFO
  }

  // Filter and Group
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

  // Sort strikes
  QList<double> sortedStrikes = strikes.values();
  std::sort(sortedStrikes.begin(), sortedStrikes.end());
  m_strikes = sortedStrikes;

  m_strikes = sortedStrikes;

  // Create Rows
  QList<QList<QStandardItem *>> callRows;
  QList<QList<QStandardItem *>> putRows;
  QList<QStandardItem *> strikeRows;

  // Prepare data off-thread (conceptually) or just prepare all items first

  FeedHandler &feed = FeedHandler::instance();

  for (double strike : sortedStrikes) {
    OptionStrikeData data;
    data.strikePrice = strike;

    // Contracts & Subscription
    if (callContracts.contains(strike)) {
      data.callToken = callContracts[strike].exchangeInstrumentID;

      feed.subscribe(exchangeSegment, data.callToken, this,
                     &OptionChainWindow::onTickUpdate);

      m_tokenToStrike[data.callToken] = strike;

      if (auto unifiedState =
              MarketData::PriceStoreGateway::instance().getUnifiedState(
                  exchangeSegment, data.callToken)) {
        if (unifiedState->ltp > 0) {
          data.callLTP = unifiedState->ltp;
          if (unifiedState->close > 0)
            data.callChng = unifiedState->ltp - unifiedState->close;
        }
        if (unifiedState->bids[0].price > 0)
          data.callBid = unifiedState->bids[0].price;
        if (unifiedState->asks[0].price > 0)
          data.callAsk = unifiedState->asks[0].price;
        if (unifiedState->bids[0].quantity > 0)
          data.callBidQty = unifiedState->bids[0].quantity;
        if (unifiedState->asks[0].quantity > 0)
          data.callAskQty = unifiedState->asks[0].quantity;
        if (unifiedState->volume > 0)
          data.callVolume = unifiedState->volume;
        if (unifiedState->openInterest > 0)
          data.callOI = (int)unifiedState->openInterest;
      }
    }

    if (putContracts.contains(strike)) {
      data.putToken = putContracts[strike].exchangeInstrumentID;

      feed.subscribe(exchangeSegment, data.putToken, this,
                     &OptionChainWindow::onTickUpdate);

      m_tokenToStrike[data.putToken] = strike;

      if (auto unifiedState =
              MarketData::PriceStoreGateway::instance().getUnifiedState(
                  exchangeSegment, data.putToken)) {
        if (unifiedState->ltp > 0) {
          data.putLTP = unifiedState->ltp;
          if (unifiedState->close > 0)
            data.putChng = unifiedState->ltp - unifiedState->close;
        }
        if (unifiedState->bids[0].price > 0)
          data.putBid = unifiedState->bids[0].price;
        if (unifiedState->asks[0].price > 0)
          data.putAsk = unifiedState->asks[0].price;
        if (unifiedState->bids[0].quantity > 0)
          data.putBidQty = (int)unifiedState->bids[0].quantity;
        if (unifiedState->asks[0].quantity > 0)
          data.putAskQty = (int)unifiedState->asks[0].quantity;
        if (unifiedState->volume > 0)
          data.putVolume = (int)unifiedState->volume;
        if (unifiedState->openInterest > 0)
          data.putOI = (int)unifiedState->openInterest;
      }
    }

    m_strikeData[strike] = data;

    // --- Create Visual Items ---

    // Helper to create colored item
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
         << createItem(data.callIV) << createItem(data.callLTP)
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
         << createItem(data.putIV) << createIntItem(data.putVolume)
         << createIntItem(data.putChngInOI) << createIntItem(data.putOI)
         << cbPut;

    for (int i = 0; i < pRow.size() - 1; ++i)
      pRow[i]->setTextAlignment(Qt::AlignCenter);
    putRows.append(pRow);
  }

  // Batch Insert to Views (Prevent Layout Thrashing)
  m_callTable->setUpdatesEnabled(false);
  m_strikeTable->setUpdatesEnabled(false);
  m_putTable->setUpdatesEnabled(false);

  // Using simple appendRow loop but with updates disabled is better,
  // but insertRow is still O(N).
  // Ideally we should use standard item model's internal list but appendRow is
  // fine if updates are off.

  // Even better: block signals from models?
  // m_callModel->blockSignals(true); // No, this stops view from referencing
  // items? calling appendRow notifies view.

  // Best:
  // Layout change only happens once if we block signals or updates enabled.

  for (const auto &row : callRows)
    m_callModel->appendRow(row);
  for (auto item : strikeRows)
    m_strikeModel->appendRow(item);
  for (const auto &row : putRows)
    m_putModel->appendRow(row);

  m_callTable->setUpdatesEnabled(true);
  m_strikeTable->setUpdatesEnabled(true);
  m_putTable->setUpdatesEnabled(true);

  // Set ATM
  if (!m_strikes.isEmpty()) {
    m_atmStrike = m_strikes[m_strikes.size() / 2];
    highlightATMStrike();

    // Auto-scroll to ATM (Center the view)
    int row = m_strikes.indexOf(m_atmStrike);
    if (row >= 0) {
      // Defer scrolling to allow view layout to update
      QTimer::singleShot(0, this, [this, row]() {
        QModelIndex strikeIdx = m_strikeModel->index(row, 0);
        if (strikeIdx.isValid()) {
          // Scroll Master (Strike Table)
          m_strikeTable->scrollTo(strikeIdx,
                                  QAbstractItemView::PositionAtCenter);

          // Manually sync others to be safe
          int val = m_strikeTable->verticalScrollBar()->value();
          m_callTable->verticalScrollBar()->setValue(val);
          m_putTable->verticalScrollBar()->setValue(val);
        }
      });
    }
  }

  // Initial color update
  updateTableColors();
}

WindowContext OptionChainWindow::getSelectedContext() const {
  WindowContext context;
  context.sourceWindow = "OptionChain";

  double strike = 0.0;
  QString optionType;
  int token = 0;

  // Prioritize Call selection
  if (m_selectedCallRow >= 0 && m_callTable->selectionModel()->hasSelection()) {
    strike = getStrikeAtRow(m_selectedCallRow);
    optionType = "CE";
    if (m_strikeData.contains(strike)) {
      token = m_strikeData[strike].callToken;
      context.ltp = m_strikeData[strike].callLTP;
      context.bid = m_strikeData[strike].callBid;
      context.ask = m_strikeData[strike].callAsk;
      context.volume = m_strikeData[strike].callVolume;
    }
  }
  // Check Put selection
  else if (m_selectedPutRow >= 0 &&
           m_putTable->selectionModel()->hasSelection()) {
    strike = getStrikeAtRow(m_selectedPutRow);
    optionType = "PE";
    if (m_strikeData.contains(strike)) {
      token = m_strikeData[strike].putToken;
      context.ltp = m_strikeData[strike].putLTP;
      context.bid = m_strikeData[strike].putBid;
      context.ask = m_strikeData[strike].putAsk;
      context.volume = m_strikeData[strike].putVolume;
    }
  }

  if (token > 0) {
    context.token = token;
    context.symbol = m_currentSymbol;
    context.expiry = m_currentExpiry;
    context.strikePrice = strike;
    context.optionType = optionType;

    // Fetch detailed contract info
    const ContractData *contract = nullptr;

    // Try to find in NSEFO first
    contract =
        RepositoryManager::getInstance()->getContractByToken("NSEFO", token);
    if (contract) {
      context.exchange = "NSEFO";
    } else {
      // Try BSEFO
      contract =
          RepositoryManager::getInstance()->getContractByToken("BSEFO", token);
      if (contract)
        context.exchange = "BSEFO";
    }

    if (contract) {
      context.segment = "D"; // Derivative
      context.instrumentType = contract->instrumentType;
      context.lotSize = contract->lotSize;
      context.tickSize = contract->tickSize;
      context.freezeQty = contract->freezeQty;
      context.displayName = contract->displayName;
      context.series = contract->series;
    }
  }

  return context;
}

void OptionChainWindow::onTickUpdate(const UDP::MarketTick &tick) {
  if (!m_tokenToStrike.contains(tick.token))
    return;

  double strike = m_tokenToStrike[tick.token];
  OptionStrikeData &data = m_strikeData[strike];

  // Determine if Call or Put
  bool isCall = (tick.token == (uint32_t)data.callToken);

  // Update fields
  if (isCall) {
    if (tick.ltp > 0) {
      data.callLTP = tick.ltp;
      // Only update Change if we have a valid LTP and Close
      if (tick.prevClose > 0) {
        data.callChng = tick.ltp - tick.prevClose;
      }
    }

    // Depth / Quote Updates
    if (tick.bids[0].price > 0)
      data.callBid = tick.bids[0].price;
    if (tick.asks[0].price > 0)
      data.callAsk = tick.asks[0].price;
    if (tick.bids[0].quantity > 0)
      data.callBidQty = (int)tick.bids[0].quantity;
    if (tick.asks[0].quantity > 0)
      data.callAskQty = (int)tick.asks[0].quantity;

    // Statistics Updates
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

  // Trigger visual update
  updateStrikeData(strike, data);
}

void OptionChainWindow::populateSymbols() {
  const QSignalBlocker blocker(m_symbolCombo);
  m_symbolCombo->clear();

  RepositoryManager *repo = RepositoryManager::getInstance();
  QSet<QString> symbols;

  // 1. Get Index Futures (NIFTY, BANKNIFTY, FINNIFTY, etc.)
  QVector<ContractData> indices = repo->getScrips("NSE", "FO", "FUTIDX");
  for (const auto &contract : indices) {
    symbols.insert(contract.name);
  }

  // 2. Get Stock Futures (RELIANCE, TCS, etc.)
  // Note: Some stocks might have options but no futures (rare), or vice versa.
  // Checking FUTSTK is a safe proxy for F&O stocks.
  QVector<ContractData> stocks = repo->getScrips("NSE", "FO", "FUTSTK");
  for (const auto &contract : stocks) {
    symbols.insert(contract.name);
  }

  // Also check BSE if NSE is empty (or merge)
  if (symbols.isEmpty()) {
    QVector<ContractData> bseIndices = repo->getScrips("BSE", "FO", "FUTIDX");
    for (const auto &contract : bseIndices)
      symbols.insert(contract.name);
  }

  // Sort
  QStringList sortedSymbols = symbols.values();
  std::sort(sortedSymbols.begin(), sortedSymbols.end());

  // Add to ComboBox
  m_symbolCombo->addItems(sortedSymbols);
  m_symbolCombo->setEditable(true); // Enable search/typing
  m_symbolCombo->setInsertPolicy(
      QComboBox::NoInsert); // Don't add user types to list if not present

  // Default Selection (NIFTY or first)
  int index = m_symbolCombo->findText("NIFTY");
  if (index >= 0) {
    m_symbolCombo->setCurrentIndex(index);
  } else if (!sortedSymbols.isEmpty()) {
    m_symbolCombo->setCurrentIndex(0);
  }

  // Manually chain to expiries since signals were blocked
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

  // Get all option contracts for this symbol
  // We can use getOptionChain which ideally returns options.
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

  // Sort Expiries (Need Date Parsing)
  // Formats: "26DEC2025", "02JAN2026" (DDMMMYYYY)
  QList<QDate> sortedDates;
  QMap<QDate, QString> dateToString;

  for (const QString &exp : expirySet) {
    // Try parsing DDMMMYYYY (e.g., 01JAN2024 or 1JAN2024)
    // Note: QDate format MMM is usually short month name (Jan/Feb).
    // XTS Expiry is usually upper case "26DEC2025". QDate might need Title Case
    // "26Dec2025".

    QString cleanExp = exp;
    // Basic normalization: valid for English locale if title case?
    // Actually custom parsing might be safer if format is strict.
    // Let's rely on QLocale::system().toDate or format string.

    // Convert "DEC" to "Dec" for parsing if needed
    if (cleanExp.length() >= 3) {
      // Lowercase all then title case month?
      // Simple hack: Titlecase the month part?
      // "26DEC2024" -> "26Dec2024"
      // Implementation details depend on Qt version quirks, but "ddMMMyyyy"
      // expects "Dec".

      // Quick fix: standardizing string for parsing
      QString dayPart, monthPart, yearPart;
      int yearIdx = cleanExp.length() - 4;
      if (yearIdx > 0) {
        yearPart = cleanExp.mid(yearIdx);
        // Month is before year, 3 chars?
        monthPart = cleanExp.mid(yearIdx - 3, 3);
        dayPart = cleanExp.left(yearIdx - 3);

        // Capitalize first letter of month, rest lowercase: "DEC" -> "Dec"
        monthPart = monthPart.at(0).toUpper() + monthPart.mid(1).toLower();

        QString parseable = dayPart + monthPart + yearPart;
        QDate d = QDate::fromString(parseable, "dMMMyyyy");
        if (d.isValid()) {
          sortedDates.append(d);
          dateToString[d] = exp; // Keep original string for UI/Filtering
        } else {
          qDebug() << "Failed to parse expiry:" << exp
                   << " Cleaned:" << parseable;
          // Fallback: just add to list?
        }
      }
    }
  }

  // Sort using Qt's comparison operator for QDate
  std::stable_sort(sortedDates.begin(), sortedDates.end(),
                   [](const QDate &a, const QDate &b) { return a < b; });

  for (const QDate &d : sortedDates) {
    m_expiryCombo->addItem(dateToString[d]);
  }

  if (m_expiryCombo->count() > 0) {
    m_expiryCombo->setCurrentIndex(0); // Select nearest expiry
    m_currentExpiry = m_expiryCombo->currentText();
  }
}
