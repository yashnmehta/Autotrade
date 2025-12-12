#include "widgets/MDITaskBar.h"
#include "widgets/CustomMDISubWindow.h"
#include <QDebug>

MDITaskBar::MDITaskBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(32);
    setStyleSheet("background-color: #2d2d30; border-top: 1px solid #3e3e42;");
    
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(5, 2, 5, 2);
    m_layout->setSpacing(5);
    m_layout->addStretch();
}

void MDITaskBar::addWindow(CustomMDISubWindow *window)
{
    if (!window || m_windowButtons.contains(window)) {
        return;
    }
    
    QPushButton *button = new QPushButton(window->title(), this);
    button->setFixedHeight(26);
    button->setMinimumWidth(120);
    button->setMaximumWidth(200);
    
    button->setStyleSheet(
        "QPushButton { "
        "   background-color: #3e3e42; "
        "   color: #ffffff; "
        "   border: 1px solid #555555; "
        "   border-radius: 2px; "
        "   padding: 2px 8px; "
        "   text-align: left; "
        "} "
        "QPushButton:hover { "
        "   background-color: #505053; "
        "} "
        "QPushButton:pressed { "
        "   background-color: #007acc; "
        "}"
    );
    
    // Remove stretch, add button, add stretch back
    m_layout->removeItem(m_layout->itemAt(m_layout->count() - 1));
    m_layout->addWidget(button);
    m_layout->addStretch();
    
    m_windowButtons[window] = button;
    
    connect(button, &QPushButton::clicked, [this, window]() {
        emit windowRestoreRequested(window);
    });
    
    qDebug() << "MDITaskBar: Button added for" << window->title();
}

void MDITaskBar::removeWindow(CustomMDISubWindow *window)
{
    if (!window || !m_windowButtons.contains(window)) {
        return;
    }
    
    QPushButton *button = m_windowButtons.take(window);
    m_layout->removeWidget(button);
    button->deleteLater();
    
    qDebug() << "MDITaskBar: Button removed for" << window->title();
}

void MDITaskBar::updateWindowTitle(CustomMDISubWindow *window, const QString &title)
{
    if (!window || !m_windowButtons.contains(window)) {
        return;
    }
    
    m_windowButtons[window]->setText(title);
}
