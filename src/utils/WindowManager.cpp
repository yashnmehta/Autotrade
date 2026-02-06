#include "utils/WindowManager.h"
#include <QDateTime>
#include <QTimer>
#include <QTableView>

WindowManager::WindowManager(QObject* parent)
    : QObject(parent) {
    qDebug() << "[WindowManager] Initialized";
}

WindowManager::~WindowManager() {
    m_windowStack.clear();
    m_initiatingWindows.clear();
}

WindowManager& WindowManager::instance() {
    static WindowManager instance;
    return instance;
}

void WindowManager::registerWindow(QWidget* window, const QString& windowName, QWidget* initiatingWindow) {
    if (!window) {
        qDebug() << "[WindowManager] Cannot register null window";
        return;
    }

    // Store the initiating window relationship if provided
    if (initiatingWindow) {
        m_initiatingWindows[window] = initiatingWindow;
        qDebug() << "[WindowManager] Stored initiating window relationship:"
                 << windowName << "initiated by"
                 << (initiatingWindow->objectName().isEmpty() ? "Unknown" : initiatingWindow->objectName());
    }

    // Check if window is already registered
    for (int i = 0; i < m_windowStack.size(); ++i) {
        if (m_windowStack[i].window == window) {
            // Window already exists, move it to the top
            WindowEntry entry = m_windowStack.takeAt(i);
            entry.timestamp = QDateTime::currentMSecsSinceEpoch();
            m_windowStack.prepend(entry);
            qDebug() << "[WindowManager] Moved existing window to top:"
                     << (windowName.isEmpty() ? entry.name : windowName)
                     << "Stack size:" << m_windowStack.size();
            return;
        }
    }

    // Add new window to the top of the stack
    QString name = windowName.isEmpty() ? QString("Window_%1").arg(reinterpret_cast<quintptr>(window)) : windowName;
    m_windowStack.prepend(WindowEntry(window, name));
    
    qDebug() << "[WindowManager] Registered new window:" << name
             << "Stack size:" << m_windowStack.size();
}

void WindowManager::unregisterWindow(QWidget* window) {
    if (!window) {
        qDebug() << "[WindowManager] Cannot unregister null window";
        return;
    }

    // Find and remove the window from the stack
    for (int i = 0; i < m_windowStack.size(); ++i) {
        if (m_windowStack[i].window == window) {
            QString name = m_windowStack[i].name;
            m_windowStack.removeAt(i);
            qDebug() << "[WindowManager] Unregistered window:" << name
                     << "Remaining windows:" << m_windowStack.size();
            
            // Check if this window had an initiating window
            QWidget* initiatingWindow = m_initiatingWindows.value(window, nullptr);
            if (initiatingWindow) {
                // Remove the mapping
                m_initiatingWindows.remove(window);
                
                // Activate the initiating window (the one that opened this window)
                if (initiatingWindow->isVisible() && !initiatingWindow->isHidden()) {
                    QTimer::singleShot(50, [initiatingWindow, name, this]() {
                        if (initiatingWindow && !initiatingWindow->isHidden()) {
                            initiatingWindow->activateWindow();
                            initiatingWindow->raise();
                            
                            // Find the table view inside MarketWatch and set focus on it for keyboard nav
                            QTableView* tableView = initiatingWindow->findChild<QTableView*>();
                            if (tableView) {
                                tableView->setFocus(Qt::ActiveWindowFocusReason);
                                qDebug() << "[WindowManager] ✓ Activated initiating window and set focus on table view";
                            } else {
                                initiatingWindow->setFocus(Qt::ActiveWindowFocusReason);
                                qDebug() << "[WindowManager] ✓ Activated initiating window (no table view found)";
                            }
                        }
                    });
                    return;
                } else {
                    qDebug() << "[WindowManager] Initiating window is not visible, falling back to stack";
                }
            }
            
            // No initiating window or it's not visible - fall back to stack-based activation
            if (!m_windowStack.isEmpty()) {
                QWidget* nextWindow = m_windowStack.first().window;
                if (nextWindow && nextWindow->isVisible()) {
                    // Use a delayed activation to ensure proper focus handling
                    QTimer::singleShot(50, [nextWindow, this]() {
                        if (nextWindow && !nextWindow->isHidden()) {
                            nextWindow->activateWindow();
                            nextWindow->raise();
                            nextWindow->setFocus(Qt::ActiveWindowFocusReason);
                            qDebug() << "[WindowManager] ✓ Activated previous window:"
                                     << m_windowStack.first().name;
                        }
                    });
                } else {
                    qDebug() << "[WindowManager] Next window is not visible, skipping activation";
                }
            } else {
                qDebug() << "[WindowManager] No more windows in stack";
            }
            return;
        }
    }

    qDebug() << "[WindowManager] Window not found in stack (may not have been registered)";
}

void WindowManager::bringToTop(QWidget* window) {
    if (!window) {
        return;
    }

    // Find the window and move it to the top
    for (int i = 0; i < m_windowStack.size(); ++i) {
        if (m_windowStack[i].window == window) {
            if (i == 0) {
                // Already at the top, just update timestamp
                m_windowStack[0].timestamp = QDateTime::currentMSecsSinceEpoch();
                return;
            }
            
            // Move to top
            WindowEntry entry = m_windowStack.takeAt(i);
            entry.timestamp = QDateTime::currentMSecsSinceEpoch();
            m_windowStack.prepend(entry);
            
            qDebug() << "[WindowManager] Brought window to top:" << entry.name
                     << "Previous position:" << i;
            return;
        }
    }

    // Window not found, register it
    qDebug() << "[WindowManager] Window not in stack, registering it";
    registerWindow(window);
}

QWidget* WindowManager::getActiveWindow() const {
    if (m_windowStack.isEmpty()) {
        return nullptr;
    }
    return m_windowStack.first().window;
}

bool WindowManager::isRegistered(QWidget* window) const {
    if (!window) {
        return false;
    }

    for (const WindowEntry& entry : m_windowStack) {
        if (entry.window == window) {
            return true;
        }
    }
    return false;
}

int WindowManager::windowCount() const {
    return m_windowStack.size();
}

QWidget* WindowManager::getInitiatingWindow(QWidget* window) const {
    if (!window) {
        return nullptr;
    }
    return m_initiatingWindows.value(window, nullptr);
}
