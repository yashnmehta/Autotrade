#ifndef XTSMARKETDATACLIENT_H
#define XTSMARKETDATACLIENT_H

#include "XTSTypes.h"
#include "NativeWebSocketClient.h"
#include "NativeHTTPClient.h"
#include <QObject>
#include <QJsonDocument>
#include <functional>
#include <memory>

class XTSMarketDataClient : public QObject
{
    Q_OBJECT

public:
    explicit    XTSMarketDataClient(const QString &baseURL,
                        const QString &apiKey,
                        const QString &secretKey,
                        const QString &source = "WEBAPI",
                        QObject *parent = nullptr);
    ~XTSMarketDataClient();
    
    // Login
    void login(std::function<void(bool, const QString&)> callback);
    
    // WebSocket connection
    void connectWebSocket(std::function<void(bool, const QString&)> callback);
    void disconnectWebSocket();

    // Market data subscription
    void subscribe(const QVector<int64_t> &instrumentIDs, int exchangeSegment, 
                   std::function<void(bool, const QString&)> callback = nullptr);
    void unsubscribe(const QVector<int64_t> &instrumentIDs,
                     std::function<void(bool, const QString&)> callback = nullptr);
    
    void setTickHandler(std::function<void(const XTS::Tick&)> handler);

    // Instrument search/fetch
    void getInstruments(int exchangeSegment, 
                        std::function<void(bool, const QVector<XTS::Instrument>&, const QString&)> callback);
    void searchInstruments(const QString &searchString, int exchangeSegment,
                            std::function<void(bool, const QVector<XTS::Instrument>&, const QString&)> callback);

    // Master contract download
    void downloadMasterContracts(const QStringList &exchangeSegments, 
                                  std::function<void(bool, const QString&, const QString&)> callback);

    // Initial snapshot
    void getQuote(int64_t exchangeInstrumentID, int exchangeSegment,
                   std::function<void(bool, const QJsonObject&, const QString&)> callback);
    void getQuote(const QVector<int64_t> &instrumentIDs, int exchangeSegment,
                   std::function<void(bool, const QVector<QJsonObject>&, const QString&)> callback);

    QString token() const { return m_token; }
    QString getToken() const { return m_token; }
    QString userID() const { return m_userID; }
    bool isLoggedIn() const { return !m_token.isEmpty(); }
    bool isConnected() const { return m_wsConnected; }

signals:
    void tickReceived(const XTS::Tick &tick);
    void connectionStatusChanged(bool connected);
    void errorOccurred(const QString &error);

private slots:
    void onWSConnected();
    void onWSDisconnected(const std::string& reason);
    void onWSError(const std::string& error);
    void onWSMessage(const std::string& message);

private:
    void processTickData(const QJsonObject &json);
    XTS::Tick parseTickFromJson(const QJsonObject &json) const;
    QJsonObject parsePipeDelimitedTickData(const QString &data) const;

    QString m_baseURL;
    QString m_apiKey;
    QString m_secretKey;
    QString m_source;
    QString m_token;
    QString m_userID;

    // Native HTTP client (698x faster than Qt)
    std::unique_ptr<NativeHTTPClient> m_httpClient;
    
    // Native WebSocket (no Qt overhead)
    std::unique_ptr<NativeWebSocketClient> m_nativeWS;
    bool m_wsConnected;

    std::function<void(const XTS::Tick&)> m_tickHandler;
    std::function<void(bool, const QString&)> m_wsConnectCallback;
};

#endif // XTSMARKETDATACLIENT_H
