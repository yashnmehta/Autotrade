#ifndef WORKSPACEMANAGER_H
#define WORKSPACEMANAGER_H

#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>

// Forward declarations
class MainWindow;
class CustomMDIArea;
class WindowFactory;

/**
 * @brief Manages workspace save/load/restore lifecycle
 *
 * Extracted from MainWindow to follow Single Responsibility Principle.
 * WorkspaceManager owns workspace persistence, profile management,
 * and the window-restore dispatch logic.
 */
class WorkspaceManager : public QObject {
  Q_OBJECT

public:
  explicit WorkspaceManager(MainWindow *mainWindow, CustomMDIArea *mdiArea,
                            WindowFactory *factory, QObject *parent = nullptr);

  // ── Workspace operations ─────────────────────────────────────────────
  void saveCurrentWorkspace();
  void loadWorkspace();
  bool loadWorkspaceByName(const QString &name);
  void manageWorkspaces();

public slots:
  /**
   * @brief Restore a single window from saved workspace data
   *
   * Connected to CustomMDIArea::restoreWindowRequested signal.
   */
  void onRestoreWindowRequested(const QString &type, const QString &title,
                                const QRect &geometry, bool isMinimized,
                                bool isMaximized, bool isPinned,
                                const QString &workspaceName, int index);

private:
  MainWindow *m_mainWindow;
  CustomMDIArea *m_mdiArea;
  WindowFactory *m_factory;
};

#endif // WORKSPACEMANAGER_H
