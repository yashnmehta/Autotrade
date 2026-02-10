#include "core/widgets/CustomTitleBar.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QMouseEvent>

CustomTitleBar::CustomTitleBar(QWidget *parent)
    : QWidget(parent), m_isDragging(false) {
  setFixedHeight(32);
  // Ensure styled background is used so stylesheet background-color is painted
  setAttribute(Qt::WA_StyledBackground, true);
  setAutoFillBackground(true);
  m_isActive = true;
  // Default style (active) - Premium dark gray
  setStyleSheet("background-color: #3e3e42; color: #ffffff; border-bottom: 2px "
                "solid #007acc;");

  QHBoxLayout *layout = new QHBoxLayout(this);
  // Add small top and right margins (2px) to allow top/right edge resizing
  // while keeping the buttons visually nearly flush.
  layout->setContentsMargins(10, 2, 2, 0);
  layout->setSpacing(0);

  // Title Label
  m_titleLabel = new QLabel("Trading Terminal", this);
  m_titleLabel->setStyleSheet(
      "font-size: 13px; color: #ffffff; font-weight: bold;");
  layout->addWidget(m_titleLabel);

  layout->addStretch();

  // Window Control Buttons
  m_minimizeButton = new QPushButton("−", this);
  m_maximizeButton = new QPushButton("□", this);
  m_closeButton = new QPushButton("✕", this);

  // Button Styling
  QString buttonStyle = "QPushButton { "
                        "   background-color: transparent; "
                        "   color: #ffffff; "
                        "   border: none; "
                        "   padding: 0px 16px; "
                        "   font-size: 16px; "
                        "} "
                        "QPushButton:hover { "
                        "   background-color: #3e3e42; "
                        "}";

  QString closeButtonStyle = buttonStyle + "QPushButton:hover { "
                                           "   background-color: #e81123; "
                                           "}";

  m_minimizeButton->setStyleSheet(buttonStyle);
  m_minimizeButton->setFixedSize(46, 28); // Reduced from 32
  m_minimizeButton->setFocusPolicy(Qt::NoFocus);

  m_maximizeButton->setStyleSheet(buttonStyle);
  m_maximizeButton->setFixedSize(46, 28); // Reduced from 32
  m_maximizeButton->setFocusPolicy(Qt::NoFocus);

  m_closeButton->setStyleSheet(closeButtonStyle);
  m_closeButton->setFixedSize(46, 28); // Reduced from 32
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
    // Active titlebar: Distinct lighter gray with a blue accent line at bottom
    setStyleSheet("background-color: #4d4d4d; "
                  "color: #ffffff; "
                  "border-bottom: 2px solid #0e0e0e;" // Premium blue accent
    );
    m_titleLabel->setStyleSheet(
        "font-size:13px; color: #ffffff; font-weight: bold;");
    m_minimizeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #ffffff; border: "
        "none; padding: 0px 16px; font-size: 16px; }"
        "QPushButton:hover { background-color: #505050; }");
    m_maximizeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #ffffff; border: "
        "none; padding: 0px 16px; font-size: 16px; }"
        "QPushButton:hover { background-color: #505050; }");
    m_closeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #ffffff; border: "
        "none; padding: 0px 16px; font-size: 16px; }"
        "QPushButton:hover { background-color: #e81123; }");
  } else {
    // Inactive: Subtle darker gray, blends more with the theme
    setStyleSheet("background-color: #323232; "
                  "color: #888888; "
                  "border: 2px solid #2c2c2c;" // Premium blue accent
    );

    m_titleLabel->setStyleSheet(
        "font-size:13px; color: #888888; font-weight: normal;"
        "border: 0px;");
    m_minimizeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #888888; border: "
        "none; padding: 0px 16px; font-size: 16px; }"
        "QPushButton:hover { background-color: #2a2a2d; }");
    m_maximizeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #888888; border: "
        "none; padding: 0px 16px; font-size: 16px; }"
        "QPushButton:hover { background-color: #2a2a2d; }");
    m_closeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: #888888; border: "
        "none; padding: 0px 16px; font-size: 16px; }"
        "QPushButton:hover { background-color: #c42b1c; }"); // Muted red for
                                                             // inactive
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
