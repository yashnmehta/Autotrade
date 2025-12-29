#include "views/SnapQuoteWindow.h"
#include <QDebug>

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
