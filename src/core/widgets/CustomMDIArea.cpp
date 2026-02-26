#include "core/widgets/CustomMDIArea.h"
#include "core/WindowConstants.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "core/widgets/MDITaskBar.h"
#include "utils/WindowManager.h"
#include <QDebug>
#include <QResizeEvent>
#include <QSettings>
#include <QVBoxLayout>
#include <QtMath>

#include <QApplication>
#include <QShortcut>

CustomMDIArea::CustomMDIArea(QWidget *parent)
    : QWidget(parent), m_activeWindow(nullptr), m_nextX(20), m_nextY(20),
      m_snapPreview(nullptr) {
  // Light theme background
  setStyleSheet("background-color: #f1f5f9;");

  // Create snap preview overlay with refined visuals
  m_snapPreview = new QWidget(this);
  m_snapPreview->setStyleSheet(
      "background-color: rgba(59, 130, 246, 40);"
      "border: 1px solid #3b82f6;"
      "border-radius: 4px;");
  m_snapPreview->hide();

  // Main layout with taskbar at bottom
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Spacer for window area (windows are direct children)
  layout->addStretch();

  // TaskBar at bottom
  m_taskBar = new MDITaskBar(this);
  layout->addWidget(m_taskBar);

  connect(m_taskBar, &MDITaskBar::windowRestoreRequested, this,
          &CustomMDIArea::restoreWindow);

  // Monitor global focus changes to handle activation when clicking inside
  // child widgets (e.g., TableView)
  connect(qApp, &QApplication::focusChanged, this,
          [this](QWidget *old, QWidget *now) {
            if (!now)
              return;

            // Find if the focused widget belongs to one of our on-screen windows
            // Skip off-screen cached windows (x <= -1000)
            for (CustomMDISubWindow *window : m_windows) {
              if (window->x() <= -1000) continue; // Skip off-screen cached windows
              if (window == now || window->isAncestorOf(now)) {
                activateWindow(window);
                // Centralized: keep WindowManager stack in sync via content widget
                if (window->contentWidget()) {
                  WindowManager::instance().bringToTop(window->contentWidget());
                }
                break;
              }
            }
          });
}

CustomMDIArea::~CustomMDIArea() {
  // Clean up windows
  qDeleteAll(m_windows);
}

void CustomMDIArea::addWindow(CustomMDISubWindow *window,
                              bool showAndActivate) {
  if (!window || m_windows.contains(window)) {
    return;
  }

  // Set parent to this MDI area
  window->setParent(this);

  // Position window
  QPoint pos = getNextWindowPosition();
  window->move(pos);

  // Add to list
  m_windows.append(window);

  // Install event filter to track activation
  window->installEventFilter(this);

  // Connect window signals
  // Minimize needs MDIArea for taskbar management
  connect(window, &CustomMDISubWindow::minimizeRequested, this,
          [this, window]() { minimizeWindow(window); });
  // Maximize is handled directly by the window (no MDIArea involvement needed)
  // windowActivated for focus/z-order management
  connect(window, &CustomMDISubWindow::windowActivated, this,
          [this, window]() { activateWindow(window); });

  // Optional show/activate (can be skipped for cached windows)
  if (showAndActivate) {
    window->show();
    window->raise();
    activateWindow(window);
  }

  emit windowAdded(window);

  qDebug() << "CustomMDIArea: Window added" << window->title();
}

void CustomMDIArea::removeWindow(CustomMDISubWindow *window) {
  if (!window)
    return;

  window->removeEventFilter(this);
  m_windows.removeOne(window);
  m_minimizedWindows.removeOne(window);

  if (m_activeWindow == window) {
    m_activeWindow = nullptr;

    // Activate another window if available
    if (!m_windows.isEmpty()) {
      activateWindow(m_windows.last());
    }
  }

  emit windowRemoved(window);

  qDebug() << "CustomMDIArea: Window removed" << window->title();

  // Safety: delete later (we removed WA_DeleteOnClose)
  window->deleteLater();
}

void CustomMDIArea::closeAllSubWindows() {
  QList<CustomMDISubWindow *> copy = m_windows;
  for (auto *window : copy) {
    window->close();
  }
}

void CustomMDIArea::activateWindow(CustomMDISubWindow *window) {
  if (!window)
    return;

  if (window->isMinimized()) {
    restoreWindow(window);
    return;
  }

  if (window == m_activeWindow)
    return;

  // Deactivate old window
  if (m_activeWindow) {
    m_activeWindow->setActive(false);
  }

  // Activate new window
  m_activeWindow = window;
  window->raise();
  window->activateWindow();
  window->setFocus(Qt::ActiveWindowFocusReason);

  // Subwindow knows how to style itself when set active
  window->setActive(true);

  emit windowActivated(window);
  qDebug() << "[MDIArea] Window activated:" << window->title();
}

void CustomMDIArea::minimizeWindow(CustomMDISubWindow *window) {
  if (!window || window->isMinimized()) {
    return;
  }

  window->minimize();
  window->hide();

  m_minimizedWindows.append(window);
  m_taskBar->addWindow(window);

  qDebug() << "CustomMDIArea: Window minimized" << window->title();
}

void CustomMDIArea::restoreWindow(CustomMDISubWindow *window) {
  if (!window || !window->isMinimized()) {
    return;
  }

  window->restore();
  window->show();
  window->raise();

  m_minimizedWindows.removeOne(window);
  m_taskBar->removeWindow(window);

  activateWindow(window);

  qDebug() << "CustomMDIArea: Window restored" << window->title();
}

CustomMDISubWindow *CustomMDIArea::activeWindow() const {
  return m_activeWindow;
}

QList<CustomMDISubWindow *> CustomMDIArea::windowList() const {
  return m_windows;
}

void CustomMDIArea::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  int availableHeight = height();
  if (m_taskBar && m_taskBar->isVisible()) {
    availableHeight -= m_taskBar->height();
  }

  // Adjust maximized windows to fill new area
  for (CustomMDISubWindow *window : m_windows) {
    if (window->isMaximized()) {
      window->setGeometry(0, 0, width(), availableHeight);
    }
  }
}

bool CustomMDIArea::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::MouseButtonPress ||
      event->type() == QEvent::FocusIn) {
    CustomMDISubWindow *window = qobject_cast<CustomMDISubWindow *>(watched);
    if (window && window->x() > -1000 && m_windows.contains(window)) {
      activateWindow(window);
    }
  }

  return QWidget::eventFilter(watched, event);
}

QPoint CustomMDIArea::getNextWindowPosition() {
  QPoint pos(m_nextX, m_nextY);

  // Cascade offset
  m_nextX += CASCADE_OFFSET;
  m_nextY += CASCADE_OFFSET;

  // Reset if too far
  if (m_nextX > 400 || m_nextY > 400) {
    m_nextX = 20;
    m_nextY = 20;
  }

  return pos;
}

void CustomMDIArea::cascadeWindows() {
  if (m_windows.isEmpty())
    return;

  int x = 20, y = 20;
  const int offset = CASCADE_OFFSET;

  for (CustomMDISubWindow *window : m_windows) {
    if (window->isMinimized())
      continue;

    window->setGeometry(x, y, 800, 600);
    window->show();
    window->raise();

    x += offset;
    y += offset;

    // Reset if too far
    if (x > width() - 400 || y > height() - 400) {
      x = 20;
      y = 20;
    }
  }

  if (!m_windows.isEmpty()) {
    activateWindow(m_windows.last());
  }
}

void CustomMDIArea::tileWindows() {
  // Default to grid tiling
  tileVertically();
}

void CustomMDIArea::tileHorizontally() {
  QList<CustomMDISubWindow *> visibleWindows;
  for (CustomMDISubWindow *window : m_windows) {
    if (!window->isMinimized()) {
      visibleWindows.append(window);
    }
  }

  if (visibleWindows.isEmpty())
    return;

  int availableHeight = height() - m_taskBar->height();
  int windowHeight = availableHeight / visibleWindows.count();
  int y = 0;

  for (CustomMDISubWindow *window : visibleWindows) {
    window->setGeometry(0, y, width(), windowHeight);
    window->show();
    y += windowHeight;
  }
}

void CustomMDIArea::tileVertically() {
  QList<CustomMDISubWindow *> visibleWindows;
  for (CustomMDISubWindow *window : m_windows) {
    if (!window->isMinimized()) {
      visibleWindows.append(window);
    }
  }

  if (visibleWindows.isEmpty())
    return;

  // Calculate grid dimensions
  int count = visibleWindows.count();
  int cols = qCeil(qSqrt(count));
  int rows = qCeil((double)count / cols);

  int availableHeight = height() - m_taskBar->height();
  int windowWidth = width() / cols;
  int windowHeight = availableHeight / rows;

  int index = 0;
  for (int row = 0; row < rows && index < count; ++row) {
    for (int col = 0; col < cols && index < count; ++col) {
      CustomMDISubWindow *window = visibleWindows[index++];
      window->setGeometry(col * windowWidth, row * windowHeight, windowWidth,
                          windowHeight);
      window->show();
    }
  }
}

QRect CustomMDIArea::getSnappedGeometry(const QRect &geometry) const {
  QRect snapped = geometry;
  int availableHeight = height() - m_taskBar->height();

  // Snap to left edge
  if (qAbs(geometry.left()) < SNAP_DISTANCE) {
    snapped.moveLeft(0);
  }

  // Snap to right edge
  if (qAbs(geometry.right() - width()) < SNAP_DISTANCE) {
    snapped.moveRight(width() - 1);
  }

  // Snap to top edge
  if (qAbs(geometry.top()) < SNAP_DISTANCE) {
    snapped.moveTop(0);
  }

  // Snap to bottom edge (above taskbar)
  if (qAbs(geometry.bottom() - availableHeight) < SNAP_DISTANCE) {
    snapped.moveBottom(availableHeight - 1);
  }

  // Snap to other windows
  for (CustomMDISubWindow *window : m_windows) {
    if (window->isMinimized() || !window->isVisible())
      continue;

    QRect other = window->geometry();

    // Snap to right edge of other window
    if (qAbs(geometry.left() - other.right()) < SNAP_DISTANCE) {
      snapped.moveLeft(other.right());
    }

    // Snap to left edge of other window
    if (qAbs(geometry.right() - other.left()) < SNAP_DISTANCE) {
      snapped.moveRight(other.left() - 1);
    }

    // Snap to bottom edge of other window
    if (qAbs(geometry.top() - other.bottom()) < SNAP_DISTANCE) {
      snapped.moveTop(other.bottom());
    }

    // Snap to top edge of other window
    if (qAbs(geometry.bottom() - other.top()) < SNAP_DISTANCE) {
      snapped.moveBottom(other.top() - 1);
    }
  }

  return snapped;
}

void CustomMDIArea::showSnapPreview(const QRect &snapRect) {
  if (m_snapPreview) {
    m_snapPreview->setGeometry(snapRect);
    m_snapPreview->raise();
    m_snapPreview->show();
  }
}

void CustomMDIArea::hideSnapPreview() {
  if (m_snapPreview) {
    m_snapPreview->hide();
  }
}

void CustomMDIArea::saveWorkspace(const QString &name) {
  QSettings settings("TradingCompany", "TradingTerminal");
  settings.beginGroup("workspaces/" + name);

  // Count and save only visible windows (skip off-screen cached windows)
  int savedIndex = 0;
  for (int i = 0; i < m_windows.count(); ++i) {
    CustomMDISubWindow *window = m_windows[i];

    // Skip off-screen cached windows (they're at -10000,-10000)
    if (window->geometry().x() < WindowConstants::VISIBLE_THRESHOLD_X) {
      qDebug() << "CustomMDIArea: Skipping off-screen window:"
               << window->title();
      continue;
    }

    QString prefix = QString("window_%1/").arg(savedIndex);

    settings.setValue(prefix + "type", window->windowType());
    settings.setValue(prefix + "title", window->title());
    settings.setValue(prefix + "geometry", window->geometry());
    settings.setValue(prefix + "minimized", window->isMinimized());
    settings.setValue(prefix + "maximized", window->isMaximized());
    settings.setValue(prefix + "pinned", window->isPinned());

    savedIndex++;
  }

  // Save actual count of visible windows
  settings.setValue("windowCount", savedIndex);

  settings.endGroup();
  qDebug() << "CustomMDIArea: Workspace saved:" << name << "(" << savedIndex
           << "windows)";
}

void CustomMDIArea::loadWorkspace(const QString &name) {
  QSettings settings("TradingCompany", "TradingTerminal");
  settings.beginGroup("workspaces/" + name);

  if (!settings.contains("windowCount")) {
    qDebug() << "CustomMDIArea: Workspace not found:" << name;
    settings.endGroup();
    return;
  }

  // Close all existing windows
  QList<CustomMDISubWindow *> windowsCopy = m_windows;
  for (CustomMDISubWindow *window : windowsCopy) {
    window->close();
  }

  // Load windows
  int windowCount = settings.value("windowCount").toInt();

  for (int i = 0; i < windowCount; ++i) {
    QString prefix = QString("window_%1/").arg(i);
    QString type = settings.value(prefix + "type").toString();
    QString title = settings.value(prefix + "title").toString();
    QRect geometry = settings.value(prefix + "geometry").toRect();
    bool minimized = settings.value(prefix + "minimized").toBool();
    bool maximized = settings.value(prefix + "maximized").toBool();
    bool pinned = settings.value(prefix + "pinned").toBool();

    // Signal parent to create window of specific type
    emit restoreWindowRequested(type, title, geometry, minimized, maximized,
                                pinned, name, i);

    qDebug() << "CustomMDIArea: Requesting restore for window:" << type
             << title;
  }

  settings.endGroup();
  qDebug() << "CustomMDIArea: Workspace loaded:" << name;
}

QStringList CustomMDIArea::availableWorkspaces() const {
  QSettings settings("TradingCompany", "TradingTerminal");
  settings.beginGroup("workspaces");
  QStringList workspaces = settings.childGroups();
  settings.endGroup();
  return workspaces;
}

void CustomMDIArea::deleteWorkspace(const QString &name) {
  QSettings settings("TradingCompany", "TradingTerminal");
  settings.beginGroup("workspaces");
  settings.remove(name);
  settings.endGroup();
  qDebug() << "CustomMDIArea: Workspace deleted:" << name;
}

void CustomMDIArea::cycleActiveWindow(bool backward) {
  if (m_windows.count() < 2)
    return;

  int index = m_activeWindow ? m_windows.indexOf(m_activeWindow) : -1;
  if (index == -1)
    index = 0;

  int count = m_windows.count();
  int nextIndex;
  if (backward) {
    nextIndex = (index - 1 + count) % count;
  } else {
    nextIndex = (index + 1) % count;
  }

  activateWindow(m_windows[nextIndex]);
}
