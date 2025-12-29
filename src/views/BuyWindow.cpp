#include "views/BuyWindow.h"
#include "views/SellWindow.h"
#include "utils/PreferencesManager.h"
#include "utils/WindowSettingsHelper.h"
#include "core/widgets/CustomMDISubWindow.h"
#include <QMessageBox>
#include <QDebug>

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
    if (quantity <= 0 || price <= 0) {
        QMessageBox::warning(this, "Buy Order", "Invalid quantity or price");
        return;
    }

    emit orderSubmitted(m_cbEx->currentText(), m_leToken->text().toInt(), m_leSymbol->text(), 
                        quantity, price, m_cbOrdType->currentText());
    
    QMessageBox::information(this, "Buy Order", "Order submitted");
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
