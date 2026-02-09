#include "views/SnapQuoteWindow.h"
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include "utils/WindowManager.h"
#include <QDebug>
#include <QShowEvent>
#include <QTimer>


SnapQuoteWindow::SnapQuoteWindow(QWidget *parent)
    : QWidget(parent), m_formWidget(nullptr), m_token(0), m_xtsClient(nullptr) {
  initUI();

  // Register with WindowManager
  WindowManager::instance().registerWindow(this, windowTitle());
}

SnapQuoteWindow::SnapQuoteWindow(const WindowContext &context, QWidget *parent)
    : QWidget(parent), m_formWidget(nullptr), m_token(0), m_xtsClient(nullptr) {
  initUI();
  loadFromContext(context);

  // Register with WindowManager
  WindowManager::instance().registerWindow(this, windowTitle());
}

SnapQuoteWindow::~SnapQuoteWindow() {
  // Unsubscribe from UDP feed
  unsubscribeFromToken();

  // Unregister from WindowManager
  WindowManager::instance().unregisterWindow(this);
}

void SnapQuoteWindow::setScripDetails(const QString &exchange,
                                      const QString &segment, int token,
                                      const QString &instType,
                                      const QString &symbol) {
  m_exchange = exchange;
  m_token = token;
  m_symbol = symbol;

  // Subscribe to new token using robust segment detection
  int exchangeSegment =
      RepositoryManager::getExchangeSegmentID(exchange, segment);
  if (exchangeSegment == -1)
    exchangeSegment = 2; // Default to NSEFO as safety fallback

  subscribeToToken(exchangeSegment, token);
  fetchQuote();
}

void SnapQuoteWindow::subscribeToToken(int exchangeSegment, int token) {
  // Unsubscribe from previous token if any
  if (m_subscribedToken > 0) {
    FeedHandler::instance().unsubscribe(m_subscribedExchangeSegment,
                                        m_subscribedToken, this);
    qDebug() << "[SnapQuote] Unsubscribed from token" << m_subscribedToken;
  }

  // Subscribe to new token
  if (token > 0) {
    FeedHandler::instance().subscribe(exchangeSegment, token, this,
                                      &SnapQuoteWindow::onTickUpdate);
    m_subscribedExchangeSegment = exchangeSegment;
    m_subscribedToken = token;
    qDebug() << "[SnapQuote] ✅ Subscribed to token" << token << "(segment"
             << exchangeSegment << ")";
  }
}

void SnapQuoteWindow::unsubscribeFromToken() {
  if (m_subscribedToken > 0) {
    FeedHandler::instance().unsubscribe(m_subscribedExchangeSegment,
                                        m_subscribedToken, this);
    qDebug() << "[SnapQuote] Unsubscribed from token" << m_subscribedToken;
    m_subscribedToken = 0;
    m_subscribedExchangeSegment = 0;
  }
}

void SnapQuoteWindow::setXTSClient(XTSMarketDataClient *client) {
  m_xtsClient = client;
  if (m_scripBar) {
    m_scripBar->setXTSClient(client);
  }
}

// ⚡ Set ScripBar to DisplayMode for instant setScripDetails() (<1ms)
void SnapQuoteWindow::setScripBarDisplayMode(bool displayMode) {
  if (m_scripBar) {
    m_scripBar->setScripBarMode(displayMode ? ScripBar::DisplayMode
                                            : ScripBar::SearchMode);
    qDebug() << "[SnapQuoteWindow] ⚡ ScripBar set to"
             << (displayMode ? "DisplayMode (<1ms)" : "SearchMode");
  }
}

// ⚡ CRITICAL OPTIMIZATION: Populate ScripBar ASYNCHRONOUSLY after window is
// visible This makes the show() call instant and defers expensive
// populateSymbols() to next event loop
void SnapQuoteWindow::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);

  // Notify WindowManager that this window is now active
  WindowManager::instance().bringToTop(this);

  // ⚡ If we have pending ScripBar update, schedule it asynchronously
  // (NON-BLOCKING!)
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

void SnapQuoteWindow::focusInEvent(QFocusEvent *event) {
  QWidget::focusInEvent(event);

  // Notify WindowManager that this window has gained focus
  WindowManager::instance().bringToTop(this);
}

void SnapQuoteWindow::closeEvent(QCloseEvent *event) {
  // Don't unregister from WindowManager here for cached windows
  // The WindowCacheManager will handle the window lifecycle
  // Only unregister when actually being destroyed

  QWidget::closeEvent(event);
}
