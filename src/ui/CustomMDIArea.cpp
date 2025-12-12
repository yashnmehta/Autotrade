#include "ui/CustomMDIArea.h"
#include "ui/CustomMDISubWindow.h"
#include "ui/MDITaskBar.h"
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QDebug>

CustomMDIArea::CustomMDIArea(QWidget *parent)
    : QWidget(parent),
      m_activeWindow(nullptr),
      m_nextX(20),
      m_nextY(20)
{
    // Dark background
    setStyleSheet("background-color: #1e1e1e;");
    
    // Main layout with taskbar at bottom
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Spacer for window area (windows are direct children)
    layout->addStretch();
    
    // TaskBar at bottom
    m_taskBar = new MDITaskBar(this);
    layout->addWidget(m_taskBar);
    
    connect(m_taskBar, &MDITaskBar::windowRestoreRequested, 
            this, &CustomMDIArea::restoreWindow);
}

CustomMDIArea::~CustomMDIArea()
{
    // Clean up windows
    qDeleteAll(m_windows);
}

void CustomMDIArea::addWindow(CustomMDISubWindow *window)
{
    if (!window || m_windows.contains(window)) {
        return;
    }
    
    // Set parent to this MDI area
    window->setParent(this);
    
    // Position window
    QPoint pos = getNextWindowPosition();
    window->move(pos);
    
    // Add to list
    m_windows.append(window);
    
    // Show and activate
    window->show();
    window->raise();
    activateWindow(window);
    
    // Install event filter to track activation
    window->installEventFilter(this);
    
    emit windowAdded(window);
    
    qDebug() << "CustomMDIArea: Window added" << window->title();
}

void CustomMDIArea::removeWindow(CustomMDISubWindow *window)
{
    if (!window) return;
    
    m_windows.removeOne(window);
    m_minimizedWindows.removeOne(window);
    
    if (m_activeWindow == window) {
        m_activeWindow = nullptr;
        
        // Activate another window if available
        if (!m_windows.isEmpty()) {
            activateWindow(m_windows.last());
        }
    }
    
    emit windowRemoved(window);
    
    qDebug() << "CustomMDIArea: Window removed" << window->title();
}

void CustomMDIArea::activateWindow(CustomMDISubWindow *window)
{
    if (!window || window == m_activeWindow) {
        return;
    }
    
    // Deactivate old window
    if (m_activeWindow) {
        m_activeWindow->setStyleSheet(
            "CustomMDISubWindow { border: 1px solid #3e3e42; }"
        );
    }
    
    // Activate new window
    m_activeWindow = window;
    window->raise();
    window->setFocus();
    window->setStyleSheet(
        "CustomMDISubWindow { border: 1px solid #007acc; }" // Active border (VS Code blue)
    );
    
    emit windowActivated(window);
    
    qDebug() << "CustomMDIArea: Window activated" << window->title();
}

void CustomMDIArea::minimizeWindow(CustomMDISubWindow *window)
{
    if (!window || window->isMinimized()) {
        return;
    }
    
    window->minimize();
    window->hide();
    
    m_minimizedWindows.append(window);
    m_taskBar->addWindow(window);
    
    qDebug() << "CustomMDIArea: Window minimized" << window->title();
}

void CustomMDIArea::restoreWindow(CustomMDISubWindow *window)
{
    if (!window || !window->isMinimized()) {
        return;
    }
    
    window->restore();
    window->show();
    window->raise();
    
    m_minimizedWindows.removeOne(window);
    m_taskBar->removeWindow(window);
    
    activateWindow(window);
    
    qDebug() << "CustomMDIArea: Window restored" << window->title();
}

CustomMDISubWindow* CustomMDIArea::activeWindow() const
{
    return m_activeWindow;
}

QList<CustomMDISubWindow*> CustomMDIArea::windowList() const
{
    return m_windows;
}

void CustomMDIArea::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // Adjust maximized windows
    for (CustomMDISubWindow *window : m_windows) {
        if (window->isMaximized()) {
            window->setGeometry(0, 0, width(), height() - m_taskBar->height());
        }
    }
}

bool CustomMDIArea::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        CustomMDISubWindow *window = qobject_cast<CustomMDISubWindow*>(watched);
        if (window && m_windows.contains(window)) {
            activateWindow(window);
        }
    }
    
    return QWidget::eventFilter(watched, event);
}

QPoint CustomMDIArea::getNextWindowPosition()
{
    QPoint pos(m_nextX, m_nextY);
    
    // Cascade offset
    m_nextX += CASCADE_OFFSET;
    m_nextY += CASCADE_OFFSET;
    
    // Reset if too far
    if (m_nextX > 400 || m_nextY > 400) {
        m_nextX = 20;
        m_nextY = 20;
    }
    
    return pos;
}

void CustomMDIArea::cascadeWindows()
{
    // TODO: Implement cascade layout
}

void CustomMDIArea::tileWindows()
{
    // TODO: Implement tile layout
}
