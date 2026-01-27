#include "views/IndicesView.h"
#include "repository/RepositoryManager.h"
#include <QDebug>
#include <QHeaderView>
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
  m_updateTimer->setInterval(100);
  m_updateTimer->setSingleShot(false);
  connect(m_updateTimer, &QTimer::timeout, this,
          &IndicesView::processPendingUpdates);
  m_updateTimer->start();

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
  m_view->setAlternatingRowColors(true);
  m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_view->setSelectionMode(QAbstractItemView::SingleSelection);
  m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);

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

  layout->addWidget(m_view);
}

void IndicesView::updateIndex(const QString &name, double ltp, double change,
                              double percentChange) {
  // ✅ PERFORMANCE FIX: Queue update instead of immediate model update
  IndexData data;
  data.name = name;
  data.ltp = ltp;
  data.change = change;
  data.percentChange = percentChange;

  m_pendingUpdates[name] = data; // Overwrites previous pending update
}

void IndicesView::processPendingUpdates() {
  if (m_pendingUpdates.isEmpty()) {
    return;
  }

  // ✅ PERFORMANCE: Batch all model updates together
  // Model/View architecture handles efficient repainting automatically
  for (auto it = m_pendingUpdates.begin(); it != m_pendingUpdates.end(); ++it) {
    const IndexData &data = it.value();
    m_model->updateIndex(data.name, data.ltp, data.change, data.percentChange);
  }

  m_pendingUpdates.clear();
}

void IndicesView::onIndexReceived(const UDP::IndexTick &tick) {
  QString name = QString::fromLatin1(tick.name).trimmed();

  // For BSE, name might be empty, so map from token
  if (tick.exchangeSegment == UDP::ExchangeSegment::BSECM ||
      tick.exchangeSegment == UDP::ExchangeSegment::BSEFO) {
    if (tick.token == 1)
      name = "SENSEX";
    else if (tick.token == 2)
      name = "BSE 100";
    else
      return;
  }

  // For NSE, standardize names (case-insensitive matching)
  QString upperName = name.toUpper();

  // NSE: Normalize to standard names (exact match only to avoid matching "Nifty
  // 50 Arbitrage", etc.)
  if (tick.exchangeSegment == UDP::ExchangeSegment::NSECM) {
    // Only match exact "NIFTY 50" or "NIFTY", not "NIFTY 50 Arbitrage"
    if ((upperName == "NIFTY 50" || upperName == "NIFTY" ||
         upperName == "NIFTY50") &&
        !upperName.contains("ARBITRAGE") && !upperName.contains("FUTURES") &&
        !upperName.contains("ALPHA")) {
      name = "NIFTY 50";
    } else if (upperName.contains("NIFTY BANK") ||
               upperName.contains("BANKNIFTY") ||
               upperName.contains("BANK NIFTY")) {
      name = "BANKNIFTY";
    }
    // Keep other indices with their original names (Nifty200 Alpha 30, etc.)
  }
  // BSE: Prevent Nifty/BankNifty conflict by prefixing
  else if (tick.exchangeSegment == UDP::ExchangeSegment::BSECM ||
           tick.exchangeSegment == UDP::ExchangeSegment::BSEFO) {
    if (upperName == "NIFTY 50" || upperName == "NIFTY" ||
        upperName.contains("NIFTY 50")) {
      name = "BSE NIFTY 50";
    } else if (upperName == "BANKNIFTY" || upperName.contains("BANKEX")) {
      if (upperName == "BANKNIFTY")
        name = "BSE BANKNIFTY";
    }

    // Allow SENSEX to merge (it's the primary index)
    if (upperName.contains("SENSEX")) {
      name = "SENSEX";
    }
  }

  // Debug output
  // qDebug() << "[IndicesView] Received tick:" << name << "Val:" << tick.value;

  // Update (will be queued and batched)
  if (tick.exchangeSegment == UDP::ExchangeSegment::NSECM) {
    if (name.contains("NIFTY 50", Qt::CaseInsensitive)) {
      qDebug() << "[IndicesView] NSECM Update:" << name
               << "Input:" << QString::fromLatin1(tick.name)
               << "Val:" << tick.value;
    }
    updateIndex(name, tick.value, tick.change, tick.changePercent);
  } else if (name == "NIFTY 50" || name == "BANKNIFTY" || name == "SENSEX") {
    updateIndex(name, tick.value, tick.change, tick.changePercent);
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

  qDebug() << "[IndicesView] Initializing with repository data...";

  // Load selected indices from settings
  QSettings settings("TradingCompany", "TradingTerminal");
  m_selectedIndices = settings.value("indices/selected").toStringList();

  // If no saved selection, use defaults
  if (m_selectedIndices.isEmpty()) {
    m_selectedIndices << "Nifty 50" << "Nifty Bank" << "SENSEX" << "Nifty IT"
                      << "Nifty Next 50" << "Nifty 500" << "Nifty Midcap 50"
                      << "Nifty200 Alpha 30" << "India VIX";
  }

  reloadSelectedIndices(m_selectedIndices);
}

void IndicesView::reloadSelectedIndices(const QStringList &selectedIndices) {
  if (!m_repoManager)
    return;

  m_selectedIndices = selectedIndices;

  // Clear existing data
  m_model->clear();
  m_pendingUpdates.clear();

  // Get the index name to token map
  const auto &indexMap = m_repoManager->getIndexNameTokenMap();

  qDebug() << "[IndicesView] Loading" << m_selectedIndices.size()
           << "selected indices";

  // Pre-populate with selected index names (values will be updated by UDP)
  int addedCount = 0;
  for (const QString &indexName : m_selectedIndices) {
    if (indexMap.contains(indexName)) {
      // Add with zero values - will be updated by UDP
      updateIndex(indexName, 0.0, 0.0, 0.0);
      addedCount++;
    }
  }

  qDebug() << "[IndicesView] Pre-populated" << addedCount << "indices";
}
