#include "views/PositionWindow.h"
#include "core/widgets/CustomNetPosition.h"
#include "services/TradingDataService.h"
#include "repository/RepositoryManager.h"
#include <QVBoxLayout>

#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QDebug>
#include <QSortFilterProxyModel>
#include <QVector>

PositionWindow::PositionWindow(TradingDataService* tradingDataService, QWidget *parent)
    : BaseBookWindow("PositionBook", parent), m_tradingDataService(tradingDataService) 
{
    setupUI();
    loadInitialProfile();
    
    if (m_tradingDataService) {
        onPositionsUpdated(m_tradingDataService->getPositions());
    }
    if (m_filterShortcut) {
        connect(m_filterShortcut, &QShortcut::activated, this, [this]() {
            this->toggleFilterRow();
        });
    }
}

PositionWindow::~PositionWindow() {}

void PositionWindow::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0); mainLayout->setSpacing(0);
    mainLayout->addWidget(createFilterWidget());
    setupTable();
    mainLayout->addWidget(m_tableView, 1);
}

QWidget* PositionWindow::createFilterWidget() {
    QWidget* container = new QWidget(this);
    container->setObjectName("filterContainer");
    container->setStyleSheet("QWidget#filterContainer { background-color: #2d2d2d; border-bottom: 1px solid #3f3f46; } QLabel { color: #d4d4d8; font-size: 11px; } QComboBox { background-color: #3f3f46; color: #ffffff; border: 1px solid #52525b; border-radius: 3px; font-size: 11px; } QPushButton { border-radius: 3px; font-weight: 600; font-size: 11px; padding: 5px 12px; }");
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(12, 10, 12, 10);
    
    auto addCombo = [&](const QString& l, QComboBox*& c, const QStringList& i) {
        QVBoxLayout* v = new QVBoxLayout(); v->addWidget(new QLabel(l)); c = new QComboBox(); c->addItems(i); v->addWidget(c); layout->addLayout(v);
        connect(c, &QComboBox::currentTextChanged, this, &PositionWindow::applyFilters);
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

void PositionWindow::setupTable() {
    m_tableView = new CustomNetPosition(this);
    CustomNetPosition* posTable = qobject_cast<CustomNetPosition*>(m_tableView);
    if (posTable) {
        connect(posTable, &CustomNetPosition::exportRequested, this, &PositionWindow::onExportClicked);
        connect(posTable, &CustomNetPosition::closePositionRequested, this, &PositionWindow::onSquareOffClicked);
    }

    PositionModel* model = new PositionModel(this);
    m_model = model;
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_tableView->setModel(m_proxyModel);
}


void PositionWindow::onPositionsUpdated(const QVector<XTS::Position>& positions) {
    m_allPositions.clear();
    for (const auto& p : positions) {
        PositionData pd;
        pd.scripCode = p.exchangeInstrumentID; 
        pd.symbol = p.tradingSymbol; 
        
        bool isNumeric;
        int segId = p.exchangeSegment.toInt(&isNumeric);
        if (isNumeric) {
            pd.exchange = RepositoryManager::getExchangeSegmentName(segId);
        } else {
            pd.exchange = p.exchangeSegment;
        }
        
        pd.netQty = p.quantity; pd.buyAvg = p.buyAveragePrice; pd.sellAvg = p.sellAveragePrice;

        pd.mtm = p.mtm; pd.productType = p.productType; pd.client = p.accountID;
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
        if (v) filtered.append(p);
    }
    static_cast<PositionModel*>(m_model)->setPositions(filtered);
    updateSummaryRow();
}

void PositionWindow::updateSummaryRow() {
    PositionData summary;
    for (const auto& p : static_cast<PositionModel*>(m_model)->positions()) {
        summary.mtm += p.mtm; summary.buyQty += p.buyQty; summary.sellQty += p.sellQty;
    }
    static_cast<PositionModel*>(m_model)->setSummary(summary);
}

void PositionWindow::onColumnFilterChanged(int c, const QStringList& s) { 
    if (c == -1) m_columnFilters.clear(); else if (s.isEmpty()) m_columnFilters.remove(c); else m_columnFilters[c] = s; 
    applyFilters(); 
}

void PositionWindow::toggleFilterRow() { BaseBookWindow::toggleFilterRow(m_model, m_tableView); }
void PositionWindow::onRefreshClicked() { if (m_tradingDataService) onPositionsUpdated(m_tradingDataService->getPositions()); }
void PositionWindow::onExportClicked() { BaseBookWindow::exportToCSV(m_model, m_tableView); }
void PositionWindow::onSquareOffClicked() { qDebug() << "Square off"; }

void PositionWindow::addPosition(const PositionData& p) { m_allPositions.append(p); applyFilters(); }
void PositionWindow::updatePosition(const QString& s, const PositionData& p) {
    for (int i=0; i<m_allPositions.size(); ++i) if (m_allPositions[i].symbol == s) { m_allPositions[i] = p; break; }
    applyFilters();
}
void PositionWindow::clearPositions() { m_allPositions.clear(); applyFilters(); }
