#include "views/MarketWatchWindow.h"
#include "models/TokenAddressBook.h"
#include "utils/LatencyTracker.h"
#include <iostream>

void MarketWatchWindow::updatePrice(int token, double ltp, double change, double changePercent)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    for (int row : rows) {
        m_model->updatePrice(row, ltp, change, changePercent);
    }
}

void MarketWatchWindow::updateVolume(int token, qint64 volume)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    for (int row : rows) {
        m_model->updateVolume(row, volume);
    }
}

void MarketWatchWindow::updateBidAsk(int token, double bid, double ask)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    for (int row : rows) {
        m_model->updateBidAsk(row, bid, ask);
    }
}

void MarketWatchWindow::updateOHLC(int token, double open, double high, double low, double close)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    for (int row : rows) {
        m_model->updateOHLC(row, open, high, low, close);
    }
}

void MarketWatchWindow::updateBidAskQuantities(int token, int bidQty, int askQty)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    for (int row : rows) {
        m_model->updateBidAskQuantities(row, bidQty, askQty);
    }
}

void MarketWatchWindow::updateTotalBuySellQty(int token, int totalBuyQty, int totalSellQty)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    for (int row : rows) {
        m_model->updateTotalBuySellQty(row, totalBuyQty, totalSellQty);
    }
}

void MarketWatchWindow::updateOpenInterest(int token, qint64 oi, double oiChangePercent)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    for (int row : rows) {
        m_model->updateOpenInterestWithChange(row, oi, oiChangePercent);
    }
}

void MarketWatchWindow::updateLastTradedQuantity(int token, qint64 ltq)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    for (int row : rows) {
        m_model->updateLastTradedQuantity(row, ltq);
    }
}

void MarketWatchWindow::onTickUpdate(const XTS::Tick& tick)
{
    int token = (int)tick.exchangeInstrumentID;
    int64_t timestampModelStart = LatencyTracker::now();
    
    // Debug logging for BSE tokens AND bid/ask data
    if (tick.exchangeSegment == 12 || tick.exchangeSegment == 11) {
        static int bseUpdateCount = 0;
        if (bseUpdateCount++ < 10) {
            int row = findTokenRow(token);
            qDebug() << "[MarketWatch] BSE Tick Update - Token:" << token 
                     << "LTP:" << tick.lastTradedPrice 
                     << "Bid:" << tick.bidPrice << "BidQty:" << tick.bidQuantity
                     << "Ask:" << tick.askPrice << "AskQty:" << tick.askQuantity
                     << "Row:" << row;
        }
    }
    
    // Debug: Log all bid/ask updates for first 100 ticks (extended for diagnosis)
    static int bidAskDebugCount = 0;
    if (bidAskDebugCount++ < 100) {
        qDebug() << "[MarketWatch] Tick" << bidAskDebugCount << "Token:" << token 
                 << "Segment:" << tick.exchangeSegment
                 << "Bid:" << tick.bidPrice << "(" << tick.bidQuantity << ")"
                 << "Ask:" << tick.askPrice << "(" << tick.askQuantity << ")";
    }
    
    // 1. Update LTP and OHLC if LTP is present (> 0)
    if (tick.lastTradedPrice > 0) {
        double change = 0;
        double changePercent = 0;
        
        // Find stored close price if tick doesn't have it
        double closePrice = tick.close;
        if (closePrice <= 0) {
            int row = findTokenRow(token);
            if (row >= 0) {
                closePrice = m_model->getScripAt(row).close;
            }
        }

        if (closePrice > 0) {
            change = tick.lastTradedPrice - closePrice;
            changePercent = (change / closePrice) * 100.0;
        }

        updatePrice(token, tick.lastTradedPrice, change, changePercent);
        
        if (tick.open > 0 || tick.high > 0 || tick.low > 0 || tick.close > 0) {
            updateOHLC(token, tick.open, tick.high, tick.low, tick.close);
        }
    }
    
    // 2. Update LTQ if present
    if (tick.lastTradedQuantity > 0) {
        updateLastTradedQuantity(token, tick.lastTradedQuantity);
    }

    // 3. Update Average Price if provided
    if (tick.averagePrice > 0) {
        QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
        for (int row : rows) {
            m_model->updateAveragePrice(row, tick.averagePrice);
        }
    }

    // 4. Update Volume if provided
    if (tick.volume > 0) {
        updateVolume(token, tick.volume);
    }
    
    // 5. Update Bid/Ask and Market Depth
    if (tick.bidPrice > 0 || tick.askPrice > 0) {
        updateBidAsk(token, tick.bidPrice, tick.askPrice);
        updateBidAskQuantities(token, tick.bidQuantity, tick.askQuantity);
    }
    // Note: BSE UDP broadcasts don't include bid/ask data
    // Bid/Ask for BSE instruments will be populated via getQuote API on scrip add
    
    // 6. Update Total Buy/Sell Qty
    if (tick.totalBuyQuantity > 0 || tick.totalSellQuantity > 0) {
        updateTotalBuySellQty(token, tick.totalBuyQuantity, tick.totalSellQuantity);
    }
    
    // 7. Update Open Interest
    if (tick.openInterest > 0) {
        updateOpenInterest(token, tick.openInterest, 0.0);
    }
    
    int64_t timestampModelEnd = LatencyTracker::now();
    
    if (tick.refNo > 0 && tick.timestampUdpRecv > 0) {
        LatencyTracker::recordLatency(
            tick.timestampUdpRecv,
            tick.timestampParsed,
            tick.timestampQueued,
            tick.timestampDequeued,
            tick.timestampFeedHandler,
            timestampModelStart,
            timestampModelEnd
        );
    }
}




int MarketWatchWindow::findTokenRow(int token) const
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    return rows.isEmpty() ? -1 : rows.first();
}

bool MarketWatchWindow::hasDuplicate(int token) const
{
    return m_tokenAddressBook->hasToken(token);
}
