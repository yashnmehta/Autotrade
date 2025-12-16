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
    void subscribe(const QVector<int64_t> &instrumentIDs, int exchangeSegment,
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
    
    // Get snapshot quote for a single instrument (exchangeInstrumentID)
    void getQuote(int64_t exchangeInstrumentID, int exchangeSegment,
                  std::function<void(bool, const QJsonObject&, const QString&)> callback);
    
    // Get snapshot quotes for multiple instruments
    void getQuote(const QVector<int64_t> &instrumentIDs, int exchangeSegment,
                  std::function<void(bool, const QVector<QJsonObject>&, const QString&)> callback);
    
    // Download master contracts (returns CSV data)
    void downloadMasterContracts(const QStringList &exchangeSegments,
                                  std::function<void(bool, const QString&, const QString&)> callback);

signals:
    void connectionStatusChanged(bool connected);
    void errorOccurred(const QString &error);
    void tickReceived(const XTS::Tick &tick);

private:
    // Native WebSocket callbacks
    void onWSConnected();
    void onWSDisconnected(const std::string& reason);
    void onWSError(const std::string& error);
    void onWSMessage(const std::string& message);
    
    void processTickData(const QJsonObject &json);
    XTS::Tick parseTickFromJson(const QJsonObject &json) const;
    QJsonObject parsePipeDelimitedTickData(const QString &data) const;

    QString m_baseURL;
    QString m_apiKey;
    QString m_secretKey;
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
