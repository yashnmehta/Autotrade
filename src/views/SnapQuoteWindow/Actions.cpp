#include "views/SnapQuoteWindow.h"
#include "api/XTSMarketDataClient.h"
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

void SnapQuoteWindow::loadFromContext(const WindowContext &context)
{
    if (!context.isValid()) return;
    m_token = context.token;
    m_exchange = context.exchange;
    m_symbol = context.symbol;
    
    if (m_leSymbol) m_leSymbol->setText(m_symbol);
    if (m_leToken) m_leToken->setText(QString::number(m_token));
    
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
