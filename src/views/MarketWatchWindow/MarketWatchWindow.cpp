#include "views/MarketWatchWindow.h"
#include "data/PriceStoreGateway.h"
#include "models/domain/TokenAddressBook.h"
#include "models/profiles/GenericProfileManager.h"
#include "models/profiles/MarketWatchColumnProfile.h"
#include "repository/RepositoryManager.h"
#include "services/FeedHandler.h"
#include "services/TokenSubscriptionManager.h"
#include "utils/PreferencesManager.h"
#include "utils/WindowSettingsHelper.h"
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QUuid>


// ============================================================================
// PERFORMANCE LOGGING - MarketWatch Window
// ============================================================================
// Logs all timing information for MarketWatch operations:
// - Window construction time
// - Data loading time
// - UI setup time
// - Scrip addition time
// - Price update latency
// ============================================================================

// PERFORMANCE OPTIMIZATION: Cache preference to avoid 50ms disk I/O per window
static bool s_useZeroCopyPriceCache_Cached = false;
static bool s_preferenceCached = false;

MarketWatchWindow::MarketWatchWindow(QWidget *parent)
    : CustomMarketWatch(parent), m_model(nullptr), m_tokenAddressBook(nullptr),
      m_xtsClient(nullptr),
      m_useZeroCopyPriceCache(false) // Will be loaded from preferences
      ,
      m_zeroCopyUpdateTimer(nullptr) {
  // ⏱️ PERFORMANCE LOG: Start timing window construction
  QElapsedTimer constructionTimer;
  constructionTimer.start();
  qint64 startTime = QDateTime::currentMSecsSinceEpoch();

  qDebug() << "========================================";
  qDebug() << "[PERF] [MARKETWATCH_CONSTRUCT] START";
  qDebug() << "  Timestamp:" << startTime;
  qDebug() << "========================================";

  qint64 t0 = constructionTimer.elapsed();

  // Setup UI
  setupUI();
  qint64 t1 = constructionTimer.elapsed();
  qDebug() << "[PERF] [MARKETWATCH_CONSTRUCT] setupUI() took:" << (t1 - t0)
           << "ms";

  // Setup connections
  setupConnections();
  qint64 t2 = constructionTimer.elapsed();
  qDebug() << "[PERF] [MARKETWATCH_CONSTRUCT] setupConnections() took:"
           << (t2 - t1) << "ms";

  // Load persisted column profile from JSON file (survives restart)
  {
    GenericProfileManager mgr("profiles", "MarketWatch");
    MarketWatchColumnProfile::registerPresets(mgr);
    mgr.loadCustomProfiles();

    // Priority: LastUsed file > named default > built-in preset
    GenericTableProfile savedProfile;
    if (mgr.loadLastUsedProfile(savedProfile)) {
      m_model->setColumnProfile(savedProfile);
      applyProfileToView(savedProfile);
      qDebug() << "[MarketWatch] Restored last-used column profile:" << savedProfile.name();
    } else {
      QString defName = mgr.loadDefaultProfileName();
      if (mgr.hasProfile(defName)) {
        savedProfile = mgr.getProfile(defName);
        m_model->setColumnProfile(savedProfile);
        applyProfileToView(savedProfile);
        qDebug() << "[MarketWatch] Restored column profile:" << defName;
      }
    }
  }

  // OPTIMIZATION: Defer keyboard shortcuts to after window is visible (saves
  // 9ms) setupKeyboardShortcuts();
  qint64 t3 = t2; // Skip for now
  // qDebug() << "[PERF] [MARKETWATCH_CONSTRUCT] setupKeyboardShortcuts() took:"
  // << (t3-t2) << "ms";

  // OPTIMIZATION: Defer settings load to after window is visible (saves 88ms)
  // WindowSettingsHelper::loadAndApplyWindowSettings(this, "MarketWatch");
  qint64 t4 = t3; // Skip for now
  // qDebug() << "[PERF] [MARKETWATCH_CONSTRUCT] loadAndApplyWindowSettings()
  // took:" << (t4-t3) << "ms";

  // OPTIMIZATION: Load preference once and cache it (saves 50ms on subsequent
  // windows)
  if (!s_preferenceCached) {
    PreferencesManager &prefs = PreferencesManager::instance();
    s_useZeroCopyPriceCache_Cached = !prefs.getUseLegacyPriceCache();
    s_preferenceCached = true;
    qDebug() << "[PERF] [MARKETWATCH_CONSTRUCT] First window - loaded "
                "preference from disk";
  }
  m_useZeroCopyPriceCache = s_useZeroCopyPriceCache_Cached;
  qint64 t5 = constructionTimer.elapsed();
  qDebug() << "[PERF] [MARKETWATCH_CONSTRUCT] Load PriceCache preference took:"
           << (t5 - t4) << "ms" << "(cached)";

  qDebug() << "[MarketWatch] PriceCache mode:"
           << (m_useZeroCopyPriceCache ? "ZERO-COPY (New)" : "LEGACY (Old)");

  // OPTIMIZATION: Defer shortcuts and settings to after window is visible
  QTimer::singleShot(0, this, [this]() {
    QElapsedTimer deferredTimer;
    deferredTimer.start();

    setupKeyboardShortcuts();
    qint64 shortcutsTime = deferredTimer.elapsed();

    WindowSettingsHelper::loadAndApplyWindowSettings(this, "MarketWatch");
    qint64 settingsTime = deferredTimer.elapsed() - shortcutsTime;

    qDebug() << "[PERF] [MARKETWATCH_DEFERRED] Shortcuts:" << shortcutsTime
             << "ms, Settings:" << settingsTime << "ms";
  });

  qint64 totalTime = constructionTimer.elapsed();
  /* qDebug() << "[PERF] [MARKETWATCH_CONSTRUCT] TOTAL TIME:" << totalTime <<
   * "ms"; */
}

MarketWatchWindow::~MarketWatchWindow() {
  // Stop timer if using zero-copy mode
  if (m_zeroCopyUpdateTimer) {
    m_zeroCopyUpdateTimer->stop();
  }

  // Unsubscribe from FeedHandler (legacy mode)
  FeedHandler::instance().unsubscribeAll(this);

  // Unsubscribe from tokens (legacy mode)
  if (m_model) {
    for (int row = 0; row < m_model->rowCount(); ++row) {
      const ScripData &scrip = m_model->getScripAt(row);
      if (scrip.isValid()) {
        TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange,
                                                          scrip.token);
      }
    }
  }

  // Clear pointer map
  m_tokenUnifiedPointers.clear();
}

void MarketWatchWindow::removeScripByToken(int token) {
  int row = findTokenRow(token);
  if (row >= 0)
    removeScrip(row);
}

void MarketWatchWindow::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Delete) {
    deleteSelectedRows();
    event->accept();
    return;
  }
  CustomMarketWatch::keyPressEvent(event);
}

void MarketWatchWindow::focusInEvent(QFocusEvent *event) {
  CustomMarketWatch::focusInEvent(event);
  
  // Restore focus to the last focused row when the Market Watch gains focus
  // Use a delayed timer to ensure the model is fully ready
  if (m_lastFocusedToken > 0) {
    qDebug() << "[MarketWatch] Focus gained, scheduling delayed focus restoration";
    QTimer::singleShot(50, this, [this]() {
      if (m_lastFocusedToken > 0) {
        restoreFocusedRow();
      }
    });
  } else {
    // No previously stored token — auto-select the first non-blank row
    // so keyboard shortcuts (F1/F2/Enter) work immediately
    QTimer::singleShot(50, this, [this]() {
      if (!selectionModel() || selectionModel()->hasSelection())
        return;
      if (!m_model || m_model->rowCount() == 0)
        return;

      // Find the first non-blank row in the source model
      for (int srcRow = 0; srcRow < m_model->rowCount(); ++srcRow) {
        if (!m_model->isBlankRow(srcRow)) {
          int proxyRow = mapToProxy(srcRow);
          if (proxyRow >= 0) {
            QModelIndex idx = proxyModel()->index(proxyRow, 0);
            setCurrentIndex(idx);
            selectRow(proxyRow);
            scrollTo(idx, QAbstractItemView::EnsureVisible);
            qDebug() << "[MarketWatch] Auto-selected first data row:" << srcRow
                     << "(proxy:" << proxyRow << ")";
            break;
          }
        }
      }
    });
  }
}

void MarketWatchWindow::focusOutEvent(QFocusEvent *event) {
  // Auto-store the current row before focus leaves so we can restore it later
  storeFocusedRow();
  CustomMarketWatch::focusOutEvent(event);
}

int MarketWatchWindow::getTokenForRow(int sourceRow) const {
  if (sourceRow < 0 || sourceRow >= m_model->rowCount())
    return -1;
  const ScripData &scrip = m_model->getScripAt(sourceRow);
  return scrip.isValid() ? scrip.token : -1;
}

bool MarketWatchWindow::isBlankRow(int sourceRow) const {
  if (sourceRow < 0 || sourceRow >= m_model->rowCount())
    return false;
  return m_model->isBlankRow(sourceRow);
}

WindowContext MarketWatchWindow::getSelectedContractContext() const {
  WindowContext context;
  context.sourceWindow = "MarketWatch";

  QModelIndexList selection = selectionModel()->selectedRows();
  if (selection.isEmpty())
    return context;

  QModelIndex lastIndex = selection.last();
  int sourceRow = mapToSource(lastIndex.row());
  if (sourceRow < 0 || sourceRow >= m_model->rowCount())
    return context;

  context.sourceRow = sourceRow;
  const ScripData &scrip = m_model->getScripAt(sourceRow);
  if (!scrip.isValid() || scrip.isBlankRow)
    return context;

  context.exchange = scrip.exchange;
  context.token = scrip.token;
  context.symbol = scrip.symbol;
  context.displayName = scrip.symbol;
  context.ltp = scrip.ltp;
  context.bid = scrip.bid;
  context.ask = scrip.ask;
  context.high = scrip.high;
  context.low = scrip.low;
  context.close = scrip.close;
  context.volume = scrip.volume;

  RepositoryManager *repo = RepositoryManager::getInstance();
  QString segment = (context.exchange == "NSEFO" || context.exchange == "BSEFO")
                        ? "FO"
                        : "CM";
  QString exchangeName = context.exchange.left(3);

  const ContractData *contract =
      repo->getContractByToken(exchangeName, segment, scrip.token);
  if (contract) {
    context.symbol = contract->name;
    context.lotSize = contract->lotSize;
    context.tickSize = contract->tickSize;
    context.freezeQty = contract->freezeQty;
    context.expiry = contract->expiryDate;
    context.strikePrice = contract->strikePrice;
    context.optionType = contract->optionType;
    context.instrumentType = contract->series;
    context.segment = (segment == "FO") ? "F" : "E";
  }

  return context;
}

bool MarketWatchWindow::hasValidSelection() const {
  QModelIndexList selection = selectionModel()->selectedRows();
  for (const QModelIndex &index : selection) {
    int sourceRow = mapToSource(index.row());
    if (sourceRow >= 0 && sourceRow < m_model->rowCount()) {
      const ScripData &scrip = m_model->getScripAt(sourceRow);
      if (scrip.isValid() && !scrip.isBlankRow)
        return true;
    }
  }
  return false;
}

void MarketWatchWindow::onRowUpdated(int row, int firstColumn, int lastColumn) {
  // Map source row to proxy row (sorting/filtering)
  int proxyRow = mapToProxy(row);
  if (proxyRow < 0)
    return;

  // Use ultra-fast direct viewport update (bypasses Qt's signal system) ✅
  // Only update the rect of the affected cells for maximum performance
  QRect firstRect = visualRect(proxyModel()->index(proxyRow, firstColumn));
  QRect lastRect = visualRect(proxyModel()->index(proxyRow, lastColumn));
  QRect updateRect = firstRect.united(lastRect);

  if (updateRect.isValid()) {
    viewport()->update(updateRect);
  }
}
void MarketWatchWindow::onRowsInserted(int firstRow, int lastRow) {
  viewport()->update();
}
void MarketWatchWindow::onRowsRemoved(int firstRow, int lastRow) {
  viewport()->update();
}
void MarketWatchWindow::onModelReset() { viewport()->update(); }

void MarketWatchWindow::closeEvent(QCloseEvent *event) {
  // Persist column profile to JSON file (survives restart)
  {
    GenericTableProfile currentProfile = m_model->getColumnProfile();
    captureProfileFromView(currentProfile);
    GenericProfileManager mgr("profiles", "MarketWatch");
    mgr.saveLastUsedProfile(currentProfile);         // always works, even for preset names
    mgr.saveDefaultProfileName(currentProfile.name());
  }
  WindowSettingsHelper::saveWindowSettings(this, "MarketWatch");
  QWidget::closeEvent(event);
}

// ============================================================================
// Zero-Copy PriceCache Implementation (New Mode)
// ============================================================================

void MarketWatchWindow::setupZeroCopyMode() {
  if (!m_useZeroCopyPriceCache)
    return;

  // Setup zero-copy mode
  qDebug() << "[MarketWatch] Setting up Zero-Copy mode connections...";

  // Setup timer for periodic zero-copy reads (100ms refresh)
  if (!m_zeroCopyUpdateTimer) {
    m_zeroCopyUpdateTimer = new QTimer(this);
    connect(m_zeroCopyUpdateTimer, &QTimer::timeout, this,
            &MarketWatchWindow::onZeroCopyTimerUpdate);
    m_zeroCopyUpdateTimer->start(100); // 100ms = 10 updates/sec
  }

  qDebug() << "[MarketWatch] ✓ Zero-copy PriceCache mode configured with 100ms "
              "timer";
}

void MarketWatchWindow::onZeroCopyTimerUpdate() {
  if (m_tokenUnifiedPointers.empty()) {
    return;
  }

  for (auto it = m_tokenUnifiedPointers.begin();
       it != m_tokenUnifiedPointers.end(); ++it) {
    uint32_t token = it.key();
    const MarketData::UnifiedState *dataPtr = it.value();

    if (!dataPtr)
      continue;

    const MarketData::UnifiedState &data = *dataPtr;

    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    if (rows.isEmpty())
      continue;

    // 1. Update LTP and change
    if (data.ltp > 0) {
      double ltp = data.ltp;
      double change = 0;
      double changePercent = 0;
      if (data.close > 0) {
        change = ltp - data.close;
        changePercent = (change / data.close) * 100.0;
      }

      for (int row : rows) {
        m_model->updatePrice(row, ltp, change, changePercent);
      }
    }

    // 2. Update OHLC
    if (data.open > 0 || data.high > 0 || data.low > 0) {
      for (int row : rows) {
        m_model->updateOHLC(row, data.open, data.high, data.low, data.close);
      }
    }

    // 3. Update volume
    if (data.volume > 0) {
      for (int row : rows) {
        m_model->updateVolume(row, data.volume);
      }
    }

    // 4. Update best bid/ask
    if (data.bids[0].price > 0 || data.asks[0].price > 0) {
      for (int row : rows) {
        m_model->updateBidAsk(row, data.bids[0].price, data.asks[0].price);
        m_model->updateBidAskQuantities(row, data.bids[0].quantity,
                                        data.asks[0].quantity);
      }
    }

    // 5. Update total buy/sell quantities
    if (data.totalBuyQty > 0 || data.totalSellQty > 0) {
      for (int row : rows) {
        m_model->updateTotalBuySellQty(row, data.totalBuyQty,
                                       data.totalSellQty);
      }
    }

    // 6. Update Open Interest
    if (data.openInterest > 0) {
      double oiChangePercent = 0.0;
      if (data.openInterestChange != 0) {
        oiChangePercent =
            (static_cast<double>(data.openInterestChange) / data.openInterest) *
            100.0;
      }
      for (int row : rows) {
        m_model->updateOpenInterestWithChange(row, data.openInterest,
                                              oiChangePercent);
      }
    }

    // 7. Update LTQ
    if (data.lastTradeQty > 0) {
      for (int row : rows) {
        m_model->updateLastTradedQuantity(row, data.lastTradeQty);
      }
    }

    // 8. Average Traded Price
    if (data.avgPrice > 0) {
      for (int row : rows) {
        m_model->updateAveragePrice(row, data.avgPrice);
      }
    }
  }
}

void MarketWatchWindow::storeFocusedRow()
{
    QModelIndexList selection = selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        qDebug() << "[MarketWatch] No row selected, cannot store focus";
        return;
    }
    
    QModelIndex lastIndex = selection.last();
    int sourceRow = mapToSource(lastIndex.row());
    
    if (sourceRow < 0 || sourceRow >= m_model->rowCount()) {
        qDebug() << "[MarketWatch] Invalid source row, cannot store focus";
        return;
    }
    
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    if (scrip.isValid() && !scrip.isBlankRow) {
        m_lastFocusedToken = scrip.token;
        m_lastFocusedSymbol = scrip.symbol;  // Store symbol for fallback
        qDebug() << "[MarketWatch] Stored focus on token:" << m_lastFocusedToken 
                 << "Symbol:" << scrip.symbol << "Row:" << sourceRow;
    } else {
        qDebug() << "[MarketWatch] Selected row is blank or invalid, cannot store focus";
    }
}

void MarketWatchWindow::restoreFocusedRow()
{
    if (m_lastFocusedToken <= 0 && m_lastFocusedSymbol.isEmpty()) {
        qDebug() << "[MarketWatch] No stored focus token or symbol, skipping restore";
        return;
    }
    
    // Check if model is valid and has data
    if (!m_model || m_model->rowCount() == 0) {
        qDebug() << "[MarketWatch] Model is not ready or empty, skipping focus restore";
        return;
    }
    
    int row = -1;
    
    // Try token-based lookup first (faster, O(1) via hash map)
    if (m_lastFocusedToken > 0) {
        row = findTokenRow(m_lastFocusedToken);
    }
    
    // Fallback to symbol-based lookup if token failed
    if (row < 0 && !m_lastFocusedSymbol.isEmpty()) {
        qDebug() << "[MarketWatch] Token" << m_lastFocusedToken << "not found, trying symbol fallback:" << m_lastFocusedSymbol;
        row = findSymbolRow(m_lastFocusedSymbol);
        
        if (row >= 0) {
            // Update token to match the found row for next time
            const ScripData &scrip = m_model->getScripAt(row);
            if (scrip.isValid()) {
                m_lastFocusedToken = scrip.token;
                qDebug() << "[MarketWatch] ✓ Found by symbol, updated token to" << m_lastFocusedToken;
            }
        }
    }
    
    if (row < 0) {
        qDebug() << "[MarketWatch] Neither token" << m_lastFocusedToken << "nor symbol" << m_lastFocusedSymbol << "found in model";
        // Don't clear immediately - keep for potential model updates/retries
        return;
    }
    
    int proxyRow = mapToProxy(row);
    if (proxyRow < 0) {
        qDebug() << "[MarketWatch] Could not map source row" << row << "to proxy row";
        return;
    }
    
    // Get the visual position of the row to simulate a click
    QModelIndex proxyIndex = proxyModel()->index(proxyRow, 0);
    QRect rowRect = visualRect(proxyIndex);
    
    // Simulate what happens on manual click:
    // 1. Clear existing selection
    clearSelection();
    
    // 2. Set current index (this is what Qt does on click)
    setCurrentIndex(proxyIndex);
    
    // 3. Select the row
    selectRow(proxyRow);
    
    // 4. Scroll to make it visible
    scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
    
    // 5. Give focus to the viewport (where clicks happen)
    viewport()->setFocus(Qt::MouseFocusReason);
    
    // 6. Also set focus on the view itself
    setFocus(Qt::MouseFocusReason);
    
    qDebug() << "[MarketWatch] ✓ Restored focus to token:" << m_lastFocusedToken 
             << "Source Row:" << row << "Proxy Row:" << proxyRow
             << "Using simulated click behavior";
}

void MarketWatchWindow::setFocusToToken(int token)
{
    if (token <= 0) {
        qDebug() << "[MarketWatch] Invalid token:" << token;
        return;
    }
    
    int row = findTokenRow(token);
    if (row < 0) {
        qDebug() << "[MarketWatch] Token" << token << "not found in model";
        return;
    }
    
    int proxyRow = mapToProxy(row);
    if (proxyRow < 0) {
        qDebug() << "[MarketWatch] Could not map source row" << row << "to proxy row";
        return;
    }
    
    // Simulate what happens on manual click:
    QModelIndex proxyIndex = proxyModel()->index(proxyRow, 0);
    
    // 1. Clear existing selection
    clearSelection();
    
    // 2. Set current index (this is what Qt does on click)
    setCurrentIndex(proxyIndex);
    
    // 3. Select the row
    selectRow(proxyRow);
    
    // 4. Scroll to make it visible
    scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
    
    // 5. Give focus to the viewport (where clicks happen)
    viewport()->setFocus(Qt::MouseFocusReason);
    
    // 6. Also set focus on the view itself
    setFocus(Qt::MouseFocusReason);
    
    // Update the last focused token and symbol
    m_lastFocusedToken = token;
    
    // Store symbol for fallback
    const ScripData &scrip = m_model->getScripAt(row);
    if (scrip.isValid()) {
        m_lastFocusedSymbol = scrip.symbol;
    }
    
    qDebug() << "[MarketWatch] ✓ Set focus to token:" << token 
             << "Source Row:" << row << "Proxy Row:" << proxyRow
             << "Using simulated click behavior";
}

int MarketWatchWindow::findSymbolRow(const QString& symbol) const
{
    if (!m_model || symbol.isEmpty()) {
        return -1;
    }
    
    // Delegate to model's findScrip method
    return m_model->findScrip(symbol);
}
