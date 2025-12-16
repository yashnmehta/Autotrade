#include "api/XTSInteractiveClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

XTSInteractiveClient::XTSInteractiveClient(const QString &baseURL,
                                             const QString &apiKey,
                                             const QString &secretKey,
                                             const QString &source,
                                             QObject *parent)
    : QObject(parent)
    , m_baseURL(baseURL)
    , m_apiKey(apiKey)
    , m_secretKey(secretKey)
    , m_source(source)
    , m_httpClient(std::make_unique<NativeHTTPClient>())
{
}

XTSInteractiveClient::~XTSInteractiveClient()
{
}

void XTSInteractiveClient::login(std::function<void(bool, const QString&)> callback)
{
    std::string url = (m_baseURL + "/interactive/user/session").toStdString();
    
    QJsonObject loginData;
    loginData["appKey"] = m_apiKey;
    loginData["secretKey"] = m_secretKey;
    loginData["source"] = m_source;
    
    QJsonDocument doc(loginData);
    std::string body = doc.toJson().toStdString();
    
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    
    auto response = m_httpClient->post(url, body, headers);
    
    if (!response.success) {
        QString error = QString("Interactive login failed: %1").arg(QString::fromStdString(response.error));
        qWarning() << error;
        if (callback) callback(false, error);
        return;
    }
    
    QJsonDocument responseDoc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    QJsonObject obj = responseDoc.object();
    
    QString type = obj["type"].toString();
    if (type == "success") {
        QJsonObject result = obj["result"].toObject();
        m_token = result["token"].toString();
        m_userID = result["userID"].toString();
        
        qDebug() << "✅ Interactive API login successful. UserID:" << m_userID;
        if (callback) callback(true, "Login successful");
    } else {
        QString error = QString("Interactive login failed: %1 - %2")
                            .arg(obj["code"].toString())
                            .arg(obj["description"].toString());
        qWarning() << error;
        if (callback) callback(false, error);
    }
}

void XTSInteractiveClient::getPositions(const QString &dayOrNet,
                                         std::function<void(bool, const QVector<XTS::Position>&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, QVector<XTS::Position>(), "Not logged in");
        return;
    }

    std::string url = (m_baseURL + "/interactive/portfolio/positions?dayOrNet=" + dayOrNet).toStdString();
    
    std::map<std::string, std::string> headers;
    headers["Authorization"] = m_token.toStdString();
    headers["Content-Type"] = "application/json";
    
    auto response = m_httpClient->get(url, headers);
    
    if (!response.success) {
        if (callback) callback(false, QVector<XTS::Position>(), QString::fromStdString(response.error));
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    QJsonObject obj = doc.object();

        if (obj["type"].toString() == "success") {
            QJsonObject result = obj["result"].toObject();
            QJsonArray positionsArray = result["positionList"].toArray();
            QVector<XTS::Position> positions;

            for (const QJsonValue &val : positionsArray) {
                QJsonObject pos = val.toObject();
                XTS::Position position;
                position.exchangeSegment = pos["exchangeSegment"].toString();
                position.exchangeInstrumentID = pos["exchangeInstrumentID"].toVariant().toLongLong();
                position.productType = pos["ProductType"].toString();
                position.quantity = pos["Quantity"].toInt();
                position.buyAveragePrice = pos["BuyAveragePrice"].toDouble();
                position.sellAveragePrice = pos["SellAveragePrice"].toDouble();
                position.netQuantity = pos["NetQuantity"].toInt();
                position.realizedProfit = pos["RealizedProfit"].toDouble();
                position.unrealizedProfit = pos["UnrealizedProfit"].toDouble();
                position.mtm = pos["MTM"].toDouble();

                positions.append(position);
            }

            if (callback) callback(true, positions, "Success");
        } else {
            if (callback) callback(false, QVector<XTS::Position>(), obj["description"].toString());
        }
}

void XTSInteractiveClient::getOrders(std::function<void(bool, const QVector<XTS::Order>&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, QVector<XTS::Order>(), "Not logged in");
        return;
    }

    std::string url = (m_baseURL + "/interactive/orders").toStdString();
    
    std::map<std::string, std::string> headers;
    headers["Authorization"] = m_token.toStdString();
    headers["Content-Type"] = "application/json";
    
    auto response = m_httpClient->get(url, headers);
    
    if (!response.success) {
        if (callback) callback(false, QVector<XTS::Order>(), QString::fromStdString(response.error));
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    QJsonObject obj = doc.object();

        if (obj["type"].toString() == "success") {
            QJsonArray ordersArray = obj["result"].toArray();
            QVector<XTS::Order> orders;

            for (const QJsonValue &val : ordersArray) {
                QJsonObject ord = val.toObject();
                XTS::Order order;
                order.appOrderID = ord["AppOrderID"].toString();
                order.orderID = ord["OrderID"].toString();
                order.exchangeOrderID = ord["ExchangeOrderID"].toString();
                order.exchangeInstrumentID = ord["ExchangeInstrumentID"].toVariant().toLongLong();
                order.orderSide = ord["OrderSide"].toString();
                order.orderType = ord["OrderType"].toString();
                order.productType = ord["ProductType"].toString();
                order.orderPrice = ord["OrderPrice"].toDouble();
                order.orderQuantity = ord["OrderQuantity"].toInt();
                order.filledQuantity = ord["FilledQuantity"].toInt();
                order.orderStatus = ord["OrderStatus"].toString();

                orders.append(order);
            }

            if (callback) callback(true, orders, "Success");
        } else {
            if (callback) callback(false, QVector<XTS::Order>(), obj["description"].toString());
        }
}

void XTSInteractiveClient::getTrades(std::function<void(bool, const QVector<XTS::Trade>&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, QVector<XTS::Trade>(), "Not logged in");
        return;
    }

    std::string url = (m_baseURL + "/interactive/orders/trades").toStdString();
    
    std::map<std::string, std::string> headers;
    headers["Authorization"] = m_token.toStdString();
    headers["Content-Type"] = "application/json";
    
    auto response = m_httpClient->get(url, headers);
    
    if (!response.success) {
        if (callback) callback(false, QVector<XTS::Trade>(), QString::fromStdString(response.error));
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    QJsonObject obj = doc.object();

        if (obj["type"].toString() == "success") {
            QJsonArray tradesArray = obj["result"].toArray();
            QVector<XTS::Trade> trades;

            for (const QJsonValue &val : tradesArray) {
                QJsonObject tr = val.toObject();
                XTS::Trade trade;
                trade.tradeID = tr["TradeID"].toString();
                trade.orderID = tr["OrderID"].toString();
                trade.exchangeInstrumentID = tr["ExchangeInstrumentID"].toVariant().toLongLong();
                trade.tradeSide = tr["TradeSide"].toString();
                trade.tradePrice = tr["TradePrice"].toDouble();
                trade.tradeQuantity = tr["TradeQuantity"].toInt();
                trade.tradeTimestamp = tr["TradeTimestamp"].toString();

                trades.append(trade);
            }

            if (callback) callback(true, trades, "Success");
        } else {
            if (callback) callback(false, QVector<XTS::Trade>(), obj["description"].toString());
        }
}

void XTSInteractiveClient::placeOrder(const QJsonObject &orderParams,
                                       std::function<void(bool, const QString&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, "", "Not logged in");
        return;
    }

    std::string url = (m_baseURL + "/interactive/orders").toStdString();
    
    QJsonDocument doc(orderParams);
    std::string body = doc.toJson().toStdString();
    
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = m_token.toStdString();
    
    auto response = m_httpClient->post(url, body, headers);
    
    if (!response.success) {
        if (callback) callback(false, "", QString::fromStdString(response.error));
        return;
    }
    
    QJsonDocument responseDoc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    QJsonObject obj = responseDoc.object();
    
    if (obj["type"].toString() == "success") {
        QJsonObject result = obj["result"].toObject();
        QString orderID = result["AppOrderID"].toString();
        if (callback) callback(true, orderID, "Order placed successfully");
    } else {
        if (callback) callback(false, "", obj["description"].toString());
    }
}
