#include "ui/CustomTitleBar.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QApplication>

CustomTitleBar::CustomTitleBar(QWidget *parent)
    : QWidget(parent), m_isDragging(false)
{
    setFixedHeight(32);
    setStyleSheet("background-color: #2d2d30; color: #ffffff;");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 0, 0);
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
    m_minimizeButton->setFixedSize(46, 32);

    m_maximizeButton->setStyleSheet(buttonStyle);
    m_maximizeButton->setFixedSize(46, 32);

    m_closeButton->setStyleSheet(closeButtonStyle);
    m_closeButton->setFixedSize(46, 32);

    layout->addWidget(m_minimizeButton);
    layout->addWidget(m_maximizeButton);
    layout->addWidget(m_closeButton);

    // Connect signals
    connect(m_minimizeButton, &QPushButton::clicked, this, &CustomTitleBar::minimizeClicked);
    connect(m_maximizeButton, &QPushButton::clicked, this, &CustomTitleBar::maximizeClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &CustomTitleBar::closeClicked);
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
        // Start dragging - the parent will handle resize detection
        m_isDragging = true;
        m_dragPosition = event->globalPos() - window()->frameGeometry().topLeft();
        event->accept();
    }
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton))
    {
        window()->move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}

void CustomTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_isDragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit maximizeClicked();
        event->accept();
    }
}
