#ifndef POSITIONWINDOW_H
#define POSITIONWINDOW_H

#include <QWidget>
#include <QTableView>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QMap>
#include <QSet>
#include <QShortcut>

class PositionModel;
class FilterRowWidget;

struct Position {
    QString symbol;
    QString seriesExpiry;
    int buyQty;
    int sellQty;
    double netPrice;
    double markPrice;
    double mtmGainLoss;
    double mtmMargin;
    double buyValue;
    double sellValue;
    QString exchange;
    QString segment;
    QString user;
    QString client;
    QString periodicity;
    
    Position() : buyQty(0), sellQty(0), netPrice(0.0), markPrice(0.0),
                 mtmGainLoss(0.0), mtmMargin(0.0), buyValue(0.0), sellValue(0.0) {}
};

class PositionWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PositionWindow(QWidget *parent = nullptr);
    ~PositionWindow();

    void addPosition(const Position& position);
    void updatePosition(const QString& symbol, const Position& position);
    void clearPositions();

private slots:
    void onFilterChanged();
    void onColumnFilterChanged(int column, const QStringList& selectedValues);
    void onRefreshClicked();
    void onExportClicked();
    void toggleFilterRow();

private:
    void setupUI();
    void setupFilterBar();
    void setupTableView();
    void applyFilters();
    void updateSummaryRow();

public:
    QList<Position> getTopFilteredPositions() const;

private:
    void recreateFilterWidgets();

    // Filter bar widgets
    QComboBox* m_cbExchange;
    QComboBox* m_cbSegment;
    QComboBox* m_cbPeriodicity;
    QComboBox* m_cbUser;
    QComboBox* m_cbClient;
    QComboBox* m_cbSecurity;
    QPushButton* m_btnRefresh;
    QPushButton* m_btnExport;
    QWidget* m_topFilterWidget;

    // Table view and model
    QTableView* m_tableView;
    PositionModel* m_model;
    bool m_filterRowVisible;
    QShortcut* m_filterShortcut;
    QList<FilterRowWidget*> m_filterWidgets;

    // All positions (unfiltered)
    QList<Position> m_allPositions;
    
    // Current filters
    QString m_filterExchange;
    QString m_filterSegment;
    QString m_filterPeriodicity;
    QString m_filterUser;
    QString m_filterClient;
    QString m_filterSecurity;
    QMap<int, QStringList> m_columnFilters; // column -> selected values list
};

// Filter row delegate for embedding widgets in table
class FilterRowWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FilterRowWidget(int column, PositionWindow* positionWindow, QWidget* parent = nullptr);
    QStringList selectedValues() const;
    void clear();
    void updateButtonDisplay();

signals:
    void filterChanged(int column, const QStringList& selectedValues);

private slots:
    void showFilterPopup();
    void applyFilter();

private:
    QStringList getUniqueValuesForColumn() const;
    
    int m_column;
    QPushButton* m_filterButton;
    PositionWindow* m_positionWindow;

public:
    QStringList m_selectedValues; // Public so PositionWindow can restore state
};

// Custom table model for positions
class PositionModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        Symbol = 0,
        SeriesExpiry,
        BuyQty,
        SellQty,
        NetPrice,
        MarkPrice,
        MTMGainLoss,
        MTMMargin,
        BuyValue,
        SellValue,
        ColumnCount
    };

    explicit PositionModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setPositions(const QList<Position>& positions);
    void setSummary(const Position& summary);
    QList<Position> positions() const { return m_positions; }
    void setFilterRowVisible(bool visible);
    bool isFilterRowVisible() const { return m_filterRowVisible; }

private:
    QList<Position> m_positions;
    Position m_summary;
    bool m_showSummary;
    bool m_filterRowVisible;
};

#endif // POSITIONWINDOW_H
