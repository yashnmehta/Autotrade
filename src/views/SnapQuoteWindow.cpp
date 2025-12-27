#include "views/SnapQuoteWindow.h"
#include "api/XTSMarketDataClient.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

SnapQuoteWindow::SnapQuoteWindow(QWidget *parent)
    : QWidget(parent)
    , m_formWidget(nullptr)
    , m_token(0)
    , m_xtsClient(nullptr)
{
    initUI();
}

SnapQuoteWindow::SnapQuoteWindow(const WindowContext &context, QWidget *parent)
    : QWidget(parent)
    , m_formWidget(nullptr)
    , m_token(0)
    , m_context(context)
    , m_xtsClient(nullptr)
{
    initUI();
    loadFromContext(context);
}

void SnapQuoteWindow::initUI()
{
    // Load UI file using QUiLoader
    QUiLoader loader;
    QFile file(":/forms/SnapQuote.ui");
    
    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "[SnapQuoteWindow] Failed to open UI file";
        return;
    }
    
    m_formWidget = loader.load(&file);
    file.close();
    
    if (!m_formWidget) {
        qWarning() << "[SnapQuoteWindow] ERROR: Failed to load UI widget from :/forms/SnapQuote.ui. Check if file is valid XML.";
        return;
    }
    
    // Main layout for this widget to host the loaded form
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_formWidget);
    
    qDebug() << "[SnapQuoteWindow] UI loaded successfully. Root widget name:" << m_formWidget->objectName();
    
    // --- Header Widgets ---
    m_cbEx = m_formWidget->findChild<QComboBox*>("cbEx");
    m_cbSegment = m_formWidget->findChild<QComboBox*>("cbSegment");
    m_leToken = m_formWidget->findChild<QLineEdit*>("leToken");
    m_leInstType = m_formWidget->findChild<QLineEdit*>("leInstType");
    m_leSymbol = m_formWidget->findChild<QLineEdit*>("leSymbol");
    m_cbExpiry = m_formWidget->findChild<QComboBox*>("cbExpiry");
    m_pbRefresh = m_formWidget->findChild<QPushButton*>("pbRefresh");
    
    // --- Market Data Widgets ---
    m_lbLTPQty = m_formWidget->findChild<QLabel*>("lbLTPQty");
    m_lbLTPPrice = m_formWidget->findChild<QLabel*>("lbLTPPrice");
    m_lbLTPIndicator = m_formWidget->findChild<QLabel*>("lbLTPIndicator");
    m_lbLTPTime = m_formWidget->findChild<QLabel*>("lbLTPTime");
    
    m_lbOpen = m_formWidget->findChild<QLabel*>("lbOpen");
    m_lbHigh = m_formWidget->findChild<QLabel*>("lbHigh");
    m_lbLow = m_formWidget->findChild<QLabel*>("lbLow");
    m_lbClose = m_formWidget->findChild<QLabel*>("lbClose");
    
    m_lbVolume = m_formWidget->findChild<QLabel*>("lbVolume");
    m_lbValue = m_formWidget->findChild<QLabel*>("lbValue");
    m_lbATP = m_formWidget->findChild<QLabel*>("lbATP");
    m_lbPercentChange = m_formWidget->findChild<QLabel*>("lbPercentChange");
    
    // --- Stats Widgets ---
    m_lbDPR = m_formWidget->findChild<QLabel*>("lbDPR");
    m_lbOI = m_formWidget->findChild<QLabel*>("lbOI");
    m_lbOIPercent = m_formWidget->findChild<QLabel*>("lbOIPercent");
    m_lbGainLoss = m_formWidget->findChild<QLabel*>("lbGainLoss");
    m_lbMTMValue = m_formWidget->findChild<QLabel*>("lbMTMValue");
    m_lbMTMPos = m_formWidget->findChild<QLabel*>("lbMTMPos");
    
    // --- Depth: Bid Widgets ---
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
    
    m_lbBidOrders1 = m_formWidget->findChild<QLabel*>("bidAt1");
    m_lbBidOrders2 = m_formWidget->findChild<QLabel*>("bidAt2");
    m_lbBidOrders3 = m_formWidget->findChild<QLabel*>("bidAt3");
    m_lbBidOrders4 = m_formWidget->findChild<QLabel*>("bidAt4");
    m_lbBidOrders5 = m_formWidget->findChild<QLabel*>("bidAt5");
    
    // --- Depth: Ask Widgets ---
    m_lbAskQty1 = m_formWidget->findChild<QLabel*>("lbAskQty1");
    m_lbAskQty2 = m_formWidget->findChild<QLabel*>("lbAskQty2");
    m_lbAskQty3 = m_formWidget->findChild<QLabel*>("lbAskQty3");
    m_lbAskQty4 = m_formWidget->findChild<QLabel*>("lbAskQty4");
    m_lbAskQty5 = m_formWidget->findChild<QLabel*>("lbAskQty5");
    
    m_lbAskPrice1 = m_formWidget->findChild<QLabel*>("lbAskPrice1");
    m_lbAskPrice2 = m_formWidget->findChild<QLabel*>("lbAskPrice2");
    m_lbAskPrice3 = m_formWidget->findChild<QLabel*>("lbAskPrice3");
    m_lbAskPrice4 = m_formWidget->findChild<QLabel*>("lbAskPrice4");
    m_lbAskPrice5 = m_formWidget->findChild<QLabel*>("lbAskPrice5");
    
    m_lbAskOrders1 = m_formWidget->findChild<QLabel*>("lbAskOrders1");
    m_lbAskOrders2 = m_formWidget->findChild<QLabel*>("lbAskOrders2");
    m_lbAskOrders3 = m_formWidget->findChild<QLabel*>("lbAskOrders3");
    m_lbAskOrders4 = m_formWidget->findChild<QLabel*>("lbAskOrders4");
    m_lbAskOrders5 = m_formWidget->findChild<QLabel*>("lbAskOrders5");

    m_lbTotalBuyers = m_formWidget->findChild<QLabel*>("lb_allBuyers");
    m_lbTotalSellers = m_formWidget->findChild<QLabel*>("lb_allSellers");
    
    populateComboBoxes();
    setupConnections();
    
    // Esc shortcut to close parent MDI window
    m_escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(m_escShortcut, &QShortcut::activated, this, [this]() {
        QWidget *p = parentWidget();
        while (p) {
            if (p->metaObject()->className() == QString("CustomMDISubWindow")) {
                p->close();
                return;
            }
            p = p->parentWidget();
        }
        close();
    });

    // F5 shortcut for Refresh
    m_refreshShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(m_refreshShortcut, &QShortcut::activated, this, &SnapQuoteWindow::onRefreshClicked);
    
    // Default focus to token field
    if (m_leToken) {
        QTimer::singleShot(0, this, [this]() {
            if (m_leToken) {
                m_leToken->setFocus();
                m_leToken->selectAll();
            }
        });
    }
    
    qDebug() << "[SnapQuoteWindow] UI initialized successfully";
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

void SnapQuoteWindow::loadFromContext(const WindowContext &context)
{
    if (!context.isValid()) {
        qDebug() << "[SnapQuoteWindow] Invalid context, skipping load";
        return;
    }
    
    m_context = context;
    m_token = context.token;
    m_exchange = context.exchange.left(3); // "NSE" from "NSEFO"
    m_segment = context.segment; // "F" or "E"
    m_symbol = context.symbol;
    
    // Set exchange
    if (m_cbEx) {
        int idx = m_cbEx->findText(m_exchange);
        if (idx >= 0) m_cbEx->setCurrentIndex(idx);
    }
    
    // Set segment
    if (m_cbSegment) {
        int idx = m_cbSegment->findText(m_segment);
        if (idx >= 0) m_cbSegment->setCurrentIndex(idx);
    }
    
    // Set token
    if (m_leToken) {
        m_leToken->setText(QString::number(context.token));
    }
    
    // Set instrument type
    if (m_leInstType) {
        m_leInstType->setText(context.instrumentType);
    }
    
    // Set symbol
    if (m_leSymbol) {
        m_leSymbol->setText(context.symbol);
    }
    
    // Set expiry for F&O
    if (m_cbExpiry && !context.expiry.isEmpty()) {
        m_cbExpiry->setCurrentText(context.expiry);
    }
    
    // Update market data if available
    if (context.ltp > 0) {
        if (m_lbLTPPrice) {
            m_lbLTPPrice->setText(QString::number(context.ltp, 'f', 2));
        }
    }
    
    // Update OHLC data
    if (m_lbOpen && context.close > 0) {
        m_lbOpen->setText(QString::number(context.close, 'f', 2)); // Previous close as open estimate
    }
    if (m_lbHigh && context.high > 0) {
        m_lbHigh->setText(QString::number(context.high, 'f', 2));
    }
    if (m_lbLow && context.low > 0) {
        m_lbLow->setText(QString::number(context.low, 'f', 2));
    }
    if (m_lbClose && context.close > 0) {
        m_lbClose->setText(QString::number(context.close, 'f', 2));
    }
    
    // Update volume
    if (m_lbVolume && context.volume > 0) {
        m_lbVolume->setText(QString::number(context.volume));
    }
    
    // Update bid/ask prices if available
    if (m_lbBidPrice1 && context.bid > 0) {
        m_lbBidPrice1->setText(QString::number(context.bid, 'f', 2));
    }
    if (m_lbAskPrice1 && context.ask > 0) {
        m_lbAskPrice1->setText(QString::number(context.ask, 'f', 2));
    }
    
    qDebug() << "[SnapQuoteWindow] Loaded context:" << context.exchange << context.symbol
             << "Token:" << context.token << "LTP:" << context.ltp;
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
        default: return;
    }
    
    if (qtyLabel) {
        qtyLabel->setText(QString::number(qty));
        qDebug() << "[SnapQuoteWindow] Set BidQty" << level << "to" << qty;
    } else {
        qWarning() << "[SnapQuoteWindow] BidQty" << level << "label is NULL!";
    }
    
    if (priceLabel) {
        priceLabel->setText(QString::number(price, 'f', 2));
        qDebug() << "[SnapQuoteWindow] Set BidPrice" << level << "to" << price;
    } else {
        qWarning() << "[SnapQuoteWindow] BidPrice" << level << "label is NULL!";
    }
    
    if (ordersLabel) {
        ordersLabel->setText(QString::number(orders));
        qDebug() << "[SnapQuoteWindow] Set BidOrders" << level << "to" << orders;
    } else {
        qWarning() << "[SnapQuoteWindow] BidOrders" << level << "label is NULL!";
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

// Helper to map exchange string to segment integer
static int getExchangeSegment(const QString &exchange)
{
    if (exchange == "NSECM") return 1;
    if (exchange == "NSEFO") return 2;
    if (exchange == "NSECD") return 13;
    if (exchange == "BSECM") return 11;
    if (exchange == "BSEFO") return 12;
    if (exchange == "BSECD") return 61;
    if (exchange == "MCXFO") return 51;
    qWarning() << "[SnapQuoteWindow] Unknown exchange segment:" << exchange;
    return 0;
}

void SnapQuoteWindow::fetchQuote()
{
    qDebug() << "[SnapQuoteWindow] fetchQuote() called";
    qDebug() << "[SnapQuoteWindow] m_xtsClient =" << m_xtsClient;
    qDebug() << "[SnapQuoteWindow] m_exchange =" << m_exchange;
    qDebug() << "[SnapQuoteWindow] m_token =" << m_token;
    qDebug() << "[SnapQuoteWindow] m_context.exchange =" << m_context.exchange;
    
    if (!m_xtsClient) {
        qWarning() << "[SnapQuoteWindow] ❌ Cannot fetch quote: XTS client not set";
        return;
    }
    
    if (m_exchange.isEmpty() || m_token <= 0) {
        qWarning() << "[SnapQuoteWindow] ❌ Cannot fetch quote: invalid exchange or token";
        qWarning() << "[SnapQuoteWindow]    Exchange:" << m_exchange << "Token:" << m_token;
        return;
    }
    
    int segment = getExchangeSegment(m_context.exchange);  // Use full exchange from context
    if (segment == 0) {
        qWarning() << "[SnapQuoteWindow] ❌ Cannot fetch quote: unknown exchange" << m_context.exchange;
        return;
    }
    
    qDebug() << "[SnapQuoteWindow] ✅ Fetching quote for exchange:" << m_context.exchange 
             << "token:" << m_token << "segment:" << segment;
    
    // Call API with lambda callback (success, data, error)
    m_xtsClient->getQuote(m_token, segment, [this](bool success, const QJsonObject &quote, const QString &error) {
        if (!success) {
            qWarning() << "[SnapQuoteWindow] Failed to fetch quote:" << error;
            return;
        }
        
        qDebug() << "[SnapQuoteWindow] Quote received for token" << m_token;
        
        // Parse touchline data
        QJsonObject touchline = quote.value("Touchline").toObject();
        if (touchline.isEmpty()) {
            qWarning() << "[SnapQuoteWindow] No Touchline data in quote response";
            return;
        }
        
        // Extract values with proper type conversion
        double ltp = touchline.value("LastTradedPrice").toDouble(0.0);
        int ltpQty = touchline.value("LastTradedQunatity").toInt(0);  // Note: API typo "Qunatity"
        if (ltpQty == 0) ltpQty = touchline.value("LastTradedQuantity").toInt(0);  // Try correct spelling
        QString ltpTime = touchline.value("LastTradeTime").toString();
        if (ltpTime.isEmpty()) ltpTime = touchline.value("LastUpdateTime").toString();
        
        double open = touchline.value("Open").toDouble(0.0);
        double high = touchline.value("High").toDouble(0.0);
        double low = touchline.value("Low").toDouble(0.0);
        double close = touchline.value("Close").toDouble(0.0);
        qint64 volume = touchline.value("TotalTradedQuantity").toVariant().toLongLong();
        double value = touchline.value("TotalTradedValue").toDouble(0.0);
        if (value == 0.0) value = touchline.value("TotalBuyQuantity").toDouble(0.0);  // Fallback
        double atp = touchline.value("AverageTradedPrice").toDouble(0.0);
        if (atp == 0.0) atp = touchline.value("ATP").toDouble(0.0);  // Try short form
        double percentChange = touchline.value("PercentChange").toDouble(0.0);
        
        // Extract total buyers/sellers
        qint64 totalBuyers = touchline.value("TotalBuyQuantity").toVariant().toLongLong();
        qint64 totalSellers = touchline.value("TotalSellQuantity").toVariant().toLongLong();

        if (m_lbTotalBuyers) m_lbTotalBuyers->setText(QString("Total Buyers: %1").arg(totalBuyers));
        if (m_lbTotalSellers) m_lbTotalSellers->setText(QString("Total Sellers: %1").arg(totalSellers));
        
        qDebug() << "[SnapQuoteWindow] LTP=" << ltp << "Vol=" << volume << "Bids/Asks:" << quote.value("Bids").toArray().size() << "/" << quote.value("Asks").toArray().size();
        
        // Update main quote display
        updateQuote(ltp, ltpQty, ltpTime, open, high, low, close, volume, value, atp, percentChange);
        
        // Parse bid depth (best 5 levels)
        QJsonArray bids = quote.value("Bids").toArray();
        
        if (bids.size() > 0) {
            for (int i = 0; i < qMin(5, bids.size()); ++i) {
                QJsonObject bid = bids[i].toObject();
                int qty = bid.value("Size").toInt();
                double price = bid.value("Price").toDouble();
                int orders = bid.value("TotalOrders").toInt();
                updateBidDepth(i + 1, qty, price, orders);
            }
        } else {
            // Fallback: Try to get single bid from Touchline
            QJsonObject bidInfo = touchline.value("BidInfo").toObject();
            if (!bidInfo.isEmpty()) {
                double price = bidInfo.value("Price").toDouble();
                int qty = bidInfo.value("Size").toInt();
                int orders = bidInfo.value("TotalOrders").toInt();
                qDebug() << "[SnapQuoteWindow] Using BidInfo from Touchline: Price=" << price << "Qty=" << qty;
                updateBidDepth(1, qty, price, orders);
            }
        }
        
        // Parse ask depth (best 5 levels)
        QJsonArray asks = quote.value("Asks").toArray();
        
        if (asks.size() > 0) {
            for (int i = 0; i < qMin(5, asks.size()); ++i) {
                QJsonObject ask = asks[i].toObject();
                double price = ask.value("Price").toDouble();
                int qty = ask.value("Size").toInt();
                int orders = ask.value("TotalOrders").toInt();
                updateAskDepth(i + 1, price, qty, orders);
            }
        } else {
            // Fallback: Try to get single ask from Touchline
            QJsonObject askInfo = touchline.value("AskInfo").toObject();
            if (!askInfo.isEmpty()) {
                double price = askInfo.value("Price").toDouble();
                int qty = askInfo.value("Size").toInt();
                qDebug() << "[SnapQuoteWindow] Using AskInfo from Touchline: Price=" << price << "Qty=" << qty;
                updateAskDepth(1, price, qty, 0);
            }
        }
        
        qDebug() << "[SnapQuoteWindow] Quote data updated successfully";
    });
}
