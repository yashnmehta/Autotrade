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
        
        QJsonArray clientCodes = result["clientCodes"].toArray();
        if (clientCodes.size() > 0) {
            m_clientID = clientCodes[0].toString();
            qDebug() << "✅ Interactive API login successful. UserID:" << m_userID << "Default ClientID:" << m_clientID;
        } else {
            qDebug() << "✅ Interactive API login successful. UserID:" << m_userID << "(No client codes found)";
        }
        
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
    if (!m_clientID.isEmpty()) {
        url += "&clientID=" + m_clientID.toStdString();
    }
    qDebug() << "[XTS IA] getPositions calling URL:" << QString::fromStdString(url);
    
    std::map<std::string, std::string> headers;
    headers["Authorization"] = m_token.toStdString();
    headers["Content-Type"] = "application/json";
    
    auto response = m_httpClient->get(url, headers);
    
    if (!response.success && response.statusCode != 400) {
        qWarning() << "[XTS IA] getPositions failed! Status:" << response.statusCode 
                   << "Error:" << QString::fromStdString(response.error);
        if (callback) callback(false, QVector<XTS::Position>(), QString::fromStdString(response.error));
        return;
    }
    
    qDebug() << "[XTS IA] getPositions response body:" << QString::fromStdString(response.body).left(200) << "...";
    
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    QJsonObject obj = doc.object();

        if (obj["type"].toString() == "success") {
            QJsonObject result = obj["result"].toObject();
            QJsonArray positionsArray = result["positionList"].toArray();
            QVector<XTS::Position> positions;

            for (const QJsonValue &val : positionsArray) {
                QJsonObject pos = val.toObject();
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

                positions.append(position);
            }

            if (callback) callback(true, positions, "Success");
        } else {
            QString code = obj["code"].toString();
            // Handle XTS quirk: returns error when no data instead of empty list
            if (code == "e-portfolio-0005" || code == "e-property-validation-failed") {
                qDebug() << "[XTS IA] getPositions: No data available (treated as success)";
                if (callback) callback(true, QVector<XTS::Position>(), "Success (No Data)");
            } else {
                qWarning() << "[XTS IA] getPositions API error:" << code << ":" << obj["description"].toString();
                if (callback) callback(false, QVector<XTS::Position>(), obj["description"].toString());
            }
        }
}

void XTSInteractiveClient::getOrders(std::function<void(bool, const QVector<XTS::Order>&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, QVector<XTS::Order>(), "Not logged in");
        return;
    }

    std::string url = (m_baseURL + "/interactive/orders").toStdString();
    if (!m_clientID.isEmpty()) {
        url += "?clientID=" + m_clientID.toStdString();
    }
    qDebug() << "[XTS IA] getOrders calling URL:" << QString::fromStdString(url);
    
    std::map<std::string, std::string> headers;
    headers["Authorization"] = m_token.toStdString();
    headers["Content-Type"] = "application/json";
    
    auto response = m_httpClient->get(url, headers);
    
    if (!response.success && response.statusCode != 400) {
        qWarning() << "[XTS IA] getOrders failed! Status:" << response.statusCode;
        if (callback) callback(false, QVector<XTS::Order>(), QString::fromStdString(response.error));
        return;
    }
    
    qDebug() << "[XTS IA] getOrders raw body:" << QString::fromStdString(response.body).left(200) << "...";
    
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    QJsonObject obj = doc.object();

        if (obj["type"].toString() == "success") {
            QJsonArray ordersArray = obj["result"].toArray();
            QVector<XTS::Order> orders;

            for (const QJsonValue &val : ordersArray) {
                QJsonObject ord = val.toObject();
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

                orders.append(order);
            }

            if (callback) callback(true, orders, "Success");
        } else {
            QString code = obj["code"].toString();
            if (code == "e-orders-0005" || code == "e-property-validation-failed") {
                qDebug() << "[XTS IA] getOrders: No data available (treated as success)";
                if (callback) callback(true, QVector<XTS::Order>(), "Success (No Data)");
            } else {
                qWarning() << "[XTS IA] getOrders API error:" << code;
                if (callback) callback(false, QVector<XTS::Order>(), obj["description"].toString());
            }
        }
}

void XTSInteractiveClient::getTrades(std::function<void(bool, const QVector<XTS::Trade>&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, QVector<XTS::Trade>(), "Not logged in");
        return;
    }

    std::string url = (m_baseURL + "/interactive/orders/trades").toStdString();
    if (!m_clientID.isEmpty()) {
        url += "?clientID=" + m_clientID.toStdString();
    }
    qDebug() << "[XTS IA] getTrades calling URL:" << QString::fromStdString(url);
    
    std::map<std::string, std::string> headers;
    headers["Authorization"] = m_token.toStdString();
    headers["Content-Type"] = "application/json";
    
    auto response = m_httpClient->get(url, headers);
    
    if (!response.success && response.statusCode != 400) {
        qWarning() << "[XTS IA] getTrades failed! Status:" << response.statusCode 
                   << "Error:" << QString::fromStdString(response.error)
                   << "Body:" << QString::fromStdString(response.body);
        if (callback) callback(false, QVector<XTS::Trade>(), QString::fromStdString(response.error));
        return;
    }
    
    qDebug() << "[XTS IA] getTrades raw body:" << QString::fromStdString(response.body).left(200) << "...";
    
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    QJsonObject obj = doc.object();

        if (obj["type"].toString() == "success") {
            QJsonArray tradesArray = obj["result"].toArray();
            QVector<XTS::Trade> trades;

            for (const QJsonValue &val : tradesArray) {
                QJsonObject tr = val.toObject();
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

                trades.append(trade);
            }

            if (callback) callback(true, trades, "Success");
        } else {
            QString code = obj["code"].toString();
            if (code == "e-tradeBook-0005" || code == "e-property-validation-failed") {
                qDebug() << "[XTS IA] getTrades: No data available (treated as success)";
                if (callback) callback(true, QVector<XTS::Trade>(), "Success (No Data)");
            } else {
                qWarning() << "[XTS IA] getTrades API error:" << code << "Body:" << QString::fromStdString(response.body);
                if (callback) callback(false, QVector<XTS::Trade>(), obj["description"].toString());
            }
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
