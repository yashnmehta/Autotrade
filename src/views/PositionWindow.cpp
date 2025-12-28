#include "views/PositionWindow.h"
#include "services/TradingDataService.h"
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QMenu>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QDebug>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include "views/GenericProfileDialog.h"
#include "models/GenericProfileManager.h"
#include "utils/TableProfileHelper.h"

PositionWindow::PositionWindow(TradingDataService* tradingDataService, QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_filterRowVisible(false)
    , m_tradingDataService(tradingDataService)
{
    setupUI();
    loadInitialProfile();
    
    if (m_tradingDataService) {
        connect(m_tradingDataService, &TradingDataService::positionsUpdated, this, &PositionWindow::onPositionsUpdated);
        onPositionsUpdated(m_tradingDataService->getPositions());
    }
    
    m_filterShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(m_filterShortcut, &QShortcut::activated, this, &PositionWindow::toggleFilterRow);
}

PositionWindow::~PositionWindow() {}

void PositionWindow::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0); mainLayout->setSpacing(0);
    
    m_topFilterWidget = createFilterWidget();
    mainLayout->addWidget(m_topFilterWidget);
    
    setupTableView();
    mainLayout->addWidget(m_tableView, 1);
}

QWidget* PositionWindow::createFilterWidget() {
    QWidget* container = new QWidget(this);
    container->setObjectName("filterContainer");
    container->setStyleSheet("QWidget#filterContainer { background-color: #2d2d2d; border-bottom: 1px solid #3f3f46; } QLabel { color: #d4d4d8; font-size: 11px; } QComboBox { background-color: #3f3f46; color: #ffffff; border: 1px solid #52525b; border-radius: 3px; font-size: 11px; } QPushButton { border-radius: 3px; font-weight: 600; font-size: 11px; padding: 5px 12px; }");
    
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(12, 10, 12, 10);
    
    auto addCombo = [&](const QString& l, QComboBox*& c, const QStringList& i) {
        QVBoxLayout* v = new QVBoxLayout(); v->addWidget(new QLabel(l));
        c = new QComboBox(); c->addItems(i); v->addWidget(c); layout->addLayout(v);
        connect(c, &QComboBox::currentTextChanged, this, &PositionWindow::onFilterChanged);
    };
    
    addCombo("Exchange", m_cbExchange, {"All", "NSE", "BSE", "MCX"});
    addCombo("Segment", m_cbSegment, {"All", "Cash", "FO", "CD", "COM"});
    addCombo("Product", m_cbPeriodicity, {"All", "MIS", "NRML", "CNC"});
    addCombo("User", m_cbUser, {"All"});
    addCombo("Client", m_cbClient, {"All"});
    
    layout->addStretch();
    m_btnRefresh = new QPushButton("Refresh"); m_btnRefresh->setStyleSheet("background-color: #16a34a; color: white;");
    m_btnExport = new QPushButton("Export"); m_btnExport->setStyleSheet("background-color: #d97706; color: white;");
    layout->addWidget(m_btnRefresh); layout->addWidget(m_btnExport);
    
    connect(m_btnRefresh, &QPushButton::clicked, this, &PositionWindow::onRefreshClicked);
    connect(m_btnExport, &QPushButton::clicked, this, &PositionWindow::onExportClicked);
    
    return container;
}

void PositionWindow::setupTableView() {
    m_tableView = new CustomNetPosition(this);
    m_tableView->setSortingEnabled(true);
    
    // Connect CustomNetPosition signals
    connect(m_tableView, &CustomNetPosition::exportRequested, this, &PositionWindow::onExportClicked);
    connect(m_tableView, &CustomNetPosition::closePositionRequested, this, &PositionWindow::onSquareOffClicked);

    m_model = new PositionModel(this);
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setDynamicSortFilter(true);
    m_tableView->setModel(m_proxyModel);
    
    // Note: Context menu is now handled by CustomNetPosition
}

void PositionWindow::onSquareOffClicked() {
    QModelIndex idx = m_tableView->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::warning(this, "Square Off", "Please select a position to square off.");
        return;
    }
    
    // Get the source index from the proxy model
    QModelIndex sourceIdx = m_proxyModel->mapToSource(idx);
    int row = sourceIdx.row();
    
    // Get position details from model
    // Assuming PositionModel has a way to get the full PositionData or we extract needed fields
    // For now, let's extract key fields using data() with appropriate columns
    
    QString symbol = m_model->data(m_model->index(row, PositionModel::Symbol), Qt::DisplayRole).toString();
    int netQty = m_model->data(m_model->index(row, PositionModel::NetQty), Qt::EditRole).toInt();
    
    if (netQty == 0) {
        QMessageBox::information(this, "Square Off", "Position is already closed (Net Qty is 0).");
        return;
    }
    
    QString side = (netQty > 0) ? "SELL" : "BUY";
    int qty = std::abs(netQty);
    
    QString msg = QString("Square Off Position?\n\nSymbol: %1\nSide: %2\nQuantity: %3")
                  .arg(symbol).arg(side).arg(qty);
                  
    if (QMessageBox::question(this, "Confirm Square Off", msg) == QMessageBox::Yes) {
        qDebug() << "[PositionWindow] Square Off Requested -> Symbol:" << symbol << "Side:" << side << "Qty:" << qty;
        // TODO: Call TradingDataService or OrderManager to place the order
        // e.g., m_tradingDataService->placeOrder(...)
    }
}

void PositionWindow::onPositionsUpdated(const QVector<XTS::Position>& positions) {
    m_allPositions.clear();
    for (const auto& p : positions) {
        PositionData pd;
        pd.scripCode = p.exchangeInstrumentID;
        pd.symbol = p.tradingSymbol;
        pd.exchange = p.exchangeSegment;
        pd.netQty = p.quantity;
        pd.buyAvg = p.buyAveragePrice;
        pd.sellAvg = p.sellAveragePrice;
        pd.mtm = p.mtm;
        pd.productType = p.productType;
        pd.client = p.accountID;
        m_allPositions.append(pd);
    }
    applyFilters();
}

void PositionWindow::applyFilters() {
    QList<PositionData> filtered;
    for (const auto& p : m_allPositions) {
        bool v = true;
        if (m_cbExchange->currentText() != "All" && p.exchange != m_cbExchange->currentText()) v = false;
        if (v && m_cbSegment->currentText() != "All" && p.instrumentType != m_cbSegment->currentText()) v = false;
        if (v && m_cbPeriodicity->currentText() != "All" && p.productType != m_cbPeriodicity->currentText()) v = false;
        
        if (v && !m_columnFilters.isEmpty()) {
            for (auto it = m_columnFilters.constBegin(); it != m_columnFilters.constEnd(); ++it) {
                int col = it.key(); const QStringList& allowed = it.value();
                if (!allowed.isEmpty()) {
                    QString val;
                    if (col == PositionModel::Symbol) val = p.symbol;
                    else if (col == PositionModel::Exchange) val = p.exchange;
                    else if (col == PositionModel::ProductType) val = p.productType;
                    else if (col == PositionModel::Client) val = p.client;
                    if (!val.isEmpty() && !allowed.contains(val)) { v = false; break; }
                }
            }
        }
        if (v) filtered.append(p);
    }
    m_model->setPositions(filtered);
    updateSummaryRow();
}

void PositionWindow::updateSummaryRow() {
    PositionData summary;
    for (const auto& p : m_model->positions()) {
        summary.mtm += p.mtm;
        summary.buyQty += p.buyQty;
        summary.sellQty += p.sellQty;
    }
    m_model->setSummary(summary);
}

void PositionWindow::toggleFilterRow() {
    m_filterRowVisible = !m_filterRowVisible; m_model->setFilterRowVisible(m_filterRowVisible);
    if (m_filterRowVisible) {
        m_filterWidgets.clear();
        for (int i=0; i<PositionModel::ColumnCount; ++i) {
            FilterRowWidget* fw = new FilterRowWidget(i, this, m_tableView);
            m_filterWidgets.append(fw); m_tableView->setIndexWidget(m_model->index(0, i), fw);
            connect(fw, &FilterRowWidget::filterChanged, this, &PositionWindow::onColumnFilterChanged);
        }
    } else {
        m_columnFilters.clear(); for (int i=0; i<PositionModel::ColumnCount; ++i) m_tableView->setIndexWidget(m_model->index(0, i), nullptr);
        applyFilters();
    }
}

void PositionWindow::onColumnFilterChanged(int c, const QStringList& s) {
    if (s.isEmpty()) m_columnFilters.remove(c); else m_columnFilters[c] = s;
    applyFilters();
}

void PositionWindow::showColumnProfileDialog() {
    TableProfileHelper::showProfileDialog("PositionBook", m_tableView, m_model, m_columnProfile, this);
}

void PositionWindow::loadInitialProfile() {
    TableProfileHelper::loadProfile("PositionBook", m_tableView, m_model, m_columnProfile);
}

void PositionWindow::onFilterChanged() { applyFilters(); }
void PositionWindow::onRefreshClicked() { if (m_tradingDataService) onPositionsUpdated(m_tradingDataService->getPositions()); }
void PositionWindow::onExportClicked() {
    QString fn = QFileDialog::getSaveFileName(this, "Export", "", "CSV (*.csv)"); if (fn.isEmpty()) return;
    QFile f(fn); if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        for (int i=0; i<m_model->columnCount(); ++i) { out << m_model->headerData(i, Qt::Horizontal).toString(); if (i < m_model->columnCount()-1) out << ","; }
        out << "\n";
        for (int r=0; r<m_model->rowCount(); ++r) {
            if (m_filterRowVisible && r == 0) continue;
            for (int c=0; c<m_model->columnCount(); ++c) { out << m_model->data(m_model->index(r, c), Qt::DisplayRole).toString(); if (c < m_model->columnCount()-1) out << ","; }
            out << "\n";
        }
    }
}

void PositionWindow::addPosition(const PositionData& p) { m_allPositions.append(p); applyFilters(); }
void PositionWindow::updatePosition(const QString& s, const PositionData& p) {
    for (int i=0; i<m_allPositions.size(); ++i) {
        if (m_allPositions[i].symbol == s) { m_allPositions[i] = p; break; }
    }
    applyFilters();
}
void PositionWindow::clearPositions() { m_allPositions.clear(); applyFilters(); }

FilterRowWidget::FilterRowWidget(int column, PositionWindow* window, QWidget* parent)
    : QWidget(parent), m_column(column), m_positionWindow(window) {
    QHBoxLayout* l = new QHBoxLayout(this); l->setContentsMargins(2,2,2,2);
    m_filterButton = new QPushButton("▼ Filter");
    m_filterButton->setStyleSheet("background: #fff; color: #333; border: 1px solid #ccc; font-size: 10px;");
    l->addWidget(m_filterButton); connect(m_filterButton, &QPushButton::clicked, this, &FilterRowWidget::showFilterPopup);
}

void FilterRowWidget::showFilterPopup() {
    QDialog d(this); d.setWindowTitle("Filter"); QVBoxLayout* l = new QVBoxLayout(&d);
    QListWidget* lw = new QListWidget(&d);
    QSet<QString> vals; for (int i=(m_positionWindow->m_model->isFilterRowVisible()?1:0); i<m_positionWindow->m_model->rowCount(); ++i) {
        QString v = m_positionWindow->m_model->data(m_positionWindow->m_model->index(i, m_column), Qt::DisplayRole).toString();
        if (!v.isEmpty()) vals.insert(v);
    }
    QStringList sorted = vals.values(); sorted.sort();
    for (const QString& v : sorted) {
        QListWidgetItem* item = new QListWidgetItem(v, lw); item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_selectedValues.contains(v) ? Qt::Checked : Qt::Unchecked);
    }
    l->addWidget(lw);
    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d);
    l->addWidget(bb); connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept); connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    if (d.exec() == QDialog::Accepted) {
        m_selectedValues.clear(); for (int i=0; i<lw->count(); ++i) if (lw->item(i)->checkState() == Qt::Checked) m_selectedValues.append(lw->item(i)->text());
        updateButtonDisplay(); emit filterChanged(m_column, m_selectedValues);
    }
}

QStringList FilterRowWidget::getUniqueValuesForColumn() const {
    QSet<QString> vals; if (!m_positionWindow || !m_positionWindow->m_model) return QStringList();
    for (int i=(m_positionWindow->m_model->isFilterRowVisible()?1:0); i<m_positionWindow->m_model->rowCount(); ++i) {
        QString v = m_positionWindow->m_model->data(m_positionWindow->m_model->index(i, m_column), Qt::DisplayRole).toString();
        if (!v.isEmpty()) vals.insert(v);
    }
    QStringList sorted = vals.values(); sorted.sort(); return sorted;
}

void FilterRowWidget::updateButtonDisplay() {
    if (m_selectedValues.isEmpty()) m_filterButton->setText("▼ Filter");
    else m_filterButton->setText(QString("▼ (%1)").arg(m_selectedValues.count()));
}

void FilterRowWidget::clear() { m_selectedValues.clear(); updateButtonDisplay(); }
