#include "views/OrderBookWindow.h"
#include "core/widgets/CustomOrderBook.h"
#include "services/TradingDataService.h"
#include "repository/RepositoryManager.h"
#include "repository/ContractData.h"
#include "api/XTSTypes.h"
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
#include <QSortFilterProxyModel>

// Custom proxy model to keep filter row at top
class OrderBookSortProxyModel : public QSortFilterProxyModel {
public:
    explicit OrderBookSortProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

protected:
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override {
        // Always keep filter row at top
        if (source_left.row() == 0) return (sortOrder() == Qt::AscendingOrder);
        if (source_right.row() == 0) return (sortOrder() == Qt::DescendingOrder);

        // For data rows, use UserRole for numeric sorting
        QVariant leftData = sourceModel()->data(source_left, Qt::UserRole);
        QVariant rightData = sourceModel()->data(source_right, Qt::UserRole);

        if (leftData.isValid() && rightData.isValid()) {
            if (leftData.type() == QVariant::Double || leftData.type() == QVariant::Int || leftData.type() == QVariant::LongLong) {
                return leftData.toDouble() < rightData.toDouble();
            }
            if (leftData.type() == QVariant::DateTime) {
                return leftData.toDateTime() < rightData.toDateTime();
            }
        }

        return QSortFilterProxyModel::lessThan(source_left, source_right);
    }
};
#include <QDialog>
#include <QDialogButtonBox>
#include <QTimer>
#include <QDebug>

OrderBookWindow::OrderBookWindow(TradingDataService* tradingDataService, QWidget *parent)
    : QWidget(parent)
    , m_tableView(nullptr)
    , m_model(nullptr)
    , m_filterRowVisible(false)
    , m_filterShortcut(nullptr)
    , m_tradingDataService(tradingDataService)
    , m_totalOrders(0)
    , m_openOrders(0)
    , m_executedOrders(0)
    , m_cancelledOrders(0)
    , m_totalOrderValue(0.0)
{
    setupUI();
    loadInitialProfile();
    setupConnections();
    
    // Initialize filter state
    m_fromTime = QDateTime::currentDateTime().addDays(-7);
    m_toTime = QDateTime::currentDateTime();
    m_instrumentFilter = "All";
    m_statusFilter = "All";
    m_orderTypeFilter = "All";
    m_exchangeFilter = "All";
    m_buySellFilter = "All";

    // Connect to trading data service if available
    if (m_tradingDataService) {
        connect(m_tradingDataService, &TradingDataService::ordersUpdated,
                this, &OrderBookWindow::onOrdersUpdated);
        
        // Load initial data
        onOrdersUpdated(m_tradingDataService->getOrders());
        qDebug() << "[OrderBookWindow] Connected to TradingDataService and loaded initial orders";
    }
    
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
    QWidget *container = new QWidget(this);
    container->setObjectName("filterContainer");
    container->setStyleSheet(
        "QWidget#filterContainer { background-color: #2d2d2d; border-bottom: 1px solid #3f3f46; }"
        "QLabel { color: #d4d4d8; font-size: 11px; font-weight: 500; }"
        "QDateTimeEdit, QComboBox { "
        "   background-color: #3f3f46; color: #ffffff; border: 1px solid #52525b; "
        "   border-radius: 3px; padding: 3px 5px; font-size: 11px; "
        "} "
        "QDateTimeEdit::drop-down, QComboBox::drop-down { border: none; } "
        "QPushButton { border-radius: 3px; font-weight: 600; font-size: 11px; padding: 5px 12px; } "
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(8);

    // Filter label
    QLabel *topLabel = new QLabel("ORDER BOOK FILTERS");
    topLabel->setStyleSheet("color: #a1a1aa; font-weight: bold; letter-spacing: 0.5px; font-size: 10px;");
    mainLayout->addWidget(topLabel);

    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(15);

    // Group 1: Time Range
    QHBoxLayout *timeLayout = new QHBoxLayout();
    timeLayout->setSpacing(6);
    QLabel *lblFrom = new QLabel("From");
    m_fromTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-7));
    m_fromTimeEdit->setDisplayFormat("dd-MM-yyyy HH:mm");
    m_fromTimeEdit->setCalendarPopup(true);
    m_fromTimeEdit->setFixedWidth(135);

    QLabel *lblTo = new QLabel("To");
    m_toTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime());
    m_toTimeEdit->setDisplayFormat("dd-MM-yyyy HH:mm");
    m_toTimeEdit->setCalendarPopup(true);
    m_toTimeEdit->setFixedWidth(135);
    
    timeLayout->addWidget(lblFrom);
    timeLayout->addWidget(m_fromTimeEdit);
    timeLayout->addWidget(lblTo);
    timeLayout->addWidget(m_toTimeEdit);
    filterLayout->addLayout(timeLayout);

    // Group 2: Categories
    auto addCombo = [&](const QString &label, QComboBox* &combo, const QStringList &items, int width) {
        QVBoxLayout *vbox = new QVBoxLayout();
        vbox->setSpacing(2);
        vbox->addWidget(new QLabel(label));
        combo = new QComboBox();
        combo->addItems(items);
        combo->setFixedWidth(width);
        vbox->addWidget(combo);
        filterLayout->addLayout(vbox);
    };

    addCombo("Instrument", m_instrumentTypeCombo, {"All", "EQ", "FUT", "OPT", "INDEX"}, 90);
    addCombo("Exchange", m_exchangeCombo, {"All", "NSE", "BSE", "NFO", "MCX"}, 70);
    addCombo("B/S", m_buySellCombo, {"All", "BUY", "SELL"}, 65);
    addCombo("Status", m_statusCombo, {"All", "Open", "Executed", "Cancelled", "Rejected", "Pending"}, 90);
    addCombo("Order Type", m_orderTypeCombo, {"All", "Market", "Limit", "SL", "SL-M"}, 85);

    filterLayout->addStretch();

    // Group 3: Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);
    btnLayout->setAlignment(Qt::AlignBottom);

    m_applyFilterBtn = new QPushButton("Apply");
    m_applyFilterBtn->setStyleSheet("background-color: #16a34a; color: white;");
    
    m_clearFilterBtn = new QPushButton("Clear");
    m_clearFilterBtn->setStyleSheet("background-color: #52525b; color: #e4e4e7; border: 1px solid #71717a;");

    m_cancelOrderBtn = new QPushButton("Cancel");
    m_cancelOrderBtn->setStyleSheet("background-color: #dc2626; color: white;");
    
    m_modifyOrderBtn = new QPushButton("Modify");
    m_modifyOrderBtn->setStyleSheet("background-color: #2563eb; color: white;");

    m_exportBtn = new QPushButton("Export");
    m_exportBtn->setStyleSheet("background-color: #d97706; color: white;");

    btnLayout->addWidget(m_applyFilterBtn);
    btnLayout->addWidget(m_clearFilterBtn);
    btnLayout->addWidget(new QWidget()); // Spacer
    btnLayout->addWidget(m_cancelOrderBtn);
    btnLayout->addWidget(m_modifyOrderBtn);
    btnLayout->addWidget(m_exportBtn);

    filterLayout->addLayout(btnLayout);
    mainLayout->addLayout(filterLayout);

    return container;
}

QWidget* OrderBookWindow::createSummaryWidget()
{
    QWidget *summaryWidget = new QWidget();
    summaryWidget->setObjectName("summaryWidget");
    summaryWidget->setStyleSheet(
        "QWidget#summaryWidget { background-color: #1e1e1e; border-top: 1px solid #333333; }"
        "QLabel { color: #cccccc; font-size: 11px; }"
    );
    summaryWidget->setFixedHeight(32);
    
    QHBoxLayout *summaryLayout = new QHBoxLayout(summaryWidget);
    summaryLayout->setContentsMargins(15, 0, 15, 0);
    
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setText("Total Orders: 0 | Open: 0 | Executed: 0 | Cancelled: 0 | Rejected: 0");
    summaryLayout->addWidget(m_summaryLabel);
    summaryLayout->addStretch();
    
    return summaryWidget;
}


#include "views/GenericProfileDialog.h"
#include "models/GenericProfileManager.h"

void OrderBookWindow::showColumnProfileDialog()
{
    QList<GenericColumnInfo> allCols;
    for (int i = 0; i < m_model->columnCount(); ++i) {
        GenericColumnInfo info;
        info.id = i;
        info.name = m_model->headerData(i, Qt::Horizontal).toString();
        info.defaultWidth = 90;
        info.visibleByDefault = true;
        allCols.append(info);
    }

    GenericProfileDialog dialog("OrderBook", allCols, m_columnProfile, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_columnProfile = dialog.getProfile();
        
        // Apply to view
        for (int i = 0; i < m_model->columnCount(); ++i) {
            bool visible = m_columnProfile.isColumnVisible(i);
            m_tableView->setColumnHidden(i, !visible);
            if (visible) {
                m_tableView->setColumnWidth(i, m_columnProfile.columnWidth(i));
            }
        }
    }
}

void OrderBookWindow::loadInitialProfile()
{
    GenericProfileManager manager("profiles");
    QString defName = manager.getDefaultProfileName("OrderBook");
    
    if (defName != "Default" && manager.loadProfile("OrderBook", defName, m_columnProfile)) {
        // Success
    } else {
        // Default layout
        m_columnProfile = GenericTableProfile("Default");
        int colCount = m_model->columnCount();
        for (int i = 0; i < colCount; ++i) {
            m_columnProfile.setColumnVisible(i, true);
            m_columnProfile.setColumnWidth(i, 90);
        }
        QList<int> order;
        for (int i = 0; i < colCount; ++i) order.append(i);
        m_columnProfile.setColumnOrder(order);
    }
    
    // Apply to view
    for (int i = 0; i < m_model->columnCount(); ++i) {
        bool visible = m_columnProfile.isColumnVisible(i);
        m_tableView->setColumnHidden(i, !visible);
        if (visible) {
            m_tableView->setColumnWidth(i, m_columnProfile.columnWidth(i));
        }
    }
}

void OrderBookWindow::setupTable()
{
    m_tableView = new CustomOrderBook(this);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        menu.addAction("Export to CSV", this, &OrderBookWindow::exportToCSV);
        menu.addAction("Refresh", this, &OrderBookWindow::refreshOrders);
        menu.addSeparator();
        menu.addAction("Column Profile...", this, &OrderBookWindow::showColumnProfileDialog);
        menu.exec(m_tableView->viewport()->mapToGlobal(pos));
    });

    m_model = new QStandardItemModel(this);
    
    // Setup proxy model for sorting
    m_proxyModel = new OrderBookSortProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setDynamicSortFilter(true);
    
    m_tableView->setModel(m_proxyModel);
    m_tableView->setSortingEnabled(true);
    
    // Headers list as requested
    QStringList headers = {
        "User", "Group", "Client Order No.", "Exchange Code", "MemberId", "TraderId",
        "Instrument Type", "Instrument Name", "Code", "Symbol/ScripId", "Ser/Exp/Group",
        "Strike Price", "Option Type", "Scrip Name", "Order Type", "B/S", "Pending Quantity",
        "Price", "Spread", "Total Quantity", "Pro/Cli", "Client", "Client Name", "Misc.",
        "Exch. Ord.No.", "Status", "Disclosed Quantity", "Settlor", "Validity",
        "Good Till Days/Date/Time", "MF/AON", "MF Quantity", "Trigger Price",
        "CP Broker Id", "Auction No.", "Last Modified Time", "Total Executed Quantity",
        "Avg. Price", "Reason", "User Remarks", "Part Type", "Product Type",
        "Server Entry Time", "AMO Order ID", "OMSID", "Give Up Status", "EOMSID",
        "BookingRef", "Dealing Instruction", "Order Instruction", "Pending Lots",
        "Total Lots", "Total Executed Lots", "Alias Settlor", "Alias PartType",
        "Initiated From", "Modified From", "Strategy", "Mapping", "Initiated From User Id",
        "Modified From User Id", "SOR Id", "Maturity Date", "Yield", "COL", "SL Price",
        "SL Trigger Price", "SL Jump Price", "LTP Jump Price", "Profit Price",
        "Leg Indicator", "Bracket Order Status", "Mapping Order Type", "Algo Type",
        "Algo Description"
    };
    
    m_model->setHorizontalHeaderLabels(headers);
    m_tableView->setModel(m_model);
    
    // Set column widths
    for (int i = 0; i < headers.size(); ++i) {
        m_tableView->setColumnWidth(i, 100);
    }
    // Adjust specific ones
    m_tableView->setColumnWidth(2, 120);  // Client Order No.
    m_tableView->setColumnWidth(9, 150);  // Symbol
    m_tableView->setColumnWidth(13, 150); // Scrip Name
    m_tableView->setColumnWidth(14, 90);  // Order Type
    m_tableView->setColumnWidth(25, 100); // Status
    m_tableView->setColumnWidth(38, 200); // Reason
    m_tableView->setColumnWidth(42, 140); // Server Entry Time
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

void OrderBookWindow::refreshOrders()
{
    qDebug() << "[OrderBookWindow] Refreshing orders...";
    // In a real scenario, this might trigger a fetch in the service
    // For now, we rely on the service being updated by the logic flow or WebSocket
    if (m_tradingDataService) {
        onOrdersUpdated(m_tradingDataService->getOrders());
    }
}

void OrderBookWindow::onOrdersUpdated(const QVector<XTS::Order>& orders)
{
    qDebug() << "[OrderBookWindow::onOrdersUpdated] Received" << orders.size() << "orders";
    
    // Remember filter row visible state
    bool filterRowVisible = m_filterRowVisible;
    if (filterRowVisible) {
        toggleFilterRow(); // Hide to clear widgets
    }

    m_model->removeRows(0, m_model->rowCount());
    
    RepositoryManager* repo = RepositoryManager::getInstance();
    
    for (const XTS::Order& order : orders) {
        QList<QStandardItem*> row;
        const ContractData* contract = repo->getContractByToken(order.exchangeSegment, order.exchangeInstrumentID);
        
        // Initialize 75 items
        for (int i = 0; i < 75; ++i) {
            row.append(new QStandardItem(""));
        }

        // Map fields to columns (0-indexed)
        row[0]->setText("MEMBER");                        // User
        row[1]->setText("DEFAULT");                       // Group
        
        row[2]->setText(order.appOrderIDStr());           // Client Order No
        row[2]->setData((qlonglong)order.appOrderID, Qt::UserRole);
        
        row[3]->setText(order.exchangeSegment);           // Exchange Code
        row[4]->setText("123");                           // MemberId
        row[5]->setText("T01");                           // TraderId
        
        row[6]->setText(contract ? contract->series : "");   // Instrument Type
        row[7]->setText(contract ? contract->series : "");   // Instrument Name
        
        row[8]->setText(QString::number(order.exchangeInstrumentID)); // Code
        row[8]->setData((qlonglong)order.exchangeInstrumentID, Qt::UserRole);
        
        row[9]->setText(order.tradingSymbol);             // Symbol/ScripId
        row[10]->setText(contract ? contract->expiryDate : ""); // Ser/Exp/Group
        
        row[11]->setText(contract && contract->strikePrice > 0 ? QString::number(contract->strikePrice, 'f', 2) : ""); // Strike Price
        row[11]->setData(contract ? contract->strikePrice : 0.0, Qt::UserRole);
        
        row[12]->setText(contract ? contract->optionType : ""); // Option Type
        row[13]->setText(contract ? contract->displayName : order.tradingSymbol); // Scrip Name
        row[14]->setText(order.orderType);                // Order Type
        row[15]->setText(order.orderSide);                // B/S
        
        row[16]->setText(QString::number(order.pendingQuantity())); // Pending Quantity
        row[16]->setData(order.pendingQuantity(), Qt::UserRole);
        
        row[17]->setText(QString::number(order.orderPrice, 'f', 2)); // Price
        row[17]->setData(order.orderPrice, Qt::UserRole);
        
        row[19]->setText(QString::number(order.orderQuantity)); // Total Quantity
        row[19]->setData(order.orderQuantity, Qt::UserRole);
        
        row[20]->setText("CLI");                          // Pro/Cli
        row[21]->setText(order.clientID);                 // Client
        
        row[24]->setText(order.exchangeOrderID);          // Exch. Ord.No.
        row[24]->setData((qlonglong)order.exchangeOrderID.toLongLong(), Qt::UserRole);
        
        row[25]->setText(order.orderStatus);              // Status
        
        row[26]->setText(QString::number(order.orderDisclosedQuantity)); // Disclosed Quantity
        row[26]->setData(order.orderDisclosedQuantity, Qt::UserRole);
        
        row[28]->setText(order.timeInForce);              // Validity
        
        row[32]->setText(QString::number(order.orderStopPrice, 'f', 2)); // Trigger Price
        row[32]->setData(order.orderStopPrice, Qt::UserRole);
        
        row[35]->setText(order.lastUpdateDateTime);       // Last Modified Time
        
        row[36]->setText(QString::number(order.cumulativeQuantity)); // Total Executed Quantity
        row[36]->setData(order.cumulativeQuantity, Qt::UserRole);
        
        row[37]->setText(QString::number(order.orderAverageTradedPrice, 'f', 2)); // Avg. Price
        row[37]->setData(order.orderAverageTradedPrice, Qt::UserRole);
        
        row[38]->setText(order.cancelRejectReason.isEmpty() ? "-" : order.cancelRejectReason); // Reason
        row[39]->setText(order.orderUniqueIdentifier);    // User Remarks
        row[41]->setText(order.productType);              // Product Type
        row[42]->setText(order.orderTimestamp());         // Server Entry Time
        row[42]->setData(QDateTime::fromString(order.orderTimestamp(), "dd-MM-yyyy HH:mm:ss"), Qt::UserRole); // For chrono sort

        // Side colors
        if (order.orderSide == "BUY") {
            row[15]->setForeground(QColor("#4CAF50"));
        } else if (order.orderSide == "SELL") {
            row[15]->setForeground(QColor("#F44336"));
        }
        QFont boldFont = row[15]->font();
        boldFont.setBold(true);
        row[15]->setFont(boldFont);

        // Status colors
        if (order.orderStatus == "Filled") row[25]->setForeground(QColor("#4CAF50"));
        else if (order.orderStatus == "Cancelled" || order.orderStatus == "Rejected") row[25]->setForeground(QColor("#F44336"));
        else if (order.orderStatus == "Open" || order.orderStatus == "New" || order.orderStatus == "PartiallyFilled") row[25]->setForeground(QColor("#2196F3"));
        row[25]->setFont(boldFont);

        for (auto item : row) item->setEditable(false);
        m_model->appendRow(row);
    }
    
    if (filterRowVisible) {
        toggleFilterRow(); // Show again
    }

    applyFilterToModel();
    updateSummary();
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
    
    m_summaryLabel->setText(QString("Total Orders: <b>%1</b> | "
                                    "Open: <b style='color: #2563eb;'>%2</b> | "
                                    "Executed: <b style='color: #16a34a;'>%3</b> | "
                                    "Cancelled: <b style='color: #dc2626;'>%4</b> | "
                                    "Total Value: <b>₹%5</b>")
                            .arg(m_totalOrders)
                            .arg(m_openOrders)
                            .arg(m_executedOrders)
                            .arg(m_cancelledOrders)
                            .arg(m_totalOrderValue, 0, 'f', 2));

    qDebug() << "[OrderBookWindow] Summary updated";
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
