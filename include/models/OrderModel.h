#ifndef ORDERMODEL_H
#define ORDERMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QStringList>
#include <QDateTime>
#include <QColor>
#include <QFont>
#include "api/XTSTypes.h"

class OrderModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        User = 0,
        Group,
        ClientOrderNo,
        ExchangeCode,
        MemberId,
        TraderId,
        InstrumentType,
        InstrumentName,
        Code,
        Symbol,
        SerExpGroup,
        StrikePrice,
        OptionType,
        ScripName,
        OrderType,
        BuySell,
        PendingQty,
        Price,
        Spread,
        TotalQty,
        ProCli,
        Client,
        ClientName,
        Misc,
        ExchOrdNo,
        Status,
        DisclosedQty,
        Settlor,
        Validity,
        GTD,
        MFAON,
        MFQty,
        TriggerPrice,
        CPBrokerId,
        AuctionNo,
        LastModifiedTime,
        TotalExecutedQty,
        AvgPrice,
        Reason,
        UserRemarks,
        PartType,
        ProductType,
        ServerEntryTime,
        ColumnCount = 75
    };

    explicit OrderModel(QObject* parent = nullptr);
    
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setOrders(const QVector<XTS::Order>& orders);
    void setFilterRowVisible(bool visible);
    bool isFilterRowVisible() const { return m_filterRowVisible; }
    
    const QVector<XTS::Order>& getOrders() const { return m_orders; }
    const XTS::Order& getOrderAt(int row) const;

private:
    QVector<XTS::Order> m_orders;
    QStringList m_headers;
    bool m_filterRowVisible = false;
};

#endif // ORDERMODEL_H
