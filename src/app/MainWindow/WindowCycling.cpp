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
    
    QList<CustomMDISubWindow*> windows = m_mdiArea->windowList();
    qDebug() << "[MainWindow] Total windows:" << windows.size();
    
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
    
    // Find current window index
    int currentIndex = windows.indexOf(activeWindow);
    if (currentIndex == -1) {
        // Active window not in list (shouldn't happen), activate first
        m_mdiArea->activateWindow(windows.first());
        qWarning() << "[MainWindow] Active window not in list, activated first";
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
    
    QList<CustomMDISubWindow*> windows = m_mdiArea->windowList();
    qDebug() << "[MainWindow] Total windows:" << windows.size();
    
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
    
    // Find current window index
    int currentIndex = windows.indexOf(activeWindow);
    if (currentIndex == -1) {
        // Active window not in list (shouldn't happen), activate last
        m_mdiArea->activateWindow(windows.last());
        qWarning() << "[MainWindow] Active window not in list, activated last";
        return;
    }
    
    // Cycle to previous window (wrap around)
    int prevIndex = (currentIndex - 1 + windows.size()) % windows.size();
    CustomMDISubWindow* prevWindow = windows.at(prevIndex);
    
    m_mdiArea->activateWindow(prevWindow);
    qDebug() << "[MainWindow] ✅ Cycled backward:" << activeWindow->title() << "→" << prevWindow->title();
}
