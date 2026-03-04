// ============================================================================
// UI.cpp — ATMWatch UI setup, models, connections, shortcuts, styling
// ============================================================================
#include "views/ATMWatchWindow.h"
#include "ui_ATMWatchWindow.h"
#include "views/ATMWatchSettingsDialog.h"
#include "views/GenericProfileDialog.h"
#include "services/GreeksCalculationService.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QShortcut>
#include <QStandardItem>
#include <QTableView>
#include <QToolBar>
#include <QVBoxLayout>

void ATMWatchWindow::setupUI() {
  ui = new Ui::ATMWatchWindow();
  ui->setupUi(this);

  // Assign convenience pointers from ui
  m_toolbar       = ui->m_toolbar;
  m_exchangeCombo = ui->m_exchangeCombo;
  m_expiryCombo   = ui->m_expiryCombo;
  m_statusLabel   = ui->m_statusLabel;
  m_callTable     = ui->m_callTable;
  m_symbolTable   = ui->m_symbolTable;
  m_putTable      = ui->m_putTable;

  // Toolbar actions (must be added programmatically)
  m_toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
  QAction *settingsAction = m_toolbar->addAction(QString::fromUtf8("⚙ Settings"));
  connect(settingsAction, &QAction::triggered, this,
          &ATMWatchWindow::onSettingsClicked);

  QAction *refreshAction = m_toolbar->addAction(QString::fromUtf8("🔄 Refresh"));
  connect(refreshAction, &QAction::triggered, this,
          &ATMWatchWindow::refreshData);

  // Install event filters on table viewports for wheel-sync
  m_callTable->viewport()->installEventFilter(this);
  m_symbolTable->viewport()->installEventFilter(this);
  m_putTable->viewport()->installEventFilter(this);

  // Enable sorting on Symbol table header
  m_symbolTable->horizontalHeader()->setSectionsClickable(true);
  m_symbolTable->horizontalHeader()->setSortIndicatorShown(true);
  connect(m_symbolTable->horizontalHeader(), &QHeaderView::sectionClicked, this,
          &ATMWatchWindow::onHeaderClicked);

  // Enable sorting on Call/Put table headers (for IV column sorting)
  m_callTable->horizontalHeader()->setSectionsClickable(true);
  m_callTable->horizontalHeader()->setSortIndicatorShown(true);
  connect(m_callTable->horizontalHeader(), &QHeaderView::sectionClicked, this,
          &ATMWatchWindow::onCallHeaderClicked);

  m_putTable->horizontalHeader()->setSectionsClickable(true);
  m_putTable->horizontalHeader()->setSortIndicatorShown(true);
  connect(m_putTable->horizontalHeader(), &QHeaderView::sectionClicked, this,
          &ATMWatchWindow::onPutHeaderClicked);

  // Keyboard-first: focus policies + tab order
  m_exchangeCombo->setFocusPolicy(Qt::StrongFocus);
  m_expiryCombo->setFocusPolicy(Qt::StrongFocus);
  m_callTable->setFocusPolicy(Qt::StrongFocus);
  m_symbolTable->setFocusPolicy(Qt::StrongFocus);
  m_putTable->setFocusPolicy(Qt::StrongFocus);
  // Tab: Exchange → Expiry → Symbol table → Call table → Put table → (wrap)
  QWidget::setTabOrder(m_exchangeCombo, m_expiryCombo);
  QWidget::setTabOrder(m_expiryCombo,   m_symbolTable);
  QWidget::setTabOrder(m_symbolTable,   m_callTable);
  QWidget::setTabOrder(m_callTable,     m_putTable);
  QWidget::setTabOrder(m_putTable,      m_exchangeCombo);
}

void ATMWatchWindow::setupModels() {
  m_callModel = new QStandardItemModel(this);
  m_callModel->setColumnCount(CALL_COUNT);
  m_callModel->setHorizontalHeaderLabels({"Chg", "Vol", "OI", "IV", "Delta",
                                          "Gamma", "Vega", "Theta", "LTP",
                                          "Bid", "Ask"});
  m_callTable->setModel(m_callModel);
  m_callTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_callTable->horizontalHeader()->setStretchLastSection(false);
  m_callTable->horizontalHeader()->setSectionsMovable(true);

  m_callDelegate = new ATMWatchDelegate(false, this);
  m_callTable->setItemDelegate(m_callDelegate);

  m_symbolModel = new QStandardItemModel(this);
  m_symbolModel->setColumnCount(SYM_COUNT);
  m_symbolModel->setHorizontalHeaderLabels(
      {"Symbol", "Spot/Fut", "ATM", "Expiry"});
  m_symbolTable->setModel(m_symbolModel);
  m_symbolTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_symbolTable->horizontalHeader()->setStretchLastSection(false);
  m_symbolTable->horizontalHeader()->setSectionsMovable(true);

  m_symbolDelegate = new ATMWatchDelegate(true, this);
  m_symbolTable->setItemDelegate(m_symbolDelegate);

  m_putModel = new QStandardItemModel(this);
  m_putModel->setColumnCount(PUT_COUNT);
  m_putModel->setHorizontalHeaderLabels({"LTP", "Bid", "Ask", "Chg", "Vol",
                                         "OI", "IV", "Delta", "Gamma", "Vega",
                                         "Theta"});
  m_putTable->setModel(m_putModel);
  m_putTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_putTable->horizontalHeader()->setStretchLastSection(false);
  m_putTable->horizontalHeader()->setSectionsMovable(true);

  m_putDelegate = new ATMWatchDelegate(false, this);
  m_putTable->setItemDelegate(m_putDelegate);
}

void ATMWatchWindow::setupConnections() {
  connect(&ATMWatchManager::getInstance(), &ATMWatchManager::atmUpdated, this,
          &ATMWatchWindow::onATMUpdated);

  // Greeks updates
  connect(
      &GreeksCalculationService::instance(),
      &GreeksCalculationService::greeksCalculated, this,
      [this](uint32_t token, int exchangeSegment, const GreeksResult &result) {
        if (!m_tokenToInfo.contains(token))
          return;
        auto info = m_tokenToInfo[token]; // {symbol, isCall}
        int row = m_symbolToRow.value(info.first, -1);
        if (row < 0)
          return;

        if (info.second) { // Call
          m_callModel->setData(
              m_callModel->index(row, CALL_IV),
              QString::number(result.impliedVolatility * 100.0, 'f', 2));
          m_callModel->setData(m_callModel->index(row, CALL_DELTA),
                               QString::number(result.delta, 'f', 2));
          m_callModel->setData(m_callModel->index(row, CALL_GAMMA),
                               QString::number(result.gamma, 'f', 4));
          m_callModel->setData(m_callModel->index(row, CALL_VEGA),
                               QString::number(result.vega, 'f', 2));
          m_callModel->setData(m_callModel->index(row, CALL_THETA),
                               QString::number(result.theta, 'f', 2));
        } else { // Put
          m_putModel->setData(
              m_putModel->index(row, PUT_IV),
              QString::number(result.impliedVolatility * 100.0, 'f', 2));
          m_putModel->setData(m_putModel->index(row, PUT_DELTA),
                              QString::number(result.delta, 'f', 2));
          m_putModel->setData(m_putModel->index(row, PUT_GAMMA),
                              QString::number(result.gamma, 'f', 4));
          m_putModel->setData(m_putModel->index(row, PUT_VEGA),
                              QString::number(result.vega, 'f', 2));
          m_putModel->setData(m_putModel->index(row, PUT_THETA),
                              QString::number(result.theta, 'f', 2));
        }
      });

  // ATM calculation error handling
  connect(&ATMWatchManager::getInstance(), &ATMWatchManager::calculationFailed,
          this, [this](const QString &symbol, const QString &errorMessage) {
            int row = m_symbolToRow.value(symbol, -1);
            if (row >= 0) {
              m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                                     "ERROR");
              m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                                     errorMessage, Qt::ToolTipRole);
              m_symbolModel->setData(m_symbolModel->index(row, SYM_ATM),
                                     QColor(Qt::red), Qt::ForegroundRole);
            }
          });

  // Filter connections
  connect(m_exchangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ATMWatchWindow::onExchangeChanged);
  connect(m_expiryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ATMWatchWindow::onExpiryChanged);

  // Context menu and double-click for symbol table
  m_symbolTable->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_symbolTable, &QTableView::customContextMenuRequested, this,
          &ATMWatchWindow::onShowContextMenu);
  // Unified: same context menu from all three table headers
  m_symbolTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_symbolTable->horizontalHeader(), &QHeaderView::customContextMenuRequested, this,
          [this](const QPoint &headerPos) {
              QPoint viewportPos = m_symbolTable->viewport()->mapFromGlobal(
                  m_symbolTable->horizontalHeader()->mapToGlobal(headerPos));
              onShowContextMenu(viewportPos);
          });
  m_callTable->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_callTable, &QTableView::customContextMenuRequested, this,
          &ATMWatchWindow::onShowContextMenu);
  m_callTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_callTable->horizontalHeader(), &QHeaderView::customContextMenuRequested, this,
          [this](const QPoint &headerPos) {
              QPoint viewportPos = m_symbolTable->viewport()->mapFromGlobal(
                  m_callTable->horizontalHeader()->mapToGlobal(headerPos));
              onShowContextMenu(viewportPos);
          });
  m_putTable->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_putTable, &QTableView::customContextMenuRequested, this,
          &ATMWatchWindow::onShowContextMenu);
  m_putTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_putTable->horizontalHeader(), &QHeaderView::customContextMenuRequested, this,
          [this](const QPoint &headerPos) {
              QPoint viewportPos = m_symbolTable->viewport()->mapFromGlobal(
                  m_putTable->horizontalHeader()->mapToGlobal(headerPos));
              onShowContextMenu(viewportPos);
          });
  connect(m_symbolTable, &QTableView::doubleClicked, this,
          &ATMWatchWindow::onSymbolDoubleClicked);

  // Async loading completion
  connect(this, &ATMWatchWindow::onSymbolsLoaded, this, [this](int count) {
    m_statusLabel->setText(QString("Loaded %1 symbols").arg(count));
  });

  // Tri-directional scroll sync (all three tables as equals)
  connect(m_symbolTable->verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this](int value) {
            if (m_syncingScroll) return;
            m_syncingScroll = true;
            m_callTable->verticalScrollBar()->setValue(value);
            m_putTable->verticalScrollBar()->setValue(value);
            m_syncingScroll = false;
          });
  connect(m_callTable->verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this](int value) {
            if (m_syncingScroll) return;
            m_syncingScroll = true;
            m_symbolTable->verticalScrollBar()->setValue(value);
            m_putTable->verticalScrollBar()->setValue(value);
            m_syncingScroll = false;
          });
  connect(m_putTable->verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this](int value) {
            if (m_syncingScroll) return;
            m_syncingScroll = true;
            m_symbolTable->verticalScrollBar()->setValue(value);
            m_callTable->verticalScrollBar()->setValue(value);
            m_syncingScroll = false;
          });

  // Initialize
  m_currentExchange = "NSE";
  populateCommonExpiries(m_currentExchange);

  // Exclusive Selection Logic
  connect(m_callTable->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, [this](const QItemSelection &selected, const QItemSelection &) {
            if (!selected.isEmpty()) {
              m_putTable->clearSelection();
              m_symbolTable->clearSelection();
            }
          });

  connect(m_putTable->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, [this](const QItemSelection &selected, const QItemSelection &) {
            if (!selected.isEmpty()) {
              m_callTable->clearSelection();
              m_symbolTable->clearSelection();
            }
          });

  connect(m_symbolTable->selectionModel(),
          &QItemSelectionModel::selectionChanged, this,
          [this](const QItemSelection &selected, const QItemSelection &) {
            if (!selected.isEmpty()) {
              m_callTable->clearSelection();
              m_putTable->clearSelection();
            }
          });
}

void ATMWatchWindow::setupShortcuts() {
  // Helper lambdas
  auto getActiveTable = [this]() -> QTableView * {
    if (m_callTable->hasFocus())   return m_callTable;
    if (m_symbolTable->hasFocus()) return m_symbolTable;
    if (m_putTable->hasFocus())    return m_putTable;
    return nullptr;
  };
  auto getRow = [](QTableView *t) -> int {
    if (!t) return 0;
    int r = t->currentIndex().row();
    return (r >= 0) ? r : 0;
  };
  auto focusTable = [](QTableView *t, int row) {
    t->setFocus();
    if (t->model() && row >= 0 && row < t->model()->rowCount())
      t->selectRow(row);
  };

  // Ctrl+Right: Call → Symbol → Put → (wraps to Call)
  auto *nextSC = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right), this);
  nextSC->setContext(Qt::WidgetWithChildrenShortcut);
  connect(nextSC, &QShortcut::activated, this, [this, getActiveTable, getRow, focusTable]() {
    QTableView *cur = getActiveTable();
    int row = getRow(cur);
    if      (cur == m_callTable)   focusTable(m_symbolTable, row);
    else if (cur == m_symbolTable) focusTable(m_putTable,    row);
    else                           focusTable(m_callTable,   row);
  });

  // Ctrl+Left: Put → Symbol → Call → (wraps to Put)
  auto *prevSC = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left), this);
  prevSC->setContext(Qt::WidgetWithChildrenShortcut);
  connect(prevSC, &QShortcut::activated, this, [this, getActiveTable, getRow, focusTable]() {
    QTableView *cur = getActiveTable();
    int row = getRow(cur);
    if      (cur == m_putTable)    focusTable(m_symbolTable, row);
    else if (cur == m_symbolTable) focusTable(m_callTable,   row);
    else                           focusTable(m_putTable,    row);
  });

  // Ctrl+R: Refresh
  auto *refreshSC = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this);
  refreshSC->setContext(Qt::WidgetWithChildrenShortcut);
  connect(refreshSC, &QShortcut::activated, this, &ATMWatchWindow::refreshData);

  // Ctrl+E: Focus Exchange combo + open dropdown
  auto *exchangeSC = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_E), this);
  exchangeSC->setContext(Qt::WidgetWithChildrenShortcut);
  connect(exchangeSC, &QShortcut::activated, this, [this]() {
    m_exchangeCombo->setFocus();
    m_exchangeCombo->showPopup();
  });

  // Alt+X: Focus Expiry combo + open dropdown
  auto *expirySC = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_X), this);
  expirySC->setContext(Qt::WidgetWithChildrenShortcut);
  connect(expirySC, &QShortcut::activated, this, [this]() {
    m_expiryCombo->setFocus();
    m_expiryCombo->showPopup();
  });

  // Ctrl+G: Jump to first (ATM) row
  auto *gotoSC = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);
  gotoSC->setContext(Qt::WidgetWithChildrenShortcut);
  connect(gotoSC, &QShortcut::activated, this, [this]() {
    if (m_symbolModel->rowCount() <= 0) return;
    m_symbolTable->setFocus();
    m_symbolTable->scrollToTop();
    m_symbolTable->selectRow(0);
    m_callTable->selectRow(0);
    m_putTable->selectRow(0);
    m_statusLabel->setText("Jumped to ATM row");
  });
}

void ATMWatchWindow::onSettingsClicked() {
  ATMWatchSettingsDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    qInfo() << "[ATMWatch] Settings updated, will take effect on next ATM "
               "calculation";
  }
}

void ATMWatchWindow::onShowContextMenu(const QPoint &pos) {
  QModelIndex index = m_symbolTable->indexAt(pos);

  QMenu contextMenu(this);
  contextMenu.setStyleSheet(
      "QMenu { background-color: #ffffff; color: #1e293b; border: 1px solid "
      "#e2e8f0; border-radius: 4px; }"
      "QMenu::item { padding: 6px 16px; }"
      "QMenu::item:selected { background-color: #bfdbfe; color: #1e40af; }"
      "QMenu::item:checked { font-weight: bold; }"
      "QMenu::separator { height: 1px; background: #e2e8f0; margin: 4px 0; }");

  // ── Row-specific actions (only when clicking a valid row) ──
  if (index.isValid()) {
    int row = index.row();
    QString symbol =
        m_symbolModel->data(m_symbolModel->index(row, SYM_NAME)).toString();

    QAction *openChainAction = contextMenu.addAction("📊 Open Option Chain");
    QAction *recalculateAction = contextMenu.addAction("🔄 Recalculate ATM");
    contextMenu.addSeparator();
    QAction *copySymbolAction = contextMenu.addAction("📋 Copy Symbol");
    contextMenu.addSeparator();

    connect(openChainAction, &QAction::triggered, this, [this, row]() {
      QString sym = m_symbolModel->data(m_symbolModel->index(row, SYM_NAME)).toString();
      QString exp = m_symbolModel->data(m_symbolModel->index(row, SYM_EXPIRY)).toString();
      openOptionChain(sym, exp);
    });
    connect(recalculateAction, &QAction::triggered, this, [this]() {
      ATMWatchManager::getInstance().calculateAll();
      m_statusLabel->setText("Recalculating ATM...");
    });
    connect(copySymbolAction, &QAction::triggered, this, [this, symbol]() {
      QApplication::clipboard()->setText(symbol);
      m_statusLabel->setText(QString("Copied: %1").arg(symbol));
    });
  }

  // ── Column profile dialogs (GenericProfileDialog per table) ──
  contextMenu.addSeparator();
  contextMenu.addAction("📋 Call Column Profile...", this, &ATMWatchWindow::showCallColumnDialog);
  contextMenu.addAction("📋 Symbol Column Profile...", this, &ATMWatchWindow::showSymbolColumnDialog);
  contextMenu.addAction("📋 Put Column Profile...", this, &ATMWatchWindow::showPutColumnDialog);
  contextMenu.addAction("💾 Save All Column Profiles", this, [this]() {
    saveAllColumnProfiles();
    m_statusLabel->setText("Column profiles saved");
  });

  contextMenu.exec(m_symbolTable->viewport()->mapToGlobal(pos));
}


