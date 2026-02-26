#include "core/widgets/CustomTitleBar.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QMouseEvent>

CustomTitleBar::CustomTitleBar(QWidget *parent)
    : QWidget(parent), m_isDragging(false) {
  setFixedHeight(34);
  // Ensure styled background is used so stylesheet background-color is painted
  setAttribute(Qt::WA_StyledBackground, true);
  setAutoFillBackground(true);
  m_isActive = true;
  // Default style (active) — Clean light theme with blue accent
  setStyleSheet("background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #ffffff, stop:1 #f8fafc);"
                "color: #0f172a;"
                "border-bottom: 2px solid #3b82f6;");

  QHBoxLayout *layout = new QHBoxLayout(this);
  // Add small top and right margins (2px) to allow top/right edge resizing
  // while keeping the buttons visually nearly flush.
  layout->setContentsMargins(12, 2, 2, 0);
  layout->setSpacing(0);

  // Title Label
  m_titleLabel = new QLabel("Trading Terminal", this);
  m_titleLabel->setStyleSheet(
      "font-size: 13px; color: #1e293b; font-weight: 600;"
      "background: transparent; border: none;");
  layout->addWidget(m_titleLabel);

  layout->addStretch();

  // Window Control Buttons
  m_minimizeButton = new QPushButton("−", this);
  m_maximizeButton = new QPushButton("□", this);
  m_closeButton = new QPushButton("✕", this);

  // Button Styling — light theme
  QString buttonStyle = "QPushButton {"
                        "  background-color: transparent;"
                        "  color: #475569;"
                        "  border: none;"
                        "  padding: 0px 16px;"
                        "  font-size: 15px;"
                        "}"
                        "QPushButton:hover {"
                        "  background-color: #e2e8f0;"
                        "  color: #0f172a;"
                        "}";

  QString closeButtonStyle = "QPushButton {"
                             "  background-color: transparent;"
                             "  color: #475569;"
                             "  border: none;"
                             "  padding: 0px 16px;"
                             "  font-size: 15px;"
                             "}"
                             "QPushButton:hover {"
                             "  background-color: #ef4444;"
                             "  color: #ffffff;"
                             "}";

  m_minimizeButton->setStyleSheet(buttonStyle);
  m_minimizeButton->setFixedSize(46, 30);
  m_minimizeButton->setFocusPolicy(Qt::NoFocus);

  m_maximizeButton->setStyleSheet(buttonStyle);
  m_maximizeButton->setFixedSize(46, 30);
  m_maximizeButton->setFocusPolicy(Qt::NoFocus);

  m_closeButton->setStyleSheet(closeButtonStyle);
  m_closeButton->setFixedSize(46, 30);
  m_closeButton->setFocusPolicy(Qt::NoFocus);

  layout->addWidget(m_minimizeButton);
  layout->addWidget(m_maximizeButton);
  layout->addWidget(m_closeButton);

  // Connect signals
  connect(m_minimizeButton, &QPushButton::clicked, this,
          &CustomTitleBar::minimizeClicked);
  connect(m_maximizeButton, &QPushButton::clicked, this,
          &CustomTitleBar::maximizeClicked);
  connect(m_closeButton, &QPushButton::clicked, this,
          &CustomTitleBar::closeClicked);
}

void CustomTitleBar::setActive(bool active) {
  if (m_isActive == active)
    return;

  m_isActive = active;

  if (m_isActive) {
    // Active titlebar: Clean white with blue accent bottom border
    setStyleSheet("background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                  "stop:0 #ffffff, stop:1 #f8fafc);"
                  "color: #0f172a;"
                  "border-bottom: 2px solid #3b82f6;");
    m_titleLabel->setStyleSheet(
        "font-size:13px; color: #1e293b; font-weight: 600;"
        "background: transparent; border: none;");
    m_minimizeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #475569; border: "
        "none; padding: 0px 16px; font-size: 15px; }"
        "QPushButton:hover { background-color: #e2e8f0; color: #0f172a; }");
    m_maximizeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #475569; border: "
        "none; padding: 0px 16px; font-size: 15px; }"
        "QPushButton:hover { background-color: #e2e8f0; color: #0f172a; }");
    m_closeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #475569; border: "
        "none; padding: 0px 16px; font-size: 15px; }"
        "QPushButton:hover { background-color: #ef4444; color: #ffffff; }");
  } else {
    // Inactive: Slightly muted light gray, text faded
    setStyleSheet("background-color: #f1f5f9;"
                  "color: #94a3b8;"
                  "border-bottom: 2px solid #e2e8f0;");
    m_titleLabel->setStyleSheet(
        "font-size:13px; color: #94a3b8; font-weight: normal;"
        "background: transparent; border: none;");
    m_minimizeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #94a3b8; border: "
        "none; padding: 0px 16px; font-size: 15px; }"
        "QPushButton:hover { background-color: #e2e8f0; color: #64748b; }");
    m_maximizeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #94a3b8; border: "
        "none; padding: 0px 16px; font-size: 15px; }"
        "QPushButton:hover { background-color: #e2e8f0; color: #64748b; }");
    m_closeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #94a3b8; border: "
        "none; padding: 0px 16px; font-size: 15px; }"
        "QPushButton:hover { background-color: #dc2626; color: #ffffff; }");
  }
}

void CustomTitleBar::setTitle(const QString &title) {
  m_titleLabel->setText(title);
}

QString CustomTitleBar::title() const { return m_titleLabel->text(); }

void CustomTitleBar::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    // Start dragging - emit signal for parent to handle
    m_isDragging = true;
    m_dragPosition = event->globalPos() - parentWidget()->geometry().topLeft();
    emit dragStarted(event->globalPos());
    event->accept();
    event->setAccepted(true);
  }
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent *event) {
  if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
    emit dragMoved(event->globalPos());
    event->accept();
    event->setAccepted(true);
    return; // Don't propagate
  }
  QWidget::mouseMoveEvent(event);
}

void CustomTitleBar::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    if (m_isDragging) {
      emit dragEnded();
      event->accept();
      event->setAccepted(true);
    }
    m_isDragging = false;
    return; // Don't propagate
  }
  QWidget::mouseReleaseEvent(event);
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    emit doubleClicked();
    emit maximizeClicked(); // Also emit for backwards compatibility
    event->accept();
  } else {
    QWidget::mouseDoubleClickEvent(event);
  }
}
