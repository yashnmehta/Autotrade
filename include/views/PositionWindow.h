#ifndef POSITIONWINDOW_H
#define POSITIONWINDOW_H

#include "models/domain/WindowContext.h"
#include "views/BaseBookWindow.h"
#include "api/xts/XTSTypes.h"
#include "PositionModel.h"
#include <QMutex>

class CustomNetPosition;
class TradingDataService;
class QComboBox;
class QPushButton;
class QLabel;
class QTimer;

class PositionWindow : public BaseBookWindow {
    Q_OBJECT
public:
    explicit PositionWindow(TradingDataService* tradingDataService, QWidget *parent = nullptr);
    ~PositionWindow();

    void addPosition(const PositionData& p);
    void updatePosition(const QString& symbol, const PositionData& p);
    void clearPositions();

    WindowContext getSelectedContext() const;

private slots:
    void applyFilters();
    void onPositionsUpdated(const QVector<XTS::Position>& positions);
    void onRefreshClicked();
    void onExportClicked();
    void onSquareOffClicked();
    void toggleFilterRow();

protected:
    void setupUI() override;
    void onColumnFilterChanged(int column, const QStringList& selectedValues) override;
    void onTextFilterChanged(int column, const QString& text) override;

private:
    void setupTable();
    void setupConnections();
    void updateSummaryRow();
    QWidget* createFilterWidget();
    
    TradingDataService* m_tradingDataService;
    QList<PositionData> m_allPositions;

    QComboBox *m_cbExchange, *m_cbSegment, *m_cbPeriodicity, *m_cbUser, *m_cbClient;
    QPushButton *m_btnRefresh, *m_btnExport;
    QMap<int, QStringList> m_columnFilters;
    QTimer* m_priceUpdateTimer;
    
    // Thread safety for concurrent updates
    mutable QMutex m_updateMutex;
    bool m_isUpdating;

private slots:
    void updateMarketPrices();
};

#endif // POSITIONWINDOW_H
