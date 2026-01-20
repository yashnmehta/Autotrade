#include "ui/ATMWatchWindow.h"
#include <QHeaderView>
#include <QScrollBar>
#include <QEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QtConcurrent>
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include "data/PriceStoreGateway.h"

ATMWatchWindow::ATMWatchWindow(QWidget *parent) : QWidget(parent) {
    setupUI();
    setupModels();
    setupConnections();
    
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
    mainLayout->setSpacing(0);

    QHBoxLayout *tableLayout = new QHBoxLayout();
    tableLayout->setSpacing(0);

    auto setupTable = [this](QTableView* table) {
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
            "QTableView { background-color: #1E2A38; color: #FFFFFF; gridline-color: #2A3A50; border: 1px solid #3A4A60; }"
            "QHeaderView::section { background-color: #2A3A50; color: #FFFFFF; padding: 4px; border: 1px solid #3A4A60; font-weight: bold; }"
        );
    };

    m_callTable = new QTableView();
    m_symbolTable = new QTableView();
    m_putTable = new QTableView();

    setupTable(m_callTable);
    setupTable(m_symbolTable);
    setupTable(m_putTable);

    // Symbols table (Middle) should show vertical scrollbar
    m_symbolTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_symbolTable->setStyleSheet(m_symbolTable->styleSheet() + "QTableView { color: #FFFF00; }");

    tableLayout->addWidget(m_callTable, 2);
    tableLayout->addWidget(m_symbolTable, 1);
    tableLayout->addWidget(m_putTable, 2);

    mainLayout->addLayout(tableLayout);
    setStyleSheet("QWidget { background-color: #1E2A38; }");
}

void ATMWatchWindow::setupModels() {
    m_callModel = new QStandardItemModel(this);
    m_callModel->setColumnCount(CALL_COUNT);
    m_callModel->setHorizontalHeaderLabels({"LTP", "Chg", "Bid", "Ask", "Vol", "OI"});
    m_callTable->setModel(m_callModel);
    m_callTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    m_symbolModel = new QStandardItemModel(this);
    m_symbolModel->setColumnCount(SYM_COUNT);
    m_symbolModel->setHorizontalHeaderLabels({"Symbol", "Spot/Fut", "ATM", "Expiry"});
    m_symbolTable->setModel(m_symbolModel);
    m_symbolTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    m_putModel = new QStandardItemModel(this);
    m_putModel->setColumnCount(PUT_COUNT);
    m_putModel->setHorizontalHeaderLabels({"Bid", "Ask", "LTP", "Chg", "Vol", "OI"});
    m_putTable->setModel(m_putModel);
    m_putTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void ATMWatchWindow::setupConnections() {
    connect(ATMWatchManager::getInstance(), &ATMWatchManager::atmUpdated, this, &ATMWatchWindow::onATMUpdated);
    
    // Sync scrolling using the middle table as master
    connect(m_symbolTable->verticalScrollBar(), &QScrollBar::valueChanged, this, &ATMWatchWindow::synchronizeScrollBars);
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
    FeedHandler& feed = FeedHandler::instance();

    int row = 0;
    for (const auto& info : atmList) {
        if (!info.isValid) continue;

        m_callModel->insertRow(row);
        m_symbolModel->insertRow(row);
        m_putModel->insertRow(row);

        m_symbolToRow[info.symbol] = row;
        
        // Populate Symbol Table
        m_symbolModel->setData(m_symbolModel->index(row, SYM_NAME), info.symbol);
        m_symbolModel->setData(m_symbolModel->index(row, SYM_PRICE), QString::number(info.basePrice, 'f', 2));
        m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM), QString::number(info.atmStrike, 'f', 2));
        m_symbolModel->setData(m_symbolModel->index(row, SYM_EXPIRY), info.expiry);

        // Initial Data for Options
        auto setupInitialOptionData = [&](int64_t token, bool isCall) {
            if (token <= 0) return;
            m_tokenToInfo[token] = {info.symbol, isCall};
            
            // Subscribe to live feed
            feed.subscribe(2, token, this, &ATMWatchWindow::onTickUpdate);

            // Fetch current price from cache
            if (auto state = MarketData::PriceStoreGateway::instance().getUnifiedState(2, token)) {
                UDP::MarketTick tick;
                tick.token = static_cast<uint32_t>(token);
                tick.ltp = state->ltp;
                tick.bids[0].price = state->bids[0].price;
                tick.asks[0].price = state->asks[0].price;
                tick.volume = state->volume;
                tick.openInterest = state->openInterest;
                onTickUpdate(tick);
            }
        };

        setupInitialOptionData(info.callToken, true);
        setupInitialOptionData(info.putToken, false);

        row++;
    }
}

void ATMWatchWindow::onATMUpdated() {
    refreshData();
}

void ATMWatchWindow::onTickUpdate(const UDP::MarketTick &tick) {
    if (!m_tokenToInfo.contains(tick.token)) return;
    
    auto info = m_tokenToInfo[tick.token];
    int row = m_symbolToRow.value(info.first, -1);
    if (row < 0) return;

    if (info.second) { // Call
        m_callModel->setData(m_callModel->index(row, CALL_LTP), QString::number(tick.ltp, 'f', 2));
        m_callModel->setData(m_callModel->index(row, CALL_BID), QString::number(tick.bestBid(), 'f', 2));
        m_callModel->setData(m_callModel->index(row, CALL_ASK), QString::number(tick.bestAsk(), 'f', 2));
        m_callModel->setData(m_callModel->index(row, CALL_VOL), QString::number(tick.volume));
        m_callModel->setData(m_callModel->index(row, CALL_OI), QString::number(tick.openInterest));
    } else { // Put
        m_putModel->setData(m_putModel->index(row, PUT_LTP), QString::number(tick.ltp, 'f', 2));
        m_putModel->setData(m_putModel->index(row, PUT_BID), QString::number(tick.bestBid(), 'f', 2));
        m_putModel->setData(m_putModel->index(row, PUT_ASK), QString::number(tick.bestAsk(), 'f', 2));
        m_putModel->setData(m_putModel->index(row, PUT_VOL), QString::number(tick.volume));
        m_putModel->setData(m_putModel->index(row, PUT_OI), QString::number(tick.openInterest));
    }
}

void ATMWatchWindow::synchronizeScrollBars(int value) {
    m_callTable->verticalScrollBar()->setValue(value);
    m_putTable->verticalScrollBar()->setValue(value);
}

void ATMWatchWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // Trigger immediate calculation when window is shown (Async to prevent UI hang)
    QtConcurrent::run([]() {
        ATMWatchManager::getInstance()->calculateAll();
    });
}

bool ATMWatchWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::Wheel) {
        if (obj == m_callTable->viewport() || obj == m_putTable->viewport() || obj == m_symbolTable->viewport()) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
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
