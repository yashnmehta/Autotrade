#include "views/TradeBookWindow.h"
#include "core/widgets/CustomTradeBook.h"
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
class TradeBookSortProxyModel : public QSortFilterProxyModel {
public:
    explicit TradeBookSortProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

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

TradeBookWindow::TradeBookWindow(TradingDataService* tradingDataService, QWidget *parent)
    : QWidget(parent)
    , m_tableView(nullptr)
    , m_model(nullptr)
    , m_filteredModel(nullptr)
    , m_filterRowVisible(false)
    , m_filterShortcut(nullptr)
    , m_tradingDataService(tradingDataService)
    , m_totalBuyQty(0.0)
    , m_totalSellQty(0.0)
    , m_totalBuyValue(0.0)
    , m_totalSellValue(0.0)
    , m_tradeCount(0)
{
    setupUI();
    loadInitialProfile();
    setupConnections();
    
    // Initialize filter state
    m_fromTime = QDateTime::currentDateTime().addDays(-7);
    m_toTime = QDateTime::currentDateTime();
    m_instrumentFilter = "All";
    m_buySellFilter = "All";
    m_orderTypeFilter = "All";
    m_exchangeFilter = "All";

    // Connect to trading data service if available
    if (m_tradingDataService) {
        connect(m_tradingDataService, &TradingDataService::tradesUpdated,
                this, &TradeBookWindow::onTradesUpdated);
        
        // Load initial data
        onTradesUpdated(m_tradingDataService->getTrades());
        qDebug() << "[TradeBookWindow] Connected to TradingDataService and loaded initial trades";
    }
    
    // Setup Ctrl+F shortcut for filter toggle
    m_filterShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(m_filterShortcut, &QShortcut::activated, this, &TradeBookWindow::toggleFilterRow);
    
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
    
    qDebug() << "[TradeBookWindow] Created - Press Ctrl+F to toggle filters, Esc to close";
}

TradeBookWindow::~TradeBookWindow()
{
    qDebug() << "[TradeBookWindow] Destroyed";
}

void TradeBookWindow::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Filter bar at top
    QWidget *filterWidget = createFilterWidget();
    mainLayout->addWidget(filterWidget);

    // Trade table
    setupTable();
    mainLayout->addWidget(m_tableView, 1);

    // Summary bar at bottom
    QWidget *summaryWidget = createSummaryWidget();
    mainLayout->addWidget(summaryWidget);
}

QWidget* TradeBookWindow::createFilterWidget()
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
    QLabel *topLabel = new QLabel("TRADE BOOK FILTERS");
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

    addCombo("Instrument", m_instrumentTypeCombo, {"All", "NSE OPT", "NSE FUT", "NSE EQ", "BSE EQ"}, 110);
    addCombo("Exchange", m_exchangeCombo, {"All", "NSE", "BSE", "MCX"}, 80);
    addCombo("Buy/Sell", m_buySellCombo, {"All", "Buy", "Sell"}, 70);
    addCombo("Order Type", m_orderTypeCombo, {"All", "Day", "IOC", "GTC"}, 80);

    filterLayout->addStretch();

    // Group 3: Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);
    btnLayout->setAlignment(Qt::AlignBottom);

    m_applyFilterBtn = new QPushButton("Apply");
    m_applyFilterBtn->setStyleSheet("background-color: #16a34a; color: white;");
    
    m_clearFilterBtn = new QPushButton("Clear");
    m_clearFilterBtn->setStyleSheet("background-color: #52525b; color: #e4e4e7; border: 1px solid #71717a;");

    m_exportBtn = new QPushButton("Export");
    m_exportBtn->setStyleSheet("background-color: #d97706; color: white;");

    m_showSummaryCheck = new QCheckBox("Show Summary Row");
    m_showSummaryCheck->setStyleSheet("color: #d4d4d8; font-size: 11px;");
    m_showSummaryCheck->setChecked(false);

    btnLayout->addWidget(m_showSummaryCheck);
    btnLayout->addWidget(m_applyFilterBtn);
    btnLayout->addWidget(m_clearFilterBtn);
    btnLayout->addWidget(m_exportBtn);

    filterLayout->addLayout(btnLayout);
    mainLayout->addLayout(filterLayout);

    return container;
}

QWidget* TradeBookWindow::createSummaryWidget()
{
    QWidget *summaryWidget = new QWidget();
    summaryWidget->setObjectName("summaryWidget");
    summaryWidget->setStyleSheet(
        "QWidget#summaryWidget { background-color: #f5f5f5; border-top: 1px solid #cccccc; }"
        "QLabel { color: #333333; font-size: 11px; }"
    );
    summaryWidget->setFixedHeight(32);
    
    QHBoxLayout *summaryLayout = new QHBoxLayout(summaryWidget);
    summaryLayout->setContentsMargins(15, 0, 15, 0);
    
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setText("Trades: 0 | Buy Qty: 0 | Sell Qty: 0 | Buy Val: 0 | Sell Val: 0");
    summaryLayout->addWidget(m_summaryLabel);
    summaryLayout->addStretch();
    
    return summaryWidget;
}

void TradeBookWindow::setupFilterBar()
{
    // Filter widgets created in setupUI
}


#include "views/GenericProfileDialog.h"
#include "models/GenericProfileManager.h"

void TradeBookWindow::showColumnProfileDialog()
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

    GenericProfileDialog dialog("TradeBook", allCols, m_columnProfile, this);
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

void TradeBookWindow::loadInitialProfile()
{
    GenericProfileManager manager("profiles");
    QString defName = manager.getDefaultProfileName("TradeBook");
    
    if (defName != "Default" && manager.loadProfile("TradeBook", defName, m_columnProfile)) {
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

void TradeBookWindow::setupTable()
{
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

    // Create model
    m_model = new QStandardItemModel(this);
    
    // Setup proxy model for sorting
    m_proxyModel = new TradeBookSortProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setDynamicSortFilter(true);
    
    m_tableView->setModel(m_proxyModel);
    m_tableView->setSortingEnabled(true);
    
    // 57 headers list as requested
    QStringList headers = {
        "User", "Group", "Exchange Code", "MemberId", "TraderId", "Instrument Type",
        "Instrument Name", "Code", "Symbol/ScripId", "Spread Symbol", "Ser/Exp/Group",
        "Strike Price", "Option Type", "Order Type", "B/S", "Quantity", "Price",
        "Time", "Spread Price", "Spread", "Pro/Cli", "Client", "Client Name",
        "Exch. Order No.", "Trade No.", "Settlor", "User Remarks", "New Client",
        "Part Type", "Product Type", "Order Entry Time", "Client Order No.",
        "Order Initiated From", "Order Modified From", "Misc.", "Strategy",
        "Mapping", "OMSID", "GiveUp Status", "EOMSID", "Booking Ref.",
        "Dealing Instruction", "Order Instruction", "Lots", "Alias Settlor",
        "Alias PartType", "New Participant Code", "Initiated From User Id",
        "Modified From User Id", "SOR Id", "New Settlor", "Maturity Date",
        "Yield", "Mapping Order Type", "Algo Type", "Algo Description", "Scrip Name"
    };
    
    m_model->setHorizontalHeaderLabels(headers);
    m_tableView->setModel(m_model);
    
    // Set column widths
    QHeaderView *header = m_tableView->horizontalHeader();
    for (int i = 0; i < headers.size(); ++i) {
        header->resizeSection(i, 100);
    }
    header->resizeSection(8, 150);  // Symbol/ScripId
    header->resizeSection(56, 150); // Scrip Name
    header->resizeSection(17, 140); // Time
    header->resizeSection(23, 120); // Exch. Order No.
    header->resizeSection(24, 120); // Trade No.
    header->resizeSection(31, 120); // Client Order No.
}

void TradeBookWindow::setupConnections()
{
    connect(m_applyFilterBtn, &QPushButton::clicked, this, &TradeBookWindow::applyFilters);
    connect(m_clearFilterBtn, &QPushButton::clicked, this, &TradeBookWindow::clearFilters);
    connect(m_exportBtn, &QPushButton::clicked, this, &TradeBookWindow::exportToCSV);
    connect(m_showSummaryCheck, &QCheckBox::toggled, m_tableView, &CustomTradeBook::setSummaryRowEnabled);
}

void TradeBookWindow::refreshTrades()
{
    qDebug() << "[TradeBookWindow] Refreshing trades...";
    if (m_tradingDataService) {
        onTradesUpdated(m_tradingDataService->getTrades());
    }
}

void TradeBookWindow::onTradesUpdated(const QVector<XTS::Trade>& trades)
{
    qDebug() << "[TradeBookWindow::onTradesUpdated] Received" << trades.size() << "trades";
    
    // Remember filter row visible state
    bool filterRowVisible = m_filterRowVisible;
    if (filterRowVisible) {
        toggleFilterRow(); // Hide to clear widgets
    }

    m_model->removeRows(0, m_model->rowCount());
    
    RepositoryManager* repo = RepositoryManager::getInstance();
    
    for (const XTS::Trade& trade : trades) {
        QList<QStandardItem*> row;
        const ContractData* contract = repo->getContractByToken(trade.exchangeSegment, trade.exchangeInstrumentID);
        
        // Initialize 57 items
        for (int i = 0; i < 57; ++i) {
            row.append(new QStandardItem(""));
        }

        // Map fields to columns (0-indexed)
        row[0]->setText("MEMBER");                        // User
        row[1]->setText("DEFAULT");                       // Group
        row[2]->setText(trade.exchangeSegment);           // Exchange Code
        row[3]->setText("123");                           // MemberId
        row[4]->setText("T01");                           // TraderId
        row[5]->setText(contract ? contract->series : "-"); // Instrument Type
        row[6]->setText(contract ? contract->series : "-"); // Instrument Name
        
        row[7]->setText(QString::number(trade.exchangeInstrumentID)); // Code
        row[7]->setData((qlonglong)trade.exchangeInstrumentID, Qt::UserRole);
        
        row[8]->setText(trade.tradingSymbol);             // Symbol/ScripId
        row[10]->setText(contract ? contract->expiryDate : "-"); // Ser/Exp/Group
        
        row[11]->setText(contract && contract->strikePrice > 0 ? QString::number(contract->strikePrice, 'f', 2) : "-"); // Strike Price
        row[11]->setData(contract ? contract->strikePrice : 0.0, Qt::UserRole);
        
        row[12]->setText(contract ? contract->optionType : "-"); // Option Type
        row[13]->setText(trade.orderType);                // Order Type
        row[14]->setText(trade.orderSide);                // B/S
        
        row[15]->setText(QString::number(trade.lastTradedQuantity)); // Quantity
        row[15]->setData(trade.lastTradedQuantity, Qt::UserRole);
        
        row[16]->setText(QString::number(trade.lastTradedPrice, 'f', 2)); // Price
        row[16]->setData(trade.lastTradedPrice, Qt::UserRole);
        
        row[17]->setText(trade.lastExecutionTransactTime); // Time
        row[17]->setData(QDateTime::fromString(trade.lastExecutionTransactTime, "dd-MM-yyyy HH:mm:ss"), Qt::UserRole);
        
        row[20]->setText("CLI");                          // Pro/Cli
        row[21]->setText(trade.clientID.isEmpty() ? "PRO7" : trade.clientID); // Client
        row[23]->setText(trade.exchangeOrderID);          // Exch. Order No.
        row[24]->setText(trade.executionID);              // Trade No.
        row[26]->setText(trade.orderUniqueIdentifier);    // User Remarks
        row[29]->setText(trade.productType);              // Product Type
        row[30]->setText(trade.orderGeneratedDateTime);   // Order Entry Time
        
        row[31]->setText(QString::number(trade.appOrderID)); // Client Order No.
        row[31]->setData((qlonglong)trade.appOrderID, Qt::UserRole);
        
        row[51]->setText(contract ? contract->expiryDate : "-"); // Maturity Date
        row[56]->setText(contract ? contract->displayName : trade.tradingSymbol); // Scrip Name

        // Coloring based on trade side
        QColor bgColor;
        QColor textColor = Qt::black; // Default for light theme
        
        if (trade.orderSide == "BUY") {
            bgColor = QColor("#e3f2fd"); // Light Blue for Buy
            row[14]->setForeground(QColor("#0d47a1"));
        } else if (trade.orderSide == "SELL") {
            bgColor = QColor("#ffebee"); // Light Red for Sell
            row[14]->setForeground(QColor("#b71c1c"));
        }
        
        if (bgColor.isValid()) {
            for (auto item : row) {
                item->setBackground(bgColor);
            }
        }
        
        QFont boldFont = row[14]->font();
        boldFont.setBold(true);
        row[14]->setFont(boldFont);

        for (auto item : row) item->setEditable(false);
        m_model->appendRow(row);
    }
    
    if (filterRowVisible) {
        toggleFilterRow(); // Show again
    }

    applyFilterToModel();
    updateSummary();
}

void TradeBookWindow::applyFilters()
{
    qDebug() << "[TradeBookWindow] Applying filters...";
    
    m_fromTime = m_fromTimeEdit->dateTime();
    m_toTime = m_toTimeEdit->dateTime();
    m_instrumentFilter = m_instrumentTypeCombo->currentText();
    m_buySellFilter = m_buySellCombo->currentText();
    m_orderTypeFilter = m_orderTypeCombo->currentText();
    m_exchangeFilter = m_exchangeCombo->currentText();
    
    applyFilterToModel();
    updateSummary();
    
    qDebug() << "[TradeBookWindow] Filters applied -"
             << "Time:" << m_fromTime.toString("dd-MM-yyyy hh:mm") << "to" << m_toTime.toString("dd-MM-yyyy hh:mm")
             << "Instrument:" << m_instrumentFilter
             << "Buy/Sell:" << m_buySellFilter
             << "Exchange:" << m_exchangeFilter;
}

void TradeBookWindow::clearFilters()
{
    qDebug() << "[TradeBookWindow] Clearing filters...";
    
    m_fromTimeEdit->setDateTime(QDateTime::currentDateTime().addDays(-7));
    m_toTimeEdit->setDateTime(QDateTime::currentDateTime());
    m_instrumentTypeCombo->setCurrentIndex(0);
    m_buySellCombo->setCurrentIndex(0);
    m_orderTypeCombo->setCurrentIndex(0);
    m_exchangeCombo->setCurrentIndex(0);
    
    applyFilters();
}

void TradeBookWindow::applyFilterToModel()
{
    // Skip filter row if present
    int startRow = m_filterRowVisible ? 1 : 0;
    
    // Simple visibility filter based on criteria
    for (int row = startRow; row < m_model->rowCount(); ++row) {
        bool visible = true;
        
        // Time filter
        QString timeStr = m_model->item(row, 17)->text();  // Time column
        QDateTime tradeTime = QDateTime::fromString(timeStr, "dd-MM-yyyy hh:mm:ss");
        if (tradeTime.isValid()) {
            if (tradeTime < m_fromTime || tradeTime > m_toTime) {
                visible = false;
            }
        }
        
        // Instrument filter
        if (m_instrumentFilter != "All") {
            QString instrument = m_model->item(row, 5)->text();  // Instrument column
            if (!instrument.contains(m_instrumentFilter)) {
                visible = false;
            }
        }
        
        // Buy/Sell filter
        if (m_buySellFilter != "All") {
            QString buySell = m_model->item(row, 14)->text();  // B/S column
            if (buySell != m_buySellFilter.toUpper()) {
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

void TradeBookWindow::updateSummary()
{
    m_totalBuyQty = 0.0;
    m_totalSellQty = 0.0;
    m_totalBuyValue = 0.0;
    m_totalSellValue = 0.0;
    m_tradeCount = 0;
    
    // Skip filter row if present
    int startRow = m_filterRowVisible ? 1 : 0;
    
    for (int row = startRow; row < m_model->rowCount(); ++row) {
        if (!m_tableView->isRowHidden(row)) {
            m_tradeCount++;
            
            QString buySell = m_model->item(row, 14)->text();
            double qty = m_model->item(row, 15)->text().toDouble();
            double price = m_model->item(row, 16)->text().toDouble();
            double value = qty * price;
            
            if (buySell == "BUY") {
                m_totalBuyQty += qty;
                m_totalBuyValue += value;
            } else if (buySell == "SELL") {
                m_totalSellQty += qty;
                m_totalSellValue += value;
            }
        }
    }
    
    m_summaryLabel->setText(QString("Trades: <b>%1</b> | "
                                    "Buy Qty: <b style='color: #16a34a;'>%2</b> | "
                                    "Sell Qty: <b style='color: #dc2626;'>%3</b> | "
                                    "Buy Val: <b>₹%4</b> | Sell Val: <b>₹%5</b>")
                            .arg(m_tradeCount)
                            .arg(m_totalBuyQty)
                            .arg(m_totalSellQty)
                            .arg(m_totalBuyValue, 0, 'f', 2)
                            .arg(m_totalSellValue, 0, 'f', 2));

    qDebug() << "[TradeBookWindow] Summary updated";
}

void TradeBookWindow::exportToCSV()
{
    QString fileName = QFileDialog::getSaveFileName(this, 
        "Export TradeBook to CSV", 
        QString("tradebook_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        "CSV Files (*.csv)");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Error", "Could not open file for writing");
        return;
    }
    
    QTextStream out(&file);
    
    // Write headers
    for (int col = 0; col < m_model->columnCount(); ++col) {
        out << m_model->horizontalHeaderItem(col)->text();
        if (col < m_model->columnCount() - 1) {
            out << ",";
        }
    }
    out << "\n";
    
    // Write data (only visible rows)
    for (int row = 0; row < m_model->rowCount(); ++row) {
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
    
    QMessageBox::information(this, "Export Successful", 
        QString("TradeBook exported to:\n%1").arg(fileName));
    
    qDebug() << "[TradeBookWindow] Exported to" << fileName;
}

void TradeBookWindow::setInstrumentFilter(const QString &instrument)
{
    int index = m_instrumentTypeCombo->findText(instrument);
    if (index >= 0) {
        m_instrumentTypeCombo->setCurrentIndex(index);
    }
}

void TradeBookWindow::setTimeFilter(const QDateTime &fromTime, const QDateTime &toTime)
{
    m_fromTimeEdit->setDateTime(fromTime);
    m_toTimeEdit->setDateTime(toTime);
}

void TradeBookWindow::setBuySellFilter(const QString &buySell)
{
    int index = m_buySellCombo->findText(buySell);
    if (index >= 0) {
        m_buySellCombo->setCurrentIndex(index);
    }
}

void TradeBookWindow::setOrderTypeFilter(const QString &orderType)
{
    int index = m_orderTypeCombo->findText(orderType);
    if (index >= 0) {
        m_orderTypeCombo->setCurrentIndex(index);
    }
}

// ============================================================================
// Excel-like Filter Row Implementation
// ============================================================================

void TradeBookWindow::toggleFilterRow()
{
    m_filterRowVisible = !m_filterRowVisible;
    
    if (m_filterRowVisible) {
        // Insert filter row at position 0
        qDebug() << "[TradeBookWindow::toggleFilterRow] Creating filter widgets";
        m_filterWidgets.clear();
        m_model->insertRow(0);
        
        for (int col = 0; col < m_model->columnCount(); ++col) {
            TradeBookFilterWidget* filterWidget = new TradeBookFilterWidget(col, this, m_tableView);
            m_filterWidgets.append(filterWidget);
            m_tableView->setIndexWidget(m_model->index(0, col), filterWidget);
            connect(filterWidget, &TradeBookFilterWidget::filterChanged, this, &TradeBookWindow::onColumnFilterChanged);
            qDebug() << "[TradeBookWindow::toggleFilterRow] Created filter widget for column" << col;
        }
        m_tableView->setRowHeight(0, 36);
        qDebug() << "[TradeBookWindow] Filter row activated (Excel-style) with" << m_filterWidgets.size() << "widgets";
    } else {
        // Clear filter widgets and remove filter row
        qDebug() << "[TradeBookWindow::toggleFilterRow] Clearing filter widgets";
        for (int col = 0; col < m_model->columnCount(); ++col) {
            m_tableView->setIndexWidget(m_model->index(0, col), nullptr);
        }
        qDeleteAll(m_filterWidgets);
        m_filterWidgets.clear();
        m_columnFilters.clear();
        m_model->removeRow(0);
        applyFilters();
        qDebug() << "[TradeBookWindow] Filter row deactivated";
    }
}

void TradeBookWindow::onColumnFilterChanged(int column, const QStringList& selectedValues)
{
    qDebug() << "[TradeBookWindow::onColumnFilterChanged] Column:" << column << "Values:" << selectedValues;
    
    if (selectedValues.isEmpty()) {
        m_columnFilters.remove(column);
    } else {
        m_columnFilters[column] = selectedValues;
    }
    
    applyFilters();
}

QList<QStandardItem*> TradeBookWindow::getTopFilteredTrades() const
{
    QList<QStandardItem*> result;
    int startRow = m_filterRowVisible ? 1 : 0;
    
    for (int row = startRow; row < m_model->rowCount(); ++row) {
        result.append(m_model->item(row, 0));
    }
    
    return result;
}

// ============================================================================
// TradeBookFilterWidget Implementation
// ============================================================================

TradeBookFilterWidget::TradeBookFilterWidget(int column, TradeBookWindow* tradeBookWindow, QWidget* parent)
    : QWidget(parent)
    , m_column(column)
    , m_filterButton(nullptr)
    , m_tradeBookWindow(tradeBookWindow)
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
        "background: #ffffff; color: #cccccc; border: 1px solid #454545; "
        "border-radius: 2px; padding: 4px 8px; text-align: left; font-size: 10px; "
        "}"
        "QPushButton:hover { background: #3e3e42; border-color: #0e639c; }"
        "QPushButton:pressed { background: #0e639c; color: #ffffff; }"
    );
    m_filterButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_filterButton->setMinimumHeight(28);

    layout->addWidget(m_filterButton);

    connect(m_filterButton, &QPushButton::clicked, this, &TradeBookFilterWidget::showFilterPopup);
}

QStringList TradeBookFilterWidget::getUniqueValuesForColumn() const
{
    QSet<QString> uniqueValues;
    
    if (!m_tradeBookWindow) return QStringList();
    
    QStandardItemModel* model = m_tradeBookWindow->m_model;
    int startRow = m_tradeBookWindow->m_filterRowVisible ? 1 : 0;
    
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

void TradeBookFilterWidget::showFilterPopup()
{
    qDebug() << "[TradeBookFilterWidget::showFilterPopup] Opening filter dialog for column" << m_column;
    qDebug() << "[TradeBookFilterWidget::showFilterPopup] Current selected values:" << m_selectedValues;
    
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
                "background: #e3f2fd; color: #1976d2; border: 1px solid #1976d2; "
                "border-radius: 2px; padding: 4px 8px; text-align: left; font-size: 10px; font-weight: bold; "
                "}"
                "QPushButton:hover { background: #bbdefb; border-color: #1565c0; }"
                "QPushButton:pressed { background: #90caf9; }"
            );
        }
        
        emit filterChanged(m_column, m_selectedValues);
    }
    
    dialog->deleteLater();
}

QStringList TradeBookFilterWidget::selectedValues() const
{
    return m_selectedValues;
}

void TradeBookFilterWidget::clear()
{
    m_selectedValues.clear();
    m_filterButton->setText("▼ Filter");
    m_filterButton->setStyleSheet(
        "QPushButton { "
        "background: #ffffff; color: #333333; border: 1px solid #cccccc; "
        "border-radius: 2px; padding: 4px 8px; text-align: left; font-size: 10px; "
        "}"
        "QPushButton:hover { background: #f0f0f0; border-color: #999999; }"
        "QPushButton:pressed { background: #e0e0e0; }"
    );
}

void TradeBookFilterWidget::updateButtonDisplay()
{
    if (m_selectedValues.isEmpty()) {
        m_filterButton->setText("▼ Filter");
    } else {
        m_filterButton->setText(QString("▼ (%1)").arg(m_selectedValues.count()));
    }
}
