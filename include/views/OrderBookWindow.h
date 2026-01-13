#ifndef ORDERBOOKWINDOW_H
#define ORDERBOOKWINDOW_H

#include "views/BaseBookWindow.h"
#include "api/XTSTypes.h"
#include "models/OrderModel.h"

class CustomOrderBook;
class TradingDataService;
class QComboBox;
class QDateTimeEdit;
class QPushButton;
class QLabel;

class OrderBookWindow : public BaseBookWindow {
    Q_OBJECT
public:
    explicit OrderBookWindow(TradingDataService* tradingDataService, QWidget *parent = nullptr);
    ~OrderBookWindow();

    void setInstrumentFilter(const QString &instrument);
    void setTimeFilter(const QDateTime &fromTime, const QDateTime &toTime);
    void setStatusFilter(const QString &status);
    void setOrderTypeFilter(const QString &orderType);

signals:
    // Order modification/cancellation signals
    void modifyOrderRequested(const XTS::Order &order);
    void batchModifyRequested(const QVector<XTS::Order> &orders);
    void cancelOrderRequested(int64_t appOrderID);

public slots:
    void refreshOrders();

private slots:
    void applyFilters();
    void clearFilters();
    void onCancelOrder();
    void onModifyOrder();
    void onOrdersUpdated(const QVector<XTS::Order>& orders);
    void toggleFilterRow();
    void exportToCSV();

protected:
    void setupUI() override;
    void onColumnFilterChanged(int column, const QStringList& selectedValues) override;
    void onTextFilterChanged(int column, const QString& text) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupTable();
    void setupConnections();
    void applyFilterToModel();
    void updateSummary();
    
    QWidget* createFilterWidget();
    QWidget* createSummaryWidget();
    
    // Helper to get selected order
    bool getSelectedOrder(XTS::Order &outOrder) const;
    QVector<XTS::Order> getSelectedOrders() const;
    
    TradingDataService* m_tradingDataService;
    QVector<XTS::Order> m_allOrders;

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

    QMap<int, QStringList> m_columnFilters;

    QLabel *m_summaryLabel;
    int m_totalOrders;
    int m_openOrders;
    int m_executedOrders;
    int m_cancelledOrders;
    double m_totalOrderValue;

    QString m_instrumentFilter;
    QString m_statusFilter;
    QString m_buySellFilter;
    QString m_exchangeFilter;
    QString m_orderTypeFilter;
    QDateTime m_fromTime;
    QDateTime m_toTime;
};

#endif // ORDERBOOKWINDOW_H

