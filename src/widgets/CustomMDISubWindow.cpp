#include "ui/CustomMDISubWindow.h"
#include "ui/CustomTitleBar.h"
#include "ui/CustomMDIArea.h"
#include <QMouseEvent>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QApplication>
#include <QDebug>

CustomMDISubWindow::CustomMDISubWindow(const QString &title, QWidget *parent)
    : QWidget(parent),
      m_contentWidget(nullptr),
      m_isMinimized(false),
      m_isMaximized(false),
      m_isPinned(false),
      m_isDragging(false),
      m_isResizing(false)
{
    // NO Qt::SubWindow flag - pure QWidget!
    setWindowFlags(Qt::Widget); // Stay as a child widget
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true); // For resize cursor
    setFocusPolicy(Qt::StrongFocus); // Allow focus for keyboard events
    setAttribute(Qt::WA_StyledBackground, true);  // Ensure stylesheet is applied

    // Main layout with margins for resize borders (must be >= border width)
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);  // 5px margin to show 4px border + extra space
    m_mainLayout->setSpacing(0);

    // Custom title bar
    m_titleBar = new CustomTitleBar(this);
    m_titleBar->setTitle(title);
    m_mainLayout->addWidget(m_titleBar);

    // Connect title bar signals
    connect(m_titleBar, &CustomTitleBar::minimizeClicked, this, &CustomMDISubWindow::minimizeRequested);
    connect(m_titleBar, &CustomTitleBar::maximizeClicked, this, &CustomMDISubWindow::maximizeRequested);
    connect(m_titleBar, &CustomTitleBar::closeClicked, this, &CustomMDISubWindow::closeRequested);

    // Connect drag signals for snapping support
    connect(m_titleBar, &CustomTitleBar::dragStarted, this, [this](const QPoint &globalPos)
            {
        // Activate window when titlebar is pressed
        emit windowActivated();

        m_dragStartPos = globalPos;
        m_dragStartGeometry = geometry();

        // If the press started on the resize border (top edge inside title bar), begin resize
        QPoint local = mapFromGlobal(globalPos);
        Qt::Edges edges;
        if (isOnResizeBorder(local, edges))
        {
            m_isResizing = true;
            m_resizeEdges = edges;
            qDebug() << "[MDISubWindow] start resize from titlebar, edges:" << edges;
        }
    });

    connect(m_titleBar, &CustomTitleBar::dragMoved, this, [this](const QPoint &globalPos)
            {
        if (m_isMaximized || m_isPinned) return;  // Don't move/resize if maximized or pinned

        QPoint delta = globalPos - m_dragStartPos;
        QRect newGeometry = m_dragStartGeometry;

        if (m_isResizing)
        {
            // Resize based on edges
            if (m_resizeEdges & Qt::LeftEdge)
                newGeometry.setLeft(m_dragStartGeometry.left() + delta.x());
            if (m_resizeEdges & Qt::TopEdge)
                newGeometry.setTop(m_dragStartGeometry.top() + delta.y());
            if (m_resizeEdges & Qt::RightEdge)
                newGeometry.setRight(m_dragStartGeometry.right() + delta.x());
            if (m_resizeEdges & Qt::BottomEdge)
                newGeometry.setBottom(m_dragStartGeometry.bottom() + delta.y());

            // Enforce minimum size
            if (newGeometry.width() < 200)
                newGeometry.setWidth(200);
            if (newGeometry.height() < 150)
                newGeometry.setHeight(150);

            setGeometry(newGeometry);
        }
        else
        {
            newGeometry.translate(delta);
            CustomMDIArea *mdiArea = qobject_cast<CustomMDIArea*>(parentWidget());
            if (mdiArea) {
                QRect snapped = mdiArea->getSnappedGeometry(newGeometry);
                mdiArea->showSnapPreview(snapped);
                setGeometry(newGeometry);  // Show actual position while dragging
            } else {
                setGeometry(newGeometry);
            }
        }
    });

    connect(m_titleBar, &CustomTitleBar::dragEnded, this, [this]()
            {
        CustomMDIArea *mdiArea = qobject_cast<CustomMDIArea*>(parentWidget());
        if (m_isResizing)
        {
            // End resizing
            m_isResizing = false;
            m_resizeEdges = Qt::Edges();
        }

        if (mdiArea) {
            // Apply snap on release
            QRect snapped = mdiArea->getSnappedGeometry(geometry());
            setGeometry(snapped);
            mdiArea->hideSnapPreview();
        }
    });

    // Styling with HIGHLY VISIBLE borders
    setAutoFillBackground(true);
    setStyleSheet(
        "CustomMDISubWindow { "
        "   background-color: #1e1e1e; "
        "   border: 4px solid #00ffff; "  // Bright cyan - highly visible!
        "}");

    resize(800, 600);

    qDebug() << "CustomMDISubWindow created:" << title;
}

void CustomMDISubWindow::setActive(bool active)
{
    if (m_titleBar)
        m_titleBar->setActive(active);
    
    // Update border color based on active state
    if (!m_isPinned)
    {
        if (active)
        {
            setStyleSheet(
                "CustomMDISubWindow { "
                "   background-color: #1e1e1e; "
                "   border: 4px solid #00ffff; "  // Bright cyan for active
                "}");
        }
        else
        {
            setStyleSheet(
                "CustomMDISubWindow { "
                "   background-color: #1e1e1e; "
                "   border: 4px solid #00aaaa; "  // Dimmer cyan for inactive
                "}");
        }
    }
}

CustomMDISubWindow::~CustomMDISubWindow()
{
    qDebug() << "CustomMDISubWindow destroyed:" << title();
}

void CustomMDISubWindow::closeEvent(QCloseEvent *event)
{
    // Remove ourselves from the MDI area before closing
    CustomMDIArea *mdiArea = qobject_cast<CustomMDIArea *>(parent());
    if (mdiArea)
    {
        mdiArea->removeWindow(this);
    }

    QWidget::closeEvent(event);
}

void CustomMDISubWindow::setContentWidget(QWidget *widget)
{
    if (m_contentWidget)
    {
        m_mainLayout->removeWidget(m_contentWidget);
        m_contentWidget->removeEventFilter(this);
        m_contentWidget->setParent(nullptr);
    }

    m_contentWidget = widget;
    if (m_contentWidget)
    {
        m_mainLayout->addWidget(m_contentWidget);
        // Install event filter to capture mouse events near edges
        m_contentWidget->installEventFilter(this);
    }
}

void CustomMDISubWindow::setTitle(const QString &title)
{
    m_titleBar->setTitle(title);
}

QString CustomMDISubWindow::title() const
{
    return m_titleBar->title();
}

void CustomMDISubWindow::minimize()
{
    m_isMinimized = true;
    hide();
}

void CustomMDISubWindow::restore()
{
    if (m_isMaximized)
    {
        setGeometry(m_normalGeometry);
        m_isMaximized = false;
    }

    m_isMinimized = false;
    show();
}

void CustomMDISubWindow::maximize()
{
    if (!m_isMaximized)
    {
        m_normalGeometry = geometry();

        // Maximize within parent
        if (parentWidget())
        {
            setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
        }

        m_isMaximized = true;
    }
    else
    {
        restore();
    }
}

void CustomMDISubWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        qDebug() << "[MDISubWindow]" << title() << "mousePressEvent at" << event->pos();
        emit windowActivated();

        // Check if on resize border
        Qt::Edges edges;
        if (isOnResizeBorder(event->pos(), edges))
        {
            qDebug() << "[MDISubWindow]" << title() << "starting resize, edges:" << edges;
            m_isResizing = true;
            m_resizeEdges = edges;
            m_dragStartPos = event->globalPos();
            m_dragStartGeometry = geometry();
            event->accept();
            return;
        }
    }

    QWidget::mousePressEvent(event);
}

void CustomMDISubWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isResizing && (event->buttons() & Qt::LeftButton))
    {
        qDebug() << "[MDISubWindow]" << title() << "resizing, pos:" << event->pos();
        QPoint delta = event->globalPos() - m_dragStartPos;
        QRect newGeometry = m_dragStartGeometry;

        // Resize based on edges
        if (m_resizeEdges & Qt::LeftEdge)
        {
            newGeometry.setLeft(m_dragStartGeometry.left() + delta.x());
        }
        if (m_resizeEdges & Qt::TopEdge)
        {
            newGeometry.setTop(m_dragStartGeometry.top() + delta.y());
        }
        if (m_resizeEdges & Qt::RightEdge)
        {
            newGeometry.setRight(m_dragStartGeometry.right() + delta.x());
        }
        if (m_resizeEdges & Qt::BottomEdge)
        {
            newGeometry.setBottom(m_dragStartGeometry.bottom() + delta.y());
        }

        // Enforce minimum size
        if (newGeometry.width() < 200)
            newGeometry.setWidth(200);
        if (newGeometry.height() < 150)
            newGeometry.setHeight(150);

        setGeometry(newGeometry);
        event->accept();
        return;
    }

    // Update cursor for resize
    if (!m_isResizing)
    {
        updateCursor(event->pos());
    }

    QWidget::mouseMoveEvent(event);
}

void CustomMDISubWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_isResizing = false;
        m_resizeEdges = Qt::Edges();
    }

    QWidget::mouseReleaseEvent(event);
}

void CustomMDISubWindow::focusInEvent(QFocusEvent *event)
{
    emit windowActivated();
    QWidget::focusInEvent(event);
}

bool CustomMDISubWindow::isOnResizeBorder(const QPoint &pos, Qt::Edges &edges) const
{
    edges = Qt::Edges();

    if (pos.x() < RESIZE_BORDER_WIDTH)
        edges |= Qt::LeftEdge;
    if (pos.x() > width() - RESIZE_BORDER_WIDTH)
        edges |= Qt::RightEdge;
    if (pos.y() < RESIZE_BORDER_WIDTH)
        edges |= Qt::TopEdge;
    if (pos.y() > height() - RESIZE_BORDER_WIDTH)
        edges |= Qt::BottomEdge;

    return edges != Qt::Edges();
}

void CustomMDISubWindow::updateCursor(const QPoint &pos)
{
    Qt::Edges edges;
    if (isOnResizeBorder(pos, edges))
    {
        if ((edges & Qt::LeftEdge) && (edges & Qt::TopEdge))
        {
            setCursor(Qt::SizeFDiagCursor);
        }
        else if ((edges & Qt::RightEdge) && (edges & Qt::TopEdge))
        {
            setCursor(Qt::SizeBDiagCursor);
        }
        else if ((edges & Qt::LeftEdge) && (edges & Qt::BottomEdge))
        {
            setCursor(Qt::SizeBDiagCursor);
        }
        else if ((edges & Qt::RightEdge) && (edges & Qt::BottomEdge))
        {
            setCursor(Qt::SizeFDiagCursor);
        }
        else if (edges & (Qt::LeftEdge | Qt::RightEdge))
        {
            setCursor(Qt::SizeHorCursor);
        }
        else if (edges & (Qt::TopEdge | Qt::BottomEdge))
        {
            setCursor(Qt::SizeVerCursor);
        }
    }
    else
    {
        setCursor(Qt::ArrowCursor);
    }
}

void CustomMDISubWindow::setPinned(bool pinned)
{
    m_isPinned = pinned;
    if (m_isPinned)
    {
        raise();
        setStyleSheet(
            "CustomMDISubWindow { "
            "   background-color: #1e1e1e; "
            "   border: 4px solid #ffff00; " // Bright yellow for pinned!
            "}");
    }
    else
    {
        setStyleSheet(
            "CustomMDISubWindow { "
            "   background-color: #1e1e1e; "
            "   border: 4px solid #00ffff; "  // Bright cyan!
            "}");
    }
}

void CustomMDISubWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    QAction *pinAction = menu.addAction(m_isPinned ? "Unpin Window" : "Pin Window");
    menu.addSeparator();

    QAction *minimizeAction = menu.addAction("Minimize");
    QAction *maximizeAction = menu.addAction(m_isMaximized ? "Restore" : "Maximize");
    menu.addSeparator();

    QAction *closeAction = menu.addAction("Close");
    QAction *closeOthersAction = menu.addAction("Close All Others");

    QAction *selected = menu.exec(event->globalPos());

    if (selected == pinAction)
    {
        setPinned(!m_isPinned);
    }
    else if (selected == minimizeAction)
    {
        emit minimizeRequested();
    }
    else if (selected == maximizeAction)
    {
        emit maximizeRequested();
    }
    else if (selected == closeAction)
    {
        close();
    }
    else if (selected == closeOthersAction)
    {
        CustomMDIArea *mdiArea = qobject_cast<CustomMDIArea *>(parent());
        if (mdiArea)
        {
            QList<CustomMDISubWindow *> windows = mdiArea->windowList();
            for (CustomMDISubWindow *win : windows)
            {
                if (win != this)
                {
                    win->close();
                }
            }
        }
    }

    event->accept();
}

bool CustomMDISubWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Filter events from content widget to handle resizing from edges
    if (watched == m_contentWidget)
    {
        if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            // Map content widget position to subwindow coordinates
            QPoint subWindowPos = m_contentWidget->mapTo(this, mouseEvent->pos());
            
            // Check if we're near the edge (within resize border area)
            Qt::Edges edges;
            if (isOnResizeBorder(subWindowPos, edges))
            {
                updateCursor(subWindowPos);
                
                // If already resizing, handle the resize
                if (m_isResizing && (mouseEvent->buttons() & Qt::LeftButton))
                {
                    QPoint delta = mouseEvent->globalPos() - m_dragStartPos;
                    QRect newGeometry = m_dragStartGeometry;

                    // Resize based on edges
                    if (m_resizeEdges & Qt::LeftEdge)
                        newGeometry.setLeft(m_dragStartGeometry.left() + delta.x());
                    if (m_resizeEdges & Qt::TopEdge)
                        newGeometry.setTop(m_dragStartGeometry.top() + delta.y());
                    if (m_resizeEdges & Qt::RightEdge)
                        newGeometry.setRight(m_dragStartGeometry.right() + delta.x());
                    if (m_resizeEdges & Qt::BottomEdge)
                        newGeometry.setBottom(m_dragStartGeometry.bottom() + delta.y());

                    // Enforce minimum size
                    if (newGeometry.width() < 200)
                        newGeometry.setWidth(200);
                    if (newGeometry.height() < 150)
                        newGeometry.setHeight(150);

                    setGeometry(newGeometry);
                    return true;  // Event handled
                }
            }
            else if (!m_isResizing)
            {
                // Reset cursor if not on border
                m_contentWidget->setCursor(Qt::ArrowCursor);
            }
        }
        else if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                QPoint subWindowPos = m_contentWidget->mapTo(this, mouseEvent->pos());
                Qt::Edges edges;
                if (isOnResizeBorder(subWindowPos, edges))
                {
                    qDebug() << "[MDISubWindow]" << title() << "starting resize from content, edges:" << edges;
                    m_isResizing = true;
                    m_resizeEdges = edges;
                    m_dragStartPos = mouseEvent->globalPos();
                    m_dragStartGeometry = geometry();
                    emit windowActivated();
                    return true;  // Event handled
                }
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && m_isResizing)
            {
                m_isResizing = false;
                m_resizeEdges = Qt::Edges();
                return true;  // Event handled
            }
        }
    }
    
    return QWidget::eventFilter(watched, event);
}
