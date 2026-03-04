#include "views/OrderBookWindow.h"
#include "ui_OrderBookWindow.h"
#include "core/widgets/CustomOrderBook.h"
#include "services/TradingDataService.h"
#include "utils/PreferencesManager.h"
#include "utils/WindowSettingsHelper.h"
#include "api/xts/XTSTypes.h"
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QDebug>
#include <QMenu>
#include <QKeyEvent>
#include "models/qt/PinnedRowProxyModel.h"

OrderBookWindow::OrderBookWindow(TradingDataService* tradingDataService, QWidget *parent)
    : BaseBookWindow("OrderBook", parent),
      ui(new Ui::OrderBookWindow),
      m_tradingDataService(tradingDataService) 
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

    // Restore saved runtime state (combo selections, geometry)
    WindowSettingsHelper::loadAndApplyWindowSettings(this, "OrderBook");
}

OrderBookWindow::~OrderBookWindow() { delete ui; }

void OrderBookWindow::setupUI() {
    ui->setupUi(this);

    // Assign convenience pointers from ui
    m_instrumentTypeCombo = ui->m_instrumentTypeCombo;
    m_statusCombo         = ui->m_statusCombo;
    m_buySellCombo        = ui->m_buySellCombo;
    m_exchangeCombo       = ui->m_exchangeCombo;
    m_orderTypeCombo      = ui->m_orderTypeCombo;
    m_applyFilterBtn      = ui->m_applyFilterBtn;
    m_clearFilterBtn      = ui->m_clearFilterBtn;
    m_exportBtn           = ui->m_exportBtn;
    m_summaryLabel        = ui->m_summaryLabel;

    // Replace placeholder table with real CustomOrderBook
    setupTable();
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(layout());
    if (mainLayout) {
        mainLayout->replaceWidget(ui->m_tableViewPlaceholder, m_tableView);
        ui->m_tableViewPlaceholder->hide();
        ui->m_tableViewPlaceholder->deleteLater();
    }
}

void OrderBookWindow::setupTable() {
    m_tableView = new CustomOrderBook(this);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
        showOrderBookContextMenu(pos);
    });
    // Unified: same context menu when right-clicking the header
    m_tableView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView->horizontalHeader(), &QHeaderView::customContextMenuRequested, this,
            [this](const QPoint &headerPos) {
                QPoint viewportPos = m_tableView->viewport()->mapFromGlobal(
                    m_tableView->horizontalHeader()->mapToGlobal(headerPos));
                showOrderBookContextMenu(viewportPos);
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

void OrderBookWindow::showOrderBookContextMenu(const QPoint &pos) {
    QMenu menu(this);
    menu.addAction("Column Profile...", this, &OrderBookWindow::showColumnProfileDialog);
    menu.addSeparator();
    menu.addAction("Modify Order (Shift+F2)", this, &OrderBookWindow::onModifyOrder);
    menu.addAction("Cancel Order (Delete)", this, &OrderBookWindow::onCancelOrder);
    menu.addSeparator();
    menu.addAction("Export to CSV", this, &OrderBookWindow::exportToCSV);
    menu.addAction("Copy", this, [this]() {
        // TODO: copy selected rows
    });
    menu.addSeparator();
    menu.addAction("Refresh", this, &OrderBookWindow::refreshOrders);
    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}
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
    
    // Safety check for mass window opening
    if (selectedOrders.size() > 5) {
        // First check if they are batch-compatible
        const XTS::Order &first = selectedOrders.first();
        bool compatible = true;
        for (int i = 1; i < selectedOrders.size(); ++i) {
             if (selectedOrders[i].exchangeSegment != first.exchangeSegment ||
                selectedOrders[i].exchangeInstrumentID != first.exchangeInstrumentID ||
                selectedOrders[i].productType != first.productType ||
                selectedOrders[i].timeInForce != first.timeInForce ||
                selectedOrders[i].orderSide != first.orderSide) {
                compatible = false;
                break;
            }
        }
        
        if (!compatible) {
            auto reply = QMessageBox::question(this, "Confirmation", 
                QString("You are about to open %1 modification windows.\nAre you sure?").arg(selectedOrders.size()),
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No) return;
        }
    }
    
    // Filter out invalid orders (Closed/Rejected/etc)
    QVector<XTS::Order> validOrders;
    for (const auto& order : selectedOrders) {
        if (order.orderStatus == "Open" || order.orderStatus == "PartiallyFilled" || 
            order.orderStatus == "New" || order.orderStatus == "PendingNew") {
            validOrders.append(order);
        }
    }

    if (validOrders.isEmpty()) {
         QMessageBox::warning(this, "Modify Order", "No modifiable orders selected (only Open/PartiallyFilled allowed).");
         return;
    }

    // Smart Batch Logic
    if (validOrders.size() > 1) {
        const XTS::Order &first = validOrders.first();
        bool compatible = true;
        for (int i = 1; i < validOrders.size(); ++i) {
             if (validOrders[i].exchangeSegment != first.exchangeSegment ||
                validOrders[i].exchangeInstrumentID != first.exchangeInstrumentID ||
                validOrders[i].productType != first.productType ||
                validOrders[i].timeInForce != first.timeInForce ||
                validOrders[i].orderSide != first.orderSide) {
                compatible = false;
                break;
            }
        }
        
        if (compatible) {
            qDebug() << "[OrderBookWindow] Emitting batchModifyRequested for" << validOrders.size() << "orders.";
            emit batchModifyRequested(validOrders);
            return;
        }
    }
    
    // Fallback: Individual Modification
    for (const XTS::Order &order : validOrders) {
        qDebug() << "[OrderBookWindow] Emitting modifyOrderRequested for AppOrderID:" << order.appOrderID;
        emit modifyOrderRequested(order);
    }
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
        event->accept();
        return;
    }
    
    if (event->modifiers() & Qt::ShiftModifier) {
        if (event->key() == Qt::Key_F1 || event->key() == Qt::Key_F2) {
            event->accept(); // Consume early — all paths below handle the key
            QVector<XTS::Order> selectedOrders = getSelectedOrders();
            if (selectedOrders.isEmpty()) {
                QMessageBox::warning(this, "Modify Order", "Please select an order to modify.");
                return;
            }
            
            bool isBuyKey = (event->key() == Qt::Key_F1);
            QString expectedSide = isBuyKey ? "BUY" : "SELL";
            
            // Filter valid orders matching the key press side
            QVector<XTS::Order> validOrders;
            for (const auto& order : selectedOrders) {
                // Check status
                if (order.orderStatus != "Open" && order.orderStatus != "PartiallyFilled" && 
                    order.orderStatus != "New" && order.orderStatus != "PendingNew") {
                    continue;
                }
                // Check side
                if (order.orderSide.toUpper() != expectedSide) {
                    continue;
                }
                validOrders.append(order);
            }
            
            if (validOrders.isEmpty()) {
                QMessageBox::warning(this, "Modify Order", 
                    QString("No valid %1 orders selected for modification.").arg(expectedSide));
                return;
            }
            
            // Smart Batch Logic
            if (validOrders.size() > 1) {
                const XTS::Order &first = validOrders.first();
                bool compatible = true;
                for (int i = 1; i < validOrders.size(); ++i) {
                     if (validOrders[i].exchangeSegment != first.exchangeSegment ||
                        validOrders[i].exchangeInstrumentID != first.exchangeInstrumentID ||
                        validOrders[i].productType != first.productType ||
                        validOrders[i].timeInForce != first.timeInForce ||
                        validOrders[i].orderSide != first.orderSide) {
                        compatible = false;
                        break;
                    }
                }
                
                if (compatible) {
                    emit batchModifyRequested(validOrders);
                    return;
                }
            }
            
            // Fallback: Individual Modification
            if (validOrders.size() > 5) {
                auto reply = QMessageBox::question(this, "Confirmation", 
                    QString("You are about to open %1 modification windows.\nAre you sure?").arg(validOrders.size()),
                    QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::No) return;
            }
            
            for (const XTS::Order &order : validOrders) {
                emit modifyOrderRequested(order);
            }
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

