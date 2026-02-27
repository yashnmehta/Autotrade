#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "api/xts/XTSMarketDataClient.h"
#include "bse_receiver.h"
#include "core/widgets/CustomMainWindow.h"
#include "multicast_receiver.h"
#include "nsecm_multicast_receiver.h"

class ConnectionBarWidget;
class BroadcastSettingsDialog;
class QRect;
#include "models/domain/WindowContext.h"
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
class IndicesView;
class AllIndicesWindow;
class WindowFactory;
class WorkspaceManager;
struct InstrumentData;

/**
 * @brief Trading Terminal Main Window
 *
 * This is the application-specific main window that uses CustomMainWindow
 * as its base. CustomMainWindow handles all the frameless window mechanics,
 * while this class focuses on trading terminal specific UI and layout.
 *
 * Window creation is delegated to WindowFactory.
 * Workspace persistence is delegated to WorkspaceManager.
 */
class MainWindow : public CustomMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  CustomMDIArea *mdiArea() const { return m_mdiArea; }

  /// Access the window factory (used by external components)
  WindowFactory *windowFactory() const { return m_windowFactory; }

  /// Access the workspace manager
  WorkspaceManager *workspaceManager() const { return m_workspaceManager; }

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
  bool hasIndicesView() const;
  void createIndicesView();
  void showAllIndices();

  // ── Order operations (stay on MainWindow — they need XTS clients) ────
  bool loadWorkspaceByName(const QString &name);
  void placeOrder(const XTS::OrderParams &params);
  void modifyOrder(const XTS::ModifyOrderParams &params);
  void cancelOrder(int64_t appOrderID);

private slots:
  // ── Thin delegators to WindowFactory (kept as slots for menu/shortcut) ──
  void createMarketWatch();
  void createBuyWindow();
  void createSellWindow();
  void createSnapQuoteWindow();
  void createOptionChainWindow();
  void createATMWatchWindow();
  void createTradeBookWindow();
  void createOrderBookWindow();
  void createPositionWindow();
  void createStrategyManagerWindow();
  void createGlobalSearchWindow();
  void createChartWindow();
  void createIndicatorChartWindow();
  void createMarketMovementWindow();
  void createOptionCalculatorWindow();
  void focusScripBar();
  void onAddToWatchRequested(const InstrumentData &instrument);
  void resetLayout();

  // Market data updates
  void onTickReceived(const XTS::Tick &tick);

  // Workspace management (delegated to WorkspaceManager)
  void saveCurrentWorkspace();
  void loadWorkspace();
  void manageWorkspaces();
  void showPreferences();

  // Window cycling (Ctrl+Tab / Ctrl+Shift+Tab)
  void cycleWindowsForward();
  void cycleWindowsBackward();

  void startBroadcastReceiver();
  void stopBroadcastReceiver();

  void onConnectionSettingsRequested();

  void onPriceSubscriptionRequest(QString requesterId, uint32_t token,
                                  uint16_t segment);
  void onScripBarEscapePressed();

  // Widget-aware window creation (invoked from CustomMDISubWindow F1/F2
  // fallback) — delegated to WindowFactory
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

  // Persist state
  void closeEvent(QCloseEvent *event) override;

  // ── Extracted collaborators ──────────────────────────────────────────
  WindowFactory *m_windowFactory;
  WorkspaceManager *m_workspaceManager;

  CustomMDIArea *m_mdiArea;
  QMenuBar *m_menuBar;
  QToolBar *m_toolBar;
  QToolBar *m_connectionToolBar;
  ConnectionBarWidget *m_connectionBar;
  QStatusBar *m_statusBar;
  InfoBar *m_infoBar;
  QDockWidget *m_infoDock;

  // Indices View
  QDockWidget *m_indicesDock;
  IndicesView *m_indicesView;
  AllIndicesWindow *m_allIndicesWindow;

  QAction *m_statusBarAction;
  QAction *m_infoBarAction;
  QAction *m_indicesViewAction;
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