#ifndef XTSINTERACTIVECLIENT_H
#define XTSINTERACTIVECLIENT_H

#include "XTSTypes.h"
#include "NativeHTTPClient.h"
#include <QObject>
#include <functional>
#include <memory>

class XTSInteractiveClient : public QObject
{
    Q_OBJECT

public:
    explicit XTSInteractiveClient(const QString &baseURL,
                                   const QString &apiKey,
                                   const QString &secretKey,
                                   const QString &source = "WEBAPI",
                                   QObject *parent = nullptr);
    ~XTSInteractiveClient();

    // Authentication
    void login(std::function<void(bool, const QString&)> callback);
    QString getToken() const { return m_token; }
    QString getUserID() const { return m_userID; }
    QString getClientID() const { return m_clientID; }
    void setClientID(const QString &id) { m_clientID = id; }
    bool isLoggedIn() const { return !m_token.isEmpty(); }

    // Position queries
    void getPositions(const QString &dayOrNet,
                      std::function<void(bool, const QVector<XTS::Position>&, const QString&)> callback);

    // Order queries
    void getOrders(std::function<void(bool, const QVector<XTS::Order>&, const QString&)> callback);

    // Trade queries
    void getTrades(std::function<void(bool, const QVector<XTS::Trade>&, const QString&)> callback);

    // WebSocket connection
    void connectWebSocket(std::function<void(bool, const QString&)> callback);
    void disconnectWebSocket();

    // Order placement (for future implementation)
    void placeOrder(const QJsonObject &orderParams,
                    std::function<void(bool, const QString&, const QString&)> callback);

signals:
    void errorOccurred(const QString &error);
    void connectionStatusChanged(bool connected);
    void orderEvent(const XTS::Order &order);
    void tradeEvent(const XTS::Trade &trade);
    void positionEvent(const XTS::Position &position);

private slots:
    void onWSConnected();
    void onWSDisconnected(const std::string& reason);
    void onWSError(const std::string& error);
    void onWSMessage(const std::string& message);

private:
    void processOrderData(const QJsonObject &json);
    void processTradeData(const QJsonObject &json);
    void processPositionData(const QJsonObject &json);
    
    // Parsers
    XTS::Order parseOrderFromJson(const QJsonObject &json) const;
    XTS::Trade parseTradeFromJson(const QJsonObject &json) const;
    XTS::Position parsePositionFromJson(const QJsonObject &json) const;

    QString m_baseURL;
    QString m_apiKey;
    QString m_secretKey;
    QString m_source;
    QString m_token;
    QString m_userID;
    QString m_clientID;

    // Native HTTP client (698x faster than Qt)
    std::unique_ptr<NativeHTTPClient> m_httpClient;
    
    // Native WebSocket (no Qt overhead)
    std::unique_ptr<class NativeWebSocketClient> m_nativeWS;
    bool m_wsConnected;
    std::function<void(bool, const QString&)> m_wsConnectCallback;
};

#endif // XTSINTERACTIVECLIENT_H
