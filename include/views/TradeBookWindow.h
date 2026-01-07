#ifndef TRADEBOOKWINDOW_H
#define TRADEBOOKWINDOW_H

#include "views/BaseBookWindow.h"
#include "api/XTSTypes.h"
#include "models/TradeModel.h"

class CustomTradeBook;
class TradingDataService;
class QComboBox;
class QDateTimeEdit;
class QPushButton;
class QCheckBox;
class QLabel;

class TradeBookWindow : public BaseBookWindow {
    Q_OBJECT
public:
    explicit TradeBookWindow(TradingDataService* tradingDataService, QWidget *parent = nullptr);
    ~TradeBookWindow();

public slots:
    void refreshTrades();

private slots:
    void applyFilters();
    void clearFilters();
    void onTradesUpdated(const QVector<XTS::Trade>& trades);
    void toggleFilterRow();
    void exportToCSV();

protected:
    void setupUI() override;
    void onColumnFilterChanged(int column, const QStringList& selectedValues) override;
    void onTextFilterChanged(int column, const QString& text) override;

private:
    void setupTable();
    void setupConnections();
    void applyFilterToModel();
    void updateSummary();
    
    QWidget* createFilterWidget();
    QWidget* createSummaryWidget();
    
    TradingDataService* m_tradingDataService;
    QVector<XTS::Trade> m_allTrades;

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

    QMap<int, QStringList> m_columnFilters;

    QLabel *m_summaryLabel;
    QString m_instrumentFilter;
    QString m_exchangeFilter;
    QString m_buySellFilter;
    QString m_orderTypeFilter;
    QDateTime m_fromTime;
    QDateTime m_toTime;
};

#endif // TRADEBOOKWINDOW_H
