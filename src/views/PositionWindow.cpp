#include "views/PositionWindow.h"
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

    // Setup filter bar (top controls)
    setupFilterBar();
    mainLayout->addWidget(m_topFilterWidget);

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
    m_cbSegment->addItems({"E", "F", "O"});
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

    // Save top filter widget and apply styling for better visibility
    m_topFilterWidget = filterWidget;
    m_topFilterWidget->setStyleSheet(
        "QWidget { background-color: #F5F5F5; padding: 8px; border-radius: 4px; }"
        "QLabel { color: #333333; font-weight: 500; }"
        "QComboBox { background: #FFFFFF; color: #111111; border: 1px solid #CCCCCC; "
        "border-radius: 3px; padding: 4px 8px; min-height: 24px; }"
        "QComboBox:hover { border-color: #999999; }"
        "QComboBox::drop-down { border: none; }"
        "QPushButton { background: #4A90E2; color: white; border: none; "
        "border-radius: 3px; padding: 6px 16px; font-weight: 500; }"
        "QPushButton:hover { background: #357ABD; }"
        "QPushButton:pressed { background: #2868A6; }"
    );
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
    
    // Style the header for better visibility
    m_tableView->horizontalHeader()->setStyleSheet(
        "QHeaderView::section { "
        "background-color: #2C2C2C; "
        "color: #FFFFFF; "
        "padding: 6px; "
        "border: 1px solid #1A1A1A; "
        "font-weight: bold; "
        "font-size: 11px; "
        "}"
        "QHeaderView::section:hover { "
        "background-color: #3A3A3A; "
        "}"
    );
    m_tableView->horizontalHeader()->setMinimumHeight(32);
    
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

QList<Position> PositionWindow::getTopFilteredPositions() const
{
    QList<Position> filtered;
    
    for (const Position& pos : m_allPositions) {
        // Apply top filter bar filters only
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
        
        filtered.append(pos);
    }
    
    return filtered;
}

void PositionWindow::applyFilters()
{
    QList<Position> filteredPositions;
    
    // First apply top filters
    QList<Position> topFiltered = getTopFilteredPositions();
    
    // Then apply column filters
    for (const Position& pos : topFiltered) {
        // Apply column filters (multi-select)
        bool passColumnFilters = true;
        for (auto it = m_columnFilters.constBegin(); it != m_columnFilters.constEnd(); ++it) {
            int column = it.key();
            QStringList selectedValues = it.value();
            if (selectedValues.isEmpty()) continue;
            
            QString cellValue;
            switch (column) {
                case PositionModel::Symbol: cellValue = pos.symbol; break;
                case PositionModel::SeriesExpiry: cellValue = pos.seriesExpiry; break;
                case PositionModel::BuyQty: cellValue = pos.buyQty > 0 ? QString::number(pos.buyQty) : ""; break;
                case PositionModel::SellQty: cellValue = pos.sellQty > 0 ? QString::number(pos.sellQty) : ""; break;
                case PositionModel::NetPrice: cellValue = pos.netPrice != 0.0 ? QString::number(pos.netPrice, 'f', 2) : ""; break;
                case PositionModel::MarkPrice: cellValue = pos.markPrice != 0.0 ? QString::number(pos.markPrice, 'f', 2) : ""; break;
                case PositionModel::MTMGainLoss: cellValue = QString::number(pos.mtmGainLoss, 'f', 2); break;
                case PositionModel::MTMMargin: cellValue = QString::number(pos.mtmMargin, 'f', 2); break;
                case PositionModel::BuyValue: cellValue = pos.buyValue != 0.0 ? QString::number(pos.buyValue, 'f', 2) : ""; break;
                case PositionModel::SellValue: cellValue = pos.sellValue != 0.0 ? QString::number(pos.sellValue, 'f', 2) : ""; break;
                default: break;
            }
            
            // Check if cellValue is in selected values
            if (!selectedValues.contains(cellValue)) {
                passColumnFilters = false;
                break;
            }
        }
        
        if (passColumnFilters) {
            filteredPositions.append(pos);
        }
    }
    
    qDebug() << "[PositionWindow::applyFilters] Filtered" << filteredPositions.size() << "positions from" << m_allPositions.size();
    qDebug() << "[PositionWindow::applyFilters] Filter row visible:" << m_filterRowVisible;
    
    m_model->setPositions(filteredPositions);
    updateSummaryRow();
    
    // Recreate filter widgets after model reset (model reset deletes all index widgets)
    if (m_filterRowVisible) {
        qDebug() << "[PositionWindow::applyFilters] Recreating filter widgets after model reset";
        recreateFilterWidgets();
    }
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

void PositionWindow::recreateFilterWidgets()
{
    if (!m_filterRowVisible) {
        qDebug() << "[PositionWindow::recreateFilterWidgets] Skipping - filter row not visible";
        return;
    }
    
    qDebug() << "[PositionWindow::recreateFilterWidgets] Recreating filter widgets";
    
    // Store current filter selections before recreating widgets
    QMap<int, QStringList> currentFilters = m_columnFilters;
    
    // Clear old widget list (widgets are already deleted by Qt during model reset)
    m_filterWidgets.clear();
    
    // Create fresh widgets for row 0
    for (int col = 0; col < PositionModel::ColumnCount; ++col) {
        FilterRowWidget* filterWidget = new FilterRowWidget(col, this, m_tableView);
        m_filterWidgets.append(filterWidget);
        
        QModelIndex idx = m_model->index(0, col);
        if (idx.isValid()) {
            m_tableView->setIndexWidget(idx, filterWidget);
            
            // Restore filter state if this column had filters
            if (currentFilters.contains(col)) {
                filterWidget->m_selectedValues = currentFilters[col];
                filterWidget->updateButtonDisplay();
            }
            
            connect(filterWidget, &FilterRowWidget::filterChanged, this, &PositionWindow::onColumnFilterChanged);
            qDebug() << "[PositionWindow::recreateFilterWidgets] Created widget for column" << col;
        } else {
            qDebug() << "[PositionWindow::recreateFilterWidgets] WARNING: Invalid index for column" << col;
        }
    }
    
    m_tableView->setRowHeight(0, 36);
    qDebug() << "[PositionWindow::recreateFilterWidgets] Completed -" << m_filterWidgets.size() << "widgets created";
}

void PositionWindow::onFilterChanged()
{
    qDebug() << "[PositionWindow::onFilterChanged] Top filter changed";
    m_filterExchange = m_cbExchange->currentText();
    m_filterSegment = m_cbSegment->currentText();
    m_filterPeriodicity = m_cbPeriodicity->currentText();
    m_filterUser = m_cbUser->currentText();
    m_filterClient = m_cbClient->currentText();
    m_filterSecurity = m_cbSecurity->currentText();
    
    applyFilters();
}

void PositionWindow::onColumnFilterChanged(int column, const QStringList& selectedValues)
{
    qDebug() << "[PositionWindow::onColumnFilterChanged] Column" << column << "filter changed. Values:" << selectedValues;
    
    if (selectedValues.isEmpty()) {
        m_columnFilters.remove(column);
        qDebug() << "[PositionWindow::onColumnFilterChanged] Removed filter for column" << column;
    } else {
        m_columnFilters[column] = selectedValues;
        qDebug() << "[PositionWindow::onColumnFilterChanged] Set filter for column" << column << "with" << selectedValues.size() << "values";
    }
    
    qDebug() << "[PositionWindow::onColumnFilterChanged] Total active column filters:" << m_columnFilters.size();
    applyFilters();
}

void PositionWindow::toggleFilterRow()
{
    m_filterRowVisible = !m_filterRowVisible;
    m_model->setFilterRowVisible(m_filterRowVisible);
    
    if (m_filterRowVisible) {
        // Show filter widgets in row 0 (Excel-style)
        qDebug() << "[PositionWindow::toggleFilterRow] Creating filter widgets";
        m_filterWidgets.clear();
        
        for (int col = 0; col < PositionModel::ColumnCount; ++col) {
            FilterRowWidget* filterWidget = new FilterRowWidget(col, this, m_tableView);
            m_filterWidgets.append(filterWidget);
            m_tableView->setIndexWidget(m_model->index(0, col), filterWidget);
            connect(filterWidget, &FilterRowWidget::filterChanged, this, &PositionWindow::onColumnFilterChanged);
            qDebug() << "[PositionWindow::toggleFilterRow] Created filter widget for column" << col;
        }
        m_tableView->setRowHeight(0, 36);
        qDebug() << "[PositionWindow] Filter row activated (Excel-style) with" << m_filterWidgets.size() << "widgets";
    } else {
        // Clear filter widgets and filters
        qDebug() << "[PositionWindow::toggleFilterRow] Clearing filter widgets";
        for (int col = 0; col < PositionModel::ColumnCount; ++col) {
            m_tableView->setIndexWidget(m_model->index(0, col), nullptr);
        }
        qDeleteAll(m_filterWidgets);
        m_filterWidgets.clear();
        m_columnFilters.clear();
        applyFilters();
        qDebug() << "[PositionWindow] Filter row deactivated";
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

FilterRowWidget::FilterRowWidget(int column, PositionWindow* positionWindow, QWidget* parent)
    : QWidget(parent)
    , m_column(column)
    , m_filterButton(nullptr)
    , m_positionWindow(positionWindow)
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

    connect(m_filterButton, &QPushButton::clicked, this, &FilterRowWidget::showFilterPopup);
}

QStringList FilterRowWidget::getUniqueValuesForColumn() const
{
    QSet<QString> uniqueValues;
    
    if (!m_positionWindow) return QStringList();
    
    // Get positions after top filters but before column filters
    QList<Position> positions = m_positionWindow->getTopFilteredPositions();
    
    for (const Position& pos : positions) {
        QString value;
        switch (m_column) {
            case PositionModel::Symbol: value = pos.symbol; break;
            case PositionModel::SeriesExpiry: value = pos.seriesExpiry; break;
            case PositionModel::BuyQty: if (pos.buyQty > 0) value = QString::number(pos.buyQty); break;
            case PositionModel::SellQty: if (pos.sellQty > 0) value = QString::number(pos.sellQty); break;
            case PositionModel::NetPrice: if (pos.netPrice != 0.0) value = QString::number(pos.netPrice, 'f', 2); break;
            case PositionModel::MarkPrice: if (pos.markPrice != 0.0) value = QString::number(pos.markPrice, 'f', 2); break;
            case PositionModel::MTMGainLoss: value = QString::number(pos.mtmGainLoss, 'f', 2); break;
            case PositionModel::MTMMargin: value = QString::number(pos.mtmMargin, 'f', 2); break;
            case PositionModel::BuyValue: if (pos.buyValue != 0.0) value = QString::number(pos.buyValue, 'f', 2); break;
            case PositionModel::SellValue: if (pos.sellValue != 0.0) value = QString::number(pos.sellValue, 'f', 2); break;
        }
        if (!value.isEmpty()) {
            uniqueValues.insert(value);
        }
    }
    
    QStringList list = uniqueValues.values();
    list.sort();
    return list;
}

void FilterRowWidget::showFilterPopup()
{
    qDebug() << "[FilterRowWidget::showFilterPopup] Opening filter dialog for column" << m_column;
    qDebug() << "[FilterRowWidget::showFilterPopup] Current selected values:" << m_selectedValues;
    
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
        
        // Reset button style if no filter
        if (m_selectedValues.isEmpty() && m_filterButton) {
            m_filterButton->setStyleSheet(
                "QPushButton { "
                "background: #FFFFFF; color: #333333; border: 1px solid rgba(0,0,0,0.15); "
                "border-radius: 3px; padding: 4px 8px; text-align: left; font-size: 10px; "
                "}"
                "QPushButton:hover { background: #F8F8F8; border-color: #4A90E2; }"
                "QPushButton:pressed { background: #E8E8E8; }"
            );
        }
        
        emit filterChanged(m_column, m_selectedValues);
    }
    
    dialog->deleteLater();
}

void FilterRowWidget::applyFilter()
{
    emit filterChanged(m_column, m_selectedValues);
}

QStringList FilterRowWidget::selectedValues() const
{
    return m_selectedValues;
}

void FilterRowWidget::updateButtonDisplay()
{
    if (!m_filterButton) return;
    
    if (m_selectedValues.isEmpty()) {
        m_filterButton->setText("▼ Filter");
        m_filterButton->setStyleSheet(
            "QPushButton { "
            "background: #FFFFFF; color: #333333; border: 1px solid rgba(0,0,0,0.15); "
            "border-radius: 3px; padding: 4px 8px; text-align: left; font-size: 10px; "
            "}"
            "QPushButton:hover { background: #F8F8F8; border-color: #4A90E2; }"
            "QPushButton:pressed { background: #E8E8E8; }"
        );
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
}

void FilterRowWidget::clear()
{
    m_selectedValues.clear();
    updateButtonDisplay();
}
