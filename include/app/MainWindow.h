#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "api/XTSMarketDataClient.h"
#include "bse_receiver.h"
#include "core/widgets/CustomMainWindow.h"
#include "multicast_receiver.h"
#include "nsecm_multicast_receiver.h"
#include "utils/LockFreeQueue.h"
class QRect;
#include "models/WindowContext.h"
#include <QString>
#include <QTimer>
#include <memory>

// Forward declarations for chart widgets
class TradingViewChartWidget;
class IndicatorChartWidget;

class CustomMDIArea;
class CustomMDISubWindow;
class QToolBar;
class QStatusBar;
class QCloseEvent;
class ScripBar;
class QLabel;
class InfoBar;
class QDockWidget;
class QAction;
class XTSInteractiveClient;
class IndicesView;      // Forward declaration
class AllIndicesWindow; // Forward declaration
struct InstrumentData;

/**
 * @brief Trading Terminal Main Window
 *
 * This is the application-specific main window that uses CustomMainWindow
 * as its base. CustomMainWindow handles all the frameless window mechanics,
 * while this class focuses on trading terminal specific UI and logic.
 */
class MainWindow : public CustomMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  CustomMDIArea *mdiArea() const { return m_mdiArea; }

  // XTS API clients
  void setXTSClients(XTSMarketDataClient *mdClient,
                     XTSInteractiveClient *iaClient);

  // Trading data service
  void setTradingDataService(class TradingDataService *tradingDataService);

  // Config Loader
  void setConfigLoader(class ConfigLoader *configLoader);

  // ScripBar refresh
  void refreshScripBar();

  // IndicesView management
  bool hasIndicesView() const; // Check if IndicesView exists
  void createIndicesView();    // Create IndicesView (called from main.cpp)
  void showAllIndices();       // Show AllIndicesWindow (creates on-demand)

  // Order operations
  bool loadWorkspaceByName(const QString &name);
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
  CustomMDISubWindow *createMarketWatch();
  CustomMDISubWindow *createBuyWindow();
  CustomMDISubWindow *createSellWindow();
  CustomMDISubWindow *createSnapQuoteWindow();
  CustomMDISubWindow *createOptionChainWindow();
  CustomMDISubWindow *createOptionChainWindowForSymbol(const QString &symbol,
                                                       const QString &expiry);
  CustomMDISubWindow *createATMWatchWindow();
  CustomMDISubWindow *createTradeBookWindow();
  CustomMDISubWindow *createOrderBookWindow();
  CustomMDISubWindow *createPositionWindow();
  CustomMDISubWindow *createStrategyManagerWindow();
  CustomMDISubWindow *createGlobalSearchWindow();
  CustomMDISubWindow *createChartWindow();
  CustomMDISubWindow *createIndicatorChartWindow();
  CustomMDISubWindow *createMarketMovementWindow();
  CustomMDISubWindow *createOptionCalculatorWindow();
  void focusScripBar();
  void onAddToWatchRequested(const InstrumentData &instrument);
  void resetLayout();

  // Window limit helper
  int countWindowsOfType(const QString &type);
  void closeWindowsByType(const QString &type);

  // Market data updates
  void onTickReceived(const XTS::Tick &tick);

  // Workspace management
  void saveCurrentWorkspace();
  void loadWorkspace();

  void manageWorkspaces();
  void onRestoreWindowRequested(const QString &type, const QString &title,
                                const QRect &geometry, bool isMinimized,
                                bool isMaximized, bool isPinned,
                                const QString &workspaceName, int index);
  void showPreferences();

  // Window cycling (Ctrl+Tab / Ctrl+Shift+Tab)
  void cycleWindowsForward();
  void cycleWindowsBackward();

  void startBroadcastReceiver();
  void stopBroadcastReceiver();
  /**
   * @brief Route price subscription request to PriceCache (zero-copy)
   * Called when new PriceCache is enabled (use_legacy_mode = false)
   * @param requesterId Unique ID to route response back to requester
   * @param token Token to subscribe
   * @param segment Market segment (1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO)
   */
  void onPriceSubscriptionRequest(QString requesterId, uint32_t token,
                                  uint16_t segment);
  void onScripBarEscapePressed();

  // Context-aware window creation with explicit initiating window
  void createBuyWindowWithContext(const WindowContext &context,
                                  QWidget *initiatingWindow);
  void createSellWindowWithContext(const WindowContext &context,
                                   QWidget *initiatingWindow);
  void createSnapQuoteWindowWithContext(const WindowContext &context,
                                        QWidget *initiatingWindow);

  // Widget-aware window creation (invoked from CustomMDISubWindow F1/F2
  // fallback)
  Q_INVOKABLE void createBuyWindowFromWidget(QWidget *initiatingWidget);
  Q_INVOKABLE void createSellWindowFromWidget(QWidget *initiatingWidget);

private:
  void setupContent();
  void setupShortcuts();
  void setupConnections();
  void setupNetwork();
  void initializeXTSFeedBridge();
  void createMenuBar();
  void createToolBar();
  void createConnectionBar();
  void createStatusBar();
  void createInfoBar();
  // createIndicesView() is now public (see above)

  // Window signal connection helper
  void connectWindowSignals(class CustomMDISubWindow *window);

  // Helper to get active MarketWatch for context
  class MarketWatchWindow *getActiveMarketWatch() const;

  // Helper to get the best available window context
  WindowContext getBestWindowContext() const;

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

  // Indices View
  QDockWidget *m_indicesDock;
  IndicesView *m_indicesView;
  AllIndicesWindow *m_allIndicesWindow;

  QAction *m_statusBarAction;
  QAction *m_infoBarAction;
  QAction *m_indicesViewAction; // New actionBar;
  QAction *m_allIndicesAction;
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