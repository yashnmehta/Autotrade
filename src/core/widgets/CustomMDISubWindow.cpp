#include "core/widgets/CustomMDISubWindow.h"
#include "core/widgets/CustomTitleBar.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/MDITaskBar.h"
#include "core/WindowCacheManager.h"
#include <QMouseEvent>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QMenu>
#include <QApplication>
#include <QTimer>
#include <QPainterPath>
#include <QSettings>
#include <QDebug>

CustomMDISubWindow::CustomMDISubWindow(const QString &title, QWidget *parent)
    : QWidget(parent),
      m_contentWidget(nullptr),
      m_isMinimized(false),
      m_isMaximized(false),
      m_isPinned(false),
      m_isCached(false),
      m_isDragging(false),
      m_isResizing(false)
{
    // NO Qt::SubWindow flag - pure QWidget!
    setWindowFlags(Qt::Widget); // Stay as a child widget
    setMouseTracking(true); // For resize cursor
    setFocusPolicy(Qt::StrongFocus); // Allow focus for keyboard events
    setAttribute(Qt::WA_StyledBackground, true);  // Ensure stylesheet is applied
    setAttribute(Qt::WA_OpaquePaintEvent, false);  // Allow border painting
    setAutoFillBackground(false);  // Let stylesheet handle background

    // Main layout with NO margins (so title bar spans full width)
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Custom title bar
    m_titleBar = new CustomTitleBar(this);
    m_titleBar->setTitle(title);
    m_mainLayout->addWidget(m_titleBar);

    // Container for content with margins for resize area
    QWidget *contentContainer = new QWidget(this);
    contentContainer->setObjectName("contentContainer");
    QVBoxLayout *containerLayout = new QVBoxLayout(contentContainer);
    // Margin of 5px on sides/bottom for resizing. Top is handled by titlebar.
    // We use a value slightly smaller than RESIZE_BORDER_WIDTH to ensure hit detection
    containerLayout->setContentsMargins(5, 0, 5, 5); 
    containerLayout->setSpacing(0);
    m_mainLayout->addWidget(contentContainer);
    
    // We store the pointer to add/remove widgets later
    m_contentLayout = containerLayout;

    // Connect title bar signals
    // Minimize: Goes through MDIArea (needs taskbar management)
    connect(m_titleBar, &CustomTitleBar::minimizeClicked, this, &CustomMDISubWindow::minimizeRequested);
    // Maximize: Handle DIRECTLY in this window (toggle maximize/restore)
    connect(m_titleBar, &CustomTitleBar::maximizeClicked, this, &CustomMDISubWindow::maximize);
    // Close: Signal to parent for proper cleanup
    connect(m_titleBar, &CustomTitleBar::closeClicked, this, &CustomMDISubWindow::close);

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

    // Styling with VS Code-like borders
    setStyleSheet(
        "CustomMDISubWindow { "
        "   background-color: #1e1e1e; "
        "   border: 2px solid #007acc; "
        "}");

    resize(800, 600);
    qDebug() << "[MDISubWindow] Created:" << title;
}

void CustomMDISubWindow::setActive(bool active)
{
    if (m_titleBar)
        m_titleBar->setActive(active);
    
    if (!m_isPinned)
    {
        // Use a single style update to prevent flickering
        QString borderColor = active ? "#007acc" : "#3e3e42";
        setStyleSheet(QString(
            "CustomMDISubWindow { "
            "   background-color: #1e1e1e; "
            "   border: 2px solid %1; "
            "}"
        ).arg(borderColor));
    }
}

CustomMDISubWindow::~CustomMDISubWindow()
{
    qDebug() << "CustomMDISubWindow destroyed:" << title();
}

void CustomMDISubWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << "[MDISubWindow] closeEvent for" << title();
    
    // If this is a cached window, move off-screen instead of hiding (MUCH faster on re-show!)
    if (m_isCached) {
        qDebug() << "[MDISubWindow] Cached window - moving off-screen instead of closing";
        
        // Save position for Buy/Sell order windows (only if in visible area!)
        if (m_windowType == "BuyWindow" || m_windowType == "SellWindow") {
            QPoint windowPos = geometry().topLeft();  // Use geometry() which gives correct position
            
            // Only save if window is NOT off-screen (avoid saving -10000,-10000)
            if (windowPos.x() >= -1000 && windowPos.y() >= -1000) {
                QSettings settings("TradingCompany", "TradingTerminal");
                settings.setValue("orderwindow/last_x", windowPos.x());
                settings.setValue("orderwindow/last_y", windowPos.y());
                qDebug() << "[MDISubWindow] Saved position:" << windowPos;
            } else {
                qDebug() << "[MDISubWindow] Skipping save - window is off-screen:" << windowPos;
            }
            
            // Notify WindowCacheManager that this window was closed (needs reset on next show)
            if (m_windowType == "BuyWindow") {
                WindowCacheManager::instance().markBuyWindowClosed();
            } else if (m_windowType == "SellWindow") {
                WindowCacheManager::instance().markSellWindowClosed();
            }
        }
        
        // Mark SnapQuote as needing reset if closed
        if (m_windowType == "SnapQuote") {
            // Get window index from property (set during creation)
            int windowIndex = property("snapQuoteIndex").toInt();
            WindowCacheManager::instance().markSnapQuoteWindowClosed(windowIndex);
        }
        
        event->ignore();  // Prevent actual close
        
        // ⚡ CRITICAL OPTIMIZATION: Move off-screen instead of hide() (10x faster on re-show!)
        // hide() forces expensive show() later with layout recalc, paint events, etc.
        // Moving off-screen keeps window visible but invisible to user (instant repositioning!)
        move(-10000, -10000);
        lower();  // Send to back so it doesn't interfere
        qDebug() << "[MDISubWindow] ⚡ Moved off-screen (still visible, fast re-show!)";
        
        return;
    }
    
    // Try to close content widget first so it can save state
    // if (m_contentWidget) {
    //     m_contentWidget->close();
    // }

    // Remove ourselves from the MDI area before closing
    CustomMDIArea *mdiArea = qobject_cast<CustomMDIArea *>(parent());
    if (mdiArea)
    {
        mdiArea->removeWindow(this);
    }

    QWidget::closeEvent(event);
}

void CustomMDISubWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        if (m_windowType != "MarketWatch")
        {
            qDebug() << "[MDISubWindow] Escape pressed - closing" << title();
            close();
        }
        return;
    }
    QWidget::keyPressEvent(event);
}

void CustomMDISubWindow::setContentWidget(QWidget *widget)
{
    if (m_contentWidget)
    {
        m_contentLayout->removeWidget(m_contentWidget);
        m_contentWidget->removeEventFilter(this);
        m_contentWidget->setParent(nullptr);
    }

    m_contentWidget = widget;
    if (m_contentWidget)
    {
        m_contentLayout->addWidget(m_contentWidget);
        // Install event filter as a fallback for the very edges
        m_contentWidget->installEventFilter(this);
        
        // Ensure content widget has its own bg or transparency correctly
        m_contentWidget->setAttribute(Qt::WA_StyledBackground, true);
        
        // Set focus to content widget
        QTimer::singleShot(0, m_contentWidget, [this]() {
            if (m_contentWidget) {
                m_contentWidget->setFocus();
            }
        });
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
    qDebug() << "[MDISubWindow] maximize() called for" << title() << "isMaximized:" << m_isMaximized;
    
    // Toggle behavior - if already maximized, restore
    if (m_isMaximized)
    {
        qDebug() << "[MDISubWindow] Already maximized, restoring...";
        restore();
        return;
    }
    
    // Save current geometry for later restore
    m_normalGeometry = geometry();
    
    // Set maximized state FIRST to prevent re-entry
    m_isMaximized = true;

    CustomMDIArea *mdiArea = qobject_cast<CustomMDIArea*>(parentWidget());
    if (mdiArea)
    {
        int availableHeight = mdiArea->height();
        if (mdiArea->taskBar() && mdiArea->taskBar()->isVisible()) {
            availableHeight -= mdiArea->taskBar()->height();
        }
        QRect maxGeom(0, 0, mdiArea->width(), availableHeight);
        qDebug() << "[MDISubWindow]" << title() << "Maximized to" << maxGeom;
        setGeometry(maxGeom);
    }
    else if (parentWidget())
    {
        setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
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
        QPoint delta = event->globalPos() - m_dragStartPos;
        QRect newGeometry = m_dragStartGeometry;

        // Use a more predictable resize logic
        if (m_resizeEdges & Qt::LeftEdge) {
            int newWidth = m_dragStartGeometry.width() - delta.x();
            if (newWidth >= minimumWidth()) {
                newGeometry.setLeft(m_dragStartGeometry.left() + delta.x());
            }
        }
        if (m_resizeEdges & Qt::RightEdge) {
            newGeometry.setRight(m_dragStartGeometry.right() + delta.x());
        }
        if (m_resizeEdges & Qt::TopEdge) {
            int newHeight = m_dragStartGeometry.height() - delta.y();
            if (newHeight >= minimumHeight()) {
                newGeometry.setTop(m_dragStartGeometry.top() + delta.y());
            }
        }
        if (m_resizeEdges & Qt::BottomEdge) {
            newGeometry.setBottom(m_dragStartGeometry.bottom() + delta.y());
        }

        // Apply constraints
        // Respect the widget's minimum size (propagated from content layout)
        QSize minSize = minimumSizeHint().expandedTo(minimumSize());
        int minW = qMax(200, minSize.width());
        int minH = qMax(100, minSize.height());

        if (newGeometry.width() < minW) newGeometry.setWidth(minW);
        if (newGeometry.height() < minH) newGeometry.setHeight(minH);

        setGeometry(newGeometry);
        event->accept();
        return;
    }

    // Always update cursor when mouse moves over the subwindow frame
    updateCursor(event->pos());
    QWidget::mouseMoveEvent(event);
}

void CustomMDISubWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        // Save position after dragging for cached order windows
        if (m_isDragging && m_isCached) {
            if (m_windowType == "BuyWindow" || m_windowType == "SellWindow") {
                QSettings settings("TradingCompany", "TradingTerminal");
                QPoint windowPos = geometry().topLeft();  // Use geometry() which gives correct position
                settings.setValue("orderwindow/last_x", windowPos.x());
                settings.setValue("orderwindow/last_y", windowPos.y());
                qDebug() << "[MDISubWindow] Saved position after drag:" << windowPos;
            }
        }
        
        m_isDragging = false;
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
    if (m_isMaximized) return false;
    
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
            "   border: 2px solid #ce9178; " // Subtle orange for pinned
            "   margin: 0px; "
            "   padding: 0px; "
            "}");
    }
    else
    {
        setStyleSheet(
            "CustomMDISubWindow { "
            "   background-color: #1e1e1e; "
            "   border: 2px solid #007acc; "  // VS Code blue
            "   margin: 0px; "
            "   padding: 0px; "
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
    menu.addSeparator();
    QAction *customizeAction = menu.addAction("Customize");

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
    else if (selected == customizeAction)
    {
        emit customizeRequested();
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

void CustomMDISubWindow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QColor borderColor;
    int borderWidth = 2; // Keep visual border thin for aesthetics
    
    if (m_isPinned)
    {
        borderColor = QColor(206, 145, 120);
    }
    else if (m_titleBar && m_titleBar->property("isActive").toBool())
    {
        borderColor = QColor(0, 122, 204);
    }
    else
    {
        borderColor = QColor(62, 62, 66);
    }
    
    QPen pen(borderColor, borderWidth);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    
    QRect borderRect = rect().adjusted(1, 1, -1, -1);
    painter.drawRect(borderRect);
    
    // Optional: draw corner handles visually
    if (!m_isMaximized && m_titleBar && m_titleBar->property("isActive").toBool()) {
        painter.setBrush(borderColor);
        int handleSize = 10;
        // Bottom right corner handle
        QRect brHandle(width() - handleSize, height() - handleSize, handleSize, handleSize);
        // Just a subtle triangle or dot
        QPainterPath path;
        path.moveTo(width(), height() - handleSize);
        path.lineTo(width(), height());
        path.lineTo(width() - handleSize, height());
        path.closeSubpath();
        painter.fillPath(path, borderColor);
    }
}
void CustomMDISubWindow::updateTitleBarVisibility()
{
    bool visible = property("titleBarVisible").toBool();
    if (m_titleBar)
    {
        m_titleBar->setVisible(visible);
    }
}
