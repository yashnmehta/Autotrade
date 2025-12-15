#ifndef XTSMARKETDATACLIENT_H
#define XTSMARKETDATACLIENT_H

#include "XTSTypes.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWebSocket>
#include <QJsonDocument>
#include <functional>

class XTSMarketDataClient : public QObject
{
    Q_OBJECT

public:
    explicit XTSMarketDataClient(const QString &baseURL,
                                  const QString &apiKey,
                                  const QString &secretKey,
                                  QObject *parent = nullptr);
    ~XTSMarketDataClient();

    // Authentication
    void login(std::function<void(bool, const QString&)> callback);
    QString getToken() const { return m_token; }
    QString getUserID() const { return m_userID; }
    bool isLoggedIn() const { return !m_token.isEmpty(); }

    // WebSocket connection
    void connectWebSocket(std::function<void(bool, const QString&)> callback);
    void disconnectWebSocket();
    bool isConnected() const { return m_wsConnected; }

    // Subscription management
    void subscribe(const QVector<int64_t> &instrumentIDs,
                   std::function<void(bool, const QString&)> callback);
    void unsubscribe(const QVector<int64_t> &instrumentIDs,
                     std::function<void(bool, const QString&)> callback);

    // Tick data callback
    void setTickHandler(std::function<void(const XTS::Tick&)> handler);

    // Market data queries
    void getInstruments(int exchangeSegment,
                        std::function<void(bool, const QVector<XTS::Instrument>&, const QString&)> callback);
    
    void searchInstruments(const QString &searchString, int exchangeSegment,
                           std::function<void(bool, const QVector<XTS::Instrument>&, const QString&)> callback);

signals:
    void connectionStatusChanged(bool connected);
    void errorOccurred(const QString &error);
    void tickReceived(const XTS::Tick &tick);

private slots:
    void onWSConnected();
    void onWSDisconnected();
    void onWSError(QAbstractSocket::SocketError error);
    void onWSTextMessageReceived(const QString &message);
    void onWSBinaryMessageReceived(const QByteArray &message);

private:
    void processTickData(const QJsonObject &json);
    QNetworkRequest createRequest(const QString &endpoint) const;

    QString m_baseURL;
    QString m_apiKey;
    QString m_secretKey;
    QString m_token;
    QString m_userID;

    QNetworkAccessManager *m_networkManager;
    QWebSocket *m_webSocket;
    bool m_wsConnected;

    std::function<void(const XTS::Tick&)> m_tickHandler;
    std::function<void(bool, const QString&)> m_wsConnectCallback;
};

#endif // XTSMARKETDATACLIENT_H
