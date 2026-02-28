// ============================================================================
// OptionChainWindow.cpp — Core lifecycle, state save/restore, context, key events
// ============================================================================
#include "views/OptionChainWindow.h"
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include "utils/WindowSettingsHelper.h"
#include "views/GenericProfileDialog.h"
#include <QDebug>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QScrollBar>
#include <QSettings>
#include <QTimer>
#include <limits>

OptionChainWindow::OptionChainWindow(QWidget *parent)
    : QWidget(parent), m_symbolCombo(nullptr), m_expiryCombo(nullptr),
      m_refreshButton(nullptr), m_calculatorButton(nullptr),
      m_titleLabel(nullptr), m_callTable(nullptr), m_strikeTable(nullptr),
      m_putTable(nullptr), m_callModel(nullptr), m_strikeModel(nullptr),
      m_putModel(nullptr), m_callDelegate(nullptr), m_putDelegate(nullptr),
      m_atmStrike(0.0), m_exchangeSegment(2), m_selectedCallRow(-1),
      m_selectedPutRow(-1), m_profileManager(nullptr) {
  setupUI();
  setupModels();
  setupConnections();
  setupShortcuts();

  // Build column metadata & register preset profiles
  QList<GenericColumnInfo> colMeta = buildColumnMetadata();
  m_profileManager = new GenericProfileManager("profiles", "OptionChain");
  m_profileManager->addPreset(createPreset_Default(colMeta));
  m_profileManager->addPreset(createPreset_Compact(colMeta));
  m_profileManager->addPreset(createPreset_Greeks(colMeta));
  m_profileManager->addPreset(createPreset_Trading(colMeta));
  m_profileManager->loadCustomProfiles();

  // Restore last-used profile (survives restart)
  // Priority: LastUsed file > named default/custom > QSettings legacy > built-in preset
  {
    GenericTableProfile lastUsed;
    if (m_profileManager->loadLastUsedProfile(lastUsed)) {
      m_columnProfile = lastUsed;
      qDebug() << "[OptionChain] Loaded last-used column profile:" << lastUsed.name();
    } else {
      QString defName = m_profileManager->loadDefaultProfileName();
      if (m_profileManager->hasProfile(defName)) {
        m_columnProfile = m_profileManager->getProfile(defName);
        qDebug() << "[OptionChain] Loaded column profile from file:" << defName;
      } else {
        // Legacy fallback: try QSettings
        QSettings settings("configs/config.ini", QSettings::IniFormat);
        settings.beginGroup("OPTION_CHAIN_PROFILE");
        QString json = settings.value("profile_json").toString();
        settings.endGroup();

        if (!json.isEmpty()) {
          QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
          if (!doc.isNull()) {
            m_columnProfile.fromJson(doc.object());
            qDebug() << "[OptionChain] Loaded column profile from QSettings (legacy)";
          }
        }
      }
    }
  }

  // Apply column visibility from the loaded profile
  applyColumnVisibility();

  // Populate symbols (silently, without triggering partial refreshes)
  populateSymbols();

  // Restore saved runtime state (combo selections, geometry)
  WindowSettingsHelper::loadAndApplyWindowSettings(this, "OptionChain");

  // One explicit refresh to load initial data
  refreshData();

  setWindowTitle("Option Chain");
  // NOTE: Do NOT call resize() here — the factory applies saved geometry
  //       or a default size via applyRestoredGeometryOrDefault().
}

OptionChainWindow::~OptionChainWindow() {
  delete m_profileManager;
}

void OptionChainWindow::setSymbol(const QString &symbol,
                                  const QString &expiry) {
  m_currentSymbol = symbol;
  m_currentExpiry = expiry;

  m_symbolCombo->setCurrentText(symbol);
  m_expiryCombo->setCurrentText(expiry);
  m_titleLabel->setText(symbol);

  // Refresh data
  emit refreshRequested();
}

void OptionChainWindow::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  // Auto-focus call table for instant keyboard navigation
  // Use 200ms to run after refreshData's 0ms ATM-scroll timer
  QTimer::singleShot(200, this, [this]() {
    m_callTable->setFocus();
    if (m_callModel->rowCount() > 0 && !m_callTable->currentIndex().isValid()) {
      int targetRow = 0;
      if (m_atmStrike > 0.0) {
        int atmIdx = m_strikes.indexOf(m_atmStrike);
        if (atmIdx >= 0)
          targetRow = atmIdx;
      }
      m_callTable->selectRow(targetRow);
    }
  });
}

void OptionChainWindow::focusInEvent(QFocusEvent *event) {
  QWidget::focusInEvent(event);

  QTimer::singleShot(50, this, [this]() {
    if (!m_callTable || m_callModel->rowCount() == 0)
      return;

    if (m_callTable->currentIndex().isValid())
      return;

    int targetRow = 0;
    if (m_atmStrike > 0.0) {
      int atmIdx = m_strikes.indexOf(m_atmStrike);
      if (atmIdx >= 0)
        targetRow = atmIdx;
    }
    m_callTable->selectRow(targetRow);
    m_callTable->setFocus();
    qDebug() << "[OptionChain] Auto-selected row on focus gain:" << targetRow
             << "(ATM strike:" << m_atmStrike << ")";
  });
}

void OptionChainWindow::keyPressEvent(QKeyEvent *event) {
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
    close();
    event->accept();
    return;
  }

  // F1: Buy CE (call focused) | PE (put focused) | CE (strike focused)
  if (event->key() == Qt::Key_F1) {
    int targetRow = -1;
    QString optType;
    if (m_callTable->hasFocus() && m_selectedCallRow >= 0) {
      targetRow = m_selectedCallRow;
      optType = "CE";
    } else if (m_putTable->hasFocus() && m_selectedPutRow >= 0) {
      targetRow = m_selectedPutRow;
      optType = "PE";
    } else if (m_strikeTable->hasFocus() && m_strikeTable->currentIndex().isValid()) {
      targetRow = m_strikeTable->currentIndex().row();
      optType = "CE";
    } else if (m_selectedCallRow >= 0) {
      targetRow = m_selectedCallRow;
      optType = "CE";
    } else if (m_selectedPutRow >= 0) {
      targetRow = m_selectedPutRow;
      optType = "PE";
    }

    if (targetRow >= 0) {
      if (optType == "CE") {
        m_selectedCallRow = targetRow;
        m_callTable->selectRow(targetRow);
      } else {
        m_selectedPutRow = targetRow;
        m_putTable->selectRow(targetRow);
      }
      WindowContext ctx = getSelectedContext();
      if (ctx.isValid()) {
        emit buyRequested(ctx);
      }
    }
    event->accept();
    return;
  }

  // F2: Sell CE/PE
  if (event->key() == Qt::Key_F2) {
    int targetRow = -1;
    QString optType;
    if (m_callTable->hasFocus() && m_selectedCallRow >= 0) {
      targetRow = m_selectedCallRow;
      optType = "CE";
    } else if (m_putTable->hasFocus() && m_selectedPutRow >= 0) {
      targetRow = m_selectedPutRow;
      optType = "PE";
    } else if (m_strikeTable->hasFocus() && m_strikeTable->currentIndex().isValid()) {
      targetRow = m_strikeTable->currentIndex().row();
      optType = "PE";
    } else if (m_selectedCallRow >= 0) {
      targetRow = m_selectedCallRow;
      optType = "CE";
    } else if (m_selectedPutRow >= 0) {
      targetRow = m_selectedPutRow;
      optType = "PE";
    }

    if (targetRow >= 0) {
      if (optType == "CE") {
        m_selectedCallRow = targetRow;
        m_callTable->selectRow(targetRow);
      } else {
        m_selectedPutRow = targetRow;
        m_putTable->selectRow(targetRow);
      }
      WindowContext ctx = getSelectedContext();
      if (ctx.isValid()) {
        emit sellRequested(ctx);
      }
    }
    event->accept();
    return;
  }

  QWidget::keyPressEvent(event);
}

WindowContext OptionChainWindow::getSelectedContext() const {
  WindowContext context;
  context.sourceWindow = "OptionChain";

  double strike = 0.0;
  QString optionType;
  int token = 0;

  // Prioritize Call selection
  if (m_selectedCallRow >= 0) {
    strike = getStrikeAtRow(m_selectedCallRow);
    optionType = "CE";
    if (m_strikeData.contains(strike)) {
      token = m_strikeData[strike].callToken;
      context.ltp = m_strikeData[strike].callLTP;
      context.bid = m_strikeData[strike].callBid;
      context.ask = m_strikeData[strike].callAsk;
      context.volume = m_strikeData[strike].callVolume;
    }
  }
  // Check Put selection
  else if (m_selectedPutRow >= 0) {
    strike = getStrikeAtRow(m_selectedPutRow);
    optionType = "PE";
    if (m_strikeData.contains(strike)) {
      token = m_strikeData[strike].putToken;
      context.ltp = m_strikeData[strike].putLTP;
      context.bid = m_strikeData[strike].putBid;
      context.ask = m_strikeData[strike].putAsk;
      context.volume = m_strikeData[strike].putVolume;
    }
  }

  if (token > 0) {
    context.token = token;
    context.symbol = m_currentSymbol;
    context.expiry = m_currentExpiry;
    context.strikePrice = strike;
    context.optionType = optionType;

    // Fetch detailed contract info
    const ContractData *contract = nullptr;

    contract =
        RepositoryManager::getInstance()->getContractByToken("NSEFO", token);
    if (contract) {
      context.exchange = "NSEFO";
    } else {
      contract =
          RepositoryManager::getInstance()->getContractByToken("BSEFO", token);
      if (contract)
        context.exchange = "BSEFO";
    }

    if (contract) {
      context.segment = "D"; // Derivative
      context.instrumentType = contract->instrumentType;
      context.lotSize = contract->lotSize;
      context.tickSize = contract->tickSize;
      context.freezeQty = contract->freezeQty;
      context.displayName = contract->displayName;
      context.series = contract->series;
    }
  }

  return context;
}

// ════════════════════════════════════════════════════════════════════════════
// Workspace save / restore
// ════════════════════════════════════════════════════════════════════════════

void OptionChainWindow::saveState(QSettings &settings) {
  settings.setValue("symbol", m_currentSymbol);
  settings.setValue("expiry", m_currentExpiry);
  settings.setValue("exchangeSegment", m_exchangeSegment);

  captureColumnWidths();
  QJsonDocument doc(m_columnProfile.toJson());
  settings.setValue("columnProfile", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));

  settings.setValue("atmStrike", m_atmStrike);

  qDebug() << "[OptionChainWindow] State saved - symbol:" << m_currentSymbol
           << "expiry:" << m_currentExpiry;
}

void OptionChainWindow::restoreState(QSettings &settings) {
  // Restore column profile first (before refreshData)
  if (settings.contains("columnProfile")) {
    QByteArray data = settings.value("columnProfile").toByteArray();
    if (data.isEmpty())
      data = settings.value("columnProfile").toString().toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
      m_columnProfile.fromJson(doc.object());
      applyColumnVisibility();
      qDebug() << "[OptionChainWindow] Restored column profile:" << m_columnProfile.name();
    }
  }

  // Restore exchange segment
  if (settings.contains("exchangeSegment")) {
    m_exchangeSegment = settings.value("exchangeSegment").toInt();
  }

  // Restore symbol and expiry (triggers data refresh)
  QString symbol = settings.value("symbol").toString();
  QString expiry = settings.value("expiry").toString();
  if (!symbol.isEmpty()) {
    int symIdx = m_symbolCombo->findText(symbol);
    if (symIdx >= 0) {
      m_symbolCombo->blockSignals(true);
      m_symbolCombo->setCurrentIndex(symIdx);
      m_symbolCombo->blockSignals(false);
    }

    populateExpiries(symbol);

    if (!expiry.isEmpty()) {
      int expIdx = m_expiryCombo->findText(expiry);
      if (expIdx >= 0) {
        m_expiryCombo->blockSignals(true);
        m_expiryCombo->setCurrentIndex(expIdx);
        m_expiryCombo->blockSignals(false);
      }
    }

    setSymbol(symbol, expiry.isEmpty() ? m_expiryCombo->currentText() : expiry);
  }

  qDebug() << "[OptionChainWindow] State restored";
}

void OptionChainWindow::closeEvent(QCloseEvent *event) {
  // Persist column profile to JSON file (survives restart)
  captureColumnWidths();
  if (m_profileManager) {
    m_profileManager->saveLastUsedProfile(m_columnProfile);  // always works, even for preset names
    m_profileManager->saveCustomProfile(m_columnProfile);     // also save as custom if not a preset
    m_profileManager->saveDefaultProfileName(m_columnProfile.name());
  }
  WindowSettingsHelper::saveWindowSettings(this, "OptionChain");
  QWidget::closeEvent(event);
}
