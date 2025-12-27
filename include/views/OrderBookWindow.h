#ifndef ORDERBOOKWINDOW_H
#define ORDERBOOKWINDOW_H

#include <QWidget>
#include <QStandardItemModel>
#include <QDateTime>
#include <QShortcut>
#include <QLabel>
#include "models/GenericTableProfile.h"
#include "api/XTSTypes.h"

class CustomOrderBook;
class QComboBox;
class QDateTimeEdit;
class QPushButton;
class QCheckBox;
class OrderBookFilterWidget;
class TradingDataService;

class OrderBookWindow : public QWidget
{
    Q_OBJECT
    
    friend class OrderBookFilterWidget;

public:
    explicit OrderBookWindow(class TradingDataService* tradingDataService, QWidget *parent = nullptr);
    ~OrderBookWindow();

    // Filter methods
    void setInstrumentFilter(const QString &instrument);
    void setTimeFilter(const QDateTime &fromTime, const QDateTime &toTime);
    void setStatusFilter(const QString &status);  // "All", "Open", "Executed", "Cancelled"
    void setOrderTypeFilter(const QString &orderType);  // "All", "Market", "Limit", "SL", "SL-M"

public slots:
    void refreshOrders();

private slots:
    void applyFilters();
    void clearFilters();
    void exportToCSV();
    void toggleFilterRow();
    void onColumnFilterChanged(int column, const QStringList& selectedValues);
    void showColumnProfileDialog(); // Added
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
    
    // UI Helper methods
    QWidget* createFilterWidget();
    QWidget* createSummaryWidget();
    
    // Filter helper
    QList<QStandardItem*> getTopFilteredOrders() const;

    // Trading data service
    TradingDataService* m_tradingDataService;

    // UI Components
    CustomOrderBook *m_tableView;
    QStandardItemModel *m_model;
    class QSortFilterProxyModel *m_proxyModel; // Added for sorting

    // Filter Components
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
    QPushButton *m_cancelOrderBtn;
    QPushButton *m_modifyOrderBtn;

    // Filter State
    QString m_instrumentFilter;
    QDateTime m_fromTime;
    QDateTime m_toTime;
    QString m_statusFilter;
    QString m_orderTypeFilter;
    QString m_exchangeFilter;
    QString m_buySellFilter;
    
    // Excel-like filter row
    bool m_filterRowVisible;
    QShortcut* m_filterShortcut;
    QShortcut* m_escShortcut;
    QList<OrderBookFilterWidget*> m_filterWidgets;
    QMap<int, QStringList> m_columnFilters;
    GenericTableProfile m_columnProfile; // Added

    // Summary data
    QLabel *m_summaryLabel;
    int m_totalOrders;
    int m_openOrders;
    int m_executedOrders;
    int m_cancelledOrders;
    double m_totalOrderValue;
};

// Excel-like filter widget for column filtering
class OrderBookFilterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OrderBookFilterWidget(int column, OrderBookWindow* orderBookWindow, QWidget* parent = nullptr);
    QStringList selectedValues() const;
    void clear();
    void updateButtonDisplay();

signals:
    void filterChanged(int column, const QStringList& selectedValues);

private slots:
    void showFilterPopup();

private:
    QStringList getUniqueValuesForColumn() const;
    
    int m_column;
    QPushButton* m_filterButton;
    OrderBookWindow* m_orderBookWindow;

public:
    QStringList m_selectedValues;
};

#endif // ORDERBOOKWINDOW_H
