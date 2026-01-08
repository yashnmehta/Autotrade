#include "views/SellWindow.h"
#include "views/BuyWindow.h"
#include "api/XTSTypes.h"
#include "utils/PreferencesManager.h"
#include "utils/WindowSettingsHelper.h"
#include "core/widgets/CustomMDISubWindow.h"
#include <QMessageBox>
#include <QDebug>
#include <cmath>


SellWindow* SellWindow::s_instance = nullptr;

SellWindow::SellWindow(QWidget *parent)
    : BaseOrderWindow(":/forms/SellWindow.ui", parent)
{
    setInstance(this);
    WindowSettingsHelper::loadAndApplyWindowSettings(this, "SellWindow");
    qDebug() << "[SellWindow] Created";
}

SellWindow::SellWindow(const WindowContext &context, QWidget *parent)
    : SellWindow(parent)
{
    loadFromContext(context);
}

SellWindow::~SellWindow() {
    if (s_instance == this) s_instance = nullptr;
}

void SellWindow::onSubmitClicked() {
    if (!m_leQty || !m_leRate) return;
    
    int quantity = m_leQty->text().toInt();
    double price = m_leRate->text().toDouble();
    if (quantity <= 0 || (m_cbOrdType->currentText() == "Limit" && price <= 0)) {
        QMessageBox::warning(this, "Sell Order", "Invalid quantity or price");
        return;
    }

    // Handle modification mode
    if (isModifyMode()) {
        // Validate: new quantity cannot be less than already filled quantity
        if (quantity < m_originalOrder.cumulativeQuantity) {
            QMessageBox::warning(this, "Modify Order", 
                QString("New quantity (%1) cannot be less than already filled quantity (%2)")
                    .arg(quantity).arg(m_originalOrder.cumulativeQuantity));
            return;
        }
        
        XTS::ModifyOrderParams params;
        params.appOrderID = m_originalOrderID;
        params.exchangeInstrumentID = m_leToken->text().toLongLong();
        params.exchangeSegment = m_cbEx->currentText();
        params.productType = m_originalOrder.productType;  // Use original product type
        params.orderType = m_cbOrdType->currentText();
        params.modifiedOrderQuantity = quantity;
        params.modifiedDisclosedQuantity = m_leDiscloseQty ? m_leDiscloseQty->text().toInt() : 0;
        params.modifiedLimitPrice = price;
        params.modifiedStopPrice = m_leTrigPrice ? m_leTrigPrice->text().toDouble() : 0.0;
        params.modifiedTimeInForce = m_cbValidity ? m_cbValidity->currentText() : "DAY";
        params.orderUniqueIdentifier = m_originalOrder.orderUniqueIdentifier;  // Use original identifier
        
        // Handle Market Orders (price = 0)
        if (params.orderType == "Market" || params.orderType == "StopMarket") {
            params.modifiedLimitPrice = 0;
        }
        
        emit orderModificationSubmitted(params);
        qDebug() << "[SellWindow] Modification request submitted for order:" << m_originalOrderID;
        return;
    }

    // Standard new order flow
    XTS::OrderParams params;
    params.exchangeSegment = m_cbEx->currentText();
    params.exchangeInstrumentID = m_leToken->text().toLongLong();
    params.productType = m_cbProduct ? m_cbProduct->currentText() : "MIS";
    params.orderType = m_cbOrdType->currentText();
    params.orderSide = "SELL";
    params.timeInForce = m_cbValidity ? m_cbValidity->currentText() : "DAY";
    params.orderQuantity = quantity;
    params.disclosedQuantity = m_leDiscloseQty ? m_leDiscloseQty->text().toInt() : 0;
    params.limitPrice = price;
    params.stopPrice = m_leTrigPrice ? m_leTrigPrice->text().toDouble() : 0.0;
    params.orderUniqueIdentifier = "TradingTerminal"; 

    // Handle clientID based on PRO/CLI selection
    if (m_cbProCli && m_cbProCli->currentText() == "PRO") {
        params.clientID = "*****";
    } else if (m_leClient && !m_leClient->text().isEmpty()) {
        params.clientID = m_leClient->text();
    }

    // Handle Market Orders logic if needed (price = 0)
    if (params.orderType == "Market" || params.orderType == "StopMarket") {
        params.limitPrice = 0;
    }

    emit orderSubmitted(params);
}


void SellWindow::calculateDefaultPrice(const WindowContext &context) {
    if (!m_leRate) return;
    double price = context.bid > 0 ? context.bid : context.ltp;
    if (price > 0) {
        double offset = PreferencesManager::instance().getSellPriceOffset();
        price -= offset;
        if (context.tickSize > 0) price = std::round(price / context.tickSize) * context.tickSize;
        m_leRate->setText(QString::number(price, 'f', 2));
    }
}

void SellWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_F2) {
        if (m_context.isValid()) emit requestBuyWithContext(m_context);
        close();
    } else {
        BaseOrderWindow::keyPressEvent(event);
    }
}

void SellWindow::closeEvent(QCloseEvent *event) {
    WindowSettingsHelper::saveWindowSettings(this, "SellWindow");
    BaseOrderWindow::closeEvent(event);
}

// Singleton Boilerplate
SellWindow* SellWindow::getInstance(QWidget *parent) {
    if (s_instance) { s_instance->raise(); s_instance->activateWindow(); return s_instance; }
    BuyWindow::closeCurrentWindow();
    return new SellWindow(parent);
}

SellWindow* SellWindow::getInstance(const WindowContext &context, QWidget *parent) {
    if (s_instance) { s_instance->loadFromContext(context); s_instance->raise(); s_instance->activateWindow(); return s_instance; }
    BuyWindow::closeCurrentWindow();
    return new SellWindow(context, parent);
}

void SellWindow::closeCurrentWindow() {
    if (s_instance) {
        QWidget *p = s_instance->parentWidget();
        while (p && !p->inherits("CustomMDISubWindow")) p = p->parentWidget();
        if (p) p->close(); else s_instance->close();
    }
}

bool SellWindow::hasActiveWindow() { return s_instance != nullptr; }
void SellWindow::setInstance(SellWindow* instance) { s_instance = instance; }
