#include "views/TradeBookWindow.h"
#include "core/widgets/CustomTradeBook.h"
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

TradeBookWindow::TradeBookWindow(QWidget *parent)
    : QWidget(parent)
    , m_tableView(nullptr)
    , m_model(nullptr)
    , m_filteredModel(nullptr)
    , m_filterRowVisible(false)
    , m_filterShortcut(nullptr)
    , m_totalBuyQty(0.0)
    , m_totalSellQty(0.0)
    , m_totalBuyValue(0.0)
    , m_totalSellValue(0.0)
    , m_tradeCount(0)
{
    setupUI();
    setupConnections();
    loadDummyData();  // TODO: Replace with XTS API call
    
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
    QGroupBox *filterGroup = new QGroupBox("Trade Book Filters");
    filterGroup->setStyleSheet(
        "QGroupBox { "
        "   background-color: #f5f5f5; "
        "   border: 1px solid #d0d0d0; "
        "   border-radius: 4px; "
        "   margin-top: 8px; "
        "   padding-top: 8px; "
        "   font-weight: bold; "
        "}"
        "QGroupBox::title { "
        "   subcontrol-origin: margin; "
        "   subcontrol-position: top left; "
        "   padding: 0 5px; "
        "   color: #333333; "
        "}"
    );

    QVBoxLayout *groupLayout = new QVBoxLayout(filterGroup);
    groupLayout->setSpacing(8);

    // Row 1: Time filter
    QHBoxLayout *timeLayout = new QHBoxLayout();
    timeLayout->addWidget(new QLabel("From:"));
    m_fromTimeEdit = new QDateTimeEdit();
    m_fromTimeEdit->setCalendarPopup(true);
    m_fromTimeEdit->setDateTime(QDateTime::currentDateTime().addDays(-7));
    m_fromTimeEdit->setDisplayFormat("dd-MM-yyyy hh:mm");
    m_fromTimeEdit->setMinimumWidth(150);
    timeLayout->addWidget(m_fromTimeEdit);

    timeLayout->addWidget(new QLabel("To:"));
    m_toTimeEdit = new QDateTimeEdit();
    m_toTimeEdit->setCalendarPopup(true);
    m_toTimeEdit->setDateTime(QDateTime::currentDateTime());
    m_toTimeEdit->setDisplayFormat("dd-MM-yyyy hh:mm");
    m_toTimeEdit->setMinimumWidth(150);
    timeLayout->addWidget(m_toTimeEdit);
    
    timeLayout->addStretch();
    groupLayout->addLayout(timeLayout);

    // Row 2: Other filters
    QHBoxLayout *filterLayout = new QHBoxLayout();
    
    filterLayout->addWidget(new QLabel("Instrument:"));
    m_instrumentTypeCombo = new QComboBox();
    m_instrumentTypeCombo->addItems({"All", "NSE OPT", "NSE FUT", "NSE EQ", "BSE EQ"});
    m_instrumentTypeCombo->setMinimumWidth(100);
    filterLayout->addWidget(m_instrumentTypeCombo);

    filterLayout->addWidget(new QLabel("Exchange:"));
    m_exchangeCombo = new QComboBox();
    m_exchangeCombo->addItems({"All", "NSE", "BSE", "MCX"});
    m_exchangeCombo->setMinimumWidth(80);
    filterLayout->addWidget(m_exchangeCombo);

    filterLayout->addWidget(new QLabel("Buy/Sell:"));
    m_buySellCombo = new QComboBox();
    m_buySellCombo->addItems({"All", "Buy", "Sell"});
    m_buySellCombo->setMinimumWidth(80);
    filterLayout->addWidget(m_buySellCombo);

    filterLayout->addWidget(new QLabel("Order Type:"));
    m_orderTypeCombo = new QComboBox();
    m_orderTypeCombo->addItems({"All", "Day", "IOC", "GTC"});
    m_orderTypeCombo->setMinimumWidth(80);
    filterLayout->addWidget(m_orderTypeCombo);

    filterLayout->addStretch();

    // Action buttons
    m_applyFilterBtn = new QPushButton("Apply Filter");
    m_applyFilterBtn->setStyleSheet(
        "QPushButton { background-color: #2196F3; color: white; padding: 6px 16px; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #1976D2; }"
        "QPushButton:pressed { background-color: #0D47A1; }"
    );
    filterLayout->addWidget(m_applyFilterBtn);

    m_clearFilterBtn = new QPushButton("Clear");
    m_clearFilterBtn->setStyleSheet(
        "QPushButton { background-color: #757575; color: white; padding: 6px 16px; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #616161; }"
    );
    filterLayout->addWidget(m_clearFilterBtn);

    m_exportBtn = new QPushButton("Export CSV");
    m_exportBtn->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; padding: 6px 16px; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #45a049; }"
    );
    filterLayout->addWidget(m_exportBtn);

    groupLayout->addLayout(filterLayout);

    return filterGroup;
}

QWidget* TradeBookWindow::createSummaryWidget()
{
    QWidget *summaryWidget = new QWidget();
    summaryWidget->setStyleSheet("background-color: #e3f2fd; border-top: 2px solid #2196F3;");
    summaryWidget->setFixedHeight(40);

    QHBoxLayout *summaryLayout = new QHBoxLayout(summaryWidget);
    summaryLayout->setContentsMargins(10, 5, 10, 5);

    m_showSummaryCheck = new QCheckBox("Show Summary");
    m_showSummaryCheck->setChecked(true);
    summaryLayout->addWidget(m_showSummaryCheck);

    summaryLayout->addWidget(new QLabel("|"));

    QLabel *tradeCountLabel = new QLabel(QString("Trades: <b>%1</b>").arg(m_tradeCount));
    summaryLayout->addWidget(tradeCountLabel);

    summaryLayout->addWidget(new QLabel("|"));

    QLabel *buyQtyLabel = new QLabel(QString("Buy Qty: <b>%1</b>").arg(m_totalBuyQty, 0, 'f', 0));
    buyQtyLabel->setStyleSheet("color: #4CAF50;");
    summaryLayout->addWidget(buyQtyLabel);

    QLabel *sellQtyLabel = new QLabel(QString("Sell Qty: <b>%1</b>").arg(m_totalSellQty, 0, 'f', 0));
    sellQtyLabel->setStyleSheet("color: #F44336;");
    summaryLayout->addWidget(sellQtyLabel);

    summaryLayout->addWidget(new QLabel("|"));

    QLabel *buyValueLabel = new QLabel(QString("Buy Value: <b>₹%1</b>").arg(m_totalBuyValue, 0, 'f', 2));
    buyValueLabel->setStyleSheet("color: #4CAF50;");
    summaryLayout->addWidget(buyValueLabel);

    QLabel *sellValueLabel = new QLabel(QString("Sell Value: <b>₹%1</b>").arg(m_totalSellValue, 0, 'f', 2));
    sellValueLabel->setStyleSheet("color: #F44336;");
    summaryLayout->addWidget(sellValueLabel);

    summaryLayout->addStretch();

    return summaryWidget;
}

void TradeBookWindow::setupFilterBar()
{
    // Filter widgets created in setupUI
}

void TradeBookWindow::setupTable()
{
    m_tableView = new CustomTradeBook(this);
    
    // Create model
    m_model = new QStandardItemModel(this);
    
    // Define columns matching reference image
    QStringList headers;
    headers << "User" << "Group" << "Exc..." << "Membe..." << "TradeId" 
            << "Instrumen..." << "Instrumen..." << "Code" << "Symbol/Sc..." 
            << "Spread Sy..." << "Ser/Exp/G..." << "Strike Pri..." << "Option Type"
            << "O..." << "B..." << "Quantity" << "Price" << "Time" 
            << "Spread ..." << "Spread" << "P..." << "Client" << "Client Name"
            << "Exch. Order No.";
    
    m_model->setHorizontalHeaderLabels(headers);
    m_tableView->setModel(m_model);
    
    // Set column widths
    QHeaderView *header = m_tableView->horizontalHeader();
    header->setDefaultSectionSize(90);
    header->resizeSection(0, 80);   // User
    header->resizeSection(1, 60);   // Group
    header->resizeSection(4, 100);  // TradeId
    header->resizeSection(8, 120);  // Symbol
    header->resizeSection(15, 80);  // Quantity
    header->resizeSection(16, 80);  // Price
    header->resizeSection(17, 100); // Time
}

void TradeBookWindow::setupConnections()
{
    connect(m_applyFilterBtn, &QPushButton::clicked, this, &TradeBookWindow::applyFilters);
    connect(m_clearFilterBtn, &QPushButton::clicked, this, &TradeBookWindow::clearFilters);
    connect(m_exportBtn, &QPushButton::clicked, this, &TradeBookWindow::exportToCSV);
    connect(m_showSummaryCheck, &QCheckBox::toggled, m_tableView, &CustomTradeBook::setSummaryRowEnabled);
}

void TradeBookWindow::loadDummyData()
{
    // Clear existing data
    m_model->removeRows(0, m_model->rowCount());
    
    // Add sample trades (TODO: Replace with XTS API call)
    QStringList sampleTrades = {
        "YASH12|DEFAULT|NSE|NSE|T12345|NSE OPT|OPTSTK|43110|NIFTY 30DEC24 26250 CE|-----|30DEC24|26250.00|CE|PRO|BUY|100|15.50|26-12-2025 09:15:30|0.00|0.00|0.00|CLI001|Client A|1234567",
        "YASH12|DEFAULT|NSE|NSE|T12346|NSE FUT|FUTIDX|43111|NIFTY 30JAN25|-----|30JAN25|0.00|---|PRO|SELL|50|26150.00|26-12-2025 09:20:15|0.00|0.00|0.00|CLI002|Client B|1234568",
        "YASH12|DEFAULT|NSE|NSE|T12347|NSE OPT|OPTSTK|43112|NIFTY 30DEC24 26500 PE|-----|30DEC24|26500.00|PE|PRO|BUY|200|25.75|26-12-2025 10:30:45|0.00|0.00|0.00|CLI001|Client A|1234569",
    };
    
    for (const QString &trade : sampleTrades) {
        QStringList fields = trade.split("|");
        QList<QStandardItem*> row;
        for (const QString &field : fields) {
            QStandardItem *item = new QStandardItem(field);
            item->setEditable(false);
            
            // Color coding for Buy/Sell
            if (field == "BUY") {
                item->setForeground(QBrush(QColor("#4CAF50")));
                item->setData(QFont(item->font().family(), item->font().pointSize(), QFont::Bold), Qt::FontRole);
            } else if (field == "SELL") {
                item->setForeground(QBrush(QColor("#F44336")));
                item->setData(QFont(item->font().family(), item->font().pointSize(), QFont::Bold), Qt::FontRole);
            }
            
            row.append(item);
        }
        m_model->appendRow(row);
    }
    
    updateSummary();
    
    qDebug() << "[TradeBookWindow] Loaded" << m_model->rowCount() << "dummy trades";
}

void TradeBookWindow::refreshTrades()
{
    qDebug() << "[TradeBookWindow] Refreshing trades...";
    // TODO: Call XTS API to fetch latest trades
    loadDummyData();
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
    
    qDebug() << "[TradeBookWindow] Summary - Trades:" << m_tradeCount
             << "Buy:" << m_totalBuyQty << "@" << m_totalBuyValue
             << "Sell:" << m_totalSellQty << "@" << m_totalSellValue;
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
        "background: #FFFFFF; color: #333333; border: 1px solid rgba(0,0,0,0.15); "
        "border-radius: 3px; padding: 4px 8px; text-align: left; font-size: 10px; "
        "}"
        "QPushButton:hover { background: #F8F8F8; border-color: #4A90E2; }"
        "QPushButton:pressed { background: #E8E8E8; }"
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
        "background: #FFFFFF; color: #333333; border: 1px solid rgba(0,0,0,0.15); "
        "border-radius: 3px; padding: 4px 8px; text-align: left; font-size: 10px; "
        "}"
        "QPushButton:hover { background: #F8F8F8; border-color: #4A90E2; }"
        "QPushButton:pressed { background: #E8E8E8; }"
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
