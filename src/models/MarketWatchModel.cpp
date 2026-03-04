#include "models/qt/MarketWatchModel.h"
#include "models/profiles/MarketWatchColumnProfile.h"
#include "services/GreeksCalculationService.h"
#include "utils/LatencyTracker.h"
#include <QColor>
#include <QDebug>
#include <QFont>
#include <QSize>


MarketWatchModel::MarketWatchModel(QObject *parent)
    : QAbstractTableModel(parent) {
  // Initialize with default column profile
  m_columnProfile = MarketWatchColumnProfile::createDefaultProfile();

  // Connect to Greeks service for live updates
  auto &greeksService = GreeksCalculationService::instance();
  connect(
      &greeksService, &GreeksCalculationService::greeksCalculated, this,
      [this](uint32_t token, int exchangeSegment, const GreeksResult &result) {
        // Find if we have this scrip in our model
        // Note: findScripByToken is linear, effectively fast for typical
        // watchlist sizes (50-100) For very large lists, we might need a
        // token->row map
        int row = findScripByToken(token);
        if (row >= 0) {
          updateGreeks(row, result.impliedVolatility, result.bidIV,
                       result.askIV, result.delta, result.gamma, result.vega,
                       result.theta);
        }
      });
}

int MarketWatchModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : m_scrips.count();
}

int MarketWatchModel::columnCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : m_columnProfile.visibleColumnCount();
}

QVariant MarketWatchModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_scrips.count())
    return QVariant();

  const ScripData &scrip = m_scrips.at(index.row());

  // Blank rows: just empty data rows (no special visual styling)
  // They have isBlankRow=true and token=-1, so they won't be subscribed/updated,
  // but visually they look identical to regular rows (just show empty cells).
  if (scrip.isBlankRow) {
    if (role == Qt::DisplayRole) {
      return QString(); // Empty text for all columns
    }
    // No special background, no special size — looks like a regular row
    return QVariant();
  }

  // Get the column enum for this index
  QList<int> visibleCols = m_columnProfile.visibleColumns();
  if (index.column() < 0 || index.column() >= visibleCols.size())
    return QVariant();

  MarketWatchColumn column =
      static_cast<MarketWatchColumn>(visibleCols.at(index.column()));

  // Display role - show formatted data
  if (role == Qt::DisplayRole) {
    return formatColumnData(scrip, column);
  }

  // Text alignment
  else if (role == Qt::TextAlignmentRole) {
    ColumnInfo info = MarketWatchColumnProfile::getColumnInfo(column);
    return static_cast<int>(info.alignment);
  }

  // User role - return raw data for sorting
  else if (role == Qt::UserRole) {
    return getColumnData(scrip, column);
  }

  // User role + 1 - return token for lookups
  else if (role == Qt::UserRole + 1) {
    return (qlonglong)scrip.token;
  }

  // User role + 2 - return exchange
  else if (role == Qt::UserRole + 2) {
    return scrip.exchange;
  }

  // Background coloring for value changes (Ticks)
  else if (role == Qt::BackgroundRole) {
    if (column == MarketWatchColumn::LAST_TRADED_PRICE ||
        column == MarketWatchColumn::BUY_PRICE ||
        column == MarketWatchColumn::SELL_PRICE) {
      int tick = 0;
      if (column == MarketWatchColumn::LAST_TRADED_PRICE)
        tick = scrip.ltpTick;
      else if (column == MarketWatchColumn::BUY_PRICE)
        tick = scrip.bidTick;
      else if (column == MarketWatchColumn::SELL_PRICE)
        tick = scrip.askTick;

      if (tick > 0)
        return QColor("#dbeafe"); // Light blue for UP tick (light theme)
      if (tick < 0)
        return QColor("#fee2e2"); // Light red for DOWN tick (light theme)
    }
  }

  return QVariant();
}

QVariant MarketWatchModel::headerData(int section, Qt::Orientation orientation,
                                      int role) const {
  if (orientation == Qt::Horizontal) {
    QList<int> visibleCols = m_columnProfile.visibleColumns();
    if (section < 0 || section >= visibleCols.size())
      return QVariant();

    MarketWatchColumn col =
        static_cast<MarketWatchColumn>(visibleCols.at(section));
    ColumnInfo info = MarketWatchColumnProfile::getColumnInfo(col);

    if (role == Qt::DisplayRole) {
      return info.name;
    } else if (role == Qt::TextAlignmentRole) {
      return static_cast<int>(info.alignment);
    } else if (role == Qt::ToolTipRole) {
      return info.description;
    }
  } else if (orientation == Qt::Vertical && role == Qt::DisplayRole) {
    return section + 1; // Row numbers
  }

  return QVariant();
}

void MarketWatchModel::addScrip(const ScripData &scrip) {
  // Reject duplicates: same exchange + token already exists (blank rows exempt)
  if (!scrip.isBlankRow && scrip.token > 0) {
    for (const ScripData &existing : m_scrips) {
      if (!existing.isBlankRow && existing.token == scrip.token &&
          existing.exchange == scrip.exchange) {
        qDebug() << "[MarketWatchModel] Skipping duplicate scrip:"
                 << scrip.symbol << "Exchange:" << scrip.exchange
                 << "Token:" << scrip.token;
        return;
      }
    }
  }

  int row = m_scrips.count();
  beginInsertRows(QModelIndex(), row, row);
  m_scrips.append(scrip);
  endInsertRows();

  // Maintain token→row map
  if (!scrip.isBlankRow && scrip.token > 0) {
    m_tokenToRow[scrip.token] = row;
  }

  emit scripAdded(row, scrip);

  qDebug() << "[MarketWatchModel] Added scrip:" << scrip.symbol
           << "Token:" << scrip.token << "at row" << row;
}

void MarketWatchModel::addScrips(const QList<ScripData> &scrips) {
  if (scrips.isEmpty())
    return;

  QList<ScripData> toAdd;
  toAdd.reserve(scrips.size());

  for (const auto &scrip : scrips) {
    if (!scrip.isBlankRow && scrip.token > 0) {
      bool isDup = false;
      for (const ScripData &existing : m_scrips) {
        if (!existing.isBlankRow && existing.token == scrip.token &&
            existing.exchange == scrip.exchange) {
          isDup = true;
          break;
        }
      }
      if (!isDup)
        toAdd.append(scrip);
    } else {
      toAdd.append(scrip);
    }
  }

  if (toAdd.isEmpty())
    return;

  int firstRow = m_scrips.count();
  int lastRow = firstRow + toAdd.count() - 1;

  beginInsertRows(QModelIndex(), firstRow, lastRow);
  m_scrips.append(toAdd);
  endInsertRows();

  for (int i = 0; i < toAdd.count(); ++i) {
    int row = firstRow + i;
    const auto &scrip = toAdd[i];
    if (!scrip.isBlankRow && scrip.token > 0) {
      m_tokenToRow[scrip.token] = row;
    }
    emit scripAdded(row, scrip);
  }
  qDebug() << "[MarketWatchModel] Bulk added" << toAdd.count() << "scrips";
}

void MarketWatchModel::insertScrip(int position, const ScripData &scrip) {
  // Reject duplicates: same exchange + token already exists (blank rows exempt)
  if (!scrip.isBlankRow && scrip.token > 0) {
    for (const ScripData &existing : m_scrips) {
      if (!existing.isBlankRow && existing.token == scrip.token &&
          existing.exchange == scrip.exchange) {
        qDebug() << "[MarketWatchModel] Skipping duplicate scrip (insert):"
                 << scrip.symbol << "Exchange:" << scrip.exchange
                 << "Token:" << scrip.token;
        return;
      }
    }
  }

  // Validate position
  if (position < 0)
    position = 0;
  if (position > m_scrips.count())
    position = m_scrips.count();

  beginInsertRows(QModelIndex(), position, position);
  m_scrips.insert(position, scrip);
  endInsertRows();

  // Re-index token→row map for all rows from position onwards
  for (int i = position; i < m_scrips.count(); ++i) {
    if (!m_scrips[i].isBlankRow && m_scrips[i].token > 0) {
      m_tokenToRow[m_scrips[i].token] = i;
    }
  }

  emit scripAdded(position, scrip);

  qDebug() << "[MarketWatchModel] Inserted scrip:" << scrip.symbol
           << "Token:" << scrip.token << "at position" << position;
}

void MarketWatchModel::removeScrip(int row) {
  if (row >= 0 && row < m_scrips.count()) {
    beginRemoveRows(QModelIndex(), row, row);

    const ScripData &scrip = m_scrips.at(row);
    qDebug() << "[MarketWatchModel] Removing scrip:" << scrip.symbol
             << "Token:" << scrip.token << "from row" << row;

    // Remove from token→row map
    if (!scrip.isBlankRow && scrip.token > 0) {
      m_tokenToRow.remove(scrip.token);
    }

    m_scrips.removeAt(row);
    endRemoveRows();

    // Re-index all rows after the removed row
    for (int i = row; i < m_scrips.count(); ++i) {
      if (!m_scrips[i].isBlankRow && m_scrips[i].token > 0) {
        m_tokenToRow[m_scrips[i].token] = i;
      }
    }

    emit scripRemoved(row);
  }
}

void MarketWatchModel::moveRow(int sourceRow, int targetRow) {
  if (sourceRow < 0 || sourceRow >= m_scrips.count())
    return;
  if (targetRow < 0 || targetRow > m_scrips.count())
    return;
  if (sourceRow == targetRow)
    return;

  // Qt's moveRows expects destination to be the position AFTER the move
  // If moving down, targetRow is already correct
  // If moving up, targetRow should be the position before the item
  int destRow = targetRow;
  if (sourceRow < targetRow) {
    destRow = targetRow;
  }

  // Use Qt's beginMoveRows to notify views
  if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow, QModelIndex(),
                     destRow)) {
    qDebug() << "[MarketWatchModel] moveRow failed: invalid move from"
             << sourceRow << "to" << targetRow;
    return;
  }

  // Perform the actual move
  ScripData scrip = m_scrips.takeAt(sourceRow);
  int insertPos = targetRow;
  if (sourceRow < targetRow) {
    insertPos = targetRow - 1;
  }
  m_scrips.insert(insertPos, scrip);

  endMoveRows();

  // Re-index affected range in token→row map
  int lo = std::min(sourceRow, insertPos);
  int hi = std::max(sourceRow, insertPos);
  for (int i = lo; i <= hi; ++i) {
    if (!m_scrips[i].isBlankRow && m_scrips[i].token > 0) {
      m_tokenToRow[m_scrips[i].token] = i;
    }
  }

  qDebug() << "[MarketWatchModel] Moved row from" << sourceRow << "to"
           << insertPos;
}

void MarketWatchModel::clearAll() {
  beginResetModel();
  m_scrips.clear();
  m_tokenToRow.clear();
  endResetModel();

  qDebug() << "[MarketWatchModel] Cleared all scrips";
}

int MarketWatchModel::findScrip(const QString &symbol) const {
  for (int i = 0; i < m_scrips.count(); ++i) {
    if (m_scrips.at(i).symbol == symbol && !m_scrips.at(i).isBlankRow)
      return i;
  }
  return -1;
}

int MarketWatchModel::findScripByToken(int token) const {
  if (token <= 0)
    return -1;

  auto it = m_tokenToRow.find(token);
  if (it != m_tokenToRow.end()) {
    int row = it.value();
    // Validate the cached row is still correct
    if (row >= 0 && row < m_scrips.count() && m_scrips.at(row).token == token &&
        !m_scrips.at(row).isBlankRow) {
      return row;
    }
  }

  // Fallback: linear scan (should not normally happen)
  for (int i = 0; i < m_scrips.count(); ++i) {
    if (m_scrips.at(i).token == token && !m_scrips.at(i).isBlankRow)
      return i;
  }
  return -1;
}

void MarketWatchModel::insertBlankRow(int position) {
  if (position < 0 || position > m_scrips.count()) {
    position = m_scrips.count(); // Append at end
  }

  beginInsertRows(QModelIndex(), position, position);
  m_scrips.insert(position, ScripData::createBlankRow());
  endInsertRows();

  // Re-index rows after the inserted blank row
  for (int i = position + 1; i < m_scrips.count(); ++i) {
    if (!m_scrips[i].isBlankRow && m_scrips[i].token > 0) {
      m_tokenToRow[m_scrips[i].token] = i;
    }
  }

  qDebug() << "[MarketWatchModel] Inserted blank row at position" << position;
}

bool MarketWatchModel::isBlankRow(int row) const {
  if (row >= 0 && row < m_scrips.count()) {
    return m_scrips.at(row).isBlankRow;
  }
  return false;
}

const ScripData &MarketWatchModel::getScripAt(int row) const {
  static const ScripData emptyData;
  if (row >= 0 && row < m_scrips.count()) {
    return m_scrips.at(row);
  }
  return emptyData;
}

ScripData &MarketWatchModel::getScripAt(int row) {
  static ScripData emptyData;
  if (row >= 0 && row < m_scrips.count()) {
    return m_scrips[row];
  }
  return emptyData;
}

void MarketWatchModel::updatePrice(int row, double ltp, double change,
                                   double changePercent) {
  if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
    ScripData &scrip = m_scrips[row];

    // Determine tick direction for LTP
    if (ltp > scrip.ltp && scrip.ltp > 0)
      scrip.ltpTick = 1;
    else if (ltp < scrip.ltp && scrip.ltp > 0)
      scrip.ltpTick = -1;

    scrip.ltp = ltp;
    scrip.change = change;
    scrip.changePercent = changePercent;

    // Notify entire row to support dynamic column profiles correctly
    notifyRowUpdated(row, 0, columnCount() - 1);
  }
}

void MarketWatchModel::updateVolume(int row, qint64 volume) {
  if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
    m_scrips[row].volume = volume;
    notifyRowUpdated(row, 0, columnCount() - 1);
  }
}

void MarketWatchModel::updateBidAsk(int row, double bid, double ask) {
  if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
    ScripData &scrip = m_scrips[row];

    // Determine tick direction for Bid
    if (bid > scrip.bid && scrip.bid > 0)
      scrip.bidTick = 1;
    else if (bid < scrip.bid && scrip.bid > 0)
      scrip.bidTick = -1;

    // Determine tick direction for Ask
    if (ask > scrip.ask && scrip.ask > 0)
      scrip.askTick = 1;
    else if (ask < scrip.ask && scrip.ask > 0)
      scrip.askTick = -1;

    scrip.bid = bid;
    scrip.ask = ask;
    scrip.buyPrice = bid;  // Buy Price = Bid
    scrip.sellPrice = ask; // Sell Price = Ask

    notifyRowUpdated(row, 0, columnCount() - 1);
  }
}

void MarketWatchModel::updateLastTradedQuantity(int row, qint64 ltq) {
  if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
    m_scrips[row].ltq = ltq;
    notifyRowUpdated(row, 0, columnCount() - 1);
  }
}

void MarketWatchModel::updateHighLow(int row, double high, double low) {
  if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
    m_scrips[row].high = high;
    m_scrips[row].low = low;
    notifyRowUpdated(row, 0, columnCount() - 1);
  }
}

void MarketWatchModel::updateOpenInterest(int row, qint64 oi) {
  if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
    m_scrips[row].openInterest = oi;
    // Notify entire row to support dynamic column profiles correctly
    notifyRowUpdated(row, 0, columnCount() - 1);
  }
}

void MarketWatchModel::updateAveragePrice(int row, double avgPrice) {
  if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
    m_scrips[row].avgTradedPrice = avgPrice;
    notifyRowUpdated(row, 0, columnCount() - 1);
  }
}

void MarketWatchModel::updateOHLC(int row, double open, double high, double low,
                                  double close) {
  if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
    ScripData &scrip = m_scrips[row];
    scrip.open = open;
    scrip.high = high;
    scrip.low = low;
    scrip.close = close;

    notifyRowUpdated(row, 0, columnCount() - 1);
  }
}

void MarketWatchModel::updateBidAskQuantities(int row, int bidQty, int askQty) {
  if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
    m_scrips[row].buyQty = bidQty;
    m_scrips[row].sellQty = askQty;

    notifyRowUpdated(row, 0, columnCount() - 1);
  }
}

void MarketWatchModel::updateTotalBuySellQty(int row, int totalBuyQty,
                                             int totalSellQty) {
  if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
    m_scrips[row].totalBuyQty = totalBuyQty;
    m_scrips[row].totalSellQty = totalSellQty;

    notifyRowUpdated(row, 0, columnCount() - 1);
  }
}

void MarketWatchModel::updateOpenInterestWithChange(int row, qint64 oi,
                                                    double oiChangePercent) {
  if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
    m_scrips[row].openInterest = oi;
    m_scrips[row].oiChangePercent = oiChangePercent;

    // Notify OI columns
    notifyRowUpdated(row, COL_OPEN_INTEREST, COL_OPEN_INTEREST);
  }
}

void MarketWatchModel::updateGreeks(int row, double iv, double bidIV,
                                    double askIV, double delta, double gamma,
                                    double vega, double theta) {
  if (row >= 0 && row < m_scrips.count() && !m_scrips.at(row).isBlankRow) {
    ScripData &scrip = m_scrips[row];
    scrip.iv = iv;
    scrip.bidIV = bidIV;
    scrip.askIV = askIV;
    scrip.delta = delta;
    scrip.gamma = gamma;
    scrip.vega = vega;
    scrip.theta = theta;

    // Notify entire row for Greeks columns
    notifyRowUpdated(row, 0, columnCount() - 1);
  }
}

void MarketWatchModel::updateScripData(int row, const ScripData &scrip) {
  if (row >= 0 && row < m_scrips.count()) {
    m_scrips[row] = scrip;
    notifyRowUpdated(row, 0, columnCount() - 1);
  }
}

void MarketWatchModel::updateFromUdpTick(int row, const UDP::MarketTick &tick,
                                         double change, double changePercent) {
  if (row < 0 || row >= m_scrips.count() || m_scrips.at(row).isBlankRow)
    return;

  ScripData &s = m_scrips[row];

  // Tick direction for LTP
  if (tick.ltp > 0) {
    if (tick.ltp > s.ltp && s.ltp > 0)
      s.ltpTick = 1;
    else if (tick.ltp < s.ltp && s.ltp > 0)
      s.ltpTick = -1;

    s.ltp = tick.ltp;
    s.change = change;
    s.changePercent = changePercent;
  }

  // OHLC (only update if values present)
  if (tick.open > 0)
    s.open = tick.open;
  if (tick.high > 0)
    s.high = tick.high;
  if (tick.low > 0)
    s.low = tick.low;
  if (tick.prevClose > 0)
    s.close = tick.prevClose;

  // LTQ
  if (tick.ltq > 0)
    s.ltq = tick.ltq;

  // Average traded price
  if (tick.atp > 0)
    s.avgTradedPrice = tick.atp;

  // Volume
  if (tick.volume > 0)
    s.volume = tick.volume;

  // Bid/Ask with tick direction
  if (tick.bids[0].price > 0 || tick.asks[0].price > 0) {
    double newBid = tick.bids[0].price;
    double newAsk = tick.asks[0].price;

    if (newBid > 0) {
      if (newBid > s.bid && s.bid > 0)
        s.bidTick = 1;
      else if (newBid < s.bid && s.bid > 0)
        s.bidTick = -1;
      s.bid = newBid;
      s.buyPrice = newBid;
    }

    if (newAsk > 0) {
      if (newAsk > s.ask && s.ask > 0)
        s.askTick = 1;
      else if (newAsk < s.ask && s.ask > 0)
        s.askTick = -1;
      s.ask = newAsk;
      s.sellPrice = newAsk;
    }

    s.buyQty = tick.bids[0].quantity;
    s.sellQty = tick.asks[0].quantity;
  }

  // Total buy/sell quantities
  if (tick.totalBidQty > 0 || tick.totalAskQty > 0) {
    s.totalBuyQty = tick.totalBidQty;
    s.totalSellQty = tick.totalAskQty;
  }

  // Open Interest (derivatives only)
  if (tick.openInterest > 0) {
    s.openInterest = tick.openInterest;
    if (tick.oiChange != 0) {
      s.oiChangePercent =
          (static_cast<double>(tick.oiChange) / tick.openInterest) * 100.0;
    }
  }

  // ONE dataChanged signal for the entire row (was 9 before!)
  notifyRowUpdated(row, 0, columnCount() - 1);
}

int MarketWatchModel::scripCount() const {
  int count = 0;
  for (const ScripData &scrip : m_scrips) {
    if (!scrip.isBlankRow) {
      ++count;
    }
  }
  return count;
}

void MarketWatchModel::emitCellChanged(int row, int column) {
  QModelIndex idx = index(row, column);
  emit dataChanged(idx, idx);
}

// ============================================================================
// Native Callback Helper (Ultra-Low Latency Mode)
// ============================================================================

void MarketWatchModel::notifyRowUpdated(int row, int firstColumn,
                                        int lastColumn) {
  // ALWAYS emit dataChanged. Proxy models and the view's internal cache
  // rely on this signal to know when to refresh.
  emit dataChanged(index(row, firstColumn), index(row, lastColumn));

  if (m_viewCallback) {
    // Optional native C++ callback for even faster viewport invalidation
    m_viewCallback->onRowUpdated(row, firstColumn, lastColumn);
  }
}

// ============================================================================
// Column Profile Management
// ============================================================================

void MarketWatchModel::setColumnProfile(const GenericTableProfile &profile) {
  beginResetModel();
  m_columnProfile = profile;
  endResetModel();
  qDebug() << "[MarketWatchModel] Column profile updated:" << profile.name();
}

void MarketWatchModel::loadProfile(const QString &profileName) {
  // Use preset factories — no global singleton manager.
  // A caller that needs custom profiles can use GenericProfileManager directly.
  if (profileName == "Default") {
    setColumnProfile(MarketWatchColumnProfile::createDefaultProfile());
    return;
  }
  if (profileName == "Compact") {
    setColumnProfile(MarketWatchColumnProfile::createCompactProfile());
    return;
  }
  if (profileName == "Detailed") {
    setColumnProfile(MarketWatchColumnProfile::createDetailedProfile());
    return;
  }
  if (profileName == "F&O") {
    setColumnProfile(MarketWatchColumnProfile::createFOProfile());
    return;
  }
  if (profileName == "Equity") {
    setColumnProfile(MarketWatchColumnProfile::createEquityProfile());
    return;
  }
  if (profileName == "Trading") {
    setColumnProfile(MarketWatchColumnProfile::createTradingProfile());
    return;
  }
  qDebug() << "[MarketWatchModel] Profile not found:" << profileName;
}

void MarketWatchModel::saveProfile(const QString &profileName) {
  // Profile persistence is handled by GenericProfileManager at the window
  // level. Model just holds the current profile in memory.
  GenericTableProfile profile = m_columnProfile;
  profile.setName(profileName);
  qDebug() << "[MarketWatchModel] Profile save requested (handled by window):"
           << profileName;
}

QStringList MarketWatchModel::getAvailableProfiles() const {
  return QStringList{"Default", "Compact", "Detailed",
                     "F&O",     "Equity",  "Trading"};
}

// ============================================================================
// Column Data Helpers
// ============================================================================

QVariant MarketWatchModel::getColumnData(const ScripData &scrip,
                                         MarketWatchColumn column) const {
  switch (column) {
  case MarketWatchColumn::CODE:
    return scrip.code;
  case MarketWatchColumn::SYMBOL:
    return scrip.symbol;
  case MarketWatchColumn::SCRIP_NAME:
    return scrip.scripName;
  case MarketWatchColumn::INSTRUMENT_NAME:
    return scrip.instrumentName;
  case MarketWatchColumn::INSTRUMENT_TYPE:
    return scrip.instrumentType;
  case MarketWatchColumn::MARKET_TYPE:
    return scrip.marketType;
  case MarketWatchColumn::EXCHANGE:
    return scrip.exchange;
  case MarketWatchColumn::STRIKE_PRICE:
    return scrip.strikePrice;
  case MarketWatchColumn::OPTION_TYPE:
    return scrip.optionType;
  case MarketWatchColumn::SERIES_EXPIRY:
    return scrip.seriesExpiry;
  case MarketWatchColumn::ISIN_CODE:
    return scrip.isinCode;
  case MarketWatchColumn::LAST_TRADED_PRICE:
    return scrip.ltp;
  case MarketWatchColumn::LAST_TRADED_QUANTITY:
    return static_cast<qlonglong>(scrip.ltq);
  case MarketWatchColumn::LAST_TRADED_TIME:
    return scrip.ltpTime;
  case MarketWatchColumn::LAST_UPDATE_TIME:
    return scrip.lastUpdateTime;
  case MarketWatchColumn::OPEN:
    return scrip.open;
  case MarketWatchColumn::HIGH:
    return scrip.high;
  case MarketWatchColumn::LOW:
    return scrip.low;
  case MarketWatchColumn::CLOSE:
    return scrip.close;
  case MarketWatchColumn::DPR:
    return scrip.dpr;
  case MarketWatchColumn::NET_CHANGE_RS:
    return scrip.change;
  case MarketWatchColumn::PERCENT_CHANGE:
    return scrip.changePercent;
  case MarketWatchColumn::TREND_INDICATOR:
    return scrip.trendIndicator;
  case MarketWatchColumn::AVG_TRADED_PRICE:
    return scrip.avgTradedPrice;
  case MarketWatchColumn::VOLUME:
    return static_cast<qlonglong>(scrip.volume);
  case MarketWatchColumn::VALUE:
    return scrip.value;
  case MarketWatchColumn::BUY_PRICE:
    return scrip.buyPrice;
  case MarketWatchColumn::BUY_QTY:
    return static_cast<qlonglong>(scrip.buyQty);
  case MarketWatchColumn::TOTAL_BUY_QTY:
    return static_cast<qlonglong>(scrip.totalBuyQty);
  case MarketWatchColumn::SELL_PRICE:
    return scrip.sellPrice;
  case MarketWatchColumn::SELL_QTY:
    return static_cast<qlonglong>(scrip.sellQty);
  case MarketWatchColumn::TOTAL_SELL_QTY:
    return static_cast<qlonglong>(scrip.totalSellQty);
  case MarketWatchColumn::OPEN_INTEREST:
    return static_cast<qlonglong>(scrip.openInterest);
  case MarketWatchColumn::OI_CHANGE_PERCENT:
    return scrip.oiChangePercent;
  case MarketWatchColumn::IMPLIED_VOLATILITY:
    return scrip.iv * 100.0; // Convert to percentage
  case MarketWatchColumn::BID_IV:
    return scrip.bidIV * 100.0;
  case MarketWatchColumn::ASK_IV:
    return scrip.askIV * 100.0;
  case MarketWatchColumn::DELTA:
    return scrip.delta;
  case MarketWatchColumn::GAMMA:
    return scrip.gamma;
  case MarketWatchColumn::VEGA:
    return scrip.vega;
  case MarketWatchColumn::THETA:
    return scrip.theta;
  case MarketWatchColumn::WEEK_52_HIGH:
    return scrip.week52High;
  case MarketWatchColumn::WEEK_52_LOW:
    return scrip.week52Low;
  case MarketWatchColumn::LIFETIME_HIGH:
    return scrip.lifetimeHigh;
  case MarketWatchColumn::LIFETIME_LOW:
    return scrip.lifetimeLow;
  case MarketWatchColumn::MARKET_CAP:
    return scrip.marketCap;
  case MarketWatchColumn::TRADE_EXECUTION_RANGE:
    return scrip.tradeExecutionRange;
  default:
    return QVariant();
  }
}

QString MarketWatchModel::formatColumnData(const ScripData &scrip,
                                           MarketWatchColumn column) const {
  ColumnInfo info = MarketWatchColumnProfile::getColumnInfo(column);
  QString format = info.format;
  QString units = info.unit;

  switch (column) {
  case MarketWatchColumn::CODE:
    return QString::number(scrip.code);

  case MarketWatchColumn::SYMBOL:
  case MarketWatchColumn::SCRIP_NAME:
  case MarketWatchColumn::INSTRUMENT_NAME:
  case MarketWatchColumn::INSTRUMENT_TYPE:
  case MarketWatchColumn::MARKET_TYPE:
  case MarketWatchColumn::EXCHANGE:
  case MarketWatchColumn::OPTION_TYPE:
  case MarketWatchColumn::ISIN_CODE:
  case MarketWatchColumn::DPR:
  case MarketWatchColumn::TREND_INDICATOR:
  case MarketWatchColumn::TRADE_EXECUTION_RANGE: {
    QString val = getColumnData(scrip, column).toString();
    if (column == MarketWatchColumn::INSTRUMENT_TYPE ||
        column == MarketWatchColumn::OPTION_TYPE) {
      // qDebug() << "[MarketWatchModel] formatting string col" << (int)column
      // << "for" << scrip.symbol << "val:" << val;
    }
    return val;
  }

  case MarketWatchColumn::SERIES_EXPIRY: {
    // For F&O instruments, show expiry date
    // For equity, show series (EQ, BE, etc.)
    QString seriesExpiry = getColumnData(scrip, column).toString();
    if (seriesExpiry.isEmpty() || seriesExpiry == "N/A")
      return "-";

    // Detect F&O instruments:
    // 1. NSE: instrumentType contains "FUT" or "OPT"
    // 2. BSE: exchange is "BSEFO" or series is "IF", "IO", "IS" (Index/Stock
    // Futures/Options)
    // 3. Generic: Has strike price or option type set
    bool isFO = scrip.instrumentType.contains("FUT") ||
                scrip.instrumentType.contains("OPT") ||
                scrip.exchange == "BSEFO" || scrip.exchange == "NSEFO" ||
                scrip.strikePrice > 0 || !scrip.optionType.isEmpty();

    if (isFO) {
      return seriesExpiry; // Show full expiry for F&O (e.g., "29JAN2026")
    } else {
      // For equity, extract series part only (before any date)
      return seriesExpiry.left(2); // "EQ", "BE", etc.
    }
  }

  case MarketWatchColumn::STRIKE_PRICE:
  case MarketWatchColumn::LAST_TRADED_PRICE:
  case MarketWatchColumn::OPEN:
  case MarketWatchColumn::HIGH:
  case MarketWatchColumn::LOW:
  case MarketWatchColumn::CLOSE:
  case MarketWatchColumn::NET_CHANGE_RS:
  case MarketWatchColumn::AVG_TRADED_PRICE:
  case MarketWatchColumn::BUY_PRICE:
  case MarketWatchColumn::SELL_PRICE:
  case MarketWatchColumn::WEEK_52_HIGH:
  case MarketWatchColumn::WEEK_52_LOW:
  case MarketWatchColumn::LIFETIME_HIGH:
  case MarketWatchColumn::LIFETIME_LOW: {
    double value = getColumnData(scrip, column).toDouble();
    if (value == 0.0)
      return "-";
    return QString::number(value, 'f', 2) +
           (units.isEmpty() ? "" : " " + units);
  }

  case MarketWatchColumn::PERCENT_CHANGE:
  case MarketWatchColumn::OI_CHANGE_PERCENT: {
    double value = getColumnData(scrip, column).toDouble();
    if (value == 0.0)
      return "-";
    QString sign = (value > 0) ? "+" : "";
    return sign + QString::number(value, 'f', 2) + "%";
  }

  case MarketWatchColumn::VOLUME: {
    qint64 vol = getColumnData(scrip, column).toLongLong();
    if (vol == 0)
      return "-";
    return QString::number(vol / 1000.0, 'f', 2) + " K"; // in 000s
  }

  case MarketWatchColumn::VALUE: {
    double val = getColumnData(scrip, column).toDouble();
    if (val == 0.0)
      return "-";
    return QString::number(val / 100000.0, 'f', 2) + " L"; // in lacs
  }

  case MarketWatchColumn::MARKET_CAP: {
    double cap = getColumnData(scrip, column).toDouble();
    if (cap == 0.0)
      return "-";
    if (cap >= 10000000.0) {
      return QString::number(cap / 10000000.0, 'f', 2) + " Cr";
    }
    return QString::number(cap / 100000.0, 'f', 2) + " L";
  }

  case MarketWatchColumn::LAST_TRADED_QUANTITY:
  case MarketWatchColumn::BUY_QTY:
  case MarketWatchColumn::SELL_QTY:
  case MarketWatchColumn::TOTAL_BUY_QTY:
  case MarketWatchColumn::TOTAL_SELL_QTY:
  case MarketWatchColumn::OPEN_INTEREST: {
    qint64 value = getColumnData(scrip, column).toLongLong();
    if (value == 0)
      return "-";
    return QString::number(value) + (units.isEmpty() ? "" : " " + units);
  }

  case MarketWatchColumn::LAST_TRADED_TIME:
  case MarketWatchColumn::LAST_UPDATE_TIME:
    return getColumnData(scrip, column).toString();

  // Greeks columns
  case MarketWatchColumn::IMPLIED_VOLATILITY:
  case MarketWatchColumn::BID_IV:
  case MarketWatchColumn::ASK_IV: {
    double value = getColumnData(scrip, column).toDouble();
    if (value <= 0.0)
      return "-";
    return QString::number(value, 'f', 1) + "%";
  }

  case MarketWatchColumn::DELTA: {
    if (scrip.iv <= 0.0)
      return "-"; // No Greeks if IV not calculated
    return QString::number(scrip.delta, 'f', 3);
  }

  case MarketWatchColumn::GAMMA: {
    if (scrip.iv <= 0.0)
      return "-";
    return QString::number(scrip.gamma, 'f', 5);
  }

  case MarketWatchColumn::VEGA: {
    if (scrip.iv <= 0.0)
      return "-";
    return QString::number(scrip.vega, 'f', 2);
  }

  case MarketWatchColumn::THETA: {
    if (scrip.iv <= 0.0)
      return "-";
    return QString::number(scrip.theta, 'f', 2);
  }

  default:
    return "-";
  }
}
