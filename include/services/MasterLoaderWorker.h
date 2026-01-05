#ifndef MASTERLOADERWORKER_H
#define MASTERLOADERWORKER_H

#include <QThread>
#include <QString>
#include <QMutex>
#include <QWaitCondition>

/**
 * @brief Thread-safe worker for loading master contracts asynchronously
 * 
 * This worker runs on a separate thread to prevent blocking the GUI
 * when loading large master contract files. It provides proper locking
 * and cross-platform compatibility.
 * 
 * Thread Safety:
 * - Uses QMutex for protecting shared state
 * - Emits signals from worker thread (Qt automatically queues to main thread)
 * - Ensures RepositoryManager operations are atomic
 * 
 * Cross-Platform:
 * - Uses Qt's cross-platform threading primitives (QThread, QMutex)
 * - Works on Windows, Linux, and macOS
 * - No platform-specific code
 */
class MasterLoaderWorker : public QThread
{
    Q_OBJECT

public:
    explicit MasterLoaderWorker(QObject *parent = nullptr);
    ~MasterLoaderWorker();

    /**
     * @brief Start loading masters from cache
     * @param mastersDir Directory containing master files
     */
    void loadFromCache(const QString& mastersDir);

    /**
     * @brief Start loading masters from downloaded data and save processed CSVs
     * @param mastersDir Directory containing master files
     * @param csvData Raw CSV data from download
     */
    void loadFromDownload(const QString& mastersDir, const QString& csvData);
    
    /**
     * @brief Start loading masters directly from memory (no file I/O)
     * @param csvData Raw CSV data from download
     * @param saveAfterLoad If true, save to disk after loading (optional)
     * @param mastersDir Directory to save files (required if saveAfterLoad=true)
     * 
     * This method is optimized for freshly downloaded data. It loads
     * directly from memory into RepositoryManager without any file I/O,
     * then optionally saves to disk for future cache loading.
     */
    void loadFromMemoryOnly(const QString& csvData, bool saveAfterLoad = true, 
                            const QString& mastersDir = QString());

    /**
     * @brief Check if worker is currently running
     */
    bool isRunning() const;

    /**
     * @brief Cancel the current operation
     */
    void cancel();

signals:
    /**
     * @brief Emitted when loading starts
     */
    void loadingStarted();

    /**
     * @brief Emitted during loading progress
     * @param percentage Progress from 0-100
     * @param message Status message
     */
    void loadingProgress(int percentage, const QString& message);

    /**
     * @brief Emitted when loading completes successfully
     * @param contractCount Total contracts loaded
     */
    void loadingComplete(int contractCount);

    /**
     * @brief Emitted when loading fails
     * @param errorMessage Error description
     */
    void loadingFailed(const QString& errorMessage);

protected:
    void run() override;

private:
    enum class LoadMode {
        None,
        FromCache,
        FromDownload,
        FromMemoryOnly  // New mode: direct memory load, no initial file I/O
    };

    LoadMode m_loadMode;
    QString m_mastersDir;
    QString m_csvData;
    bool m_saveAfterLoad;  // For FromMemoryOnly mode
    
    mutable QMutex m_mutex;
    QWaitCondition m_condition;
    bool m_cancelled;

    // Thread-safe getters/setters
    bool isCancelled() const;
    void saveDownloadedFile(const QString& filePath, const QString& csvData);
};

#endif // MASTERLOADERWORKER_H
