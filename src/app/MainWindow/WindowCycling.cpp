// Window Cycling Implementation for MainWindow
// Ctrl+Tab / Ctrl+Shift+Tab support

#include "app/MainWindow.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include <QDebug>

/**
 * @brief Cycle to the next window (Ctrl+Tab)
 * 
 * Activates the next window in the window list, wrapping around to the first
 * window if currently at the last window.
 */
void MainWindow::cycleWindowsForward()
{
    qDebug() << "[MainWindow] ⌨️ Ctrl+Tab pressed - cycling windows forward";
    
    if (!m_mdiArea) {
        qWarning() << "[MainWindow] Cannot cycle: m_mdiArea is null";
        return;
    }
    
    // Only cycle through windows that are on-screen (x > -1000)
    // Cached windows that are "closed" are moved to OFF_SCREEN_X = -10000
    QList<CustomMDISubWindow*> allWindows = m_mdiArea->windowList();
    QList<CustomMDISubWindow*> windows;
    for (auto *w : allWindows) {
        if (w && !w->isMinimized() && w->x() > -1000)
            windows.append(w);
    }
    qDebug() << "[MainWindow] On-screen windows:" << windows.size() << "/" << allWindows.size();
    
    if (windows.isEmpty()) {
        qDebug() << "[MainWindow] No windows to cycle";
        return;
    }
    
    CustomMDISubWindow* activeWindow = m_mdiArea->activeWindow();
    
    // If no active window, activate the first one
    if (!activeWindow) {
        m_mdiArea->activateWindow(windows.first());
        qDebug() << "[MainWindow] ✅ Activated first window:" << windows.first()->title();
        return;
    }
    
    // Find current window index in the on-screen list
    int currentIndex = windows.indexOf(activeWindow);
    if (currentIndex == -1) {
        // Active window not on-screen, activate first on-screen
        m_mdiArea->activateWindow(windows.first());
        qDebug() << "[MainWindow] Active window off-screen, activated first on-screen";
        return;
    }
    
    // Cycle to next window (wrap around)
    int nextIndex = (currentIndex + 1) % windows.size();
    CustomMDISubWindow* nextWindow = windows.at(nextIndex);
    
    m_mdiArea->activateWindow(nextWindow);
    qDebug() << "[MainWindow] ✅ Cycled forward:" << activeWindow->title() << "→" << nextWindow->title();
}

/**
 * @brief Cycle to the previous window (Ctrl+Shift+Tab)
 * 
 * Activates the previous window in the window list, wrapping around to the last
 * window if currently at the first window.
 */
void MainWindow::cycleWindowsBackward()
{
    qDebug() << "[MainWindow] ⌨️ Ctrl+Shift+Tab pressed - cycling windows backward";
    
    if (!m_mdiArea) {
        qWarning() << "[MainWindow] Cannot cycle: m_mdiArea is null";
        return;
    }
    
    // Only cycle through windows that are on-screen (x > -1000)
    // Cached windows that are "closed" are moved to OFF_SCREEN_X = -10000
    QList<CustomMDISubWindow*> allWindows = m_mdiArea->windowList();
    QList<CustomMDISubWindow*> windows;
    for (auto *w : allWindows) {
        if (w && !w->isMinimized() && w->x() > -1000)
            windows.append(w);
    }
    qDebug() << "[MainWindow] On-screen windows:" << windows.size() << "/" << allWindows.size();
    
    if (windows.isEmpty()) {
        qDebug() << "[MainWindow] No windows to cycle";
        return;
    }
    
    CustomMDISubWindow* activeWindow = m_mdiArea->activeWindow();
    
    // If no active window, activate the last one
    if (!activeWindow) {
        m_mdiArea->activateWindow(windows.last());
        qDebug() << "[MainWindow] ✅ Activated last window:" << windows.last()->title();
        return;
    }
    
    // Find current window index in the on-screen list
    int currentIndex = windows.indexOf(activeWindow);
    if (currentIndex == -1) {
        // Active window not on-screen, activate last on-screen
        m_mdiArea->activateWindow(windows.last());
        qDebug() << "[MainWindow] Active window off-screen, activated last on-screen";
        return;
    }
    
    // Cycle to previous window (wrap around)
    int prevIndex = (currentIndex - 1 + windows.size()) % windows.size();
    CustomMDISubWindow* prevWindow = windows.at(prevIndex);
    
    m_mdiArea->activateWindow(prevWindow);
    qDebug() << "[MainWindow] ✅ Cycled backward:" << activeWindow->title() << "→" << prevWindow->title();
}
