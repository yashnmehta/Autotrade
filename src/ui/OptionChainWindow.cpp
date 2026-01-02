#include "ui/OptionChainWindow.h"
#include <QHeaderView>
#include <QSplitter>
#include <QScrollBar>
#include <QPainter>
#include <QDebug>
#include <QPushButton>
#include <QGroupBox>
#include <QWheelEvent>
#include <cmath>
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include "services/PriceCache.h"
#include "services/TokenSubscriptionManager.h"

// ============================================================================
// OptionChainDelegate Implementation
// ============================================================================

OptionChainDelegate::OptionChainDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void OptionChainDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    // For checkbox columns, use default rendering
    QString headerText = index.model()->headerData(index.column(), Qt::Horizontal).toString();
    if (headerText.isEmpty() && index.column() == 0) {
        // This is a checkbox column (empty header in first or last column)
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    if (headerText.isEmpty() && index.column() == index.model()->columnCount() - 1) {
        // This is a checkbox column (empty header in first or last column)
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    
    painter->save();
    
    // Get the value and determine color
    QString text = index.data(Qt::DisplayRole).toString();
    QColor bgColor = QColor("#2A3A50"); // Default background
    QColor textColor = Qt::white;
    
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
                                   const QModelIndex &index) const
{
    return QStyledItemDelegate::sizeHint(option, index);
}

// ============================================================================
// OptionChainWindow Implementation
// ============================================================================

OptionChainWindow::OptionChainWindow(QWidget *parent)
    : QWidget(parent)
    , m_symbolCombo(nullptr)
    , m_expiryCombo(nullptr)
    , m_refreshButton(nullptr)
    , m_calculatorButton(nullptr)
    , m_titleLabel(nullptr)
    , m_callTable(nullptr)
    , m_strikeTable(nullptr)
    , m_putTable(nullptr)
    , m_callModel(nullptr)
    , m_strikeModel(nullptr)
    , m_putModel(nullptr)
    , m_callDelegate(nullptr)
    , m_putDelegate(nullptr)
    , m_atmStrike(0.0)
    , m_selectedCallRow(-1)
    , m_selectedPutRow(-1)
{
    setupUI();
    setupModels();
    setupConnections();
    
    // Populate symbols (triggers chain reaction: symbol -> expiry -> refresh)
    populateSymbols();
    
    setWindowTitle("Option Chain");
    resize(1600, 800);
}

OptionChainWindow::~OptionChainWindow()
{
}

void OptionChainWindow::setupUI()
{
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
    m_titleLabel->setStyleSheet("QLabel { font-size: 18px; font-weight: bold; color: #FFFFFF; }");
    headerLayout->addWidget(m_titleLabel);
    
    headerLayout->addStretch();
    
    // Symbol selection
    QLabel *symbolLabel = new QLabel("Symbol:");
    symbolLabel->setStyleSheet("QLabel { color: #FFFFFF; }");
    headerLayout->addWidget(symbolLabel);
    
    m_symbolCombo = new QComboBox();
    m_symbolCombo->setMinimumWidth(120);
    m_symbolCombo->setStyleSheet(
        "QComboBox { background: #2A3A50; color: #FFFFFF; border: 1px solid #3A4A60; padding: 5px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox::down-arrow { image: url(none); }"
    );
    headerLayout->addWidget(m_symbolCombo);
    
    // Expiry selection
    QLabel *expiryLabel = new QLabel("Expiry:");
    expiryLabel->setStyleSheet("QLabel { color: #FFFFFF; }");
    headerLayout->addWidget(expiryLabel);
    
    m_expiryCombo = new QComboBox();
    m_expiryCombo->setMinimumWidth(120);
    m_expiryCombo->setStyleSheet(
        "QComboBox { background: #2A3A50; color: #FFFFFF; border: 1px solid #3A4A60; padding: 5px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox::down-arrow { image: url(none); }"
    );
    headerLayout->addWidget(m_expiryCombo);
    
    // Refresh button
    m_refreshButton = new QPushButton("Refresh");
    m_refreshButton->setStyleSheet(
        "QPushButton { background: #2A5A80; color: #FFFFFF; border: none; padding: 6px 15px; border-radius: 3px; }"
        "QPushButton:hover { background: #3A6A90; }"
        "QPushButton:pressed { background: #1A4A70; }"
    );
    headerLayout->addWidget(m_refreshButton);
    
    // Calculator button
    m_calculatorButton = new QPushButton("View Calculators");
    m_calculatorButton->setStyleSheet(
        "QPushButton { background: #2A5A80; color: #FFFFFF; border: none; padding: 6px 15px; border-radius: 3px; }"
        "QPushButton:hover { background: #3A6A90; }"
        "QPushButton:pressed { background: #1A4A70; }"
    );
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
    m_callTable->setStyleSheet(
        "QTableView {"
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
        "}"
    );
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
    m_strikeTable->setStyleSheet(
        "QTableView {"
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
        "}"
    );
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
    m_putTable->setStyleSheet(
        "QTableView {"
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
        "}"
    );
    tableLayout->addWidget(m_putTable, 4);
    
    mainLayout->addLayout(tableLayout);
    
    // Set main widget style
    setStyleSheet("QWidget { background-color: #1E2A38; }");
}

void OptionChainWindow::setupModels()
{
    // ========================================================================
    // Call Model
    // ========================================================================
    m_callModel = new QStandardItemModel(this);
    m_callModel->setColumnCount(CALL_COLUMN_COUNT);
    m_callModel->setHorizontalHeaderLabels({
        "", "OI", "Chng in OI", "Volume", "IV", "LTP", "Chng", "BID QTY", "BID", "ASK", "ASK QTY"
    });
    
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
    m_putModel->setHorizontalHeaderLabels({
        "BID QTY", "BID", "ASK", "ASK QTY", "Chng", "LTP", "IV", "Volume", "Chng in OI", "OI", ""
    });
    
    m_putTable->setModel(m_putModel);
    
    // Set column widths
    m_putTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_putTable->setColumnWidth(PUT_COLUMN_COUNT - 1, 30); // Checkbox column
    m_putTable->horizontalHeader()->setSectionResizeMode(PUT_COLUMN_COUNT - 1, QHeaderView::Fixed);
    
    // Custom delegate for color coding
    m_putDelegate = new OptionChainDelegate(this);
    m_putTable->setItemDelegate(m_putDelegate);
}

void OptionChainWindow::setupConnections()
{
    // Header controls
    connect(m_symbolCombo, &QComboBox::currentTextChanged, 
            this, &OptionChainWindow::onSymbolChanged);
    connect(m_expiryCombo, &QComboBox::currentTextChanged,
            this, &OptionChainWindow::onExpiryChanged);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &OptionChainWindow::onRefreshClicked);
    connect(m_calculatorButton, &QPushButton::clicked,
            this, &OptionChainWindow::onCalculatorClicked);
    
    // Table interactions
    connect(m_callTable, &QTableView::clicked,
            this, &OptionChainWindow::onCallTableClicked);
    connect(m_putTable, &QTableView::clicked,
            this, &OptionChainWindow::onPutTableClicked);
    connect(m_strikeTable, &QTableView::clicked,
            this, &OptionChainWindow::onStrikeTableClicked);
    
    // Synchronize scroll bars - Only use strike table as master
    connect(m_strikeTable->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &OptionChainWindow::synchronizeScrollBars);
            
    connect(this, &OptionChainWindow::refreshRequested, 
            this, &OptionChainWindow::refreshData);
}



void OptionChainWindow::setSymbol(const QString &symbol, const QString &expiry)
{
    m_currentSymbol = symbol;
    m_currentExpiry = expiry;
    
    m_symbolCombo->setCurrentText(symbol);
    m_expiryCombo->setCurrentText(expiry);
    m_titleLabel->setText(symbol);
    
    // Refresh data
    emit refreshRequested();
}

void OptionChainWindow::updateStrikeData(double strike, const OptionStrikeData &data)
{
    m_strikeData[strike] = data;
    
    // Find the row for this strike
    int row = m_strikes.indexOf(strike);
    if (row < 0) return;
    
    // Update call data
    m_callModel->item(row, CALL_OI)->setText(QString::number(data.callOI));
    m_callModel->item(row, CALL_CHNG_IN_OI)->setText(QString::number(data.callChngInOI));
    m_callModel->item(row, CALL_VOLUME)->setText(QString::number(data.callVolume));
    m_callModel->item(row, CALL_IV)->setText(QString::number(data.callIV, 'f', 2));
    m_callModel->item(row, CALL_LTP)->setText(QString::number(data.callLTP, 'f', 2));
    m_callModel->item(row, CALL_CHNG)->setText(QString::number(data.callChng, 'f', 2));
    m_callModel->item(row, CALL_BID_QTY)->setText(QString::number(data.callBidQty));
    m_callModel->item(row, CALL_BID)->setText(QString::number(data.callBid, 'f', 2));
    m_callModel->item(row, CALL_ASK)->setText(QString::number(data.callAsk, 'f', 2));
    m_callModel->item(row, CALL_ASK_QTY)->setText(QString::number(data.callAskQty));
    
    // Update put data
    m_putModel->item(row, PUT_BID_QTY)->setText(QString::number(data.putBidQty));
    m_putModel->item(row, PUT_BID)->setText(QString::number(data.putBid, 'f', 2));
    m_putModel->item(row, PUT_ASK)->setText(QString::number(data.putAsk, 'f', 2));
    m_putModel->item(row, PUT_ASK_QTY)->setText(QString::number(data.putAskQty));
    m_putModel->item(row, PUT_CHNG)->setText(QString::number(data.putChng, 'f', 2));
    m_putModel->item(row, PUT_LTP)->setText(QString::number(data.putLTP, 'f', 2));
    m_putModel->item(row, PUT_IV)->setText(QString::number(data.putIV, 'f', 2));
    m_putModel->item(row, PUT_VOLUME)->setText(QString::number(data.putVolume));
    m_putModel->item(row, PUT_CHNG_IN_OI)->setText(QString::number(data.putChngInOI));
    m_putModel->item(row, PUT_OI)->setText(QString::number(data.putOI));
}

void OptionChainWindow::clearData()
{
    m_callModel->removeRows(0, m_callModel->rowCount());
    m_strikeModel->removeRows(0, m_strikeModel->rowCount());
    m_putModel->removeRows(0, m_putModel->rowCount());
    m_strikeData.clear();
    m_strikes.clear();
}

void OptionChainWindow::setStrikeRange(double minStrike, double maxStrike, double interval)
{
    clearData();
    
    for (double strike = minStrike; strike <= maxStrike; strike += interval) {
        m_strikes.append(strike);
    }
}

void OptionChainWindow::setATMStrike(double atmStrike)
{
    m_atmStrike = atmStrike;
    highlightATMStrike();
}

void OptionChainWindow::onSymbolChanged(const QString &symbol)
{
    if (symbol == m_currentSymbol) return;
    
    m_currentSymbol = symbol;
    m_titleLabel->setText(symbol);
    
    // Update expiries for the new symbol
    populateExpiries(symbol);
    
    emit refreshRequested();
}

void OptionChainWindow::onExpiryChanged(const QString &expiry)
{
    if (expiry == m_currentExpiry) return;
    
    m_currentExpiry = expiry;
    emit refreshRequested();
}

void OptionChainWindow::onRefreshClicked()
{
    emit refreshRequested();
}

void OptionChainWindow::onTradeClicked()
{
    // Determine which option is selected
    if (m_selectedCallRow >= 0) {
        double strike = getStrikeAtRow(m_selectedCallRow);
        emit tradeRequested(m_currentSymbol, m_currentExpiry, strike, "CE");
    } else if (m_selectedPutRow >= 0) {
        double strike = getStrikeAtRow(m_selectedPutRow);
        emit tradeRequested(m_currentSymbol, m_currentExpiry, strike, "PE");
    }
}

void OptionChainWindow::onCalculatorClicked()
{
    emit calculatorRequested(m_currentSymbol, m_currentExpiry, 0.0, "");
}

void OptionChainWindow::onCallTableClicked(const QModelIndex &index)
{
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

void OptionChainWindow::onPutTableClicked(const QModelIndex &index)
{
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

void OptionChainWindow::onStrikeTableClicked(const QModelIndex &index)
{
    int row = index.row();
    
    // Strike click - select both call and put for the same strike
    m_selectedCallRow = row;
    m_selectedPutRow = row;
    
    m_callTable->selectRow(row);
    m_putTable->selectRow(row);
    m_strikeTable->selectRow(row);
    
    qDebug() << "Strike selected:" << getStrikeAtRow(row) << "- Both Call and Put selected";
}

bool OptionChainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Handle wheel events on all table viewports for synchronized scrolling
    if (event->type() == QEvent::Wheel) {
        if (obj == m_callTable->viewport() || 
            obj == m_putTable->viewport() || 
            obj == m_strikeTable->viewport()) {
            
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            
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

void OptionChainWindow::synchronizeScrollBars(int value)
{
    // Strike table is the master - sync call and put tables to it
    // No need to block signals since only strike table emits
    m_callTable->verticalScrollBar()->setValue(value);
    m_putTable->verticalScrollBar()->setValue(value);
}

void OptionChainWindow::highlightATMStrike()
{
    // Find ATM row
    int atmRow = -1;
    for (int i = 0; i < m_strikes.size(); ++i) {
        if (m_strikes[i] == m_atmStrike) {
            atmRow = i;
            break;
        }
    }
    
    if (atmRow < 0) return;
    
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

void OptionChainWindow::updateTableColors()
{
    // This method can be called after data updates to refresh colors
    m_callTable->viewport()->update();
    m_putTable->viewport()->update();
}

QModelIndex OptionChainWindow::getStrikeIndex(int row) const
{
    return m_strikeModel->index(row, 0);
}

double OptionChainWindow::getStrikeAtRow(int row) const
{
    if (row >= 0 && row < m_strikes.size()) {
        return m_strikes[row];
    }
    return 0.0;
}

void OptionChainWindow::refreshData()
{
    // Clear existing data and subscriptions
    FeedHandler::instance().unsubscribeAll(this);
    clearData();
    m_tokenToStrike.clear();
    
    // Get parameters
    QString symbol = m_symbolCombo->currentText();
    QString expiry = m_expiryCombo->currentText();
    
    if (symbol.isEmpty()) return;
    
    m_currentSymbol = symbol;
    m_currentExpiry = expiry;
    
    RepositoryManager* repo = RepositoryManager::getInstance();
    
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
    
    for (const auto& contract : contracts) {
        if (!expiry.isEmpty() && contract.expiryDate != expiry) continue;
        
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
    
    // Create Rows
    QList<QStandardItem*> callRow;
    QList<QStandardItem*> putRow;
    
    FeedHandler& feed = FeedHandler::instance();
    
    for (double strike : sortedStrikes) {
        OptionStrikeData data;
        data.strikePrice = strike;
        
        // Contracts
        if (callContracts.contains(strike)) {
            data.callToken = callContracts[strike].exchangeInstrumentID;
            // Subscribe
            feed.subscribe(exchangeSegment, data.callToken, this, &OptionChainWindow::onTickUpdate);
            m_tokenToStrike[data.callToken] = strike;
            
            // Initial Load from Cache
            if (auto cached = PriceCache::instance().getPrice(exchangeSegment, data.callToken)) {
                const auto& tick = *cached;
                if (tick.lastTradedPrice > 0) {
                    data.callLTP = tick.lastTradedPrice;
                    if (tick.close > 0) data.callChng = tick.lastTradedPrice - tick.close;
                }
                if (tick.bidPrice > 0) data.callBid = tick.bidPrice;
                if (tick.askPrice > 0) data.callAsk = tick.askPrice;
                if (tick.bidQuantity > 0) data.callBidQty = tick.bidQuantity;
                if (tick.askQuantity > 0) data.callAskQty = tick.askQuantity;
                if (tick.volume > 0) data.callVolume = tick.volume;
                if (tick.openInterest > 0) data.callOI = tick.openInterest;
            }
        }
        
        if (putContracts.contains(strike)) {
            data.putToken = putContracts[strike].exchangeInstrumentID;
            // Subscribe
            feed.subscribe(exchangeSegment, data.putToken, this, &OptionChainWindow::onTickUpdate);
            m_tokenToStrike[data.putToken] = strike;
            
            // Initial Load from Cache
            if (auto cached = PriceCache::instance().getPrice(exchangeSegment, data.putToken)) {
                const auto& tick = *cached;
                if (tick.lastTradedPrice > 0) {
                    data.putLTP = tick.lastTradedPrice;
                    if (tick.close > 0) data.putChng = tick.lastTradedPrice - tick.close;
                }
                if (tick.bidPrice > 0) data.putBid = tick.bidPrice;
                if (tick.askPrice > 0) data.putAsk = tick.askPrice;
                if (tick.bidQuantity > 0) data.putBidQty = tick.bidQuantity;
                if (tick.askQuantity > 0) data.putAskQty = tick.askQuantity;
                if (tick.volume > 0) data.putVolume = tick.volume;
                if (tick.openInterest > 0) data.putOI = tick.openInterest;
            }
        }
        
        m_strikeData[strike] = data;
        
        // Add visual rows (Initial empty/zero data)
        // Checkbox column
        QStandardItem *callCheckbox = new QStandardItem("");
        callCheckbox->setCheckable(true);
        callRow.clear();
        callRow << callCheckbox
                << new QStandardItem("0") // OI
                << new QStandardItem("0") // ChangeInOI
                << new QStandardItem("0") // Volume
                << new QStandardItem("0.00") // IV
                << new QStandardItem("0.00") // LTP
                << new QStandardItem("0.00") // Chng
                << new QStandardItem("0") // BidQty
                << new QStandardItem("0.00") // Bid
                << new QStandardItem("0.00") // Ask
                << new QStandardItem("0"); // AskQty
        
        for (int i=1; i<callRow.size(); ++i) callRow[i]->setTextAlignment(Qt::AlignCenter);
        m_callModel->appendRow(callRow);
        
        // Strike
        QStandardItem *strikeItem = new QStandardItem(QString::number(strike, 'f', 2));
        strikeItem->setTextAlignment(Qt::AlignCenter);
        m_strikeModel->appendRow(strikeItem);
        
        // Put Row
        QStandardItem *putCheckbox = new QStandardItem("");
        putCheckbox->setCheckable(true);
        putRow.clear();
        putRow << new QStandardItem("0") // BidQty
               << new QStandardItem("0.00") // Bid
               << new QStandardItem("0.00") // Ask
               << new QStandardItem("0") // AskQty
               << new QStandardItem("0.00") // Chng
               << new QStandardItem("0.00") // LTP
               << new QStandardItem("0.00") // IV
               << new QStandardItem("0") // Vol
               << new QStandardItem("0") // ChngInOI
               << new QStandardItem("0") // OI
               << putCheckbox;

        for (int i=0; i<putRow.size()-1; ++i) putRow[i]->setTextAlignment(Qt::AlignCenter);
        m_putModel->appendRow(putRow);
    }
    
    // Set ATM (approximate based on first futures or spot if available, otherwise middle)
    // For now, pick middle strike
    if (!m_strikes.isEmpty()) {
        m_atmStrike = m_strikes[m_strikes.size() / 2];
        highlightATMStrike();
    }
}

void OptionChainWindow::onTickUpdate(const XTS::Tick &tick)
{
    if (!m_tokenToStrike.contains(tick.exchangeInstrumentID)) return;
    
    double strike = m_tokenToStrike[tick.exchangeInstrumentID];
    OptionStrikeData& data = m_strikeData[strike];
    
    // Determine if Call or Put
    bool isCall = (tick.exchangeInstrumentID == data.callToken);
    // bool isPut = (tick.exchangeInstrumentID == data.putToken); // Implicit
    
    // Update fields
    // Update fields
    if (isCall) {
        if (tick.lastTradedPrice > 0) {
            data.callLTP = tick.lastTradedPrice;
            // Only update Change if we have a valid LTP and Close
            if (tick.close > 0) {
                 data.callChng = tick.lastTradedPrice - tick.close;
            }
        }
        
        // Depth / Quote Updates
        if (tick.bidPrice > 0) data.callBid = tick.bidPrice;
        if (tick.askPrice > 0) data.callAsk = tick.askPrice;
        if (tick.bidQuantity > 0) data.callBidQty = tick.bidQuantity;
        if (tick.askQuantity > 0) data.callAskQty = tick.askQuantity;
        
        // Statistics Updates
        if (tick.volume > 0) data.callVolume = tick.volume;
        if (tick.openInterest > 0) data.callOI = tick.openInterest;
        
    } else {
        if (tick.lastTradedPrice > 0) {
            data.putLTP = tick.lastTradedPrice;
             if (tick.close > 0) {
                 data.putChng = tick.lastTradedPrice - tick.close;
            }
        }
        
        if (tick.bidPrice > 0) data.putBid = tick.bidPrice;
        if (tick.askPrice > 0) data.putAsk = tick.askPrice;
        if (tick.bidQuantity > 0) data.putBidQty = tick.bidQuantity;
        if (tick.askQuantity > 0) data.putAskQty = tick.askQuantity;
        
        if (tick.volume > 0) data.putVolume = tick.volume;
        if (tick.openInterest > 0) data.putOI = tick.openInterest;
    }
    
    // Trigger visual update
    updateStrikeData(strike, data);
}

void OptionChainWindow::populateSymbols()
{
    m_symbolCombo->clear();
    
    RepositoryManager* repo = RepositoryManager::getInstance();
    QSet<QString> symbols;
    
    // 1. Get Index Futures (NIFTY, BANKNIFTY, FINNIFTY, etc.)
    QVector<ContractData> indices = repo->getScrips("NSE", "FO", "FUTIDX");
    for (const auto& contract : indices) {
        symbols.insert(contract.name);
    }
    
    // 2. Get Stock Futures (RELIANCE, TCS, etc.)
    // Note: Some stocks might have options but no futures (rare), or vice versa.
    // Checking FUTSTK is a safe proxy for F&O stocks.
    QVector<ContractData> stocks = repo->getScrips("NSE", "FO", "FUTSTK");
    for (const auto& contract : stocks) {
        symbols.insert(contract.name);
    }
    
    // Also check BSE if NSE is empty (or merge)
    if (symbols.isEmpty()) {
       QVector<ContractData> bseIndices = repo->getScrips("BSE", "FO", "FUTIDX");
       for (const auto& contract : bseIndices) symbols.insert(contract.name);
    }

    // Sort
    QStringList sortedSymbols = symbols.values();
    std::sort(sortedSymbols.begin(), sortedSymbols.end());
    
    // Add to ComboBox
    m_symbolCombo->addItems(sortedSymbols);
    m_symbolCombo->setEditable(true); // Enable search/typing
    m_symbolCombo->setInsertPolicy(QComboBox::NoInsert); // Don't add user types to list if not present
    
    // Default Selection (NIFTY or first)
    int index = m_symbolCombo->findText("NIFTY");
    if (index >= 0) {
        m_symbolCombo->setCurrentIndex(index);
    } else if (!sortedSymbols.isEmpty()) {
        m_symbolCombo->setCurrentIndex(0);
    }
}

void OptionChainWindow::populateExpiries(const QString &symbol)
{
    m_expiryCombo->clear();
    if (symbol.isEmpty()) return;
    
    RepositoryManager* repo = RepositoryManager::getInstance();
    
    // Get all option contracts for this symbol
    // We can use getOptionChain which ideally returns options.
    QVector<ContractData> contracts = repo->getOptionChain("NSE", symbol);
    if (contracts.isEmpty()) {
        contracts = repo->getOptionChain("BSE", symbol);
    }
    
    QSet<QString> expirySet;
    for (const auto& contract : contracts) {
        if (!contract.expiryDate.isEmpty()) {
            expirySet.insert(contract.expiryDate);
        }
    }
    
    if (expirySet.isEmpty()) return;
    
    // Sort Expiries (Need Date Parsing)
    // Formats: "26DEC2025", "02JAN2026" (DDMMMYYYY)
    QList<QDate> sortedDates;
    QMap<QDate, QString> dateToString;
    
    for (const QString& exp : expirySet) {
        // Try parsing DDMMMYYYY (e.g., 01JAN2024 or 1JAN2024)
        // Note: QDate format MMM is usually short month name (Jan/Feb).
        // XTS Expiry is usually upper case "26DEC2025". QDate might need Title Case "26Dec2025".
        
        QString cleanExp = exp;
        // Basic normalization: valid for English locale if title case?
        // Actually custom parsing might be safer if format is strict.
        // Let's rely on QLocale::system().toDate or format string.
        
        // Convert "DEC" to "Dec" for parsing if needed
         if (cleanExp.length() >= 3) {
             // Lowercase all then title case month?
             // Simple hack: Titlecase the month part?
             // "26DEC2024" -> "26Dec2024"
             // Implementation details depend on Qt version quirks, but "ddMMMyyyy" expects "Dec".
             
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
                     qDebug() << "Failed to parse expiry:" << exp << " Cleaned:" << parseable;
                     // Fallback: just add to list?
                }
             }
         }
    }
    
    std::sort(sortedDates.begin(), sortedDates.end());
    
    for (const QDate& d : sortedDates) {
        m_expiryCombo->addItem(dateToString[d]);
    }
    
    if (m_expiryCombo->count() > 0) {
        m_expiryCombo->setCurrentIndex(0); // Select nearest expiry
    }
}
