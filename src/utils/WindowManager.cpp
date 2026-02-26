#include "utils/WindowManager.h"
#include <QApplication>
#include <QDateTime>
#include <QTimer>
#include <QTableView>

WindowManager::WindowManager(QObject* parent)
    : QObject(parent) {
    // Auto-track focus changes across all windows
    connect(qApp, &QApplication::focusChanged, this, &WindowManager::onFocusChanged);
    qDebug() << "[WindowManager] Initialized with widget-level focus tracking";
}

WindowManager::~WindowManager() {
    m_windowStack.clear();
    m_initiatingWindows.clear();
    m_lastFocusedWidgets.clear();
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
        qWarning() << "[WindowManager][FOCUS-DEBUG] registerWindow:" << windowName
                 << "(" << window->metaObject()->className() << ")"
                 << "initiated by" << initiatingWindow->metaObject()->className()
                 << initiatingWindow->objectName();
    } else {
        qWarning() << "[WindowManager][FOCUS-DEBUG] registerWindow:" << windowName
                 << "(" << window->metaObject()->className() << ") - no initiating window";
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
            qWarning() << "[WindowManager][FOCUS-DEBUG] Unregistered:" << name
                     << "| Remaining:" << m_windowStack.size();
            // Dump current stack for debugging
            qWarning() << "[WindowManager][FOCUS-DEBUG] Stack dump:";
            for (int j = 0; j < qMin(m_windowStack.size(), 5); j++) {
                auto &e = m_windowStack[j];
                qDebug() << "  [" << j << "]" << e.name
                         << "(" << (e.window ? e.window->metaObject()->className() : "null") << ")";
            }
            
            // Check if this window had an initiating window
            QWidget* initiatingWindow = m_initiatingWindows.value(window, nullptr);
            qWarning() << "[WindowManager][FOCUS-DEBUG] initiatingWindow for" << name << ":"
                     << (initiatingWindow ? initiatingWindow->metaObject()->className() : "nullptr")
                     << (initiatingWindow ? initiatingWindow->objectName() : "");

            // Clean up focus state for the closed window
            m_lastFocusedWidgets.remove(window);

            if (initiatingWindow) {
                // Remove the mapping
                m_initiatingWindows.remove(window);
                
                // Activate the initiating window (the one that opened this window)
                if (initiatingWindow->isVisible() && !initiatingWindow->isHidden()) {
                    QTimer::singleShot(50, [initiatingWindow, name, this]() {
                        if (initiatingWindow && !initiatingWindow->isHidden()) {
                            // If initiatingWindow is a content widget (not top-level),
                            // activate its parent CustomMDISubWindow for proper z-order
                            QWidget *activateTarget = initiatingWindow;
                            QWidget *p = initiatingWindow->parentWidget();
                            while (p) {
                                if (p->inherits("CustomMDISubWindow")) {
                                    activateTarget = p;
                                    break;
                                }
                                p = p->parentWidget();
                            }
                            activateTarget->activateWindow();
                            activateTarget->raise();
                            
                            // Restore last focused widget (tracked via focusChanged signal)
                            restoreFocusState(initiatingWindow);
                            qDebug() << "[WindowManager] ✓ Activated initiating window with focus restoration";
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
                            
                            // Restore last focused widget (tracked via focusChanged signal)
                            restoreFocusState(nextWindow);
                            qDebug() << "[WindowManager] ✓ Activated previous window with focus restoration";
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

// ─── Widget-level focus tracking ────────────────────────────────────────────

void WindowManager::onFocusChanged(QWidget* old, QWidget* /*now*/) {
    if (!old) return;

    // Find which registered window the losing-focus widget belongs to
    for (const auto& entry : m_windowStack) {
        if (entry.window && (entry.window == old || entry.window->isAncestorOf(old))) {
            m_lastFocusedWidgets[entry.window] = old;
            break;
        }
    }
}

void WindowManager::saveFocusState(QWidget* window) {
    if (!window) return;

    QWidget* focused = QApplication::focusWidget();
    if (focused && (focused == window || window->isAncestorOf(focused))) {
        m_lastFocusedWidgets[window] = focused;
        qDebug() << "[WindowManager] Manually saved focus state for"
                 << window->objectName() << "-> widget:"
                 << focused->objectName() << focused->metaObject()->className();
    }
}

bool WindowManager::restoreFocusState(QWidget* window) {
    if (!window) return false;

    // 1. Try saved widget from auto-tracking
    auto it = m_lastFocusedWidgets.find(window);
    if (it != m_lastFocusedWidgets.end() && !it.value().isNull()) {
        QWidget* widget = it.value();
        if (!widget->isHidden()) {
            widget->setFocus(Qt::ActiveWindowFocusReason);
            qDebug() << "[WindowManager] ✓ Restored focus to:"
                     << widget->objectName()
                     << "(" << widget->metaObject()->className() << ")"
                     << "in window:" << window->objectName();
            return true;
        }
    }

    // 2. Fallback: first QTableView child (common for data windows)
    QTableView* tableView = window->findChild<QTableView*>();
    if (tableView) {
        tableView->setFocus(Qt::ActiveWindowFocusReason);
        qDebug() << "[WindowManager] Restored focus to QTableView (fallback)"
                 << "in window:" << window->objectName();
        return true;
    }

    // 3. Final fallback: window itself
    window->setFocus(Qt::ActiveWindowFocusReason);
    qDebug() << "[WindowManager] Restored focus to window itself (fallback):"
             << window->objectName();
    return false;
}
