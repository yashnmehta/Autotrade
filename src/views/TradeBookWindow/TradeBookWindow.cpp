#include "views/TradeBookWindow.h"
#include "core/widgets/CustomTradeBook.h"
#include "services/TradingDataService.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QMenu>
#include <QDebug>
#include "models/PinnedRowProxyModel.h"

TradeBookWindow::TradeBookWindow(TradingDataService* tradingDataService, QWidget *parent)
    : BaseBookWindow("TradeBook", parent), m_tradingDataService(tradingDataService) 
{
    setupUI();
    loadInitialProfile();
    setupConnections();
    
    m_instrumentFilter = m_exchangeFilter = m_buySellFilter = m_orderTypeFilter = "All";
    m_fromTime = QDateTime::currentDateTime().addDays(-7);
    m_toTime = QDateTime::currentDateTime();

    if (m_tradingDataService) {
        connect(m_tradingDataService, &TradingDataService::tradesUpdated, this, &TradeBookWindow::onTradesUpdated);
        onTradesUpdated(m_tradingDataService->getTrades());
    }
    connect(m_filterShortcut, &QShortcut::activated, this, &TradeBookWindow::toggleFilterRow);
}

TradeBookWindow::~TradeBookWindow() {}

void TradeBookWindow::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0); mainLayout->setSpacing(0);
    mainLayout->addWidget(createFilterWidget());
    setupTable();
    mainLayout->addWidget(m_tableView, 1);
    mainLayout->addWidget(createSummaryWidget());
}

QWidget* TradeBookWindow::createFilterWidget() {
    QWidget *container = new QWidget(this);
    container->setObjectName("filterContainer");
    container->setStyleSheet("QWidget#filterContainer { background-color: #f8fafc; border-bottom: 1px solid #e2e8f0; } QLabel { color: #475569; font-size: 11px; } QDateTimeEdit, QComboBox { background-color: #ffffff; color: #1e293b; border: 1px solid #cbd5e1; border-radius: 4px; font-size: 11px; } QPushButton { border-radius: 4px; font-weight: 600; font-size: 11px; padding: 5px 12px; }");
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(8);
    QHBoxLayout *filterLayout = new QHBoxLayout();
    
    auto addCombo = [&](const QString &l, QComboBox* &c, const QStringList &i) {
        QVBoxLayout *v = new QVBoxLayout(); v->addWidget(new QLabel(l)); c = new QComboBox(); c->addItems(i); v->addWidget(c); filterLayout->addLayout(v);
    };
    addCombo("Instrument", m_instrumentTypeCombo, {"All", "NSE OPT", "NSE FUT", "NSE EQ"});
    addCombo("Exchange", m_exchangeCombo, {"All", "NSE", "BSE"});
    addCombo("Buy/Sell", m_buySellCombo, {"All", "Buy", "Sell"});
    addCombo("Order Type", m_orderTypeCombo, {"All", "Day", "IOC"});
    filterLayout->addStretch();
    
    m_applyFilterBtn = new QPushButton("Apply"); m_applyFilterBtn->setStyleSheet("background-color: #16a34a; color: white;");
    m_clearFilterBtn = new QPushButton("Clear"); m_clearFilterBtn->setStyleSheet("background-color: #f1f5f9; color: #475569; border: 1px solid #cbd5e1; border-radius: 4px;");
    m_exportBtn = new QPushButton("Export"); m_exportBtn->setStyleSheet("background-color: #d97706; color: white;");
    m_showSummaryCheck = new QCheckBox("Summary"); m_showSummaryCheck->setStyleSheet("color: #475569;");
    filterLayout->addWidget(m_showSummaryCheck); filterLayout->addWidget(m_applyFilterBtn); filterLayout->addWidget(m_clearFilterBtn); filterLayout->addWidget(m_exportBtn);
    mainLayout->addLayout(filterLayout);
    return container;
}

QWidget* TradeBookWindow::createSummaryWidget() {
    QWidget *sw = new QWidget(); sw->setStyleSheet("background-color: #f5f5f5; border-top: 1px solid #ccc;"); sw->setFixedHeight(32);
    QHBoxLayout *l = new QHBoxLayout(sw); m_summaryLabel = new QLabel(); l->addWidget(m_summaryLabel); l->addStretch(); return sw;
}

void TradeBookWindow::setupTable() {
    m_tableView = new CustomTradeBook(this);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        menu.addAction("Export to CSV", this, &TradeBookWindow::exportToCSV);
        menu.addAction("Refresh", this, &TradeBookWindow::refreshTrades);
        menu.addSeparator();
        menu.addAction("Column Profile...", this, &TradeBookWindow::showColumnProfileDialog);
        menu.exec(m_tableView->viewport()->mapToGlobal(pos));
    });
    TradeModel* model = new TradeModel(this);
    m_model = model;
    m_proxyModel = new PinnedRowProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_tableView->setModel(m_proxyModel);
}

void TradeBookWindow::onTradesUpdated(const QVector<XTS::Trade>& trades) { m_allTrades = trades; applyFilterToModel(); }

void TradeBookWindow::applyFilters() {
    m_instrumentFilter = m_instrumentTypeCombo->currentText();
    m_exchangeFilter = m_exchangeCombo->currentText();
    m_buySellFilter = m_buySellCombo->currentText();
    m_orderTypeFilter = m_orderTypeCombo->currentText();
    applyFilterToModel();
}

void TradeBookWindow::clearFilters() {
    m_instrumentTypeCombo->setCurrentIndex(0); m_exchangeCombo->setCurrentIndex(0); m_buySellCombo->setCurrentIndex(0); m_orderTypeCombo->setCurrentIndex(0);
    applyFilters();
}

void TradeBookWindow::applyFilterToModel() {
    QVector<XTS::Trade> filtered;
    for (const auto& t : m_allTrades) {
        bool v = true;
        if (m_instrumentFilter != "All" && !t.tradingSymbol.contains(m_instrumentFilter, Qt::CaseInsensitive)) v = false;
        if (v && m_exchangeFilter != "All" && !t.exchangeSegment.contains(m_exchangeFilter, Qt::CaseInsensitive)) v = false;
        if (v && m_buySellFilter != "All" && t.orderSide.toUpper() != m_buySellFilter.toUpper()) v = false;
        
        if (v && !m_columnFilters.isEmpty()) {
            // column specific filtering...
        }
        
        // Text filters (inline search)
        if (v && !m_textFilters.isEmpty()) {
            for (auto it = m_textFilters.begin(); it != m_textFilters.end(); ++it) {
                int col = it.key();
                const QString& filterText = it.value();
                if (filterText.isEmpty()) continue;
                
                QString val;
                bool hasMapping = true;
                switch (col) {
                    case TradeModel::Symbol: val = t.tradingSymbol; break;
                    case TradeModel::ExchangeCode: val = t.exchangeSegment; break;
                    case TradeModel::BuySell: val = t.orderSide; break;
                    case TradeModel::OrderType: val = t.orderType; break;
                    case TradeModel::ExchOrdNo: val = t.exchangeOrderID; break;
                    case TradeModel::Client: val = t.clientID; break;
                    case TradeModel::InstrumentName: val = t.tradingSymbol; break;
                    case TradeModel::Code: val = QString::number(t.exchangeInstrumentID); break;
                    default: hasMapping = false; break;
                }
                
                // Only filter if we have a mapping for this column
                if (hasMapping && !val.contains(filterText, Qt::CaseInsensitive)) {
                    v = false;
                    break;
                }
            }
        }
        
        if (v) filtered.append(t);
    }
    static_cast<TradeModel*>(m_model)->setTrades(filtered); 
    updateSummary();
}


void TradeBookWindow::updateSummary() {
    int total = m_model->rowCount() - (m_filterRowVisible ? 1 : 0);
    m_summaryLabel->setText(QString("Trades: %1").arg(total));
}

void TradeBookWindow::onColumnFilterChanged(int c, const QStringList& s) { 
    if (c == -1) m_columnFilters.clear(); else if (s.isEmpty()) m_columnFilters.remove(c); else m_columnFilters[c] = s; 
    applyFilterToModel(); 
}

void TradeBookWindow::onTextFilterChanged(int col, const QString& text) {
    BaseBookWindow::onTextFilterChanged(col, text);
    applyFilterToModel();
}

void TradeBookWindow::toggleFilterRow() { BaseBookWindow::toggleFilterRow(m_model, m_tableView); }
void TradeBookWindow::exportToCSV() { BaseBookWindow::exportToCSV(m_model, m_tableView); }
void TradeBookWindow::refreshTrades() { if (m_tradingDataService) onTradesUpdated(m_tradingDataService->getTrades()); }

