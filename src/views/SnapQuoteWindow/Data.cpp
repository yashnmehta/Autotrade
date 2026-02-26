#include "views/SnapQuoteWindow.h"
#include "data/PriceStoreGateway.h"
#include <QLabel>
#include <QLocale>
#include <QDateTime>
#include <QTime>

// NSE epoch offset: seconds between 1970-01-01 and 1980-01-01
static constexpr int64_t NSE_EPOCH_OFFSET = 315532800;

// Convert NSE lastTradeTime (seconds since 1980-01-01) to display string
static QString nseTimeToString(uint32_t nseTime) {
    if (nseTime == 0) return QString();
    // NSE timestamps are IST-based, so use Qt::UTC to avoid double +5:30 offset
    QDateTime dt = QDateTime::fromSecsSinceEpoch(
        static_cast<int64_t>(nseTime) + NSE_EPOCH_OFFSET, Qt::UTC);
    return dt.toString("HH:mm:ss");
}

// Handle UDP tick updates
void SnapQuoteWindow::onTickUpdate(const UDP::MarketTick& tick)
{
    // Token check no longer needed - FeedHandler only sends subscribed tokens
    // if (static_cast<int>(tick.token) != m_token) return;
    
    // Update LTP
    if (tick.ltp > 0 && m_lbLTPPrice) {
        double prevLtp = m_lbLTPPrice->text().toDouble();
        m_lbLTPPrice->setText(QString::number(tick.ltp, 'f', 2));
        setLTPIndicator(tick.ltp >= prevLtp);
    }
    
    // Update LTP quantity
    if (tick.ltq > 0 && m_lbLTPQty) {
        m_lbLTPQty->setText(QLocale().toString((int)tick.ltq));
    }
    
    // Update timestamp - read lastTradeTime from unified price store
    if (m_lbLTPTime && m_subscribedToken > 0) {
        auto state = MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
            m_subscribedExchangeSegment, m_subscribedToken);
        if (state.lastTradeTime > 0) {
            // Debug: log raw value for token 59175
            if (m_subscribedToken == 59175) {
                static int logCount = 0;
                if (logCount++ < 5) {
                    qDebug() << "[SnapQuote] Token 59175 raw lastTradeTime:"
                             << state.lastTradeTime
                             << "converted:" << nseTimeToString(state.lastTradeTime);
                }
            }
            m_lbLTPTime->setText(nseTimeToString(state.lastTradeTime));
        }
    }
    
    // Update OHLC
    if (tick.open > 0 && m_lbOpen) m_lbOpen->setText(QString::number(tick.open, 'f', 2));
    if (tick.high > 0 && m_lbHigh) m_lbHigh->setText(QString::number(tick.high, 'f', 2));
    if (tick.low > 0 && m_lbLow) m_lbLow->setText(QString::number(tick.low, 'f', 2));
    if (tick.prevClose > 0 && m_lbClose) m_lbClose->setText(QString::number(tick.prevClose, 'f', 2));
    
    // Update volume 
    if (tick.volume > 0 && m_lbVolume) {
        m_lbVolume->setText(QLocale().toString(static_cast<qint64>(tick.volume)));
    }
    
    // Update ATP
    if (tick.atp > 0 && m_lbATP) {
        m_lbATP->setText(QString::number(tick.atp, 'f', 2));
    }
    
    // Update percent change
    if (tick.prevClose > 0 && tick.ltp > 0 && m_lbPercentChange) {
        double pct = ((tick.ltp - tick.prevClose) / tick.prevClose) * 100.0;
        m_lbPercentChange->setText(QString::number(pct, 'f', 2) + "%");
        setChangeColor(pct);
    }
    
    // Update OI
    if (tick.openInterest > 0 && m_lbOI) {
        m_lbOI->setText(QLocale().toString(static_cast<qint64>(tick.openInterest)));
    }
    
    // Update totals
    if (m_lbTotalBuyers) m_lbTotalBuyers->setText(QLocale().toString((int64_t)tick.totalBidQty));
    if (m_lbTotalSellers) m_lbTotalSellers->setText(QLocale().toString((int64_t)tick.totalAskQty));
    
    // Update 5-level depth
    for (int i = 0; i < 5; i++) {
        updateBidDepth(i + 1, 
                       static_cast<int>(tick.bids[i].quantity),
                       tick.bids[i].price,
                       tick.bids[i].orders);
        updateAskDepth(i + 1,
                       tick.asks[i].price,
                       static_cast<int>(tick.asks[i].quantity),
                       tick.asks[i].orders);
    }
}

void SnapQuoteWindow::updateQuote(double ltpPrice, int ltpQty, const QString &ltpTime,
                                  double open, double high, double low, double close,
                                  qint64 volume, double value, double atp, double percentChange)
{
    if (m_lbLTPPrice) m_lbLTPPrice->setText(QString::number(ltpPrice, 'f', 2));
    if (m_lbLTPQty) m_lbLTPQty->setText(QLocale().toString(ltpQty));
    if (m_lbLTPTime) m_lbLTPTime->setText(ltpTime);
    if (m_lbOpen) m_lbOpen->setText(QString::number(open, 'f', 2));
    if (m_lbHigh) m_lbHigh->setText(QString::number(high, 'f', 2));
    if (m_lbLow) m_lbLow->setText(QString::number(low, 'f', 2));
    if (m_lbClose) m_lbClose->setText(QString::number(close, 'f', 2));
    if (m_lbVolume) m_lbVolume->setText(QLocale().toString(volume));
    if (m_lbATP) m_lbATP->setText(QString::number(atp, 'f', 2));
    if (m_lbPercentChange) {
        m_lbPercentChange->setText(QString::number(percentChange, 'f', 2) + "%");
        setChangeColor(percentChange);
    }
}

void SnapQuoteWindow::updateStatistics(const QString &dpr, qint64 oi, double oiPercent,
                                       double gainLoss, double mtmValue, double mtmPos)
{
    if (m_lbDPR) m_lbDPR->setText(dpr);
    if (m_lbOI) m_lbOI->setText(QLocale().toString(oi));
    if (m_lbOIPercent) m_lbOIPercent->setText(QString::number(oiPercent, 'f', 2) + "%");
    if (m_lbGainLoss) m_lbGainLoss->setText(QString::number(gainLoss, 'f', 2));
    if (m_lbMTMValue) m_lbMTMValue->setText(QString::number(mtmValue, 'f', 2));
    if (m_lbMTMPos) m_lbMTMPos->setText(QString::number(mtmPos, 'f', 2));
}

void SnapQuoteWindow::updateBidDepth(int level, int qty, double price, int orders)
{
    QLabel *qtyLabel = nullptr;
    QLabel *priceLabel = nullptr;
    QLabel *ordersLabel = nullptr;
    
    switch (level) {
        case 1: qtyLabel = m_lbBidQty1; priceLabel = m_lbBidPrice1; ordersLabel = m_lbBidOrders1; break;
        case 2: qtyLabel = m_lbBidQty2; priceLabel = m_lbBidPrice2; ordersLabel = m_lbBidOrders2; break;
        case 3: qtyLabel = m_lbBidQty3; priceLabel = m_lbBidPrice3; ordersLabel = m_lbBidOrders3; break;
        case 4: qtyLabel = m_lbBidQty4; priceLabel = m_lbBidPrice4; ordersLabel = m_lbBidOrders4; break;
        case 5: qtyLabel = m_lbBidQty5; priceLabel = m_lbBidPrice5; ordersLabel = m_lbBidOrders5; break;
    }
    
    if (qtyLabel) qtyLabel->setText(QLocale().toString(qty));
    if (priceLabel) priceLabel->setText(price > 0 ? QString::number(price, 'f', 2) : "-");
    if (ordersLabel) ordersLabel->setText(QString::number(orders));
}

void SnapQuoteWindow::updateAskDepth(int level, double price, int qty, int orders)
{
    QLabel *qtyLabel = nullptr;
    QLabel *priceLabel = nullptr;
    QLabel *ordersLabel = nullptr;
    
    switch (level) {
        case 1: qtyLabel = m_lbAskQty1; priceLabel = m_lbAskPrice1; ordersLabel = m_lbAskOrders1; break;
        case 2: qtyLabel = m_lbAskQty2; priceLabel = m_lbAskPrice2; ordersLabel = m_lbAskOrders2; break;
        case 3: qtyLabel = m_lbAskQty3; priceLabel = m_lbAskPrice3; ordersLabel = m_lbAskOrders3; break;
        case 4: qtyLabel = m_lbAskQty4; priceLabel = m_lbAskPrice4; ordersLabel = m_lbAskOrders4; break;
        case 5: qtyLabel = m_lbAskQty5; priceLabel = m_lbAskPrice5; ordersLabel = m_lbAskOrders5; break;
    }
    
    if (qtyLabel) qtyLabel->setText(QLocale().toString(qty));
    if (priceLabel) priceLabel->setText(price > 0 ? QString::number(price, 'f', 2) : "-");
    if (ordersLabel) ordersLabel->setText(QString::number(orders));
}
