#include "views/SnapQuoteWindow.h"
#include <QDebug>
#include <QShowEvent>
#include <QTimer>

SnapQuoteWindow::SnapQuoteWindow(QWidget *parent)
    : QWidget(parent), m_formWidget(nullptr), m_token(0), m_xtsClient(nullptr)
{
    initUI();
}

SnapQuoteWindow::SnapQuoteWindow(const WindowContext &context, QWidget *parent)
    : QWidget(parent), m_formWidget(nullptr), m_token(0), m_xtsClient(nullptr)
{
    initUI();
    loadFromContext(context);
}

SnapQuoteWindow::~SnapQuoteWindow() {}

void SnapQuoteWindow::setScripDetails(const QString &exchange, const QString &segment,
                                      int token, const QString &instType, const QString &symbol)
{
    m_exchange = exchange;
    m_token = token;
    m_symbol = symbol;
    fetchQuote();
}

void SnapQuoteWindow::setXTSClient(XTSMarketDataClient *client)
{
    m_xtsClient = client;
    if (m_scripBar) {
        m_scripBar->setXTSClient(client);
    }
}

// ⚡ Set ScripBar to DisplayMode for instant setScripDetails() (<1ms)
void SnapQuoteWindow::setScripBarDisplayMode(bool displayMode)
{
    if (m_scripBar) {
        m_scripBar->setScripBarMode(displayMode ? ScripBar::DisplayMode : ScripBar::SearchMode);
        qDebug() << "[SnapQuoteWindow] ⚡ ScripBar set to" << (displayMode ? "DisplayMode (<1ms)" : "SearchMode");
    }
}

// ⚡ CRITICAL OPTIMIZATION: Populate ScripBar ASYNCHRONOUSLY after window is visible
// This makes the show() call instant and defers expensive populateSymbols() to next event loop
void SnapQuoteWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    
    // ⚡ If we have pending ScripBar update, schedule it asynchronously (NON-BLOCKING!)
    if (m_needsScripBarUpdate && m_scripBar) {
        qDebug() << "[SnapQuoteWindow] ⚡ Scheduling async ScripBar update";
        
        // Use QTimer::singleShot to defer to next event loop (instant show!)
        QTimer::singleShot(0, this, [this]() {
            if (m_scripBar && m_needsScripBarUpdate) {
                qDebug() << "[SnapQuoteWindow] ⚡ Executing async ScripBar update";
                m_scripBar->setScripDetails(m_pendingScripData);
                m_needsScripBarUpdate = false;
                qDebug() << "[SnapQuoteWindow] ⚡ ScripBar populated (async complete)";
            }
        });
    }
}
