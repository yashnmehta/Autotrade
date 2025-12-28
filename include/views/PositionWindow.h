#ifndef POSITIONWINDOW_H
#define POSITIONWINDOW_H

#include <QWidget>
#include <QShortcut>
#include "models/PositionModel.h"
#include "models/GenericTableProfile.h"
#include "core/widgets/CustomNetPosition.h"
#include "api/XTSTypes.h"

namespace XTS {
    struct Position;
}

#include "views/helpers/GenericTableFilter.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class TradingDataService;

class PositionWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PositionWindow(TradingDataService* tradingDataService, QWidget *parent = nullptr);
    ~PositionWindow();

    void addPosition(const PositionData& p);
    void updatePosition(const QString& s, const PositionData& p);
    void clearPositions();

public slots:
    void showColumnProfileDialog();

private slots:
    void onFilterChanged();
    void onColumnFilterChanged(int column, const QStringList& selectedValues);
    void onRefreshClicked();
    void onExportClicked();
    void toggleFilterRow();
    void onPositionsUpdated(const QVector<XTS::Position>& positions);
    void onSquareOffClicked();

private:
    void setupUI();
    QWidget* createFilterWidget();
    void setupTableView();
    void loadInitialProfile();
    void applyFilters();
    void updateSummaryRow();
    void recreateFilterWidgets();

    QComboBox* m_cbExchange;
    QComboBox* m_cbSegment;
    QComboBox* m_cbPeriodicity;
    QComboBox* m_cbUser;
    QComboBox* m_cbClient;
    QComboBox* m_cbSecurity;
    QPushButton* m_btnRefresh;
    QPushButton* m_btnExport;
    QWidget* m_topFilterWidget;

    CustomNetPosition* m_tableView;
    PositionModel* m_model;
    QSortFilterProxyModel* m_proxyModel;

    bool m_filterRowVisible;
    QShortcut* m_filterShortcut;
    QShortcut* m_escShortcut;
    QList<GenericTableFilter*> m_filterWidgets;
    QMap<int, QStringList> m_columnFilters;
    GenericTableProfile m_columnProfile;

    TradingDataService* m_tradingDataService;
    QList<PositionData> m_allPositions;

    QString m_filterExchange;
    QString m_filterSegment;
    QString m_filterPeriodicity;
    QString m_filterUser;
    QString m_filterClient;
    QString m_filterSecurity;
    
protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif // POSITIONWINDOW_H
