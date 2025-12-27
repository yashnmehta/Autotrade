#ifndef TRADINGDATASERVICE_H
#define TRADINGDATASERVICE_H

#include "api/XTSTypes.h"
#include <QObject>
#include <QVector>
#include <QMutex>

/**
 * @brief Centralized service for managing trading data (positions, orders, trades)
 * 
 * This service stores trading data fetched from the XTS API and provides
 * thread-safe access to it. It emits signals when data is updated, allowing
 * UI windows to subscribe and refresh their displays.
 * 
 * Thread Safety: All public methods are thread-safe using QMutex.
 */
class TradingDataService : public QObject
{
    Q_OBJECT

public:
    explicit TradingDataService(QObject *parent = nullptr);
    ~TradingDataService();

    // Setters (called by LoginFlowService or API callbacks)
    void setPositions(const QVector<XTS::Position>& positions);
    void setOrders(const QVector<XTS::Order>& orders);
    void setTrades(const QVector<XTS::Trade>& trades);

    // Getters (thread-safe)
    QVector<XTS::Position> getPositions() const;
    QVector<XTS::Order> getOrders() const;
    QVector<XTS::Trade> getTrades() const;

    // Clear all data
    void clearAll();

signals:
    // Emitted when data is updated
    void positionsUpdated(const QVector<XTS::Position>& positions);
    void ordersUpdated(const QVector<XTS::Order>& orders);
    void tradesUpdated(const QVector<XTS::Trade>& trades);

private:
    // Data storage
    QVector<XTS::Position> m_positions;
    QVector<XTS::Order> m_orders;
    QVector<XTS::Trade> m_trades;

    // Thread safety
    mutable QMutex m_positionsMutex;
    mutable QMutex m_ordersMutex;
    mutable QMutex m_tradesMutex;
};

#endif // TRADINGDATASERVICE_H
