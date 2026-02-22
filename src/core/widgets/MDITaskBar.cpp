#include "core/widgets/MDITaskBar.h"
#include "core/widgets/CustomMDISubWindow.h"
#include <QDebug>

MDITaskBar::MDITaskBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(32);
    setStyleSheet("background-color: #f8fafc; border-top: 1px solid #e2e8f0;");
    
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
        "   background-color: #f1f5f9; "
        "   color: #334155; "
        "   border: 1px solid #e2e8f0; "
        "   border-radius: 4px; "
        "   padding: 2px 8px; "
        "   text-align: left; "
        "} "
        "QPushButton:hover { "
        "   background-color: #e2e8f0; "
        "   color: #1e293b; "
        "} "
        "QPushButton:pressed { "
        "   background-color: #dbeafe; "
        "   color: #1e40af; "
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
