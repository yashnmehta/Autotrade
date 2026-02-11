#include "views/IndicesView.h"
#include "repository/RepositoryManager.h"
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QHeaderView>
#include <QMenu>
#include <QMoveEvent>
#include <QScreen>
#include <QSettings>
#include <QVBoxLayout>

// ========== IndicesModel Implementation ==========

QVariant IndicesModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_indices.size())
    return QVariant();

  const IndexData &data = m_indices[index.row()];

  if (role == Qt::DisplayRole) {
    switch (index.column()) {
    case COL_NAME:
      return data.name;
    case COL_LTP: {
      QString chgPrefix = (data.change >= 0) ? "+" : "";
      return QString("%1 (%2%3)")
          .arg(data.ltp, 0, 'f', 2)
          .arg(chgPrefix)
          .arg(data.change, 0, 'f', 2);
    }
    case COL_PERCENT:
      return (data.percentChange >= 0) ? QString("▲") : QString("▼");
    }
  } else if (role == Qt::ForegroundRole) {
    if (index.column() == COL_NAME) {
      return QColor("#000000");
    } else if (index.column() == COL_LTP || index.column() == COL_PERCENT) {
      return (data.percentChange >= 0) ? QColor("#00aa00") : QColor("#ff0000");
    }
  } else if (role == Qt::TextAlignmentRole) {
    switch (index.column()) {
    case COL_NAME:
      return int(Qt::AlignLeft | Qt::AlignVCenter);
    case COL_LTP:
      return int(Qt::AlignRight | Qt::AlignVCenter);
    case COL_PERCENT:
      return int(Qt::AlignCenter | Qt::AlignVCenter);
    }
  }

  return QVariant();
}

QVariant IndicesModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch (section) {
    case COL_NAME:
      return "Index";
    case COL_LTP:
      return "LTP";
    case COL_CHANGE:
      return "Chg";
    case COL_PERCENT:
      return "%";
    }
  }
  return QVariant();
}

void IndicesModel::updateIndex(const QString &name, double ltp, double change,
                               double percentChange) {
  // Find existing row or add new one
  int row = -1;
  if (m_nameToRow.contains(name)) {
    row = m_nameToRow[name];
    // Update existing data
    m_indices[row] = IndexData(name, ltp, change, percentChange);
    // ✅ PERFORMANCE: Only emit dataChanged for this row
    emit dataChanged(index(row, 0), index(row, COL_COUNT - 1));
  } else {
    // Add new row
    row = m_indices.size();
    beginInsertRows(QModelIndex(), row, row);
    m_indices.append(IndexData(name, ltp, change, percentChange));
    m_nameToRow[name] = row;
    endInsertRows();
  }
}

void IndicesModel::clear() {
  beginResetModel();
  m_indices.clear();
  m_nameToRow.clear();
  endResetModel();
}

// ========== IndicesView Implementation ==========

IndicesView::IndicesView(QWidget *parent)
    : QWidget(parent), m_view(new QTableView(this)),
      m_model(new IndicesModel(this)), m_updateTimer(new QTimer(this)) {
  setupUI();

  // Setup throttling timer - max 10 updates per second (100ms interval)
#ifdef Q_OS_MACOS
  // macOS optimization: Use coarse timer and longer interval
  m_updateTimer->setInterval(250); // 4 updates/sec (reduced for macOS)
  m_updateTimer->setTimerType(Qt::CoarseTimer);
#else
  m_updateTimer->setInterval(100);
#endif
  m_updateTimer->setSingleShot(false);
  connect(m_updateTimer, &QTimer::timeout, this,
          &IndicesView::processPendingUpdates);
  m_updateTimer->start();

  // ✅ Load saved position or set default to top-right corner
  loadPosition();

  // Test data removed - wait for real UDP updates
}

IndicesView::~IndicesView() {}

void IndicesView::setupUI() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Set model to view
  m_view->setModel(m_model);

  // Hide vertical header
  m_view->verticalHeader()->setVisible(false);

  // Hide horizontal header
  m_view->horizontalHeader()->setVisible(false);

  // Style
  m_view->setShowGrid(false);
  m_view->setAlternatingRowColors(false); // Disable for clean dark look
  m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_view->setSelectionMode(QAbstractItemView::SingleSelection);
  m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);

  setStyleSheet(
      "background-color: #1e1e1e; color: #cccccc; border: 1px solid #3e3e42;");
  m_view->setStyleSheet("QTableView { "
                        "   background-color: #1e1e1e; "
                        "   gridline-color: #333333; "
                        "   color: #e0e0e0; "
                        "   border: none; "
                        "} "
                        "QTableView::item:selected { "
                        "   background-color: #094771; "
                        "   color: white; "
                        "} "
                        "QHeaderView::section { "
                        "   background-color: #252526; "
                        "   color: #cccccc; "
                        "   border: 1px solid #3e3e42; "
                        "}");

  // Column sizing
  QHeaderView *header = m_view->horizontalHeader();
  header->setSectionResizeMode(IndicesModel::COL_NAME, QHeaderView::Stretch);
  header->setSectionResizeMode(IndicesModel::COL_LTP,
                               QHeaderView::ResizeToContents);
  header->setSectionResizeMode(IndicesModel::COL_CHANGE,
                               QHeaderView::ResizeToContents);
  header->setSectionResizeMode(IndicesModel::COL_PERCENT,
                               QHeaderView::ResizeToContents);

  // View styling
  m_view->setStyleSheet(
      "QTableView { background-color: #ffffff; color: #000000; border: none; "
      "font-weight: bold; font-family: 'Inter', sans-serif; }"
      "QTableView::item { padding: 4px; border-bottom: 1px solid #eeeeee; }"
      "QTableView::item:selected { background-color: #e5f3ff; color: #000000; "
      "}");

  // Enable custom context menu
  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_view, &QTableView::customContextMenuRequested, this,
          &IndicesView::showContextMenu);

  layout->addWidget(m_view);
}

void IndicesView::showContextMenu(const QPoint &pos) {
  QMenu contextMenu(tr("Context Menu"), this);
  QAction *removeAction = contextMenu.addAction("Remove Index");

  // Check if an item is selected
  QModelIndex index = m_view->indexAt(pos);
  if (!index.isValid()) {
    removeAction->setEnabled(false);
  }

  connect(removeAction, &QAction::triggered, this, [this, index]() {
    if (index.isValid()) {
      // Get the name from the model (column 0)
      QString name =
          m_model
              ->data(m_model->index(index.row(), IndicesModel::COL_NAME),
                     Qt::DisplayRole)
              .toString();
      removeIndex(name);
    }
  });

  contextMenu.exec(m_view->mapToGlobal(pos));
}

void IndicesView::removeIndex(const QString &name) {
  if (name.isEmpty())
    return;

  // 1. Remove from selected list
  m_selectedIndices.remove(name);

  // 2. Persist to settings
  QStringList list = m_selectedIndices.toList();
  QSettings settings("TradingCompany", "TradingTerminal");
  settings.setValue("indices/selected", list);

  // 3. Reload view
  reloadSelectedIndices(list);
}

void IndicesView::updateIndex(const QString &name, double ltp, double change,
                              double percentChange) {
  // ✅ Queue update instead of immediate model update
  IndexData data(name, ltp, change, percentChange);
  m_pendingUpdates[name] = data;
}

void IndicesView::processPendingUpdates() {
  if (m_pendingUpdates.isEmpty())
    return;

  for (auto it = m_pendingUpdates.begin(); it != m_pendingUpdates.end(); ++it) {
    const IndexData &data = it.value();
    m_model->updateIndex(data.name, data.ltp, data.change, data.percentChange);
  }
  m_pendingUpdates.clear();
}

void IndicesView::onIndexReceived(const UDP::IndexTick &tick) {
  // ✅ ULTRA-FAST PATH 1: Use token-based lookup (O(1))
  if (tick.token > 0 && m_tokenToName.contains(tick.token)) {
    const QString &name = m_tokenToName[tick.token];
    updateIndex(name, tick.value, tick.change, tick.changePercent);
    return;
  }

  // ✅ ROBUST PATH 2: Use name-based lookup (O(1) with normalization cache)
  // This is critical for NSE where tokens are often 0 in IndexTick records
  QString rawName = QString::fromLatin1(tick.name).trimmed();
  if (!rawName.isEmpty()) {
    QString upperRaw = rawName.toUpper();
    if (m_upperToDisplayName.contains(upperRaw)) {
      const QString &displayName = m_upperToDisplayName[upperRaw];
      updateIndex(displayName, tick.value, tick.change, tick.changePercent);
      return;
    }
  }

  // Fallback for SENSEX/BSE basics if not in token map but logically mapped
  if (tick.exchangeSegment == UDP::ExchangeSegment::BSECM ||
      tick.exchangeSegment == UDP::ExchangeSegment::BSEFO) {
    QString name;
    if (tick.token == 1)
      name = "SENSEX";
    else if (tick.token == 2)
      name = "BSE 100";

    if (!name.isEmpty() && m_selectedIndices.contains(name)) {
      updateIndex(name, tick.value, tick.change, tick.changePercent);
    }
  }
}

void IndicesView::clear() {
  m_model->clear();
  m_pendingUpdates.clear();
}

void IndicesView::initialize(RepositoryManager *repoManager) {
  if (!repoManager)
    return;
  m_repoManager = repoManager;

  QSettings settings("TradingCompany", "TradingTerminal");
  QStringList selected = settings.value("indices/selected").toStringList();

  if (selected.isEmpty()) {
    selected << "Nifty 50" << "Nifty Bank" << "SENSEX" << "Nifty IT"
             << "Nifty Next 50" << "Nifty 500" << "Nifty Midcap 50"
             << "Nifty200 Alpha 30" << "India VIX";
  }

  reloadSelectedIndices(selected);
}

void IndicesView::reloadSelectedIndices(const QStringList &selectedIndices) {
  if (!m_repoManager)
    return;

  m_selectedIndices =
      QSet<QString>(selectedIndices.begin(), selectedIndices.end());
  m_tokenToName.clear();
  m_upperToDisplayName.clear();
  m_model->clear();
  m_pendingUpdates.clear();

  const auto &indexMap = m_repoManager->getIndexNameTokenMap();

  // Populate maps for fast UDP lookup
  for (const QString &indexName : m_selectedIndices) {
    // 1. Add to upper-case name map for robust string matching
    m_upperToDisplayName[indexName.toUpper()] = indexName;

    // 2. Exact match in repository
    if (indexMap.contains(indexName)) {
      uint32_t token = static_cast<uint32_t>(indexMap[indexName]);
      m_tokenToName[token] = indexName;
      updateIndex(indexName, 0.0, 0.0, 0.0);
    }
    // 3. Case-insensitive fallback logic for repository mapping
    else {
      bool mapped = false;
      for (auto it = indexMap.constBegin(); it != indexMap.constEnd(); ++it) {
        if (it.key().compare(indexName, Qt::CaseInsensitive) == 0) {
          uint32_t token = static_cast<uint32_t>(it.value());
          m_tokenToName[token] = indexName; // Map token -> display name
          updateIndex(indexName, 0.0, 0.0, 0.0);
          mapped = true;
          break;
        }
      }

      // If still not mapped to a token, we might rely purely on name in
      // onIndexReceived
      if (!mapped) {
        // Pre-populate so it shows up in UI even if no price yet
        updateIndex(indexName, 0.0, 0.0, 0.0);
      }
    }
  }

  qDebug() << "[IndicesView] Reloaded:" << m_selectedIndices.size()
           << "selected," << m_tokenToName.size() << "token-mapped,"
           << m_upperToDisplayName.size() << "name-mapped";
}

void IndicesView::loadPosition() {
  QSettings settings("TradingCompany", "TradingTerminal");

  // Check if we have a saved position
  if (settings.contains("indicesview/pos_x") &&
      settings.contains("indicesview/pos_y")) {
    int x = settings.value("indicesview/pos_x").toInt();
    int y = settings.value("indicesview/pos_y").toInt();

    // ✅ Validate position is on-screen
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
      QRect screenGeometry = screen->availableGeometry();

      // Make sure window is within screen bounds
      if (x < screenGeometry.x())
        x = screenGeometry.x();
      if (y < screenGeometry.y())
        y = screenGeometry.y();
      if (x + width() > screenGeometry.right())
        x = screenGeometry.right() - width();
      if (y + height() > screenGeometry.bottom())
        y = screenGeometry.bottom() - height();

      move(x, y);
      qDebug() << "[IndicesView] Restored position:" << QPoint(x, y);
      return;
    }
  }

  // ✅ No saved position - default to TOP-RIGHT corner
  if (parentWidget()) {
    // Position relative to parent (MainWindow)
    QWidget *parent = parentWidget();
    int x = parent->x() + parent->width() - width() -
            20;               // 20px margin from right edge
    int y = parent->y() + 50; // 50px from top

    // Make sure it's on-screen
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
      QRect screenGeometry = screen->availableGeometry();
      if (x + width() > screenGeometry.right()) {
        x = screenGeometry.right() - width() - 20;
      }
      if (y < screenGeometry.y()) {
        y = screenGeometry.y() + 50;
      }
    }

    move(x, y);
    qDebug() << "[IndicesView] Default position (top-right):" << QPoint(x, y);
  }
}

void IndicesView::savePosition() {
  QSettings settings("TradingCompany", "TradingTerminal");
  settings.setValue("indicesview/pos_x", x());
  settings.setValue("indicesview/pos_y", y());
  qDebug() << "[IndicesView] Saved position:" << pos();
}

void IndicesView::closeEvent(QCloseEvent *event) {
  // ✅ Save position when window is closed
  savePosition();

  // ✅ When user closes with X button:
  // - Hide the window temporarily
  // - Uncheck the menu action
  // - But DON'T save preference as false (keep it true for next launch)
  emit hideRequested();

  // Just hide, don't actually close
  hide();
  event->ignore();
}

void IndicesView::moveEvent(QMoveEvent *event) {
  // ✅ Save position whenever window is moved
  QWidget::moveEvent(event);

  // Debounce saves - only save if position has stabilized
  static QTimer *saveTimer = nullptr;
  if (!saveTimer) {
    saveTimer = new QTimer(this);
    saveTimer->setSingleShot(true);
    saveTimer->setInterval(500); // Save 500ms after user stops dragging
    connect(saveTimer, &QTimer::timeout, this, &IndicesView::savePosition);
  }

  saveTimer->start();
}
