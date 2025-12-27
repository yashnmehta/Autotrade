#ifndef TRADEBOOKWINDOW_H
#define TRADEBOOKWINDOW_H

#include <QWidget>
#include <QStandardItemModel>
#include <QDateTime>
#include <QShortcut>
#include <QLabel>
#include "models/GenericTableProfile.h"
#include "api/XTSTypes.h"

class CustomTradeBook;
class QComboBox;
class QDateTimeEdit;
class QPushButton;
class QCheckBox;
class FilterRowWidget;
class TradeBookFilterWidget;
class TradingDataService;

class TradeBookWindow : public QWidget
{
    Q_OBJECT
    
    friend class TradeBookFilterWidget;

public:
    explicit TradeBookWindow(class TradingDataService* tradingDataService, QWidget *parent = nullptr);
    ~TradeBookWindow();

    // Filter methods
    void setInstrumentFilter(const QString &instrument);
    void setTimeFilter(const QDateTime &fromTime, const QDateTime &toTime);
    void setBuySellFilter(const QString &buySell);  // "All", "Buy", "Sell"
    void setOrderTypeFilter(const QString &orderType);  // "All", "Day", "IOC", etc.

public slots:
    // applyFilters, clearFilters, exportToCSV, toggleFilterRow, onColumnFilterChanged remain public slots
    void applyFilters();
    void clearFilters();
    void exportToCSV();
    void toggleFilterRow();
    void onColumnFilterChanged(int column, const QStringList& selectedValues);
    void showColumnProfileDialog(); // Added

signals:
    void tradeSelected(const QString &tradeId);

private slots: // refreshTrades moved here, onTradesUpdated added
    void refreshTrades();
    void onTradesUpdated(const QVector<XTS::Trade>& trades);

private:
    void setupUI();
    void setupFilterBar();
    void setupTable();
    void loadInitialProfile();
    void setupConnections();
    void applyFilterToModel();
    void updateSummary();
    
    // UI Helper methods
    QWidget* createFilterWidget();
    QWidget* createSummaryWidget();
    
    // Filter helper
    QList<QStandardItem*> getTopFilteredTrades() const;

    // Trading data service
    TradingDataService* m_tradingDataService;

    // UI Components
    CustomTradeBook *m_tableView;
    QStandardItemModel *m_model;
    QStandardItemModel *m_filteredModel;
    class QSortFilterProxyModel *m_proxyModel; // Added for sorting

    // Filter Components
    QComboBox *m_instrumentTypeCombo;
    QComboBox *m_buySellCombo;
    QComboBox *m_orderTypeCombo;
    QComboBox *m_exchangeCombo;
    QDateTimeEdit *m_fromTimeEdit;
    QDateTimeEdit *m_toTimeEdit;
    QPushButton *m_applyFilterBtn;
    QPushButton *m_clearFilterBtn;
    QPushButton *m_exportBtn;
    QCheckBox *m_showSummaryCheck;

    // Filter State
    QString m_instrumentFilter;
    QDateTime m_fromTime;
    QDateTime m_toTime;
    QString m_buySellFilter;
    QString m_orderTypeFilter;
    QString m_exchangeFilter;
    
    // Excel-like filter row
    bool m_filterRowVisible;
    QShortcut* m_filterShortcut;
    QShortcut* m_escShortcut;
    QList<TradeBookFilterWidget*> m_filterWidgets;
    QMap<int, QStringList> m_columnFilters;
    GenericTableProfile m_columnProfile; // Added

    // Summary data
    QLabel *m_summaryLabel;
    double m_totalBuyQty;
    double m_totalSellQty;
    double m_totalBuyValue;
    double m_totalSellValue;
    int m_tradeCount;
};

// Excel-like filter widget for column filtering
class TradeBookFilterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TradeBookFilterWidget(int column, TradeBookWindow* tradeBookWindow, QWidget* parent = nullptr);
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
    TradeBookWindow* m_tradeBookWindow;

public:
    QStringList m_selectedValues;
};

#endif // TRADEBOOKWINDOW_H
