#ifndef MASTERDATASTATE_H
#define MASTERDATASTATE_H

#include <QObject>
#include <QMutex>
#include <QString>

/**
 * @brief Singleton class to manage shared state of master data loading
 * 
 * This class provides thread-safe state management for master contract data
 * to coordinate between splash screen preloading and login window operations.
 * Prevents redundant master file loading and downloads.
 * 
 * Thread Safety:
 * - Uses QMutex for protecting shared state
 * - All public methods are thread-safe
 * - Can be safely accessed from multiple threads
 * 
 * Cross-Platform:
 * - Uses Qt's cross-platform threading primitives
 * - Works on Windows, Linux, and macOS
 */
class MasterDataState : public QObject
{
    Q_OBJECT

public:
    enum class LoadState {
        NotLoaded,      // Masters not loaded yet
        Loading,        // Currently loading in background
        Loaded,         // Successfully loaded from cache
        LoadFailed,     // Failed to load from cache (need download)
        Downloaded      // Downloaded and loaded during login
    };

    /**
     * @brief Get singleton instance
     */
    static MasterDataState* getInstance();

    /**
     * @brief Check if masters are already loaded
     * @return true if masters are loaded and ready to use
     */
    bool areMastersLoaded() const;

    /**
     * @brief Check if masters are currently being loaded
     * @return true if loading operation is in progress
     */
    bool isLoading() const;

    /**
     * @brief Get current load state
     */
    LoadState getLoadState() const;

    /**
     * @brief Set load state (thread-safe)
     */
    void setLoadState(LoadState state);

    /**
     * @brief Mark that masters are successfully loaded
     * @param contractCount Number of contracts loaded
     */
    void setMastersLoaded(int contractCount);

    /**
     * @brief Mark that master loading failed
     * @param errorMessage Error description
     */
    void setLoadingFailed(const QString& errorMessage);

    /**
     * @brief Mark that loading started
     */
    void setLoadingStarted();

    /**
     * @brief Get number of loaded contracts
     */
    int getContractCount() const;

    /**
     * @brief Get last error message
     */
    QString getLastError() const;

    /**
     * @brief Reset state (for testing or re-initialization)
     */
    void reset();

signals:
    /**
     * @brief Emitted when load state changes
     */
    void stateChanged(LoadState newState);

    /**
     * @brief Emitted when masters are successfully loaded
     */
    void mastersReady(int contractCount);

    /**
     * @brief Emitted when loading fails
     */
    void loadingError(const QString& errorMessage);

private:
    // Private constructor for singleton
    explicit MasterDataState(QObject *parent = nullptr);
    ~MasterDataState();

    // Prevent copying
    MasterDataState(const MasterDataState&) = delete;
    MasterDataState& operator=(const MasterDataState&) = delete;

    static MasterDataState* s_instance;
    static QMutex s_instanceMutex;

    mutable QMutex m_stateMutex;
    LoadState m_loadState;
    int m_contractCount;
    QString m_lastError;
};

#endif // MASTERDATASTATE_H
