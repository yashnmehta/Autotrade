#include "services/MasterLoaderWorker.h"
#include "repository/RepositoryManager.h"
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QMutexLocker>

MasterLoaderWorker::MasterLoaderWorker(QObject *parent)
    : QThread(parent)
    , m_loadMode(LoadMode::None)
    , m_saveAfterLoad(false)
    , m_cancelled(false)
{
}

MasterLoaderWorker::~MasterLoaderWorker()
{
    // Ensure thread is stopped before destruction
    cancel();
    if (isRunning()) {
        wait(5000); // Wait up to 5 seconds
        if (isRunning()) {
            terminate(); // Force terminate if still running
            wait(1000);
        }
    }
}

void MasterLoaderWorker::loadFromCache(const QString& mastersDir)
{
    QMutexLocker locker(&m_mutex);
    
    if (isRunning()) {
        qWarning() << "[MasterLoaderWorker] Worker already running, cannot start new task";
        return;
    }
    
    m_loadMode = LoadMode::FromCache;
    m_mastersDir = mastersDir;
    m_csvData.clear();
    m_cancelled = false;
    
    start();
}

void MasterLoaderWorker::loadFromDownload(const QString& mastersDir, const QString& csvData)
{
    QMutexLocker locker(&m_mutex);
    
    if (isRunning()) {
        qWarning() << "[MasterLoaderWorker] Worker already running, cannot start new task";
        return;
    }
    
    m_loadMode = LoadMode::FromDownload;
    m_mastersDir = mastersDir;
    m_csvData = csvData;
    m_saveAfterLoad = false;
    m_cancelled = false;
    
    start();
}

void MasterLoaderWorker::loadFromMemoryOnly(const QString& csvData, bool saveAfterLoad, const QString& mastersDir)
{
    QMutexLocker locker(&m_mutex);
    
    if (isRunning()) {
        qWarning() << "[MasterLoaderWorker] Worker already running, cannot start new task";
        return;
    }
    
    m_loadMode = LoadMode::FromMemoryOnly;
    m_mastersDir = mastersDir;
    m_csvData = csvData;
    m_saveAfterLoad = saveAfterLoad;
    m_cancelled = false;
    
    qDebug() << "[MasterLoaderWorker] Starting memory-only load (save after load:" 
             << saveAfterLoad << ")";
    
    start();
}

bool MasterLoaderWorker::isRunning() const
{
    return QThread::isRunning();
}

void MasterLoaderWorker::cancel()
{
    QMutexLocker locker(&m_mutex);
    m_cancelled = true;
}

bool MasterLoaderWorker::isCancelled() const
{
    QMutexLocker locker(&m_mutex);
    return m_cancelled;
}

void MasterLoaderWorker::run()
{
    emit loadingStarted();
    
    LoadMode mode;
    QString mastersDir;
    QString csvData;
    bool saveAfterLoad;
    
    // Copy parameters under lock
    {
        QMutexLocker locker(&m_mutex);
        mode = m_loadMode;
        mastersDir = m_mastersDir;
        csvData = m_csvData;
        saveAfterLoad = m_saveAfterLoad;
    }
    
    if (isCancelled()) {
        emit loadingFailed("Operation cancelled");
        return;
    }
    
    try {
        RepositoryManager* repo = RepositoryManager::getInstance();
        if (!repo) {
            emit loadingFailed("Failed to get RepositoryManager instance");
            return;
        }
        
        if (mode == LoadMode::FromCache) {
            // Load from cached files
            qDebug() << "[MasterLoaderWorker] Loading from cache:" << mastersDir;
            emit loadingProgress(10, "Loading cached masters...");
            
            if (isCancelled()) {
                emit loadingFailed("Operation cancelled");
                return;
            }
            
            // Emit progress updates during loading
            emit loadingProgress(20, "Loading NSE CM...");
            
            // This operation is thread-safe as RepositoryManager uses internal locking
            bool success = repo->loadAll(mastersDir);
            
            emit loadingProgress(80, "Building caches...");
            
            if (isCancelled()) {
                emit loadingFailed("Operation cancelled");
                return;
            }
            
            if (success) {
                int count = repo->getTotalContractCount();
                qDebug() << "[MasterLoaderWorker] Successfully loaded" << count << "contracts from cache";
                emit loadingProgress(100, "Cache loaded successfully");
                emit loadingComplete(count);
            } else {
                qWarning() << "[MasterLoaderWorker] Failed to load from cache";
                emit loadingFailed("Failed to load cached masters - you may need to download them");
            }
            
        } else if (mode == LoadMode::FromDownload) {
            // Save downloaded file and load
            qDebug() << "[MasterLoaderWorker] Processing downloaded masters...";
            emit loadingProgress(10, "Saving downloaded data...");
            
            if (isCancelled()) {
                emit loadingFailed("Operation cancelled");
                return;
            }
            
            // Ensure directory exists
            QDir().mkpath(mastersDir);
            
            // Save to file
            QString filePath = mastersDir + "/master_contracts_latest.txt";
            saveDownloadedFile(filePath, csvData);
            
            if (isCancelled()) {
                emit loadingFailed("Operation cancelled");
                return;
            }
            
            emit loadingProgress(30, "Loading master contracts...");
            qDebug() << "[MasterLoaderWorker] Loading masters into RepositoryManager...";
            
            // Load the saved file (thread-safe operation)
            bool success = repo->loadAll(mastersDir);
            
            if (isCancelled()) {
                emit loadingFailed("Operation cancelled");
                return;
            }
            
            if (!success) {
                emit loadingFailed("Failed to load downloaded masters");
                return;
            }
            
            emit loadingProgress(70, "Saving processed CSVs...");
            qDebug() << "[MasterLoaderWorker] Saving processed CSVs for faster future loading...";
            
            // Save processed CSVs (thread-safe operation)
            if (repo->saveProcessedCSVs(mastersDir)) {
                qDebug() << "[MasterLoaderWorker] Processed CSVs saved successfully";
            } else {
                qWarning() << "[MasterLoaderWorker] Failed to save processed CSVs (non-fatal)";
            }
            
            if (isCancelled()) {
                emit loadingFailed("Operation cancelled");
                return;
            }
            
            int count = repo->getTotalContractCount();
            qDebug() << "[MasterLoaderWorker] Successfully loaded" << count << "contracts from download";
            emit loadingProgress(100, "Masters loaded successfully");
            emit loadingComplete(count);
            
        } else if (mode == LoadMode::FromMemoryOnly) {
            // OPTIMIZED: Load directly from memory - NO FILE I/O!
            qDebug() << "[MasterLoaderWorker] Loading directly from memory (size:" 
                     << csvData.size() << "bytes)";
            emit loadingProgress(10, "Parsing in-memory data...");
            
            if (isCancelled()) {
                emit loadingFailed("Operation cancelled");
                return;
            }
            
            // Load directly from memory into RepositoryManager (no file I/O)
            bool success = repo->loadFromMemory(csvData);
            
            if (isCancelled()) {
                emit loadingFailed("Operation cancelled");
                return;
            }
            
            if (!success) {
                emit loadingFailed("Failed to load from memory");
                return;
            }
            
            int count = repo->getTotalContractCount();
            qDebug() << "[MasterLoaderWorker] ✓ Successfully loaded" << count 
                     << "contracts from memory (NO FILE I/O)";
            emit loadingProgress(70, QString("Loaded %1 contracts from memory").arg(count));
            
            // Optionally save to disk for future cache loading
            if (saveAfterLoad && !mastersDir.isEmpty()) {
                qDebug() << "[MasterLoaderWorker] Saving to disk for future cache...";
                emit loadingProgress(80, "Saving to disk...");
                
                // Ensure directory exists
                QDir().mkpath(mastersDir);
                
                // Save raw master file
                QString filePath = mastersDir + "/master_contracts_latest.txt";
                try {
                    saveDownloadedFile(filePath, csvData);
                    qDebug() << "[MasterLoaderWorker] ✓ Raw master file saved";
                } catch (const std::exception& e) {
                    qWarning() << "[MasterLoaderWorker] Failed to save raw file (non-fatal):" << e.what();
                }
                
                if (isCancelled()) {
                    // Even if cancelled, data is already loaded, so emit success
                    emit loadingComplete(count);
                    return;
                }
                
                emit loadingProgress(90, "Saving processed CSVs...");
                
                // Save processed CSVs for faster future loading
                if (repo->saveProcessedCSVs(mastersDir)) {
                    qDebug() << "[MasterLoaderWorker] ✓ Processed CSVs saved";
                } else {
                    qWarning() << "[MasterLoaderWorker] Failed to save processed CSVs (non-fatal)";
                }
            }
            
            emit loadingProgress(100, "Memory load complete");
            emit loadingComplete(count);
            
        } else {
            emit loadingFailed("Invalid load mode");
        }
        
    } catch (const std::exception& e) {
        qCritical() << "[MasterLoaderWorker] Exception during loading:" << e.what();
        emit loadingFailed(QString("Exception: %1").arg(e.what()));
    } catch (...) {
        qCritical() << "[MasterLoaderWorker] Unknown exception during loading";
        emit loadingFailed("Unknown exception occurred");
    }
}

void MasterLoaderWorker::saveDownloadedFile(const QString& filePath, const QString& csvData)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString error = QString("Failed to open file for writing: %1").arg(file.errorString());
        qCritical() << "[MasterLoaderWorker]" << error;
        throw std::runtime_error(error.toStdString());
    }
    
    QTextStream stream(&file);
    stream << csvData;
    file.close();
    
    qDebug() << "[MasterLoaderWorker] Master contracts saved to:" << filePath;
}
