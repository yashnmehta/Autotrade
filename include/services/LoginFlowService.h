#ifndef LOGINFLOWSERVICE_H
#define LOGINFLOWSERVICE_H

#include "api/XTSMarketDataClient.h"
#include "api/XTSInteractiveClient.h"
#include "api/XTSTypes.h"
#include "services/MasterLoaderWorker.h"
#include <QObject>
#include <functional>

class LoginFlowService : public QObject
{
    Q_OBJECT

public:
    explicit LoginFlowService(QObject *parent = nullptr);
    ~LoginFlowService();

    // Execute complete login flow
    void executeLogin(
        const QString &mdAppKey,
        const QString &mdSecretKey,
        const QString &iaAppKey,
        const QString &iaSecretKey,
        const QString &loginID,
        bool downloadMasters,
        const QString &baseURL,
        const QString &source = "WEBAPI"
    );

    // Getters for API clients
    XTSMarketDataClient* getMarketDataClient() const { return m_mdClient; }
    XTSInteractiveClient* getInteractiveClient() const { return m_iaClient; }

    // Set trading data service for storing fetched data
    void setTradingDataService(class TradingDataService* service) { m_tradingDataService = service; }

    // Status callbacks
    void setStatusCallback(std::function<void(const QString&, const QString&, int)> callback);
    void setErrorCallback(std::function<void(const QString&, const QString&)> callback);
    void setCompleteCallback(std::function<void()> callback);

signals:
    void statusUpdate(const QString &phase, const QString &message, int progress);
    void errorOccurred(const QString &phase, const QString &error);
    void loginComplete();
    void mastersLoaded();

private:
    void updateStatus(const QString &phase, const QString &message, int progress);
    void notifyError(const QString &phase, const QString &error);
    
    // Internal methods for handling async master loading
    void handleMasterLoadingComplete(int contractCount);
    void handleMasterLoadingFailed(const QString& errorMessage);
    void continueLoginAfterMasters();
    void startMasterDownload(const QString& mastersDir);

    XTSMarketDataClient *m_mdClient;
    XTSInteractiveClient *m_iaClient;
    TradingDataService *m_tradingDataService;
    MasterLoaderWorker *m_masterLoader;

    std::function<void(const QString&, const QString&, int)> m_statusCallback;
    std::function<void(const QString&, const QString&)> m_errorCallback;
    std::function<void()> m_completeCallback;
};

#endif // LOGINFLOWSERVICE_H
