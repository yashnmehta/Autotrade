#ifndef WINDOWCACHEMANAGER_H
#define WINDOWCACHEMANAGER_H

#include <QObject>

class MainWindow;
class CustomMDISubWindow;
class BuyWindow;
class SellWindow;
struct WindowContext;

/**
 * @brief Manages pre-cached Buy/Sell windows for fast opening (~10ms instead of ~400ms)
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

private:
    WindowCacheManager();
    ~WindowCacheManager();
    WindowCacheManager(const WindowCacheManager&) = delete;
    WindowCacheManager& operator=(const WindowCacheManager&) = delete;
    
    void createCachedWindows();
    void resetBuyWindow();
    void resetSellWindow();
    
    MainWindow* m_mainWindow = nullptr;
    bool m_initialized = false;
    
    // Cached windows
    CustomMDISubWindow* m_cachedBuyMdiWindow = nullptr;
    CustomMDISubWindow* m_cachedSellMdiWindow = nullptr;
    BuyWindow* m_cachedBuyWindow = nullptr;
    SellWindow* m_cachedSellWindow = nullptr;
};

#endif // WINDOWCACHEMANAGER_H
