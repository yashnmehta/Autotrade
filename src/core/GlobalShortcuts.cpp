#include "app/MainWindow.h"
#include "views/MarketWatchWindow.h"
#include "views/SnapQuoteWindow.h"
#include "views/OrderBookWindow.h"
#include "views/TradeBookWindow.h"
#include "views/PositionWindow.h"
#include <QShortcut>
#include <QKeySequence>
#include <QDebug>
#include <QApplication>
#include <QKeyEvent>

/**
 * @brief Centralized shortcut management for the entire application.
 * User requested all shortcuts for all classes to be in a single dedicated file.
 */

// Event filter to handle Ctrl+Tab cycling robustly, bypassing widget focus issues
class WindowCyclingFilter : public QObject {
    MainWindow* m_window;
public:
    WindowCyclingFilter(MainWindow* win) : QObject(win), m_window(win) {}
    
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            int key = keyEvent->key();
            Qt::KeyboardModifiers mods = keyEvent->modifiers();

#ifdef Q_OS_MAC
            // 1. macOS Standard: Command + ~ (Tilde/Backtick)
            // Qt::ControlModifier maps to Command on macOS
            if ((mods & Qt::ControlModifier) && (key == Qt::Key_QuoteLeft || key == Qt::Key_AsciiTilde)) {
                if (mods & Qt::ShiftModifier) {
                    QMetaObject::invokeMethod(m_window, "cycleWindowsBackward");
                } else {
                    QMetaObject::invokeMethod(m_window, "cycleWindowsForward");
                }
                return true;
            }
#endif

            // 2. Control + Tab (Physical Control on macOS = MetaModifier)
            // We check both Meta and Control to be robust against key mapping settings
            bool isCtrl = (mods & Qt::ControlModifier) || (mods & Qt::MetaModifier);

            if (isCtrl && (key == Qt::Key_Tab || key == Qt::Key_Backtab)) {
                if ((mods & Qt::ShiftModifier) || key == Qt::Key_Backtab) {
                    QMetaObject::invokeMethod(m_window, "cycleWindowsBackward");
                } else {
                    QMetaObject::invokeMethod(m_window, "cycleWindowsForward");
                }
                return true; 
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

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
    
    // Install robust application-wide filter for Window Cycling (Ctrl+Tab)
    // This bypasses focus-stealing widgets (like tables/inputs)
    WindowCyclingFilter* cycleFilter = new WindowCyclingFilter(window);
    qApp->installEventFilter(cycleFilter);
    
    qDebug() << "[GlobalShortcuts] Global Event Filter installed for Window Cycling";
    
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
    
    // Ctrl+Shift+E - Export price cache for debugging
    QObject::connect(new QShortcut(QKeySequence("Ctrl+Shift+E"), window), &QShortcut::activated, window, &MarketWatchWindow::exportPriceCacheDebug);
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
