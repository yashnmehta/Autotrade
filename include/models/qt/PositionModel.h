#ifndef POSITIONMODEL_H
#define POSITIONMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QString>
#include <QColor>
#include <QFont>
#include "api/xts/XTSTypes.h"

struct PositionData {
    // Basic & Contract Info
    int64_t scripCode;      // Scrip Code
    QString symbol;         // Symbol
    QString seriesExpiry;   // Ser/Exp
    double strikePrice;     // Strike Price
    QString optionType;     // Option Type
    QString exchange;       // Exchange
    QString name;           // Name
    QString instrumentType; // Instrument Type
    QString instrumentName; // Instrument Name
    QString scripName;      // Scrip Name
    QString productType;    // Product Type
    QString optionFlag;     // Option Flag

    // Quantities & Positions
    int netQty;             // Net Qty
    int buyQty;             // Buy Qty
    int sellQty;            // Sell Qty
    int totalQuantity;      // Total Quantity
    int unsettledQty;       // Unsettled Quantity
    
    // Lots & Weights (Placeholders)
    double buyLot;
    double buyWeight;
    double sellLot;
    double sellWeight;
    double netLot;
    double netWeight;
    double totalLot;
    double totalWeight;

    // Prices
    double marketPrice;     // Market Price
    double netPrice;        // Net Price (Avg Price)
    double buyAvg;          // Buy Avg.
    double sellAvg;         // Sell Avg.
    double cfAvgPrice;      // CF Avg Price

    // Values & Profits
    double mtm;             // MTM G/L
    double mtmvPos;         // MTMV Pos
    double totalValue;      // Total Value
    double buyVal;          // Buy Val
    double sellVal;         // Sell Val
    double netVal;          // Net Val
    double brokerage;       // Brokerage
    double netMtm;          // Net MTM
    double netValPostExp;   // Net Value Post Expenses
    double actualMtm;       // Actual MTM P&L
    
    // Risk & Other metadata
    QString user;           // User
    QString client;         // Client
    QString clientGroup;    // Client Group
    double dprRange;        // DPR Range
    QString maturityDate;   // Maturity Date
    double yield;           // Yield
    double varPercent;      // VAR %
    double varAmount;       // VAR Amount
    QString smCategory;     // SM Category
    
    PositionData() : scripCode(0), strikePrice(0.0), netQty(0), buyQty(0), sellQty(0),
                 totalQuantity(0), unsettledQty(0), buyLot(0.0), buyWeight(0.0),
                 sellLot(0.0), sellWeight(0.0), netLot(0.0), netWeight(0.0),
                 totalLot(0.0), totalWeight(0.0), marketPrice(0.0), netPrice(0.0),
                 buyAvg(0.0), sellAvg(0.0), cfAvgPrice(0.0), mtm(0.0), mtmvPos(0.0),
                 totalValue(0.0), buyVal(0.0), sellVal(0.0), netVal(0.0),
                 brokerage(0.0), netMtm(0.0), netValPostExp(0.0), actualMtm(0.0),
                 dprRange(0.0), yield(0.0), varPercent(0.0), varAmount(0.0) {}
};

class PositionModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ScripCode = 0, Symbol, SeriesExpiry, StrikePrice, OptionType, NetQty,
        MarketPrice, MTMGL, NetPrice, MTMVPos, TotalValue, BuyVal, SellVal,
        Exchange, User, Client, Name, InstrumentType, InstrumentName,
        ScripName, BuyQty, BuyLot, BuyWeight, BuyAvg, SellQty, SellLot,
        SellWeight, SellAvg, NetLot, NetWeight, NetVal, ProductType,
        ClientGroup, DPRRange, MaturityDate, Yield, TotalQuantity,
        TotalLot, TotalWeight, Brokerage, NetMTM, NetValuePostExp,
        OptionFlag, VarPercent, VarAmount, SMCategory, CfAvgPrice,
        ActualMTM, UnsettledQty, ColumnCount
    };

    explicit PositionModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setPositions(const QList<PositionData>& positions);
    void setSummary(const PositionData& summary);
    QList<PositionData> positions() const { return m_positions; }
    
    Q_INVOKABLE void setFilterRowVisible(bool visible);
    bool isFilterRowVisible() const { return m_filterRowVisible; }

private:
    QList<PositionData> m_positions;
    PositionData m_summary;
    bool m_showSummary;
    bool m_filterRowVisible;
};

#endif // POSITIONMODEL_H
