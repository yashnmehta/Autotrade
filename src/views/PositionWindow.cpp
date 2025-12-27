#include "views/PositionWindow.h"
#include "services/TradingDataService.h"
#include "repository/RepositoryManager.h"
#include "repository/ContractData.h"
#include "api/XTSTypes.h"
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
#include <QTimer>
#include <QDebug>
#include <QSortFilterProxyModel>

// Custom proxy model to keep filter row at top and summary row at bottom
class PositionSortProxyModel : public QSortFilterProxyModel {
public:
    explicit PositionSortProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

protected:
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override {
        PositionModel* model = qobject_cast<PositionModel*>(sourceModel());
        if (!model) return QSortFilterProxyModel::lessThan(source_left, source_right);

        bool filterVisible = model->isFilterRowVisible();
        int rowCount = model->rowCount();
        
        // Always keep filter row at top
        if (filterVisible) {
            if (source_left.row() == 0) return (sortOrder() == Qt::AscendingOrder);
            if (source_right.row() == 0) return (sortOrder() == Qt::DescendingOrder);
        }

        // Always keep summary row at bottom
        // Note: Summary row is the last row in the model
        if (source_left.row() == rowCount - 1) return (sortOrder() == Qt::DescendingOrder);
        if (source_right.row() == rowCount - 1) return (sortOrder() == Qt::AscendingOrder);

        // For data rows, use UserRole for numeric sorting
        QVariant leftData = model->data(source_left, Qt::UserRole);
        QVariant rightData = model->data(source_right, Qt::UserRole);

        if (leftData.type() == QVariant::Double || leftData.type() == QVariant::Int || leftData.type() == QVariant::LongLong) {
            return leftData.toDouble() < rightData.toDouble();
        }

        return QSortFilterProxyModel::lessThan(source_left, source_right);
    }
};

// ============================================================================
// PositionWindow Implementation
// ============================================================================

PositionWindow::PositionWindow(TradingDataService* tradingDataService, QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_filterRowVisible(false)
    , m_filterShortcut(nullptr)
    , m_tradingDataService(tradingDataService)
{
    setupUI();
    
    // Load initial profile
    loadInitialProfile();
    
    // Connect to trading data service if available
    if (m_tradingDataService) {
        connect(m_tradingDataService, &TradingDataService::positionsUpdated,
                this, &PositionWindow::onPositionsUpdated);
        
        // Load initial data
        onPositionsUpdated(m_tradingDataService->getPositions());
        qDebug() << "[PositionWindow] Connected to TradingDataService and loaded initial positions";
    } else {
        qWarning() << "[PositionWindow] No TradingDataService provided - window will be empty";
    }
    
    // Setup Ctrl+F shortcut for filter toggle
    m_filterShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(m_filterShortcut, &QShortcut::activated, this, &PositionWindow::toggleFilterRow);
    
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
    
    qDebug() << "[PositionWindow] Created successfully - Press Ctrl+F to toggle filters, Esc to close";
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
    m_topFilterWidget = new QWidget(this);
    m_topFilterWidget->setObjectName("filterContainer");
    m_topFilterWidget->setStyleSheet(
        "QWidget#filterContainer { background-color: #2d2d2d; border-bottom: 1px solid #3f3f46; border-radius: 4px; }"
        "QLabel { color: #d4d4d8; font-size: 11px; font-weight: 500; }"
        "QComboBox { "
        "   background-color: #3f3f46; color: #ffffff; border: 1px solid #52525b; "
        "   border-radius: 3px; padding: 3px 5px; font-size: 11px; "
        "} "
        "QComboBox::drop-down { border: none; } "
        "QPushButton { border-radius: 3px; font-weight: 600; font-size: 11px; padding: 5px 12px; } "
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(m_topFilterWidget);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(8);

    // Filter label
    QLabel *topLabel = new QLabel("POSITION FILTERS");
    topLabel->setStyleSheet("color: #a1a1aa; font-weight: bold; letter-spacing: 0.5px; font-size: 10px;");
    mainLayout->addWidget(topLabel);

    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(15);

    auto addCombo = [&](const QString &label, QComboBox* &combo, const QStringList &items, int width) {
        QVBoxLayout *vbox = new QVBoxLayout();
        vbox->setSpacing(2);
        vbox->addWidget(new QLabel(label));
        combo = new QComboBox();
        combo->addItems(items);
        combo->setFixedWidth(width);
        vbox->addWidget(combo);
        filterLayout->addLayout(vbox);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
                this, &PositionWindow::onFilterChanged);
    };

    addCombo("Exchange", m_cbExchange, {"(ALL)", "NSE", "BSE", "MCX"}, 80);
    addCombo("Mkt Segment", m_cbSegment, {"(ALL)", "E", "F", "O"}, 80);
    addCombo("Periodicity", m_cbPeriodicity, {"Daily", "Weekly", "Monthly"}, 90);
    addCombo("User", m_cbUser, {"MEMBER", "Admin", "Trader1"}, 100);
    addCombo("Client", m_cbClient, {"(ALL)", "CLIENT001", "CLIENT002", "CLIENT003"}, 100);

    // Security (Editable)
    QVBoxLayout *vboxSec = new QVBoxLayout();
    vboxSec->setSpacing(2);
    vboxSec->addWidget(new QLabel("Security/Contract"));
    m_cbSecurity = new QComboBox(this);
    m_cbSecurity->addItem("(ALL)");
    m_cbSecurity->setFixedWidth(140);
    m_cbSecurity->setEditable(true);
    vboxSec->addWidget(m_cbSecurity);
    filterLayout->addLayout(vboxSec);
    connect(m_cbSecurity, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PositionWindow::onFilterChanged);

    filterLayout->addStretch();

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);
    btnLayout->setAlignment(Qt::AlignBottom);

    m_btnRefresh = new QPushButton("Refresh", this);
    m_btnRefresh->setStyleSheet("background-color: #16a34a; color: white;");
    connect(m_btnRefresh, &QPushButton::clicked, this, &PositionWindow::onRefreshClicked);

    m_btnExport = new QPushButton("Export", this);
    m_btnExport->setStyleSheet("background-color: #d97706; color: white;");
    connect(m_btnExport, &QPushButton::clicked, this, &PositionWindow::onExportClicked);

    btnLayout->addWidget(m_btnRefresh);
    btnLayout->addWidget(m_btnExport);

    filterLayout->addLayout(btnLayout);
    mainLayout->addLayout(filterLayout);
}

#include "views/GenericProfileDialog.h"
#include "models/GenericProfileManager.h"

void PositionWindow::showColumnProfileDialog()
{
    QList<GenericColumnInfo> allCols;
    for (int i = 0; i < PositionModel::ColumnCount; ++i) {
        GenericColumnInfo info;
        info.id = i;
        info.name = m_model->headerData(i, Qt::Horizontal).toString();
        info.defaultWidth = 90;
        info.visibleByDefault = true;
        allCols.append(info);
    }

    GenericProfileDialog dialog("PositionBook", allCols, m_columnProfile, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_columnProfile = dialog.getProfile();
        
        // Apply to view
        for (int i = 0; i < PositionModel::ColumnCount; ++i) {
            bool visible = m_columnProfile.isColumnVisible(i);
            m_tableView->setColumnHidden(i, !visible);
            if (visible) {
                m_tableView->setColumnWidth(i, m_columnProfile.columnWidth(i));
            }
        }
    }
}

void PositionWindow::loadInitialProfile()
{
    GenericProfileManager manager("profiles");
    QString defName = manager.getDefaultProfileName("PositionBook");
    
    if (defName != "Default" && manager.loadProfile("PositionBook", defName, m_columnProfile)) {
        // Success
    } else {
        // Default layout
        m_columnProfile = GenericTableProfile("Default");
        for (int i = 0; i < PositionModel::ColumnCount; ++i) {
            m_columnProfile.setColumnVisible(i, true);
            m_columnProfile.setColumnWidth(i, 90);
        }
        QList<int> order;
        for (int i = 0; i < PositionModel::ColumnCount; ++i) order.append(i);
        m_columnProfile.setColumnOrder(order);
    }
    
    // Apply to view
    for (int i = 0; i < PositionModel::ColumnCount; ++i) {
        bool visible = m_columnProfile.isColumnVisible(i);
        m_tableView->setColumnHidden(i, !visible);
        if (visible) {
            m_tableView->setColumnWidth(i, m_columnProfile.columnWidth(i));
        }
    }
}

void PositionWindow::setupTableView()
{
    m_tableView = new QTableView(this);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        menu.addAction("Export to CSV", this, &PositionWindow::onExportClicked);
        menu.addAction("Refresh", this, &PositionWindow::onRefreshClicked);
        menu.addSeparator();
        menu.addAction("Column Profile...", this, &PositionWindow::showColumnProfileDialog);
        menu.exec(m_tableView->viewport()->mapToGlobal(pos));
    });

    m_model = new PositionModel(this);
    
    // Setup proxy model for sorting
    m_proxyModel = new PositionSortProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setDynamicSortFilter(true);
    
    m_tableView->setModel(m_proxyModel);
    m_tableView->setSortingEnabled(true);
    
    // Premium Dark Theme styling
    m_tableView->setStyleSheet(
        "QTableView {"
        "   background-color: #ffffff;"
        "   alternate-background-color: #f9f9f9;"
        "   selection-background-color: #e3f2fd;"
        "   selection-color: #000000;"
        "   gridline-color: #e0e0e0;"
        "   border: none;"
        "   color: #333333;"
        "   font-size: 11px;"
        "}"
        "QTableView::item {"
        "   padding: 4px;"
        "   border: none;"
        "}"
        "QHeaderView::section {"
        "   background-color: #f5f5f5;"
        "   color: #333333;"
        "   padding: 6px;"
        "   border: none;"
        "   border-right: 1px solid #e0e0e0;"
        "   border-bottom: 1px solid #e0e0e0;"
        "   font-weight: bold;"
        "   font-size: 11px;"
        "}"
        "QHeaderView::section:hover {"
        "   background-color: #eeeeee;"
        "}"
    );

    // Configure table behavior
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setShowGrid(true);
    m_tableView->setWordWrap(false);
    
    // Header behavior
    m_tableView->horizontalHeader()->setMinimumHeight(32);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionsClickable(true);
    m_tableView->horizontalHeader()->setHighlightSections(false);
    
    // Default column widths
    for (int i = 0; i < PositionModel::ColumnCount; ++i) {
        m_tableView->setColumnWidth(i, 90);
    }
    m_tableView->setColumnWidth(PositionModel::Symbol, 120);
    m_tableView->setColumnWidth(PositionModel::Name, 150);
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
        if (m_filterSegment != "(ALL)" && !m_filterSegment.isEmpty() && pos.productType != m_filterSegment)
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
                case PositionModel::ScripCode: cellValue = QString::number(pos.scripCode); break;
                case PositionModel::Symbol: cellValue = pos.symbol; break;
                case PositionModel::SeriesExpiry: cellValue = pos.seriesExpiry; break;
                case PositionModel::StrikePrice: cellValue = pos.strikePrice > 0 ? QString::number(pos.strikePrice, 'f', 2) : ""; break;
                case PositionModel::OptionType: cellValue = pos.optionType; break;
                case PositionModel::NetQty: cellValue = QString::number(pos.netQty); break;
                case PositionModel::MarketPrice: cellValue = QString::number(pos.marketPrice, 'f', 2); break;
                case PositionModel::MTMGL: cellValue = QString::number(pos.mtm, 'f', 2); break;
                case PositionModel::NetPrice: cellValue = QString::number(pos.netPrice, 'f', 2); break;
                case PositionModel::MTMVPos: cellValue = QString::number(pos.mtmvPos, 'f', 2); break;
                case PositionModel::TotalValue: cellValue = QString::number(pos.totalValue, 'f', 2); break;
                case PositionModel::BuyVal: cellValue = QString::number(pos.buyVal, 'f', 2); break;
                case PositionModel::SellVal: cellValue = QString::number(pos.sellVal, 'f', 2); break;
                case PositionModel::Exchange: cellValue = pos.exchange; break;
                case PositionModel::User: cellValue = pos.user; break;
                case PositionModel::Client: cellValue = pos.client; break;
                case PositionModel::Name: cellValue = pos.name; break;
                case PositionModel::InstrumentType: cellValue = pos.instrumentType; break;
                case PositionModel::InstrumentName: cellValue = pos.instrumentName; break;
                case PositionModel::ScripName: cellValue = pos.scripName; break;
                case PositionModel::BuyQty: cellValue = QString::number(pos.buyQty); break;
                case PositionModel::BuyLot: cellValue = QString::number(pos.buyLot, 'f', 2); break;
                case PositionModel::BuyWeight: cellValue = QString::number(pos.buyWeight, 'f', 2); break;
                case PositionModel::BuyAvg: cellValue = QString::number(pos.buyAvg, 'f', 2); break;
                case PositionModel::SellQty: cellValue = QString::number(pos.sellQty); break;
                case PositionModel::SellLot: cellValue = QString::number(pos.sellLot, 'f', 2); break;
                case PositionModel::SellWeight: cellValue = QString::number(pos.sellWeight, 'f', 2); break;
                case PositionModel::SellAvg: cellValue = QString::number(pos.sellAvg, 'f', 2); break;
                case PositionModel::NetLot: cellValue = QString::number(pos.netLot, 'f', 2); break;
                case PositionModel::NetWeight: cellValue = QString::number(pos.netWeight, 'f', 2); break;
                case PositionModel::NetVal: cellValue = QString::number(pos.netVal, 'f', 2); break;
                case PositionModel::ProductType: cellValue = pos.productType; break;
                case PositionModel::ClientGroup: cellValue = pos.clientGroup; break;
                case PositionModel::DPRRange: cellValue = QString::number(pos.dprRange, 'f', 2); break;
                case PositionModel::MaturityDate: cellValue = pos.maturityDate; break;
                case PositionModel::Yield: cellValue = QString::number(pos.yield, 'f', 2); break;
                case PositionModel::TotalQuantity: cellValue = QString::number(pos.totalQuantity); break;
                case PositionModel::TotalLot: cellValue = QString::number(pos.totalLot, 'f', 2); break;
                case PositionModel::TotalWeight: cellValue = QString::number(pos.totalWeight, 'f', 2); break;
                case PositionModel::Brokerage: cellValue = QString::number(pos.brokerage, 'f', 2); break;
                case PositionModel::NetMTM: cellValue = QString::number(pos.netMtm, 'f', 2); break;
                case PositionModel::NetValuePostExp: cellValue = QString::number(pos.netValPostExp, 'f', 2); break;
                case PositionModel::OptionFlag: cellValue = pos.optionFlag; break;
                case PositionModel::VarPercent: cellValue = QString::number(pos.varPercent, 'f', 2); break;
                case PositionModel::VarAmount: cellValue = QString::number(pos.varAmount, 'f', 2); break;
                case PositionModel::SMCategory: cellValue = pos.smCategory; break;
                case PositionModel::CfAvgPrice: cellValue = QString::number(pos.cfAvgPrice, 'f', 2); break;
                case PositionModel::ActualMTM: cellValue = QString::number(pos.actualMtm, 'f', 2); break;
                case PositionModel::UnsettledQty: cellValue = QString::number(pos.unsettledQty); break;
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
    summary.name = "(ALL)";
    
    QList<Position> positions = m_model->positions();
    for (const Position& pos : positions) {
        summary.buyQty += pos.buyQty;
        summary.sellQty += pos.sellQty;
        summary.netQty += pos.netQty;
        summary.mtm += pos.mtm;
        summary.actualMtm += pos.actualMtm;
        summary.buyVal += pos.buyVal;
        summary.sellVal += pos.sellVal;
        summary.totalValue += pos.totalValue;
        summary.netVal += pos.netVal;
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
        
        QModelIndex idx = m_proxyModel->index(0, col);
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

void PositionWindow::onPositionsUpdated(const QVector<XTS::Position>& positions)
{
    qDebug() << "[PositionWindow::onPositionsUpdated] Received" << positions.size() << "positions from TradingDataService";
    
    m_allPositions.clear();
    RepositoryManager* repo = RepositoryManager::getInstance();
    
    // Convert XTS::Position to local Position struct
    for (const XTS::Position& xtsPos : positions) {
        Position pos;
        const ContractData* contract = repo->getContractByToken(xtsPos.exchangeSegment, xtsPos.exchangeInstrumentID);
        
        // Basic & Contract Info
        pos.scripCode = xtsPos.exchangeInstrumentID;
        pos.symbol = contract ? contract->name : xtsPos.tradingSymbol;
        pos.seriesExpiry = contract ? contract->expiryDate : "-";
        pos.strikePrice = contract ? contract->strikePrice : 0.0;
        pos.optionType = contract ? contract->optionType : "-";
        
        QString exchangeCode = "NSE";
        if (xtsPos.exchangeSegment.startsWith("NSE")) exchangeCode = "NSE";
        else if (xtsPos.exchangeSegment.startsWith("BSE")) exchangeCode = "BSE";
        pos.exchange = exchangeCode;
        
        pos.name = contract ? contract->displayName : xtsPos.tradingSymbol;
        pos.instrumentType = contract ? contract->series : xtsPos.productType;
        pos.instrumentName = contract ? contract->series : "";
        pos.scripName = contract ? contract->displayName : xtsPos.tradingSymbol;
        pos.productType = xtsPos.productType;
        pos.optionFlag = (contract && (contract->optionType == "CE" || contract->optionType == "PE")) ? "OPT" : "FUT";

        // Quantities & Positions
        pos.netQty = xtsPos.quantity;
        pos.buyQty = xtsPos.openBuyQuantity;
        pos.sellQty = xtsPos.openSellQuantity;
        pos.totalQuantity = xtsPos.quantity; // Or sum? usually net
        pos.unsettledQty = 0; // Placeholder

        // Prices
        pos.marketPrice = xtsPos.sellAveragePrice; // XTS uses this for MTM / LTP in some views
        pos.netPrice = xtsPos.buyAveragePrice;
        pos.buyAvg = xtsPos.buyAveragePrice;
        pos.sellAvg = xtsPos.sellAveragePrice;
        pos.cfAvgPrice = xtsPos.buyAveragePrice; // Placeholder

        // Values & Profits
        pos.mtm = xtsPos.mtm;
        pos.mtmvPos = xtsPos.mtm; // Placeholder
        pos.totalValue = xtsPos.quantity * (pos.marketPrice > 0 ? pos.marketPrice : pos.netPrice);
        pos.buyVal = xtsPos.buyAmount;
        pos.sellVal = xtsPos.sellAmount;
        pos.netVal = xtsPos.buyAmount - xtsPos.sellAmount;
        pos.brokerage = 0.0; 
        pos.netMtm = xtsPos.mtm;
        pos.netValPostExp = xtsPos.mtm;
        pos.actualMtm = xtsPos.mtm;
        
        // Metadata & Risk
        pos.user = "MEMBER";
        pos.client = xtsPos.accountID;
        pos.clientGroup = "DEFAULT";
        pos.maturityDate = contract ? contract->expiryDate : "-";
        pos.yield = 0.0;
        pos.varPercent = 0.0;
        pos.varAmount = 0.0;
        pos.smCategory = "-";
        
        m_allPositions.append(pos);
    }
    
    qDebug() << "[PositionWindow::onPositionsUpdated] Converted to" << m_allPositions.size() << "local positions";
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
            case ScripCode: return QString::number(pos.scripCode);
            case Symbol: return pos.symbol;
            case SeriesExpiry: return pos.seriesExpiry;
            case StrikePrice: return pos.strikePrice > 0 ? QString::number(pos.strikePrice, 'f', 2) : QString();
            case OptionType: return pos.optionType;
            case NetQty: return QString::number(pos.netQty);
            case MarketPrice: return pos.marketPrice != 0.0 ? QString::number(pos.marketPrice, 'f', 2) : QString();
            case MTMGL: return QString::number(pos.mtm, 'f', 2);
            case NetPrice: return pos.netPrice != 0.0 ? QString::number(pos.netPrice, 'f', 2) : QString();
            case MTMVPos: return QString::number(pos.mtmvPos, 'f', 2);
            case TotalValue: return QString::number(pos.totalValue, 'f', 2);
            case BuyVal: return QString::number(pos.buyVal, 'f', 2);
            case SellVal: return QString::number(pos.sellVal, 'f', 2);
            case Exchange: return pos.exchange;
            case User: return pos.user;
            case Client: return pos.client;
            case Name: return pos.name;
            case InstrumentType: return pos.instrumentType;
            case InstrumentName: return pos.instrumentName;
            case ScripName: return pos.scripName;
            case BuyQty: return QString::number(pos.buyQty);
            case BuyLot: return QString::number(pos.buyLot, 'f', 2);
            case BuyWeight: return QString::number(pos.buyWeight, 'f', 2);
            case BuyAvg: return QString::number(pos.buyAvg, 'f', 2);
            case SellQty: return QString::number(pos.sellQty);
            case SellLot: return QString::number(pos.sellLot, 'f', 2);
            case SellWeight: return QString::number(pos.sellWeight, 'f', 2);
            case SellAvg: return QString::number(pos.sellAvg, 'f', 2);
            case NetLot: return QString::number(pos.netLot, 'f', 2);
            case NetWeight: return QString::number(pos.netWeight, 'f', 2);
            case NetVal: return QString::number(pos.netVal, 'f', 2);
            case ProductType: return pos.productType;
            case ClientGroup: return pos.clientGroup;
            case DPRRange: return QString::number(pos.dprRange, 'f', 2);
            case MaturityDate: return pos.maturityDate;
            case Yield: return QString::number(pos.yield, 'f', 2);
            case TotalQuantity: return QString::number(pos.totalQuantity);
            case TotalLot: return QString::number(pos.totalLot, 'f', 2);
            case TotalWeight: return QString::number(pos.totalWeight, 'f', 2);
            case Brokerage: return QString::number(pos.brokerage, 'f', 2);
            case NetMTM: return QString::number(pos.netMtm, 'f', 2);
            case NetValuePostExp: return QString::number(pos.netValPostExp, 'f', 2);
            case OptionFlag: return pos.optionFlag;
            case VarPercent: return QString::number(pos.varPercent, 'f', 2);
            case VarAmount: return QString::number(pos.varAmount, 'f', 2);
            case SMCategory: return pos.smCategory;
            case CfAvgPrice: return QString::number(pos.cfAvgPrice, 'f', 2);
            case ActualMTM: return QString::number(pos.actualMtm, 'f', 2);
            case UnsettledQty: return QString::number(pos.unsettledQty);
        }
    }
    else if (role == Qt::EditRole || role == Qt::UserRole) {
        switch (col) {
            case ScripCode: return (qlonglong)pos.scripCode;
            case Symbol: return pos.symbol;
            case SeriesExpiry: return pos.seriesExpiry;
            case StrikePrice: return pos.strikePrice;
            case OptionType: return pos.optionType;
            case NetQty: return pos.netQty;
            case MarketPrice: return pos.marketPrice;
            case MTMGL: return pos.mtm;
            case NetPrice: return pos.netPrice;
            case MTMVPos: return pos.mtmvPos;
            case TotalValue: return pos.totalValue;
            case BuyVal: return pos.buyVal;
            case SellVal: return pos.sellVal;
            case Exchange: return pos.exchange;
            case User: return pos.user;
            case Client: return pos.client;
            case Name: return pos.name;
            case InstrumentType: return pos.instrumentType;
            case InstrumentName: return pos.instrumentName;
            case ScripName: return pos.scripName;
            case BuyQty: return pos.buyQty;
            case BuyLot: return pos.buyLot;
            case BuyWeight: return pos.buyWeight;
            case BuyAvg: return pos.buyAvg;
            case SellQty: return pos.sellQty;
            case SellLot: return pos.sellLot;
            case SellWeight: return pos.sellWeight;
            case SellAvg: return pos.sellAvg;
            case NetLot: return pos.netLot;
            case NetWeight: return pos.netWeight;
            case NetVal: return pos.netVal;
            case ProductType: return pos.productType;
            case ClientGroup: return pos.clientGroup;
            case DPRRange: return pos.dprRange;
            case MaturityDate: return pos.maturityDate;
            case Yield: return pos.yield;
            case TotalQuantity: return pos.totalQuantity;
            case TotalLot: return pos.totalLot;
            case TotalWeight: return pos.totalWeight;
            case Brokerage: return pos.brokerage;
            case NetMTM: return pos.netMtm;
            case NetValuePostExp: return pos.netValPostExp;
            case OptionFlag: return pos.optionFlag;
            case VarPercent: return pos.varPercent;
            case VarAmount: return pos.varAmount;
            case SMCategory: return pos.smCategory;
            case CfAvgPrice: return pos.cfAvgPrice;
            case ActualMTM: return pos.actualMtm;
            case UnsettledQty: return pos.unsettledQty;
        }
    }
    else if (role == Qt::TextAlignmentRole) {
        switch (col) {
            case ScripCode:
            case Symbol:
            case SeriesExpiry:
            case OptionType:
            case Exchange:
            case User:
            case Client:
            case Name:
            case InstrumentType:
            case InstrumentName:
            case ScripName:
            case ProductType:
            case ClientGroup:
            case MaturityDate:
            case OptionFlag:
            case SMCategory:
                return Qt::AlignLeft + Qt::AlignVCenter;
            default:
                return Qt::AlignRight + Qt::AlignVCenter;
        }
    }
    else if (role == Qt::ForegroundRole) {
        if (col == MTMGL || col == NetMTM || col == ActualMTM) {
            double val = (col == MTMGL) ? pos.mtm : (col == NetMTM ? pos.netMtm : pos.actualMtm);
            return val > 0 ? QColor(0, 150, 0) : 
                   val < 0 ? QColor(200, 0, 0) : QColor(0, 0, 0);
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
            case ScripCode: return "Scrip Code";
            case Symbol: return "Symbol";
            case SeriesExpiry: return "Ser/Exp";
            case StrikePrice: return "Strike Price";
            case OptionType: return "Option Type";
            case NetQty: return "Net Qty";
            case MarketPrice: return "Market Price";
            case MTMGL: return "MTM G/L";
            case NetPrice: return "Net Price";
            case MTMVPos: return "MTMV Pos";
            case TotalValue: return "Total Value";
            case BuyVal: return "Buy Val";
            case SellVal: return "Sell Val";
            case Exchange: return "Exchange";
            case User: return "User";
            case Client: return "Client";
            case Name: return "Name";
            case InstrumentType: return "Instrument Type";
            case InstrumentName: return "Instrument Name";
            case ScripName: return "Scrip Name";
            case BuyQty: return "Buy Qty";
            case BuyLot: return "Buy Lot";
            case BuyWeight: return "Buy Weight";
            case BuyAvg: return "Buy Avg.";
            case SellQty: return "Sell Qty";
            case SellLot: return "Sell Lot";
            case SellWeight: return "Sell Weight";
            case SellAvg: return "Sell Avg.";
            case NetLot: return "Net Lot";
            case NetWeight: return "Net Weight";
            case NetVal: return "Net Val";
            case ProductType: return "Product Type";
            case ClientGroup: return "Client Group";
            case DPRRange: return "DPR Range";
            case MaturityDate: return "Maturity Date";
            case Yield: return "Yield";
            case TotalQuantity: return "Total Quantity";
            case TotalLot: return "Total Lot";
            case TotalWeight: return "Total Weight";
            case Brokerage: return "Brokerage";
            case NetMTM: return "Net MTM";
            case NetValuePostExp: return "Net Value Post Expenses";
            case OptionFlag: return "Option Flag";
            case VarPercent: return "VAR %";
            case VarAmount: return "VAR Amount";
            case SMCategory: return "SM Category";
            case CfAvgPrice: return "CF Avg Price";
            case ActualMTM: return "Actual MTM P&L";
            case UnsettledQty: return "Unsettled Quantity";
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
            case PositionModel::ScripCode: value = QString::number(pos.scripCode); break;
            case PositionModel::Symbol: value = pos.symbol; break;
            case PositionModel::SeriesExpiry: value = pos.seriesExpiry; break;
            case PositionModel::StrikePrice: value = pos.strikePrice > 0 ? QString::number(pos.strikePrice, 'f', 2) : ""; break;
            case PositionModel::OptionType: value = pos.optionType; break;
            case PositionModel::NetQty: value = QString::number(pos.netQty); break;
            case PositionModel::MarketPrice: value = QString::number(pos.marketPrice, 'f', 2); break;
            case PositionModel::MTMGL: value = QString::number(pos.mtm, 'f', 2); break;
            case PositionModel::NetPrice: value = QString::number(pos.netPrice, 'f', 2); break;
            case PositionModel::MTMVPos: value = QString::number(pos.mtmvPos, 'f', 2); break;
            case PositionModel::TotalValue: value = QString::number(pos.totalValue, 'f', 2); break;
            case PositionModel::BuyVal: value = QString::number(pos.buyVal, 'f', 2); break;
            case PositionModel::SellVal: value = QString::number(pos.sellVal, 'f', 2); break;
            case PositionModel::Exchange: value = pos.exchange; break;
            case PositionModel::User: value = pos.user; break;
            case PositionModel::Client: value = pos.client; break;
            case PositionModel::Name: value = pos.name; break;
            case PositionModel::InstrumentType: value = pos.instrumentType; break;
            case PositionModel::InstrumentName: value = pos.instrumentName; break;
            case PositionModel::ScripName: value = pos.scripName; break;
            case PositionModel::BuyQty: value = QString::number(pos.buyQty); break;
            case PositionModel::BuyLot: value = QString::number(pos.buyLot, 'f', 2); break;
            case PositionModel::BuyWeight: value = QString::number(pos.buyWeight, 'f', 2); break;
            case PositionModel::BuyAvg: value = QString::number(pos.buyAvg, 'f', 2); break;
            case PositionModel::SellQty: value = QString::number(pos.sellQty); break;
            case PositionModel::SellLot: value = QString::number(pos.sellLot, 'f', 2); break;
            case PositionModel::SellWeight: value = QString::number(pos.sellWeight, 'f', 2); break;
            case PositionModel::SellAvg: value = QString::number(pos.sellAvg, 'f', 2); break;
            case PositionModel::NetLot: value = QString::number(pos.netLot, 'f', 2); break;
            case PositionModel::NetWeight: value = QString::number(pos.netWeight, 'f', 2); break;
            case PositionModel::NetVal: value = QString::number(pos.netVal, 'f', 2); break;
            case PositionModel::ProductType: value = pos.productType; break;
            case PositionModel::ClientGroup: value = pos.clientGroup; break;
            case PositionModel::DPRRange: value = QString::number(pos.dprRange, 'f', 2); break;
            case PositionModel::MaturityDate: value = pos.maturityDate; break;
            case PositionModel::Yield: value = QString::number(pos.yield, 'f', 2); break;
            case PositionModel::TotalQuantity: value = QString::number(pos.totalQuantity); break;
            case PositionModel::TotalLot: value = QString::number(pos.totalLot, 'f', 2); break;
            case PositionModel::TotalWeight: value = QString::number(pos.totalWeight, 'f', 2); break;
            case PositionModel::Brokerage: value = QString::number(pos.brokerage, 'f', 2); break;
            case PositionModel::NetMTM: value = QString::number(pos.netMtm, 'f', 2); break;
            case PositionModel::NetValuePostExp: value = QString::number(pos.netValPostExp, 'f', 2); break;
            case PositionModel::OptionFlag: value = pos.optionFlag; break;
            case PositionModel::VarPercent: value = QString::number(pos.varPercent, 'f', 2); break;
            case PositionModel::VarAmount: value = QString::number(pos.varAmount, 'f', 2); break;
            case PositionModel::SMCategory: value = pos.smCategory; break;
            case PositionModel::CfAvgPrice: value = QString::number(pos.cfAvgPrice, 'f', 2); break;
            case PositionModel::ActualMTM: value = QString::number(pos.actualMtm, 'f', 2); break;
            case PositionModel::UnsettledQty: value = QString::number(pos.unsettledQty); break;
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
