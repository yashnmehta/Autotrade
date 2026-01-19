#include "app/MainWindow.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "views/MarketWatchWindow.h"
#include "views/OrderBookWindow.h"
#include "views/TradeBookWindow.h"
#include "views/PositionWindow.h"
#include "views/BuyWindow.h"
#include "views/SellWindow.h"
#include "views/SnapQuoteWindow.h"
#include "ui/OptionChainWindow.h"
#include "views/CustomizeDialog.h"
#include "app/ScripBar.h"
#include "services/UdpBroadcastService.h"
#include "data/PriceStoreGateway.h"
#include "repository/RepositoryManager.h"
#include <QDebug>
#include <QStatusBar>


// Helper to count windows
int MainWindow::countWindowsOfType(const QString& type)
{
    int count = 0;
    if (m_mdiArea) {
        for (auto window : m_mdiArea->windowList()) {
            if (window->windowType() == type) {
                count++;
            }
        }
    }
    return count;
}

// Helper to close windows by type
void MainWindow::closeWindowsByType(const QString& type)
{
    if (!m_mdiArea) return;
    
    QList<CustomMDISubWindow*> windows = m_mdiArea->windowList();
    for (auto window : windows) {
        if (window->windowType() == type) {
            window->close();
        }
    }
}

// Helper to connect signals
void MainWindow::connectWindowSignals(CustomMDISubWindow *window)
{
    if (!window) return;

    // Connect MDI area signals
    connect(window, &CustomMDISubWindow::closeRequested, window, &QWidget::close);
    connect(window, &CustomMDISubWindow::minimizeRequested, this, [this, window]() {
        m_mdiArea->minimizeWindow(window);
    });
    connect(window, &CustomMDISubWindow::maximizeRequested, window, &CustomMDISubWindow::maximize);
    connect(window, &CustomMDISubWindow::windowActivated, this, [this, window]() {
        m_mdiArea->activateWindow(window);
    });
    
    // Connect Customize signal
    connect(window, &CustomMDISubWindow::customizeRequested, this, [this, window]() {
        QString windowType = window->windowType();
        QWidget *targetWidget = window->contentWidget();
        
        CustomizeDialog dialog(windowType, targetWidget, this);
        dialog.exec();
    });

    // Auto-connect BaseOrderWindow signals (Buy/Sell)
    if (window->contentWidget() && (window->windowType() == "BuyWindow" || window->windowType() == "SellWindow")) {
        BaseOrderWindow *orderWin = qobject_cast<BaseOrderWindow*>(window->contentWidget());
        if (orderWin) {
            connect(orderWin, &BaseOrderWindow::orderSubmitted, this, &MainWindow::placeOrder);
        }
    }
}

void MainWindow::createMarketWatch()
{
    static int counter = 1;
    CustomMDISubWindow *window = new CustomMDISubWindow(QString("Market Watch %1").arg(counter++), m_mdiArea);
    window->setWindowType("MarketWatch");

    MarketWatchWindow *marketWatch = new MarketWatchWindow(window);
    marketWatch->setupZeroCopyMode();
    window->setContentWidget(marketWatch);
    window->resize(900, 400);

    connectWindowSignals(window);
    m_mdiArea->addWindow(window);
    
    window->setFocus();
    window->raise();
    window->activateWindow();
}

void MainWindow::createBuyWindow()
{
    // Enforce single order window limit: Only one Buy OR Sell window at a time
    closeWindowsByType("BuyWindow");
    closeWindowsByType("SellWindow");

    CustomMDISubWindow *window = new CustomMDISubWindow("Buy Order", m_mdiArea);
    window->setWindowType("BuyWindow");

    MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
    BuyWindow *buyWindow = nullptr;
    
    if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
        WindowContext context = activeMarketWatch->getSelectedContractContext();
        if (context.isValid()) {
            buyWindow = new BuyWindow(context, window);
        } else {
            buyWindow = new BuyWindow(window);
        }
    } else {
        CustomMDISubWindow *activeSub = m_mdiArea ? m_mdiArea->activeWindow() : nullptr;
        if (activeSub) {
            // Check for Snap Quote
            SnapQuoteWindow *snap = qobject_cast<SnapQuoteWindow*>(activeSub->contentWidget());
            if (snap) {
                WindowContext ctx = snap->getContext();
                if (ctx.isValid()) {
                    buyWindow = new BuyWindow(ctx, window);
                }
            } 
            // Check for Position Window
            else if (activeSub->windowType() == "PositionWindow") {
                PositionWindow *pw = qobject_cast<PositionWindow*>(activeSub->contentWidget());
                if (pw) {
                    WindowContext ctx = pw->getSelectedContext();
                    if (ctx.isValid()) {
                        buyWindow = new BuyWindow(ctx, window);
                    }
                }
            }
            // Check for Option Chain Window
            else if (activeSub->windowType() == "OptionChain") {
                OptionChainWindow *oc = qobject_cast<OptionChainWindow*>(activeSub->contentWidget());
                if (oc) {
                    WindowContext ctx = oc->getSelectedContext();
                    if (ctx.isValid()) {
                        buyWindow = new BuyWindow(ctx, window);
                    }
                }
            }
        }
        if (!buyWindow) buyWindow = new BuyWindow(window);
    }
    
    window->setContentWidget(buyWindow);
    // window->setMinimumWidth(1200);
    // window->setMinimumHeight(260);
    
    window->resize(1220, 260);
    connectWindowSignals(window);
    m_mdiArea->addWindow(window);
    window->activateWindow();
}

void MainWindow::createSellWindow()
{
    // Enforce single order window limit: Only one Buy OR Sell window at a time
    closeWindowsByType("BuyWindow");
    closeWindowsByType("SellWindow");

    CustomMDISubWindow *window = new CustomMDISubWindow("Sell Order", m_mdiArea);
    window->setWindowType("SellWindow");

    MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
    SellWindow *sellWindow = nullptr;
    
    if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
        WindowContext context = activeMarketWatch->getSelectedContractContext();
        if (context.isValid()) {
            sellWindow = new SellWindow(context, window);
        } else {
            sellWindow = new SellWindow(window);
        }
    } else {
        CustomMDISubWindow *activeSub = m_mdiArea ? m_mdiArea->activeWindow() : nullptr;
        if (activeSub) {
            SnapQuoteWindow *snap = qobject_cast<SnapQuoteWindow*>(activeSub->contentWidget());
            if (snap) {
                WindowContext ctx = snap->getContext();
                if (ctx.isValid()) {
                    sellWindow = new SellWindow(ctx, window);
                }
            }
            // Check for Position Window
            else if (activeSub->windowType() == "PositionWindow") {
                PositionWindow *pw = qobject_cast<PositionWindow*>(activeSub->contentWidget());
                if (pw) {
                    WindowContext ctx = pw->getSelectedContext();
                    if (ctx.isValid()) {
                        sellWindow = new SellWindow(ctx, window);
                    }
                }
            }
            // Check for Option Chain Window
            else if (activeSub->windowType() == "OptionChain") {
                OptionChainWindow *oc = qobject_cast<OptionChainWindow*>(activeSub->contentWidget());
                if (oc) {
                    WindowContext ctx = oc->getSelectedContext();
                    if (ctx.isValid()) {
                        sellWindow = new SellWindow(ctx, window);
                    }
                }
            }
        }
        if (!sellWindow) sellWindow = new SellWindow(window);
    }
    
    window->setContentWidget(sellWindow);
    // window->setMinimumWidth(1200);
    window->resize(1220, 260);
    connectWindowSignals(window);
    m_mdiArea->addWindow(window);
    window->activateWindow();
}

void MainWindow::createSnapQuoteWindow()
{
    // Use the helper to count existing SnapQuote windows
    int count = 0;
    QList<CustomMDISubWindow*> allWindows = m_mdiArea->windowList();
    for (auto win : allWindows) {
        qDebug() << "[MainWindow] Checking window:" << win->title() << "Type:" << win->windowType();
        if (win->windowType() == "SnapQuote") count++;
    }
    
    qDebug() << "[MainWindow] Total Snap Quote windows found:" << count;

    if (count >= 3) {
        if (m_statusBar) m_statusBar->showMessage("Maximum 3 Snap Quote windows allowed", 3000);
        return;
    }

    // Determine a unique index (1, 2, or 3)
    QSet<int> used;
    for (auto win : allWindows) {
        if (win->windowType() == "SnapQuote") {
            QString t = win->title();
            // Expecting "Snap Quote #"
            if (t.startsWith("Snap Quote ")) {
                used.insert(t.mid(11).toInt());
            } else if (t == "Snap Quote") {
                used.insert(0); // Old title format
            }
        }
    }
    int idx = 1;
    while (used.contains(idx)) idx++;

    QString title = QString("Snap Quote %1").arg(idx);
    qDebug() << "[MainWindow] Creating new Snap Quote window with title:" << title;
    
    CustomMDISubWindow *window = new CustomMDISubWindow(title, m_mdiArea);
    window->setWindowType("SnapQuote");

    MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
    SnapQuoteWindow *snapWindow = nullptr;
    
    if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
        WindowContext context = activeMarketWatch->getSelectedContractContext();
        if (context.isValid()) {
            snapWindow = new SnapQuoteWindow(context, window);
        }
    } else {
         CustomMDISubWindow *activeSub = m_mdiArea ? m_mdiArea->activeWindow() : nullptr;
         if (activeSub && activeSub->windowType() == "OptionChain") {
             OptionChainWindow *oc = qobject_cast<OptionChainWindow*>(activeSub->contentWidget());
             if (oc) {
                 WindowContext ctx = oc->getSelectedContext();
                 if (ctx.isValid()) {
                     snapWindow = new SnapQuoteWindow(ctx, window);
                 }
             }
         }
    }
    if (!snapWindow) snapWindow = new SnapQuoteWindow(window);
    
    if (m_xtsMarketDataClient) {
        snapWindow->setXTSClient(m_xtsMarketDataClient);
        if (snapWindow->getContext().isValid()) {
            snapWindow->fetchQuote();
        }
    }
    
    // Connect to UDP broadcast service for real-time tick updates
    connect(&UdpBroadcastService::instance(), &UdpBroadcastService::udpTickReceived,
            snapWindow, &SnapQuoteWindow::onTickUpdate);
    
    window->setContentWidget(snapWindow);
    window->resize(860, 300);
    connectWindowSignals(window);
    m_mdiArea->addWindow(window);
    window->activateWindow();
}

void MainWindow::createOptionChainWindow()
{
    CustomMDISubWindow *window = new CustomMDISubWindow("Option Chain", m_mdiArea);
    window->setWindowType("OptionChain");
    
    OptionChainWindow *optionWindow = new OptionChainWindow(window);
    // Logic to set symbol/expiry from context is simplified here
    
    window->setContentWidget(optionWindow);
    window->resize(1600, 800);
    connectWindowSignals(window);
    m_mdiArea->addWindow(window);
    window->activateWindow();
}

void MainWindow::createOrderBookWindow()
{
    if (countWindowsOfType("OrderBook") >= 5) return;
    CustomMDISubWindow *window = new CustomMDISubWindow("Order Book", m_mdiArea);
    window->setWindowType("OrderBook");
    OrderBookWindow *ob = new OrderBookWindow(m_tradingDataService, window);
    
    // Connect order modification signal - route to appropriate Buy/Sell window
    connect(ob, &OrderBookWindow::modifyOrderRequested, this, [this](const XTS::Order &order) {
        if (order.orderSide.toUpper() == "BUY") {
            openBuyWindowForModification(order);
        } else {
            openSellWindowForModification(order);
        }
    });

    // Connect Batch Modification signal
    connect(ob, &OrderBookWindow::batchModifyRequested, this, [this](const QVector<XTS::Order> &orders) {
        if (orders.isEmpty()) return;
        if (orders.first().orderSide.toUpper() == "BUY") {
            openBatchBuyWindowForModification(orders);
        } else {
            openBatchSellWindowForModification(orders);
        }
    });
    
    // Connect order cancellation signal
    connect(ob, &OrderBookWindow::cancelOrderRequested, this, &MainWindow::cancelOrder);
    
    window->setContentWidget(ob);
    window->resize(1400, 600);
    connectWindowSignals(window);
    m_mdiArea->addWindow(window);
    window->activateWindow();
}


void MainWindow::createTradeBookWindow()
{
    if (countWindowsOfType("TradeBook") >= 5) return;
    CustomMDISubWindow *window = new CustomMDISubWindow("Trade Book", m_mdiArea);
    window->setWindowType("TradeBook");
    TradeBookWindow *tb = new TradeBookWindow(m_tradingDataService, window);
    window->setContentWidget(tb);
    window->resize(1400, 600);
    connectWindowSignals(window);
    m_mdiArea->addWindow(window);
    window->activateWindow();
}

void MainWindow::createPositionWindow()
{
    if (countWindowsOfType("PositionWindow") >= 5) return;
    CustomMDISubWindow *window = new CustomMDISubWindow("Integrated Net Position", m_mdiArea);
    window->setWindowType("PositionWindow");
    PositionWindow *pw = new PositionWindow(m_tradingDataService, window);
    window->setContentWidget(pw);
    window->resize(1000, 500);
    connectWindowSignals(window);
    m_mdiArea->addWindow(window);
    window->activateWindow();
}

void MainWindow::onAddToWatchRequested(const InstrumentData &instrument)
{
    // Find active market watch window
    CustomMDISubWindow *activeWindow = m_mdiArea->activeWindow();
    MarketWatchWindow *marketWatch = nullptr;
    
    if (activeWindow && activeWindow->windowType() == "MarketWatch") {
        marketWatch = qobject_cast<MarketWatchWindow*>(activeWindow->contentWidget());
    } else {
        for (auto win : m_mdiArea->windowList()) {
            if (win->windowType() == "MarketWatch") {
                marketWatch = qobject_cast<MarketWatchWindow*>(win->contentWidget());
                if (marketWatch) { m_mdiArea->activateWindow(win); break; }
            }
        }
    }
    
    if (!marketWatch) {
        createMarketWatch();
        activeWindow = m_mdiArea->activeWindow();
        if (activeWindow) {
            marketWatch = qobject_cast<MarketWatchWindow*>(activeWindow->contentWidget());
            if (marketWatch) marketWatch->setupZeroCopyMode();
        }
    }
    
    if (marketWatch) {
        // Simple add scrip for now (snippet had complex mapping, simplifying for compilation safety)
        marketWatch->addScrip(instrument.symbol, 
                             RepositoryManager::getExchangeSegmentName(instrument.exchangeSegment), 
                             (int)instrument.exchangeInstrumentID);

        
        // Apply cached price if available from Distributed PriceStore
        auto unifiedState = MarketData::PriceStoreGateway::instance().getUnifiedState(instrument.exchangeSegment, instrument.exchangeInstrumentID);
        if (unifiedState) {
            double lastTradedPrice = unifiedState->ltp;
            double closePrice = unifiedState->close;
            if (closePrice <= 0) {
                const ContractData* contract = RepositoryManager::getInstance()->getContractByToken(
                    RepositoryManager::getExchangeSegmentName(instrument.exchangeSegment), 
                    instrument.exchangeInstrumentID);
                if (contract) closePrice = contract->prevClose;
            }

            if (closePrice > 0) {
                double change = lastTradedPrice - closePrice;
                double pct = (change / closePrice) * 100;
                marketWatch->updatePrice(instrument.exchangeInstrumentID, lastTradedPrice, change, pct);
            } else {
                marketWatch->updatePrice(instrument.exchangeInstrumentID, lastTradedPrice, 0, 0);
            }
        }
    }
}

void MainWindow::onRestoreWindowRequested(const QString &type, const QString &title, const QRect &geometry, bool isMinimized, bool isMaximized, bool isPinned, const QString &workspaceName, int index)
{
    qDebug() << "[MainWindow] Restoring window:" << type << title << "Index:" << index;

    // Create the window based on type
    if (type == "MarketWatch") {
        createMarketWatch();
    } else if (type == "BuyWindow") {
        createBuyWindow();
    } else if (type == "SellWindow") {
        createSellWindow();
    } else if (type == "SnapQuote" || type.startsWith("SnapQuote")) {
        createSnapQuoteWindow();
    } else if (type == "OptionChain") {
        createOptionChainWindow();
    } else if (type == "OrderBook") {
        createOrderBookWindow();
    } else if (type == "TradeBook") {
        createTradeBookWindow();
    } else if (type == "PositionWindow") {
        createPositionWindow();
    } else {
        qWarning() << "[MainWindow] Unknown window type for restore:" << type;
        return;
    }

    // Find the window we just created
    // Since createXWindow() functions activate the new window, it should be the active one.
    CustomMDISubWindow *window = m_mdiArea->activeWindow();
    if (!window) {
        qWarning() << "[MainWindow] Failed to find restored window for:" << type;
        return;
    }

    // Apply saved geometry
    if (!isMaximized && !isMinimized) {
         window->setGeometry(geometry);
    }
    
    // Apply state
    if (isMaximized) {
        window->maximize(); 
    } else if (isMinimized) {
        m_mdiArea->minimizeWindow(window);
    }

    window->setPinned(isPinned);
    
    if (!title.isEmpty() && window->title() != title) {
        window->setTitle(title);
    }
    
    // Restore detailed state (Script lists, column profiles, etc.)
    if (!workspaceName.isEmpty() && index >= 0 && window->contentWidget()) {
        QSettings settings("TradingCompany", "TradingTerminal");
        settings.beginGroup(QString("workspaces/%1/window_%2").arg(workspaceName).arg(index));
        
        if (type == "MarketWatch") {
            MarketWatchWindow *mw = qobject_cast<MarketWatchWindow*>(window->contentWidget());
            if (mw) {
                mw->setupZeroCopyMode();
                mw->restoreState(settings);
            }
        } else {
            // Try BaseBookWindow for order/trade/position windows
            BaseBookWindow *book = qobject_cast<BaseBookWindow*>(window->contentWidget());
            if (book) book->restoreState(settings);
        }
        
        settings.endGroup();
    }
}


