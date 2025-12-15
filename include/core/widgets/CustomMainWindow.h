#ifndef CUSTOMMAINWINDOW_H
#define CUSTOMMAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QRect>
#include <QPoint>

class CustomTitleBar;

/**
 * @brief Reusable Frameless Main Window with Full Window Management
 *
 * This is a complete replacement for QMainWindow when you need a frameless window.
 * It implements all the functionality you lose when removing the native frame:
 * - Custom title bar with minimize/maximize/close
 * - 8-direction edge resizing (corners + sides)
 * - Window dragging via title bar
 * - Double-click title bar to maximize
 * - Proper geometry restoration
 * - Platform-agnostic implementation
 *
 * Usage:
 *   CustomMainWindow *window = new CustomMainWindow();
 *   window->setTitle("My App");
 *   window->setCentralWidget(myContentWidget);
 *   window->show();
 */
class CustomMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CustomMainWindow(QWidget *parent = nullptr);
    virtual ~CustomMainWindow();

    // Content Management
    void setCentralWidget(QWidget *widget);
    QWidget *centralWidget() const { return m_centralWidget; }

    // Window Properties
    void setTitle(const QString &title);
    QString title() const;

    void setMinimumSize(int minw, int minh);
    void setMaximumSize(int maxw, int maxh);

    // Title Bar Access (for adding custom buttons/widgets)
    CustomTitleBar *titleBar() const { return m_titleBar; }

    // Toolbar Management
    void addToolBar(QToolBar *toolbar) { QMainWindow::addToolBar(toolbar); }

public slots:
    void showMinimized();
    void showMaximized();
    void showNormal();
    void toggleMaximize();

signals:
    void windowStateChanged(bool isMaximized);

protected:
    // Event Handlers for Manual Window Management
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    bool event(QEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void applyDefaultStyling();

    // Resize Detection
    enum ResizeDirection
    {
        None = 0,
        Top = 1,
        Bottom = 2,
        Left = 4,
        Right = 8,
        TopLeft = Top | Left,
        TopRight = Top | Right,
        BottomLeft = Bottom | Left,
        BottomRight = Bottom | Right
    };

    ResizeDirection detectResizeDirection(const QPoint &pos) const;
    void updateCursorShape(ResizeDirection direction);
    void performResize(const QPoint &globalPos);

    // Window State Management
    void saveNormalGeometry();
    void restoreNormalGeometry();

    // UI Components
    CustomTitleBar *m_titleBar;
    QWidget *m_centralWidget;
    QVBoxLayout *m_mainLayout;

    // Window Management State
    bool m_isMaximized;
    QRect m_normalGeometry;

    // Drag State
    QPoint m_dragOffset; // Offset from cursor to window top-left during drag

    // Resize State
    bool m_isResizing;
    ResizeDirection m_resizeDirection;
    QPoint m_resizeStartPos;
    QRect m_resizeStartGeometry;

    // Pending resize state when click originates from titlebar child widgets
    bool m_pendingResize;
    QPoint m_pendingResizeStartPos;
    ResizeDirection m_pendingResizeDirection;

    // Constraints
    QSize m_minimumSize;
    QSize m_maximumSize;

    // Configuration
    static constexpr int RESIZE_BORDER_WIDTH = 8;
};

#endif // CUSTOMMAINWINDOW_H
