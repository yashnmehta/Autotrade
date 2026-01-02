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
#include "services/PriceCache.h"
#include "services/UdpBroadcastService.h"
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
            SnapQuoteWindow *snap = qobject_cast<SnapQuoteWindow*>(activeSub->contentWidget());
            if (snap) {
                WindowContext ctx = snap->getContext();
                if (ctx.isValid()) {
                    buyWindow = new BuyWindow(ctx, window);
                }
            }
        }
        if (!buyWindow) buyWindow = new BuyWindow(window);
    }
    
    window->setContentWidget(buyWindow);
    window->setMinimumWidth(1200);
    window->resize(1200, 200);
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
        }
        if (!sellWindow) sellWindow = new SellWindow(window);
    }
    
    window->setContentWidget(sellWindow);
    window->setMinimumWidth(1200);
    window->resize(1200, 200);
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
    }
    if (!snapWindow) snapWindow = new SnapQuoteWindow(window);
    
    if (m_xtsMarketDataClient) {
        snapWindow->setXTSClient(m_xtsMarketDataClient);
        if (snapWindow->getContext().isValid()) {
            snapWindow->fetchQuote();
        }
    }
    
    // Connect to UDP broadcast service for real-time tick updates
    connect(&UdpBroadcastService::instance(), &UdpBroadcastService::tickReceived,
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
        if (activeWindow) marketWatch = qobject_cast<MarketWatchWindow*>(activeWindow->contentWidget());
    }
    
    if (marketWatch) {
        // Simple add scrip for now (snippet had complex mapping, simplifying for compilation safety)
        marketWatch->addScrip(instrument.symbol, 
                             RepositoryManager::getExchangeSegmentName(instrument.exchangeSegment), 
                             (int)instrument.exchangeInstrumentID);

        
        // Apply cached price if available
        auto cached = PriceCache::instance().getPrice(instrument.exchangeInstrumentID);
        if (cached.has_value()) {
            double closePrice = cached->close;
            if (closePrice <= 0) {
                const ContractData* contract = RepositoryManager::getInstance()->getContractByToken(
                    RepositoryManager::getExchangeSegmentName(instrument.exchangeSegment), 
                    instrument.exchangeInstrumentID);
                if (contract) closePrice = contract->prevClose;
            }

            if (closePrice > 0) {
                double change = cached->lastTradedPrice - closePrice;
                double pct = (change / closePrice) * 100;
                marketWatch->updatePrice(instrument.exchangeInstrumentID, cached->lastTradedPrice, change, pct);
            } else {
                marketWatch->updatePrice(instrument.exchangeInstrumentID, cached->lastTradedPrice, 0, 0);
            }
        }
    }
}
