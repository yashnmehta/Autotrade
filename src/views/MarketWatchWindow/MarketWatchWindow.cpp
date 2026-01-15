#include "views/MarketWatchWindow.h"
#include "models/TokenAddressBook.h"
#include "services/TokenSubscriptionManager.h"
#include "services/FeedHandler.h"
#include "services/PriceCacheZeroCopy.h"
#include "utils/PreferencesManager.h"
#include "utils/WindowSettingsHelper.h"
#include "repository/RepositoryManager.h"
#include <QDebug>
#include <QKeyEvent>
#include <QTimer>
#include <QUuid>

MarketWatchWindow::MarketWatchWindow(QWidget *parent)
    : CustomMarketWatch(parent)
    , m_model(nullptr)
    , m_tokenAddressBook(nullptr)
    , m_xtsClient(nullptr)
    , m_useZeroCopyPriceCache(false)  // Will be loaded from preferences
    , m_zeroCopyUpdateTimer(nullptr)
{
    setupUI();
    setupConnections();
    setupKeyboardShortcuts();

    // Load and apply previous runtime state and customizations
    WindowSettingsHelper::loadAndApplyWindowSettings(this, "MarketWatch");
    
    // Load preference for PriceCache mode
    PreferencesManager& prefs = PreferencesManager::instance();
    m_useZeroCopyPriceCache = !prefs.getUseLegacyPriceCache(); // Inverted: false = legacy, true = zero-copy
    
    qDebug() << "[MarketWatch] PriceCache mode:" 
             << (m_useZeroCopyPriceCache ? "ZERO-COPY (New)" : "LEGACY (Old)");
}

MarketWatchWindow::~MarketWatchWindow()
{
    // Stop timer if using zero-copy mode
    if (m_zeroCopyUpdateTimer) {
        m_zeroCopyUpdateTimer->stop();
    }
    
    // Unsubscribe from FeedHandler (legacy mode)
    FeedHandler::instance().unsubscribeAll(this);
    
    // Unsubscribe from tokens (legacy mode)
    if (m_model) {
        for (int row = 0; row < m_model->rowCount(); ++row) {
            const ScripData &scrip = m_model->getScripAt(row);
            if (scrip.isValid()) {
                TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange, scrip.token);
                
                // Unsubscribe from new PriceCache if in zero-copy mode
                if (m_useZeroCopyPriceCache) {
                    PriceCacheTypes::MarketSegment segment = PriceCacheTypes::MarketSegment::NSE_CM;
                    if (scrip.exchange == "NSEFO") segment = PriceCacheTypes::MarketSegment::NSE_FO;
                    else if (scrip.exchange == "BSECM") segment = PriceCacheTypes::MarketSegment::BSE_CM;
                    else if (scrip.exchange == "BSEFO") segment = PriceCacheTypes::MarketSegment::BSE_FO;
                    
                    PriceCacheTypes::PriceCacheZeroCopy::getInstance().unsubscribe(scrip.token, segment);
                }
            }
        }
    }
    
    // Clear pointer map (no need to free memory - owned by PriceCache)
    m_tokenDataPointers.clear();
    m_pendingSubscriptions.clear();
}

void MarketWatchWindow::removeScripByToken(int token)
{
    int row = findTokenRow(token);
    if (row >= 0) removeScrip(row);
}

void MarketWatchWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        deleteSelectedRows();
        event->accept();
        return;
    }
    CustomMarketWatch::keyPressEvent(event);
}

int MarketWatchWindow::getTokenForRow(int sourceRow) const
{
    if (sourceRow < 0 || sourceRow >= m_model->rowCount()) return -1;
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    return scrip.isValid() ? scrip.token : -1;
}

bool MarketWatchWindow::isBlankRow(int sourceRow) const
{
    if (sourceRow < 0 || sourceRow >= m_model->rowCount()) return false;
    return m_model->isBlankRow(sourceRow);
}

WindowContext MarketWatchWindow::getSelectedContractContext() const
{
    WindowContext context;
    context.sourceWindow = "MarketWatch";
    
    QModelIndexList selection = selectionModel()->selectedRows();
    if (selection.isEmpty()) return context;
    
    QModelIndex lastIndex = selection.last();
    int sourceRow = mapToSource(lastIndex.row());
    if (sourceRow < 0 || sourceRow >= m_model->rowCount()) return context;
    
    context.sourceRow = sourceRow;
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    if (!scrip.isValid() || scrip.isBlankRow) return context;
    
    context.exchange = scrip.exchange;
    context.token = scrip.token;
    context.symbol = scrip.symbol;
    context.displayName = scrip.symbol;
    context.ltp = scrip.ltp;
    context.bid = scrip.bid;
    context.ask = scrip.ask;
    context.high = scrip.high;
    context.low = scrip.low;
    context.close = scrip.close;
    context.volume = scrip.volume;
    
    RepositoryManager *repo = RepositoryManager::getInstance();
    QString segment = (context.exchange == "NSEFO" || context.exchange == "BSEFO") ? "FO" : "CM";
    QString exchangeName = context.exchange.left(3);
    
    const ContractData *contract = repo->getContractByToken(exchangeName, segment, scrip.token);
    if (contract) {
        context.symbol = contract->name;
        context.lotSize = contract->lotSize;
        context.tickSize = contract->tickSize;
        context.freezeQty = contract->freezeQty;
        context.expiry = contract->expiryDate;
        context.strikePrice = contract->strikePrice;
        context.optionType = contract->optionType;
        context.instrumentType = contract->series;
        context.segment = (segment == "FO") ? "F" : "E";
    }
    
    return context;
}

bool MarketWatchWindow::hasValidSelection() const
{
    QModelIndexList selection = selectionModel()->selectedRows();
    for (const QModelIndex &index : selection) {
        int sourceRow = mapToSource(index.row());
        if (sourceRow >= 0 && sourceRow < m_model->rowCount()) {
            const ScripData &scrip = m_model->getScripAt(sourceRow);
            if (scrip.isValid() && !scrip.isBlankRow) return true;
        }
    }
    return false;
}

void MarketWatchWindow::onRowUpdated(int row, int firstColumn, int lastColumn) {
    // Map source row to proxy row (sorting/filtering)
    int proxyRow = mapToProxy(row);
    if (proxyRow < 0) return;

    // Use ultra-fast direct viewport update (bypasses Qt's signal system) ✅
    // Only update the rect of the affected cells for maximum performance
    QRect firstRect = visualRect(proxyModel()->index(proxyRow, firstColumn));
    QRect lastRect = visualRect(proxyModel()->index(proxyRow, lastColumn));
    QRect updateRect = firstRect.united(lastRect);
    
    if (updateRect.isValid()) {
        viewport()->update(updateRect);
    }
}
void MarketWatchWindow::onRowsInserted(int firstRow, int lastRow) { viewport()->update(); }
void MarketWatchWindow::onRowsRemoved(int firstRow, int lastRow) { viewport()->update(); }
void MarketWatchWindow::onModelReset() { viewport()->update(); }



void MarketWatchWindow::closeEvent(QCloseEvent *event) {
    WindowSettingsHelper::saveWindowSettings(this, "MarketWatch");
    QWidget::closeEvent(event);
}

// ============================================================================
// Zero-Copy PriceCache Implementation (New Mode)
// ============================================================================

void MarketWatchWindow::setupZeroCopyMode()
{
    if (!m_useZeroCopyPriceCache) return;

    // Setup zero-copy mode
    qDebug() << "[MarketWatch] Setting up Zero-Copy mode connections...";

    // Connect to PriceCache subscription signals
    // Safe now because QApplication exists
    connect(&PriceCacheTypes::PriceCacheZeroCopy::getInstance(), &PriceCacheTypes::PriceCacheZeroCopy::subscriptionReady,
            this, &MarketWatchWindow::onPriceCacheSubscriptionReady,
            Qt::QueuedConnection); // Thread-safe async delivery
    
    // Connect our request signal to FeedHandler
    connect(this, &MarketWatchWindow::requestTokenSubscription,
            &FeedHandler::instance(), &FeedHandler::requestPriceSubscription,
            Qt::QueuedConnection);
    
    // Setup timer for periodic zero-copy reads (100ms refresh)
    if (!m_zeroCopyUpdateTimer) {
        m_zeroCopyUpdateTimer = new QTimer(this);
        connect(m_zeroCopyUpdateTimer, &QTimer::timeout, 
                this, &MarketWatchWindow::onZeroCopyTimerUpdate);
        m_zeroCopyUpdateTimer->start(100); // 100ms = 10 updates/sec
    }
    
    qDebug() << "[MarketWatch] ✓ Zero-copy PriceCache mode configured with 100ms timer";
}

void MarketWatchWindow::onPriceCacheSubscriptionReady(
    QString requesterId,
    uint32_t token,
    PriceCacheTypes::MarketSegment segment,
    PriceCacheTypes::ConsolidatedMarketData* dataPointer,
    PriceCacheTypes::ConsolidatedMarketData snapshot,
    bool success,
    QString errorMessage)
{
    // Check if this response is for our request
    if (!m_pendingSubscriptions.contains(requesterId)) {
        return; // Not our request
    }
    
    uint32_t expectedToken = m_pendingSubscriptions.value(requesterId);
    m_pendingSubscriptions.remove(requesterId);
    
    if (expectedToken != token) {
        qWarning() << "[MarketWatch] Token mismatch in subscription response!"
                   << "Expected:" << expectedToken << "Got:" << token;
        return;
    }
    
    if (!success) {
        qWarning() << "[MarketWatch] ✗ Subscription failed for token" << token 
                   << "Error:" << errorMessage;
        // TODO: Show error to user or fallback to legacy mode
        return;
    }
    
    qDebug() << "[MarketWatch] ✓ Zero-copy subscription ready for token" << token
             << "Pointer:" << static_cast<void*>(dataPointer);
    
    // Store pointer for direct zero-copy reads
    m_tokenDataPointers[token] = dataPointer;
    
    // Use initial snapshot to update UI immediately
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    for (int row : rows) {
        // Update LTP (convert paise to rupees)
        if (snapshot.lastTradedPrice > 0) {
            double ltp = snapshot.lastTradedPrice / 100.0;
            double change = 0;
            double changePercent = 0;
            if (snapshot.closePrice > 0) {
                double close = snapshot.closePrice / 100.0;
                change = ltp - close;
                changePercent = (change / close) * 100.0;
            }
            m_model->updatePrice(row, ltp, change, changePercent);
        }
        
        // Update OHLC (convert paise to rupees)
        if (snapshot.openPrice > 0 || snapshot.highPrice > 0 || snapshot.lowPrice > 0) {
            m_model->updateOHLC(row, 
                snapshot.openPrice / 100.0, 
                snapshot.highPrice / 100.0, 
                snapshot.lowPrice / 100.0, 
                snapshot.closePrice / 100.0);
        }
        
        // Update volume
        if (snapshot.volumeTradedToday > 0) {
            m_model->updateVolume(row, snapshot.volumeTradedToday);
        }
        
        // Update bid/ask (use best level - index 0, convert paise to rupees)
        if (snapshot.bidPrice[0] > 0 || snapshot.askPrice[0] > 0) {
            m_model->updateBidAsk(row, snapshot.bidPrice[0] / 100.0, snapshot.askPrice[0] / 100.0);
        }
        
        // Note: openInterest not in ConsolidatedMarketData structure yet
        // TODO: Add openInterest field when implementing F&O support
    }
    
    qDebug() << "[MarketWatch] Initial snapshot applied for token" << token 
             << "LTP:" << (snapshot.lastTradedPrice / 100.0) << "Volume:" << snapshot.volumeTradedToday;
}

void MarketWatchWindow::onZeroCopyTimerUpdate()
{
    // This method is called every 100ms to read data directly from memory
    // Zero-copy: No function calls, no copies, just direct pointer dereferencing
    
    if (m_tokenDataPointers.empty()) {
        return; // No subscriptions yet
    }
    
    // Read all subscribed tokens (ultra-fast direct memory access)
    // Iterate using iterator for QMap
    for (auto it = m_tokenDataPointers.begin(); it != m_tokenDataPointers.end(); ++it) {
        uint32_t token = it.key();
        PriceCacheTypes::ConsolidatedMarketData* dataPtr = it.value();
        
        if (!dataPtr) continue; // Safety check
        
        // Zero-copy read! Direct memory access (< 100ns)
        const PriceCacheTypes::ConsolidatedMarketData& data = *dataPtr;
        
        // Get rows for this token
        QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
        if (rows.isEmpty()) continue;
        
        // Update LTP and change (convert paise to rupees)
        if (data.lastTradedPrice > 0) {
            double ltp = data.lastTradedPrice / 100.0; // Convert paise to rupees
            double change = 0;
            double changePercent = 0;
            if (data.closePrice > 0) {
                double close = data.closePrice / 100.0;
                change = ltp - close;
                changePercent = (change / close) * 100.0;
            }
            
            for (int row : rows) {
                m_model->updatePrice(row, ltp, change, changePercent);
            }
        }
        
        // Update OHLC (convert paise to rupees)
        if (data.openPrice > 0 || data.highPrice > 0 || data.lowPrice > 0) {
            for (int row : rows) {
                m_model->updateOHLC(row, 
                    data.openPrice / 100.0, 
                    data.highPrice / 100.0, 
                    data.lowPrice / 100.0, 
                    data.closePrice / 100.0);
            }
        }
        
        // Update volume
        if (data.volumeTradedToday > 0) {
            for (int row : rows) {
                m_model->updateVolume(row, data.volumeTradedToday);
            }
        }
        
        // Update bid/ask (use best level - index 0, convert paise to rupees)
        if (data.bidPrice[0] > 0 || data.askPrice[0] > 0) {
            for (int row : rows) {
                m_model->updateBidAsk(row, data.bidPrice[0] / 100.0, data.askPrice[0] / 100.0);
            }
        }
        
        // Update bid/ask quantities (use best level - index 0)
        if (data.bidQuantity[0] > 0 || data.askQuantity[0] > 0) {
            for (int row : rows) {
                m_model->updateBidAskQuantities(row, data.bidQuantity[0], data.askQuantity[0]);
            }
        }
        
        // Update total buy/sell quantities
        if (data.totalBuyQuantity > 0 || data.totalSellQuantity > 0) {
            for (int row : rows) {
                m_model->updateTotalBuySellQty(row, data.totalBuyQuantity, data.totalSellQuantity);
            }
        }
        
        // Note: openInterest not in ConsolidatedMarketData structure yet
        // TODO: Add openInterest field when implementing F&O support
        
        // Update LTQ
        if (data.lastTradeQuantity > 0) {
            for (int row : rows) {
                m_model->updateLastTradedQuantity(row, data.lastTradeQuantity);
            }
        }
    }
}


