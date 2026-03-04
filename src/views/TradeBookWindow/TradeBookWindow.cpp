#include "views/TradeBookWindow.h"
#include "ui_TradeBookWindow.h"
#include "core/widgets/CustomTradeBook.h"
#include "services/TradingDataService.h"
#include "utils/WindowSettingsHelper.h"
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QMenu>
#include <QDebug>
#include "models/qt/PinnedRowProxyModel.h"

TradeBookWindow::TradeBookWindow(TradingDataService* tradingDataService, QWidget *parent)
    : BaseBookWindow("TradeBook", parent),
      ui(new Ui::TradeBookWindow),
      m_tradingDataService(tradingDataService) 
{
    setupUI();
    loadInitialProfile();
    setupConnections();
    
    m_instrumentFilter = m_exchangeFilter = m_buySellFilter = m_orderTypeFilter = "All";
    m_fromTime = QDateTime::currentDateTime().addDays(-7);
    m_toTime = QDateTime::currentDateTime();

    if (m_tradingDataService) {
        connect(m_tradingDataService, &TradingDataService::tradesUpdated, this, &TradeBookWindow::onTradesUpdated);
        onTradesUpdated(m_tradingDataService->getTrades());
    }
    connect(m_filterShortcut, &QShortcut::activated, this, &TradeBookWindow::toggleFilterRow);

    // Restore saved runtime state (combo selections, geometry)
    WindowSettingsHelper::loadAndApplyWindowSettings(this, "TradeBook");
}

TradeBookWindow::~TradeBookWindow() { delete ui; }

void TradeBookWindow::setupUI() {
    ui->setupUi(this);

    // Assign convenience pointers from ui
    m_instrumentTypeCombo = ui->m_instrumentTypeCombo;
    m_exchangeCombo       = ui->m_exchangeCombo;
    m_buySellCombo        = ui->m_buySellCombo;
    m_orderTypeCombo      = ui->m_orderTypeCombo;
    m_showSummaryCheck    = ui->m_showSummaryCheck;
    m_applyFilterBtn      = ui->m_applyFilterBtn;
    m_clearFilterBtn      = ui->m_clearFilterBtn;
    m_exportBtn           = ui->m_exportBtn;
    m_summaryLabel        = ui->m_summaryLabel;

    // Replace placeholder table with real CustomTradeBook
    setupTable();
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(layout());
    if (mainLayout) {
        mainLayout->replaceWidget(ui->m_tableViewPlaceholder, m_tableView);
        ui->m_tableViewPlaceholder->hide();
        ui->m_tableViewPlaceholder->deleteLater();
    }
}

void TradeBookWindow::setupTable() {
    m_tableView = new CustomTradeBook(this);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
        showTradeBookContextMenu(pos);
    });
    // Unified: same context menu when right-clicking the header
    m_tableView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView->horizontalHeader(), &QHeaderView::customContextMenuRequested, this,
            [this](const QPoint &headerPos) {
                QPoint viewportPos = m_tableView->viewport()->mapFromGlobal(
                    m_tableView->horizontalHeader()->mapToGlobal(headerPos));
                showTradeBookContextMenu(viewportPos);
            });
    TradeModel* model = new TradeModel(this);
    m_model = model;
    m_proxyModel = new PinnedRowProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_tableView->setModel(m_proxyModel);
}

void TradeBookWindow::onTradesUpdated(const QVector<XTS::Trade>& trades) { m_allTrades = trades; applyFilterToModel(); }

void TradeBookWindow::applyFilters() {
    m_instrumentFilter = m_instrumentTypeCombo->currentText();
    m_exchangeFilter = m_exchangeCombo->currentText();
    m_buySellFilter = m_buySellCombo->currentText();
    m_orderTypeFilter = m_orderTypeCombo->currentText();
    applyFilterToModel();
}

void TradeBookWindow::clearFilters() {
    m_instrumentTypeCombo->setCurrentIndex(0); m_exchangeCombo->setCurrentIndex(0); m_buySellCombo->setCurrentIndex(0); m_orderTypeCombo->setCurrentIndex(0);
    applyFilters();
}

void TradeBookWindow::applyFilterToModel() {
    QVector<XTS::Trade> filtered;
    for (const auto& t : m_allTrades) {
        bool v = true;
        if (m_instrumentFilter != "All" && !t.tradingSymbol.contains(m_instrumentFilter, Qt::CaseInsensitive)) v = false;
        if (v && m_exchangeFilter != "All" && !t.exchangeSegment.contains(m_exchangeFilter, Qt::CaseInsensitive)) v = false;
        if (v && m_buySellFilter != "All" && t.orderSide.toUpper() != m_buySellFilter.toUpper()) v = false;
        
        if (v && !m_columnFilters.isEmpty()) {
            // column specific filtering...
        }
        
        // Text filters (inline search)
        if (v && !m_textFilters.isEmpty()) {
            for (auto it = m_textFilters.begin(); it != m_textFilters.end(); ++it) {
                int col = it.key();
                const QString& filterText = it.value();
                if (filterText.isEmpty()) continue;
                
                QString val;
                bool hasMapping = true;
                switch (col) {
                    case TradeModel::Symbol: val = t.tradingSymbol; break;
                    case TradeModel::ExchangeCode: val = t.exchangeSegment; break;
                    case TradeModel::BuySell: val = t.orderSide; break;
                    case TradeModel::OrderType: val = t.orderType; break;
                    case TradeModel::ExchOrdNo: val = t.exchangeOrderID; break;
                    case TradeModel::Client: val = t.clientID; break;
                    case TradeModel::InstrumentName: val = t.tradingSymbol; break;
                    case TradeModel::Code: val = QString::number(t.exchangeInstrumentID); break;
                    default: hasMapping = false; break;
                }
                
                // Only filter if we have a mapping for this column
                if (hasMapping && !val.contains(filterText, Qt::CaseInsensitive)) {
                    v = false;
                    break;
                }
            }
        }
        
        if (v) filtered.append(t);
    }
    static_cast<TradeModel*>(m_model)->setTrades(filtered); 
    updateSummary();
}


void TradeBookWindow::updateSummary() {
    int total = m_model->rowCount() - (m_filterRowVisible ? 1 : 0);
    m_summaryLabel->setText(QString("Trades: %1").arg(total));
}

void TradeBookWindow::onColumnFilterChanged(int c, const QStringList& s) { 
    if (c == -1) m_columnFilters.clear(); else if (s.isEmpty()) m_columnFilters.remove(c); else m_columnFilters[c] = s; 
    applyFilterToModel(); 
}

void TradeBookWindow::onTextFilterChanged(int col, const QString& text) {
    BaseBookWindow::onTextFilterChanged(col, text);
    applyFilterToModel();
}

void TradeBookWindow::toggleFilterRow() { BaseBookWindow::toggleFilterRow(m_model, m_tableView); }
void TradeBookWindow::exportToCSV() { BaseBookWindow::exportToCSV(m_model, m_tableView); }

void TradeBookWindow::showTradeBookContextMenu(const QPoint &pos) {
    QMenu menu(this);
    menu.addAction("Column Profile...", this, &TradeBookWindow::showColumnProfileDialog);
    menu.addSeparator();
    menu.addAction("Export to CSV", this, &TradeBookWindow::exportToCSV);
    menu.addAction("Copy", this, [this]() {
        // TODO: copy selected rows
    });
    menu.addSeparator();
    menu.addAction("Refresh", this, &TradeBookWindow::refreshTrades);
    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}
void TradeBookWindow::refreshTrades() { if (m_tradingDataService) onTradesUpdated(m_tradingDataService->getTrades()); }

