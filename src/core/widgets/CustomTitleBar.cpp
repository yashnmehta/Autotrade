#include "core/widgets/CustomTitleBar.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QApplication>

CustomTitleBar::CustomTitleBar(QWidget *parent)
    : QWidget(parent), m_isDragging(false)
{
    setFixedHeight(32);
    // Ensure styled background is used so stylesheet background-color is painted
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    m_isActive = true;
    // Default style (active)
    setStyleSheet("background-color: #2d2d30; color: #ffffff;");

    QHBoxLayout *layout = new QHBoxLayout(this);
    // Add small top and right margins (2px) to allow top/right edge resizing
    // while keeping the buttons visually nearly flush.
    layout->setContentsMargins(10, 2, 2, 0);
    layout->setSpacing(0);

    // Title Label
    m_titleLabel = new QLabel("Trading Terminal", this);
    m_titleLabel->setStyleSheet("font-size: 13px; color: #cccccc;");
    layout->addWidget(m_titleLabel);

    layout->addStretch();

    // Window Control Buttons
    m_minimizeButton = new QPushButton("−", this);
    m_maximizeButton = new QPushButton("□", this);
    m_closeButton = new QPushButton("✕", this);

    // Button Styling
    QString buttonStyle =
        "QPushButton { "
        "   background-color: transparent; "
        "   color: #ffffff; "
        "   border: none; "
        "   padding: 0px 16px; "
        "   font-size: 16px; "
        "} "
        "QPushButton:hover { "
        "   background-color: #3e3e42; "
        "}";

    QString closeButtonStyle = buttonStyle +
                               "QPushButton:hover { "
                               "   background-color: #e81123; "
                               "}";

    m_minimizeButton->setStyleSheet(buttonStyle);
    m_minimizeButton->setFixedSize(46, 28); // Reduced from 32

    m_maximizeButton->setStyleSheet(buttonStyle);
    m_maximizeButton->setFixedSize(46, 28); // Reduced from 32

    m_closeButton->setStyleSheet(closeButtonStyle);
    m_closeButton->setFixedSize(46, 28); // Reduced from 32

    layout->addWidget(m_minimizeButton);
    layout->addWidget(m_maximizeButton);
    layout->addWidget(m_closeButton);

    // Connect signals
    connect(m_minimizeButton, &QPushButton::clicked, this, &CustomTitleBar::minimizeClicked);
    connect(m_maximizeButton, &QPushButton::clicked, this, &CustomTitleBar::maximizeClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &CustomTitleBar::closeClicked);
}

void CustomTitleBar::setActive(bool active)
{
    if (m_isActive == active)
        return;

    m_isActive = active;

    if (m_isActive) {
        // Active titlebar: slightly lighter background and brighter title
        setStyleSheet(
            "background-color: #32343a; color: #ffffff;"
        );
        m_titleLabel->setStyleSheet("font-size:13px; color: #e6f0ff;");
        m_minimizeButton->setStyleSheet(
            "QPushButton { background-color: transparent; color: #ffffff; border: none; padding: 0px 16px; font-size: 16px; }"
            "QPushButton:hover { background-color: #3e3e42; }"
        );
        m_maximizeButton->setStyleSheet(
            "QPushButton { background-color: transparent; color: #ffffff; border: none; padding: 0px 16px; font-size: 16px; }"
            "QPushButton:hover { background-color: #3e3e42; }"
        );
        m_closeButton->setStyleSheet(
            "QPushButton { background-color: transparent; color: #ffffff; border: none; padding: 0px 16px; font-size: 16px; }"
            "QPushButton:hover { background-color: #e81123; }"
        );
    } else {
        // Inactive (background closer to MDI area background, dim title)
        setStyleSheet(
            "background-color: #262626; color: #bdbdbd;"
        );
        m_titleLabel->setStyleSheet("font-size:13px; color: #9f9f9f;");
        m_minimizeButton->setStyleSheet(
            "QPushButton { background-color: transparent; color: #bdbdbd; border: none; padding: 0px 16px; font-size: 16px; }"
            "QPushButton:hover { background-color: #2f2f32; }"
        );
        m_maximizeButton->setStyleSheet(
            "QPushButton { background-color: transparent; color: #bdbdbd; border: none; padding: 0px 16px; font-size: 16px; }"
            "QPushButton:hover { background-color: #2f2f32; }"
        );
        m_closeButton->setStyleSheet(
            "QPushButton { background-color: transparent; color: #bdbdbd; border: none; padding: 0px 16px; font-size: 16px; }"
            "QPushButton:hover { background-color: #7a1e20; }"
        );
    }
}

void CustomTitleBar::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

QString CustomTitleBar::title() const
{
    return m_titleLabel->text();
}

void CustomTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        // Start dragging - emit signal for parent to handle
        m_isDragging = true;
        m_dragPosition = event->globalPos() - parentWidget()->geometry().topLeft();
        emit dragStarted(event->globalPos());
        event->accept();
        event->setAccepted(true);
    }
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton))
    {
        emit dragMoved(event->globalPos());
        event->accept();
        event->setAccepted(true);
        return;  // Don't propagate
    }
    QWidget::mouseMoveEvent(event);
}

void CustomTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (m_isDragging)
        {
            emit dragEnded();
            event->accept();
            event->setAccepted(true);
        }
        m_isDragging = false;
        return;  // Don't propagate
    }
    QWidget::mouseReleaseEvent(event);
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit doubleClicked();
        emit maximizeClicked(); // Also emit for backwards compatibility
        event->accept();
    }
    else
    {
        QWidget::mouseDoubleClickEvent(event);
    }
}
