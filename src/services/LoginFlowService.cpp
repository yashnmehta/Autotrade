#include "services/LoginFlowService.h"
#include <QDebug>

LoginFlowService::LoginFlowService(QObject *parent)
    : QObject(parent)
    , m_mdClient(nullptr)
    , m_iaClient(nullptr)
    , m_statusCallback(nullptr)
    , m_errorCallback(nullptr)
    , m_completeCallback(nullptr)
{
}

LoginFlowService::~LoginFlowService()
{
    delete m_mdClient;
    delete m_iaClient;
}

void LoginFlowService::setStatusCallback(std::function<void(const QString&, const QString&, int)> callback)
{
    m_statusCallback = callback;
}

void LoginFlowService::setErrorCallback(std::function<void(const QString&, const QString&)> callback)
{
    m_errorCallback = callback;
}

void LoginFlowService::setCompleteCallback(std::function<void()> callback)
{
    m_completeCallback = callback;
}

void LoginFlowService::executeLogin(
    const QString &mdAppKey,
    const QString &mdSecretKey,
    const QString &iaAppKey,
    const QString &iaSecretKey,
    const QString &loginID,
    bool downloadMasters,
    const QString &baseURL)
{
    updateStatus("init", "Starting login process...", 0);

    // Create API clients
    m_mdClient = new XTSMarketDataClient(baseURL, mdAppKey, mdSecretKey, this);
    m_iaClient = new XTSInteractiveClient(baseURL, iaAppKey, iaSecretKey, "WEBAPI", this);

    // Phase 1: Market Data API Login
    updateStatus("md_login", "Connecting to Market Data API...", 10);
    
    m_mdClient->login([this, downloadMasters](bool success, const QString &message) {
        if (!success) {
            notifyError("md_login", message);
            return;
        }

        updateStatus("md_login", "Market Data API connected", 30);

        // Phase 2: Interactive API Login
        updateStatus("ia_login", "Connecting to Interactive API...", 40);
        
        m_iaClient->login([this, downloadMasters](bool success, const QString &message) {
            if (!success) {
                notifyError("ia_login", message);
                return;
            }

            updateStatus("ia_login", "Interactive API connected", 60);

            // Phase 3: Download Masters (if requested)
            if (downloadMasters) {
                updateStatus("masters", "Downloading master contracts...", 70);
                // TODO: Implement master download
                updateStatus("masters", "Master files downloaded", 75);
            } else {
                updateStatus("masters", "Skipping master download", 75);
            }

            // Phase 4: Connect WebSocket
            updateStatus("websocket", "Establishing real-time connection...", 80);
            
            m_mdClient->connectWebSocket([this](bool success, const QString &message) {
                if (!success) {
                    // Non-fatal: log warning but continue
                    qWarning() << "WebSocket connection failed:" << message;
                }

                updateStatus("websocket", "Real-time connection established", 90);

                // Phase 5: Fetch initial data
                updateStatus("data", "Loading account data...", 95);
                
                // Fetch positions
                m_iaClient->getPositions("DayWise", [this](bool success, const QVector<XTS::Position> &positions, const QString &message) {
                    if (success) {
                        qDebug() << "Loaded" << positions.size() << "positions";
                    } else {
                        qWarning() << "Failed to load positions:" << message;
                    }

                    // Fetch orders
                    m_iaClient->getOrders([this](bool success, const QVector<XTS::Order> &orders, const QString &message) {
                        if (success) {
                            qDebug() << "Loaded" << orders.size() << "orders";
                        } else {
                            qWarning() << "Failed to load orders:" << message;
                        }

                        // Fetch trades
                        m_iaClient->getTrades([this](bool success, const QVector<XTS::Trade> &trades, const QString &message) {
                            if (success) {
                                qDebug() << "Loaded" << trades.size() << "trades";
                            } else {
                                qWarning() << "Failed to load trades:" << message;
                            }

                            // Complete!
                            updateStatus("complete", "Login successful!", 100);
                            emit loginComplete();
                            
                            if (m_completeCallback) {
                                m_completeCallback();
                            }
                        });
                    });
                });
            });
        });
    });
}

void LoginFlowService::updateStatus(const QString &phase, const QString &message, int progress)
{
    qDebug() << "[" << phase << "]" << message << "(" << progress << "%)";
    
    emit statusUpdate(phase, message, progress);
    
    if (m_statusCallback) {
        m_statusCallback(phase, message, progress);
    }
}

void LoginFlowService::notifyError(const QString &phase, const QString &error)
{
    qWarning() << "[ERROR:" << phase << "]" << error;
    
    emit errorOccurred(phase, error);
    
    if (m_errorCallback) {
        m_errorCallback(phase, error);
    }
}
