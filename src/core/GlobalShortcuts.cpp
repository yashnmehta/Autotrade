#include "app/MainWindow.h"
#include "views/MarketWatchWindow.h"
#include "views/SnapQuoteWindow.h"
#include "views/OrderBookWindow.h"
#include "views/TradeBookWindow.h"
#include "views/PositionWindow.h"
#include <QShortcut>
#include <QKeySequence>
#include <QDebug>

/**
 * @brief Centralized shortcut management for the entire application.
 * User requested all shortcuts for all classes to be in a single dedicated file.
 */

// --- MAIN WINDOW SHORTCUTS ---
void setupMainWindowShortcuts(MainWindow* window) {
    if (!window) return;
    new QShortcut(QKeySequence(Qt::Key_F1), window, SLOT(createBuyWindow()));
    new QShortcut(QKeySequence(Qt::Key_F2), window, SLOT(createSellWindow()));
    new QShortcut(QKeySequence("Ctrl+M"), window, SLOT(createMarketWatch()));
    new QShortcut(QKeySequence("Ctrl+S"), window, SLOT(focusScripBar()));
    new QShortcut(QKeySequence("Ctrl+P"), window, SLOT(showPreferences()));
    new QShortcut(QKeySequence("Alt+W"), window, SLOT(loadWorkspace()));
    new QShortcut(QKeySequence("Alt+S"), window, SLOT(saveCurrentWorkspace()));
    
    // Window cycling shortcuts (Ctrl+Tab / Ctrl+Shift+Tab)
    // Use explicit key codes and SLOT() macro for private slots
    QShortcut *cycleForward = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab), window);
    cycleForward->setContext(Qt::ApplicationShortcut);
    QObject::connect(cycleForward, SIGNAL(activated()), window, SLOT(cycleWindowsForward()));
    
    QShortcut *cycleBackward = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab), window);
    cycleBackward->setContext(Qt::ApplicationShortcut);
    QObject::connect(cycleBackward, SIGNAL(activated()), window, SLOT(cycleWindowsBackward()));
    
    qDebug() << "[GlobalShortcuts] Window cycling shortcuts registered (Ctrl+Tab / Ctrl+Shift+Tab)";
    
    // Additional window shortcuts
    new QShortcut(QKeySequence(Qt::Key_F3), window, SLOT(createOrderBookWindow()));
    new QShortcut(QKeySequence(Qt::Key_F4), window, SLOT(createMarketWatch()));
    new QShortcut(QKeySequence(Qt::Key_F5), window, SLOT(createSnapQuoteWindow()));
    new QShortcut(QKeySequence(Qt::Key_F8), window, SLOT(createTradeBookWindow()));
    new QShortcut(QKeySequence(Qt::Key_F10), window, SLOT(createOrderBookWindow()));

    #ifdef Q_OS_MAC
    new QShortcut(QKeySequence("Meta+F6"), window, SLOT(createPositionWindow()));
    #else
    new QShortcut(QKeySequence("Alt+F6"), window, SLOT(createPositionWindow()));
    #endif
}

// --- MARKET WATCH SHORTCUTS ---
void setupMarketWatchShortcuts(MarketWatchWindow* window) {
    if (!window) return;
    QObject::connect(new QShortcut(QKeySequence::Copy, window), &QShortcut::activated, window, &MarketWatchWindow::copyToClipboard);
    QObject::connect(new QShortcut(QKeySequence::Cut, window), &QShortcut::activated, window, &MarketWatchWindow::cutToClipboard);
    QObject::connect(new QShortcut(QKeySequence::Paste, window), &QShortcut::activated, window, &MarketWatchWindow::pasteFromClipboard);
    QObject::connect(new QShortcut(QKeySequence::SelectAll, window), &QShortcut::activated, window, (void (QTableView::*)())&QTableView::selectAll);
}

// --- SNAP QUOTE SHORTCUTS ---
void setupSnapQuoteShortcuts(SnapQuoteWindow* window) {
    if (!window) return;
    new QShortcut(QKeySequence(Qt::Key_Escape), window, SLOT(close()));
    new QShortcut(QKeySequence(Qt::Key_F5), window, SLOT(onRefreshClicked()));
}

// --- BOOK WINDOW SHORTCUTS (Base functionality) ---
void setupBookWindowShortcuts(BaseBookWindow* window) {
    if (!window) return;
    // Assuming m_filterShortcut is protected and exists in BaseBookWindow subclasses
    // If not, we use the specific classes
}

// --- REDIRECTS ---
void MainWindow::setupShortcuts() { setupMainWindowShortcuts(this); }
void MarketWatchWindow::setupKeyboardShortcuts() { setupMarketWatchShortcuts(this); }
// Add more redirections if needed in respective classes
