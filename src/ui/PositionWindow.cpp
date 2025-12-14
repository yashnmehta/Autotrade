#include "ui/PositionWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QCheckBox>
#include <QListWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QDebug>

// ============================================================================
// PositionWindow Implementation
// ============================================================================

PositionWindow::PositionWindow(QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_filterRowVisible(false)
    , m_filterShortcut(nullptr)
{
    setupUI();
    loadSampleData();
    
    // Setup Ctrl+F shortcut for filter toggle
    m_filterShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(m_filterShortcut, &QShortcut::activated, this, &PositionWindow::toggleFilterRow);
    
    qDebug() << "[PositionWindow] Created successfully - Press Ctrl+F to toggle filters";
}

PositionWindow::~PositionWindow()
{
}

void PositionWindow::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // Setup filter bar (top controls) and column filter bar (per-column inputs)
    setupFilterBar();
    mainLayout->addWidget(m_topFilterWidget);
    // Column-level filter bar will be created by setupFilterBar and initially hidden
    mainLayout->addWidget(m_columnFilterBar);

    // Setup table view
    setupTableView();
    mainLayout->addWidget(m_tableView);
}

void PositionWindow::setupFilterBar()
{
    QWidget* filterWidget = new QWidget(this);
    QHBoxLayout* filterLayout = new QHBoxLayout(filterWidget);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(10);

    // Exchange
    QLabel* lblExchange = new QLabel("Exchange:", this);
    m_cbExchange = new QComboBox(this);
    m_cbExchange->addItem("(ALL)");
    m_cbExchange->addItems({"NSE", "BSE", "MCX"});
    m_cbExchange->setMinimumWidth(80);
    connect(m_cbExchange, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PositionWindow::onFilterChanged);

    // Segment
    QLabel* lblSegment = new QLabel("Mkt Segment:", this);
    m_cbSegment = new QComboBox(this);
    m_cbSegment->addItem("(ALL)");
    m_cbSegment->addItems({"CM", "FO", "CD"});
    m_cbSegment->setMinimumWidth(80);
    connect(m_cbSegment, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PositionWindow::onFilterChanged);

    // Periodicity
    QLabel* lblPeriodicity = new QLabel("Periodicity:", this);
    m_cbPeriodicity = new QComboBox(this);
    m_cbPeriodicity->addItem("Daily");
    m_cbPeriodicity->addItem("Weekly");
    m_cbPeriodicity->addItem("Monthly");
    m_cbPeriodicity->setMinimumWidth(80);
    connect(m_cbPeriodicity, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PositionWindow::onFilterChanged);

    // User
    QLabel* lblUser = new QLabel("User:", this);
    m_cbUser = new QComboBox(this);
    m_cbUser->addItem("MEMBER");
    m_cbUser->addItem("Admin");
    m_cbUser->addItem("Trader1");
    m_cbUser->setMinimumWidth(100);
    connect(m_cbUser, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PositionWindow::onFilterChanged);

    // Client
    QLabel* lblClient = new QLabel("Client:", this);
    m_cbClient = new QComboBox(this);
    m_cbClient->addItem("(ALL)");
    m_cbClient->addItems({"CLIENT001", "CLIENT002", "CLIENT003"});
    m_cbClient->setMinimumWidth(100);
    connect(m_cbClient, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PositionWindow::onFilterChanged);

    // Security
    QLabel* lblSecurity = new QLabel("Security/Contract:", this);
    m_cbSecurity = new QComboBox(this);
    m_cbSecurity->addItem("(ALL)");
    m_cbSecurity->setMinimumWidth(120);
    m_cbSecurity->setEditable(true);
    connect(m_cbSecurity, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PositionWindow::onFilterChanged);

    // Buttons
    m_btnRefresh = new QPushButton("Refresh", this);
    m_btnExport = new QPushButton("Export", this);
    connect(m_btnRefresh, &QPushButton::clicked, this, &PositionWindow::onRefreshClicked);
    connect(m_btnExport, &QPushButton::clicked, this, &PositionWindow::onExportClicked);

    // Add widgets to layout
    filterLayout->addWidget(lblExchange);
    filterLayout->addWidget(m_cbExchange);
    filterLayout->addWidget(lblSegment);
    filterLayout->addWidget(m_cbSegment);
    filterLayout->addWidget(lblPeriodicity);
    filterLayout->addWidget(m_cbPeriodicity);
    filterLayout->addWidget(lblUser);
    filterLayout->addWidget(m_cbUser);
    filterLayout->addWidget(lblClient);
    filterLayout->addWidget(m_cbClient);
    filterLayout->addWidget(lblSecurity);
    filterLayout->addWidget(m_cbSecurity);
    filterLayout->addStretch();
    filterLayout->addWidget(m_btnRefresh);
    filterLayout->addWidget(m_btnExport);

    // Save top filter widget (contains Exchange/Segment/Periodicity/User/Client/Security)
    m_topFilterWidget = filterWidget;

    // Create column-level filter bar (per-column inputs) - hidden by default
    m_columnFilterBar = new QWidget(this);
    QHBoxLayout* colLayout = new QHBoxLayout(m_columnFilterBar);
    colLayout->setContentsMargins(2, 2, 2, 2);
    colLayout->setSpacing(6);
    m_columnFilterBar->setStyleSheet("background-color: #F5F5F5; border-bottom: 1px solid rgba(0,0,0,0.12);");

    for (int col = 0; col < PositionModel::ColumnCount; ++col) {
        QLineEdit* le = new QLineEdit(m_columnFilterBar);
        le->setPlaceholderText("Filter...");
        le->setClearButtonEnabled(true);
        le->setMinimumHeight(30);
        le->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        le->setStyleSheet("QLineEdit { background: #FFFFFF; color: #111111; border: 1px solid rgba(0,0,0,0.12); border-radius:4px; padding:4px 8px; }");
        connect(le, &QLineEdit::textChanged, [this, col](const QString& txt){
            onColumnFilterChanged(col, txt);
        });
        colLayout->addWidget(le);
    }

    m_columnFilterBar->setVisible(false);
}

void PositionWindow::setupTableView()
{
    m_tableView = new QTableView(this);
    m_model = new PositionModel(this);
    m_tableView->setModel(m_model);

    // Table view properties
    // Make table background white and disable alternating rows and grid
    m_tableView->setStyleSheet("QTableView { background-color: #FFFFFF; }");
    m_tableView->setAlternatingRowColors(false);
    m_tableView->setShowGrid(false);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setSortingEnabled(true);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->verticalHeader()->setVisible(false);
    
    // Set column widths
    m_tableView->setColumnWidth(PositionModel::Symbol, 120);
    m_tableView->setColumnWidth(PositionModel::SeriesExpiry, 80);
    m_tableView->setColumnWidth(PositionModel::BuyQty, 80);
    m_tableView->setColumnWidth(PositionModel::SellQty, 80);
    m_tableView->setColumnWidth(PositionModel::NetPrice, 90);
    m_tableView->setColumnWidth(PositionModel::MarkPrice, 90);
    m_tableView->setColumnWidth(PositionModel::MTMGainLoss, 100);
    m_tableView->setColumnWidth(PositionModel::MTMMargin, 100);
    m_tableView->setColumnWidth(PositionModel::BuyValue, 100);
    m_tableView->setColumnWidth(PositionModel::SellValue, 100);
    
    // Note: column-level filter widgets are created in setupFilterBar() and toggled separately
}

void PositionWindow::addPosition(const Position& position)
{
    m_allPositions.append(position);
    applyFilters();
}

void PositionWindow::updatePosition(const QString& symbol, const Position& position)
{
    for (int i = 0; i < m_allPositions.size(); ++i) {
        if (m_allPositions[i].symbol == symbol) {
            m_allPositions[i] = position;
            applyFilters();
            return;
        }
    }
    addPosition(position);
}

void PositionWindow::clearPositions()
{
    m_allPositions.clear();
    applyFilters();
}

void PositionWindow::applyFilters()
{
    QList<Position> filteredPositions;
    
    for (const Position& pos : m_allPositions) {
        // Apply top filter bar filters
        if (m_filterExchange != "(ALL)" && !m_filterExchange.isEmpty() && pos.exchange != m_filterExchange)
            continue;
        if (m_filterSegment != "(ALL)" && !m_filterSegment.isEmpty() && pos.segment != m_filterSegment)
            continue;
        if (!m_filterUser.isEmpty() && pos.user != m_filterUser)
            continue;
        if (m_filterClient != "(ALL)" && !m_filterClient.isEmpty() && pos.client != m_filterClient)
            continue;
        if (m_filterSecurity != "(ALL)" && !m_filterSecurity.isEmpty() && pos.symbol != m_filterSecurity)
            continue;
        
        // Apply column filters (text-based search)
        bool passColumnFilters = true;
        for (auto it = m_columnFilters.constBegin(); it != m_columnFilters.constEnd(); ++it) {
            int column = it.key();
            QString filterText = it.value();
            if (filterText.isEmpty()) continue;
            
            QString cellValue;
            switch (column) {
                case PositionModel::Symbol: cellValue = pos.symbol; break;
                case PositionModel::SeriesExpiry: cellValue = pos.seriesExpiry; break;
                case PositionModel::BuyQty: cellValue = QString::number(pos.buyQty); break;
                case PositionModel::SellQty: cellValue = QString::number(pos.sellQty); break;
                case PositionModel::NetPrice: cellValue = QString::number(pos.netPrice, 'f', 2); break;
                case PositionModel::MarkPrice: cellValue = QString::number(pos.markPrice, 'f', 2); break;
                case PositionModel::MTMGainLoss: cellValue = QString::number(pos.mtmGainLoss, 'f', 2); break;
                case PositionModel::MTMMargin: cellValue = QString::number(pos.mtmMargin, 'f', 2); break;
                case PositionModel::BuyValue: cellValue = QString::number(pos.buyValue, 'f', 2); break;
                case PositionModel::SellValue: cellValue = QString::number(pos.sellValue, 'f', 2); break;
                default: break;
            }
            
            if (!cellValue.contains(filterText, Qt::CaseInsensitive)) {
                passColumnFilters = false;
                break;
            }
        }
        
        if (passColumnFilters) {
            filteredPositions.append(pos);
        }
    }
    
    m_model->setPositions(filteredPositions);
    updateSummaryRow();
}

void PositionWindow::updateSummaryRow()
{
    Position summary;
    summary.symbol = "(ALL)";
    summary.seriesExpiry = "(ALL)";
    
    QList<Position> positions = m_model->positions();
    for (const Position& pos : positions) {
        summary.buyQty += pos.buyQty;
        summary.sellQty += pos.sellQty;
        summary.mtmGainLoss += pos.mtmGainLoss;
        summary.mtmMargin += pos.mtmMargin;
        summary.buyValue += pos.buyValue;
        summary.sellValue += pos.sellValue;
    }
    
    m_model->setSummary(summary);
}

void PositionWindow::onFilterChanged()
{
    m_filterExchange = m_cbExchange->currentText();
    m_filterSegment = m_cbSegment->currentText();
    m_filterPeriodicity = m_cbPeriodicity->currentText();
    m_filterUser = m_cbUser->currentText();
    m_filterClient = m_cbClient->currentText();
    m_filterSecurity = m_cbSecurity->currentText();
    
    applyFilters();
}

void PositionWindow::onColumnFilterChanged(int column, const QString& filterText)
{
    if (filterText.isEmpty()) {
        m_columnFilters.remove(column);
    } else {
        m_columnFilters[column] = filterText;
    }
    applyFilters();
}

void PositionWindow::toggleFilterRow()
{
    m_filterRowVisible = !m_filterRowVisible;
    m_model->setFilterRowVisible(m_filterRowVisible);
    
    if (m_filterRowVisible) {
        // Show the dedicated column filter bar above the table
        if (m_columnFilterBar) m_columnFilterBar->setVisible(true);
        qDebug() << "[PositionWindow] Column filter bar shown";
    } else {
        // Hide and clear filter inputs
        if (m_columnFilterBar) {
            QList<QLineEdit*> edits = m_columnFilterBar->findChildren<QLineEdit*>();
            for (QLineEdit* e : edits) e->clear();
            m_columnFilterBar->setVisible(false);
        }
        m_columnFilters.clear();
        applyFilters();
        qDebug() << "[PositionWindow] Column filter bar hidden";
    }
}

void PositionWindow::onRefreshClicked()
{
    qDebug() << "[PositionWindow] Refresh clicked";
    applyFilters();
}

void PositionWindow::onExportClicked()
{
    qDebug() << "[PositionWindow] Export clicked";
    QMessageBox::information(this, "Export", "Export functionality will be implemented.");
}

void PositionWindow::loadSampleData()
{
    // Sample data matching the image
    Position p1;
    p1.symbol = "BAHINDOA"; p1.seriesExpiry = "EQ"; p1.buyQty = 15; p1.sellQty = 15;
    p1.netPrice = 0.0; p1.markPrice = 345.40; p1.mtmGainLoss = -3.00;
    p1.mtmMargin = 0.00; p1.buyValue = 5182.50; p1.sellValue = 5179.50;
    p1.exchange = "NSE"; p1.segment = "CM"; p1.user = "MEMBER"; p1.client = "CLIENT001";
    m_allPositions.append(p1);

    Position p2;
    p2.symbol = "CANEX"; p2.seriesExpiry = "EQ"; p2.buyQty = 5; p2.sellQty = 5;
    p2.netPrice = 0.0; p2.markPrice = 427.00; p2.mtmGainLoss = -1.25;
    p2.mtmMargin = 0.00; p2.buyValue = 2121.00; p2.sellValue = 2119.25;
    p2.exchange = "NSE"; p2.segment = "CM"; p2.user = "MEMBER"; p2.client = "CLIENT001";
    m_allPositions.append(p2);

    Position p3;
    p3.symbol = "DCB"; p3.seriesExpiry = "EQ"; p3.buyQty = 25; p3.sellQty = 20;
    p3.netPrice = 44.60; p3.markPrice = 44.45; p3.mtmGainLoss = -0.75;
    p3.mtmMargin = 222.25; p3.buyValue = 1110.00; p3.sellValue = 887.00;
    p3.exchange = "NSE"; p3.segment = "CM"; p3.user = "MEMBER"; p3.client = "CLIENT002";
    m_allPositions.append(p3);

    Position p4;
    p4.symbol = "CORIBANK"; p4.seriesExpiry = "EQ"; p4.buyQty = 50; p4.sellQty = 40;
    p4.netPrice = 100.55; p4.markPrice = 99.10; p4.mtmGainLoss = -7.50;
    p4.mtmMargin = 990.00; p4.buyValue = 5007.50; p4.sellValue = 4002.00;
    p4.exchange = "NSE"; p4.segment = "CM"; p4.user = "MEMBER"; p4.client = "CLIENT002";
    m_allPositions.append(p4);

    Position p5;
    p5.symbol = "HDPCBANK"; p5.seriesExpiry = "EQ"; p5.buyQty = 15; p5.sellQty = 10;
    p5.netPrice = 570.90; p5.markPrice = 577.90; p5.mtmGainLoss = 35.00;
    p5.mtmMargin = 2889.50; p5.buyValue = 8642.00; p5.sellValue = 5707.50;
    p5.exchange = "NSE"; p5.segment = "CM"; p5.user = "MEMBER"; p5.client = "CLIENT003";
    m_allPositions.append(p5);

    Position p6;
    p6.symbol = "INDYSTACK"; p6.seriesExpiry = "EQ"; p6.buyQty = 1; p6.sellQty = 1;
    p6.netPrice = 0.0; p6.markPrice = 372.05; p6.mtmGainLoss = -0.60;
    p6.mtmMargin = 0.00; p6.buyValue = 374.85; p6.sellValue = 374.25;
    p6.exchange = "NSE"; p6.segment = "CM"; p6.user = "MEMBER"; p6.client = "CLIENT003";
    m_allPositions.append(p6);

    Position p7;
    p7.symbol = "RELGOLD"; p7.seriesExpiry = "EQ"; p7.buyQty = 30; p7.sellQty = 10;
    p7.netPrice = 2065.35; p7.markPrice = 2757.75; p7.mtmGainLoss = 13848.00;
    p7.mtmMargin = 55155.00; p7.buyValue = 41307.00; p7.sellValue = 0.00;
    p7.exchange = "BSE"; p7.segment = "CM"; p7.user = "MEMBER"; p7.client = "CLIENT001";
    m_allPositions.append(p7);

    Position p8;
    p8.symbol = "SSBJ"; p8.seriesExpiry = "EQ"; p8.buyQty = 0; p8.sellQty = 15;
    p8.netPrice = 0.00; p8.markPrice = 384.90; p8.mtmGainLoss = -5773.50;
    p8.mtmMargin = -5773.50; p8.buyValue = 0.00; p8.sellValue = 0.00;
    p8.exchange = "BSE"; p8.segment = "CM"; p8.user = "MEMBER"; p8.client = "CLIENT002";
    m_allPositions.append(p8);

    Position p9;
    p9.symbol = "SBIN"; p9.seriesExpiry = "EQ"; p9.buyQty = 25; p9.sellQty = 10;
    p9.netPrice = 2203.95; p9.markPrice = 2208.60; p9.mtmGainLoss = 69.75;
    p9.mtmMargin = 33129.00; p9.buyValue = 55098.75; p9.sellValue = 22039.50;
    p9.exchange = "NSE"; p9.segment = "CM"; p9.user = "MEMBER"; p9.client = "CLIENT003";
    m_allPositions.append(p9);

    applyFilters();
}

// ============================================================================
// PositionModel Implementation
// ============================================================================

PositionModel::PositionModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_showSummary(true)
    , m_filterRowVisible(false)
{
}

int PositionModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    int rows = m_positions.size();
    if (m_filterRowVisible) rows += 1; // +1 for filter row
    if (m_showSummary) rows += 1; // +1 for summary row
    return rows;
}

int PositionModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant PositionModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int row = index.row();
    int col = index.column();
    
    // Filter row (row 0 when visible)
    if (m_filterRowVisible && row == 0) {
        if (role == Qt::BackgroundRole) {
            return QColor(240, 248, 255); // Light blue background
        }
        return QVariant();
    }
    
    // Adjust row index if filter row is visible
    int dataRow = m_filterRowVisible ? row - 1 : row;
    
    // Summary row
    bool isSummaryRow = m_showSummary && dataRow == m_positions.size();
    const Position& pos = isSummaryRow ? m_summary : m_positions.at(dataRow);

    if (role == Qt::DisplayRole) {
        switch (col) {
            case Symbol: return pos.symbol;
            case SeriesExpiry: return pos.seriesExpiry;
            case BuyQty: return pos.buyQty > 0 ? QString::number(pos.buyQty) : QString();
            case SellQty: return pos.sellQty > 0 ? QString::number(pos.sellQty) : QString();
            case NetPrice: return pos.netPrice != 0.0 ? QString::number(pos.netPrice, 'f', 2) : QString();
            case MarkPrice: return pos.markPrice != 0.0 ? QString::number(pos.markPrice, 'f', 2) : QString();
            case MTMGainLoss: return QString::number(pos.mtmGainLoss, 'f', 2);
            case MTMMargin: return QString::number(pos.mtmMargin, 'f', 2);
            case BuyValue: return pos.buyValue != 0.0 ? QString::number(pos.buyValue, 'f', 2) : QString();
            case SellValue: return pos.sellValue != 0.0 ? QString::number(pos.sellValue, 'f', 2) : QString();
        }
    }
    else if (role == Qt::TextAlignmentRole) {
        if (col == Symbol || col == SeriesExpiry)
            return Qt::AlignLeft + Qt::AlignVCenter;
        return Qt::AlignRight + Qt::AlignVCenter;
    }
    else if (role == Qt::ForegroundRole) {
        if (col == MTMGainLoss) {
            return pos.mtmGainLoss > 0 ? QColor(0, 150, 0) : 
                   pos.mtmGainLoss < 0 ? QColor(200, 0, 0) : QColor(0, 0, 0);
        }
    }
    else if (role == Qt::BackgroundRole) {
        if (isSummaryRow) {
            return QColor(230, 230, 230);
        }
    }
    else if (role == Qt::FontRole) {
        if (isSummaryRow) {
            QFont font;
            font.setBold(true);
            return font;
        }
    }

    return QVariant();
}

QVariant PositionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case Symbol: return "Symbol";
            case SeriesExpiry: return "Ser/Exp";
            case BuyQty: return "Buy Qty";
            case SellQty: return "Sell Qty";
            case NetPrice: return "Net Pr...";
            case MarkPrice: return "Mark...";
            case MTMGainLoss: return "MTM GA";
            case MTMMargin: return "MTM-M...";
            case BuyValue: return "Buy Val";
            case SellValue: return "Sell Val";
        }
    }
    return QVariant();
}

void PositionModel::setPositions(const QList<Position>& positions)
{
    beginResetModel();
    m_positions = positions;
    endResetModel();
}

void PositionModel::setSummary(const Position& summary)
{
    m_summary = summary;
    m_showSummary = true;
    
    // Update summary row
    if (!m_positions.isEmpty()) {
        int summaryRow = m_positions.size() + (m_filterRowVisible ? 1 : 0);
        emit dataChanged(index(summaryRow, 0), index(summaryRow, ColumnCount - 1));
    }
}

void PositionModel::setFilterRowVisible(bool visible)
{
    if (m_filterRowVisible != visible) {
        if (visible) {
            beginInsertRows(QModelIndex(), 0, 0);
            m_filterRowVisible = visible;
            endInsertRows();
        } else {
            beginRemoveRows(QModelIndex(), 0, 0);
            m_filterRowVisible = visible;
            endRemoveRows();
        }
    }
}

// ============================================================================
// FilterRowWidget Implementation
// ============================================================================

FilterRowWidget::FilterRowWidget(int column, QWidget* parent)
    : QWidget(parent)
    , m_column(column)
    , m_lineEdit(nullptr)
    , m_comboBox(nullptr)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Visual container: slightly lighter than header to separate from dark chrome
    this->setStyleSheet("background-color: #F5F5F5; border-bottom: 1px solid rgba(0,0,0,0.12);");

    // Use line edit for text filtering
    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setPlaceholderText("Filter...");
    m_lineEdit->setClearButtonEnabled(true);
    m_lineEdit->setMinimumHeight(30);
    m_lineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    // Make the edit visually distinct: white background, darker text, padding
    m_lineEdit->setStyleSheet(
        "QLineEdit { background: #FFFFFF; color: #111111; border: 1px solid rgba(0,0,0,0.12); "
        "border-radius: 4px; padding: 4px 8px; }");

    layout->addWidget(m_lineEdit);

    connect(m_lineEdit, &QLineEdit::textChanged, [this](const QString& text) {
        emit filterChanged(m_column, text);
    });
}

QString FilterRowWidget::filterText() const
{
    return m_lineEdit ? m_lineEdit->text() : QString();
}

void FilterRowWidget::clear()
{
    if (m_lineEdit) {
        m_lineEdit->clear();
    }
}
