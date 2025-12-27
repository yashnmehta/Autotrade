#include "services/TradingDataService.h"
#include <QMutexLocker>
#include <QDebug>

TradingDataService::TradingDataService(QObject *parent)
    : QObject(parent)
{
    qDebug() << "[TradingDataService] Service initialized";
}

TradingDataService::~TradingDataService()
{
    qDebug() << "[TradingDataService] Service destroyed";
}

void TradingDataService::setPositions(const QVector<XTS::Position>& positions)
{
    {
        QMutexLocker locker(&m_positionsMutex);
        m_positions = positions;
    }
    
    qDebug() << "[TradingDataService] Positions updated:" << positions.size() << "positions";
    emit positionsUpdated(positions);
}

void TradingDataService::setOrders(const QVector<XTS::Order>& orders)
{
    {
        QMutexLocker locker(&m_ordersMutex);
        m_orders = orders;
    }
    
    qDebug() << "[TradingDataService] Orders updated:" << orders.size() << "orders";
    emit ordersUpdated(orders);
}

void TradingDataService::setTrades(const QVector<XTS::Trade>& trades)
{
    {
        QMutexLocker locker(&m_tradesMutex);
        m_trades = trades;
    }
    
    qDebug() << "[TradingDataService] Trades updated:" << trades.size() << "trades";
    emit tradesUpdated(trades);
}

QVector<XTS::Position> TradingDataService::getPositions() const
{
    QMutexLocker locker(&m_positionsMutex);
    return m_positions;
}

QVector<XTS::Order> TradingDataService::getOrders() const
{
    QMutexLocker locker(&m_ordersMutex);
    return m_orders;
}

QVector<XTS::Trade> TradingDataService::getTrades() const
{
    QMutexLocker locker(&m_tradesMutex);
    return m_trades;
}

void TradingDataService::clearAll()
{
    {
        QMutexLocker locker(&m_positionsMutex);
        m_positions.clear();
    }
    {
        QMutexLocker locker(&m_ordersMutex);
        m_orders.clear();
    }
    {
        QMutexLocker locker(&m_tradesMutex);
        m_trades.clear();
    }
    
    qDebug() << "[TradingDataService] All data cleared";
    
    // Emit updates with empty data
    emit positionsUpdated(QVector<XTS::Position>());
    emit ordersUpdated(QVector<XTS::Order>());
    emit tradesUpdated(QVector<XTS::Trade>());
}
