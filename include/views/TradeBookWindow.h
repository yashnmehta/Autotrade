#ifndef TRADEBOOKWINDOW_H
#define TRADEBOOKWINDOW_H

#include <QWidget>
#include <QDateTime>
#include <QShortcut>
#include <QLabel>
#include <QMap>
#include <QVector>
#include "models/GenericTableProfile.h"
#include "api/XTSTypes.h"
#include "models/TradeModel.h"

class CustomTradeBook;
class QComboBox;
class QDateTimeEdit;
class QPushButton;
class QCheckBox;
class TradeBookFilterWidget;
class TradingDataService;
class QSortFilterProxyModel;

class TradeBookWindow : public QWidget
{
    Q_OBJECT
    
    friend class TradeBookFilterWidget;

public:
    explicit TradeBookWindow(TradingDataService* tradingDataService, QWidget *parent = nullptr);
    ~TradeBookWindow();

public slots:
    void refreshTrades();

private slots:
    void applyFilters();
    void clearFilters();
    void exportToCSV();
    void toggleFilterRow();
    void onColumnFilterChanged(int column, const QStringList& selectedValues);
    void showColumnProfileDialog();
    void onTradesUpdated(const QVector<XTS::Trade>& trades);

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
    QVector<XTS::Trade> m_allTrades;

    CustomTradeBook *m_tableView;
    TradeModel *m_model;
    QSortFilterProxyModel *m_proxyModel;

    QComboBox *m_instrumentTypeCombo;
    QComboBox *m_exchangeCombo;
    QComboBox *m_buySellCombo;
    QComboBox *m_orderTypeCombo;
    QDateTimeEdit *m_fromTimeEdit;
    QDateTimeEdit *m_toTimeEdit;
    QPushButton *m_applyFilterBtn;
    QPushButton *m_clearFilterBtn;
    QPushButton *m_exportBtn;
    QCheckBox *m_showSummaryCheck;

    bool m_filterRowVisible;
    QShortcut* m_filterShortcut;
    QList<TradeBookFilterWidget*> m_filterWidgets;
    QMap<int, QStringList> m_columnFilters;
    GenericTableProfile m_columnProfile;

    QLabel *m_summaryLabel;
    QDateTime m_fromTime;
    QDateTime m_toTime;
    QString m_instrumentFilter;
    QString m_buySellFilter;
    QString m_orderTypeFilter;
    QString m_exchangeFilter;

    // Summary data
    double m_totalBuyQty;
    double m_totalSellQty;
    double m_totalBuyValue;
    double m_totalSellValue;
    int m_tradeCount;
};

class TradeBookFilterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TradeBookFilterWidget(int column, TradeBookWindow* tradeBookWindow, QWidget* parent = nullptr);
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
    QStringList m_selectedValues;
};

#endif // TRADEBOOKWINDOW_H
