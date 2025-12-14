#include "ui/SnapQuoteWindow.h"
#include <QVBoxLayout>
#include <QMessageBox>

SnapQuoteWindow::SnapQuoteWindow(QWidget *parent)
    : QWidget(parent)
    , m_formWidget(nullptr)
    , m_token(0)
{
    // Load UI file
    QUiLoader loader;
    QFile file(":/forms/SnapQuote.ui");
    
    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "[SnapQuoteWindow] Failed to open UI file";
        return;
    }
    
    m_formWidget = loader.load(&file, this);
    file.close();
    
    if (!m_formWidget) {
        qWarning() << "[SnapQuoteWindow] Failed to load UI";
        return;
    }
    
    // Set layout
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_formWidget);
    
    // Find header widgets
    m_cbEx = m_formWidget->findChild<QComboBox*>("cbEx");
    m_cbSegment = m_formWidget->findChild<QComboBox*>("cbSegment");
    m_leToken = m_formWidget->findChild<QLineEdit*>("leToken");
    m_leInstType = m_formWidget->findChild<QLineEdit*>("leInstType");
    m_leSymbol = m_formWidget->findChild<QLineEdit*>("leSymbol");
    m_cbExpiry = m_formWidget->findChild<QComboBox*>("cbExpiry");
    m_pbRefresh = m_formWidget->findChild<QPushButton*>("pbRefresh");
    
    // Find LTP widgets
    m_lbLTPQty = m_formWidget->findChild<QLabel*>("lbLTPQty");
    m_lbLTPPrice = m_formWidget->findChild<QLabel*>("lbLTPPrice");
    m_lbLTPIndicator = m_formWidget->findChild<QLabel*>("lbLTPIndicator");
    m_lbLTPTime = m_formWidget->findChild<QLabel*>("lbLTPTime");
    
    // Find market statistics widgets
    m_lbVolume = m_formWidget->findChild<QLabel*>("lbVolume");
    m_lbValue = m_formWidget->findChild<QLabel*>("lbValue");
    m_lbATP = m_formWidget->findChild<QLabel*>("lbATP");
    m_lbPercentChange = m_formWidget->findChild<QLabel*>("lbPercentChange");
    
    // Find price data widgets
    m_lbOpen = m_formWidget->findChild<QLabel*>("lbOpen");
    m_lbHigh = m_formWidget->findChild<QLabel*>("lbHigh");
    m_lbLow = m_formWidget->findChild<QLabel*>("lbLow");
    m_lbClose = m_formWidget->findChild<QLabel*>("lbClose");
    
    // Find additional statistics widgets
    m_lbDPR = m_formWidget->findChild<QLabel*>("lbDPR");
    m_lbOI = m_formWidget->findChild<QLabel*>("lbOI");
    m_lbOIPercent = m_formWidget->findChild<QLabel*>("lbOIPercent");
    m_lbGainLoss = m_formWidget->findChild<QLabel*>("lbGainLoss");
    m_lbMTMValue = m_formWidget->findChild<QLabel*>("lbMTMValue");
    m_lbMTMPos = m_formWidget->findChild<QLabel*>("lbMTMPos");
    
    // Find bid depth widgets
    m_lbBidQty1 = m_formWidget->findChild<QLabel*>("lbBidQty1");
    m_lbBidQty2 = m_formWidget->findChild<QLabel*>("lbBidQty2");
    m_lbBidQty3 = m_formWidget->findChild<QLabel*>("lbBidQty3");
    m_lbBidQty4 = m_formWidget->findChild<QLabel*>("lbBidQty4");
    m_lbBidQty5 = m_formWidget->findChild<QLabel*>("lbBidQty5");
    
    m_lbBidPrice1 = m_formWidget->findChild<QLabel*>("lbBidPrice1");
    m_lbBidPrice2 = m_formWidget->findChild<QLabel*>("lbBidPrice2");
    m_lbBidPrice3 = m_formWidget->findChild<QLabel*>("lbBidPrice3");
    m_lbBidPrice4 = m_formWidget->findChild<QLabel*>("lbBidPrice4");
    m_lbBidPrice5 = m_formWidget->findChild<QLabel*>("lbBidPrice5");
    
    // Find ask depth widgets
    m_lbAskPrice1 = m_formWidget->findChild<QLabel*>("lbAskPrice1");
    m_lbAskPrice2 = m_formWidget->findChild<QLabel*>("lbAskPrice2");
    m_lbAskPrice3 = m_formWidget->findChild<QLabel*>("lbAskPrice3");
    m_lbAskPrice4 = m_formWidget->findChild<QLabel*>("lbAskPrice4");
    m_lbAskPrice5 = m_formWidget->findChild<QLabel*>("lbAskPrice5");
    
    m_lbAskQty1 = m_formWidget->findChild<QLabel*>("lbAskQty1");
    m_lbAskQty2 = m_formWidget->findChild<QLabel*>("lbAskQty2");
    m_lbAskQty3 = m_formWidget->findChild<QLabel*>("lbAskQty3");
    m_lbAskQty4 = m_formWidget->findChild<QLabel*>("lbAskQty4");
    m_lbAskQty5 = m_formWidget->findChild<QLabel*>("lbAskQty5");
    
    m_lbAskOrders1 = m_formWidget->findChild<QLabel*>("lbAskOrders1");
    m_lbAskOrders2 = m_formWidget->findChild<QLabel*>("lbAskOrders2");
    m_lbAskOrders3 = m_formWidget->findChild<QLabel*>("lbAskOrders3");
    m_lbAskOrders4 = m_formWidget->findChild<QLabel*>("lbAskOrders4");
    m_lbAskOrders5 = m_formWidget->findChild<QLabel*>("lbAskOrders5");
    
    populateComboBoxes();
    setupConnections();
    
    qDebug() << "[SnapQuoteWindow] Created successfully";
}

SnapQuoteWindow::~SnapQuoteWindow()
{
}

void SnapQuoteWindow::populateComboBoxes()
{
    // Populate Exchange combo
    if (m_cbEx) {
        m_cbEx->addItems({"NSE", "BSE", "MCX"});
    }
    
    // Populate Segment combo
    if (m_cbSegment) {
        m_cbSegment->addItems({"F", "C", "D"});
    }
    
    // Populate Expiry combo (sample dates)
    if (m_cbExpiry) {
        m_cbExpiry->addItems({
            "19-Dec-2024", "26-Dec-2024", "02-Jan-2025", "09-Jan-2025",
            "16-Jan-2025", "23-Jan-2025", "30-Jan-2025", "06-Feb-2025",
            "13-Feb-2025", "27-Feb-2025", "27-Mar-2025", "26-Jun-2025"
        });
    }
}

void SnapQuoteWindow::setupConnections()
{
    if (m_pbRefresh) {
        connect(m_pbRefresh, &QPushButton::clicked, this, &SnapQuoteWindow::onRefreshClicked);
    }
}

void SnapQuoteWindow::setScripDetails(const QString &exchange, const QString &segment,
                                      int token, const QString &instType, const QString &symbol)
{
    m_exchange = exchange;
    m_segment = segment;
    m_token = token;
    m_symbol = symbol;
    
    if (m_cbEx) {
        int idx = m_cbEx->findText(exchange);
        if (idx >= 0) m_cbEx->setCurrentIndex(idx);
    }
    
    if (m_cbSegment) {
        int idx = m_cbSegment->findText(segment);
        if (idx >= 0) m_cbSegment->setCurrentIndex(idx);
    }
    
    if (m_leToken) {
        m_leToken->setText(QString::number(token));
    }
    
    if (m_leInstType) {
        m_leInstType->setText(instType);
    }
    
    if (m_leSymbol) {
        m_leSymbol->setText(symbol);
    }
    
    qDebug() << "[SnapQuoteWindow] Set scrip:" << exchange << segment << token << instType << symbol;
}

void SnapQuoteWindow::updateQuote(double ltpPrice, int ltpQty, const QString &ltpTime,
                                  double open, double high, double low, double close,
                                  qint64 volume, double value, double atp, double percentChange)
{
    // Update LTP section
    if (m_lbLTPPrice) {
        m_lbLTPPrice->setText(QString::number(ltpPrice, 'f', 2));
    }
    
    if (m_lbLTPQty) {
        m_lbLTPQty->setText(QString::number(ltpQty));
    }
    
    if (m_lbLTPTime) {
        m_lbLTPTime->setText(ltpTime);
    }
    
    // Update price data
    if (m_lbOpen) {
        m_lbOpen->setText(QString::number(open, 'f', 2));
    }
    
    if (m_lbHigh) {
        m_lbHigh->setText(QString::number(high, 'f', 2));
    }
    
    if (m_lbLow) {
        m_lbLow->setText(QString::number(low, 'f', 2));
    }
    
    if (m_lbClose) {
        m_lbClose->setText(QString::number(close, 'f', 2));
    }
    
    // Update market statistics
    if (m_lbVolume) {
        m_lbVolume->setText(QString::number(volume));
    }
    
    if (m_lbValue) {
        m_lbValue->setText(QString::number(value, 'f', 2));
    }
    
    if (m_lbATP) {
        m_lbATP->setText(QString::number(atp, 'f', 2));
    }
    
    if (m_lbPercentChange) {
        m_lbPercentChange->setText(QString::number(percentChange, 'f', 2));
        setChangeColor(percentChange);
    }
    
    // Set LTP indicator based on price change
    double change = ltpPrice - close;
    setLTPIndicator(change > 0);
}

void SnapQuoteWindow::updateStatistics(const QString &dpr, qint64 oi, double oiPercent,
                                       double gainLoss, double mtmValue, double mtmPos)
{
    if (m_lbDPR) {
        m_lbDPR->setText(dpr);
    }
    
    if (m_lbOI) {
        m_lbOI->setText(QString::number(oi));
    }
    
    if (m_lbOIPercent) {
        m_lbOIPercent->setText(QString::number(oiPercent, 'f', 2));
    }
    
    if (m_lbGainLoss) {
        m_lbGainLoss->setText(QString::number(gainLoss, 'f', 2));
        // Color based on gain/loss
        if (gainLoss > 0) {
            m_lbGainLoss->setStyleSheet("color: #2ECC71; font-weight: bold;");
        } else if (gainLoss < 0) {
            m_lbGainLoss->setStyleSheet("color: #E74C3C; font-weight: bold;");
        } else {
            m_lbGainLoss->setStyleSheet("color: #F0F0F0; font-weight: bold;");
        }
    }
    
    if (m_lbMTMValue) {
        m_lbMTMValue->setText(QString::number(mtmValue, 'f', 2));
    }
    
    if (m_lbMTMPos) {
        m_lbMTMPos->setText(QString::number(mtmPos, 'f', 2));
    }
}

void SnapQuoteWindow::updateBidDepth(int level, int qty, double price)
{
    QLabel *qtyLabel = nullptr;
    QLabel *priceLabel = nullptr;
    
    switch (level) {
        case 1: qtyLabel = m_lbBidQty1; priceLabel = m_lbBidPrice1; break;
        case 2: qtyLabel = m_lbBidQty2; priceLabel = m_lbBidPrice2; break;
        case 3: qtyLabel = m_lbBidQty3; priceLabel = m_lbBidPrice3; break;
        case 4: qtyLabel = m_lbBidQty4; priceLabel = m_lbBidPrice4; break;
        case 5: qtyLabel = m_lbBidQty5; priceLabel = m_lbBidPrice5; break;
        default: return;
    }
    
    if (qtyLabel) {
        qtyLabel->setText(QString::number(qty));
    }
    
    if (priceLabel) {
        priceLabel->setText(QString::number(price, 'f', 2));
    }
}

void SnapQuoteWindow::updateAskDepth(int level, double price, int qty, int orders)
{
    QLabel *priceLabel = nullptr;
    QLabel *qtyLabel = nullptr;
    QLabel *ordersLabel = nullptr;
    
    switch (level) {
        case 1: 
            priceLabel = m_lbAskPrice1;
            qtyLabel = m_lbAskQty1;
            ordersLabel = m_lbAskOrders1;
            break;
        case 2:
            priceLabel = m_lbAskPrice2;
            qtyLabel = m_lbAskQty2;
            ordersLabel = m_lbAskOrders2;
            break;
        case 3:
            priceLabel = m_lbAskPrice3;
            qtyLabel = m_lbAskQty3;
            ordersLabel = m_lbAskOrders3;
            break;
        case 4:
            priceLabel = m_lbAskPrice4;
            qtyLabel = m_lbAskQty4;
            ordersLabel = m_lbAskOrders4;
            break;
        case 5:
            priceLabel = m_lbAskPrice5;
            qtyLabel = m_lbAskQty5;
            ordersLabel = m_lbAskOrders5;
            break;
        default: return;
    }
    
    if (priceLabel) {
        priceLabel->setText(QString::number(price, 'f', 2));
    }
    
    if (qtyLabel) {
        qtyLabel->setText(QString::number(qty));
    }
    
    if (ordersLabel) {
        ordersLabel->setText(QString::number(orders));
    }
}

void SnapQuoteWindow::setLTPIndicator(bool isUp)
{
    if (m_lbLTPIndicator) {
        if (isUp) {
            m_lbLTPIndicator->setText("▲");
            m_lbLTPIndicator->setStyleSheet("font-size: 16px; color: #2ECC71;");
        } else {
            m_lbLTPIndicator->setText("▼");
            m_lbLTPIndicator->setStyleSheet("font-size: 16px; color: #E74C3C;");
        }
    }
}

void SnapQuoteWindow::setChangeColor(double change)
{
    if (m_lbPercentChange) {
        if (change > 0) {
            m_lbPercentChange->setStyleSheet("color: #2ECC71; font-weight: bold;");
        } else if (change < 0) {
            m_lbPercentChange->setStyleSheet("color: #E74C3C; font-weight: bold;");
        } else {
            m_lbPercentChange->setStyleSheet("color: #F0F0F0; font-weight: bold;");
        }
    }
}

void SnapQuoteWindow::onRefreshClicked()
{
    emit refreshRequested(m_exchange, m_token);
    qDebug() << "[SnapQuoteWindow] Refresh requested for" << m_exchange << m_token;
}
