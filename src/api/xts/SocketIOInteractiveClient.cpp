/**
 * Socket.IO Interactive Client Implementation
 * 
 * Uses the official socket.io-client-cpp library for proper Socket.IO protocol.
 * Based on Python reference: Application/Services/Xts/Sockets/Trade/interactiveApi.py
 */

#include "api/xts/SocketIOInteractiveClient.h"
#include <sio_client.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

SocketIOInteractiveClient::SocketIOInteractiveClient(QObject *parent)
    : QObject(parent)
    , m_sioClient(std::make_unique<sio::client>())
{
    // Setup connection listeners
    m_sioClient->set_open_listener([this]() {
        this->onConnect();
    });

    m_sioClient->set_close_listener([this](sio::client::close_reason const& reason) {
        std::string reasonStr = "Unknown";
        switch (reason) {
            case sio::client::close_reason_normal: reasonStr = "Normal"; break;
            case sio::client::close_reason_drop: reasonStr = "Drop"; break;
        }
        this->onDisconnect(reasonStr);
    });

    m_sioClient->set_fail_listener([this]() {
        this->onError("Connection failed");
    });
}

SocketIOInteractiveClient::~SocketIOInteractiveClient()
{
    disconnect();
}

void SocketIOInteractiveClient::connect(const QString &baseURL, const QString &token, 
                                         const QString &userID, const QString &clientID)
{
    m_baseURL = baseURL;
    m_token = token;
    m_userID = userID;
    m_clientID = clientID;

    // Build connection URL like Python:
    // connection_url = URL + "/?token=" + token + "&userID=" + userID + "&apiType=INTERACTIVE"
    QString connectionUrl = baseURL + "/?token=" + token + "&userID=" + userID + "&apiType=INTERACTIVE";
    if (!clientID.isEmpty()) {
        connectionUrl += "&clientID=" + clientID;
    }

    qDebug() << "[SocketIO] Connecting to Interactive:" << connectionUrl;

    // Setup event handlers before connecting
    auto socket = m_sioClient->socket("/interactive");
    
    socket->on("joined", [this](sio::event& ev) {
        if (!ev.get_messages().empty()) {
            auto msg = ev.get_messages()[0];
            if (msg->get_flag() == sio::message::flag_string) {
                this->onJoined(msg->get_string());
            }
        }
    });

    socket->on("order", [this](sio::event& ev) {
        if (!ev.get_messages().empty()) {
            auto msg = ev.get_messages()[0];
            if (msg->get_flag() == sio::message::flag_string) {
                this->onOrder(msg->get_string());
            }
        }
    });

    socket->on("trade", [this](sio::event& ev) {
        if (!ev.get_messages().empty()) {
            auto msg = ev.get_messages()[0];
            if (msg->get_flag() == sio::message::flag_string) {
                this->onTrade(msg->get_string());
            }
        }
    });

    socket->on("position", [this](sio::event& ev) {
        if (!ev.get_messages().empty()) {
            auto msg = ev.get_messages()[0];
            if (msg->get_flag() == sio::message::flag_string) {
                this->onPosition(msg->get_string());
            }
        }
    });

    // Connect with WebSocket transport (like Python: transports='websocket')
    std::map<std::string, std::string> query;
    query["token"] = token.toStdString();
    query["userID"] = userID.toStdString();
    query["apiType"] = "INTERACTIVE";
    if (!clientID.isEmpty()) {
        query["clientID"] = clientID.toStdString();
    }

    m_sioClient->connect(baseURL.toStdString(), query, 
                         {{"path", "/interactive/socket.io"}});
}

void SocketIOInteractiveClient::disconnect()
{
    if (m_sioClient && m_connected) {
        m_sioClient->close();
        m_connected = false;
    }
}

void SocketIOInteractiveClient::onConnect()
{
    m_connected = true;
    qDebug() << "✅ [SocketIO] Interactive socket connected!";
    emit connected();
}

void SocketIOInteractiveClient::onDisconnect(const std::string &reason)
{
    m_connected = false;
    qDebug() << "❌ [SocketIO] Interactive socket disconnected:" << QString::fromStdString(reason);
    emit disconnected();
}

void SocketIOInteractiveClient::onError(const std::string &error)
{
    qWarning() << "⚠️ [SocketIO] Interactive socket error:" << QString::fromStdString(error);
    emit errorOccurred(QString::fromStdString(error));
}

void SocketIOInteractiveClient::onJoined(const std::string &data)
{
    qDebug() << "✅ [SocketIO] Interactive socket joined:" << QString::fromStdString(data);
    emit joined(QString::fromStdString(data));
}

void SocketIOInteractiveClient::onOrder(const std::string &data)
{
    qDebug() << "[SocketIO] Order event received";
    
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(data));
    if (!doc.isObject()) return;
    
    QJsonObject json = doc.object();
    XTS::Order order = parseOrderFromJson(json);
    
    qDebug() << "[SocketIO] Order:" << order.appOrderID << order.orderStatus;
    emit orderEvent(order);
}

void SocketIOInteractiveClient::onTrade(const std::string &data)
{
    qDebug() << "[SocketIO] Trade event received";
    
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(data));
    if (!doc.isObject()) return;
    
    QJsonObject json = doc.object();
    XTS::Trade trade = parseTradeFromJson(json);
    
    qDebug() << "[SocketIO] Trade:" << trade.executionID;
    emit tradeEvent(trade);
}

void SocketIOInteractiveClient::onPosition(const std::string &data)
{
    qDebug() << "[SocketIO] Position event received";
    
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(data));
    if (!doc.isObject()) return;
    
    QJsonObject json = doc.object();
    XTS::Position position = parsePositionFromJson(json);
    
    emit positionEvent(position);
}

XTS::Order SocketIOInteractiveClient::parseOrderFromJson(const QJsonObject &ord) const
{
    XTS::Order order;
    order.appOrderID = ord["AppOrderID"].toVariant().toLongLong();
    order.exchangeOrderID = ord["ExchangeOrderID"].toString();
    order.clientID = ord["ClientID"].toString();
    order.loginID = ord["LoginID"].toString();
    order.exchangeSegment = ord["ExchangeSegment"].toString();
    order.exchangeInstrumentID = ord["ExchangeInstrumentID"].toVariant().toLongLong();
    order.tradingSymbol = ord["TradingSymbol"].toString();
    order.orderSide = ord["OrderSide"].toString();
    order.orderType = ord["OrderType"].toString();
    order.orderPrice = ord["OrderPrice"].toVariant().toDouble();
    order.orderStopPrice = ord["OrderStopPrice"].toVariant().toDouble();
    order.orderQuantity = ord["OrderQuantity"].toVariant().toInt();
    order.cumulativeQuantity = ord["CumulativeQuantity"].toVariant().toInt();
    order.leavesQuantity = ord["LeavesQuantity"].toVariant().toInt();
    order.orderStatus = ord["OrderStatus"].toString();
    order.orderAverageTradedPrice = ord["OrderAverageTradedPrice"].toVariant().toDouble();
    order.productType = ord["ProductType"].toString();
    order.timeInForce = ord["TimeInForce"].toString();
    order.orderGeneratedDateTime = ord["OrderGeneratedDateTime"].toString();
    order.exchangeTransactTime = ord["ExchangeTransactTime"].toString();
    order.lastUpdateDateTime = ord["LastUpdateDateTime"].toString();
    order.orderUniqueIdentifier = ord["OrderUniqueIdentifier"].toString();
    order.orderReferenceID = ord["OrderReferenceID"].toString();
    order.cancelRejectReason = ord["CancelRejectReason"].toString();
    order.orderCategoryType = ord["OrderCategoryType"].toString();
    order.orderLegStatus = ord["OrderLegStatus"].toString();
    order.orderDisclosedQuantity = ord["OrderDisclosedQuantity"].toVariant().toInt();
    order.orderExpiryDate = ord["OrderExpiryDate"].toString();
    return order;
}

XTS::Trade SocketIOInteractiveClient::parseTradeFromJson(const QJsonObject &tr) const
{
    XTS::Trade trade;
    trade.executionID = tr["ExecutionID"].toString();
    trade.appOrderID = tr["AppOrderID"].toVariant().toLongLong();
    trade.exchangeOrderID = tr["ExchangeOrderID"].toString();
    trade.clientID = tr["ClientID"].toString();
    trade.loginID = tr["LoginID"].toString();
    trade.exchangeSegment = tr["ExchangeSegment"].toString();
    trade.exchangeInstrumentID = tr["ExchangeInstrumentID"].toVariant().toLongLong();
    trade.tradingSymbol = tr["TradingSymbol"].toString();
    trade.orderSide = tr["OrderSide"].toString();
    trade.orderType = tr["OrderType"].toString();
    trade.lastTradedPrice = tr["LastTradedPrice"].toVariant().toDouble();
    trade.lastTradedQuantity = tr["LastTradedQuantity"].toVariant().toInt();
    trade.lastExecutionTransactTime = tr["LastExecutionTransactTime"].toString();
    trade.orderGeneratedDateTime = tr["OrderGeneratedDateTime"].toString();
    trade.exchangeTransactTime = tr["ExchangeTransactTime"].toString();
    trade.orderAverageTradedPrice = tr["OrderAverageTradedPrice"].toVariant().toDouble();
    trade.cumulativeQuantity = tr["CumulativeQuantity"].toVariant().toInt();
    trade.leavesQuantity = tr["LeavesQuantity"].toVariant().toInt();
    trade.orderStatus = tr["OrderStatus"].toString();
    trade.productType = tr["ProductType"].toString();
    trade.orderUniqueIdentifier = tr["OrderUniqueIdentifier"].toString();
    trade.orderPrice = tr["OrderPrice"].toVariant().toDouble();
    trade.orderQuantity = tr["OrderQuantity"].toVariant().toInt();
    return trade;
}

XTS::Position SocketIOInteractiveClient::parsePositionFromJson(const QJsonObject &pos) const
{
    XTS::Position position;
    position.accountID = pos["AccountID"].toString();
    position.actualBuyAmount = pos["ActualBuyAmount"].toVariant().toDouble();
    position.actualBuyAveragePrice = pos["ActualBuyAveragePrice"].toVariant().toDouble();
    position.actualSellAmount = pos["ActualSellAmount"].toVariant().toDouble();
    position.actualSellAveragePrice = pos["ActualSellAveragePrice"].toVariant().toDouble();
    position.bep = pos["BEP"].toVariant().toDouble();
    position.buyAmount = pos["BuyAmount"].toVariant().toDouble();
    position.buyAveragePrice = pos["BuyAveragePrice"].toVariant().toDouble();
    position.exchangeInstrumentID = pos["ExchangeInstrumentId"].toVariant().toLongLong();
    position.exchangeSegment = pos["ExchangeSegment"].toString();
    position.loginID = pos["LoginID"].toString();
    position.mtm = pos["MTM"].toVariant().toDouble();
    position.marketLot = pos["Marketlot"].toVariant().toInt();
    position.multiplier = pos["Multiplier"].toVariant().toDouble();
    position.netAmount = pos["NetAmount"].toVariant().toDouble();
    position.openBuyQuantity = pos["OpenBuyQuantity"].toVariant().toInt();
    position.openSellQuantity = pos["OpenSellQuantity"].toVariant().toInt();
    position.productType = pos["ProductType"].toString();
    position.quantity = pos["Quantity"].toVariant().toInt();
    position.realizedMTM = pos["RealizedMTM"].toVariant().toDouble();
    position.sellAmount = pos["SellAmount"].toVariant().toDouble();
    position.sellAveragePrice = pos["SellAveragePrice"].toVariant().toDouble();
    position.tradingSymbol = pos["TradingSymbol"].toString();
    position.unrealizedMTM = pos["UnrealizedMTM"].toVariant().toDouble();
    return position;
}
