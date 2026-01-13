#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "core/widgets/CustomMainWindow.h"
#include "api/XTSMarketDataClient.h"
#include "multicast_receiver.h"
#include "nsecm_multicast_receiver.h"
#include "bse_receiver.h"
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
    
    // Config Loader
    void setConfigLoader(class ConfigLoader *configLoader);
    
    // ScripBar refresh
    void refreshScripBar();

    // Order operations
    void placeOrder(const XTS::OrderParams &params);
    void modifyOrder(const XTS::ModifyOrderParams &params);
    void cancelOrder(int64_t appOrderID);
    
    // Open order window for modification
    void openBuyWindowForModification(const XTS::Order &order);
    void openSellWindowForModification(const XTS::Order &order);
    
    // Batch modification
    void openBatchBuyWindowForModification(const QVector<XTS::Order> &orders);
    void openBatchSellWindowForModification(const QVector<XTS::Order> &orders);

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
    void focusScripBar();
    void onAddToWatchRequested(const InstrumentData &instrument);
    void resetLayout();
    
    // Window limit helper
    int countWindowsOfType(const QString& type);
    void closeWindowsByType(const QString& type);
    
    // Market data updates
    void onTickReceived(const XTS::Tick &tick);

    // Workspace management
    void saveCurrentWorkspace();
    void loadWorkspace();
    void manageWorkspaces();
    void onRestoreWindowRequested(const QString &type, const QString &title, const QRect &geometry, bool isMinimized, bool isMaximized, bool isPinned, const QString &workspaceName, int index);
    void showPreferences();
    
    // Window cycling (Ctrl+Tab / Ctrl+Shift+Tab)
    void cycleWindowsForward();
    void cycleWindowsBackward();
    
    // Broadcast receiver slots
    void onUdpTickReceived(const XTS::Tick& tick);
    void startBroadcastReceiver();
    void stopBroadcastReceiver();

private:
    void setupContent();
    void setupShortcuts();
    void setupConnections();
    void setupNetwork();
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
    QMenuBar *m_menuBar;
    QToolBar *m_toolBar;
    QToolBar *m_connectionToolBar;
    QWidget *m_connectionBar;
    QStatusBar *m_statusBar;
    InfoBar *m_infoBar;
    QDockWidget *m_infoDock; // Added dock widget
    QAction *m_statusBarAction;
    QAction *m_infoBarAction;
    ScripBar *m_scripBar;
    QToolBar *m_scripToolBar;
    
    // XTS API clients
    XTSMarketDataClient *m_xtsMarketDataClient;
    XTSInteractiveClient *m_xtsInteractiveClient;
    
    // Trading data service
    TradingDataService *m_tradingDataService;
    
    // Config Loader
    class ConfigLoader *m_configLoader;
};


#endif // MAINWINDOW_H