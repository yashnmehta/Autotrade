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
    
    // Connect to quote signal (use UniqueConnection to avoid duplicates)
    connect(m_xtsClient, &XTSMarketDataClient::quoteReceived, this, [this](bool success, const QJsonObject &quote, const QString &error) {
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
    }, Qt::UniqueConnection);
    
    // Trigger the actual quote request
    m_xtsClient->getQuote(m_token, segment);
}

void SnapQuoteWindow::onScripSelected(const InstrumentData &data)
{
    m_token = data.exchangeInstrumentID;
    
    // Determine simpler exchange string for internal usage
    QString segKey = RepositoryManager::getExchangeSegmentName(data.exchangeSegment);
    QString exchangePrefix;
    QString segSuffix;
    
    // Better logic to parse "NSECM", "NSEFO", etc. 
    if (segKey.startsWith("NSE")) {
        exchangePrefix = "NSE";
        if (segKey.contains("FO")) segSuffix = "FO"; else segSuffix = "CM";
    } else if (segKey.startsWith("BSE")) {
        exchangePrefix = "BSE";
        if (segKey.contains("FO")) segSuffix = "FO"; else segSuffix = "CM";
    } else if (segKey.startsWith("MCX")) {
        exchangePrefix = "MCX";
        segSuffix = "FO";
    } else {
        exchangePrefix = "NSE"; segSuffix = "CM";
    }
    
    m_exchange = exchangePrefix + segSuffix; // Store full segment like "NSEFO", "NSECM"
    m_symbol = data.symbol;
    
    qDebug() << "[SnapQuote] Selected:" << m_symbol << m_token << m_exchange;
    
    // --- Update Context ---
    m_context = WindowContext();
    m_context.sourceWindow = "SnapQuote";
    m_context.exchange = m_exchange;
    m_context.token = m_token;
    m_context.symbol = m_symbol;
    m_context.displayName = data.name; // Use instrument name if available
    
    // Lookup details for richer context
    const ContractData* contract = RepositoryManager::getInstance()->getContractByToken(
        data.exchangeSegment, m_token);
        
    if (contract) {
        m_context.displayName = contract->displayName;
        m_context.instrumentType = contract->series; // Or infer from ID
        // Better instrument type logic
        if (contract->instrumentType == 1) m_context.instrumentType = "FUTIDX"; // simplified
        else if (contract->instrumentType == 2) m_context.instrumentType = "OPTIDX";
        else if (contract->instrumentType == 4) m_context.instrumentType = "SPD";
        else m_context.instrumentType = "EQUITY";
        
        m_context.expiry = contract->expiryDate;
        m_context.strikePrice = contract->strikePrice;
        m_context.optionType = contract->optionType;
        m_context.lotSize = contract->lotSize;
        m_context.tickSize = contract->tickSize;
        m_context.series = contract->series;
    } else {
        // Fallback
        m_context.instrumentType = (segSuffix == "CM") ? "EQUITY" : "FUTIDX";
    }
    
    fetchQuote();
}

void SnapQuoteWindow::loadFromContext(const WindowContext &context)
{
    if (!context.isValid()) return;
    m_context = context; // Save the context!
    
    m_token = context.token;
    m_exchange = context.exchange;
    m_symbol = context.symbol;
    
    // Update ScripBar with context
    if (m_scripBar) {
        InstrumentData data;
        data.exchangeInstrumentID = m_token;
        data.symbol = m_symbol;
        data.name = context.displayName;
        
        // Try to reverse map exchange string to ID
        // "NSECM" -> 1, "NSEFO" -> 2
        int segID = RepositoryManager::getExchangeSegmentID(m_exchange.left(3), m_exchange.mid(3));
        if (segID == 0) segID = 1; // Default
        
        data.exchangeSegment = segID;
        data.instrumentType = context.instrumentType; 
        
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
