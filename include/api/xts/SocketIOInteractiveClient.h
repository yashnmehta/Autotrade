/**
 * Socket.IO Interactive Client
 * 
 * Uses the official socket.io-client-cpp library for proper Socket.IO protocol support.
 * Based on Python reference: Application/Services/Xts/Sockets/Trade/interactiveApi.py
 * 
 * This client handles:
 * - Order events (new, modified, filled, cancelled, rejected)
 * - Trade events (execution reports)
 * - Position events (real-time position updates)
 */

#ifndef SOCKETIO_INTERACTIVE_CLIENT_H
#define SOCKETIO_INTERACTIVE_CLIENT_H

#include <QObject>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>
#include "api/xts/XTSTypes.h"

// Forward declaration for Socket.IO client
namespace sio {
    class client;
}

class SocketIOInteractiveClient : public QObject {
    Q_OBJECT

public:
    explicit SocketIOInteractiveClient(QObject *parent = nullptr);
    ~SocketIOInteractiveClient();

    // Prevent copying
    SocketIOInteractiveClient(const SocketIOInteractiveClient&) = delete;
    SocketIOInteractiveClient& operator=(const SocketIOInteractiveClient&) = delete;

    // Connection management
    void connect(const QString &baseURL, const QString &token, const QString &userID, 
                 const QString &clientID = QString());
    void disconnect();
    bool isConnected() const { return m_connected; }

signals:
    // Connection status
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void joined(const QString &message);

    // Trading events
    void orderEvent(const XTS::Order &order);
    void tradeEvent(const XTS::Trade &trade);
    void positionEvent(const XTS::Position &position);

private:
    // Event handlers (called by Socket.IO)
    void onConnect();
    void onDisconnect(const std::string &reason);
    void onError(const std::string &error);
    void onJoined(const std::string &data);
    void onOrder(const std::string &data);
    void onTrade(const std::string &data);
    void onPosition(const std::string &data);

    // Parsing helpers
    XTS::Order parseOrderFromJson(const QJsonObject &json) const;
    XTS::Trade parseTradeFromJson(const QJsonObject &json) const;
    XTS::Position parsePositionFromJson(const QJsonObject &json) const;

    // State
    std::unique_ptr<sio::client> m_sioClient;
    bool m_connected{false};
    QString m_baseURL;
    QString m_token;
    QString m_userID;
    QString m_clientID;
};

#endif // SOCKETIO_INTERACTIVE_CLIENT_H
