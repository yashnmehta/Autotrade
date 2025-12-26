#include "views/OrderBookWindow.h"
#include "core/widgets/CustomOrderBook.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QListWidget>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTimer>
#include <QDebug>

OrderBookWindow::OrderBookWindow(QWidget *parent)
    : QWidget(parent)
    , m_tableView(nullptr)
    , m_model(nullptr)
    , m_filterRowVisible(false)
    , m_filterShortcut(nullptr)
    , m_totalOrders(0)
    , m_openOrders(0)
    , m_executedOrders(0)
    , m_cancelledOrders(0)
    , m_totalOrderValue(0.0)
{
    setupUI();
    setupConnections();
    loadDummyData();  // TODO: Replace with XTS API call
    
    // Setup Ctrl+F shortcut for filter toggle
    m_filterShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(m_filterShortcut, &QShortcut::activated, this, &OrderBookWindow::toggleFilterRow);
    
    // Setup Esc shortcut to close parent MDI window
    m_escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(m_escShortcut, &QShortcut::activated, this, [this]() {
        // Find parent CustomMDISubWindow and close it
        QWidget *p = parentWidget();
        while (p) {
            if (p->metaObject()->className() == QString("CustomMDISubWindow")) {
                p->close();
                return;
            }
            p = p->parentWidget();
        }
        // Fallback: close this widget
        close();
    });
    
    // Set default focus to table for immediate keyboard navigation
    if (m_tableView) {
        QTimer::singleShot(0, this, [this]() {
            if (m_tableView) {
                m_tableView->setFocus();
            }
        });
    }
    
    qDebug() << "[OrderBookWindow] Created - Press Ctrl+F to toggle filters, Esc to close";
}

OrderBookWindow::~OrderBookWindow()
{
    qDebug() << "[OrderBookWindow] Destroyed";
}

void OrderBookWindow::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Filter bar at top
    QWidget *filterWidget = createFilterWidget();
    mainLayout->addWidget(filterWidget);

    // Order table
    setupTable();
    mainLayout->addWidget(m_tableView, 1);

    // Summary bar at bottom
    QWidget *summaryWidget = createSummaryWidget();
    mainLayout->addWidget(summaryWidget);
}

QWidget* OrderBookWindow::createFilterWidget()
{
    QGroupBox *filterGroup = new QGroupBox("Order Book Filters");
    filterGroup->setStyleSheet("QGroupBox { font-weight: bold; padding-top: 10px; background-color: #f5f5f5; }");
    
    QHBoxLayout *filterLayout = new QHBoxLayout(filterGroup);
    filterLayout->setContentsMargins(10, 15, 10, 10);
    filterLayout->setSpacing(10);

    // Time Range
    QLabel *lblFrom = new QLabel("From:");
    m_fromTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-7));
    m_fromTimeEdit->setDisplayFormat("dd-MM-yyyy HH:mm");
    m_fromTimeEdit->setCalendarPopup(true);
    m_fromTimeEdit->setMinimumWidth(150);

    QLabel *lblTo = new QLabel("To:");
    m_toTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime());
    m_toTimeEdit->setDisplayFormat("dd-MM-yyyy HH:mm");
    m_toTimeEdit->setCalendarPopup(true);
    m_toTimeEdit->setMinimumWidth(150);

    // Instrument Type
    QLabel *lblInstrument = new QLabel("Instrument:");
    m_instrumentTypeCombo = new QComboBox();
    m_instrumentTypeCombo->addItems({"All", "EQ", "FUT", "OPT", "INDEX"});
    m_instrumentTypeCombo->setMinimumWidth(100);

    // Exchange
    QLabel *lblExchange = new QLabel("Exchange:");
    m_exchangeCombo = new QComboBox();
    m_exchangeCombo->addItems({"All", "NSE", "BSE", "NFO", "MCX"});
    m_exchangeCombo->setMinimumWidth(100);

    // Buy/Sell
    QLabel *lblBuySell = new QLabel("B/S:");
    m_buySellCombo = new QComboBox();
    m_buySellCombo->addItems({"All", "BUY", "SELL"});
    m_buySellCombo->setMinimumWidth(80);

    // Status
    QLabel *lblStatus = new QLabel("Status:");
    m_statusCombo = new QComboBox();
    m_statusCombo->addItems({"All", "Open", "Executed", "Cancelled", "Rejected", "Pending"});
    m_statusCombo->setMinimumWidth(100);

    // Order Type
    QLabel *lblOrderType = new QLabel("Order Type:");
    m_orderTypeCombo = new QComboBox();
    m_orderTypeCombo->addItems({"All", "Market", "Limit", "SL", "SL-M"});
    m_orderTypeCombo->setMinimumWidth(100);

    // Buttons
    m_applyFilterBtn = new QPushButton("Apply");
    m_applyFilterBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 5px 15px; }");
    
    m_clearFilterBtn = new QPushButton("Clear");
    m_clearFilterBtn->setStyleSheet("QPushButton { background-color: #757575; color: white; padding: 5px 15px; }");

    m_cancelOrderBtn = new QPushButton("Cancel Order");
    m_cancelOrderBtn->setStyleSheet("QPushButton { background-color: #F44336; color: white; padding: 5px 15px; }");
    
    m_modifyOrderBtn = new QPushButton("Modify Order");
    m_modifyOrderBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; padding: 5px 15px; }");

    m_exportBtn = new QPushButton("Export");
    m_exportBtn->setStyleSheet("QPushButton { background-color: #FF9800; color: white; padding: 5px 15px; }");

    // Add widgets to layout
    filterLayout->addWidget(lblFrom);
    filterLayout->addWidget(m_fromTimeEdit);
    filterLayout->addWidget(lblTo);
    filterLayout->addWidget(m_toTimeEdit);
    filterLayout->addWidget(lblInstrument);
    filterLayout->addWidget(m_instrumentTypeCombo);
    filterLayout->addWidget(lblExchange);
    filterLayout->addWidget(m_exchangeCombo);
    filterLayout->addWidget(lblBuySell);
    filterLayout->addWidget(m_buySellCombo);
    filterLayout->addWidget(lblStatus);
    filterLayout->addWidget(m_statusCombo);
    filterLayout->addWidget(lblOrderType);
    filterLayout->addWidget(m_orderTypeCombo);
    filterLayout->addStretch();
    filterLayout->addWidget(m_applyFilterBtn);
    filterLayout->addWidget(m_clearFilterBtn);
    filterLayout->addWidget(m_cancelOrderBtn);
    filterLayout->addWidget(m_modifyOrderBtn);
    filterLayout->addWidget(m_exportBtn);

    return filterGroup;
}

QWidget* OrderBookWindow::createSummaryWidget()
{
    QWidget *summaryWidget = new QWidget();
    summaryWidget->setStyleSheet("background-color: #e1e1e1; padding: 5px;");
    summaryWidget->setFixedHeight(35);
    
    QHBoxLayout *summaryLayout = new QHBoxLayout(summaryWidget);
    summaryLayout->setContentsMargins(10, 5, 10, 5);
    
    QLabel *summaryLabel = new QLabel(QString("Total Orders: <b>%1</b> | Open: <b style='color: #2196F3;'>%2</b> | "
                                               "Executed: <b style='color: #4CAF50;'>%3</b> | Cancelled: <b style='color: #F44336;'>%4</b> | "
                                               "Total Value: <b>₹%5</b>")
                                       .arg(m_totalOrders)
                                       .arg(m_openOrders)
                                       .arg(m_executedOrders)
                                       .arg(m_cancelledOrders)
                                       .arg(m_totalOrderValue, 0, 'f', 2));
    summaryLayout->addWidget(summaryLabel);
    summaryLayout->addStretch();
    
    return summaryWidget;
}

void OrderBookWindow::setupTable()
{
    m_tableView = new CustomOrderBook(this);
    m_model = new QStandardItemModel(this);
    
    // 20 columns for Order Book
    QStringList headers = {
        "Order ID", "Time", "Exchange", "Segment", "Symbol", "Series/Expiry",
        "Instrument", "B/S", "Order Type", "Product", "Validity",
        "Qty", "Price", "Trigger Price", "Filled Qty", "Pending Qty",
        "Avg Price", "Status", "Order Value", "Message"
    };
    
    m_model->setHorizontalHeaderLabels(headers);
    m_tableView->setModel(m_model);
    
    // Set column widths
    m_tableView->setColumnWidth(0, 120);  // Order ID
    m_tableView->setColumnWidth(1, 140);  // Time
    m_tableView->setColumnWidth(2, 80);   // Exchange
    m_tableView->setColumnWidth(3, 80);   // Segment
    m_tableView->setColumnWidth(4, 150);  // Symbol
    m_tableView->setColumnWidth(5, 100);  // Series/Expiry
    m_tableView->setColumnWidth(6, 100);  // Instrument
    m_tableView->setColumnWidth(7, 60);   // B/S
    m_tableView->setColumnWidth(8, 100);  // Order Type
    m_tableView->setColumnWidth(9, 80);   // Product
    m_tableView->setColumnWidth(10, 80);  // Validity
    m_tableView->setColumnWidth(11, 80);  // Qty
    m_tableView->setColumnWidth(12, 100); // Price
    m_tableView->setColumnWidth(13, 100); // Trigger Price
    m_tableView->setColumnWidth(14, 80);  // Filled Qty
    m_tableView->setColumnWidth(15, 80);  // Pending Qty
    m_tableView->setColumnWidth(16, 100); // Avg Price
    m_tableView->setColumnWidth(17, 100); // Status
    m_tableView->setColumnWidth(18, 120); // Order Value
    m_tableView->setColumnWidth(19, 200); // Message
}

void OrderBookWindow::setupConnections()
{
    connect(m_applyFilterBtn, &QPushButton::clicked, this, &OrderBookWindow::applyFilters);
    connect(m_clearFilterBtn, &QPushButton::clicked, this, &OrderBookWindow::clearFilters);
    connect(m_exportBtn, &QPushButton::clicked, this, &OrderBookWindow::exportToCSV);
    connect(m_cancelOrderBtn, &QPushButton::clicked, this, &OrderBookWindow::onCancelOrder);
    connect(m_modifyOrderBtn, &QPushButton::clicked, this, &OrderBookWindow::onModifyOrder);
    
    connect(m_tableView, &CustomOrderBook::cancelOrderRequested, this, &OrderBookWindow::onCancelOrder);
    connect(m_tableView, &CustomOrderBook::modifyOrderRequested, this, &OrderBookWindow::onModifyOrder);
}

void OrderBookWindow::loadDummyData()
{
    // Sample orders for testing
    QList<QStringList> orders = {
        {"ORD001", "26-12-2025 09:15:30", "NSE", "EQ", "RELIANCE", "-", "EQ", "BUY", "Limit", "NRML", "DAY", "100", "2450.50", "0.00", "100", "0", "2450.50", "Executed", "245050.00", "Order executed successfully"},
        {"ORD002", "26-12-2025 09:20:15", "NSE", "FO", "NIFTY", "28-DEC-2025", "FUT", "SELL", "Market", "MIS", "DAY", "50", "0.00", "0.00", "50", "0", "21550.25", "Executed", "1077512.50", "Order executed"},
        {"ORD003", "26-12-2025 09:25:45", "NSE", "EQ", "TCS", "-", "EQ", "BUY", "Limit", "NRML", "DAY", "25", "3650.00", "0.00", "0", "25", "0.00", "Open", "91250.00", "Order pending"},
        {"ORD004", "26-12-2025 09:30:20", "NFO", "FO", "BANKNIFTY", "28-DEC-2025", "OPT", "BUY", "Limit", "MIS", "DAY", "75", "250.50", "0.00", "75", "0", "250.50", "Executed", "18787.50", "Order executed"},
        {"ORD005", "26-12-2025 09:35:10", "NSE", "EQ", "INFY", "-", "EQ", "SELL", "Limit", "NRML", "DAY", "50", "1450.00", "0.00", "0", "50", "0.00", "Open", "72500.00", "Order pending"},
        {"ORD006", "26-12-2025 09:40:00", "NSE", "EQ", "HDFCBANK", "-", "EQ", "BUY", "SL", "NRML", "DAY", "30", "1650.00", "1645.00", "0", "30", "0.00", "Pending", "49500.00", "Stop loss order placed"},
        {"ORD007", "26-12-2025 09:45:30", "BSE", "EQ", "WIPRO", "-", "EQ", "SELL", "Limit", "NRML", "DAY", "100", "450.75", "0.00", "0", "0", "0.00", "Cancelled", "45075.00", "Cancelled by user"},
        {"ORD008", "26-12-2025 09:50:15", "NFO", "FO", "NIFTY", "28-DEC-2025", "OPT", "BUY", "Limit", "MIS", "DAY", "100", "180.50", "0.00", "50", "50", "180.25", "Partially Filled", "18050.00", "Partial execution"},
        {"ORD009", "26-12-2025 09:55:00", "NSE", "EQ", "TATAMOTORS", "-", "EQ", "BUY", "Market", "NRML", "DAY", "150", "0.00", "0.00", "150", "0", "725.50", "Executed", "108825.00", "Market order executed"},
        {"ORD010", "26-12-2025 10:00:45", "NSE", "EQ", "SBIN", "-", "EQ", "SELL", "Limit", "NRML", "DAY", "200", "590.00", "0.00", "0", "200", "0.00", "Open", "118000.00", "Order pending"}
    };

    for (const QStringList &order : orders) {
        QList<QStandardItem*> row;
        for (int i = 0; i < order.size(); ++i) {
            QStandardItem *item = new QStandardItem(order[i]);
            item->setEditable(false);
            
            // Color coding for B/S column
            if (i == 7) {  // B/S column
                if (order[i] == "BUY") {
                    item->setForeground(QColor("#4CAF50"));  // Green
                } else if (order[i] == "SELL") {
                    item->setForeground(QColor("#F44336"));  // Red
                }
            }
            
            // Color coding for Status column
            if (i == 17) {  // Status column
                if (order[i] == "Executed") {
                    item->setForeground(QColor("#4CAF50"));  // Green
                } else if (order[i] == "Cancelled" || order[i] == "Rejected") {
                    item->setForeground(QColor("#F44336"));  // Red
                } else if (order[i] == "Open" || order[i] == "Pending") {
                    item->setForeground(QColor("#2196F3"));  // Blue
                } else if (order[i] == "Partially Filled") {
                    item->setForeground(QColor("#FF9800"));  // Orange
                }
            }
            
            row.append(item);
        }
        m_model->appendRow(row);
    }
    
    // Initialize filters
    m_fromTime = QDateTime::currentDateTime().addDays(-7);
    m_toTime = QDateTime::currentDateTime();
    m_instrumentFilter = "All";
    m_statusFilter = "All";
    m_orderTypeFilter = "All";
    m_exchangeFilter = "All";
    m_buySellFilter = "All";
    
    updateSummary();
    
    qDebug() << "[OrderBookWindow] Loaded" << m_model->rowCount() << "sample orders";
}

void OrderBookWindow::refreshOrders()
{
    qDebug() << "[OrderBookWindow] Refresh orders";
    // TODO: Call XTS API to fetch orders
    loadDummyData();
}

void OrderBookWindow::applyFilters()
{
    m_instrumentFilter = m_instrumentTypeCombo->currentText();
    m_statusFilter = m_statusCombo->currentText();
    m_orderTypeFilter = m_orderTypeCombo->currentText();
    m_exchangeFilter = m_exchangeCombo->currentText();
    m_buySellFilter = m_buySellCombo->currentText();
    m_fromTime = m_fromTimeEdit->dateTime();
    m_toTime = m_toTimeEdit->dateTime();
    
    applyFilterToModel();
    
    qDebug() << "[OrderBookWindow] Filters applied";
}

void OrderBookWindow::clearFilters()
{
    m_instrumentTypeCombo->setCurrentIndex(0);
    m_statusCombo->setCurrentIndex(0);
    m_orderTypeCombo->setCurrentIndex(0);
    m_exchangeCombo->setCurrentIndex(0);
    m_buySellCombo->setCurrentIndex(0);
    m_fromTimeEdit->setDateTime(QDateTime::currentDateTime().addDays(-7));
    m_toTimeEdit->setDateTime(QDateTime::currentDateTime());
    
    m_instrumentFilter = "All";
    m_statusFilter = "All";
    m_orderTypeFilter = "All";
    m_exchangeFilter = "All";
    m_buySellFilter = "All";
    
    // Clear column filters
    m_columnFilters.clear();
    for (auto widget : m_filterWidgets) {
        widget->clear();
    }
    
    applyFilterToModel();
    
    qDebug() << "[OrderBookWindow] Filters cleared";
}

void OrderBookWindow::exportToCSV()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export Orders", "", "CSV Files (*.csv)");
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Failed", "Could not open file for writing.");
        return;
    }
    
    QTextStream out(&file);
    
    // Write headers
    for (int col = 0; col < m_model->columnCount(); ++col) {
        out << m_model->headerData(col, Qt::Horizontal).toString();
        if (col < m_model->columnCount() - 1) {
            out << ",";
        }
    }
    out << "\n";
    
    // Write visible data rows
    int startRow = m_filterRowVisible ? 1 : 0;
    for (int row = startRow; row < m_model->rowCount(); ++row) {
        if (!m_tableView->isRowHidden(row)) {
            for (int col = 0; col < m_model->columnCount(); ++col) {
                out << m_model->item(row, col)->text();
                if (col < m_model->columnCount() - 1) {
                    out << ",";
                }
            }
            out << "\n";
        }
    }
    
    file.close();
    QMessageBox::information(this, "Export Complete", QString("Orders exported to %1").arg(fileName));
    
    qDebug() << "[OrderBookWindow] Exported to" << fileName;
}

void OrderBookWindow::applyFilterToModel()
{
    // Skip filter row if present
    int startRow = m_filterRowVisible ? 1 : 0;
    
    // Simple visibility filter based on criteria
    for (int row = startRow; row < m_model->rowCount(); ++row) {
        bool visible = true;
        
        // Time filter
        QString timeStr = m_model->item(row, 1)->text();  // Time column
        QDateTime orderTime = QDateTime::fromString(timeStr, "dd-MM-yyyy HH:mm:ss");
        if (orderTime.isValid()) {
            if (orderTime < m_fromTime || orderTime > m_toTime) {
                visible = false;
            }
        }
        
        // Instrument filter
        if (m_instrumentFilter != "All") {
            QString instrument = m_model->item(row, 6)->text();  // Instrument column
            if (!instrument.contains(m_instrumentFilter)) {
                visible = false;
            }
        }
        
        // Status filter
        if (m_statusFilter != "All") {
            QString status = m_model->item(row, 17)->text();  // Status column
            if (!status.contains(m_statusFilter, Qt::CaseInsensitive)) {
                visible = false;
            }
        }
        
        // Order Type filter
        if (m_orderTypeFilter != "All") {
            QString orderType = m_model->item(row, 8)->text();  // Order Type column
            if (!orderType.contains(m_orderTypeFilter, Qt::CaseInsensitive)) {
                visible = false;
            }
        }
        
        // Exchange filter
        if (m_exchangeFilter != "All") {
            QString exchange = m_model->item(row, 2)->text();  // Exchange column
            if (!exchange.contains(m_exchangeFilter)) {
                visible = false;
            }
        }
        
        // Buy/Sell filter
        if (m_buySellFilter != "All") {
            QString buySell = m_model->item(row, 7)->text();  // B/S column
            if (buySell != m_buySellFilter.toUpper()) {
                visible = false;
            }
        }
        
        // Apply column filters (Excel-like)
        for (auto it = m_columnFilters.constBegin(); it != m_columnFilters.constEnd(); ++it) {
            int col = it.key();
            const QStringList& allowedValues = it.value();
            
            if (!allowedValues.isEmpty()) {
                QStandardItem* item = m_model->item(row, col);
                if (item) {
                    QString cellValue = item->text();
                    if (!allowedValues.contains(cellValue)) {
                        visible = false;
                        break;
                    }
                }
            }
        }
        
        m_tableView->setRowHidden(row, !visible);
    }
    
    updateSummary();
}

void OrderBookWindow::updateSummary()
{
    m_totalOrders = 0;
    m_openOrders = 0;
    m_executedOrders = 0;
    m_cancelledOrders = 0;
    m_totalOrderValue = 0.0;
    
    // Skip filter row if present
    int startRow = m_filterRowVisible ? 1 : 0;
    
    for (int row = startRow; row < m_model->rowCount(); ++row) {
        if (!m_tableView->isRowHidden(row)) {
            m_totalOrders++;
            
            QString status = m_model->item(row, 17)->text();
            double orderValue = m_model->item(row, 18)->text().toDouble();
            
            if (status.contains("Open", Qt::CaseInsensitive) || status.contains("Pending", Qt::CaseInsensitive)) {
                m_openOrders++;
            } else if (status.contains("Executed", Qt::CaseInsensitive)) {
                m_executedOrders++;
            } else if (status.contains("Cancelled", Qt::CaseInsensitive) || status.contains("Rejected", Qt::CaseInsensitive)) {
                m_cancelledOrders++;
            }
            
            m_totalOrderValue += orderValue;
        }
    }
    
    qDebug() << "[OrderBookWindow] Summary - Total:" << m_totalOrders
             << "Open:" << m_openOrders << "Executed:" << m_executedOrders
             << "Cancelled:" << m_cancelledOrders << "Value:" << m_totalOrderValue;
}

void OrderBookWindow::setInstrumentFilter(const QString &instrument)
{
    int index = m_instrumentTypeCombo->findText(instrument);
    if (index >= 0) {
        m_instrumentTypeCombo->setCurrentIndex(index);
    }
}

void OrderBookWindow::setTimeFilter(const QDateTime &fromTime, const QDateTime &toTime)
{
    m_fromTimeEdit->setDateTime(fromTime);
    m_toTimeEdit->setDateTime(toTime);
}

void OrderBookWindow::setStatusFilter(const QString &status)
{
    int index = m_statusCombo->findText(status);
    if (index >= 0) {
        m_statusCombo->setCurrentIndex(index);
    }
}

void OrderBookWindow::setOrderTypeFilter(const QString &orderType)
{
    int index = m_orderTypeCombo->findText(orderType);
    if (index >= 0) {
        m_orderTypeCombo->setCurrentIndex(index);
    }
}

void OrderBookWindow::onCancelOrder()
{
    QModelIndex currentIndex = m_tableView->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::warning(this, "Cancel Order", "Please select an order to cancel.");
        return;
    }
    
    QString orderId = m_model->item(currentIndex.row(), 0)->text();
    QString status = m_model->item(currentIndex.row(), 17)->text();
    
    if (status == "Executed" || status == "Cancelled") {
        QMessageBox::information(this, "Cancel Order", QString("Order %1 is already %2 and cannot be cancelled.").arg(orderId).arg(status.toLower()));
        return;
    }
    
    int ret = QMessageBox::question(this, "Cancel Order", 
                                      QString("Are you sure you want to cancel order %1?").arg(orderId),
                                      QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // TODO: Call XTS API to cancel order
        m_model->item(currentIndex.row(), 17)->setText("Cancelled");
        m_model->item(currentIndex.row(), 17)->setForeground(QColor("#F44336"));
        m_model->item(currentIndex.row(), 19)->setText("Cancelled by user");
        updateSummary();
        qDebug() << "[OrderBookWindow] Order cancelled:" << orderId;
        QMessageBox::information(this, "Success", QString("Order %1 cancelled successfully.").arg(orderId));
    }
}

void OrderBookWindow::onModifyOrder()
{
    QModelIndex currentIndex = m_tableView->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::warning(this, "Modify Order", "Please select an order to modify.");
        return;
    }
    
    QString orderId = m_model->item(currentIndex.row(), 0)->text();
    QString status = m_model->item(currentIndex.row(), 17)->text();
    
    if (status != "Open" && status != "Pending") {
        QMessageBox::information(this, "Modify Order", QString("Order %1 with status '%2' cannot be modified.").arg(orderId).arg(status));
        return;
    }
    
    // TODO: Open modify order dialog and call XTS API
    QMessageBox::information(this, "Modify Order", QString("Modify order dialog for %1 will be implemented.").arg(orderId));
    qDebug() << "[OrderBookWindow] Modify order requested:" << orderId;
}

// ============================================================================
// Excel-like Filter Row Implementation
// ============================================================================

void OrderBookWindow::toggleFilterRow()
{
    m_filterRowVisible = !m_filterRowVisible;
    
    if (m_filterRowVisible) {
        // Insert filter row at position 0
        qDebug() << "[OrderBookWindow::toggleFilterRow] Creating filter widgets";
        m_filterWidgets.clear();
        m_model->insertRow(0);
        
        for (int col = 0; col < m_model->columnCount(); ++col) {
            OrderBookFilterWidget* filterWidget = new OrderBookFilterWidget(col, this, m_tableView);
            m_filterWidgets.append(filterWidget);
            m_tableView->setIndexWidget(m_model->index(0, col), filterWidget);
            connect(filterWidget, &OrderBookFilterWidget::filterChanged, this, &OrderBookWindow::onColumnFilterChanged);
            qDebug() << "[OrderBookWindow::toggleFilterRow] Created filter widget for column" << col;
        }
        m_tableView->setRowHeight(0, 36);
        qDebug() << "[OrderBookWindow] Filter row activated (Excel-style) with" << m_filterWidgets.size() << "widgets";
    } else {
        // Clear filter widgets and remove filter row
        qDebug() << "[OrderBookWindow::toggleFilterRow] Clearing filter widgets";
        for (int col = 0; col < m_model->columnCount(); ++col) {
            m_tableView->setIndexWidget(m_model->index(0, col), nullptr);
        }
        qDeleteAll(m_filterWidgets);
        m_filterWidgets.clear();
        m_columnFilters.clear();
        m_model->removeRow(0);
        applyFilters();
        qDebug() << "[OrderBookWindow] Filter row deactivated";
    }
}

void OrderBookWindow::onColumnFilterChanged(int column, const QStringList& selectedValues)
{
    qDebug() << "[OrderBookWindow::onColumnFilterChanged] Column:" << column << "Values:" << selectedValues;
    
    if (selectedValues.isEmpty()) {
        m_columnFilters.remove(column);
    } else {
        m_columnFilters[column] = selectedValues;
    }
    
    applyFilterToModel();
}

QList<QStandardItem*> OrderBookWindow::getTopFilteredOrders() const
{
    QList<QStandardItem*> result;
    int startRow = m_filterRowVisible ? 1 : 0;
    
    for (int row = startRow; row < m_model->rowCount(); ++row) {
        result.append(m_model->item(row, 0));
    }
    
    return result;
}

// ============================================================================
// OrderBookFilterWidget Implementation
// ============================================================================

OrderBookFilterWidget::OrderBookFilterWidget(int column, OrderBookWindow* orderBookWindow, QWidget* parent)
    : QWidget(parent)
    , m_column(column)
    , m_filterButton(nullptr)
    , m_orderBookWindow(orderBookWindow)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);

    // Visual container
    this->setStyleSheet("background-color: #F5F5F5; border-bottom: 1px solid rgba(0,0,0,0.12);");

    // Create filter button with dropdown icon
    m_filterButton = new QPushButton(this);
    m_filterButton->setText("▼ Filter");
    m_filterButton->setStyleSheet(
        "QPushButton { "
        "background: #FFFFFF; color: #333333; border: 1px solid rgba(0,0,0,0.15); "
        "border-radius: 3px; padding: 4px 8px; text-align: left; font-size: 10px; "
        "}"
        "QPushButton:hover { background: #F8F8F8; border-color: #4A90E2; }"
        "QPushButton:pressed { background: #E8E8E8; }"
    );
    m_filterButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_filterButton->setMinimumHeight(28);

    layout->addWidget(m_filterButton);

    connect(m_filterButton, &QPushButton::clicked, this, &OrderBookFilterWidget::showFilterPopup);
}

QStringList OrderBookFilterWidget::getUniqueValuesForColumn() const
{
    QSet<QString> uniqueValues;
    
    if (!m_orderBookWindow) return QStringList();
    
    QStandardItemModel* model = m_orderBookWindow->m_model;
    int startRow = m_orderBookWindow->m_filterRowVisible ? 1 : 0;
    
    for (int row = startRow; row < model->rowCount(); ++row) {
        QStandardItem* item = model->item(row, m_column);
        if (item) {
            QString value = item->text();
            if (!value.isEmpty()) {
                uniqueValues.insert(value);
            }
        }
    }
    
    QStringList list = uniqueValues.values();
    list.sort();
    return list;
}

void OrderBookFilterWidget::showFilterPopup()
{
    qDebug() << "[OrderBookFilterWidget::showFilterPopup] Opening filter dialog for column" << m_column;
    qDebug() << "[OrderBookFilterWidget::showFilterPopup] Current selected values:" << m_selectedValues;
    
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("Filter Options");
    dialog->setMinimumWidth(250);
    dialog->setMinimumHeight(300);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
    
    // Search box
    QLineEdit* searchBox = new QLineEdit(dialog);
    searchBox->setPlaceholderText("Search...");
    mainLayout->addWidget(searchBox);
    
    // List widget with checkboxes
    QListWidget* listWidget = new QListWidget(dialog);
    QStringList uniqueValues = getUniqueValuesForColumn();
    
    // Add "(Select All)" option
    QListWidgetItem* selectAllItem = new QListWidgetItem("(Select All)", listWidget);
    selectAllItem->setFlags(selectAllItem->flags() | Qt::ItemIsUserCheckable);
    selectAllItem->setCheckState(m_selectedValues.isEmpty() ? Qt::Checked : Qt::Unchecked);
    
    // Add unique values
    for (const QString& value : uniqueValues) {
        QListWidgetItem* item = new QListWidgetItem(value, listWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_selectedValues.isEmpty() || m_selectedValues.contains(value) ? Qt::Checked : Qt::Unchecked);
    }
    
    mainLayout->addWidget(listWidget);
    
    // Connect select all
    connect(listWidget, &QListWidget::itemChanged, [listWidget, selectAllItem](QListWidgetItem* item) {
        if (item == selectAllItem) {
            Qt::CheckState state = item->checkState();
            for (int i = 1; i < listWidget->count(); ++i) {
                listWidget->item(i)->setCheckState(state);
            }
        } else {
            // Update select all state
            bool allChecked = true;
            for (int i = 1; i < listWidget->count(); ++i) {
                if (listWidget->item(i)->checkState() != Qt::Checked) {
                    allChecked = false;
                    break;
                }
            }
            selectAllItem->setCheckState(allChecked ? Qt::Checked : Qt::Unchecked);
        }
    });
    
    // Search functionality
    connect(searchBox, &QLineEdit::textChanged, [listWidget](const QString& text) {
        for (int i = 1; i < listWidget->count(); ++i) {
            QListWidgetItem* item = listWidget->item(i);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
    });
    
    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    mainLayout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    
    if (dialog->exec() == QDialog::Accepted) {
        // Collect selected values
        m_selectedValues.clear();
        int checkedCount = 0;
        for (int i = 1; i < listWidget->count(); ++i) {
            QListWidgetItem* item = listWidget->item(i);
            if (item->checkState() == Qt::Checked && !item->isHidden()) {
                m_selectedValues.append(item->text());
                checkedCount++;
            }
        }
        
        // Count total visible items
        int totalVisible = 0;
        for (int i = 1; i < listWidget->count(); ++i) {
            if (!listWidget->item(i)->isHidden()) totalVisible++;
        }
        
        // Update button text - show filter is active if not all items selected
        if (m_selectedValues.isEmpty() || checkedCount == totalVisible) {
            m_filterButton->setText("▼ Filter");
            m_selectedValues.clear(); // Clear means no filter
        } else {
            m_filterButton->setText(QString("▼ (%1)").arg(m_selectedValues.count()));
            m_filterButton->setStyleSheet(
                "QPushButton { "
                "background: #E3F2FD; color: #1976D2; border: 1px solid #1976D2; "
                "border-radius: 3px; padding: 4px 8px; text-align: left; font-size: 10px; font-weight: bold; "
                "}"
                "QPushButton:hover { background: #BBDEFB; border-color: #1565C0; }"
                "QPushButton:pressed { background: #90CAF9; }"
            );
        }
        
        emit filterChanged(m_column, m_selectedValues);
    }
    
    dialog->deleteLater();
}

QStringList OrderBookFilterWidget::selectedValues() const
{
    return m_selectedValues;
}

void OrderBookFilterWidget::clear()
{
    m_selectedValues.clear();
    m_filterButton->setText("▼ Filter");
    m_filterButton->setStyleSheet(
        "QPushButton { "
        "background: #FFFFFF; color: #333333; border: 1px solid rgba(0,0,0,0.15); "
        "border-radius: 3px; padding: 4px 8px; text-align: left; font-size: 10px; "
        "}"
        "QPushButton:hover { background: #F8F8F8; border-color: #4A90E2; }"
        "QPushButton:pressed { background: #E8E8E8; }"
    );
}

void OrderBookFilterWidget::updateButtonDisplay()
{
    if (m_selectedValues.isEmpty()) {
        m_filterButton->setText("▼ Filter");
    } else {
        m_filterButton->setText(QString("▼ (%1)").arg(m_selectedValues.count()));
    }
}
