#ifndef CUSTOMMDIAREA_H
#define CUSTOMMDIAREA_H

#include <QWidget>
#include <QList>
#include <QPoint>

class CustomMDISubWindow;
class MDITaskBar;

/**
 * @brief Custom MDI Area - Pure C++ implementation without QMdiArea limitations
 * 
 * Features:
 * - Native C++ window management
 * - No Qt::SubWindow flag restrictions
 * - Direct child window control
 * - Custom taskbar for minimized windows
 */
class CustomMDIArea : public QWidget
{
    Q_OBJECT

public:
    explicit CustomMDIArea(QWidget *parent = nullptr);
    ~CustomMDIArea();

    // Window Management (Pure C++)
    void addWindow(CustomMDISubWindow *window);
    void removeWindow(CustomMDISubWindow *window);
    void activateWindow(CustomMDISubWindow *window);
    void minimizeWindow(CustomMDISubWindow *window);
    void restoreWindow(CustomMDISubWindow *window);
    
    CustomMDISubWindow* activeWindow() const;
    QList<CustomMDISubWindow*> windowList() const;
    
    MDITaskBar* taskBar() const { return m_taskBar; }

signals:
    void windowActivated(CustomMDISubWindow *window);
    void windowAdded(CustomMDISubWindow *window);
    void windowRemoved(CustomMDISubWindow *window);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void cascadeWindows();
    void tileWindows();
    QPoint getNextWindowPosition();
    
    QList<CustomMDISubWindow*> m_windows;
    QList<CustomMDISubWindow*> m_minimizedWindows;
    CustomMDISubWindow *m_activeWindow;
    MDITaskBar *m_taskBar;
    
    int m_nextX;
    int m_nextY;
    static constexpr int CASCADE_OFFSET = 30;
};

#endif // CUSTOMMDIAREA_H
