#include "ui/CustomMainWindow.h"
#include "ui/CustomTitleBar.h"
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>
#include <QDebug>

CustomMainWindow::CustomMainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_centralWidget(nullptr),
      m_isMaximized(false),
      m_isResizing(false),
      m_resizeDirection(None),
      m_pendingResize(false),
      m_pendingResizeStartPos(QPoint()),
      m_pendingResizeDirection(None),
      m_minimumSize(400, 300),
      m_maximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX)
{
    setupUi();
    applyDefaultStyling();

    // Enable mouse tracking for resize cursor changes
    setMouseTracking(true);
}

CustomMainWindow::~CustomMainWindow()
{
}

void CustomMainWindow::setupUi()
{
    // Frameless window with proper flags
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_Hover);

    // Default size
    resize(1280, 720);

    // If some layout already exists on this widget, try to reuse it (legacy cases)
    if (this->layout())
    {
        m_mainLayout = qobject_cast<QVBoxLayout *>(this->layout());
        if (m_mainLayout)
        {
            QWidget *possibleCentral = m_mainLayout->parentWidget();
            if (possibleCentral)
            {
                // Make sure QMainWindow knows about the central widget
                QMainWindow::setCentralWidget(possibleCentral);
            }
        }
    }

    // Create a central container widget for QMainWindow and install layout on it if none exists
    QWidget *central = nullptr;
    if (!m_mainLayout)
    {
        central = new QWidget(this);
        central->setObjectName("centralContainer");
        m_mainLayout = new QVBoxLayout(central);
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_mainLayout->setSpacing(0);
        QMainWindow::setCentralWidget(central);
    }
    else
    {
        // central will be the parent widget of the existing layout
        central = m_mainLayout->parentWidget();
    }
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create title bar outside the central widget and place it in the QMainWindow's menu/widget area
    // This ensures any QToolBar added to the QMainWindow will appear below the title bar
    m_titleBar = new CustomTitleBar(this);
    QMainWindow::setMenuWidget(m_titleBar);

    // Set the central widget on QMainWindow (central contains the main layout without the title bar)
    QMainWindow::setCentralWidget(central);

    // Install event filter on title bar to handle resize events
    m_titleBar->installEventFilter(this);

    // Install event filter on title bar children (buttons, labels) to handle resize from button areas
    for (QObject *child : m_titleBar->children())
    {
        if (qobject_cast<QWidget *>(child))
        {
            child->installEventFilter(this);
        }
    }

    // Connect title bar signals
    connect(m_titleBar, &CustomTitleBar::minimizeClicked, this, &CustomMainWindow::showMinimized);
    connect(m_titleBar, &CustomTitleBar::maximizeClicked, this, &CustomMainWindow::toggleMaximize);
    connect(m_titleBar, &CustomTitleBar::closeClicked, this, &QWidget::close);
    
    // Connect drag signals to move the window
    connect(m_titleBar, &CustomTitleBar::dragMoved, this, [this](const QPoint &globalPos) {
        if (!m_isMaximized && !m_isResizing) {
            move(globalPos - m_titleBar->mapFromGlobal(globalPos) - QPoint(0, 0));
        }
    });

    qDebug() << "CustomMainWindow created";
}

void CustomMainWindow::applyDefaultStyling()
{
    setStyleSheet(
        "CustomMainWindow { "
        "   background-color: #1e1e1e; "
        "   border: 1px solid #3e3e42; "
        "}");
}

void CustomMainWindow::setCentralWidget(QWidget *widget)
{
    if (m_centralWidget)
    {
        m_mainLayout->removeWidget(m_centralWidget);
        m_centralWidget->setParent(nullptr);
    }

    m_centralWidget = widget;
    if (m_centralWidget)
    {
        m_mainLayout->addWidget(m_centralWidget);
    }
}

void CustomMainWindow::setTitle(const QString &title)
{
    m_titleBar->setTitle(title);
    setWindowTitle(title); // For taskbar
}

QString CustomMainWindow::title() const
{
    return m_titleBar->title();
}

void CustomMainWindow::setMinimumSize(int minw, int minh)
{
    m_minimumSize = QSize(minw, minh);
    QWidget::setMinimumSize(minw, minh);
}

void CustomMainWindow::setMaximumSize(int maxw, int maxh)
{
    m_maximumSize = QSize(maxw, maxh);
    QWidget::setMaximumSize(maxw, maxh);
}

void CustomMainWindow::showMinimized()
{
    QWidget::showMinimized();
}

void CustomMainWindow::showMaximized()
{
    if (!m_isMaximized)
    {
        saveNormalGeometry();

        // Get available screen geometry (excluding taskbar)
        QScreen *screen = QApplication::screenAt(geometry().center());
        if (!screen)
        {
            screen = QApplication::primaryScreen();
        }

        QRect screenGeometry = screen->availableGeometry();
        setGeometry(screenGeometry);

        m_isMaximized = true;
        emit windowStateChanged(true);

        qDebug() << "Window maximized to" << screenGeometry;
    }
}

void CustomMainWindow::showNormal()
{
    if (m_isMaximized)
    {
        restoreNormalGeometry();
        m_isMaximized = false;
        emit windowStateChanged(false);

        qDebug() << "Window restored to" << geometry();
    }
}

void CustomMainWindow::toggleMaximize()
{
    if (m_isMaximized)
    {
        showNormal();
    }
    else
    {
        showMaximized();
    }
}

void CustomMainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_isMaximized)
    {
        // Detect if clicking on resize border
        m_resizeDirection = detectResizeDirection(event->pos());
        qDebug() << "[MainWindow Press] pos:" << event->pos() << "globalPos:" << event->globalPos()
                 << "detected direction:" << m_resizeDirection << "windowSize:" << size();

        if (m_resizeDirection != None)
        {
            m_isResizing = true;
            m_resizeStartPos = event->globalPos();
            m_resizeStartGeometry = geometry();
            event->accept();

            qDebug() << "[MainWindow] Resize started, direction:" << m_resizeDirection;
            return;
        }
    }

    QWidget::mousePressEvent(event);
}

void CustomMainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isResizing && (event->buttons() & Qt::LeftButton))
    {
        performResize(event->globalPos());
        event->accept();
        return;
    }

    // Update cursor based on position (only when not maximized and not resizing)
    if (!m_isMaximized && !m_isResizing)
    {
        ResizeDirection direction = detectResizeDirection(event->pos());
        updateCursorShape(direction);
    }

    QWidget::mouseMoveEvent(event);
}

void CustomMainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isResizing)
    {
        qDebug() << "Resize ended";
        m_isResizing = false;
        m_resizeDirection = None;
        updateCursorShape(None); // Reset cursor
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void CustomMainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Double-click on title bar area to maximize
    if (event->button() == Qt::LeftButton)
    {
        QRect titleBarRect = m_titleBar->geometry();
        if (titleBarRect.contains(event->pos()))
        {
            toggleMaximize();
            event->accept();
            return;
        }
    }

    QWidget::mouseDoubleClickEvent(event);
}

bool CustomMainWindow::event(QEvent *event)
{
    // Handle hover events for cursor changes
    if (event->type() == QEvent::HoverMove && !m_isMaximized && !m_isResizing)
    {
        QHoverEvent *hoverEvent = static_cast<QHoverEvent *>(event);
        ResizeDirection direction = detectResizeDirection(hoverEvent->pos());
        updateCursorShape(direction);
    }

    return QWidget::event(event);
}

void CustomMainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        // Handle minimize/restore from taskbar
        if (windowState() & Qt::WindowMinimized)
        {
            qDebug() << "Window minimized";
        }
        else if (windowState() & Qt::WindowMaximized)
        {
            m_isMaximized = true;
            emit windowStateChanged(true);
        }
        else
        {
            m_isMaximized = false;
            emit windowStateChanged(false);
        }
    }

    QWidget::changeEvent(event);
}

CustomMainWindow::ResizeDirection CustomMainWindow::detectResizeDirection(const QPoint &pos) const
{
    int direction = None;

    // Check edges
    if (pos.x() <= RESIZE_BORDER_WIDTH)
    {
        direction |= Left;
    }
    else if (pos.x() >= width() - RESIZE_BORDER_WIDTH)
    {
        direction |= Right;
    }

    if (pos.y() <= RESIZE_BORDER_WIDTH)
    {
        direction |= Top;
    }
    else if (pos.y() >= height() - RESIZE_BORDER_WIDTH)
    {
        direction |= Bottom;
    }

    return static_cast<ResizeDirection>(direction);
}

void CustomMainWindow::updateCursorShape(ResizeDirection direction)
{
    switch (direction)
    {
    case TopLeft:
    case BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case TopRight:
    case BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case Top:
    case Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case Left:
    case Right:
        setCursor(Qt::SizeHorCursor);
        break;
    default:
        setCursor(Qt::ArrowCursor);
        break;
    }
}

void CustomMainWindow::performResize(const QPoint &globalPos)
{
    QPoint delta = globalPos - m_resizeStartPos;
    QRect newGeometry = m_resizeStartGeometry;

    // Calculate new geometry based on resize direction
    if (m_resizeDirection & Left)
    {
        int newLeft = m_resizeStartGeometry.left() + delta.x();
        int maxLeft = m_resizeStartGeometry.right() - m_minimumSize.width();
        newGeometry.setLeft(qMin(newLeft, maxLeft));
    }

    if (m_resizeDirection & Right)
    {
        int newRight = m_resizeStartGeometry.right() + delta.x();
        int minRight = m_resizeStartGeometry.left() + m_minimumSize.width();
        newGeometry.setRight(qMax(newRight, minRight));
    }

    if (m_resizeDirection & Top)
    {
        int newTop = m_resizeStartGeometry.top() + delta.y();
        int maxTop = m_resizeStartGeometry.bottom() - m_minimumSize.height();
        newGeometry.setTop(qMin(newTop, maxTop));
    }

    if (m_resizeDirection & Bottom)
    {
        int newBottom = m_resizeStartGeometry.bottom() + delta.y();
        int minBottom = m_resizeStartGeometry.top() + m_minimumSize.height();
        newGeometry.setBottom(qMax(newBottom, minBottom));
    }

    // Enforce maximum size
    if (newGeometry.width() > m_maximumSize.width())
    {
        if (m_resizeDirection & Left)
        {
            newGeometry.setLeft(newGeometry.right() - m_maximumSize.width());
        }
        else
        {
            newGeometry.setRight(newGeometry.left() + m_maximumSize.width());
        }
    }

    if (newGeometry.height() > m_maximumSize.height())
    {
        if (m_resizeDirection & Top)
        {
            newGeometry.setTop(newGeometry.bottom() - m_maximumSize.height());
        }
        else
        {
            newGeometry.setBottom(newGeometry.top() + m_maximumSize.height());
        }
    }

    setGeometry(newGeometry);
}

void CustomMainWindow::saveNormalGeometry()
{
    m_normalGeometry = geometry();
}

void CustomMainWindow::restoreNormalGeometry()
{
    if (m_normalGeometry.isValid())
    {
        setGeometry(m_normalGeometry);
    }
}

bool CustomMainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_titleBar && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && !m_isMaximized)
        {
            // Map titleBar local coordinates to MainWindow coordinates for accurate detection
            QPoint windowPos = m_titleBar->mapTo(this, mouseEvent->pos());
            ResizeDirection direction = detectResizeDirection(windowPos);

            qDebug() << "[Press] titleBarPos:" << mouseEvent->pos() << "windowPos:" << windowPos
                     << "direction:" << direction;

            if (direction != None)
            {
                // Handle resize directly
                m_resizeDirection = direction;
                m_isResizing = true;
                m_resizeStartPos = mouseEvent->globalPos();
                m_resizeStartGeometry = geometry();

                qDebug() << "[Resize Start] direction:" << m_resizeDirection;
                return true; // Consume the event
            }
        }
    }
    // Also handle events from title bar children (buttons)
    else if (m_titleBar->isAncestorOf(qobject_cast<QWidget *>(watched)) && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && !m_isMaximized)
        {
            // Map child widget coordinates to MainWindow coordinates
            QWidget *childWidget = qobject_cast<QWidget *>(watched);
            QPoint windowPos = childWidget->mapTo(this, mouseEvent->pos());
            ResizeDirection direction = detectResizeDirection(windowPos);

            qDebug() << "[Press Child]" << childWidget->objectName() << "childPos:" << mouseEvent->pos()
                     << "windowPos:" << windowPos << "direction:" << direction;

            if (direction != None)
            {
                m_resizeDirection = direction;
                m_isResizing = true;
                m_resizeStartPos = mouseEvent->globalPos();
                m_resizeStartGeometry = geometry();
                qDebug() << "[Resize Start Child] direction:" << m_resizeDirection;
                return true; // Consume to start resize instead of button click
            }
        }
    }
    else if (watched == m_titleBar && event->type() == QEvent::MouseMove)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (m_isResizing && (mouseEvent->buttons() & Qt::LeftButton))
        {
            performResize(mouseEvent->globalPos());
            return true; // We are actively resizing, consume the event
        }
        else if (!m_isMaximized && !m_isResizing)
        {
            // Map titleBar coordinates to MainWindow for accurate cursor detection
            QPoint windowPos = m_titleBar->mapTo(this, mouseEvent->pos());
            ResizeDirection direction = detectResizeDirection(windowPos);
            updateCursorShape(direction);
            // do NOT return true; let title bar handle the underlying event (dragging)
        }
    }
    // Handle move events from title bar children
    else if (m_titleBar->isAncestorOf(qobject_cast<QWidget *>(watched)) && event->type() == QEvent::MouseMove)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (m_isResizing && (mouseEvent->buttons() & Qt::LeftButton))
        {
            performResize(mouseEvent->globalPos());
            return true;
        }
        else if (!m_isMaximized && !m_isResizing)
        {
            // Map child coordinates to MainWindow
            QWidget *childWidget = qobject_cast<QWidget *>(watched);
            QPoint windowPos = childWidget->mapTo(this, mouseEvent->pos());
            ResizeDirection direction = detectResizeDirection(windowPos);
            updateCursorShape(direction);
        }
    }
    else if ((watched == m_titleBar || m_titleBar->isAncestorOf(qobject_cast<QWidget *>(watched))) && event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_isResizing)
        {
            qDebug() << "[Resize End]";
            m_isResizing = false;
            m_resizeDirection = None;
            updateCursorShape(None);
            return true;
        }
    }

    // Let other events pass through
    return QWidget::eventFilter(watched, event);
}
