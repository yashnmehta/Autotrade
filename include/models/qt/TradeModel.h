#ifndef TRADEMODEL_H
#define TRADEMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QStringList>
#include <QDateTime>
#include <QColor>
#include <QFont>
#include "api/xts/XTSTypes.h"

class TradeModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        User = 0, Group, ExchangeCode, MemberId, TraderId, InstrumentType,
        InstrumentName, Code, Symbol, SpreadSymbol, SerExpGroup,
        StrikePrice, OptionType, OrderType, BuySell, Quantity, Price,
        Time, SpreadPrice, Spread, ProCli, Client, ClientName,
        ExchOrdNo, TradeNo, Settlor, UserRemarks, NewClient,
        PartType, ProductType, OrderEntryTime, ClientOrderNo,
        OrderInitiatedFrom, OrderModifiedFrom, Misc, Strategy,
        Mapping, OMSID, GiveUpStatus, EOMSID, BookingRef,
        DealingInstruction, OrderInstruction, Lots, AliasSettlor,
        AliasPartType, NewParticipantCode, InitiatedFromUserId,
        ModifiedFromUserId, SORId, NewSettlor, MaturityDate,
        Yield, MappingOrderType, AlgoType, AlgoDescription, ScripName,
        ColumnCount
    };

    explicit TradeModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setTrades(const QVector<XTS::Trade>& trades);
    void setFilterRowVisible(bool visible);
    bool isFilterRowVisible() const { return m_filterRowVisible; }
    
    const QVector<XTS::Trade>& getTrades() const { return m_trades; }

private:
    QVector<XTS::Trade> m_trades;
    QStringList m_headers;
    bool m_filterRowVisible = false;
};

#endif // TRADEMODEL_H
