#include "views/SnapQuoteWindow.h"
#include <QLabel>
#include <QLocale>
#include <QDateTime>

// Handle UDP tick updates
void SnapQuoteWindow::onTickUpdate(const XTS::Tick& tick)
{
    // Only process ticks for our token
    if (static_cast<int>(tick.exchangeInstrumentID) != m_token) return;
    
    // Update LTP
    if (tick.lastTradedPrice > 0 && m_lbLTPPrice) {
        double prevLtp = m_lbLTPPrice->text().toDouble();
        m_lbLTPPrice->setText(QString::number(tick.lastTradedPrice, 'f', 2));
        setLTPIndicator(tick.lastTradedPrice >= prevLtp);
    }
    
    // Update LTP quantity
    if (tick.lastTradedQuantity > 0 && m_lbLTPQty) {
        m_lbLTPQty->setText(QLocale().toString(tick.lastTradedQuantity));
    }
    
    // Update timestamp
    if (tick.lastUpdateTime > 0 && m_lbLTPTime) {
        QDateTime dt = QDateTime::fromSecsSinceEpoch(tick.lastUpdateTime);
        m_lbLTPTime->setText(dt.toString("HH:mm:ss"));
    }
    
    // Update OHLC
    if (tick.open > 0 && m_lbOpen) m_lbOpen->setText(QString::number(tick.open, 'f', 2));
    if (tick.high > 0 && m_lbHigh) m_lbHigh->setText(QString::number(tick.high, 'f', 2));
    if (tick.low > 0 && m_lbLow) m_lbLow->setText(QString::number(tick.low, 'f', 2));
    if (tick.close > 0 && m_lbClose) m_lbClose->setText(QString::number(tick.close, 'f', 2));
    
    // Update volume 
    if (tick.volume > 0 && m_lbVolume) {
        m_lbVolume->setText(QLocale().toString(static_cast<qint64>(tick.volume)));
    }
    
    // Update ATP
    if (tick.averagePrice > 0 && m_lbATP) {
        m_lbATP->setText(QString::number(tick.averagePrice, 'f', 2));
    }
    
    // Update percent change
    if (tick.close > 0 && tick.lastTradedPrice > 0 && m_lbPercentChange) {
        double pct = ((tick.lastTradedPrice - tick.close) / tick.close) * 100.0;
        m_lbPercentChange->setText(QString::number(pct, 'f', 2) + "%");
        setChangeColor(pct);
    }
    
    // Update OI
    if (tick.openInterest > 0 && m_lbOI) {
        m_lbOI->setText(QLocale().toString(static_cast<qint64>(tick.openInterest)));
    }
    
    // Update totals
    if (m_lbTotalBuyers) m_lbTotalBuyers->setText(QLocale().toString(tick.totalBuyQuantity));
    if (m_lbTotalSellers) m_lbTotalSellers->setText(QLocale().toString(tick.totalSellQuantity));
    
    // Update 5-level depth
    for (int i = 0; i < 5; i++) {
        updateBidDepth(i + 1, 
                       static_cast<int>(tick.bidDepth[i].quantity),
                       tick.bidDepth[i].price,
                       tick.bidDepth[i].orders);
        updateAskDepth(i + 1,
                       tick.askDepth[i].price,
                       static_cast<int>(tick.askDepth[i].quantity),
                       tick.askDepth[i].orders);
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
