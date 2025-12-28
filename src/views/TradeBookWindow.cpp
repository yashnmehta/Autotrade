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
#include <QDialog>
#include <QDialogButtonBox>
#include <QTimer>
#include <QDebug>
#include <QMenu>
#include <QCloseEvent>
#include "views/GenericProfileDialog.h"
#include "models/GenericProfileManager.h"
#include "utils/TableProfileHelper.h"
#include "utils/WindowSettingsHelper.h"

// Custom proxy model to keep filter row at top
class TradeBookSortProxyModel : public QSortFilterProxyModel {
public:
    explicit TradeBookSortProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

protected:
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override {
        TradeModel* model = qobject_cast<TradeModel*>(sourceModel());
        if (!model) return QSortFilterProxyModel::lessThan(source_left, source_right);

        if (model->rowCount() > 0 && model->data(model->index(0, 0), Qt::UserRole).toString() == "FILTER_ROW") {
            if (source_left.row() == 0) return (sortOrder() == Qt::AscendingOrder);
            if (source_right.row() == 0) return (sortOrder() == Qt::DescendingOrder);
        }

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

TradeBookWindow::TradeBookWindow(TradingDataService* tradingDataService, QWidget *parent)
    : QWidget(parent)
    , m_tableView(nullptr)
    , m_model(nullptr)
    , m_proxyModel(nullptr)
    , m_filterRowVisible(false)
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
    
    m_fromTime = QDateTime::currentDateTime().addDays(-7);
    m_toTime = QDateTime::currentDateTime();
    m_instrumentFilter = "All";
    m_buySellFilter = "All";
    m_orderTypeFilter = "All";
    m_exchangeFilter = "All";

    if (m_tradingDataService) {
        connect(m_tradingDataService, &TradingDataService::tradesUpdated, this, &TradeBookWindow::onTradesUpdated);
        onTradesUpdated(m_tradingDataService->getTrades());
    }
    
    m_filterShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(m_filterShortcut, &QShortcut::activated, this, &TradeBookWindow::toggleFilterRow);

    // Load and apply previous runtime state and customizations
    WindowSettingsHelper::loadAndApplyWindowSettings(this, "TradeBook");
    applyFilters();
}

TradeBookWindow::~TradeBookWindow() {}

void TradeBookWindow::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0); mainLayout->setSpacing(0);
    mainLayout->addWidget(createFilterWidget());
    setupTable();
    mainLayout->addWidget(m_tableView, 1);
    mainLayout->addWidget(createSummaryWidget());
}

QWidget* TradeBookWindow::createFilterWidget() {
    QWidget *container = new QWidget(this);
    container->setObjectName("filterContainer");
    container->setStyleSheet("QWidget#filterContainer { background-color: #2d2d2d; border-bottom: 1px solid #3f3f46; } QLabel { color: #d4d4d8; font-size: 11px; } QDateTimeEdit, QComboBox { background-color: #3f3f46; color: #ffffff; border: 1px solid #52525b; border-radius: 3px; font-size: 11px; } QPushButton { border-radius: 3px; font-weight: 600; font-size: 11px; padding: 5px 12px; }");
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(8);
    QHBoxLayout *filterLayout = new QHBoxLayout();
    m_fromTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-7));
    m_toTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime());
    m_fromTimeEdit->setDisplayFormat("dd-MM-yyyy HH:mm"); m_toTimeEdit->setDisplayFormat("dd-MM-yyyy HH:mm");
    auto addCombo = [&](const QString &l, QComboBox* &c, const QStringList &i, const QString& objName) {
        QVBoxLayout *v = new QVBoxLayout(); v->addWidget(new QLabel(l)); c = new QComboBox(); c->addItems(i); c->setObjectName(objName); v->addWidget(c); filterLayout->addLayout(v);
    };
    addCombo("Instrument", m_instrumentTypeCombo, {"All", "NSE OPT", "NSE FUT", "NSE EQ"}, "instrumentTypeCombo");
    addCombo("Exchange", m_exchangeCombo, {"All", "NSE", "BSE"}, "exchangeCombo");
    addCombo("Buy/Sell", m_buySellCombo, {"All", "Buy", "Sell"}, "buySellCombo");
    addCombo("Order Type", m_orderTypeCombo, {"All", "Day", "IOC"}, "orderTypeCombo");
    filterLayout->addStretch();
    m_applyFilterBtn = new QPushButton("Apply"); m_applyFilterBtn->setStyleSheet("background-color: #16a34a; color: white;");
    m_clearFilterBtn = new QPushButton("Clear"); m_clearFilterBtn->setStyleSheet("background-color: #52525b; color: white;");
    m_exportBtn = new QPushButton("Export"); m_exportBtn->setStyleSheet("background-color: #d97706; color: white;");
    m_showSummaryCheck = new QCheckBox("Summary Row"); m_showSummaryCheck->setStyleSheet("color: #d4d4d8;");
    m_showSummaryCheck->setObjectName("showSummaryCheck");
    filterLayout->addWidget(m_showSummaryCheck); filterLayout->addWidget(m_applyFilterBtn); filterLayout->addWidget(m_clearFilterBtn); filterLayout->addWidget(m_exportBtn);
    mainLayout->addLayout(filterLayout);
    return container;
}

QWidget* TradeBookWindow::createSummaryWidget() {
    QWidget *sw = new QWidget(); sw->setStyleSheet("background-color: #f5f5f5; border-top: 1px solid #ccc;"); sw->setFixedHeight(32);
    QHBoxLayout *l = new QHBoxLayout(sw); m_summaryLabel = new QLabel(); l->addWidget(m_summaryLabel); l->addStretch(); return sw;
}

void TradeBookWindow::setupTable() {
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
    m_model = new TradeModel(this);
    m_proxyModel = new TradeBookSortProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setDynamicSortFilter(true);
    m_tableView->setModel(m_proxyModel);
    for (int i=0; i<TradeModel::ColumnCount; ++i) m_tableView->setColumnWidth(i, 100);
}

void TradeBookWindow::setupConnections() {
    connect(m_applyFilterBtn, &QPushButton::clicked, this, &TradeBookWindow::applyFilters);
    connect(m_clearFilterBtn, &QPushButton::clicked, this, &TradeBookWindow::clearFilters);
    connect(m_exportBtn, &QPushButton::clicked, this, &TradeBookWindow::exportToCSV);
    connect(m_showSummaryCheck, &QCheckBox::toggled, m_tableView, &CustomTradeBook::setSummaryRowEnabled);
}

void TradeBookWindow::refreshTrades() { if (m_tradingDataService) onTradesUpdated(m_tradingDataService->getTrades()); }

void TradeBookWindow::onTradesUpdated(const QVector<XTS::Trade>& trades) { m_allTrades = trades; applyFilterToModel(); }

void TradeBookWindow::applyFilters() {
    m_instrumentFilter = m_instrumentTypeCombo->currentText();
    m_buySellFilter = m_buySellCombo->currentText();
    m_exchangeFilter = m_exchangeCombo->currentText();
    m_fromTime = m_fromTimeEdit->dateTime();
    m_toTime = m_toTimeEdit->dateTime();
    applyFilterToModel();
}

void TradeBookWindow::clearFilters() {
    m_instrumentTypeCombo->setCurrentIndex(0); m_buySellCombo->setCurrentIndex(0); m_exchangeCombo->setCurrentIndex(0);
    m_fromTimeEdit->setDateTime(QDateTime::currentDateTime().addDays(-7)); m_toTimeEdit->setDateTime(QDateTime::currentDateTime());
    m_columnFilters.clear(); for (auto w : m_filterWidgets) w->clear(); applyFilterToModel();
}

void TradeBookWindow::applyFilterToModel() {
    QVector<XTS::Trade> filtered;
    for (const auto& t : m_allTrades) {
        bool v = true;
        QDateTime dt = QDateTime::fromString(t.lastExecutionTransactTime, "dd-MM-yyyy HH:mm:ss");
        if (dt.isValid() && (dt < m_fromTime || dt > m_toTime)) v = false;
        
        // Instrument Type Filter
        if (v && m_instrumentFilter != "All") {
            if (m_instrumentFilter == "NSE EQ" && t.exchangeSegment != "NSECM") v = false;
            else if ((m_instrumentFilter == "NSE OPT" || m_instrumentFilter == "NSE FUT") && t.exchangeSegment != "NSEFO") v = false;
            else if (!t.tradingSymbol.contains(m_instrumentFilter, Qt::CaseInsensitive) && 
                     !t.exchangeSegment.contains(m_instrumentFilter, Qt::CaseInsensitive)) {
                if (m_instrumentFilter != "NSE EQ" && m_instrumentFilter != "NSE OPT" && m_instrumentFilter != "NSE FUT")
                    v = false;
            }
        }

        if (v && m_exchangeFilter != "All" && !t.exchangeSegment.contains(m_exchangeFilter, Qt::CaseInsensitive)) v = false;
        if (v && m_buySellFilter != "All" && t.orderSide.trimmed().toUpper() != m_buySellFilter.toUpper()) v = false;
        
        // Order Type Filter
        if (v && m_orderTypeFilter != "All" && !t.orderType.contains(m_orderTypeFilter, Qt::CaseInsensitive)) v = false;

        if (v && !m_columnFilters.isEmpty()) {
            for (auto it = m_columnFilters.constBegin(); it != m_columnFilters.constEnd(); ++it) {
                int c = it.key(); const QStringList& a = it.value();
                if (!a.isEmpty()) {
                    QString val;
                    if (c == TradeModel::Symbol) val = t.tradingSymbol;
                    else if (c == TradeModel::BuySell) val = t.orderSide;
                    else if (c == TradeModel::ExchangeCode) val = t.exchangeSegment;
                    if (!val.isEmpty() && !a.contains(val)) { v = false; break; }
                }
            }
        }
        if (v) filtered.append(t);
    }
    m_model->setTrades(filtered); updateSummary();
}

void TradeBookWindow::updateSummary() {
    int total = m_model->rowCount(); if (m_filterRowVisible) total--;
    double bq=0, sq=0, bv=0, sv=0;
    for (int i=0; i<m_model->rowCount(); ++i) {
        if (m_filterRowVisible && i == 0) continue;
        QString s = m_model->data(m_model->index(i, TradeModel::BuySell)).toString().toUpper();
        int q = m_model->data(m_model->index(i, TradeModel::Quantity), Qt::UserRole).toInt();
        double p = m_model->data(m_model->index(i, TradeModel::Price), Qt::UserRole).toDouble();
        if (s == "BUY") { bq += q; bv += q*p; } else { sq += q; sv += q*p; }
    }
    m_summaryLabel->setText(QString("Trades: %1 | Buy Qty: %2 | Sell Qty: %3 | Buy Val: ₹%4 | Sell Val: ₹%5").arg(total).arg(bq).arg(sq).arg(bv, 0, 'f', 2).arg(sv, 0, 'f', 2));
}

void TradeBookWindow::toggleFilterRow() {
    m_filterRowVisible = !m_filterRowVisible; m_model->setFilterRowVisible(m_filterRowVisible);
    if (m_filterRowVisible) {
        m_filterWidgets.clear();
        for (int i=0; i<TradeModel::ColumnCount; ++i) {
            GenericTableFilter* fw = new GenericTableFilter(i, m_model, m_tableView, m_tableView);
            fw->setFilterRowOffset(1);
            m_filterWidgets.append(fw); m_tableView->setIndexWidget(m_model->index(0, i), fw);
            connect(fw, &GenericTableFilter::filterChanged, this, &TradeBookWindow::onColumnFilterChanged);
        }
    } else {
        m_columnFilters.clear(); for (int i=0; i<TradeModel::ColumnCount; ++i) m_tableView->setIndexWidget(m_model->index(0, i), nullptr);
        applyFilterToModel();
    }
}

void TradeBookWindow::onColumnFilterChanged(int c, const QStringList& s) { if (s.isEmpty()) m_columnFilters.remove(c); else m_columnFilters[c] = s; applyFilterToModel(); }

void TradeBookWindow::showColumnProfileDialog() {
    TableProfileHelper::showProfileDialog("TradeBook", m_tableView, m_model, m_columnProfile, this);
}

void TradeBookWindow::loadInitialProfile() {
    TableProfileHelper::loadProfile("TradeBook", m_tableView, m_model, m_columnProfile);
}

void TradeBookWindow::exportToCSV() {
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

void TradeBookWindow::closeEvent(QCloseEvent *event) {
    TableProfileHelper::saveCurrentProfile("TradeBook", m_tableView, m_model, m_columnProfile);
    WindowSettingsHelper::saveWindowSettings(this, "TradeBook");
    QWidget::closeEvent(event);
}
