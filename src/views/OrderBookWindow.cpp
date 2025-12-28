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
#include <QDialog>
#include <QDialogButtonBox>
#include <QTimer>
#include <QDebug>
#include <QMenu>
#include "views/GenericProfileDialog.h"
#include "models/GenericProfileManager.h"
#include "utils/TableProfileHelper.h"

// Custom proxy model to keep filter row at top
class OrderBookSortProxyModel : public QSortFilterProxyModel {
public:
    explicit OrderBookSortProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

protected:
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override {
        OrderModel* model = qobject_cast<OrderModel*>(sourceModel());
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

OrderBookWindow::OrderBookWindow(TradingDataService* tradingDataService, QWidget *parent)
    : QWidget(parent)
    , m_tableView(nullptr)
    , m_model(nullptr)
    , m_filterRowVisible(false)
    , m_tradingDataService(tradingDataService)
{
    setupUI();
    loadInitialProfile();
    setupConnections();
    
    // Initialize filters to "All"
    m_instrumentFilter = "All";
    m_statusFilter = "All";
    m_buySellFilter = "All";
    m_exchangeFilter = "All";
    m_orderTypeFilter = "All";
    m_fromTime = QDateTime::currentDateTime().addDays(-7);
    m_toTime = QDateTime::currentDateTime();
    if (m_tradingDataService) {
        connect(m_tradingDataService, &TradingDataService::ordersUpdated, this, &OrderBookWindow::onOrdersUpdated);
        onOrdersUpdated(m_tradingDataService->getOrders());
    }
    m_filterShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(m_filterShortcut, &QShortcut::activated, this, &OrderBookWindow::toggleFilterRow);
}

OrderBookWindow::~OrderBookWindow() {}

void OrderBookWindow::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0); mainLayout->setSpacing(0);
    mainLayout->addWidget(createFilterWidget());
    setupTable();
    mainLayout->addWidget(m_tableView, 1);
    mainLayout->addWidget(createSummaryWidget());
}

QWidget* OrderBookWindow::createFilterWidget() {
    QWidget *container = new QWidget(this);
    container->setObjectName("filterContainer");
    container->setStyleSheet("QWidget#filterContainer { background-color: #2d2d2d; border-bottom: 1px solid #3f3f46; } QLabel { color: #d4d4d8; font-size: 11px; } QDateTimeEdit, QComboBox { background-color: #3f3f46; color: #ffffff; border: 1px solid #52525b; border-radius: 3px; font-size: 11px; } QPushButton { border-radius: 3px; font-weight: 600; font-size: 11px; padding: 5px 12px; }");
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(8);
    QHBoxLayout *filterLayout = new QHBoxLayout();
    m_fromTimeEdit = new QDateTimeEdit(); m_toTimeEdit = new QDateTimeEdit();
    m_fromTimeEdit->setDisplayFormat("dd-MM-yyyy HH:mm"); m_toTimeEdit->setDisplayFormat("dd-MM-yyyy HH:mm");
    auto addCombo = [&](const QString &l, QComboBox* &c, const QStringList &i) {
        QVBoxLayout *v = new QVBoxLayout(); v->addWidget(new QLabel(l)); c = new QComboBox(); c->addItems(i); v->addWidget(c); filterLayout->addLayout(v);
    };
    addCombo("Instrument", m_instrumentTypeCombo, {"All", "NSE OPT", "NSE FUT", "NSE EQ"});
    addCombo("Status", m_statusCombo, {"All", "Open", "Filled", "Cancelled", "Rejected"});
    addCombo("Buy/Sell", m_buySellCombo, {"All", "Buy", "Sell"});
    addCombo("Exchange", m_exchangeCombo, {"All", "NSE", "BSE"});
    addCombo("Order Type", m_orderTypeCombo, {"All", "Market", "Limit"});
    filterLayout->addStretch();
    m_applyFilterBtn = new QPushButton("Apply"); m_applyFilterBtn->setStyleSheet("background-color: #16a34a; color: white;");
    m_clearFilterBtn = new QPushButton("Clear"); m_clearFilterBtn->setStyleSheet("background-color: #52525b; color: white;");
    m_exportBtn = new QPushButton("Export"); m_exportBtn->setStyleSheet("background-color: #d97706; color: white;");
    filterLayout->addWidget(m_applyFilterBtn); filterLayout->addWidget(m_clearFilterBtn); filterLayout->addWidget(m_exportBtn);
    mainLayout->addLayout(filterLayout);
    return container;
}

QWidget* OrderBookWindow::createSummaryWidget() {
    QWidget *sw = new QWidget(); sw->setStyleSheet("background-color: #f5f5f5; border-top: 1px solid #ccc;"); sw->setFixedHeight(32);
    QHBoxLayout *l = new QHBoxLayout(sw); m_summaryLabel = new QLabel(); l->addWidget(m_summaryLabel); l->addStretch(); return sw;
}

void OrderBookWindow::setupTable() {
    m_tableView = new CustomOrderBook(this);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        menu.addAction("Modify Order", this, &OrderBookWindow::onModifyOrder);
        menu.addAction("Cancel Order", this, &OrderBookWindow::onCancelOrder);
        menu.addSeparator();
        menu.addAction("Export to CSV", this, &OrderBookWindow::exportToCSV);
        menu.addAction("Column Profile...", this, &OrderBookWindow::showColumnProfileDialog);
        menu.exec(m_tableView->viewport()->mapToGlobal(pos));
    });
    m_model = new OrderModel(this);
    m_proxyModel = new OrderBookSortProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setDynamicSortFilter(true);
    m_tableView->setModel(m_proxyModel);
    for (int i=0; i<OrderModel::ColumnCount; ++i) m_tableView->setColumnWidth(i, 100);
}

void OrderBookWindow::setupConnections() {
    connect(m_applyFilterBtn, &QPushButton::clicked, this, &OrderBookWindow::applyFilters);
    connect(m_clearFilterBtn, &QPushButton::clicked, this, &OrderBookWindow::clearFilters);
    connect(m_exportBtn, &QPushButton::clicked, this, &OrderBookWindow::exportToCSV);
}

void OrderBookWindow::refreshOrders() { if (m_tradingDataService) onOrdersUpdated(m_tradingDataService->getOrders()); }

void OrderBookWindow::onOrdersUpdated(const QVector<XTS::Order>& orders) { 
    m_allOrders = orders; 
    qDebug() << "[OrderBookWindow] Received" << m_allOrders.size() << "orders from service";
    applyFilterToModel(); 
}

void OrderBookWindow::applyFilters() {
    m_instrumentFilter = m_instrumentTypeCombo->currentText();
    m_statusFilter = m_statusCombo->currentText();
    m_buySellFilter = m_buySellCombo->currentText();
    m_exchangeFilter = m_exchangeCombo->currentText();
    m_orderTypeFilter = m_orderTypeCombo->currentText();
    applyFilterToModel();
}

void OrderBookWindow::clearFilters() {
    m_instrumentTypeCombo->setCurrentIndex(0); 
    m_statusCombo->setCurrentIndex(0); 
    m_buySellCombo->setCurrentIndex(0);
    m_exchangeCombo->setCurrentIndex(0); 
    m_orderTypeCombo->setCurrentIndex(0);
    m_instrumentFilter = "All";
    m_statusFilter = "All";
    m_buySellFilter = "All";
    m_exchangeFilter = "All";
    m_orderTypeFilter = "All";
    m_columnFilters.clear(); 
    for (auto w : m_filterWidgets) w->clear(); 
    applyFilterToModel();
}

void OrderBookWindow::applyFilterToModel() {
    QVector<XTS::Order> filtered;
    QString instrF = m_instrumentFilter.trimmed().toUpper();
    QString statusF = m_statusFilter.trimmed().toUpper();
    QString bsF = m_buySellFilter.trimmed().toUpper();
    QString exchF = m_exchangeFilter.trimmed().toUpper();
    QString typeF = m_orderTypeFilter.trimmed().toUpper();

    for (const auto& o : m_allOrders) {
        bool v = true;
        
        // Instrument Type Filter
        if (instrF != "ALL") {
            QString segment = o.exchangeSegment.toUpper();
            if (instrF == "NSE EQ" && segment != "NSECM") v = false;
            else if ((instrF == "NSE OPT" || instrF == "NSE FUT") && segment != "NSEFO") v = false;
            else if (instrF != "NSE EQ" && instrF != "NSE OPT" && instrF != "NSE FUT") {
                if (!o.tradingSymbol.contains(instrF, Qt::CaseInsensitive) && 
                    !segment.contains(instrF, Qt::CaseInsensitive)) v = false;
            }
        }
        
        // Status Filter
        if (v && statusF != "ALL") {
            if (!o.orderStatus.contains(statusF, Qt::CaseInsensitive)) v = false;
        }
        
        // Buy/Sell Filter
        if (v && bsF != "ALL") {
            if (o.orderSide.trimmed().toUpper() != bsF) v = false;
        }
        
        // Exchange Filter
        if (v && exchF != "ALL") {
            if (!o.exchangeSegment.contains(exchF, Qt::CaseInsensitive)) v = false;
        }
        
        // Order Type Filter
        if (v && typeF != "ALL") {
            if (!o.orderType.contains(typeF, Qt::CaseInsensitive)) v = false;
        }

        // Column filters
        if (v && !m_columnFilters.isEmpty()) {
            for (auto it = m_columnFilters.constBegin(); it != m_columnFilters.constEnd(); ++it) {
                int c = it.key(); const QStringList& a = it.value();
                if (!a.isEmpty()) {
                    QString val;
                    if (c == OrderModel::Symbol) val = o.tradingSymbol;
                    else if (c == OrderModel::Status) val = o.orderStatus;
                    else if (c == OrderModel::BuySell) val = o.orderSide;
                    else if (c == OrderModel::ExchangeCode) val = o.exchangeSegment;
                    
                    bool found = false;
                    for (const QString& s : a) {
                        if (val.compare(s, Qt::CaseInsensitive) == 0) { found = true; break; }
                    }
                    if (!found) { v = false; break; }
                }
            }
        }
        if (v) filtered.append(o);
    }
    
    qDebug() << "[OrderBookWindow] Filtered" << filtered.size() << "out of" << m_allOrders.size() << "orders";
    m_model->setOrders(filtered); 
    updateSummary();
}

void OrderBookWindow::updateSummary() {
    m_totalOrders = m_model->rowCount(); if (m_filterRowVisible) m_totalOrders--;
    m_openOrders = 0; m_executedOrders = 0; m_cancelledOrders = 0; m_totalOrderValue = 0.0;
    for (int i=0; i<m_model->rowCount(); ++i) {
        if (m_filterRowVisible && i == 0) continue;
        QString s = m_model->data(m_model->index(i, OrderModel::Status)).toString();
        double val = m_model->data(m_model->index(i, OrderModel::Price), Qt::UserRole).toDouble() * m_model->data(m_model->index(i, OrderModel::TotalQty), Qt::UserRole).toInt();
        if (s.contains("Filled", Qt::CaseInsensitive)) m_executedOrders++;
        else if (s.contains("Open", Qt::CaseInsensitive) || s.contains("New", Qt::CaseInsensitive)) m_openOrders++;
        else if (s.contains("Cancelled", Qt::CaseInsensitive)) m_cancelledOrders++;
        m_totalOrderValue += val;
    }
    m_summaryLabel->setText(QString("Total: %1 | Open: %2 | Executed: %3 | Cancelled: %4 | Value: ₹%5").arg(m_totalOrders).arg(m_openOrders).arg(m_executedOrders).arg(m_cancelledOrders).arg(m_totalOrderValue, 0, 'f', 2));
}

void OrderBookWindow::toggleFilterRow() {
    m_filterRowVisible = !m_filterRowVisible; m_model->setFilterRowVisible(m_filterRowVisible);
    if (m_filterRowVisible) {
        m_filterWidgets.clear();
        for (int i=0; i<OrderModel::ColumnCount; ++i) {
            OrderBookFilterWidget* fw = new OrderBookFilterWidget(i, this, m_tableView);
            m_filterWidgets.append(fw); m_tableView->setIndexWidget(m_model->index(0, i), fw);
            connect(fw, &OrderBookFilterWidget::filterChanged, this, &OrderBookWindow::onColumnFilterChanged);
        }
    } else {
        m_columnFilters.clear(); for (int i=0; i<OrderModel::ColumnCount; ++i) m_tableView->setIndexWidget(m_model->index(0, i), nullptr);
        applyFilterToModel();
    }
}

void OrderBookWindow::onColumnFilterChanged(int c, const QStringList& s) { if (s.isEmpty()) m_columnFilters.remove(c); else m_columnFilters[c] = s; applyFilterToModel(); }

void OrderBookWindow::showColumnProfileDialog() {
    TableProfileHelper::showProfileDialog("OrderBook", m_tableView, m_model, m_columnProfile, this);
}

void OrderBookWindow::loadInitialProfile() {
    TableProfileHelper::loadProfile("OrderBook", m_tableView, m_model, m_columnProfile);
}

void OrderBookWindow::onModifyOrder() {
    QModelIndex idx = m_tableView->currentIndex();
    if (!idx.isValid()) { QMessageBox::warning(this, "Modify", "Select an order."); return; }
    QString id = m_model->data(m_proxyModel->mapToSource(idx), Qt::DisplayRole).toString();
    qDebug() << "Modify order:" << id;
}

void OrderBookWindow::onCancelOrder() {
    QModelIndex idx = m_tableView->currentIndex();
    if (!idx.isValid()) { QMessageBox::warning(this, "Cancel", "Select an order."); return; }
    QString id = m_model->data(m_proxyModel->mapToSource(idx), Qt::DisplayRole).toString();
    if (QMessageBox::question(this, "Cancel", "Cancel order " + id + "?") == QMessageBox::Yes) {
        qDebug() << "Cancelled order:" << id;
    }
}

void OrderBookWindow::exportToCSV() {
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

void OrderBookWindow::setInstrumentFilter(const QString& i) { m_instrumentTypeCombo->setCurrentText(i); applyFilters(); }
void OrderBookWindow::setTimeFilter(const QDateTime& f, const QDateTime& t) { m_fromTimeEdit->setDateTime(f); m_toTimeEdit->setDateTime(t); applyFilters(); }
void OrderBookWindow::setStatusFilter(const QString& s) { m_statusCombo->setCurrentText(s); applyFilters(); }
void OrderBookWindow::setOrderTypeFilter(const QString& o) { m_orderTypeCombo->setCurrentText(o); applyFilters(); }

OrderBookFilterWidget::OrderBookFilterWidget(int column, OrderBookWindow* window, QWidget* parent)
    : QWidget(parent), m_column(column), m_orderBookWindow(window) {
    QHBoxLayout* l = new QHBoxLayout(this); l->setContentsMargins(2,2,2,2);
    m_filterButton = new QPushButton("▼ Filter");
    m_filterButton->setStyleSheet("background-color: #f0f0f0; color: #333; border: 1px solid #ccc; font-size: 10px;");
    l->addWidget(m_filterButton); connect(m_filterButton, &QPushButton::clicked, this, &OrderBookFilterWidget::showFilterPopup);
}

void OrderBookFilterWidget::showFilterPopup() {
    QDialog d(this); d.setWindowTitle("Filter"); QVBoxLayout* l = new QVBoxLayout(&d);
    QListWidget* lw = new QListWidget(&d);
    QSet<QString> vals; for (int i=(m_orderBookWindow->m_filterRowVisible?1:0); i<m_orderBookWindow->m_model->rowCount(); ++i) {
        QString v = m_orderBookWindow->m_model->data(m_orderBookWindow->m_model->index(i, m_column), Qt::DisplayRole).toString();
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

QStringList OrderBookFilterWidget::getUniqueValuesForColumn() const {
    QSet<QString> vals; if (!m_orderBookWindow || !m_orderBookWindow->m_model) return QStringList();
    for (int i=(m_orderBookWindow->m_filterRowVisible?1:0); i<m_orderBookWindow->m_model->rowCount(); ++i) {
        QString v = m_orderBookWindow->m_model->data(m_orderBookWindow->m_model->index(i, m_column), Qt::DisplayRole).toString();
        if (!v.isEmpty()) vals.insert(v);
    }
    QStringList sorted = vals.values(); sorted.sort(); return sorted;
}

void OrderBookFilterWidget::updateButtonDisplay() {
    if (m_selectedValues.isEmpty()) m_filterButton->setText("▼ Filter");
    else m_filterButton->setText(QString("▼ (%1)").arg(m_selectedValues.count()));
}

void OrderBookFilterWidget::clear() { m_selectedValues.clear(); updateButtonDisplay(); }
