#include "views/SellWindow.h"
#include "views/BuyWindow.h"
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
    if (quantity <= 0 || price <= 0) {
        QMessageBox::warning(this, "Sell Order", "Invalid quantity or price");
        return;
    }

    emit orderSubmitted(m_cbEx->currentText(), m_leToken->text().toInt(), m_leSymbol->text(), 
                        quantity, price, m_cbOrdType->currentText());
    
    QMessageBox::information(this, "Sell Order", "Order submitted");
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
