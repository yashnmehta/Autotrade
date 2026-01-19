#include "views/MarketWatchWindow.h"
#include "models/TokenAddressBook.h"
#include "services/TokenSubscriptionManager.h"
#include "services/FeedHandler.h"
#include "data/PriceStoreGateway.h"
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
            }
        }
    }
    
    // Clear pointer map
    m_tokenUnifiedPointers.clear();
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

    // Setup timer for periodic zero-copy reads (100ms refresh)
    if (!m_zeroCopyUpdateTimer) {
        m_zeroCopyUpdateTimer = new QTimer(this);
        connect(m_zeroCopyUpdateTimer, &QTimer::timeout, 
                this, &MarketWatchWindow::onZeroCopyTimerUpdate);
        m_zeroCopyUpdateTimer->start(100); // 100ms = 10 updates/sec
    }
    
    qDebug() << "[MarketWatch] ✓ Zero-copy PriceCache mode configured with 100ms timer";
}


void MarketWatchWindow::onZeroCopyTimerUpdate()
{
    if (m_tokenUnifiedPointers.empty()) {
        return;
    }
    
    for (auto it = m_tokenUnifiedPointers.begin(); it != m_tokenUnifiedPointers.end(); ++it) {
        uint32_t token = it.key();
        const MarketData::UnifiedState* dataPtr = it.value();
        
        if (!dataPtr) continue;
        
        const MarketData::UnifiedState& data = *dataPtr;
        
        QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
        if (rows.isEmpty()) continue;
        
        // 1. Update LTP and change
        if (data.ltp > 0) {
            double ltp = data.ltp;
            double change = 0;
            double changePercent = 0;
            if (data.close > 0) {
                change = ltp - data.close;
                changePercent = (change / data.close) * 100.0;
            }
            
            for (int row : rows) {
                m_model->updatePrice(row, ltp, change, changePercent);
            }
        }
        
        // 2. Update OHLC
        if (data.open > 0 || data.high > 0 || data.low > 0) {
            for (int row : rows) {
                m_model->updateOHLC(row, data.open, data.high, data.low, data.close);
            }
        }
        
        // 3. Update volume
        if (data.volume > 0) {
            for (int row : rows) {
                m_model->updateVolume(row, data.volume);
            }
        }
        
        // 4. Update best bid/ask
        if (data.bids[0].price > 0 || data.asks[0].price > 0) {
            for (int row : rows) {
                m_model->updateBidAsk(row, data.bids[0].price, data.asks[0].price);
                m_model->updateBidAskQuantities(row, data.bids[0].quantity, data.asks[0].quantity);
            }
        }
        
        // 5. Update total buy/sell quantities
        if (data.totalBuyQty > 0 || data.totalSellQty > 0) {
            for (int row : rows) {
                m_model->updateTotalBuySellQty(row, data.totalBuyQty, data.totalSellQty);
            }
        }
        
        // 6. Update Open Interest
        if (data.openInterest > 0) {
            double oiChangePercent = 0.0;
            if (data.openInterestChange != 0) {
                oiChangePercent = (static_cast<double>(data.openInterestChange) / data.openInterest) * 100.0;
            }
            for (int row : rows) {
                m_model->updateOpenInterestWithChange(row, data.openInterest, oiChangePercent);
            }
        }
        
        // 7. Update LTQ
        if (data.lastTradeQty > 0) {
            for (int row : rows) {
                m_model->updateLastTradedQuantity(row, data.lastTradeQty);
            }
        }

        // 8. Average Traded Price
        if (data.avgPrice > 0) {
            for (int row : rows) {
                m_model->updateAveragePrice(row, data.avgPrice);
            }
        }
    }
}


