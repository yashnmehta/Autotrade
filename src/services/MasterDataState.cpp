#include "services/MasterDataState.h"
#include <QMutexLocker>
#include <QDebug>

// Static member initialization
MasterDataState* MasterDataState::s_instance = nullptr;
QMutex MasterDataState::s_instanceMutex;

MasterDataState::MasterDataState(QObject *parent)
    : QObject(parent)
    , m_loadState(LoadState::NotLoaded)
    , m_contractCount(0)
{
    qDebug() << "[MasterDataState] Singleton instance created";
}

MasterDataState::~MasterDataState()
{
    qDebug() << "[MasterDataState] Singleton instance destroyed";
}

MasterDataState* MasterDataState::getInstance()
{
    // Double-checked locking pattern for thread-safe singleton
    if (s_instance == nullptr) {
        QMutexLocker locker(&s_instanceMutex);
        if (s_instance == nullptr) {
            s_instance = new MasterDataState();
        }
    }
    return s_instance;
}

bool MasterDataState::areMastersLoaded() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_loadState == LoadState::Loaded || m_loadState == LoadState::Downloaded;
}

bool MasterDataState::isLoading() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_loadState == LoadState::Loading;
}

MasterDataState::LoadState MasterDataState::getLoadState() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_loadState;
}

void MasterDataState::setLoadState(LoadState state)
{
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_loadState == state) {
            return; // No change
        }
        m_loadState = state;
    }
    
    qDebug() << "[MasterDataState] State changed to:" << static_cast<int>(state);
    emit stateChanged(state);
}

void MasterDataState::setMastersLoaded(int contractCount)
{
    {
        QMutexLocker locker(&m_stateMutex);
        m_contractCount = contractCount;
        m_loadState = LoadState::Loaded;
        m_lastError.clear();
    }
    
    qDebug() << "[MasterDataState] Masters loaded successfully with" << contractCount << "contracts";
    emit stateChanged(LoadState::Loaded);
    emit mastersReady(contractCount);
}

void MasterDataState::setLoadingFailed(const QString& errorMessage)
{
    {
        QMutexLocker locker(&m_stateMutex);
        m_loadState = LoadState::LoadFailed;
        m_lastError = errorMessage;
    }
    
    qWarning() << "[MasterDataState] Loading failed:" << errorMessage;
    emit stateChanged(LoadState::LoadFailed);
    emit loadingError(errorMessage);
}

void MasterDataState::setLoadingStarted()
{
    {
        QMutexLocker locker(&m_stateMutex);
        m_loadState = LoadState::Loading;
        m_lastError.clear();
    }
    
    qDebug() << "[MasterDataState] Loading started";
    emit stateChanged(LoadState::Loading);
}

int MasterDataState::getContractCount() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_contractCount;
}

QString MasterDataState::getLastError() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_lastError;
}

void MasterDataState::reset()
{
    {
        QMutexLocker locker(&m_stateMutex);
        m_loadState = LoadState::NotLoaded;
        m_contractCount = 0;
        m_lastError.clear();
    }
    
    qDebug() << "[MasterDataState] State reset";
    emit stateChanged(LoadState::NotLoaded);
}
