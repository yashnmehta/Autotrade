#include "core/widgets/CustomTitleBar.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QMouseEvent>

CustomTitleBar::CustomTitleBar(QWidget *parent)
    : QWidget(parent), m_isDragging(false) {
  setFixedHeight(36);
  // Ensure styled background is used so stylesheet background-color is painted
  setAttribute(Qt::WA_StyledBackground, true);
  setAutoFillBackground(true);
  m_isActive = true;
  // Default style (active) — Professional blue gradient with shadow effect
  setStyleSheet("background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #2563eb, stop:1 #1e40af);"
                "color: #ffffff;"
                "border-bottom: 1px solid #1e3a8a;");

  QHBoxLayout *layout = new QHBoxLayout(this);
  // Add small top and right margins (2px) to allow top/right edge resizing
  // while keeping the buttons visually nearly flush.
  layout->setContentsMargins(12, 2, 2, 0);
  layout->setSpacing(0);

  // Title Label
  m_titleLabel = new QLabel("Trading Terminal", this);
  m_titleLabel->setStyleSheet(
      "font-size: 13px; color: #ffffff; font-weight: 600;"
      "background: transparent; border: none;");
  layout->addWidget(m_titleLabel);

  layout->addStretch();

  // Window Control Buttons
  m_minimizeButton = new QPushButton("−", this);
  m_maximizeButton = new QPushButton("□", this);
  m_closeButton = new QPushButton("✕", this);

  // Button Styling — light text on active blue background
  QString buttonStyle = "QPushButton {"
                        "  background-color: transparent;"
                        "  color: #dbeafe;"
                        "  border: none;"
                        "  padding: 0px 16px;"
                        "  font-size: 15px;"
                        "}"
                        "QPushButton:hover {"
                        "  background-color: rgba(255,255,255,0.15);"
                        "  color: #ffffff;"
                        "}";

  QString closeButtonStyle = "QPushButton {"
                             "  background-color: transparent;"
                             "  color: #dbeafe;"
                             "  border: none;"
                             "  padding: 0px 16px;"
                             "  font-size: 15px;"
                             "}"
                             "QPushButton:hover {"
                             "  background-color: #dc2626;"
                             "  color: #ffffff;"
                             "}";

  m_minimizeButton->setStyleSheet(buttonStyle);
  m_minimizeButton->setFixedSize(46, 34);
  m_minimizeButton->setFocusPolicy(Qt::NoFocus);

  m_maximizeButton->setStyleSheet(buttonStyle);
  m_maximizeButton->setFixedSize(46, 34);
  m_maximizeButton->setFocusPolicy(Qt::NoFocus);

  m_closeButton->setStyleSheet(closeButtonStyle);
  m_closeButton->setFixedSize(46, 34);
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
    // Active titlebar: Strong blue gradient with white text (Windows 10 style)
    setStyleSheet("background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                  "stop:0 #2563eb, stop:1 #1e40af);"
                  "color: #ffffff;"
                  "border-bottom: 1px solid #1e3a8a;");
    m_titleLabel->setStyleSheet(
        "font-size:13px; color: #ffffff; font-weight: 600;"
        "background: transparent; border: none;");
    m_minimizeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #dbeafe; border: "
        "none; padding: 0px 16px; font-size: 15px; }"
        "QPushButton:hover { background-color: rgba(255,255,255,0.15); color: #ffffff; }");
    m_maximizeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #dbeafe; border: "
        "none; padding: 0px 16px; font-size: 15px; }"
        "QPushButton:hover { background-color: rgba(255,255,255,0.15); color: #ffffff; }");
    m_closeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #dbeafe; border: "
        "none; padding: 0px 16px; font-size: 15px; }"
        "QPushButton:hover { background-color: #dc2626; color: #ffffff; }");
  } else {
    // Inactive: Subtle light gray with darker text for contrast
    setStyleSheet("background-color: #e2e8f0;"
                  "color: #64748b;"
                  "border-bottom: 1px solid #cbd5e1;");
    m_titleLabel->setStyleSheet(
        "font-size:13px; color: #64748b; font-weight: normal;"
        "background: transparent; border: none;");
    m_minimizeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #94a3b8; border: "
        "none; padding: 0px 16px; font-size: 15px; }"
        "QPushButton:hover { background-color: #cbd5e1; color: #475569; }");
    m_maximizeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #94a3b8; border: "
        "none; padding: 0px 16px; font-size: 15px; }"
        "QPushButton:hover { background-color: #cbd5e1; color: #475569; }");
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
