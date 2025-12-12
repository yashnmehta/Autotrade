#ifndef CUSTOMMDISUBWINDOW_H
#define CUSTOMMDISUBWINDOW_H

#include <QWidget>
#include <QVBoxLayout>
#include <QPoint>
#include <QCloseEvent>

class CustomTitleBar;

/**
 * @brief Custom MDI SubWindow - Frameless, no Qt::SubWindow flag
 *
 * This is a pure QWidget-based window that behaves like an MDI child
 * but without Qt's SubWindow limitations. It's fully draggable, resizable,
 * and supports minimize/maximize within the MDI area.
 *
 * Key Design:
 * - Uses QWidget (NOT QMdiSubWindow)
 * - No Qt::SubWindow flag
 * - Manual drag/resize implementation
 * - Parent is CustomMDIArea (for clipping/stacking)
 */
class CustomMDISubWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CustomMDISubWindow(const QString &title, QWidget *parent = nullptr);
    ~CustomMDISubWindow();

    void setContentWidget(QWidget *widget);
    QWidget *contentWidget() const { return m_contentWidget; }

    void setTitle(const QString &title);
    QString title() const;

    // Window Type (for workspace save/load)
    void setWindowType(const QString &type) { m_windowType = type; }
    QString windowType() const { return m_windowType; }

    // Window State
    bool isMinimized() const { return m_isMinimized; }
    bool isMaximized() const { return m_isMaximized; }

    void minimize();
    void restore();
    void maximize();

    // Pinning
    void setPinned(bool pinned);
    bool isPinned() const { return m_isPinned; }

signals:
    void closeRequested();
    void minimizeRequested();
    void maximizeRequested();
    void windowActivated();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void setupResizeHandles();
    bool isOnResizeBorder(const QPoint &pos, Qt::Edges &edges) const;
    void updateCursor(const QPoint &pos);

    CustomTitleBar *m_titleBar;
    QWidget *m_contentWidget;
    QVBoxLayout *m_mainLayout;
    QString m_windowType; // For workspace persistence

    // Window State
    bool m_isMinimized;
    bool m_isMaximized;
    bool m_isPinned;
    QRect m_normalGeometry; // For restoring from maximize

    // Dragging/Resizing (Native C++)
    bool m_isDragging;
    bool m_isResizing;
    QPoint m_dragStartPos;
    QRect m_dragStartGeometry;
    Qt::Edges m_resizeEdges;

    static constexpr int RESIZE_BORDER_WIDTH = 8;
};

#endif // CUSTOMMDISUBWINDOW_H
