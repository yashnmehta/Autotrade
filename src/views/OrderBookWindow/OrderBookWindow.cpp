#include "views/OrderBookWindow.h"
#include "core/widgets/CustomOrderBook.h"
#include "services/TradingDataService.h"
#include "api/XTSTypes.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QDebug>
#include <QMenu>
#include <QSortFilterProxyModel>

OrderBookWindow::OrderBookWindow(TradingDataService* tradingDataService, QWidget *parent)
    : BaseBookWindow("OrderBook", parent), m_tradingDataService(tradingDataService) 
{
    setupUI();
    loadInitialProfile();
    setupConnections();
    
    m_instrumentFilter = m_statusFilter = m_buySellFilter = m_exchangeFilter = m_orderTypeFilter = "All";
    m_fromTime = QDateTime::currentDateTime().addDays(-7);
    m_toTime = QDateTime::currentDateTime();

    if (m_tradingDataService) {
        connect(m_tradingDataService, &TradingDataService::ordersUpdated, this, &OrderBookWindow::onOrdersUpdated);
        onOrdersUpdated(m_tradingDataService->getOrders());
    }
    
    connect(m_filterShortcut, &QShortcut::activated, this, &OrderBookWindow::toggleFilterRow);
}

OrderBookWindow::~OrderBookWindow() {}

void OrderBookWindow::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0); mainLayout->setSpacing(0);
    mainLayout->addWidget(createFilterWidget());
    setupTable();
    mainLayout->addWidget(m_tableView, 1);
    mainLayout->addWidget(createSummaryWidget());
}

QWidget* OrderBookWindow::createFilterWidget() {
    QWidget *container = new QWidget(this);
    container->setObjectName("filterContainer");
    container->setStyleSheet("QWidget#filterContainer { background-color: #2d2d2d; border-bottom: 1px solid #3f3f46; } QLabel { color: #d4d4d8; font-size: 11px; } QDateTimeEdit, QComboBox { background-color: #3f3f46; color: #ffffff; border: 1px solid #52525b; border-radius: 3px; font-size: 11px; } QPushButton { border-radius: 3px; font-weight: 600; font-size: 11px; padding: 5px 12px; }");
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(8);
    QHBoxLayout *filterLayout = new QHBoxLayout();
    
    auto addCombo = [&](const QString &l, QComboBox* &c, const QStringList &i) {
        QVBoxLayout *v = new QVBoxLayout(); v->addWidget(new QLabel(l)); c = new QComboBox(); c->addItems(i); v->addWidget(c); filterLayout->addLayout(v);
    };
    addCombo("Instrument", m_instrumentTypeCombo, {"All", "NSE OPT", "NSE FUT", "NSE EQ"});
    addCombo("Status", m_statusCombo, {"All", "Open", "Filled", "Cancelled", "Rejected"});
    addCombo("Buy/Sell", m_buySellCombo, {"All", "Buy", "Sell"});
    addCombo("Exchange", m_exchangeCombo, {"All", "NSE", "BSE"});
    addCombo("Order Type", m_orderTypeCombo, {"All", "Market", "Limit"});
    filterLayout->addStretch();
    
    m_applyFilterBtn = new QPushButton("Apply"); m_applyFilterBtn->setStyleSheet("background-color: #16a34a; color: white;");
    m_clearFilterBtn = new QPushButton("Clear"); m_clearFilterBtn->setStyleSheet("background-color: #52525b; color: white;");
    m_exportBtn = new QPushButton("Export"); m_exportBtn->setStyleSheet("background-color: #d97706; color: white;");
    filterLayout->addWidget(m_applyFilterBtn); filterLayout->addWidget(m_clearFilterBtn); filterLayout->addWidget(m_exportBtn);
    mainLayout->addLayout(filterLayout);
    return container;
}

QWidget* OrderBookWindow::createSummaryWidget() {
    QWidget *sw = new QWidget(); sw->setStyleSheet("background-color: #f5f5f5; border-top: 1px solid #ccc;"); sw->setFixedHeight(32);
    QHBoxLayout *l = new QHBoxLayout(sw); m_summaryLabel = new QLabel(); l->addWidget(m_summaryLabel); l->addStretch(); return sw;
}

void OrderBookWindow::setupTable() {
    m_tableView = new CustomOrderBook(this);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        menu.addAction("Modify Order", this, &OrderBookWindow::onModifyOrder);
        menu.addAction("Cancel Order", this, &OrderBookWindow::onCancelOrder);
        menu.addSeparator();
        menu.addAction("Export to CSV", this, &OrderBookWindow::exportToCSV);
        menu.addAction("Column Profile...", this, &OrderBookWindow::showColumnProfileDialog);
        menu.exec(m_tableView->viewport()->mapToGlobal(pos));
    });
    OrderModel* model = new OrderModel(this);
    m_model = model;
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_tableView->setModel(m_proxyModel);
}

void OrderBookWindow::onOrdersUpdated(const QVector<XTS::Order>& orders) { 
    m_allOrders = orders; 
    applyFilterToModel(); 
}

void OrderBookWindow::applyFilters() {
    m_instrumentFilter = m_instrumentTypeCombo->currentText();
    m_statusFilter = m_statusCombo->currentText();
    m_buySellFilter = m_buySellCombo->currentText();
    m_exchangeFilter = m_exchangeCombo->currentText();
    m_orderTypeFilter = m_orderTypeCombo->currentText();
    applyFilterToModel();
}

void OrderBookWindow::clearFilters() {
    m_instrumentTypeCombo->setCurrentIndex(0); 
    m_statusCombo->setCurrentIndex(0); 
    m_buySellCombo->setCurrentIndex(0);
    m_exchangeCombo->setCurrentIndex(0); 
    m_orderTypeCombo->setCurrentIndex(0);
    applyFilters();
}

void OrderBookWindow::applyFilterToModel() {
    QVector<XTS::Order> filtered;
    for (const auto& o : m_allOrders) {
        bool v = true;
        if (m_instrumentFilter != "All" && !o.tradingSymbol.contains(m_instrumentFilter, Qt::CaseInsensitive)) v = false;
        if (v && m_statusFilter != "All" && !o.orderStatus.contains(m_statusFilter, Qt::CaseInsensitive)) v = false;
        if (v && m_buySellFilter != "All" && o.orderSide.toUpper() != m_buySellFilter.toUpper()) v = false;
        
        if (v && !m_columnFilters.isEmpty()) {
            for (auto it = m_columnFilters.constBegin(); it != m_columnFilters.constEnd(); ++it) {
                // column specific filtering...
            }
        }
        if (v) filtered.append(o);
    }
    static_cast<OrderModel*>(m_model)->setOrders(filtered); 
    updateSummary();
}

void OrderBookWindow::updateSummary() {
    m_totalOrders = m_model->rowCount() - (m_filterRowVisible ? 1 : 0);
    m_summaryLabel->setText(QString("Total: %1").arg(m_totalOrders));
}

void OrderBookWindow::onColumnFilterChanged(int c, const QStringList& s) { 
    if (c == -1) m_columnFilters.clear(); else if (s.isEmpty()) m_columnFilters.remove(c); else m_columnFilters[c] = s; 
    applyFilterToModel(); 
}

void OrderBookWindow::toggleFilterRow() { BaseBookWindow::toggleFilterRow(m_model, m_tableView); }
void OrderBookWindow::exportToCSV() { BaseBookWindow::exportToCSV(m_model, m_tableView); }
void OrderBookWindow::refreshOrders() { if (m_tradingDataService) onOrdersUpdated(m_tradingDataService->getOrders()); }
void OrderBookWindow::onModifyOrder() { qDebug() << "Modify order"; }
void OrderBookWindow::onCancelOrder() { qDebug() << "Cancel order"; }

// setX methods...
void OrderBookWindow::setInstrumentFilter(const QString& i) { m_instrumentTypeCombo->setCurrentText(i); applyFilters(); }
void OrderBookWindow::setTimeFilter(const QDateTime& f, const QDateTime& t) { applyFilters(); }
void OrderBookWindow::setStatusFilter(const QString& s) { m_statusCombo->setCurrentText(s); applyFilters(); }
void OrderBookWindow::setOrderTypeFilter(const QString& o) { m_orderTypeCombo->setCurrentText(o); applyFilters(); }
