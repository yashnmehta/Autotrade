#include "views/SnapQuoteWindow.h"
#include "api/XTSMarketDataClient.h"
#include "repository/RepositoryManager.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>

void SnapQuoteWindow::onRefreshClicked()
{
    emit refreshRequested(m_exchange, m_token);
    fetchQuote();
}

void SnapQuoteWindow::fetchQuote()
{
    if (!m_xtsClient || m_token <= 0) return;
    
    // Simplified segment mapping for demo
    int segment = (m_exchange.contains("FO")) ? 2 : 1;
    
    m_xtsClient->getQuote(m_token, segment, [this](bool success, const QJsonObject &quote, const QString &error) {
        if (!success) return;
        
        QJsonObject touchline = quote.value("Touchline").toObject();
        if (touchline.isEmpty()) return;
        
        double ltp = touchline.value("LastTradedPrice").toDouble(0.0);
        double close = touchline.value("Close").toDouble(0.0);
        double percentChange = touchline.value("PercentChange").toDouble(0.0);
        
        if (m_lbLTPPrice) m_lbLTPPrice->setText(QString::number(ltp, 'f', 2));
        if (m_lbPercentChange) {
            m_lbPercentChange->setText(QString::number(percentChange, 'f', 2) + "%");
            setChangeColor(percentChange);
        }
        setLTPIndicator(ltp >= close);
    });
}

void SnapQuoteWindow::onScripSelected(const InstrumentData &data)
{
    m_token = data.exchangeInstrumentID;
    QString segKey = RepositoryManager::getExchangeSegmentName(data.exchangeSegment);
    // Parse exchange from segKey (NSECM -> NSE, BSEFO -> BSE)
    if (segKey.startsWith("BSE")) m_exchange = "BSE";
    else if (segKey.startsWith("MCX")) m_exchange = "MCX";
    else if (segKey.startsWith("NSECD")) m_exchange = "NSECDS";
    else m_exchange = "NSE";
    
    m_symbol = data.symbol;
    
    qDebug() << "[SnapQuote] Selected:" << m_symbol << m_token << m_exchange;
    fetchQuote();
}

void SnapQuoteWindow::loadFromContext(const WindowContext &context)
{
    if (!context.isValid()) return;
    m_token = context.token;
    m_exchange = context.exchange;
    m_symbol = context.symbol;
    
    // Update ScripBar with context
    if (m_scripBar) {
        InstrumentData data;
        data.exchangeInstrumentID = m_token;
        data.symbol = m_symbol;
        // Map exchange string to ID if needed, or rely on RepositoryManager inside ScripBar
        data.exchangeSegment = RepositoryManager::getExchangeSegmentID(m_exchange, 
                              (m_exchange == "NSE" && m_token < 100000) ? "E" : "F"); // Approximation
        // Ideally context has more info or we look it up
        data.name = m_symbol; 
        data.instrumentType = context.instrumentType; // Assuming context has this or "EQUITY"
        
        m_scripBar->setScripDetails(data);
    }

    fetchQuote();
}

void SnapQuoteWindow::setLTPIndicator(bool isUp)
{
    if (m_lbLTPIndicator) {
        m_lbLTPIndicator->setText(isUp ? "▲" : "▼");
        m_lbLTPIndicator->setStyleSheet(isUp ? "color: #2ECC71;" : "color: #E74C3C;");
    }
}

void SnapQuoteWindow::setChangeColor(double change)
{
    if (m_lbPercentChange) {
        m_lbPercentChange->setStyleSheet(change >= 0 ? "color: #2ECC71;" : "color: #E74C3C;");
    }
}
