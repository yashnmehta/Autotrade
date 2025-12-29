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

void MarketWatchWindow::onTickUpdate(const XTS::Tick& tick)
{
    int token = (int)tick.exchangeInstrumentID;
    int64_t timestampModelStart = LatencyTracker::now();
    
    double change = tick.lastTradedPrice - tick.close;
    double changePercent = (tick.close != 0) ? (change / tick.close) * 100.0 : 0.0;
    
    updatePrice(token, tick.lastTradedPrice, change, changePercent);
    updateOHLC(token, tick.open, tick.high, tick.low, tick.close);
    
    if (tick.volume > 0) {
        updateVolume(token, tick.volume);
    }
    
    if (tick.bidPrice > 0 || tick.askPrice > 0) {
        updateBidAsk(token, tick.bidPrice, tick.askPrice);
        updateBidAskQuantities(token, tick.bidQuantity, tick.askQuantity);
    }
    
    if (tick.totalBuyQuantity > 0 || tick.totalSellQuantity > 0) {
        updateTotalBuySellQty(token, tick.totalBuyQuantity, tick.totalSellQuantity);
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
        
        static int trackedCount = 0;
        if (++trackedCount % 100 == 1) {
            LatencyTracker::printLatencyBreakdown(
                tick.refNo,
                token,
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
