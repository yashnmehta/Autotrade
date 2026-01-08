#include "views/OrderBookWindow.h"
#include "core/widgets/CustomOrderBook.h"
#include "services/TradingDataService.h"
#include "utils/PreferencesManager.h"
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
#include <QKeyEvent>
#include "models/PinnedRowProxyModel.h"

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
    
    // Load default status filter from preferences AFTER UI is setup
    QString defaultStatus = PreferencesManager::instance().value("General/OrderBookDefaultStatus", "All").toString();
    qDebug() << "[OrderBookWindow] Loading default status from preferences:" << defaultStatus;
    
    if (!defaultStatus.isEmpty() && m_statusCombo) {
        int index = m_statusCombo->findText(defaultStatus);
        qDebug() << "[OrderBookWindow] Found index for status" << defaultStatus << "=" << index;
        
        if (index >= 0) {
            m_statusCombo->setCurrentIndex(index);
            m_statusFilter = defaultStatus;
            qDebug() << "[OrderBookWindow] Applied default status filter:" << m_statusFilter;
            applyFilterToModel();  // Apply the filter immediately
        } else {
            qDebug() << "[OrderBookWindow] ERROR: Status" << defaultStatus << "not found in dropdown!";
        }
    } else {
        qDebug() << "[OrderBookWindow] Using default 'All' status (combo exists:" << (m_statusCombo != nullptr) << ")";
    }
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
    addCombo("Status", m_statusCombo, {"All", "Pending", "Unconfirmed", "Open", "Filled", "Executed", "Success", "Cancelled", "Rejected", "Failed", "Admin pending", "min/admin Pending"});
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
        menu.addAction("Modify Order (Shift+F2)", this, &OrderBookWindow::onModifyOrder);
        menu.addAction("Cancel Order (Delete)", this, &OrderBookWindow::onCancelOrder);
        menu.addSeparator();
        menu.addAction("Export to CSV", this, &OrderBookWindow::exportToCSV);
        menu.addAction("Column Profile...", this, &OrderBookWindow::showColumnProfileDialog);
        menu.exec(m_tableView->viewport()->mapToGlobal(pos));
    });
    OrderModel* model = new OrderModel(this);
    m_model = model;
    m_proxyModel = new PinnedRowProxyModel(this);
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
    qDebug() << "[OrderBookWindow] applyFilterToModel: m_allOrders:" << m_allOrders.size() 
             << "m_statusFilter:" << m_statusFilter;
    
    QVector<XTS::Order> filtered;
    for (const auto& o : m_allOrders) {
        bool v = true;
        if (m_instrumentFilter != "All" && !o.tradingSymbol.contains(m_instrumentFilter, Qt::CaseInsensitive)) v = false;
        
        if (v && m_statusFilter != "All") {
            bool statusMatches = o.orderStatus.contains(m_statusFilter, Qt::CaseInsensitive);
            if (!statusMatches) {
                qDebug() << "[OrderBookWindow] Filtering out order - Status:" << o.orderStatus 
                         << "doesn't match filter:" << m_statusFilter;
                v = false;
            }
        }
        
        if (v && m_buySellFilter != "All" && o.orderSide.toUpper() != m_buySellFilter.toUpper()) v = false;
        
        if (v && !m_columnFilters.isEmpty()) {
            for (auto it = m_columnFilters.constBegin(); it != m_columnFilters.constEnd(); ++it) {
                // column specific filtering...
            }
        }
        
        // Text filters (inline search)
        if (v && !m_textFilters.isEmpty()) {
            qDebug() << "[OrderBookWindow] Checking text filters for order:" << o.tradingSymbol;
            for (auto it = m_textFilters.begin(); it != m_textFilters.end(); ++it) {
                int col = it.key();
                const QString& filterText = it.value();
                if (filterText.isEmpty()) continue;
                
                QString val;
                bool hasMapping = true;
                switch (col) {
                    case OrderModel::Symbol: val = o.tradingSymbol; break;
                    case OrderModel::ExchangeCode: val = o.exchangeSegment; break;
                    case OrderModel::BuySell: val = o.orderSide; break;
                    case OrderModel::Status: val = o.orderStatus; break;
                    case OrderModel::ExchOrdNo: val = o.exchangeOrderID; break;
                    case OrderModel::Client: val = o.clientID; break;
                    case OrderModel::OrderType: val = o.orderType; break;
                    case OrderModel::User: val = o.loginID; break;
                    case OrderModel::Code: val = QString::number(o.exchangeInstrumentID); break;
                    case OrderModel::InstrumentName: val = o.tradingSymbol; break;
                    case OrderModel::ScripName: val = o.tradingSymbol; break;
                    default: hasMapping = false; break;
                }
                
                // Only filter if we have a mapping for this column
                if (hasMapping && !val.contains(filterText, Qt::CaseInsensitive)) {
                    v = false;
                    break;
                }
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

void OrderBookWindow::onTextFilterChanged(int col, const QString& text) {
    qDebug() << "[OrderBookWindow] onTextFilterChanged called: col=" << col << "text=" << text;
    BaseBookWindow::onTextFilterChanged(col, text);
    qDebug() << "[OrderBookWindow] Calling applyFilterToModel, m_textFilters size:" << m_textFilters.size();
    applyFilterToModel();
}

void OrderBookWindow::toggleFilterRow() { 
    OrderModel* om = qobject_cast<OrderModel*>(m_model);
    if (om) {
        om->setFilterRowVisible(!m_filterRowVisible);
    }
    BaseBookWindow::toggleFilterRow(m_model, m_tableView); 
}

void OrderBookWindow::exportToCSV() { BaseBookWindow::exportToCSV(m_model, m_tableView); }
void OrderBookWindow::refreshOrders() { if (m_tradingDataService) onOrdersUpdated(m_tradingDataService->getOrders()); }


bool OrderBookWindow::getSelectedOrder(XTS::Order &outOrder) const {
    QModelIndex index = m_tableView->currentIndex();
    if (!index.isValid()) return false;
    
    // Map through proxy to source
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    if (!sourceIndex.isValid()) return false;
    
    OrderModel* orderModel = qobject_cast<OrderModel*>(m_model);
    if (!orderModel) return false;
    
    // Skip filter row (row 0 when filter is visible)
    int row = sourceIndex.row();
    if (orderModel->isFilterRowVisible() && row == 0) return false;
    
    // Adjust row index when filter row is visible
    int dataRow = orderModel->isFilterRowVisible() ? row - 1 : row;
    if (dataRow < 0 || dataRow >= orderModel->getOrders().size()) return false;
    
    outOrder = orderModel->getOrderAt(dataRow);
    return true;
}

QVector<XTS::Order> OrderBookWindow::getSelectedOrders() const {
    QVector<XTS::Order> selectedOrders;
    
    QItemSelectionModel *selectionModel = m_tableView->selectionModel();
    if (!selectionModel) return selectedOrders;
    
    QModelIndexList selectedIndexes = selectionModel->selectedRows();
    
    OrderModel* orderModel = qobject_cast<OrderModel*>(m_model);
    if (!orderModel) return selectedOrders;
    
    for (const QModelIndex &index : selectedIndexes) {
        // Map through proxy to source
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        if (!sourceIndex.isValid()) continue;
        
        // Skip filter row (row 0 when filter is visible)
        int row = sourceIndex.row();
        if (orderModel->isFilterRowVisible() && row == 0) continue;
        
        // Adjust row index when filter row is visible
        int dataRow = orderModel->isFilterRowVisible() ? row - 1 : row;
        if (dataRow < 0 || dataRow >= orderModel->getOrders().size()) continue;
        
        selectedOrders.append(orderModel->getOrderAt(dataRow));
    }
    
    return selectedOrders;
}

void OrderBookWindow::onModifyOrder() {
    QVector<XTS::Order> selectedOrders = getSelectedOrders();
    if (selectedOrders.isEmpty()) {
        QMessageBox::warning(this, "Modify Order", "Please select an order to modify.");
        return;
    }
    
    if (selectedOrders.size() > 1) {
        QMessageBox::warning(this, "Modify Order", "Please select only one order to modify.");
        return;
    }
    
    XTS::Order order = selectedOrders.first();
    
    // Validate order status - can only modify open or partially filled orders
    if (order.orderStatus != "Open" && order.orderStatus != "PartiallyFilled" && 
        order.orderStatus != "New" && order.orderStatus != "PendingNew") {
        QMessageBox::warning(this, "Modify Order", 
            QString("Cannot modify order - Status: %1\nOnly Open or PartiallyFilled orders can be modified.")
                .arg(order.orderStatus));
        return;
    }
    
    qDebug() << "[OrderBookWindow] Emitting modifyOrderRequested for AppOrderID:" << order.appOrderID 
             << "Symbol:" << order.tradingSymbol << "Side:" << order.orderSide;
    
    emit modifyOrderRequested(order);
}

void OrderBookWindow::onCancelOrder() {
    QVector<XTS::Order> selectedOrders = getSelectedOrders();
    if (selectedOrders.isEmpty()) {
        QMessageBox::warning(this, "Cancel Order", "Please select an order to cancel.");
        return;
    }
    
    // Filter valid orders for cancellation
    QVector<XTS::Order> validOrders;
    for (const XTS::Order &order : selectedOrders) {
        if (order.orderStatus == "Open" || order.orderStatus == "PartiallyFilled" || 
            order.orderStatus == "New" || order.orderStatus == "PendingNew") {
            validOrders.append(order);
        }
    }
    
    if (validOrders.isEmpty()) {
        QMessageBox::warning(this, "Cancel Order", "No selected orders can be cancelled.\nOnly Open or PartiallyFilled orders can be cancelled.");
        return;
    }
    
    // Confirm cancellation
    QString confirmMsg;
    if (validOrders.size() == 1) {
        const XTS::Order &order = validOrders.first();
        confirmMsg = QString("Cancel order?\n\n"
                             "Symbol: %1\n"
                             "Side: %2\n"
                             "Qty: %3 (Filled: %4)\n"
                             "Price: %5")
                            .arg(order.tradingSymbol)
                            .arg(order.orderSide)
                            .arg(order.orderQuantity)
                            .arg(order.cumulativeQuantity)
                            .arg(order.orderPrice, 0, 'f', 2);
    } else {
        confirmMsg = QString("Cancel %1 selected orders?").arg(validOrders.size());
    }
    
    if (QMessageBox::question(this, "Confirm Cancellation", confirmMsg, 
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        for (const XTS::Order &order : validOrders) {
            qDebug() << "[OrderBookWindow] Emitting cancelOrderRequested for AppOrderID:" << order.appOrderID;
            emit cancelOrderRequested(order.appOrderID);
        }
    }
}

void OrderBookWindow::keyPressEvent(QKeyEvent *event) {
    // Shift+F1: Modify selected order (opens Buy window for BUY orders)
    // Shift+F2: Modify selected order (opens Sell window for SELL orders)  
    // Delete: Cancel selected order
    
    if (event->key() == Qt::Key_Delete) {
        onCancelOrder();
        return;
    }
    
    if (event->modifiers() & Qt::ShiftModifier) {
        if (event->key() == Qt::Key_F1 || event->key() == Qt::Key_F2) {
            QVector<XTS::Order> selectedOrders = getSelectedOrders();
            if (selectedOrders.isEmpty()) {
                QMessageBox::warning(this, "Modify Order", "Please select an order to modify.");
                return;
            }
            
            if (selectedOrders.size() > 1) {
                QMessageBox::warning(this, "Modify Order", "Please select only one order to modify.");
                return;
            }
            
            XTS::Order order = selectedOrders.first();
            
            // Validate order status before modification
            if (order.orderStatus != "Open" && order.orderStatus != "PartiallyFilled" && 
                order.orderStatus != "New" && order.orderStatus != "PendingNew") {
                QMessageBox::warning(this, "Modify Order", 
                    QString("Cannot modify order - Status: %1").arg(order.orderStatus));
                return;
            }
            
            // Shift+F1 for buy side, Shift+F2 for sell side
            bool isBuy = (event->key() == Qt::Key_F1);
            bool orderIsBuy = (order.orderSide.toUpper() == "BUY");
            
            if (isBuy != orderIsBuy) {
                QMessageBox::warning(this, "Modify Order", 
                    QString("Order side mismatch.\nOrder is %1 but Shift+%2 opens %3 window.\n\n"
                            "Use Shift+%4 instead.")
                        .arg(order.orderSide)
                        .arg(isBuy ? "F1" : "F2")
                        .arg(isBuy ? "Buy" : "Sell")
                        .arg(orderIsBuy ? "F1" : "F2"));
                return;
            }
            
            emit modifyOrderRequested(order);
            return;
        }
    }
    
    BaseBookWindow::keyPressEvent(event);
}

// setX methods...
void OrderBookWindow::setInstrumentFilter(const QString& i) { m_instrumentTypeCombo->setCurrentText(i); applyFilters(); }
void OrderBookWindow::setTimeFilter(const QDateTime& f, const QDateTime& t) { applyFilters(); }
void OrderBookWindow::setStatusFilter(const QString& s) { m_statusCombo->setCurrentText(s); applyFilters(); }
void OrderBookWindow::setOrderTypeFilter(const QString& o) { m_orderTypeCombo->setCurrentText(o); applyFilters(); }

