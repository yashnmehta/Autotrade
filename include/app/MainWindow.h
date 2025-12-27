#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "core/widgets/CustomMainWindow.h"
#include "api/XTSMarketDataClient.h"
#include "multicast_receiver.h"
#include "utils/LockFreeQueue.h"
#include <memory>
#include <QTimer>

class CustomMDIArea;
class QToolBar;
class QStatusBar;
class QCloseEvent;
class ScripBar;
class QLabel;
class InfoBar;
class QDockWidget;
class XTSInteractiveClient;
struct InstrumentData;

/**
 * @brief Trading Terminal Main Window
 *
 * This is the application-specific main window that uses CustomMainWindow
 * as its base. CustomMainWindow handles all the frameless window mechanics,
 * while this class focuses on trading terminal specific UI and logic.
 */
class MainWindow : public CustomMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    CustomMDIArea *mdiArea() const { return m_mdiArea; }
    
    // XTS API clients
    void setXTSClients(XTSMarketDataClient *mdClient, XTSInteractiveClient *iaClient);
    
    // Trading data service
    void setTradingDataService(class TradingDataService *tradingDataService);
    
    // ScripBar refresh
    void refreshScripBar();

private slots:
    // Window actions
    void createMarketWatch();
    void createBuyWindow();
    void createSellWindow();
    void createSnapQuoteWindow();
    void createOptionChainWindow();
    void createTradeBookWindow();
    void createOrderBookWindow();
    void createPositionWindow();
    void onAddToWatchRequested(const InstrumentData &instrument);
    void resetLayout();
    
    // Window limit helper
    int countWindowsOfType(const QString& type);
    
    // Market data updates
    void onTickReceived(const XTS::Tick &tick);

    // Workspace management
    void saveCurrentWorkspace();
    void loadWorkspace();
    void manageWorkspaces();
    void showPreferences();
    
    // UDP Broadcast receivers
    void startBroadcastReceiver();
    void stopBroadcastReceiver();
    
    // UDP tick queue drain (1ms timer)
    void drainTickQueue();

private:
    void setupContent();
    void setupShortcuts();
    void createMenuBar();
    void createToolBar();
    void createConnectionBar();
    void createStatusBar();
    void createInfoBar();
    
    // Window signal connection helper
    void connectWindowSignals(class CustomMDISubWindow *window);

    // Helper to get active MarketWatch for context
    class MarketWatchWindow* getActiveMarketWatch() const;
    
    // Persist state
    void closeEvent(QCloseEvent *event) override;

    CustomMDIArea *m_mdiArea;
    QToolBar *m_toolBar;
    QToolBar *m_connectionToolBar;
    QWidget *m_connectionBar;
    QStatusBar *m_statusBar;
    QDockWidget *m_infoDock;
    InfoBar *m_infoBar;
    QAction *m_statusBarAction;
    QAction *m_infoBarAction;
    ScripBar *m_scripBar;
    QToolBar *m_scripToolBar;
    
    // XTS API clients
    XTSMarketDataClient *m_xtsMarketDataClient;
    XTSInteractiveClient *m_xtsInteractiveClient;
    
    // Trading data service
    TradingDataService *m_tradingDataService;
    
    // UDP Broadcast receiver
    std::unique_ptr<MulticastReceiver> m_udpReceiver;
    
    // UDP tick queue (lock-free SPSC for ultra-low latency)
    LockFreeQueue<XTS::Tick> m_udpTickQueue{8192};  // 8K capacity
    QTimer *m_tickDrainTimer;  // Drain queue every 1ms
    
    // UDP message counters
    std::atomic<uint64_t> m_msg7200Count{0};
    std::atomic<uint64_t> m_msg7201Count{0};
    std::atomic<uint64_t> m_msg7202Count{0};
    std::atomic<uint64_t> m_msg7208Count{0};
    std::atomic<uint64_t> m_depthCount{0};
};

#endif // MAINWINDOW_H