// ============================================================================
// ATMWatchWindow.cpp — Core lifecycle, state save/restore, context, key events
// ============================================================================
#include "views/ATMWatchWindow.h"
#include "views/GenericProfileDialog.h"
#include "models/domain/WindowContext.h"
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include "utils/TableProfileHelper.h"
#include "utils/WindowSettingsHelper.h"
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

ATMWatchWindow::ATMWatchWindow(QWidget *parent) : QWidget(parent) {
  setupUI();
  setupModels();
  setupConnections();
  setupShortcuts();

  // Initialize base price update timer (every 1 second)
  m_basePriceTimer = new QTimer(this);
  m_basePriceTimer->setInterval(1000); // 1 second
  connect(m_basePriceTimer, &QTimer::timeout, this,
          &ATMWatchWindow::onBasePriceUpdate);
  m_basePriceTimer->start();

  // Initialize generic profile managers for all three tables
  initProfileManagers();

  // Load persisted column profiles (widths, order, visibility) from JSON
  // This overwrites any legacy QSettings-based visibility.
  loadAllColumnProfiles();

  // Restore saved runtime state (combo selections, geometry)
  WindowSettingsHelper::loadAndApplyWindowSettings(this, "ATMWatch");

  // Initial data load
  refreshData();

  setWindowTitle("ATM Watch");
  // NOTE: Do NOT call resize() here — the factory applies saved geometry
  //       or a default size via applyRestoredGeometryOrDefault().
}

ATMWatchWindow::~ATMWatchWindow() {
  FeedHandler::instance().unsubscribeAll(this);
}

void ATMWatchWindow::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  // Load all symbols when window is shown (in background)
  loadAllSymbols();
  // Auto-focus symbol table so keyboard navigation is instant
  QTimer::singleShot(150, this, [this]() {
    m_symbolTable->setFocus();
    if (m_symbolModel->rowCount() > 0 && !m_symbolTable->currentIndex().isValid())
      m_symbolTable->selectRow(0);
  });
}

bool ATMWatchWindow::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::Wheel) {
    if (obj == m_callTable->viewport() || obj == m_putTable->viewport() ||
        obj == m_symbolTable->viewport()) {
      QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
      int currentValue = m_symbolTable->verticalScrollBar()->value();
      int delta = wheelEvent->angleDelta().y();
      int step = m_symbolTable->verticalScrollBar()->singleStep();
      int newValue = currentValue - (delta > 0 ? step : -step);
      m_symbolTable->verticalScrollBar()->setValue(newValue);
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

WindowContext ATMWatchWindow::getCurrentContext() {
  WindowContext context;
  context.sourceWindow = "ATMWatch";

  QTableView *activeTable = nullptr;
  if (m_callTable->hasFocus())
    activeTable = m_callTable;
  else if (m_putTable->hasFocus())
    activeTable = m_putTable;
  else if (m_symbolTable->hasFocus())
    activeTable = m_symbolTable;

  // Fallback to table with selection
  if (!activeTable) {
    if (m_callTable->selectionModel()->hasSelection())
      activeTable = m_callTable;
    else if (m_putTable->selectionModel()->hasSelection())
      activeTable = m_putTable;
    else if (m_symbolTable->selectionModel()->hasSelection())
      activeTable = m_symbolTable;
  }

  if (!activeTable)
    return context;

  QModelIndex index = activeTable->currentIndex();
  if (!index.isValid())
    return context;

  int row = index.row();
  QString symbol =
      m_symbolModel->data(m_symbolModel->index(row, SYM_NAME)).toString();

  // Retrieve ATM Info from Manager to get Tokens
  auto atmList = ATMWatchManager::getInstance().getATMWatchArray();
  ATMWatchManager::ATMInfo info;
  bool found = false;
  for (const auto &item : atmList) {
    if (item.symbol == symbol) {
      info = item;
      found = true;
      break;
    }
  }

  if (!found)
    return context;

  int64_t token = 0;
  if (activeTable == m_callTable) {
    token = info.callToken;
  } else if (activeTable == m_putTable) {
    token = info.putToken;
  } else {
    // Symbol Table (Underlying)
    token = info.underlyingToken;

    // Fix: If viewing Index Spot (Cash), switch to Future for trading context
    // because Indices themselves are not tradeable
    bool isIndex = (symbol == "NIFTY" || symbol == "BANKNIFTY" ||
                    symbol == "FINNIFTY" || symbol == "MIDCPNIFTY");

    if (isIndex) {
      auto repo = RepositoryManager::getInstance();
      int64_t futToken =
          repo->getFutureTokenForSymbolExpiry(symbol, info.expiry);
      if (futToken > 0) {
        token = futToken; // Switch to Future token
        qDebug() << "[ATMWatch] Context: Switched Index Spot to Future Token:"
                 << token;
      }
    }
  }

  if (token <= 0)
    return context;

  // Fetch full contract details
  auto repo = RepositoryManager::getInstance();
  const ContractData *contractPtr = nullptr;
  QString segment = "FO";

  // Try FO first
  contractPtr = repo->getContractByToken(m_currentExchange, "FO", token);

  // If not found, try CM
  if (!contractPtr) {
    contractPtr = repo->getContractByToken(m_currentExchange, "CM", token);
    segment = "CM";
  }
  if (!contractPtr)
    return context;

  const ContractData &contract = *contractPtr;

  qDebug() << "[ATMWatch] Context Resolved - Symbol:" << contract.name
           << "Series:" << contract.series << "Type:" << contract.instrumentType
           << "Token:" << token << "Segment:" << segment;

  context.exchange = m_currentExchange;
  context.token = token;
  context.symbol = contract.name;
  context.displayName = contract.description;
  context.segment = segment;
  context.series = contract.series;

  // Populate Option Details
  context.optionType = contract.optionType;
  context.expiry = contract.expiryDate;
  context.strikePrice = contract.strikePrice;
  context.lotSize = contract.lotSize;
  context.tickSize = contract.tickSize;

  // Use series from contract if available, otherwise smart detect
  if (!contract.series.isEmpty()) {
    context.instrumentType = contract.series;
  } else {
    bool isIndex =
        (contract.name == "NIFTY" || contract.name == "BANKNIFTY" ||
         contract.name == "FINNIFTY" || contract.name == "MIDCPNIFTY");

    if (contract.instrumentType == 1) { // Future
      context.instrumentType = isIndex ? "FUTIDX" : "FUTSTK";
    } else if (contract.instrumentType == 2) { // Option
      context.instrumentType = isIndex ? "OPTIDX" : "OPTSTK";
    } else {
      context.instrumentType = "EQ";
    }
  }

  qDebug() << "[ATMWatch] Final InstrumentType:" << context.instrumentType;

  context.expiry = contract.expiryDate;
  context.strikePrice = contract.strikePrice;
  context.optionType = contract.optionType;
  context.lotSize = contract.lotSize;
  context.tickSize = contract.tickSize;
  context.freezeQty = contract.freezeQty;

  return context;
}

void ATMWatchWindow::keyPressEvent(QKeyEvent *event) {
  // F5: let global SnapQuote shortcut handle it
  if (event->key() == Qt::Key_F5) {
    QWidget::keyPressEvent(event);
    return;
  }

  // Escape: close the parent MDI subwindow
  if (event->key() == Qt::Key_Escape) {
    QWidget *p = parentWidget();
    while (p) {
      if (p->inherits("CustomMDISubWindow")) {
        p->close();
        event->accept();
        return;
      }
      p = p->parentWidget();
    }
    close(); // Fallback: close self
    event->accept();
    return;
  }

  // Enter/Return (symbol table focused): open Option Chain
  if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
      && m_symbolTable->hasFocus()) {
    QModelIndex idx = m_symbolTable->currentIndex();
    if (idx.isValid()) {
      onSymbolDoubleClicked(idx);
    }
    event->accept();
    return;
  }

  // F1 Buy | F2 Sell | F6 SnapQuote | Delete remove watch
  if (event->key() == Qt::Key_F1 || event->key() == Qt::Key_F2 ||
      event->key() == Qt::Key_F6 || event->key() == Qt::Key_Delete) {
    event->accept();
    WindowContext context = getCurrentContext();

    if (event->key() == Qt::Key_Delete) {
      if (context.isValid()) {
        ATMWatchManager::getInstance().removeWatch(context.symbol);
      }
      return;
    }

    if (!context.isValid()) {
      return;
    }

    if (event->key() == Qt::Key_F1) { // Buy
      emit buyRequested(context);
    } else if (event->key() == Qt::Key_F2) { // Sell
      emit sellRequested(context);
    } else if (event->key() == Qt::Key_F6) { // SnapQuote
      emit snapQuoteRequested(context);
    }
    return;
  }

  QWidget::keyPressEvent(event);
}

// ════════════════════════════════════════════════════════════════════════════
// Workspace save / restore
// ════════════════════════════════════════════════════════════════════════════

void ATMWatchWindow::saveState(QSettings &settings) {
  // Save combo selections
  settings.setValue("exchange", m_exchangeCombo->currentText());
  settings.setValue("expiry", m_expiryCombo->currentText());
  settings.setValue("expiryData", m_expiryCombo->currentData().toString());

  // Save sort state
  settings.setValue("sortSource", static_cast<int>(m_sortSource));
  settings.setValue("sortColumn", m_sortColumn);
  settings.setValue("sortOrder", static_cast<int>(m_sortOrder));

  // Persist column profiles to JSON file (survives restart)
  saveAllColumnProfiles();

  qDebug() << "[ATMWatchWindow] State saved - exchange:" << m_exchangeCombo->currentText()
           << "expiry:" << m_expiryCombo->currentText();
}

void ATMWatchWindow::restoreState(QSettings &settings) {
  // Restore exchange combo
  if (settings.contains("exchange")) {
    QString exchange = settings.value("exchange").toString();
    int idx = m_exchangeCombo->findText(exchange);
    if (idx >= 0) {
      m_exchangeCombo->setCurrentIndex(idx);
    }
  }

  // Restore expiry combo (after exchange triggers expiry population)
  if (settings.contains("expiry")) {
    QString expiry = settings.value("expiry").toString();
    int idx = m_expiryCombo->findText(expiry);
    if (idx >= 0) {
      m_expiryCombo->setCurrentIndex(idx);
    }
  }

  // Column profiles are restored from JSON file (loadColumnProfiles in constructor)
  // No need to restore from QSettings anymore.

  // Restore sort state
  if (settings.contains("sortSource")) {
    m_sortSource = static_cast<SortSource>(settings.value("sortSource").toInt());
    m_sortColumn = settings.value("sortColumn", 0).toInt();
    m_sortOrder = static_cast<Qt::SortOrder>(settings.value("sortOrder", 0).toInt());
  }

  // Trigger refresh with restored settings
  refreshData();

  qDebug() << "[ATMWatchWindow] State restored";
}

void ATMWatchWindow::closeEvent(QCloseEvent *event) {
  // Persist column profiles (widths, order, visibility) to JSON
  saveAllColumnProfiles();
  WindowSettingsHelper::saveWindowSettings(this, "ATMWatch");
  QWidget::closeEvent(event);
}

// ════════════════════════════════════════════════════════════════════════════
// Column Profile Management — GenericProfileManager per table
// ════════════════════════════════════════════════════════════════════════════

QList<GenericColumnInfo> ATMWatchWindow::buildCallColumnMetadata() {
  return {
    { CALL_CHG,   "Chg",    60,  true  },
    { CALL_VOL,   "Vol",    60,  true  },
    { CALL_OI,    "OI",     65,  true  },
    { CALL_IV,    "IV",     55,  true  },
    { CALL_DELTA, "Delta",  60,  false },
    { CALL_GAMMA, "Gamma",  60,  false },
    { CALL_VEGA,  "Vega",   60,  false },
    { CALL_THETA, "Theta",  60,  false },
    { CALL_LTP,   "LTP",    65,  true  },
    { CALL_BID,   "Bid",    60,  true  },
    { CALL_ASK,   "Ask",    60,  true  },
  };
}

QList<GenericColumnInfo> ATMWatchWindow::buildPutColumnMetadata() {
  return {
    { PUT_LTP,   "LTP",    65,  true  },
    { PUT_BID,   "Bid",    60,  true  },
    { PUT_ASK,   "Ask",    60,  true  },
    { PUT_CHG,   "Chg",    60,  true  },
    { PUT_VOL,   "Vol",    60,  true  },
    { PUT_OI,    "OI",     65,  true  },
    { PUT_IV,    "IV",     55,  true  },
    { PUT_DELTA, "Delta",  60,  false },
    { PUT_GAMMA, "Gamma",  60,  false },
    { PUT_VEGA,  "Vega",   60,  false },
    { PUT_THETA, "Theta",  60,  false },
  };
}

QList<GenericColumnInfo> ATMWatchWindow::buildSymbolColumnMetadata() {
  return {
    { SYM_NAME,   "Symbol",  80,  true },
    { SYM_PRICE,  "Price",   65,  true },
    { SYM_ATM,    "ATM",     65,  true },
    { SYM_EXPIRY, "Expiry",  75,  true },
  };
}

void ATMWatchWindow::initProfileManagers() {
  // Call table
  m_callProfileMgr = new GenericProfileManager("profiles", "ATMWatch_Call");
  auto callMeta = buildCallColumnMetadata();
  GenericTableProfile callDefault = GenericTableProfile::createDefault(callMeta);
  callDefault.setName("Default");
  m_callProfileMgr->addPreset(callDefault);

  // Compact preset for call — hide greeks
  {
    GenericTableProfile p("Compact");
    p.setDescription("Hide Greeks, show core values only");
    QList<int> order;
    for (const auto &c : callMeta) {
      order.append(c.id);
      bool vis = (c.id == CALL_LTP || c.id == CALL_BID || c.id == CALL_ASK ||
                  c.id == CALL_CHG || c.id == CALL_OI || c.id == CALL_VOL);
      p.setColumnVisible(c.id, vis);
      p.setColumnWidth(c.id, c.defaultWidth);
    }
    p.setColumnOrder(order);
    m_callProfileMgr->addPreset(p);
  }

  // Greeks preset — show greeks + IV
  {
    GenericTableProfile p("Greeks");
    p.setDescription("Greek values and IV focused");
    QList<int> order;
    for (const auto &c : callMeta) {
      order.append(c.id);
      bool vis = (c.id == CALL_IV || c.id == CALL_DELTA || c.id == CALL_GAMMA ||
                  c.id == CALL_VEGA || c.id == CALL_THETA || c.id == CALL_LTP);
      p.setColumnVisible(c.id, vis);
      p.setColumnWidth(c.id, c.defaultWidth);
    }
    p.setColumnOrder(order);
    m_callProfileMgr->addPreset(p);
  }

  m_callProfileMgr->loadCustomProfiles();

  // Symbol table (center panel)
  m_symbolProfileMgr = new GenericProfileManager("profiles", "ATMWatch_Symbol");
  auto symMeta = buildSymbolColumnMetadata();
  GenericTableProfile symDefault = GenericTableProfile::createDefault(symMeta);
  symDefault.setName("Default");
  m_symbolProfileMgr->addPreset(symDefault);

  // Compact preset for symbol — hide ATM and Expiry, keep Symbol + Price
  {
    GenericTableProfile p("Compact");
    p.setDescription("Symbol and Price only");
    QList<int> order;
    for (const auto &c : symMeta) {
      order.append(c.id);
      bool vis = (c.id == SYM_NAME || c.id == SYM_PRICE);
      p.setColumnVisible(c.id, vis);
      p.setColumnWidth(c.id, c.defaultWidth);
    }
    p.setColumnOrder(order);
    m_symbolProfileMgr->addPreset(p);
  }

  m_symbolProfileMgr->loadCustomProfiles();

  // Put table (mirrors call presets)
  m_putProfileMgr = new GenericProfileManager("profiles", "ATMWatch_Put");
  auto putMeta = buildPutColumnMetadata();
  GenericTableProfile putDefault = GenericTableProfile::createDefault(putMeta);
  putDefault.setName("Default");
  m_putProfileMgr->addPreset(putDefault);

  // Compact preset for put
  {
    GenericTableProfile p("Compact");
    p.setDescription("Hide Greeks, show core values only");
    QList<int> order;
    for (const auto &c : putMeta) {
      order.append(c.id);
      bool vis = (c.id == PUT_LTP || c.id == PUT_BID || c.id == PUT_ASK ||
                  c.id == PUT_CHG || c.id == PUT_OI || c.id == PUT_VOL);
      p.setColumnVisible(c.id, vis);
      p.setColumnWidth(c.id, c.defaultWidth);
    }
    p.setColumnOrder(order);
    m_putProfileMgr->addPreset(p);
  }

  // Greeks preset for put
  {
    GenericTableProfile p("Greeks");
    p.setDescription("Greek values and IV focused");
    QList<int> order;
    for (const auto &c : putMeta) {
      order.append(c.id);
      bool vis = (c.id == PUT_IV || c.id == PUT_DELTA || c.id == PUT_GAMMA ||
                  c.id == PUT_VEGA || c.id == PUT_THETA || c.id == PUT_LTP);
      p.setColumnVisible(c.id, vis);
      p.setColumnWidth(c.id, c.defaultWidth);
    }
    p.setColumnOrder(order);
    m_putProfileMgr->addPreset(p);
  }

  m_putProfileMgr->loadCustomProfiles();
}

void ATMWatchWindow::saveAllColumnProfiles() {
  if (!m_callProfileMgr || !m_symbolProfileMgr || !m_putProfileMgr) return;

  TableProfileHelper::captureProfile(m_callTable, m_callModel, m_callProfile);
  TableProfileHelper::captureProfile(m_symbolTable, m_symbolModel, m_symbolProfile);
  TableProfileHelper::captureProfile(m_putTable, m_putModel, m_putProfile);

  m_callProfileMgr->saveLastUsedProfile(m_callProfile);
  m_symbolProfileMgr->saveLastUsedProfile(m_symbolProfile);
  m_putProfileMgr->saveLastUsedProfile(m_putProfile);

  qDebug() << "[ATMWatch] All column profiles saved via GenericProfileManager";
}

void ATMWatchWindow::loadAllColumnProfiles() {
  if (!m_callProfileMgr || !m_symbolProfileMgr || !m_putProfileMgr) return;

  // Call table
  {
    GenericTableProfile lastUsed;
    if (m_callProfileMgr->loadLastUsedProfile(lastUsed)) {
      m_callProfile = lastUsed;
    } else {
      QString defName = m_callProfileMgr->loadDefaultProfileName();
      m_callProfile = m_callProfileMgr->hasProfile(defName)
          ? m_callProfileMgr->getProfile(defName)
          : GenericTableProfile::createDefault(buildCallColumnMetadata());
    }
    TableProfileHelper::applyProfile(m_callTable, m_callModel, m_callProfile);
  }

  // Symbol table
  {
    GenericTableProfile lastUsed;
    if (m_symbolProfileMgr->loadLastUsedProfile(lastUsed)) {
      m_symbolProfile = lastUsed;
    } else {
      m_symbolProfile = GenericTableProfile::createDefault(buildSymbolColumnMetadata());
    }
    TableProfileHelper::applyProfile(m_symbolTable, m_symbolModel, m_symbolProfile);
  }

  // Put table
  {
    GenericTableProfile lastUsed;
    if (m_putProfileMgr->loadLastUsedProfile(lastUsed)) {
      m_putProfile = lastUsed;
    } else {
      QString defName = m_putProfileMgr->loadDefaultProfileName();
      m_putProfile = m_putProfileMgr->hasProfile(defName)
          ? m_putProfileMgr->getProfile(defName)
          : GenericTableProfile::createDefault(buildPutColumnMetadata());
    }
    TableProfileHelper::applyProfile(m_putTable, m_putModel, m_putProfile);
  }

  // Column profiles loaded from file override the auto-fit
  m_initialColumnsResized = true;

  qDebug() << "[ATMWatch] Column profiles loaded via GenericProfileManager"
           << "call:" << m_callProfile.name()
           << "put:" << m_putProfile.name();
}

void ATMWatchWindow::showCallColumnDialog() {
  if (!m_callProfileMgr) return;

  // Capture current state via TableProfileHelper
  TableProfileHelper::captureProfile(m_callTable, m_callModel, m_callProfile);

  auto colMeta = buildCallColumnMetadata();
  GenericProfileDialog dialog("ATM Watch — Call Columns", colMeta, m_callProfileMgr, m_callProfile, this);
  if (dialog.exec() == QDialog::Accepted) {
    m_callProfile = dialog.getProfile();
    TableProfileHelper::applyProfile(m_callTable, m_callModel, m_callProfile);
    m_callProfileMgr->saveLastUsedProfile(m_callProfile);
    m_callProfileMgr->saveCustomProfile(m_callProfile);
    m_callProfileMgr->saveDefaultProfileName(m_callProfile.name());
    qInfo() << "[ATMWatch] Call column profile updated:" << m_callProfile.name();
  }
}

void ATMWatchWindow::showPutColumnDialog() {
  if (!m_putProfileMgr) return;

  // Capture current state via TableProfileHelper
  TableProfileHelper::captureProfile(m_putTable, m_putModel, m_putProfile);

  auto colMeta = buildPutColumnMetadata();
  GenericProfileDialog dialog("ATM Watch — Put Columns", colMeta, m_putProfileMgr, m_putProfile, this);
  if (dialog.exec() == QDialog::Accepted) {
    m_putProfile = dialog.getProfile();
    TableProfileHelper::applyProfile(m_putTable, m_putModel, m_putProfile);
    m_putProfileMgr->saveLastUsedProfile(m_putProfile);
    m_putProfileMgr->saveCustomProfile(m_putProfile);
    m_putProfileMgr->saveDefaultProfileName(m_putProfile.name());
    qInfo() << "[ATMWatch] Put column profile updated:" << m_putProfile.name();
  }
}

void ATMWatchWindow::showSymbolColumnDialog() {
  if (!m_symbolProfileMgr) return;

  // Capture current state via TableProfileHelper
  TableProfileHelper::captureProfile(m_symbolTable, m_symbolModel, m_symbolProfile);

  auto colMeta = buildSymbolColumnMetadata();
  GenericProfileDialog dialog("ATM Watch — Symbol Columns", colMeta, m_symbolProfileMgr, m_symbolProfile, this);
  if (dialog.exec() == QDialog::Accepted) {
    m_symbolProfile = dialog.getProfile();
    TableProfileHelper::applyProfile(m_symbolTable, m_symbolModel, m_symbolProfile);
    m_symbolProfileMgr->saveLastUsedProfile(m_symbolProfile);
    m_symbolProfileMgr->saveCustomProfile(m_symbolProfile);
    m_symbolProfileMgr->saveDefaultProfileName(m_symbolProfile.name());
    qInfo() << "[ATMWatch] Symbol column profile updated:" << m_symbolProfile.name();
  }
}
