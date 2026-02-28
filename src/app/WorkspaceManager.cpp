/**
 * @file WorkspaceManager.cpp
 * @brief Manages workspace save/load/restore lifecycle
 *
 * Extracted from MainWindow::UI.cpp + Windows.cpp during Phase 3.2 refactor.
 */

#include "app/WorkspaceManager.h"
#include "app/MainWindow.h"
#include "app/WindowFactory.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "models/profiles/GenericProfileManager.h"
#include "models/profiles/GenericTableProfile.h"
#include "views/ATMWatchWindow.h"
#include "views/BaseBookWindow.h"
#include "views/MarketWatchWindow.h"
#include "views/OptionChainWindow.h"

#include <QDebug>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLineEdit>
#include <QSettings>

// ═══════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════

WorkspaceManager::WorkspaceManager(MainWindow *mainWindow,
                                   CustomMDIArea *mdiArea,
                                   WindowFactory *factory, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow), m_mdiArea(mdiArea),
      m_factory(factory) {}

// ═══════════════════════════════════════════════════════════════════════
// Save / Load / Manage
// ═══════════════════════════════════════════════════════════════════════

void WorkspaceManager::saveCurrentWorkspace() {
  bool ok;
  QString name = QInputDialog::getText(
      m_mainWindow, "Save Workspace", "Workspace name:", QLineEdit::Normal, "",
      &ok);
  if (!ok || name.isEmpty() || !m_mdiArea)
    return;

  // 1. Save Window Layout (Geometry, Type, etc.)
  m_mdiArea->saveWorkspace(name);

  // 2. Save Specific Content (Scrips, Profiles) for OPEN windows
  //    IMPORTANT: Use the same savedIndex counter as CustomMDIArea::saveWorkspace()
  //    to keep content indices aligned with geometry indices. Skip off-screen
  //    cached windows just like saveWorkspace() does.
  QSettings settings("TradingCompany", "TradingTerminal");
  settings.beginGroup("workspaces/" + name);

  QList<CustomMDISubWindow *> windows = m_mdiArea->windowList();
  int savedIndex = 0;
  for (int i = 0; i < windows.count(); ++i) {
    CustomMDISubWindow *window = windows[i];

    // Skip off-screen cached windows (matches CustomMDIArea::saveWorkspace logic)
    if (window->geometry().x() < -1000)
      continue;

    if (!window->contentWidget()) {
      savedIndex++;
      continue;
    }

    settings.beginGroup(QString("window_%1").arg(savedIndex));

    if (window->windowType() == "MarketWatch") {
      MarketWatchWindow *mw =
          qobject_cast<MarketWatchWindow *>(window->contentWidget());
      if (mw)
        mw->saveState(settings);
    } else if (window->windowType() == "ATMWatch") {
      ATMWatchWindow *atm =
          qobject_cast<ATMWatchWindow *>(window->contentWidget());
      if (atm)
        atm->saveState(settings);
    } else if (window->windowType() == "OptionChain") {
      OptionChainWindow *oc =
          qobject_cast<OptionChainWindow *>(window->contentWidget());
      if (oc)
        oc->saveState(settings);
    } else {
      // BaseBookWindow covers OrderBook, TradeBook, PositionWindow
      BaseBookWindow *book =
          qobject_cast<BaseBookWindow *>(window->contentWidget());
      if (book)
        book->saveState(settings);
    }

    settings.endGroup();
    savedIndex++;
  }

  // 3. Save Default Profiles for specific window types
  settings.beginGroup("global_profiles");
  QStringList types = {"OrderBook", "TradeBook", "PositionWindow"};
  for (const auto &type : types) {
    GenericProfileManager pm("profiles", type);
    pm.loadCustomProfiles();
    QString defName = pm.loadDefaultProfileName();
    if (pm.hasProfile(defName)) {
      GenericTableProfile p = pm.getProfile(defName);
      settings.setValue(
          type, QJsonDocument(p.toJson()).toJson(QJsonDocument::Compact));
    }
  }
  settings.endGroup();

  settings.endGroup();
}

void WorkspaceManager::loadWorkspace() {
  if (!m_mdiArea)
    return;
  QStringList workspaces = m_mdiArea->availableWorkspaces();
  if (workspaces.isEmpty())
    return;
  bool ok;
  QString name =
      QInputDialog::getItem(m_mainWindow, "Load Workspace",
                            "Select workspace:", workspaces, 0, false, &ok);
  if (!ok || name.isEmpty())
    return;

  loadWorkspaceByName(name);
}

bool WorkspaceManager::loadWorkspaceByName(const QString &name) {
  if (!m_mdiArea || name.isEmpty())
    return false;

  if (!m_mdiArea->availableWorkspaces().contains(name))
    return false;

  // 1. Restore Global Profiles
  QSettings settings("TradingCompany", "TradingTerminal");
  settings.beginGroup("workspaces/" + name + "/global_profiles");

  QStringList types = settings.childKeys();
  for (const auto &type : types) {
    QByteArray data = settings.value(type).toByteArray();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
      GenericTableProfile p;
      p.fromJson(doc.object());
      GenericProfileManager typePm("profiles", type);
      typePm.saveCustomProfile(p);
      typePm.saveDefaultProfileName(p.name());
    }
  }
  settings.endGroup();

  // 2. Load Layout (triggers onRestoreWindowRequested for each window)
  m_mdiArea->loadWorkspace(name);
  return true;
}

void WorkspaceManager::manageWorkspaces() {
  // Basic implementation or dialog — placeholder
}

// ═══════════════════════════════════════════════════════════════════════
// Window restore dispatch
// ═══════════════════════════════════════════════════════════════════════

void WorkspaceManager::onRestoreWindowRequested(
    const QString &type, const QString &title, const QRect &geometry,
    bool isMinimized, bool isMaximized, bool isPinned,
    const QString &workspaceName, int index) {
  qDebug() << "[WorkspaceManager] Restoring window:" << type << title
           << "Index:" << index;

  CustomMDISubWindow *window = nullptr;

  // Dispatch to WindowFactory
  if (type == "MarketWatch") {
    window = m_factory->createMarketWatch();
  } else if (type == "BuyWindow") {
    window = m_factory->createBuyWindow();
  } else if (type == "SellWindow") {
    window = m_factory->createSellWindow();
  } else if (type == "SnapQuote" || type.startsWith("SnapQuote")) {
    window = m_factory->createSnapQuoteWindow();
  } else if (type == "OptionChain") {
    window = m_factory->createOptionChainWindow();
  } else if (type == "ATMWatch") {
    window = m_factory->createATMWatchWindow();
  } else if (type == "OptionCalculator") {
    window = m_factory->createOptionCalculatorWindow();
  } else if (type == "StrategyManager") {
    window = m_factory->createStrategyManagerWindow();
  } else if (type == "MarketMovement") {
    // MarketMovement requires a context — skip on workspace restore
    qDebug() << "[WorkspaceManager] Skipping MarketMovement (needs live context)";
    return;
  } else if (type == "OrderBook") {
    window = m_factory->createOrderBookWindow();
  } else if (type == "TradeBook") {
    window = m_factory->createTradeBookWindow();
  } else if (type == "PositionWindow") {
    window = m_factory->createPositionWindow();
  } else {
    qWarning() << "[WorkspaceManager] Unknown window type for restore:" << type;
    return;
  }

  if (!window) {
    qWarning() << "[WorkspaceManager] Failed to find restored window for:"
               << type;
    return;
  }

  // Apply saved geometry
  if (!isMaximized && !isMinimized) {
    window->setGeometry(geometry);
  }

  // Apply state
  if (isMaximized) {
    window->maximize();
  } else if (isMinimized) {
    m_mdiArea->minimizeWindow(window);
  }

  window->setPinned(isPinned);

  // Set title carefully (avoid incorrect titles on cached windows)
  bool shouldSetTitle = false;
  if (!title.isEmpty() && window->title() != title) {
    if ((type == "SnapQuote" || type.startsWith("SnapQuote")) &&
        title.startsWith("Snap Quote")) {
      shouldSetTitle = true;
    } else if (type == "BuyWindow" && title.contains("Buy")) {
      shouldSetTitle = true;
    } else if (type == "SellWindow" && title.contains("Sell")) {
      shouldSetTitle = true;
    } else if (type != "SnapQuote" && !type.startsWith("SnapQuote") &&
               type != "BuyWindow" && type != "SellWindow") {
      shouldSetTitle = true;
    }

    if (shouldSetTitle) {
      window->setTitle(title);
    } else {
      qDebug() << "[WorkspaceManager] Skipping mismatched title:" << title
               << "for window type:" << type;
    }
  }

  // Restore detailed state (script lists, column profiles, etc.)
  if (!workspaceName.isEmpty() && index >= 0 && window->contentWidget()) {
    QSettings settings("TradingCompany", "TradingTerminal");
    settings.beginGroup(
        QString("workspaces/%1/window_%2").arg(workspaceName).arg(index));

    if (type == "MarketWatch") {
      MarketWatchWindow *mw =
          qobject_cast<MarketWatchWindow *>(window->contentWidget());
      if (mw) {
        mw->setupZeroCopyMode();
        mw->restoreState(settings);
      }
    } else if (type == "ATMWatch") {
      ATMWatchWindow *atm =
          qobject_cast<ATMWatchWindow *>(window->contentWidget());
      if (atm)
        atm->restoreState(settings);
    } else if (type == "OptionChain") {
      OptionChainWindow *oc =
          qobject_cast<OptionChainWindow *>(window->contentWidget());
      if (oc)
        oc->restoreState(settings);
    } else {
      // BaseBookWindow covers OrderBook, TradeBook, PositionWindow
      BaseBookWindow *book =
          qobject_cast<BaseBookWindow *>(window->contentWidget());
      if (book)
        book->restoreState(settings);
    }

    settings.endGroup();
  }
}
