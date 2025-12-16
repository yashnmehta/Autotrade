#include "api/XTSMarketDataClient.h"
#include "services/PriceCache.h"
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
    QUrl url(m_baseURL + "/auth/login");
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

    // XTS uses Socket.IO protocol with specific path: /apimarketdata/socket.io
    // Full URL: ws://host:port/apimarketdata/socket.io/?EIO=3&transport=websocket&token=xxx&userID=xxx&publishFormat=JSON&broadcastMode=Partial
    
    m_wsConnectCallback = callback;

    // Extract base URL (protocol + host + port)
    QString wsUrl = m_baseURL;
    wsUrl.replace("https://", "wss://").replace("http://", "ws://");
    
    // Remove any existing path (like /apimarketdata) to get clean base
    QUrl baseUrl(wsUrl);
    wsUrl = QString("%1://%2:%3").arg(baseUrl.scheme()).arg(baseUrl.host()).arg(baseUrl.port(3000));
    
    // Add Socket.IO path and query parameters
    wsUrl += "/apimarketdata/socket.io/?EIO=3&transport=websocket";
    wsUrl += "&token=" + m_token;
    
    if (!m_userID.isEmpty()) {
        wsUrl += "&userID=" + m_userID;
    }
    
    // Use Partial mode for efficiency (only changed fields sent)
    wsUrl += "&publishFormat=JSON&broadcastMode=Partial";
    
    qDebug() << "Connecting to Socket.IO WebSocket:" << wsUrl;
    m_webSocket->open(QUrl(wsUrl));
}

void XTSMarketDataClient::disconnectWebSocket()
{
    if (m_webSocket && m_wsConnected) {
        m_webSocket->close();
    }
}

void XTSMarketDataClient::subscribe(const QVector<int64_t> &instrumentIDs, int exchangeSegment,
                                     std::function<void(bool, const QString&)> callback)
{
    // XTS subscription is done via REST API, not WebSocket
    // POST /instruments/subscription with {"instruments":[{"exchangeSegment":2,"exchangeInstrumentID":xxx}],"xtsMessageCode":1502}
    
    if (m_token.isEmpty()) {
        if (callback) callback(false, "Not logged in");
        return;
    }

    QUrl url(m_baseURL + "/instruments/subscription");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", m_token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject reqObj;
    QJsonArray instruments;
    for (int64_t id : instrumentIDs) {
        QJsonObject inst;
        inst["exchangeSegment"] = exchangeSegment;
        inst["exchangeInstrumentID"] = (qint64)id;
        instruments.append(inst);
    }
    reqObj["instruments"] = instruments;
    reqObj["xtsMessageCode"] = 1502;  // Full market data

    QJsonDocument doc(reqObj);
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, instrumentIDs, exchangeSegment]() {
        reply->deleteLater();

        QByteArray response = reply->readAll();
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "Subscription response (HTTP" << httpStatus << "):" << response;
        
        QJsonDocument doc = QJsonDocument::fromJson(response);
        
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            
            // Handle "Already Subscribed" error (e-session-0002)
            if (httpStatus == 400 && obj["code"].toString() == "e-session-0002") {
                qDebug() << "⚠️ Instrument already subscribed - fetching snapshot via getQuote";
                
                // Fallback: Use getQuote to get initial snapshot
                for (int64_t instrumentID : instrumentIDs) {
                    getQuote(instrumentID, exchangeSegment, [this](bool success, const QJsonObject &quoteData, const QString &error) {
                        if (success) {
                            qDebug() << "✅ Got snapshot via getQuote for already-subscribed instrument";
                            processTickData(quoteData);
                        } else {
                            qWarning() << "❌ getQuote fallback failed:" << error;
                        }
                    });
                }
                
                // Still report success since instrument is subscribed (just not by us)
                if (callback) callback(true, "Already subscribed (snapshot fetched)");
                return;
            }
            
            if (obj["type"].toString() == "success") {
                qDebug() << "✅ Subscribed to" << instrumentIDs.size() << "instruments";
                
                // Parse and emit tick data from subscription response (if token not already subscribed)
                QJsonObject result = obj["result"].toObject();
                QJsonArray listQuotes = result["listQuotes"].toArray();
                
                for (const QJsonValue &val : listQuotes) {
                    QString quoteStr = val.toString();
                    QJsonDocument quoteDoc = QJsonDocument::fromJson(quoteStr.toUtf8());
                    if (!quoteDoc.isNull() && quoteDoc.isObject()) {
                        QJsonObject quoteData = quoteDoc.object();
                        // Process as tick data (contains Touchline with LTP, OHLC, etc.)
                        processTickData(quoteData);
                        
                        // Cache the price for future use
                        XTS::Tick tick = parseTickFromJson(quoteData);
                        PriceCache::instance()->updatePrice(tick.exchangeInstrumentID, tick);
                    }
                }
                
                if (callback) callback(true, "Subscription successful");
                return;
            }
        }
        
        if (callback) callback(false, "Subscription failed");
    });
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

void XTSMarketDataClient::getQuote(int64_t exchangeInstrumentID, int exchangeSegment,
                                   std::function<void(bool, const QJsonObject&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, QJsonObject(), "Not logged in");
        return;
    }

    QUrl url(m_baseURL + "/instruments/quotes");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", m_token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // XTS API format: {"instruments":[{"exchangeSegment":2,"exchangeInstrumentID":49508}],"xtsMessageCode":1502}
    QJsonObject reqObj;
    QJsonArray instruments;
    QJsonObject inst;
    inst["exchangeSegment"] = exchangeSegment;
    inst["exchangeInstrumentID"] = (qint64)exchangeInstrumentID;
    instruments.append(inst);
    reqObj["instruments"] = instruments;
    reqObj["xtsMessageCode"] = 1502;
    reqObj["publishFormat"] = "JSON";  // Required by XTS API

    QJsonDocument doc(reqObj);
    
    // Debug: Log the request
    qDebug() << "[XTSMarketDataClient] getQuote POST URL:" << url.toString();
    qDebug() << "[XTSMarketDataClient] getQuote Request Body:" << doc.toJson(QJsonDocument::Compact);
    qDebug() << "[XTSMarketDataClient] getQuote Authorization:" << m_token.left(20) + "...";
    
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());

    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (callback) callback(false, QJsonObject(), reply->errorString());
            return;
        }

        QByteArray response = reply->readAll();
        qDebug() << "[XTSMarketDataClient] getQuote Response:" << response;
        
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isNull() || !doc.isObject()) {
            if (callback) callback(false, QJsonObject(), "Invalid JSON response");
            return;
        }

        QJsonObject obj = doc.object();
        if (obj["type"].toString() == "success") {
            QJsonObject result = obj["result"].toObject();
            
            // XTS returns listQuotes as array of JSON strings
            QJsonArray listQuotes = result["listQuotes"].toArray();
            if (!listQuotes.isEmpty()) {
                QString quoteStr = listQuotes[0].toString();
                QJsonDocument quoteDoc = QJsonDocument::fromJson(quoteStr.toUtf8());
                if (!quoteDoc.isNull() && quoteDoc.isObject()) {
                    QJsonObject quoteData = quoteDoc.object();
                    if (callback) callback(true, quoteData, "Success");
                    return;
                }
            }
            
            // Fallback to result if no listQuotes
            if (callback) callback(true, result, "Success");
        } else {
            if (callback) callback(false, QJsonObject(), obj["description"].toString());
        }
    });
}

void XTSMarketDataClient::getQuote(const QVector<int64_t> &instrumentIDs, int exchangeSegment,
                                   std::function<void(bool, const QVector<QJsonObject>&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, QVector<QJsonObject>(), "Not logged in");
        return;
    }

    QUrl url(m_baseURL + "/instruments/quotes");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", m_token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Build instruments array
    QJsonObject reqObj;
    QJsonArray instruments;
    for (int64_t id : instrumentIDs) {
        QJsonObject inst;
        inst["exchangeSegment"] = exchangeSegment;
        inst["exchangeInstrumentID"] = (qint64)id;
        instruments.append(inst);
    }
    reqObj["instruments"] = instruments;
    reqObj["xtsMessageCode"] = 1502;
    reqObj["publishFormat"] = "JSON";  // Required by XTS API

    QJsonDocument doc(reqObj);
    
    qDebug() << "[XTSMarketDataClient] getQuote (multiple) POST URL:" << url.toString();
    qDebug() << "[XTSMarketDataClient] getQuote Request Body:" << doc.toJson(QJsonDocument::Compact);
    
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());

    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (callback) callback(false, QVector<QJsonObject>(), reply->errorString());
            return;
        }

        QByteArray response = reply->readAll();
        qDebug() << "[XTSMarketDataClient] getQuote (multiple) Response:" << response;
        
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isNull() || !doc.isObject()) {
            if (callback) callback(false, QVector<QJsonObject>(), "Invalid JSON response");
            return;
        }

        QJsonObject obj = doc.object();
        if (obj["type"].toString() == "success") {
            QJsonObject result = obj["result"].toObject();
            QJsonArray listQuotes = result["listQuotes"].toArray();
            
            QVector<QJsonObject> quotes;
            for (const QJsonValue &val : listQuotes) {
                QString quoteStr = val.toString();
                QJsonDocument quoteDoc = QJsonDocument::fromJson(quoteStr.toUtf8());
                if (!quoteDoc.isNull() && quoteDoc.isObject()) {
                    quotes.append(quoteDoc.object());
                }
            }
            
            if (callback) callback(true, quotes, "Success");
        } else {
            if (callback) callback(false, QVector<QJsonObject>(), obj["description"].toString());
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
    // Socket.IO messages are prefixed with packet type codes:
    // 0 = open, 2 = ping, 3 = pong, 4 = message, 40 = connect, 41 = disconnect, 42 = event
    
    if (message.startsWith("0")) {
        // Socket.IO handshake: 0{"sid":"xxx","upgrades":[],"pingInterval":25000,"pingTimeout":20000}
        qDebug() << "Socket.IO handshake received:" << message.mid(1);
        return;
    }
    
    if (message.startsWith("2")) {
        // Ping - respond with pong (3)
        m_webSocket->sendTextMessage("3");
        return;
    }
    
    if (message.startsWith("40")) {
        // Socket.IO connected to namespace
        qDebug() << "Socket.IO namespace connected";
        return;
    }
    
    if (message.startsWith("42")) {
        // Socket.IO event message: 42["event_name",{...}] or 42["event_name",data1,data2,...]
        QString jsonPart = message.mid(2);
        QJsonDocument doc = QJsonDocument::fromJson(jsonPart.toUtf8());
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            if (arr.size() >= 2) {
                QString eventName = arr[0].toString();
                
                // Ignore 1105-json-partial events (unnecessary blank data)
                if (eventName == "1105-json-partial") {
                    return;
                }
                
                // NOTE: Removed noisy debug log for 1502 events to avoid console spam.
                
                // XTS sends events like "1501-json-full", "1501-json-partial", "1502-json-full", "1502-json-partial"
                if (eventName == "1501-json-full" || eventName == "1501-json-partial" ||
                    eventName == "1502-json-full" || eventName == "1502-json-partial" ||
                    eventName == "xts-marketdata" || eventName == "joined") {
                    
                    // Check if second element is an object, string, or array
                    if (arr[1].isObject()) {
                        QJsonObject data = arr[1].toObject();
                        
                        // Skip empty objects (no useful data)
                        if (!data.isEmpty()) {
                            processTickData(data);
                        }
                    } else if (arr[1].isString()) {
                        // Partial updates come as pipe-delimited strings
                        // Format: "t:2_59175,bi:...,ai:...,ltp:60010,o:60306,h:60306,l:59966.2,c:60332.8,..."
                        QString partialData = arr[1].toString();
                        QJsonObject parsedData = parsePipeDelimitedTickData(partialData);
                        if (!parsedData.isEmpty()) {
                            processTickData(parsedData);
                        }
                    } else if (arr[1].isArray()) {
                        // Sometimes XTS sends array of tick data
                        QJsonArray tickArray = arr[1].toArray();
                        for (const QJsonValue &val : tickArray) {
                            if (val.isObject()) {
                                processTickData(val.toObject());
                            } else if (val.isString()) {
                                QString partialData = val.toString();
                                QJsonObject parsedData = parsePipeDelimitedTickData(partialData);
                                if (!parsedData.isEmpty()) {
                                    processTickData(parsedData);
                                }
                            }
                        }
                    }
                } else if (arr[1].isObject()) {
                    QJsonObject data = arr[1].toObject();
                    if (!data.isEmpty()) {
                        processTickData(data);
                    }
                }
            }
        }
        return;
    }
    
    // Fallback: try to parse as plain JSON
    qDebug() << "WebSocket message:" << message;
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

QJsonObject XTSMarketDataClient::parsePipeDelimitedTickData(const QString &data) const
{
    // Parse pipe-delimited partial update format
    // Example: "t:2_59175,bi:0|90|59991.6|2,ai:0|60|60023|1,ltp:60010,ltq:30,tb:7980,ts:7830,v:12810,o:60306,h:60306,l:59966.2,c:60332.8"
    
    QJsonObject result;
    QStringList parts = data.split(',');
    
    for (const QString &part : parts) {
        if (part.contains(':')) {
            QStringList keyValue = part.split(':');
            if (keyValue.size() == 2) {
                QString key = keyValue[0].trimmed();
                QString value = keyValue[1].trimmed();
                
                // Parse token info "t:2_59175" -> ExchangeSegment=2, ExchangeInstrumentID=59175
                if (key == "t" && value.contains('_')) {
                    QStringList tokenParts = value.split('_');
                    if (tokenParts.size() == 2) {
                        result["ExchangeSegment"] = tokenParts[0].toInt();
                        result["ExchangeInstrumentID"] = tokenParts[1].toLongLong();
                    }
                }
                // Bid info (5-level depth) "bi:0|90|59991.6|2|1|90|59991.4|2|..."
                else if (key == "bi") {
                    QStringList bidLevels = value.split('|');
                    if (bidLevels.size() >= 2) {
                        // First bid: index 0 (level), 1 (size), 2 (price), 3 (orders)
                        QJsonObject bidInfo;
                        bidInfo["Price"] = bidLevels[2].toDouble();
                        bidInfo["Size"] = bidLevels[1].toInt();
                        
                        QJsonObject touchline = result["Touchline"].toObject();
                        touchline["BidInfo"] = bidInfo;
                        result["Touchline"] = touchline;
                    }
                }
                // Ask info (5-level depth) "ai:0|60|60023|1|1|30|60023.2|1|..."
                else if (key == "ai") {
                    QStringList askLevels = value.split('|');
                    if (askLevels.size() >= 2) {
                        // First ask: index 0 (level), 1 (size), 2 (price), 3 (orders)
                        QJsonObject askInfo;
                        askInfo["Price"] = askLevels[2].toDouble();
                        askInfo["Size"] = askLevels[1].toInt();
                        
                        QJsonObject touchline = result["Touchline"].toObject();
                        touchline["AskInfo"] = askInfo;
                        result["Touchline"] = touchline;
                    }
                }
                // Last traded price
                else if (key == "ltp") {
                    QJsonObject touchline = result["Touchline"].toObject();
                    touchline["LastTradedPrice"] = value.toDouble();
                    result["Touchline"] = touchline;
                }
                // Last traded quantity
                else if (key == "ltq") {
                    QJsonObject touchline = result["Touchline"].toObject();
                    touchline["LastTradedQunatity"] = value.toInt(); // Note: XTS typo
                    result["Touchline"] = touchline;
                }
                // Total buy quantity
                else if (key == "tb") {
                    QJsonObject touchline = result["Touchline"].toObject();
                    touchline["TotalBuyQuantity"] = value.toLongLong();
                    result["Touchline"] = touchline;
                }
                // Total sell quantity
                else if (key == "ts") {
                    QJsonObject touchline = result["Touchline"].toObject();
                    touchline["TotalSellQuantity"] = value.toLongLong();
                    result["Touchline"] = touchline;
                }
                // Volume
                else if (key == "v") {
                    QJsonObject touchline = result["Touchline"].toObject();
                    touchline["TotalTradedQuantity"] = value.toLongLong();
                    result["Touchline"] = touchline;
                }
                // Average traded price
                else if (key == "ap") {
                    QJsonObject touchline = result["Touchline"].toObject();
                    touchline["AverageTradedPrice"] = value.toDouble();
                    result["Touchline"] = touchline;
                }
                // Percent change
                else if (key == "pc") {
                    QJsonObject touchline = result["Touchline"].toObject();
                    touchline["PercentChange"] = value.toDouble();
                    result["Touchline"] = touchline;
                }
                // OHLC
                else if (key == "o") {
                    QJsonObject touchline = result["Touchline"].toObject();
                    touchline["Open"] = value.toDouble();
                    result["Touchline"] = touchline;
                }
                else if (key == "h") {
                    QJsonObject touchline = result["Touchline"].toObject();
                    touchline["High"] = value.toDouble();
                    result["Touchline"] = touchline;
                }
                else if (key == "l") {
                    QJsonObject touchline = result["Touchline"].toObject();
                    touchline["Low"] = value.toDouble();
                    result["Touchline"] = touchline;
                }
                else if (key == "c") {
                    QJsonObject touchline = result["Touchline"].toObject();
                    touchline["Close"] = value.toDouble();
                    result["Touchline"] = touchline;
                }
            }
        }
    }
    
    return result;
}

XTS::Tick XTSMarketDataClient::parseTickFromJson(const QJsonObject &json) const
{
    // Parse tick data from XTS format
    XTS::Tick tick;
    tick.exchangeSegment = json["ExchangeSegment"].toInt();
    tick.exchangeInstrumentID = json["ExchangeInstrumentID"].toVariant().toLongLong();
    
    // XTS sends market data in nested Touchline object
    QJsonObject touchline = json["Touchline"].toObject();
    if (!touchline.isEmpty()) {
        tick.lastTradedPrice = touchline["LastTradedPrice"].toDouble();
        tick.lastTradedQuantity = touchline["LastTradedQunatity"].toInt();  // Note: XTS typo "Qunatity"
        tick.totalBuyQuantity = touchline["TotalBuyQuantity"].toVariant().toLongLong();
        tick.totalSellQuantity = touchline["TotalSellQuantity"].toVariant().toLongLong();
        tick.volume = touchline["TotalTradedQuantity"].toVariant().toLongLong();
        tick.open = touchline["Open"].toDouble();
        tick.high = touchline["High"].toDouble();
        tick.low = touchline["Low"].toDouble();
        tick.close = touchline["Close"].toDouble();
        
        // Bid/Ask from nested objects
        QJsonObject bidInfo = touchline["BidInfo"].toObject();
        QJsonObject askInfo = touchline["AskInfo"].toObject();
        tick.bidPrice = bidInfo["Price"].toDouble();
        tick.bidQuantity = bidInfo["Size"].toInt();
        tick.askPrice = askInfo["Price"].toDouble();
        tick.askQuantity = askInfo["Size"].toInt();
    } else {
        // Fallback: try flat structure (for backwards compatibility)
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
    }

    return tick;
}

void XTSMarketDataClient::processTickData(const QJsonObject &json)
{
    XTS::Tick tick = parseTickFromJson(json);
    
    // Cache the tick data globally
    PriceCache::instance()->updatePrice(tick.exchangeInstrumentID, tick);
    
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

void XTSMarketDataClient::downloadMasterContracts(const QStringList &exchangeSegments,
                                                    std::function<void(bool, const QString&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        callback(false, QString(), "Not authenticated - please login first");
        return;
    }

    QJsonObject requestObj;
    QJsonArray segmentsArray;
    for (const QString &segment : exchangeSegments) {
        segmentsArray.append(segment);
    }
    requestObj["exchangeSegmentList"] = segmentsArray;

    QJsonDocument requestDoc(requestObj);
    QByteArray requestData = requestDoc.toJson();

    QNetworkRequest request = createRequest("/instruments/master");
    
    QNetworkReply *reply = m_networkManager->post(request, requestData);
    
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            callback(false, QString(), "Network error: " + reply->errorString());
            return;
        }

        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);

        if (doc.isNull() || !doc.isObject()) {
            callback(false, QString(), "Invalid JSON response");
            return;
        }

        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();
        
        if (type != "success") {
            QString error = obj["description"].toString();
            callback(false, QString(), "Download failed: " + error);
            return;
        }

        // Extract result - can be string (CSV) or object
        QString csvData;
        if (obj["result"].isString()) {
            csvData = obj["result"].toString();
        } else if (obj["result"].isObject() || obj["result"].isArray()) {
            // Re-serialize to JSON string if it's an object/array
            QJsonDocument resultDoc(obj["result"].toObject());
            csvData = QString::fromUtf8(resultDoc.toJson());
        } else {
            csvData = QString::fromUtf8(responseData);
        }

        callback(true, csvData, QString());
    });
}
