#ifndef WINDOWCACHEMANAGER_H
#define WINDOWCACHEMANAGER_H

#include <QObject>
#include <QPoint>
#include <QVector>
#include <QDateTime>

class MainWindow;
class CustomMDISubWindow;
class BuyWindow;
class SellWindow;
class SnapQuoteWindow;
struct WindowContext;

/**
 * @brief Manages pre-cached Buy/Sell/SnapQuote windows for fast opening (~10ms instead of ~400ms)
 * 
 * This singleton class handles window pre-creation and reuse to dramatically improve
 * window open performance. It keeps existing MainWindow code clean by managing all
 * caching logic separately.
 */
class WindowCacheManager : public QObject
{
    Q_OBJECT

public:
    static WindowCacheManager& instance();
    
    /**
     * @brief Initialize the window cache (call after MainWindow is ready)
     * @param mainWindow Pointer to the main window
     */
    void initialize(MainWindow* mainWindow);
    
    /**
     * @brief Check if cache is initialized and ready
     */
    bool isInitialized() const { return m_initialized; }
    
    /**
     * @brief Set XTS Market Data Client for cached SnapQuote window
     * @param client Pointer to XTS Market Data Client
     */
    void setXTSClientForSnapQuote(class XTSMarketDataClient *client);
    
    /**
     * @brief Show cached Buy window with optional context
     * @param context Window context (instrument details)
     * @return true if cached window was shown, false if cache not available
     */
    bool showBuyWindow(const WindowContext* context = nullptr);
    
    /**
     * @brief Show cached Sell window with optional context
     * @param context Window context (instrument details)
     * @return true if cached window was shown, false if cache not available
     */
    bool showSellWindow(const WindowContext* context = nullptr);
    
    /**
     * @brief Show cached SnapQuote window with optional context
     * @param context Window context (instrument details)
     * @return true if cached window was shown, false if cache not available
     */
    bool showSnapQuoteWindow(const WindowContext* context = nullptr);
    
    /**
     * @brief Mark Buy window as needing reset (called when user closes it)
     */
    void markBuyWindowClosed() { 
        m_buyWindowNeedsReset = true;
        m_lastBuyToken = -1;  // Clear cached token
    }
    
    /**
     * @brief Mark Sell window as needing reset (called when user closes it)
     */
    void markSellWindowClosed() { 
        m_sellWindowNeedsReset = true;
        m_lastSellToken = -1;  // Clear cached token
    }
    
    /**
     * @brief Mark SnapQuote window as needing reset (called when user closes it)
     * @param windowIndex Index of the SnapQuote window (0-2)
     */
    void markSnapQuoteWindowClosed(int windowIndex);

    /**
     * @brief Save the current order window position to memory and lazily to disk
     * @param pos The new position
     */
    void saveOrderWindowPosition(const QPoint& pos);

private:
    WindowCacheManager();
    ~WindowCacheManager();
    WindowCacheManager(const WindowCacheManager&) = delete;
    WindowCacheManager& operator=(const WindowCacheManager&) = delete;
    
    void createCachedWindows();
    void resetBuyWindow();
    void resetSellWindow();
    void resetSnapQuoteWindow(int index);
    int findLeastRecentlyUsedSnapQuoteWindow();
    
    // ⚡ SnapQuote window pool entry
    struct SnapQuoteWindowEntry {
        CustomMDISubWindow* mdiWindow = nullptr;
        SnapQuoteWindow* window = nullptr;
        int lastToken = -1;
        QDateTime lastUsedTime;
        bool needsReset = true;
        bool isVisible = false;
    };
    
    MainWindow* m_mainWindow = nullptr;
    bool m_initialized = false;
    
    // Cached windows
    CustomMDISubWindow* m_cachedBuyMdiWindow = nullptr;
    CustomMDISubWindow* m_cachedSellMdiWindow = nullptr;
    BuyWindow* m_cachedBuyWindow = nullptr;
    SellWindow* m_cachedSellWindow = nullptr;
    
    // ⚡ MULTIPLE SNAPQUOTE WINDOWS (up to 3)
    static constexpr int MAX_SNAPQUOTE_WINDOWS = 3;
    QVector<SnapQuoteWindowEntry> m_snapQuoteWindows;
    
    // State tracking for smart reset
    bool m_buyWindowNeedsReset = true;   ///< True if Buy window was closed (needs reset on next show)
    bool m_sellWindowNeedsReset = true;  ///< True if Sell window was closed (needs reset on next show)
    
    // Cached context to avoid reloading same data
    int m_lastBuyToken = -1;     ///< Last token loaded in Buy window
    int m_lastSellToken = -1;    ///< Last token loaded in Sell window

    // In-memory cache for window position (avoids slow QSettings read on every F1/F2)
    QPoint m_lastOrderWindowPos;
    bool m_hasSavedPosition = false;

    // Event coalescing: track pending window activation to cancel stale requests
    enum class PendingWindow { None, Buy, Sell };
    PendingWindow m_pendingActivation = PendingWindow::None;
};

#endif // WINDOWCACHEMANAGER_H
