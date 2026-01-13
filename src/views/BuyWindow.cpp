#include "views/BuyWindow.h"
#include "views/SellWindow.h"
#include "api/XTSTypes.h"
#include "utils/PreferencesManager.h"
#include "utils/SoundManager.h"
#include "utils/WindowSettingsHelper.h"
#include "core/widgets/CustomMDISubWindow.h"
#include <QMessageBox>
#include <QDebug>
#include <cmath>


BuyWindow* BuyWindow::s_instance = nullptr;

BuyWindow::BuyWindow(QWidget *parent)
    : BaseOrderWindow(":/forms/BuyWindow.ui", parent)
{
    setInstance(this);
    WindowSettingsHelper::loadAndApplyWindowSettings(this, "BuyWindow");
    qDebug() << "[BuyWindow] Created";
}

BuyWindow::BuyWindow(const WindowContext &context, QWidget *parent)
    : BuyWindow(parent)
{
    loadFromContext(context);
}

BuyWindow::~BuyWindow() {
    if (s_instance == this) s_instance = nullptr;
}

void BuyWindow::onSubmitClicked() {
    if (!m_leQty || !m_leRate) return;
    
    int quantity = m_leQty->text().toInt();
    double price = m_leRate->text().toDouble();
    if (quantity <= 0 || (m_cbOrdType->currentText() == "Limit" && price <= 0)) {
        QMessageBox::warning(this, "Buy Order", "Invalid quantity or price");
        return;
    }

    // Handle modification mode
    if (isModifyMode() || isBatchModifyMode()) {
        
        // Helper lambda to modify a single order
        auto processModification = [&](const XTS::Order &originalOrd, int64_t originalID) {
             XTS::ModifyOrderParams params;
            params.appOrderID = originalID;
            params.exchangeInstrumentID = m_leToken->text().toLongLong();
            params.exchangeSegment = m_cbEx->currentText();
            params.productType = originalOrd.productType; 
            params.orderType = m_cbOrdType->currentText();
            
            // In Batch Mode, we use original quantity. In Single Modify, we use UI quantity.
            params.modifiedOrderQuantity = isBatchModifyMode() ? originalOrd.orderQuantity : quantity;
            params.modifiedDisclosedQuantity = isBatchModifyMode() ? originalOrd.orderDisclosedQuantity : (m_leDiscloseQty ? m_leDiscloseQty->text().toInt() : 0);
            
            params.modifiedLimitPrice = price;
            params.modifiedStopPrice = m_leTrigPrice ? m_leTrigPrice->text().toDouble() : 0.0;
            params.modifiedTimeInForce = m_cbValidity ? m_cbValidity->currentText() : "DAY";
            params.orderUniqueIdentifier = originalOrd.orderUniqueIdentifier;
            
            // Handle Market Orders (price = 0)
            if (params.orderType == "Market" || params.orderType == "StopMarket") {
                params.modifiedLimitPrice = 0;
            }
            
            emit orderModificationSubmitted(params);
        };

        if (isBatchModifyMode()) {
            int count = 0;
            for (const auto &ord : m_batchOrders) {
                // Determine effectively what quantity to check?
                // For batch, we trust the original quantity is valid as we aren't changing it.
                // We only check if Price is valid.
                processModification(ord, ord.appOrderID);
                count++;
            }
            qDebug() << "[BuyWindow] Batch modification submitted for" << count << "orders.";
            QMessageBox::information(this, "Batch Modify", QString("Submitted modification requests for %1 orders.").arg(count));
            close(); // Close window after batch submit
        } else {
             // Validate: new quantity cannot be less than already filled quantity
            if (quantity < m_originalOrder.cumulativeQuantity) {
                QMessageBox::warning(this, "Modify Order", 
                    QString("New quantity (%1) cannot be less than already filled quantity (%2)")
                        .arg(quantity).arg(m_originalOrder.cumulativeQuantity));
                return;
            }
            processModification(m_originalOrder, m_originalOrderID);
            qDebug() << "[BuyWindow] Modification request submitted for order:" << m_originalOrderID;
        }
        return;
    }

    // Standard new order flow
    XTS::OrderParams params;
    params.exchangeSegment = m_cbEx->currentText();
    params.exchangeInstrumentID = m_leToken->text().toLongLong();
    params.productType = m_cbProduct ? m_cbProduct->currentText() : "MIS";
    params.orderType = m_cbOrdType->currentText();
    params.orderSide = "BUY";
    params.timeInForce = m_cbValidity ? m_cbValidity->currentText() : "DAY";
    params.orderQuantity = quantity;
    params.disclosedQuantity = m_leDiscloseQty ? m_leDiscloseQty->text().toInt() : 0;
    params.limitPrice = price;
    params.stopPrice = m_leTrigPrice ? m_leTrigPrice->text().toDouble() : 0.0;
    params.orderUniqueIdentifier = "TradingTerminal"; 

    // Handle clientID based on PRO/CLI selection
    // PRO = Use "*****" (5 asterisks) as clientID
    // CLI = Use the actual client ID from the text field
    if (m_cbProCli && m_cbProCli->currentText() == "PRO") {
        params.clientID = "*****";  // 5 asterisks for PRO
    } else if (m_leClient && !m_leClient->text().isEmpty()) {
        params.clientID = m_leClient->text();
    }
    // If neither is set, MainWindow will use default clientID from login

    // Handle Market Orders logic if needed (price = 0)
    if (params.orderType == "Market" || params.orderType == "StopMarket") {
        params.limitPrice = 0;
    }

    emit orderSubmitted(params);
}


void BuyWindow::calculateDefaultPrice(const WindowContext &context) {
    if (!m_leRate) return;
    double price = context.ask > 0 ? context.ask : context.ltp;
    if (price > 0) {
        double offset = PreferencesManager::instance().getBuyPriceOffset();
        price += offset;
        if (context.tickSize > 0) price = std::round(price / context.tickSize) * context.tickSize;
        m_leRate->setText(QString::number(price, 'f', 2));
    }
}

void BuyWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_F2) {
        if (m_context.isValid()) emit requestSellWithContext(m_context);
        close();
    } else {
        BaseOrderWindow::keyPressEvent(event);
    }
}

void BuyWindow::closeEvent(QCloseEvent *event) {
    WindowSettingsHelper::saveWindowSettings(this, "BuyWindow");
    BaseOrderWindow::closeEvent(event);
}

// Singleton Boilerplate
BuyWindow* BuyWindow::getInstance(QWidget *parent) {
    if (s_instance) { s_instance->raise(); s_instance->activateWindow(); return s_instance; }
    SellWindow::closeCurrentWindow();
    return new BuyWindow(parent);
}

BuyWindow* BuyWindow::getInstance(const WindowContext &context, QWidget *parent) {
    if (s_instance) { s_instance->loadFromContext(context); s_instance->raise(); s_instance->activateWindow(); return s_instance; }
    SellWindow::closeCurrentWindow();
    return new BuyWindow(context, parent);
}

void BuyWindow::closeCurrentWindow() {
    if (s_instance) {
        QWidget *p = s_instance->parentWidget();
        while (p && !p->inherits("CustomMDISubWindow")) p = p->parentWidget();
        if (p) p->close(); else s_instance->close();
    }
}

bool BuyWindow::hasActiveWindow() { return s_instance != nullptr; }
void BuyWindow::setInstance(BuyWindow* instance) { s_instance = instance; }

void BuyWindow::showEvent(QShowEvent *event)
{
    BaseOrderWindow::showEvent(event);
    SoundManager::instance().playBeepForEvent(SoundManager::EventType::BuyWindowOpen);
}
