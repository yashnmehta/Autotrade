#include "api/XTSInteractiveClient.h"
#include "api/NativeWebSocketClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDebug>
#include <iostream>

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
    , m_nativeWS(std::make_unique<NativeWebSocketClient>())
    , m_wsConnected(false)
    , m_wsConnectCallback(nullptr)
{
}

XTSInteractiveClient::~XTSInteractiveClient()
{
    disconnectWebSocket();
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
    
    // DEBUG: Print raw response
    qDebug() << "[XTS IA] Login Response Code:" << response.statusCode;
    qDebug() << "[XTS IA] Login Response Body:" << QString::fromStdString(response.body);


    if (!response.success) {
        QString error = QString("Interactive login failed. Code: %1 Error: %2 Body: %3")
                            .arg(response.statusCode)
                            .arg(QString::fromStdString(response.error))
                            .arg(QString::fromStdString(response.body));
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

void XTSInteractiveClient::modifyOrder(const XTS::ModifyOrderParams &params,
                                        std::function<void(bool, const QString&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, "", "Not logged in");
        return;
    }

    std::string url = (m_baseURL + "/interactive/orders").toStdString();
    
    // Build modification JSON payload
    QJsonObject modifyData;
    modifyData["appOrderID"] = QString::number(params.appOrderID);
    modifyData["modifiedProductType"] = params.productType;  // REQUIRED: Must send original product type
    modifyData["modifiedOrderType"] = params.orderType;
    modifyData["modifiedOrderQuantity"] = params.modifiedOrderQuantity;
    modifyData["modifiedDisclosedQuantity"] = params.modifiedDisclosedQuantity;
    modifyData["modifiedLimitPrice"] = params.modifiedLimitPrice;
    modifyData["modifiedStopPrice"] = params.modifiedStopPrice;
    modifyData["modifiedTimeInForce"] = params.modifiedTimeInForce;
    modifyData["orderUniqueIdentifier"] = params.orderUniqueIdentifier;  // Use original order's identifier
    
    // Add clientID if specified
    if (!params.clientID.isEmpty()) {
        modifyData["clientID"] = params.clientID;
    } else if (!m_clientID.isEmpty()) {
        modifyData["clientID"] = m_clientID;
    }
    
    QJsonDocument doc(modifyData);
    std::string body = doc.toJson().toStdString();
    
    qDebug() << "[XTS IA] modifyOrder request:" << QString::fromStdString(body);
    
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = m_token.toStdString();
    
    // Use PUT for modification
    auto response = m_httpClient->put(url, body, headers);
    
    qDebug() << "[XTS IA] modifyOrder response:" << response.statusCode 
             << QString::fromStdString(response.body).left(200);
    
    if (!response.success) {
        QString error = QString("Modify order failed: %1").arg(QString::fromStdString(response.error));
        qWarning() << error;
        if (callback) callback(false, "", error);
        return;
    }
    
    QJsonDocument responseDoc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    QJsonObject obj = responseDoc.object();
    
    if (obj["type"].toString() == "success") {
        QJsonObject result = obj["result"].toObject();
        QString orderID = QString::number(result["AppOrderID"].toVariant().toLongLong());
        qDebug() << "[XTS IA] Order modified successfully. AppOrderID:" << orderID;
        if (callback) callback(true, orderID, "Order modified successfully");
    } else {
        QString error = obj["description"].toString();
        qWarning() << "[XTS IA] Modify order failed:" << error;
        if (callback) callback(false, "", error);
    }
}

void XTSInteractiveClient::cancelOrder(int64_t appOrderID,
                                        std::function<void(bool, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, "Not logged in");
        return;
    }

    // XTS cancel order uses DELETE with appOrderID as query param
    QString urlStr = m_baseURL + "/interactive/orders?appOrderID=" + QString::number(appOrderID);
    if (!m_clientID.isEmpty()) {
        urlStr += "&clientID=" + m_clientID;
    }
    
    std::string url = urlStr.toStdString();
    
    qDebug() << "[XTS IA] cancelOrder URL:" << urlStr;
    
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = m_token.toStdString();
    
    // Use DELETE for cancellation
    auto response = m_httpClient->del(url, headers);
    
    qDebug() << "[XTS IA] cancelOrder response:" << response.statusCode 
             << QString::fromStdString(response.body).left(200);
    
    if (!response.success) {
        QString error = QString("Cancel order failed: %1").arg(QString::fromStdString(response.error));
        qWarning() << error;
        if (callback) callback(false, error);
        return;
    }
    
    QJsonDocument responseDoc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    QJsonObject obj = responseDoc.object();
    
    if (obj["type"].toString() == "success") {
        qDebug() << "[XTS IA] Order cancelled successfully. AppOrderID:" << appOrderID;
        if (callback) callback(true, "Order cancelled successfully");
    } else {
        QString error = obj["description"].toString();
        qWarning() << "[XTS IA] Cancel order failed:" << error;
        if (callback) callback(false, error);
    }
}

void XTSInteractiveClient::connectWebSocket(std::function<void(bool, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        qWarning() << "Cannot connect Interactive WebSocket: Not logged in";
        if (callback) callback(false, "Not logged in");
        return;
    }

    // Interactive API Socket Path: /interactive/socket.io
    // Based on Python reference: connection_url = URL + "/?token=" + token + "&userID=" + userID + "&apiType=INTERACTIVE"
    // socketio_path = '/interactive/socket.io'
    m_wsConnectCallback = callback;

    QString wsUrl = m_baseURL;
    QUrl baseUrl(wsUrl);
    int port = baseUrl.port(3000);
    QString scheme = (port == 3000) ? "ws" : "wss";
    wsUrl = QString("%1://%2:%3").arg(scheme).arg(baseUrl.host()).arg(port);
    
    // Socket.IO v3 URL format: /socket.io/?EIO=3&transport=websocket&...
    // With Interactive namespace and apiType=INTERACTIVE (like Python)
    wsUrl += "/interactive/socket.io/?EIO=3&transport=websocket";
    wsUrl += "&token=" + m_token;
    wsUrl += "&userID=" + m_userID;
    wsUrl += "&apiType=INTERACTIVE";  // Key parameter from Python implementation
    if (!m_clientID.isEmpty()) wsUrl += "&clientID=" + m_clientID;
    wsUrl += "&publishFormat=JSON";

    qDebug() << "Connecting to Interactive WebSocket (native):" << wsUrl;
    
    m_nativeWS->setMessageCallback([this](const std::string& msg) {
        this->onWSMessage(msg);
    });
    
    // Enable auto-reconnect for Interactive socket
    m_nativeWS->setAutoReconnect(true);
    
    m_nativeWS->connect(
        wsUrl.toStdString(),
        [this]() { this->onWSConnected(); },
        [this](const std::string& reason) { this->onWSDisconnected(reason); },
        [this](const std::string& error) { this->onWSError(error); }
    );
}

void XTSInteractiveClient::disconnectWebSocket()
{
    if (m_nativeWS && m_wsConnected) {
        m_nativeWS->disconnect();
        m_wsConnected = false;
    }
}

void XTSInteractiveClient::onWSConnected()
{
    m_wsConnected = true;
    qDebug() << "✅ Interactive Native WebSocket connected";
    emit connectionStatusChanged(true);
    
    // Send Socket.IO "join" event to register with the Interactive server
    // This is REQUIRED by XTS to keep the connection alive and receive order updates
    // Format: 42["join",{"userID":"<user>","publishFormat":"JSON"}]
    // We send after a small delay to ensure Socket.IO namespace is connected first
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        if (!m_wsConnected) return;
        
        QJsonObject joinData;
        joinData["userID"] = m_userID;
        joinData["publishFormat"] = "JSON";
        if (!m_clientID.isEmpty()) {
            joinData["clientID"] = m_clientID;
        }
        
        QJsonArray eventArray;
        eventArray.append("join");
        eventArray.append(joinData);
        
        QJsonDocument doc(eventArray);
        QString eventStr = "42" + QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        
        m_nativeWS->sendText(eventStr.toStdString());
        qDebug() << "📤 Sent Interactive Socket.IO join event:" << eventStr;
        
    }).detach();
    
    if (m_wsConnectCallback) {
        m_wsConnectCallback(true, "Connected");
        m_wsConnectCallback = nullptr;
    }
}

void XTSInteractiveClient::onWSDisconnected(const std::string& reason)
{
    m_wsConnected = false;
    qDebug() << "❌ Interactive Native WebSocket disconnected:" << QString::fromStdString(reason);
    emit connectionStatusChanged(false);
}

void XTSInteractiveClient::onWSError(const std::string& error)
{
    QString errorStr = QString::fromStdString(error);
    qWarning() << "Interactive Native WebSocket error:" << errorStr;
    emit errorOccurred(errorStr);
    
    if (m_wsConnectCallback) {
        m_wsConnectCallback(false, errorStr);
        m_wsConnectCallback = nullptr;
    }
}

void XTSInteractiveClient::onWSMessage(const std::string& message)
{
    // Handle Socket.IO events for interactive
    QString qmsg = QString::fromStdString(message);
    
    // Debug logging to see incoming socket messages
    qDebug() << "[XTS IA WebSocket] Received message:" << qmsg.left(200);
    
    QJsonDocument doc = QJsonDocument::fromJson(qmsg.toUtf8());
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        if (arr.size() >= 2) {
            QString eventName = arr[0].toString();
            qDebug() << "[XTS IA WebSocket] Event:" << eventName;
            
            // Data can be a JSON object or a JSON string that needs to be parsed again
            QJsonObject data;
            if (arr[1].isObject()) {
                data = arr[1].toObject();
            } else if (arr[1].isString()) {
                // XTS sometimes sends data as a JSON string inside the array
                QString dataStr = arr[1].toString();
                QJsonDocument dataDoc = QJsonDocument::fromJson(dataStr.toUtf8());
                if (dataDoc.isObject()) {
                    data = dataDoc.object();
                }
            }
            
            if (!data.isEmpty()) {
                if (eventName == "order") {
                    qDebug() << "[XTS IA WebSocket] Processing ORDER event";
                    processOrderData(data);
                } else if (eventName == "trade") {
                    qDebug() << "[XTS IA WebSocket] Processing TRADE event";
                    processTradeData(data);
                } else if (eventName == "position") {
                    qDebug() << "[XTS IA WebSocket] Processing POSITION event";
                    processPositionData(data);
                } else if (eventName == "joined") {
                    qDebug() << "[XTS IA WebSocket] Joined event received";
                } else {
                    qDebug() << "[XTS IA WebSocket] Unhandled event:" << eventName;
                }
            }
        }
    } else {
        qDebug() << "[XTS IA WebSocket] Message is not a JSON array:" << qmsg.left(100);
    }
}

void XTSInteractiveClient::processOrderData(const QJsonObject &json)
{
    XTS::Order order = parseOrderFromJson(json);
    // Thread-safe: use invokeMethod to emit from main thread
    QMetaObject::invokeMethod(this, [this, order]() {
        emit orderEvent(order);
    }, Qt::QueuedConnection);
}

void XTSInteractiveClient::processTradeData(const QJsonObject &json)
{
    XTS::Trade trade = parseTradeFromJson(json);
    // Thread-safe: use invokeMethod to emit from main thread
    QMetaObject::invokeMethod(this, [this, trade]() {
        emit tradeEvent(trade);
    }, Qt::QueuedConnection);
}

void XTSInteractiveClient::processPositionData(const QJsonObject &json)
{
    XTS::Position pos = parsePositionFromJson(json);
    // Thread-safe: use invokeMethod to emit from main thread
    QMetaObject::invokeMethod(this, [this, pos]() {
        emit positionEvent(pos);
    }, Qt::QueuedConnection);
}

XTS::Order XTSInteractiveClient::parseOrderFromJson(const QJsonObject &ord) const
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

XTS::Trade XTSInteractiveClient::parseTradeFromJson(const QJsonObject &tr) const
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

XTS::Position XTSInteractiveClient::parsePositionFromJson(const QJsonObject &pos) const
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
