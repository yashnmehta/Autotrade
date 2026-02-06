#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <QObject>
#include <QWidget>
#include <QList>
#include <QDebug>
#include <QDateTime>

/**
 * @class WindowManager
 * @brief Global manager for tracking and managing window focus across the application
 * 
 * This singleton class maintains a stack of active windows and handles focus restoration
 * when windows are closed. It ensures that when a window is closed, the previously active
 * window automatically regains focus.
 * 
 * Usage:
 * - Register windows when they are opened/activated
 * - Unregister windows when they are closed
 * - The manager automatically activates the last active window
 */
class WindowManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance of WindowManager
     * @return Reference to the WindowManager instance
     */
    static WindowManager& instance();

    /**
     * @brief Register a window with the manager (push to top of stack)
     * This should be called when a window is opened or activated
     * @param window Pointer to the window to register
     * @param windowName Optional name for debugging purposes
     * @param initiatingWindow Optional pointer to the window that opened this window
     */
    void registerWindow(QWidget* window, const QString& windowName = QString(), QWidget* initiatingWindow = nullptr);

    /**
     * @brief Unregister a window and activate the previous one
     * This should be called when a window is closed
     * @param window Pointer to the window to unregister
     */
    void unregisterWindow(QWidget* window);

    /**
     * @brief Move a window to the top of the stack (make it the active window)
     * This should be called when a window gains focus
     * @param window Pointer to the window to move to top
     */
    void bringToTop(QWidget* window);

    /**
     * @brief Get the current active window (top of stack)
     * @return Pointer to the active window, or nullptr if no windows are registered
     */
    QWidget* getActiveWindow() const;

    /**
     * @brief Check if a window is registered
     * @param window Pointer to the window to check
     * @return true if the window is registered, false otherwise
     */
    bool isRegistered(QWidget* window) const;

    /**
     * @brief Get the number of registered windows
     * @return Number of windows in the stack
     */
    int windowCount() const;

    /**
     * @brief Get the initiating window for a given window
     * @param window Pointer to the window to check
     * @return Pointer to the initiating window, or nullptr if none
     */
    QWidget* getInitiatingWindow(QWidget* window) const;

private:
    WindowManager(QObject* parent = nullptr);
    ~WindowManager();
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    struct WindowEntry {
        QWidget* window;
        QString name;
        qint64 timestamp;

        WindowEntry(QWidget* w, const QString& n)
            : window(w), name(n), timestamp(QDateTime::currentMSecsSinceEpoch()) {}
    };

    QList<WindowEntry> m_windowStack;
    QMap<QWidget*, QWidget*> m_initiatingWindows;  // child -> parent mapping
};

#endif // WINDOWMANAGER_H
