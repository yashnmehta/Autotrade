#include "core/widgets/CustomMDIChild.h"
#include "core/widgets/CustomTitleBar.h"
#include <QVBoxLayout>

CustomMDIChild::CustomMDIChild(const QString &title, QWidget *parent)
    : QWidget(parent), m_contentWidget(nullptr)
{
    // Main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Custom title bar
    m_titleBar = new CustomTitleBar(this);
    m_titleBar->setTitle(title);
    m_mainLayout->addWidget(m_titleBar);

    // Connect title bar signals
    connect(m_titleBar, &CustomTitleBar::minimizeClicked, this, &CustomMDIChild::minimizeRequested);
    connect(m_titleBar, &CustomTitleBar::closeClicked, this, &CustomMDIChild::closeRequested);

    // Set window flags for floating
    setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);

    // Styling
    setStyleSheet(
        "CustomMDIChild { "
        "   background-color: #ffffff; "
        "  "
        "}");

    resize(600, 400);
}

void CustomMDIChild::setContentWidget(QWidget *widget)
{
    if (m_contentWidget)
    {
        m_mainLayout->removeWidget(m_contentWidget);
        m_contentWidget->setParent(nullptr);
    }

    m_contentWidget = widget;
    if (m_contentWidget)
    {
        m_mainLayout->addWidget(m_contentWidget);
    }
}

QWidget *CustomMDIChild::contentWidget() const
{
    return m_contentWidget;
}

void CustomMDIChild::setTitle(const QString &title)
{
    m_titleBar->setTitle(title);
}

QString CustomMDIChild::title() const
{
    return m_titleBar->title();
}
