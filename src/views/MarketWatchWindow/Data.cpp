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
            // qDebug() << "[MarketWatch] BSE Tick Update - Token:" << token 
            //          << "LTP:" << tick.lastTradedPrice 
            //          << "Bid:" << tick.bidPrice << "BidQty:" << tick.bidQuantity
            //          << "Ask:" << tick.askPrice << "AskQty:" << tick.askQuantity
            //          << "Row:" << row;
        }
    }
    
    // Debug: Log all bid/ask updates for first 100 ticks (extended for diagnosis)
    static int bidAskDebugCount = 0;
    if (bidAskDebugCount++ < 100) {
        // qDebug() << "[MarketWatch] Tick" << bidAskDebugCount << "Token:" << token 
        //          << "Segment:" << tick.exchangeSegment
        //          << "Bid:" << tick.bidPrice << "(" << tick.bidQuantity << ")"
        //          << "Ask:" << tick.askPrice << "(" << tick.askQuantity << ")";
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

// NEW: UDP-specific tick handler with cleaner semantics
void MarketWatchWindow::onUdpTickUpdate(const UDP::MarketTick& tick)
{
    int token = tick.token;
    int64_t timestampModelStart = LatencyTracker::now();
    
    // Debug logging for BSE tokens
    if (tick.exchangeSegment == UDP::ExchangeSegment::BSEFO || 
        tick.exchangeSegment == UDP::ExchangeSegment::BSECM) {
        static int bseUpdateCount = 0;
        if (bseUpdateCount++ < 10) {
            int row = findTokenRow(token);
            // qDebug() << "[MarketWatch] BSE UDP Tick - Token:" << token 
            //          << "LTP:" << tick.ltp 
            //          << "Bid:" << tick.bestBid() << "(" << tick.bids[0].quantity << ")"
            //          << "Ask:" << tick.bestAsk() << "(" << tick.asks[0].quantity << ")"
            //          << "Row:" << row;
        }
    }
    
    // 1. Update LTP and OHLC if LTP is present
    if (tick.ltp > 0) {
        double change = 0;
        double changePercent = 0;
        
        // Use prevClose from UDP tick (explicit semantic)
        double closePrice = tick.prevClose;
        if (closePrice <= 0) {
            int row = findTokenRow(token);
            if (row >= 0) {
                closePrice = m_model->getScripAt(row).close;
            }
        }

        if (closePrice > 0) {
            change = tick.ltp - closePrice;
            changePercent = (change / closePrice) * 100.0;
        }

        updatePrice(token, tick.ltp, change, changePercent);
        
        // Update OHLC - prevClose is explicit in UDP
        if (tick.open > 0 || tick.high > 0 || tick.low > 0 || tick.prevClose > 0) {
            updateOHLC(token, tick.open, tick.high, tick.low, tick.prevClose);
        }
    }
    
    // 2. Update LTQ if present
    if (tick.ltq > 0) {
        updateLastTradedQuantity(token, tick.ltq);
    }

    // 3. Update Average Traded Price (ATP - explicit naming in UDP)
    if (tick.atp > 0) {
        QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
        for (int row : rows) {
            m_model->updateAveragePrice(row, tick.atp);
        }
    }

    // 4. Update Volume if provided
    if (tick.volume > 0) {
        updateVolume(token, tick.volume);
    }
    
    // 5. Update Bid/Ask from 5-level depth (best bid/ask from level 0)
    if (tick.bids[0].price > 0 || tick.asks[0].price > 0) {
        updateBidAsk(token, tick.bids[0].price, tick.asks[0].price);
        updateBidAskQuantities(token, (int)tick.bids[0].quantity, (int)tick.asks[0].quantity);
    }
    
    // 6. Update Total Buy/Sell Qty (aggregated from 5 levels)
    if (tick.totalBidQty > 0 || tick.totalAskQty > 0) {
        updateTotalBuySellQty(token, (int)tick.totalBidQty, (int)tick.totalAskQty);
    }
    
    // 7. Update Open Interest (for derivatives only)
    if (tick.isDerivative() && tick.openInterest > 0) {
        double oiChangePercent = 0.0;
        if (tick.oiChange != 0 && tick.openInterest > 0) {
            oiChangePercent = (static_cast<double>(tick.oiChange) / tick.openInterest) * 100.0;
        }
        updateOpenInterest(token, tick.openInterest, oiChangePercent);
    }
    
    int64_t timestampModelEnd = LatencyTracker::now();
    
    // Latency tracking
    if (tick.refNo > 0 && tick.timestampUdpRecv > 0) {
        LatencyTracker::recordLatency(
            tick.timestampUdpRecv,
            tick.timestampParsed,
            tick.timestampEmitted,
            0,  // No dequeue timestamp for UDP path
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
