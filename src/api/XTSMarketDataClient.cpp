#include "api/XTSMarketDataClient.h"
#include "services/PriceCache.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QtMath>
#include <QDebug>
#include <iostream>
#include <thread>

XTSMarketDataClient::XTSMarketDataClient(const QString &baseURL,
                                           const QString &apiKey,
                                           const QString &secretKey,
                                           QObject *parent)
    : QObject(parent)
    , m_baseURL(baseURL)
    , m_apiKey(apiKey)
    , m_secretKey(secretKey)
    , m_httpClient(std::make_unique<NativeHTTPClient>())
    , m_nativeWS(std::make_unique<NativeWebSocketClient>())
    , m_wsConnected(false)
    , m_tickHandler(nullptr)
    , m_wsConnectCallback(nullptr)
{
    std::cout << "[XTS] Using native WebSocket (698x faster than Qt)" << std::endl;
}

XTSMarketDataClient::~XTSMarketDataClient()
{
    disconnectWebSocket();
    // m_nativeWS is automatically destroyed by unique_ptr
}

void XTSMarketDataClient::login(std::function<void(bool, const QString&)> callback)
{
    // Native HTTP - no threading issues, synchronous call
    std::string url = (m_baseURL + "/auth/login").toStdString();
    
    QJsonObject loginData;
    loginData["appKey"] = m_apiKey;
    loginData["secretKey"] = m_secretKey;
    loginData["source"] = "WEBAPI";
    
    QJsonDocument doc(loginData);
    std::string body = doc.toJson().toStdString();
    
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    
    auto response = m_httpClient->post(url, body, headers);
    
    if (!response.success) {
        QString error = QString("Login failed: %1").arg(QString::fromStdString(response.error));
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
        
        qDebug() << "✅ Market Data login successful. Token:" << m_token.left(20) + "...";
        if (callback) callback(true, "Login successful");
    } else {
        QString error = QString("Login failed: %1 - %2")
                            .arg(obj["code"].toString())
                            .arg(obj["description"].toString());
        qWarning() << error;
        if (callback) callback(false, error);
    }
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
    
    // Remove any existing path (like /apimarketdata) to get clean base
    QUrl baseUrl(wsUrl);
    int port = baseUrl.port(3000);
    
    // Port 3000 typically doesn't support SSL - use plain ws://
    QString scheme = (port == 3000) ? "ws" : "wss";
    wsUrl = QString("%1://%2:%3").arg(scheme).arg(baseUrl.host()).arg(port);
    
    // Add Socket.IO path and query parameters
    wsUrl += "/apimarketdata/socket.io/?EIO=3&transport=websocket";
    wsUrl += "&token=" + m_token;
    
    if (!m_userID.isEmpty()) {
        wsUrl += "&userID=" + m_userID;
    }
    
    // Use Partial mode for efficiency (only changed fields sent)
    wsUrl += "&publishFormat=JSON&broadcastMode=Partial";
    
    qDebug() << "Connecting to Socket.IO WebSocket (native):" << wsUrl;
    
    // Setup native WebSocket callbacks
    m_nativeWS->setMessageCallback([this](const std::string& msg) {
        this->onWSMessage(msg);
    });
    
    m_nativeWS->connect(
        wsUrl.toStdString(),
        [this]() { this->onWSConnected(); },
        [this](const std::string& reason) { this->onWSDisconnected(reason); },
        [this](const std::string& error) { this->onWSError(error); }
    );
}

void XTSMarketDataClient::disconnectWebSocket()
{
    if (m_nativeWS && m_wsConnected) {
        m_nativeWS->disconnect();
        m_wsConnected = false;
    }
}

void XTSMarketDataClient::subscribe(const QVector<int64_t> &instrumentIDs, int exchangeSegment,
                                     std::function<void(bool, const QString&)> callback)
{
    // Native HTTP - synchronous, no threading issues
    if (m_token.isEmpty()) {
        if (callback) callback(false, "Not logged in");
        return;
    }

    std::string url = (m_baseURL + "/instruments/subscription").toStdString();
    
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
    
    QJsonDocument doc(reqObj);
    std::string body = doc.toJson().toStdString();
    
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = m_token.toStdString();
    
    auto response = m_httpClient->post(url, body, headers);
    int httpStatus = response.statusCode;
    
    qDebug() << "Subscription response (HTTP" << httpStatus << "):" << QString::fromStdString(response.body);
    
    if (!response.success) {
        if (callback) callback(false, QString::fromStdString(response.error));
        return;
    }
    
    QJsonDocument responseDoc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    
    if (!responseDoc.isNull() && responseDoc.isObject()) {
        QJsonObject obj = responseDoc.object();
            
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
                        PriceCache::instance().updatePrice(tick.exchangeInstrumentID, tick);
                    }
                }
                
                if (callback) callback(true, "Subscription successful");
                return;
            }
        }
    
    if (callback) callback(false, "Subscription failed");
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
    std::string jsonMsg = doc.toJson(QJsonDocument::Compact).toStdString();
    m_nativeWS->sendText(jsonMsg);

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

    std::string url = (m_baseURL + "/marketdata/instruments/master?exchangeSegment=" + QString::number(exchangeSegment)).toStdString();
    
    std::map<std::string, std::string> headers;
    headers["Authorization"] = m_token.toStdString();
    
    auto response = m_httpClient->get(url, headers);
    
    if (!response.success) {
        if (callback) callback(false, QVector<XTS::Instrument>(), QString::fromStdString(response.error));
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
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
}

void XTSMarketDataClient::searchInstruments(const QString &searchString, int exchangeSegment,
                                             std::function<void(bool, const QVector<XTS::Instrument>&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, QVector<XTS::Instrument>(), "Not logged in");
        return;
    }

    std::string url = (m_baseURL + "/marketdata/search/instrumentsbystring").toStdString();
    
    QJsonObject searchData;
    searchData["searchString"] = searchString;
    searchData["exchangeSegment"] = exchangeSegment;
    
    QJsonDocument doc(searchData);
    std::string body = doc.toJson().toStdString();
    
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = m_token.toStdString();
    
    auto response = m_httpClient->post(url, body, headers);
    
    if (!response.success) {
        if (callback) callback(false, QVector<XTS::Instrument>(), QString::fromStdString(response.error));
        return;
    }
    
    QJsonDocument responseDoc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    QJsonObject obj = responseDoc.object();

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
}

void XTSMarketDataClient::getQuote(int64_t exchangeInstrumentID, int exchangeSegment,
                                   std::function<void(bool, const QJsonObject&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, QJsonObject(), "Not logged in");
        return;
    }

    std::string url = (m_baseURL + "/instruments/quotes").toStdString();
    
    QJsonObject reqObj;
    QJsonArray instruments;
    QJsonObject inst;
    inst["exchangeSegment"] = exchangeSegment;
    inst["exchangeInstrumentID"] = (qint64)exchangeInstrumentID;
    instruments.append(inst);
    reqObj["instruments"] = instruments;
    reqObj["xtsMessageCode"] = 1502;
    reqObj["publishFormat"] = "JSON";
    
    QJsonDocument doc(reqObj);
    std::string body = doc.toJson().toStdString();
    
    qDebug() << "[XTSMarketDataClient] getQuote POST URL:" << QString::fromStdString(url);
    qDebug() << "[XTSMarketDataClient] getQuote Request Body:" << doc.toJson(QJsonDocument::Compact);
    qDebug() << "[XTSMarketDataClient] getQuote Authorization:" << m_token.left(20) + "...";
    
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = m_token.toStdString();
    
    auto response = m_httpClient->post(url, body, headers);
    
    if (!response.success) {
        if (callback) callback(false, QJsonObject(), QString::fromStdString(response.error));
        return;
    }
    
    qDebug() << "[XTSMarketDataClient] getQuote Response:" << QString::fromStdString(response.body);
    
    QJsonDocument responseDoc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    if (responseDoc.isNull() || !responseDoc.isObject()) {
        if (callback) callback(false, QJsonObject(), "Invalid JSON response");
        return;
    }
    
    QJsonObject obj = responseDoc.object();
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
}

void XTSMarketDataClient::getQuote(const QVector<int64_t> &instrumentIDs, int exchangeSegment,
                                   std::function<void(bool, const QVector<QJsonObject>&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        if (callback) callback(false, QVector<QJsonObject>(), "Not logged in");
        return;
    }

    std::string url = (m_baseURL + "/instruments/quotes").toStdString();
    
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
    reqObj["publishFormat"] = "JSON";
    
    QJsonDocument doc(reqObj);
    std::string body = doc.toJson().toStdString();
    
    qDebug() << "[XTSMarketDataClient] getQuote (multiple) POST URL:" << QString::fromStdString(url);
    qDebug() << "[XTSMarketDataClient] getQuote Request Body:" << doc.toJson(QJsonDocument::Compact);
    
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = m_token.toStdString();
    
    auto response = m_httpClient->post(url, body, headers);
    
    if (!response.success) {
        if (callback) callback(false, QVector<QJsonObject>(), QString::fromStdString(response.error));
        return;
    }
    
    qDebug() << "[XTSMarketDataClient] getQuote (multiple) Response:" << QString::fromStdString(response.body);
    
    QJsonDocument responseDoc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
    if (responseDoc.isNull() || !responseDoc.isObject()) {
        if (callback) callback(false, QVector<QJsonObject>(), "Invalid JSON response");
        return;
    }
    
    QJsonObject obj = responseDoc.object();
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
}

void XTSMarketDataClient::onWSConnected()
{
    m_wsConnected = true;
    qDebug() << "✅ Native WebSocket connected";
    emit connectionStatusChanged(true);
    
    // Socket.IO connect (40/) is already sent by NativeWebSocketClient::connect()
    // No need to send it again here to avoid duplicate connections
    
    if (m_wsConnectCallback) {
        m_wsConnectCallback(true, "Connected");
        m_wsConnectCallback = nullptr;
    }
}

void XTSMarketDataClient::onWSDisconnected(const std::string& reason)
{
    m_wsConnected = false;
    qDebug() << "❌ Native WebSocket disconnected:" << QString::fromStdString(reason);
    emit connectionStatusChanged(false);
    // Note: Native client handles reconnection internally with exponential backoff
}

void XTSMarketDataClient::onWSError(const std::string& error)
{
    QString errorStr = QString::fromStdString(error);
    qWarning() << "Native WebSocket error:" << errorStr;
    emit errorOccurred(errorStr);
    
    if (m_wsConnectCallback) {
        m_wsConnectCallback(false, errorStr);
        m_wsConnectCallback = nullptr;
    }
}

void XTSMarketDataClient::onWSMessage(const std::string& message)
{
    // Native WebSocket client already handles Engine.IO protocol (0,2,3,4 packets)
    // We only receive Socket.IO event messages here: ["event_name",{...}]
    // The message is already stripped of Engine.IO prefix (42) - just the JSON array
    
    QString qmsg = QString::fromStdString(message);
    
    // Parse Socket.IO event: ["event_name",data]
    QJsonDocument doc = QJsonDocument::fromJson(qmsg.toUtf8());
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        if (arr.size() >= 2) {
            QString eventName = arr[0].toString();
            
            // Ignore 1105-json-partial events (unnecessary blank data)
            if (eventName == "1105-json-partial") {
                return;
            }
            
            // Log Socket.IO event data (already filtered 1105-json-partial)
            qDebug() << "📨 Socket.IO event data:" << qmsg;
            
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
}

// Removed onWSBinaryMessageReceived - native client handles all message types via onWSMessage

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
    
    // Cache the tick data globally (10x faster native C++)
    PriceCache::instance().updatePrice(tick.exchangeInstrumentID, tick);
    
    // Call handler if set
    if (m_tickHandler) {
        m_tickHandler(tick);
    }

    emit tickReceived(tick);
}

void XTSMarketDataClient::downloadMasterContracts(const QStringList &exchangeSegments,
                                                    std::function<void(bool, const QString&, const QString&)> callback)
{
    if (m_token.isEmpty()) {
        callback(false, QString(), "Not authenticated - please login first");
        return;
    }

    // Run download in background thread to avoid blocking UI
    std::string url = (m_baseURL + "/instruments/master").toStdString();
    std::string token = m_token.toStdString();
    
    QJsonObject requestObj;
    QJsonArray segmentsArray;
    for (const QString &segment : exchangeSegments) {
        segmentsArray.append(segment);
    }
    requestObj["exchangeSegmentList"] = segmentsArray;
    
    QJsonDocument requestDoc(requestObj);
    std::string body = requestDoc.toJson().toStdString();
    
    std::cout << "[XTS] Starting master download in background thread..." << std::endl;
    
    std::thread([this, url, token, body, callback]() {
        std::map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json";
        headers["Authorization"] = token;
        
        auto response = m_httpClient->post(url, body, headers);
        
        if (!response.success) {
            QMetaObject::invokeMethod(this, [callback, error = response.error]() {
                callback(false, QString(), "Network error: " + QString::fromStdString(error));
            }, Qt::QueuedConnection);
            return;
        }
        
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
        
        if (doc.isNull() || !doc.isObject()) {
            QMetaObject::invokeMethod(this, [callback]() {
                callback(false, QString(), "Invalid JSON response");
            }, Qt::QueuedConnection);
            return;
        }
        
        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();
        
        if (type != "success") {
            QString error = obj["description"].toString();
            QMetaObject::invokeMethod(this, [callback, error]() {
                callback(false, QString(), "Download failed: " + error);
            }, Qt::QueuedConnection);
            return;
        }
        
        // Extract result - can be string (CSV) or object
        QString csvData;
        if (obj["result"].isString()) {
            csvData = obj["result"].toString();
        } else if (obj["result"].isObject() || obj["result"].isArray()) {
            QJsonDocument resultDoc(obj["result"].toObject());
            csvData = QString::fromUtf8(resultDoc.toJson());
        } else {
            csvData = QString::fromStdString(response.body);
        }
        
        std::cout << "[XTS] Master download complete (" << csvData.length() << " bytes)" << std::endl;
        
        // Invoke callback on main thread
        QMetaObject::invokeMethod(this, [callback, csvData]() {
            callback(true, csvData, QString());
        }, Qt::QueuedConnection);
        
    }).detach(); // Detach thread to run independently
}

// Removed attemptReconnect() - native WebSocket client handles reconnection internally with exponential backoff
