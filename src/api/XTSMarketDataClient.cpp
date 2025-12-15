#include "api/XTSMarketDataClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDebug>

XTSMarketDataClient::XTSMarketDataClient(const QString &baseURL,
                                           const QString &apiKey,
                                           const QString &secretKey,
                                           QObject *parent)
    : QObject(parent)
    , m_baseURL(baseURL)
    , m_apiKey(apiKey)
    , m_secretKey(secretKey)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_webSocket(new QWebSocket())
    , m_wsConnected(false)
    , m_tickHandler(nullptr)
    , m_wsConnectCallback(nullptr)
{
    // Connect WebSocket signals
    connect(m_webSocket, &QWebSocket::connected, this, &XTSMarketDataClient::onWSConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &XTSMarketDataClient::onWSDisconnected);
    connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &XTSMarketDataClient::onWSError);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &XTSMarketDataClient::onWSTextMessageReceived);
    connect(m_webSocket, &QWebSocket::binaryMessageReceived, this, &XTSMarketDataClient::onWSBinaryMessageReceived);
}

XTSMarketDataClient::~XTSMarketDataClient()
{
    disconnectWebSocket();
    delete m_webSocket;
}

void XTSMarketDataClient::login(std::function<void(bool, const QString&)> callback)
{
    QUrl url(m_baseURL + "/marketdata/auth/login");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject loginData;
    loginData["appKey"] = m_apiKey;
    loginData["secretKey"] = m_secretKey;
    loginData["source"] = "WEBAPI";

    QJsonDocument doc(loginData);
    QByteArray data = doc.toJson();

    QNetworkReply *reply = m_networkManager->post(request, data);

    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QString error = QString("Login failed: %1").arg(reply->errorString());
            qWarning() << error;
            if (callback) callback(false, error);
            return;
        }

        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        QJsonObject obj = doc.object();

        QString type = obj["type"].toString();
        if (type == "success") {
            QJsonObject result = obj["result"].toObject();
            m_token = result["token"].toString();
            m_userID = result["userID"].toString();

            qDebug() << "✅ Market Data login successful. Token:" << m_token.left(20) + "...";
            if (callback) callback(true, "Login successful");
        } else {
            QString error = QString("Login failed: %1 - %2")
                                .arg(obj["code"].toString())
                                .arg(obj["description"].toString());
            qWarning() << error;
            if (callback) callback(false, error);
        }
    });
}

void XTSMarketDataClient::connectWebSocket(std::function<void(bool, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        qWarning() << "Cannot connect WebSocket: Not logged in";
        if (callback) callback(false, "Not logged in");
        return;
    }

    m_wsConnectCallback = callback;

    // XTS WebSocket URL format: wss://host/marketdata/feed?token=xxx
    QString wsUrl = m_baseURL.replace("https://", "wss://").replace("http://", "ws://");
    wsUrl += "/marketdata/feed?token=" + m_token;

    qDebug() << "Connecting to WebSocket:" << wsUrl;
    m_webSocket->open(QUrl(wsUrl));
}

void XTSMarketDataClient::disconnectWebSocket()
{
    if (m_webSocket && m_wsConnected) {
        m_webSocket->close();
    }
}

void XTSMarketDataClient::subscribe(const QVector<int64_t> &instrumentIDs,
                                     std::function<void(bool, const QString&)> callback)
{
    if (!m_wsConnected) {
        if (callback) callback(false, "WebSocket not connected");
        return;
    }

    QJsonObject subscribeMsg;
    subscribeMsg["type"] = "subscribe";
    
    QJsonArray instruments;
    for (int64_t id : instrumentIDs) {
        QJsonObject inst;
        inst["exchangeInstrumentID"] = (qint64)id;
        instruments.append(inst);
    }
    subscribeMsg["instruments"] = instruments;

    QJsonDocument doc(subscribeMsg);
    m_webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));

    qDebug() << "Subscription request sent for" << instrumentIDs.size() << "instruments";
    if (callback) callback(true, "Subscription request sent");
}

void XTSMarketDataClient::unsubscribe(const QVector<int64_t> &instrumentIDs,
                                       std::function<void(bool, const QString&)> callback)
{
    if (!m_wsConnected) {
        if (callback) callback(false, "WebSocket not connected");
        return;
    }

    QJsonObject unsubscribeMsg;
    unsubscribeMsg["type"] = "unsubscribe";
    
    QJsonArray instruments;
    for (int64_t id : instrumentIDs) {
        QJsonObject inst;
        inst["exchangeInstrumentID"] = (qint64)id;
        instruments.append(inst);
    }
    unsubscribeMsg["instruments"] = instruments;

    QJsonDocument doc(unsubscribeMsg);
    m_webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));

    if (callback) callback(true, "Unsubscription request sent");
}

void XTSMarketDataClient::setTickHandler(std::function<void(const XTS::Tick&)> handler)
{
    m_tickHandler = handler;
}

void XTSMarketDataClient::getInstruments(int exchangeSegment,
                                          std::function<void(bool, const QVector<XTS::Instrument>&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, QVector<XTS::Instrument>(), "Not logged in");
        return;
    }

    QUrl url(m_baseURL + "/marketdata/instruments/master");
    QUrlQuery query;
    query.addQueryItem("exchangeSegment", QString::number(exchangeSegment));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", m_token.toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (callback) callback(false, QVector<XTS::Instrument>(), reply->errorString());
            return;
        }

        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        QJsonObject obj = doc.object();

        if (obj["type"].toString() == "success") {
            QJsonArray instruments = obj["result"].toArray();
            QVector<XTS::Instrument> result;

            for (const QJsonValue &val : instruments) {
                QJsonObject inst = val.toObject();
                XTS::Instrument instrument;
                instrument.exchangeSegment = inst["exchangeSegment"].toInt();
                instrument.exchangeInstrumentID = inst["exchangeInstrumentID"].toVariant().toLongLong();
                instrument.instrumentName = inst["name"].toString();
                instrument.series = inst["series"].toString();
                // Add more fields as needed
                result.append(instrument);
            }

            if (callback) callback(true, result, "Success");
        } else {
            if (callback) callback(false, QVector<XTS::Instrument>(), obj["description"].toString());
        }
    });
}

void XTSMarketDataClient::searchInstruments(const QString &searchString, int exchangeSegment,
                                             std::function<void(bool, const QVector<XTS::Instrument>&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, QVector<XTS::Instrument>(), "Not logged in");
        return;
    }

    QUrl url(m_baseURL + "/marketdata/search/instrumentsbystring");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", m_token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject searchData;
    searchData["searchString"] = searchString;
    searchData["exchangeSegment"] = exchangeSegment;

    QJsonDocument doc(searchData);
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());

    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (callback) callback(false, QVector<XTS::Instrument>(), reply->errorString());
            return;
        }

        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        QJsonObject obj = doc.object();

        if (obj["type"].toString() == "success") {
            QJsonArray instruments = obj["result"].toArray();
            QVector<XTS::Instrument> result;

            for (const QJsonValue &val : instruments) {
                QJsonObject inst = val.toObject();
                XTS::Instrument instrument;
                instrument.exchangeSegment = inst["exchangeSegment"].toInt();
                instrument.exchangeInstrumentID = inst["exchangeInstrumentID"].toVariant().toLongLong();
                instrument.instrumentName = inst["name"].toString();
                // Add more fields
                result.append(instrument);
            }

            if (callback) callback(true, result, "Success");
        } else {
            if (callback) callback(false, QVector<XTS::Instrument>(), obj["description"].toString());
        }
    });
}

void XTSMarketDataClient::onWSConnected()
{
    m_wsConnected = true;
    qDebug() << "✅ WebSocket connected";
    emit connectionStatusChanged(true);
    
    if (m_wsConnectCallback) {
        m_wsConnectCallback(true, "Connected");
        m_wsConnectCallback = nullptr;
    }
}

void XTSMarketDataClient::onWSDisconnected()
{
    m_wsConnected = false;
    qDebug() << "❌ WebSocket disconnected";
    emit connectionStatusChanged(false);
}

void XTSMarketDataClient::onWSError(QAbstractSocket::SocketError error)
{
    QString errorStr = m_webSocket->errorString();
    qWarning() << "WebSocket error:" << errorStr;
    emit errorOccurred(errorStr);
    
    if (m_wsConnectCallback) {
        m_wsConnectCallback(false, errorStr);
        m_wsConnectCallback = nullptr;
    }
}

void XTSMarketDataClient::onWSTextMessageReceived(const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isNull() && doc.isObject()) {
        processTickData(doc.object());
    }
}

void XTSMarketDataClient::onWSBinaryMessageReceived(const QByteArray &message)
{
    // XTS typically sends JSON, but handle binary if needed
    QJsonDocument doc = QJsonDocument::fromJson(message);
    if (!doc.isNull() && doc.isObject()) {
        processTickData(doc.object());
    }
}

void XTSMarketDataClient::processTickData(const QJsonObject &json)
{
    // Parse tick data from XTS format
    XTS::Tick tick;
    tick.exchangeSegment = json["ExchangeSegment"].toInt();
    tick.exchangeInstrumentID = json["ExchangeInstrumentID"].toVariant().toLongLong();
    tick.lastTradedPrice = json["LastTradedPrice"].toDouble();
    tick.lastTradedQuantity = json["LastTradedQuantity"].toInt();
    tick.totalBuyQuantity = json["TotalBuyQuantity"].toInt();
    tick.totalSellQuantity = json["TotalSellQuantity"].toInt();
    tick.volume = json["Volume"].toVariant().toLongLong();
    tick.open = json["Open"].toDouble();
    tick.high = json["High"].toDouble();
    tick.low = json["Low"].toDouble();
    tick.close = json["Close"].toDouble();
    tick.bidPrice = json["BidPrice"].toDouble();
    tick.bidQuantity = json["BidQuantity"].toInt();
    tick.askPrice = json["AskPrice"].toDouble();
    tick.askQuantity = json["AskQuantity"].toInt();

    // Call handler if set
    if (m_tickHandler) {
        m_tickHandler(tick);
    }

    emit tickReceived(tick);
}

QNetworkRequest XTSMarketDataClient::createRequest(const QString &endpoint) const
{
    QNetworkRequest request(QUrl(m_baseURL + endpoint));
    request.setRawHeader("Authorization", m_token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return request;
}
