#include "views/PositionWindow.h"
#include "core/widgets/CustomNetPosition.h"
#include "repository/RepositoryManager.h"
#include "services/TradingDataService.h"
#include <QVBoxLayout>


#include "models/PinnedRowProxyModel.h"
#include <QComboBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QVector>


#include "data/PriceStoreGateway.h"
#include <QMutexLocker>
#include <QTimer>


PositionWindow::PositionWindow(TradingDataService *tradingDataService,
                               QWidget *parent)
    : BaseBookWindow("PositionBook", parent),
      m_tradingDataService(tradingDataService), m_isUpdating(false) {
  setupUI();
  loadInitialProfile();

  if (m_tradingDataService) {
    // Connect to real-time position updates from socket events
    connect(m_tradingDataService, &TradingDataService::positionsUpdated, this,
            &PositionWindow::onPositionsUpdated);
    // Load initial positions
    onPositionsUpdated(m_tradingDataService->getPositions());
  }
  if (m_filterShortcut) {
    connect(m_filterShortcut, &QShortcut::activated, this,
            [this]() { this->toggleFilterRow(); });
  }

  // Initialize 500ms refresh timer for market prices
  m_priceUpdateTimer = new QTimer(this);
  connect(m_priceUpdateTimer, &QTimer::timeout, this,
          &PositionWindow::updateMarketPrices);
  m_priceUpdateTimer->start(500);
}

PositionWindow::~PositionWindow() {}

void PositionWindow::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  mainLayout->addWidget(createFilterWidget());
  setupTable();
  mainLayout->addWidget(m_tableView, 1);
}

QWidget *PositionWindow::createFilterWidget() {
  QWidget *container = new QWidget(this);
  container->setObjectName("filterContainer");
  container->setStyleSheet(
      "QWidget#filterContainer { background-color: #2d2d2d; border-bottom: 1px "
      "solid #3f3f46; } QLabel { color: #d4d4d8; font-size: 11px; } QComboBox "
      "{ background-color: #3f3f46; color: #ffffff; border: 1px solid #52525b; "
      "border-radius: 3px; font-size: 11px; } QPushButton { border-radius: "
      "3px; font-weight: 600; font-size: 11px; padding: 5px 12px; }");
  QHBoxLayout *layout = new QHBoxLayout(container);
  layout->setContentsMargins(12, 10, 12, 10);

  auto addCombo = [&](const QString &l, QComboBox *&c, const QStringList &i) {
    QVBoxLayout *v = new QVBoxLayout();
    v->addWidget(new QLabel(l));
    c = new QComboBox();
    c->addItems(i);
    v->addWidget(c);
    layout->addLayout(v);
    connect(c, &QComboBox::currentTextChanged, this,
            &PositionWindow::applyFilters);
  };
  addCombo("Exchange", m_cbExchange, {"All", "NSE", "BSE", "MCX"});
  addCombo("Segment", m_cbSegment, {"All", "Cash", "FO", "CD", "COM"});
  addCombo("Product", m_cbPeriodicity, {"All", "MIS", "NRML", "CNC"});
  addCombo("User", m_cbUser, {"All"});
  addCombo("Client", m_cbClient, {"All"});

  layout->addStretch();
  m_btnRefresh = new QPushButton("Refresh");
  m_btnRefresh->setStyleSheet("background-color: #16a34a; color: white;");
  m_btnExport = new QPushButton("Export");
  m_btnExport->setStyleSheet("background-color: #d97706; color: white;");
  layout->addWidget(m_btnRefresh);
  layout->addWidget(m_btnExport);

  connect(m_btnRefresh, &QPushButton::clicked, this,
          &PositionWindow::onRefreshClicked);
  connect(m_btnExport, &QPushButton::clicked, this,
          &PositionWindow::onExportClicked);
  return container;
}

void PositionWindow::setupTable() {
  m_tableView = new CustomNetPosition(this);
  CustomNetPosition *posTable = qobject_cast<CustomNetPosition *>(m_tableView);
  if (posTable) {
    connect(posTable, &CustomNetPosition::exportRequested, this,
            &PositionWindow::onExportClicked);
    connect(posTable, &CustomNetPosition::closePositionRequested, this,
            &PositionWindow::onSquareOffClicked);
    connect(posTable, &CustomNetPosition::filterToggleRequested, this,
            &PositionWindow::toggleFilterRow);
  }

  PositionModel *model = new PositionModel(this);
  m_model = model;
  m_model = model;
  m_proxyModel = new PinnedRowProxyModel(this);
  m_proxyModel->setSourceModel(m_model);
  m_tableView->setModel(m_proxyModel);
}

void PositionWindow::onPositionsUpdated(
    const QVector<XTS::Position> &positions) {
  QMutexLocker locker(&m_updateMutex);

  m_allPositions.clear();
  qDebug() << "[PositionWindow] onPositionsUpdated: Received"
           << positions.size() << "positions";
  for (const auto &p : positions) {
    PositionData pd;
    pd.scripCode = p.exchangeInstrumentID;

    // --- 1. Basic Fields ---
    bool isNumeric;
    int segId = p.exchangeSegment.toInt(&isNumeric);

    // Normalize Exchange Name (NSEFO -> NSE, NSECM -> NSE)
    QString rawExchange;
    if (isNumeric) {
      rawExchange = RepositoryManager::getExchangeSegmentName(segId);
    } else {
      rawExchange = p.exchangeSegment;
    }

    pd.exchange = rawExchange.left(3); // "NSE", "BSE", "MCX"
    // Store precise segment for internal logic if needed
    QString segmentSuffix = rawExchange.mid(3); // "CM", "FO"

    pd.productType = p.productType;
    pd.client = p.accountID;
    pd.user = p.loginID;

    // --- 2. Quantities ---
    pd.netQty = p.quantity;
    pd.buyQty = p.openBuyQuantity;
    pd.sellQty = p.openSellQuantity;

    // --- 3. Prices ---
    pd.buyAvg = p.buyAveragePrice;
    pd.sellAvg = p.sellAveragePrice;
    if (pd.netQty != 0) {
      pd.netPrice = std::abs(p.netAmount / pd.netQty);
    } else {
      pd.netPrice = 0.0;
    }

    // --- 4. Values ---
    pd.buyVal = p.buyAmount;
    pd.sellVal = p.sellAmount;
    pd.netVal = p.netAmount;
    // Total Value: Use absolute turnover
    pd.totalValue = std::abs(p.buyAmount) + std::abs(p.sellAmount);

    // --- 5. P&L ---
    pd.mtm = p.mtm;
    pd.actualMtm = p.realizedMTM + p.unrealizedMTM;

    // --- 6. Contract Master Lookup ---
    auto repo = RepositoryManager::getInstance();
    const ContractData *contract = nullptr;

    // Use the raw segment name/ID for lookup
    int lookupId = 0;
    if (isNumeric) {
      lookupId = segId;
    } else {
      lookupId =
          RepositoryManager::getExchangeSegmentID(pd.exchange, segmentSuffix);
    }

    // Fallback: If lookupId is invalid or suffix was empty, try to guess
    if (lookupId <= 0) {
      if (pd.exchange == "NSE") {
        // Try FO first for NIFTY/BANKNIFTY symbols usually
        contract = repo->getContractByToken(
            RepositoryManager::getExchangeSegmentID("NSE", "FO"), pd.scripCode);
        if (!contract) {
          contract = repo->getContractByToken(
              RepositoryManager::getExchangeSegmentID("NSE", "CM"),
              pd.scripCode);
        }
      } else if (pd.exchange == "BSE") {
        contract = repo->getContractByToken(
            RepositoryManager::getExchangeSegmentID("BSE", "FO"), pd.scripCode);
        if (!contract)
          contract = repo->getContractByToken(
              RepositoryManager::getExchangeSegmentID("BSE", "CM"),
              pd.scripCode);
      } else if (pd.exchange == "MCX") {
        contract = repo->getContractByToken(
            RepositoryManager::getExchangeSegmentID("MCX", "FO"), pd.scripCode);
      }
    } else {
      contract = repo->getContractByToken(lookupId, pd.scripCode);

      // Double Check: If lookup failed but we have a code, maybe the segment
      // was wrong?
      if (!contract && pd.scripCode > 0) {
        if (pd.exchange == "NSE") {
          // If we tried one, try the other
          int altId =
              (segmentSuffix == "FO")
                  ? RepositoryManager::getExchangeSegmentID("NSE", "CM")
                  : RepositoryManager::getExchangeSegmentID("NSE", "FO");
          if (altId > 0)
            contract = repo->getContractByToken(altId, pd.scripCode);
        }
      }
    }

    if (contract) {
      qDebug() << "[PositionWindow] Contract found for" << p.tradingSymbol
               << ":" << contract->name;
      pd.symbol = contract->name;
      pd.scripName = contract->description;
      pd.instrumentName = contract->displayName;
      pd.seriesExpiry = contract->expiryDate;
      if (pd.seriesExpiry.isEmpty())
        pd.seriesExpiry = contract->series;
      pd.strikePrice = contract->strikePrice;
      pd.optionType = contract->optionType;

      if (contract->instrumentType == 1)
        pd.instrumentType = "FUT";
      else if (contract->instrumentType == 2)
        pd.instrumentType = "OPT";
      else if (contract->instrumentType == 4)
        pd.instrumentType = "SPD";
      else
        pd.instrumentType = "EQ";
    } else {
      qDebug() << "[PositionWindow] Contract NOT found for" << p.tradingSymbol
               << "ScripCode:" << pd.scripCode << "LookupID:" << lookupId;
      // Fallback
      pd.symbol = p.tradingSymbol;
      // Infer instrument type from segment suffix
      if (segmentSuffix == "CM")
        pd.instrumentType = "EQ";
      else if (segmentSuffix == "FO")
        pd.instrumentType = "FUT"; // Default guessing for FO
      else
        pd.instrumentType = "UNKNOWN"; // Fallback for other cases
    }

    m_allPositions.append(pd);
  }
  qDebug() << "[PositionWindow] Processed" << m_allPositions.size()
           << "positions. Applying filters...";
  applyFilters();
}

void PositionWindow::applyFilters() {
  QList<PositionData> filtered;
  QString exFilter = m_cbExchange->currentText();
  QString segFilter = m_cbSegment->currentText();
  QString prodFilter = m_cbPeriodicity->currentText();
  QString cbUserFilter = m_cbUser->currentText();
  QString cbClientFilter = m_cbClient->currentText();

  for (const auto &p : m_allPositions) {
    bool v = true;

    // Exchange Filter
    if (exFilter != "All" && p.exchange != exFilter)
      v = false;

    // Segment Filter
    if (v && segFilter != "All") {
      if (segFilter == "Cash") {
        if (p.instrumentType != "EQ" && p.instrumentType != "EQUITY")
          v = false;
      } else if (segFilter == "FO") {
        if (p.instrumentType != "FUT" && p.instrumentType != "OPT" &&
            p.instrumentType != "SPD")
          v = false;
      } else {
        if (p.instrumentType != segFilter)
          v = false;
      }
    }

    // Product Filter
    if (v && prodFilter != "All" && p.productType != prodFilter)
      v = false;

    // User/Client Filters
    if (v && cbUserFilter != "All" && p.user != cbUserFilter)
      v = false;
    if (v && cbClientFilter != "All" && p.client != cbClientFilter)
      v = false;

    // --- Column-level Excel Filters ---
    if (v && !m_columnFilters.isEmpty()) {
      for (auto it = m_columnFilters.begin(); it != m_columnFilters.end();
           ++it) {
        int col = it.key();
        const QStringList &allowed = it.value();
        if (allowed.isEmpty())
          continue;

        // Map column index to data field using PositionModel columns
        QString val;
        switch (col) {
        case PositionModel::ScripCode:
          val = QString::number(p.scripCode);
          break;
        case PositionModel::Symbol:
          val = p.symbol;
          break;
        case PositionModel::SeriesExpiry:
          val = p.seriesExpiry;
          break;
        case PositionModel::StrikePrice:
          val = QString::number(p.strikePrice, 'f', 2);
          break;
        case PositionModel::OptionType:
          val = p.optionType;
          break;
        case PositionModel::NetQty:
          val = QString::number(p.netQty);
          break;
        case PositionModel::MarketPrice:
          val = QString::number(p.marketPrice, 'f', 2);
          break;
        case PositionModel::MTMGL:
          val = QString::number(p.mtm, 'f', 2);
          break;
        case PositionModel::NetPrice:
          val = QString::number(p.netPrice, 'f', 2);
          break;
        case PositionModel::MTMVPos:
          val = QString::number(p.mtmvPos, 'f', 2);
          break;
        case PositionModel::TotalValue:
          val = QString::number(p.totalValue, 'f', 2);
          break;
        case PositionModel::BuyVal:
          val = QString::number(p.buyVal, 'f', 2);
          break;
        case PositionModel::SellVal:
          val = QString::number(p.sellVal, 'f', 2);
          break;
        case PositionModel::Exchange:
          val = p.exchange;
          break;
        case PositionModel::User:
          val = p.user;
          break;
        case PositionModel::Client:
          val = p.client;
          break;
        case PositionModel::Name:
          val = p.name;
          break;
        case PositionModel::InstrumentType:
          val = p.instrumentType;
          break;
        case PositionModel::InstrumentName:
          val = p.instrumentName;
          break;
        case PositionModel::ScripName:
          val = p.scripName;
          break;
        case PositionModel::BuyQty:
          val = QString::number(p.buyQty);
          break;
        case PositionModel::BuyLot:
          val = QString::number(p.buyLot, 'f', 2);
          break;
        case PositionModel::BuyWeight:
          val = QString::number(p.buyWeight, 'f', 2);
          break;
        case PositionModel::BuyAvg:
          val = QString::number(p.buyAvg, 'f', 2);
          break;
        case PositionModel::SellQty:
          val = QString::number(p.sellQty);
          break;
        case PositionModel::SellLot:
          val = QString::number(p.sellLot, 'f', 2);
          break;
        case PositionModel::SellWeight:
          val = QString::number(p.sellWeight, 'f', 2);
          break;
        case PositionModel::SellAvg:
          val = QString::number(p.sellAvg, 'f', 2);
          break;
        case PositionModel::NetLot:
          val = QString::number(p.netLot, 'f', 2);
          break;
        case PositionModel::NetWeight:
          val = QString::number(p.netWeight, 'f', 2);
          break;
        case PositionModel::NetVal:
          val = QString::number(p.netVal, 'f', 2);
          break;
        case PositionModel::ProductType:
          val = p.productType;
          break;
        case PositionModel::ClientGroup:
          val = p.clientGroup;
          break;
        case PositionModel::DPRRange:
          val = QString::number(p.dprRange, 'f', 2);
          break;
        case PositionModel::MaturityDate:
          val = p.maturityDate;
          break;
        case PositionModel::Yield:
          val = QString::number(p.yield, 'f', 2);
          break;
        case PositionModel::TotalQuantity:
          val = QString::number(p.totalQuantity);
          break;
        case PositionModel::TotalLot:
          val = QString::number(p.totalLot, 'f', 2);
          break;
        case PositionModel::TotalWeight:
          val = QString::number(p.totalWeight, 'f', 2);
          break;
        case PositionModel::Brokerage:
          val = QString::number(p.brokerage, 'f', 2);
          break;
        case PositionModel::NetMTM:
          val = QString::number(p.netMtm, 'f', 2);
          break;
        case PositionModel::NetValuePostExp:
          val = QString::number(p.netValPostExp, 'f', 2);
          break;
        case PositionModel::OptionFlag:
          val = p.optionFlag;
          break;
        case PositionModel::VarPercent:
          val = QString::number(p.varPercent, 'f', 2);
          break;
        case PositionModel::VarAmount:
          val = QString::number(p.varAmount, 'f', 2);
          break;
        case PositionModel::SMCategory:
          val = p.smCategory;
          break;
        case PositionModel::CfAvgPrice:
          val = QString::number(p.cfAvgPrice, 'f', 2);
          break;
        case PositionModel::ActualMTM:
          val = QString::number(p.actualMtm, 'f', 2);
          break;
        case PositionModel::UnsettledQty:
          val = QString::number(p.unsettledQty);
          break;
        default:
          val = "";
          break;
        }

        if (!allowed.contains(val)) {
          v = false;
          break;
        }
      }
    }

    // --- Inline Text Filters (from BaseBookWindow::m_textFilters) ---
    if (v && !m_textFilters.isEmpty()) {
      for (auto it = m_textFilters.begin(); it != m_textFilters.end(); ++it) {
        int col = it.key();
        const QString &filterText = it.value();
        if (filterText.isEmpty())
          continue;

        // Get value for this column
        QString val;
        bool hasMapping = true;
        switch (col) {
        case PositionModel::ScripCode:
          val = QString::number(p.scripCode);
          break;
        case PositionModel::Symbol:
          val = p.symbol;
          break;
        case PositionModel::SeriesExpiry:
          val = p.seriesExpiry;
          break;
        case PositionModel::StrikePrice:
          val = QString::number(p.strikePrice, 'f', 2);
          break;
        case PositionModel::OptionType:
          val = p.optionType;
          break;
        case PositionModel::Exchange:
          val = p.exchange;
          break;
        case PositionModel::User:
          val = p.user;
          break;
        case PositionModel::Client:
          val = p.client;
          break;
        case PositionModel::Name:
          val = p.name;
          break;
        case PositionModel::InstrumentType:
          val = p.instrumentType;
          break;
        case PositionModel::InstrumentName:
          val = p.instrumentName;
          break;
        case PositionModel::ScripName:
          val = p.scripName;
          break;
        case PositionModel::ProductType:
          val = p.productType;
          break;
        default:
          hasMapping = false;
          break;
        }

        // Only filter if we have a mapping for this column
        if (hasMapping && !val.contains(filterText, Qt::CaseInsensitive)) {
          v = false;
          break;
        }
      }
    }

    if (v)
      filtered.append(p);
  }

  // qDebug() << "[PositionWindow] Filters applied. Resulting count:" <<
  // filtered.size();
  static_cast<PositionModel *>(m_model)->setPositions(filtered);
  updateSummaryRow();
}

void PositionWindow::updateSummaryRow() {
  PositionData summary;
  // Initialize summary with "Total" label for the Symbol column (or similar) if
  // desired
  summary.symbol = "Total";

  for (const auto &p : static_cast<PositionModel *>(m_model)->positions()) {
    summary.mtm += p.mtm;
    summary.buyQty += p.buyQty;
    summary.sellQty += p.sellQty;
    summary.netQty += p.netQty;

    summary.totalValue += p.totalValue;
    summary.buyVal += p.buyVal;
    summary.sellVal += p.sellVal;
    summary.netVal += p.netVal;
  }
  static_cast<PositionModel *>(m_model)->setSummary(summary);
}

void PositionWindow::onColumnFilterChanged(int c, const QStringList &s) {
  if (c == -1)
    m_columnFilters.clear();
  else if (s.isEmpty())
    m_columnFilters.remove(c);
  else
    m_columnFilters[c] = s;
  applyFilters();
}

void PositionWindow::onTextFilterChanged(int col, const QString &text) {
  // Call base class to store the filter
  BaseBookWindow::onTextFilterChanged(col, text);
  // Re-apply filters to update the view
  applyFilters();
}

void PositionWindow::toggleFilterRow() {
  BaseBookWindow::toggleFilterRow(m_model, m_tableView);
}
void PositionWindow::onRefreshClicked() {
  if (m_tradingDataService)
    onPositionsUpdated(m_tradingDataService->getPositions());
}
void PositionWindow::onExportClicked() {
  BaseBookWindow::exportToCSV(m_model, m_tableView);
}
void PositionWindow::onSquareOffClicked() { qDebug() << "Square off"; }

void PositionWindow::updateMarketPrices() {
  // Prevent concurrent updates - skip if already updating
  if (m_isUpdating)
    return;

  QMutexLocker locker(&m_updateMutex);
  m_isUpdating = true;

  auto &gateway = MarketData::PriceStoreGateway::instance();
  bool anyChanged = false;

  // We iterate over m_allPositions directly
  for (int i = 0; i < m_allPositions.size(); ++i) {
    PositionData &pd = m_allPositions[i];

    // Construct segment ID logic
    int segIds[2] = {0, 0};
    if (pd.exchange == "NSE") {
      segIds[0] = 1;
      segIds[1] = 2;
    } else if (pd.exchange == "BSE") {
      segIds[0] = 11;
      segIds[1] = 12;
    }

    MarketData::UnifiedState snapshot;
    bool found = false;

    // Try getting price from first segment candidate
    if (segIds[0] > 0) {
      snapshot = gateway.getUnifiedSnapshot(segIds[0], pd.scripCode);
      if (snapshot.token != 0)
        found = true;

      // If not found, try second candidate (e.g., if we didn't differentiate CM
      // vs FO correctly)
      if (!found && segIds[1] > 0) {
        snapshot = gateway.getUnifiedSnapshot(segIds[1], pd.scripCode);
        if (snapshot.token != 0)
          found = true;
      }
    }

    if (found) {
      double ltp = snapshot.ltp;
      if (ltp > 0 && ltp != pd.marketPrice) {
        pd.marketPrice = ltp;

        // Recalculate MTM
        double buyVal = pd.buyVal;   // from API (usually absolute cost)
        double sellVal = pd.sellVal; // from API (usually absolute revenue)

        // MTM = (Sell Value - Buy Value) + (Net Qty * Current Market Price)
        // Note: Val fields from XTS might be negative/positive depending on
        // side. Assuming standard P&L calc: Realized + Unrealized Unrealized =
        // (Net Qty * LTP) - (Net Qty * Avg Price) Total MTM = Realized +
        // Unrealized

        // Simplified view update logic:
        // pd.mtm = currentVal;

        // For now preserving original logic:
        // double currentVal = sellVal - buyVal + (pd.netQty * ltp);
        // pd.mtm = currentVal;

        // Actually recalculating MTM based on average price is more standard?
        // MTM = (Net Qty * LTP) - (Net Invested Value)
        // For Position Window usually we show Total MTM.

        // Let's stick to the previous implementation logic:
        // double currentVal = sellVal - buyVal + (pd.netQty * ltp);
        // But wait, sellVal/buyVal in XTS are usually positive numbers.
        // If I Sell 100 @ 100 -> SellVal = 10000. Buy 0. Net Qty = -100.
        // MTM = 10000 - 0 + (-100 * 90) = 10000 - 9000 = 1000 Profit. Correct.

        // If I Buy 100 @ 100 -> BuyVal = 10000. Sell 0. Net Qty = 100.
        // MTM = 0 - 10000 + (100 * 110) = -10000 + 11000 = 1000 Profit.
        // Correct.

        if (pd.netQty != 0) {
          pd.mtm = pd.sellVal - pd.buyVal + (pd.netQty * ltp);
        }

        pd.netVal = pd.netQty * ltp;
        pd.totalValue = std::abs(pd.buyVal) + std::abs(pd.sellVal);

        anyChanged = true;
      }
    }
  }

  if (anyChanged) {
    applyFilters(); // Refreshes the model with updated data
  }

  m_isUpdating = false;
}

void PositionWindow::addPosition(const PositionData &p) {
  m_allPositions.append(p);
  applyFilters();
}
void PositionWindow::updatePosition(const QString &s, const PositionData &p) {
  for (int i = 0; i < m_allPositions.size(); ++i)
    if (m_allPositions[i].symbol == s) {
      m_allPositions[i] = p;
      break;
    }
  applyFilters();
}

WindowContext PositionWindow::getSelectedContext() const {
  WindowContext ctx;
  ctx.sourceWindow = "PositionWindow";

  if (!m_tableView || !m_tableView->selectionModel())
    return ctx;

  QModelIndex idx = m_tableView->selectionModel()->currentIndex();
  if (!idx.isValid())
    return ctx;

  // Map proxy index to source index
  QModelIndex sourceIdx = m_proxyModel->mapToSource(idx);
  if (!sourceIdx.isValid())
    return ctx;

  PositionModel *model = qobject_cast<PositionModel *>(m_model);
  if (!model)
    return ctx;

  int row = sourceIdx.row();
  // Handle filter row offset
  if (model->isFilterRowVisible()) {
    if (row == 0)
      return ctx; // Filter row selected
    row--;
  }

  QList<PositionData> positions = model->positions();
  if (row < 0 || row >= positions.size())
    return ctx;

  const PositionData &p = positions[row];

  // Determine Exchange Segment (NSECM, NSEFO, etc.)
  QString segSuffix = "FO";
  if (p.instrumentType == "EQUITY" || p.instrumentType == "EQ") {
    segSuffix = "CM";
  }
  ctx.exchange = p.exchange + segSuffix;

  ctx.token = p.scripCode;
  ctx.symbol = p.symbol;
  ctx.displayName = p.scripName.isEmpty() ? p.symbol : p.scripName;
  ctx.series =
      p.seriesExpiry; // Logic reusing seriesExpiry field for series if equity?
  // Actually PositionData has distinct seriesExpiry? No, looks like series is
  // mixed. Let's rely on RepositoryManager lookup if needed for pure 'Series'.

  ctx.instrumentType = p.instrumentType;
  ctx.expiry = p.seriesExpiry;
  ctx.strikePrice = p.strikePrice;
  ctx.optionType = p.optionType;
  ctx.ltp = p.marketPrice;
  ctx.close = p.netPrice; // Fallback

  ctx.productType = p.productType; // MIS, NRML, CNC from PositionData

  return ctx;
}
void PositionWindow::clearPositions() {
  m_allPositions.clear();
  applyFilters();
}
