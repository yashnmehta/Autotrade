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
    
    // Async operations (use signals for results)
    void login();
    void connectWebSocket();
    void disconnectWebSocket();

    // Market data subscription
    void subscribe(const QVector<int64_t> &instrumentIDs, int exchangeSegment);
    void unsubscribe(const QVector<int64_t> &instrumentIDs, int exchangeSegment);

    // Instrument search/fetch
    void getInstruments(int exchangeSegment);
    void searchInstruments(const QString &searchString, int exchangeSegment);

    // Master contract download
    void downloadMasterContracts(const QStringList &exchangeSegments);

    // Initial snapshot
    void getQuote(int64_t exchangeInstrumentID, int exchangeSegment);
    void getQuoteBatch(const QVector<int64_t> &instrumentIDs, int exchangeSegment);

    QString token() const { return m_token; }
    QString getToken() const { return m_token; }
    QString userID() const { return m_userID; }
    bool isLoggedIn() const { return !m_token.isEmpty(); }
    bool isConnected() const { return m_wsConnected; }

signals:
    // Async operation results
    void loginCompleted(bool success, const QString& error);
    void wsConnectionStatusChanged(bool connected, const QString& error);
    void subscriptionCompleted(bool success, const QString& error);
    void unsubscriptionCompleted(bool success, const QString& error);
    void instrumentsReceived(bool success, const QVector<XTS::Instrument>& instruments, const QString& error);
    void masterContractsDownloaded(bool success, const QString& filePath, const QString& error);
    void quoteReceived(bool success, const QJsonObject& quote, const QString& error);
    void quoteBatchReceived(bool success, const QVector<QJsonObject>& quotes, const QString& error);
    
    // Real-time data
    void tickReceived(const XTS::Tick &tick);
    
    // General errors
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
};

#endif // XTSMARKETDATACLIENT_H
