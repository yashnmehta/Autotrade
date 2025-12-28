#ifndef ORDERBOOKWINDOW_H
#define ORDERBOOKWINDOW_H

#include <QWidget>
#include <QDateTime>
#include <QShortcut>
#include <QLabel>
#include <QMap>
#include <QVector>
#include "models/GenericTableProfile.h"
#include "api/XTSTypes.h"
#include "models/OrderModel.h"

#include "views/helpers/GenericTableFilter.h"

class CustomOrderBook;
class QComboBox;
class QDateTimeEdit;
class QPushButton;
class QCheckBox;
class TradingDataService;
class QSortFilterProxyModel;

class OrderBookWindow : public QWidget
{
    Q_OBJECT

public:
    explicit OrderBookWindow(TradingDataService* tradingDataService, QWidget *parent = nullptr);
    ~OrderBookWindow();

    // Filter methods
    void setInstrumentFilter(const QString &instrument);
    void setTimeFilter(const QDateTime &fromTime, const QDateTime &toTime);
    void setStatusFilter(const QString &status);
    void setOrderTypeFilter(const QString &orderType);

public slots:
    void refreshOrders();

private slots:
    void applyFilters();
    void clearFilters();
    void exportToCSV();
    void toggleFilterRow();
    void onColumnFilterChanged(int column, const QStringList& selectedValues);
    void showColumnProfileDialog();
    void onCancelOrder();
    void onModifyOrder();
    void onOrdersUpdated(const QVector<XTS::Order>& orders);

signals:
    void orderSelected(const QString &orderId);

private:
    void setupUI();
    void setupTable();
    void loadInitialProfile();
    void setupConnections();
    void applyFilterToModel();
    void updateSummary();
    
    QWidget* createFilterWidget();
    QWidget* createSummaryWidget();
    
    TradingDataService* m_tradingDataService;
    QVector<XTS::Order> m_allOrders;

    CustomOrderBook *m_tableView;
    OrderModel *m_model;
    QSortFilterProxyModel *m_proxyModel;

    QComboBox *m_instrumentTypeCombo;
    QComboBox *m_statusCombo;
    QComboBox *m_orderTypeCombo;
    QComboBox *m_exchangeCombo;
    QComboBox *m_buySellCombo;
    QDateTimeEdit *m_fromTimeEdit;
    QDateTimeEdit *m_toTimeEdit;
    QPushButton *m_applyFilterBtn;
    QPushButton *m_clearFilterBtn;
    QPushButton *m_exportBtn;

    bool m_filterRowVisible;
    QShortcut* m_filterShortcut;
    QList<GenericTableFilter*> m_filterWidgets;
    QMap<int, QStringList> m_columnFilters;
    GenericTableProfile m_columnProfile;

    // Filter State
    QDateTime m_fromTime;
    QDateTime m_toTime;
    QString m_instrumentFilter;
    QString m_statusFilter;
    QString m_buySellFilter;
    QString m_exchangeFilter;
    QString m_orderTypeFilter;

    QLabel *m_summaryLabel;
    int m_totalOrders;
    int m_openOrders;
    int m_executedOrders;
    int m_cancelledOrders;
    double m_totalOrderValue;
    
protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif // ORDERBOOKWINDOW_H
