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

void TradingDataService::onOrderEvent(const XTS::Order& order)
{
    QVector<XTS::Order> currentOrders;
    {
        QMutexLocker locker(&m_ordersMutex);
        bool found = false;
        for (int i = 0; i < m_orders.size(); ++i) {
            if (m_orders[i].appOrderID == order.appOrderID) {
                m_orders[i] = order;
                found = true;
                break;
            }
        }
        if (!found) {
            m_orders.append(order);
        }
        currentOrders = m_orders; // efficient copy-on-write
    }
    
    qDebug() << "[TradingDataService] Order event:" << order.appOrderID << order.orderStatus;
    emit ordersUpdated(currentOrders);
}

void TradingDataService::onTradeEvent(const XTS::Trade& trade)
{
    QVector<XTS::Trade> currentTrades;
    {
        QMutexLocker locker(&m_tradesMutex);
        m_trades.append(trade); // Trades are usually additive log
        currentTrades = m_trades;
    }
    
    qDebug() << "[TradingDataService] Trade event:" << trade.executionID;
    emit tradesUpdated(currentTrades);
}

void TradingDataService::onPositionEvent(const XTS::Position& position)
{
    QVector<XTS::Position> currentPositions;
    {
        QMutexLocker locker(&m_positionsMutex);
        bool found = false;
        // Identify position by exchange segment + instrument ID + product type
        for (int i = 0; i < m_positions.size(); ++i) {
            if (m_positions[i].exchangeInstrumentID == position.exchangeInstrumentID &&
                m_positions[i].productType == position.productType &&
                m_positions[i].exchangeSegment == position.exchangeSegment) {
                m_positions[i] = position;
                found = true;
                break;
            }
        }
        if (!found) {
            m_positions.append(position);
        }
        currentPositions = m_positions;
    }
    
    qDebug() << "[TradingDataService] Position event: instrumentID="
             << position.exchangeInstrumentID
             << "segment=" << position.exchangeSegment
             << "product=" << position.productType;
    emit positionsUpdated(currentPositions);
}
