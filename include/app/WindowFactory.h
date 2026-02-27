#ifndef WINDOWFACTORY_H
#define WINDOWFACTORY_H

#include <QObject>
#include <QRect>
#include <QString>
#include <QVector>

// Forward declarations
class MainWindow;
class CustomMDIArea;
class CustomMDISubWindow;
class MarketWatchWindow;
class XTSMarketDataClient;
class XTSInteractiveClient;
class TradingDataService;
struct WindowContext;
struct InstrumentData;

namespace XTS {
struct Order;
struct OrderParams;
struct ModifyOrderParams;
}

/**
 * @brief Factory for creating and managing MDI child windows
 *
 * Extracted from MainWindow to follow Single Responsibility Principle.
 * WindowFactory owns all createXxxWindow() methods and the signal wiring
 * that connects each newly created window back to MainWindow or services.
 */
class WindowFactory : public QObject {
  Q_OBJECT

public:
  explicit WindowFactory(MainWindow *mainWindow, CustomMDIArea *mdiArea,
                         QObject *parent = nullptr);

  // Dependency injection
  void setXTSClients(XTSMarketDataClient *mdClient,
                     XTSInteractiveClient *iaClient);
  void setTradingDataService(TradingDataService *service);

  // ── Primary window creation ──────────────────────────────────────────
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

  // ── Order modification windows ───────────────────────────────────────
  void openBuyWindowForModification(const XTS::Order &order);
  void openSellWindowForModification(const XTS::Order &order);
  void openBatchBuyWindowForModification(const QVector<XTS::Order> &orders);
  void openBatchSellWindowForModification(const QVector<XTS::Order> &orders);

  // ── Context-aware creation ───────────────────────────────────────────
  void createBuyWindowWithContext(const WindowContext &context,
                                  QWidget *initiatingWindow);
  void createSellWindowWithContext(const WindowContext &context,
                                   QWidget *initiatingWindow);
  void createSnapQuoteWindowWithContext(const WindowContext &context,
                                        QWidget *initiatingWindow);

  // ── Widget-aware creation (from CustomMDISubWindow F1/F2 fallback) ──
  Q_INVOKABLE void createBuyWindowFromWidget(QWidget *initiatingWidget);
  Q_INVOKABLE void createSellWindowFromWidget(QWidget *initiatingWidget);

  // ── Helpers ──────────────────────────────────────────────────────────
  int countWindowsOfType(const QString &type);
  void closeWindowsByType(const QString &type);
  void connectWindowSignals(CustomMDISubWindow *window);
  WindowContext getBestWindowContext() const;
  MarketWatchWindow *getActiveMarketWatch() const;

  // ── Script addition ──────────────────────────────────────────────────
  void onAddToWatchRequested(const InstrumentData &instrument);

private:
  MainWindow *m_mainWindow;
  CustomMDIArea *m_mdiArea;

  // XTS API clients (borrowed pointers, not owned)
  XTSMarketDataClient *m_xtsMarketDataClient = nullptr;
  XTSInteractiveClient *m_xtsInteractiveClient = nullptr;
  TradingDataService *m_tradingDataService = nullptr;
};

#endif // WINDOWFACTORY_H
