// ============================================================================
// UI.cpp — OptionChain UI setup, models, connections, shortcuts, delegate,
//          column metadata, presets, visibility
// ============================================================================
#include "views/OptionChainWindow.h"
#include "services/GreeksCalculationService.h"
#include "views/GenericProfileDialog.h"
#include <QCheckBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QVBoxLayout>
#include <limits>

// ============================================================================
// OptionChainDelegate Implementation
// ============================================================================

OptionChainDelegate::OptionChainDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void OptionChainDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
  // For checkbox columns, use default rendering
  QString headerText =
      index.model()->headerData(index.column(), Qt::Horizontal).toString();
  if (headerText.isEmpty() && index.column() == 0) {
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }
  if (headerText.isEmpty() &&
      index.column() == index.model()->columnCount() - 1) {
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  painter->save();

  QString text = index.data(Qt::DisplayRole).toString();
  QColor bgColor = QColor("transparent");
  QColor textColor = QColor("#1e293b");

  // Check for dynamic tick update color first
  int direction = index.data(Qt::UserRole + 1).toInt();
  if (direction == 1) {
    bgColor = QColor("#dbeafe");
    textColor = QColor("#1d4ed8");
  } else if (direction == 2) {
    bgColor = QColor("#fee2e2");
    textColor = QColor("#dc2626");
  }

  // IV column highlight
  if (headerText == "IV" || headerText == "BidIV" || headerText == "AskIV") {
    if (bgColor == QColor("transparent"))
      bgColor = QColor("#fef9c3");
    textColor = QColor("#92400e");
    QFont f = option.font;
    f.setBold(true);
    painter->setFont(f);
  }

  // Change column coloring
  if (headerText.contains("Chng") || headerText.contains("Change")) {
    bool ok;
    double value = text.toDouble(&ok);

    if (ok && value != 0.0) {
      if (value > 0) {
        textColor = QColor("#16a34a");
      } else {
        textColor = QColor("#dc2626");
      }
    }
  }

  // Selection overrides
  if (option.state & QStyle::State_Selected) {
    bgColor = QColor("#dbeafe");
  }

  painter->fillRect(option.rect, bgColor);
  painter->setPen(textColor);
  painter->drawText(option.rect, Qt::AlignCenter, text);

  painter->restore();
}

QSize OptionChainDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const {
  return QStyledItemDelegate::sizeHint(option, index);
}

// ============================================================================
// Column metadata & preset factories
// ============================================================================

QList<GenericColumnInfo> OptionChainWindow::buildColumnMetadata()
{
    return {
        { 0,  "OI",          70,  true  },
        { 1,  "Chng in OI",  80,  true  },
        { 2,  "Volume",      70,  true  },
        { 3,  "IV",          60,  true  },
        { 4,  "Bid IV",      60,  false },
        { 5,  "Ask IV",      60,  false },
        { 6,  "Delta",       65,  false },
        { 7,  "Gamma",       65,  false },
        { 8,  "Vega",        65,  false },
        { 9,  "Theta",       65,  false },
        { 10, "LTP",         70,  true  },
        { 11, "Chng",        70,  true  },
        { 12, "Bid Qty",     70,  true  },
        { 13, "Bid",         70,  true  },
        { 14, "Ask",         70,  true  },
        { 15, "Ask Qty",     70,  true  },
    };
}

GenericTableProfile OptionChainWindow::createPreset_Default(const QList<GenericColumnInfo> &cols)
{
    return GenericTableProfile::createDefault(cols);
}

GenericTableProfile OptionChainWindow::createPreset_Compact(const QList<GenericColumnInfo> &cols)
{
    GenericTableProfile p("Compact");
    p.setDescription("Minimal columns for quick overview");
    QList<int> order;
    QSet<int> vis = { 0/*OI*/, 2/*Volume*/, 10/*LTP*/, 11/*Chng*/, 13/*Bid*/, 14/*Ask*/ };
    for (const auto &c : cols) {
        order.append(c.id);
        p.setColumnVisible(c.id, vis.contains(c.id));
        p.setColumnWidth(c.id, c.defaultWidth);
    }
    p.setColumnOrder(order);
    return p;
}

GenericTableProfile OptionChainWindow::createPreset_Greeks(const QList<GenericColumnInfo> &cols)
{
    GenericTableProfile p("Greeks");
    p.setDescription("Greek values and implied volatility");
    QList<int> order;
    QSet<int> vis = { 0/*OI*/, 3/*IV*/, 4/*BidIV*/, 5/*AskIV*/,
                      6/*Delta*/, 7/*Gamma*/, 8/*Vega*/, 9/*Theta*/, 10/*LTP*/ };
    for (const auto &c : cols) {
        order.append(c.id);
        p.setColumnVisible(c.id, vis.contains(c.id));
        p.setColumnWidth(c.id, c.defaultWidth);
    }
    p.setColumnOrder(order);
    return p;
}

GenericTableProfile OptionChainWindow::createPreset_Trading(const QList<GenericColumnInfo> &cols)
{
    GenericTableProfile p("Trading");
    p.setDescription("Full trading view with OI and bid/ask");
    QList<int> order;
    QSet<int> vis = { 0/*OI*/, 1/*ChngInOI*/, 2/*Volume*/, 3/*IV*/,
                      10/*LTP*/, 11/*Chng*/, 12/*BidQty*/, 13/*Bid*/, 14/*Ask*/, 15/*AskQty*/ };
    for (const auto &c : cols) {
        order.append(c.id);
        p.setColumnVisible(c.id, vis.contains(c.id));
        p.setColumnWidth(c.id, c.defaultWidth);
    }
    p.setColumnOrder(order);
    return p;
}

// ============================================================================
// UI Setup
// ============================================================================

void OptionChainWindow::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(10, 10, 10, 10);
  mainLayout->setSpacing(10);

  // Header Section
  QHBoxLayout *headerLayout = new QHBoxLayout();
  headerLayout->setSpacing(10);

  m_titleLabel = new QLabel("NIFTY");
  m_titleLabel->setStyleSheet(
      "QLabel { font-size: 16px; font-weight: bold; color: #1e293b; }");
  headerLayout->addWidget(m_titleLabel);

  headerLayout->addStretch();

  QLabel *symbolLabel = new QLabel("Symbol:");
  symbolLabel->setStyleSheet("QLabel { color: #475569; font-weight: bold; }");
  headerLayout->addWidget(symbolLabel);

  m_symbolCombo = new QComboBox();
  m_symbolCombo->setObjectName("symbolCombo");
  m_symbolCombo->setMinimumWidth(120);
  m_symbolCombo->setStyleSheet(
      "QComboBox { background: #ffffff; color: #0f172a; border: 1px solid "
      "#cbd5e1; padding: 4px; border-radius: 4px; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox QAbstractItemView { background-color: #ffffff; color: "
      "#0f172a; selection-background-color: #bfdbfe; selection-color: #1e40af; }");
  headerLayout->addWidget(m_symbolCombo);

  QLabel *expiryLabel = new QLabel("Expiry:");
  expiryLabel->setStyleSheet("QLabel { color: #475569; font-weight: bold; }");
  headerLayout->addWidget(expiryLabel);

  m_expiryCombo = new QComboBox();
  m_expiryCombo->setObjectName("expiryCombo");
  m_expiryCombo->setMinimumWidth(120);
  m_expiryCombo->setStyleSheet(
      "QComboBox { background: #ffffff; color: #0f172a; border: 1px solid "
      "#cbd5e1; padding: 4px; border-radius: 4px; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox QAbstractItemView { background-color: #ffffff; color: "
      "#0f172a; selection-background-color: #bfdbfe; selection-color: #1e40af; }");
  headerLayout->addWidget(m_expiryCombo);

  m_refreshButton = new QPushButton("Refresh");
  m_refreshButton->setStyleSheet(
      "QPushButton { background: #f1f5f9; color: #334155; border: 1px solid "
      "#cbd5e1; "
      "padding: 5px 12px; border-radius: 4px; font-weight: 600; }"
      "QPushButton:hover { background: #e2e8f0; color: #0f172a; }"
      "QPushButton:pressed { background: #dbeafe; border-color: #3b82f6; }");
  headerLayout->addWidget(m_refreshButton);

  m_calculatorButton = new QPushButton("View Calculators");
  m_calculatorButton->setStyleSheet(
      "QPushButton { background: #f1f5f9; color: #334155; border: 1px solid "
      "#cbd5e1; "
      "padding: 5px 12px; border-radius: 4px; font-weight: 600; }"
      "QPushButton:hover { background: #e2e8f0; color: #0f172a; }"
      "QPushButton:pressed { background: #dbeafe; border-color: #3b82f6; }");
  headerLayout->addWidget(m_calculatorButton);

  m_columnsButton = new QPushButton("Columns...");
  m_columnsButton->setStyleSheet(
      "QPushButton { background: #f1f5f9; color: #334155; border: 1px solid "
      "#cbd5e1; "
      "padding: 5px 12px; border-radius: 4px; font-weight: 600; }"
      "QPushButton:hover { background: #e2e8f0; color: #0f172a; }"
      "QPushButton:pressed { background: #dbeafe; border-color: #3b82f6; }");
  connect(m_columnsButton, &QPushButton::clicked, this, &OptionChainWindow::showColumnDialog);
  headerLayout->addWidget(m_columnsButton);

  mainLayout->addLayout(headerLayout);

  // Table Section
  QHBoxLayout *tableLayout = new QHBoxLayout();
  tableLayout->setSpacing(0);
  tableLayout->setContentsMargins(0, 0, 0, 0);

  QString tableStyle = "QTableView {"
                       "   background-color: #ffffff;"
                       "   color: #1e293b;"
                       "   gridline-color: #f1f5f9;"
                       "   border: 1px solid #e2e8f0;"
                       "   selection-background-color: #bfdbfe;"
                       "   selection-color: #1e40af;"
                       "}"
                       "QTableView::item {"
                       "   padding: 4px;"
                       "}"
                       "QHeaderView::section {"
                       "   background-color: #f8fafc;"
                       "   color: #475569;"
                       "   padding: 4px;"
                       "   border: none;"
                       "   border-bottom: 2px solid #e2e8f0;"
                       "   font-weight: bold;"
                       "   font-size: 11px;"
                       "}";

  // Call Table (Left)
  m_callTable = new QTableView();
  m_callTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_callTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_callTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_callTable->verticalHeader()->hide();
  m_callTable->setAlternatingRowColors(false);
  m_callTable->setShowGrid(true);
  m_callTable->setGridStyle(Qt::SolidLine);
  m_callTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_callTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_callTable->viewport()->installEventFilter(this);
  m_callTable->setStyleSheet(tableStyle);
  m_callTable->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_callTable, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
    QMenu menu(this);
    menu.addAction("Column Profile...", this, &OptionChainWindow::showColumnDialog);
    menu.exec(m_callTable->viewport()->mapToGlobal(pos));
  });
  // Unified: same context menu from header
  m_callTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_callTable->horizontalHeader(), &QHeaderView::customContextMenuRequested, this,
          [this](const QPoint &headerPos) {
              QMenu menu(this);
              menu.addAction("Column Profile...", this, &OptionChainWindow::showColumnDialog);
              menu.exec(m_callTable->horizontalHeader()->mapToGlobal(headerPos));
          });
  tableLayout->addWidget(m_callTable, 4);

  // Strike Table (Center)
  m_strikeTable = new QTableView();
  m_strikeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_strikeTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_strikeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_strikeTable->verticalHeader()->hide();
  m_strikeTable->setAlternatingRowColors(false);
  m_strikeTable->setShowGrid(true);
  m_strikeTable->setGridStyle(Qt::SolidLine);
  m_strikeTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_strikeTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_strikeTable->viewport()->installEventFilter(this);
  m_strikeTable->setStyleSheet("QTableView {"
                               "   background-color: #f8fafc;"
                               "   color: #0f172a;"
                               "   gridline-color: #e2e8f0;"
                               "   border: 1px solid #e2e8f0;"
                               "   font-weight: bold;"
                               "   font-size: 12px;"
                               "   selection-background-color: #bfdbfe;"
                               "}"
                               "QTableView::item {"
                               "   padding: 4px;"
                               "}"
                               "QHeaderView::section {"
                               "   background-color: #f1f5f9;"
                               "   color: #475569;"
                               "   padding: 4px;"
                               "   border: none;"
                               "   border-bottom: 2px solid #e2e8f0;"
                               "   font-weight: bold;"
                               "}");
  tableLayout->addWidget(m_strikeTable, 1);

  // Put Table (Right)
  m_putTable = new QTableView();
  m_putTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_putTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_putTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_putTable->verticalHeader()->hide();
  m_putTable->setAlternatingRowColors(false);
  m_putTable->setShowGrid(true);
  m_putTable->setGridStyle(Qt::SolidLine);
  m_putTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_putTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_putTable->viewport()->installEventFilter(this);
  m_putTable->setStyleSheet(tableStyle);
  m_putTable->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_putTable, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
    QMenu menu(this);
    menu.addAction("Column Profile...", this, &OptionChainWindow::showColumnDialog);
    menu.exec(m_putTable->viewport()->mapToGlobal(pos));
  });
  // Unified: same context menu from header
  m_putTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_putTable->horizontalHeader(), &QHeaderView::customContextMenuRequested, this,
          [this](const QPoint &headerPos) {
              QMenu menu(this);
              menu.addAction("Column Profile...", this, &OptionChainWindow::showColumnDialog);
              menu.exec(m_putTable->horizontalHeader()->mapToGlobal(headerPos));
          });
  tableLayout->addWidget(m_putTable, 4);

  mainLayout->addLayout(tableLayout);

  // Keyboard-first: focus policies + tab order
  m_symbolCombo->setFocusPolicy(Qt::StrongFocus);
  m_expiryCombo->setFocusPolicy(Qt::StrongFocus);
  m_refreshButton->setFocusPolicy(Qt::StrongFocus);
  m_calculatorButton->setFocusPolicy(Qt::StrongFocus);
  m_callTable->setFocusPolicy(Qt::StrongFocus);
  m_strikeTable->setFocusPolicy(Qt::StrongFocus);
  m_putTable->setFocusPolicy(Qt::StrongFocus);
  QWidget::setTabOrder(m_symbolCombo,      m_expiryCombo);
  QWidget::setTabOrder(m_expiryCombo,      m_refreshButton);
  QWidget::setTabOrder(m_refreshButton,    m_calculatorButton);
  QWidget::setTabOrder(m_calculatorButton, m_callTable);
  QWidget::setTabOrder(m_callTable,        m_strikeTable);
  QWidget::setTabOrder(m_strikeTable,      m_putTable);
  QWidget::setTabOrder(m_putTable,         m_symbolCombo);

  setStyleSheet("QWidget { background-color: #ffffff; }");
}

void OptionChainWindow::setupModels() {
  // Call Model
  m_callModel = new QStandardItemModel(this);
  m_callModel->setColumnCount(CALL_COLUMN_COUNT);
  m_callModel->setHorizontalHeaderLabels(
      {"", "OI", "Chng in OI", "Volume", "IV", "BidIV", "AskIV", "Delta",
       "Gamma", "Vega", "Theta", "LTP", "Chng", "BID QTY", "BID", "ASK",
       "ASK QTY"});

  m_callTable->setModel(m_callModel);
  m_callTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_callTable->horizontalHeader()->setStretchLastSection(false);
  m_callTable->horizontalHeader()->setSectionsMovable(true);
  m_callTable->setColumnWidth(0, 30);
  m_callTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);

  m_callDelegate = new OptionChainDelegate(this);
  m_callTable->setItemDelegate(m_callDelegate);

  // Strike Model
  m_strikeModel = new QStandardItemModel(this);
  m_strikeModel->setColumnCount(1);
  m_strikeModel->setHorizontalHeaderLabels({"Strike"});

  m_strikeTable->setModel(m_strikeModel);
  m_strikeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_strikeTable->horizontalHeader()->setSectionsMovable(false);

  // Put Model
  m_putModel = new QStandardItemModel(this);
  m_putModel->setColumnCount(PUT_COLUMN_COUNT);
  m_putModel->setHorizontalHeaderLabels({"BID QTY", "BID", "ASK", "ASK QTY",
                                         "Chng", "LTP", "IV", "BidIV", "AskIV",
                                         "Delta", "Gamma", "Vega", "Theta",
                                         "Volume", "Chng in OI", "OI", ""});

  m_putTable->setModel(m_putModel);
  m_putTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_putTable->horizontalHeader()->setStretchLastSection(false);
  m_putTable->horizontalHeader()->setSectionsMovable(true);
  m_putTable->setColumnWidth(PUT_COLUMN_COUNT - 1, 30);
  m_putTable->horizontalHeader()->setSectionResizeMode(PUT_COLUMN_COUNT - 1,
                                                       QHeaderView::Fixed);

  m_putDelegate = new OptionChainDelegate(this);
  m_putTable->setItemDelegate(m_putDelegate);
}

void OptionChainWindow::setupConnections() {
  // Header controls
  connect(m_symbolCombo, &QComboBox::currentTextChanged, this,
          &OptionChainWindow::onSymbolChanged);
  connect(m_expiryCombo, &QComboBox::currentTextChanged, this,
          &OptionChainWindow::onExpiryChanged);
  connect(m_refreshButton, &QPushButton::clicked, this,
          &OptionChainWindow::onRefreshClicked);
  connect(m_calculatorButton, &QPushButton::clicked, this,
          &OptionChainWindow::onCalculatorClicked);

  // Table interactions
  connect(m_callTable, &QTableView::clicked, this,
          &OptionChainWindow::onCallTableClicked);
  connect(m_putTable, &QTableView::clicked, this,
          &OptionChainWindow::onPutTableClicked);
  connect(m_strikeTable, &QTableView::clicked, this,
          &OptionChainWindow::onStrikeTableClicked);

  // Tri-directional scroll sync
  connect(m_strikeTable->verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this](int v) {
            if (m_syncingScroll) return;
            m_syncingScroll = true;
            m_callTable->verticalScrollBar()->setValue(v);
            m_putTable->verticalScrollBar()->setValue(v);
            m_syncingScroll = false;
          });
  connect(m_callTable->verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this](int v) {
            if (m_syncingScroll) return;
            m_syncingScroll = true;
            m_strikeTable->verticalScrollBar()->setValue(v);
            m_putTable->verticalScrollBar()->setValue(v);
            m_syncingScroll = false;
          });
  connect(m_putTable->verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this](int v) {
            if (m_syncingScroll) return;
            m_syncingScroll = true;
            m_strikeTable->verticalScrollBar()->setValue(v);
            m_callTable->verticalScrollBar()->setValue(v);
            m_syncingScroll = false;
          });

  connect(this, &OptionChainWindow::refreshRequested, this,
          &OptionChainWindow::refreshData);

  // Greeks Updates
  connect(
      &GreeksCalculationService::instance(),
      &GreeksCalculationService::greeksCalculated, this,
      [this](uint32_t token, int exchangeSegment, const GreeksResult &result) {
        if (!m_tokenToStrike.contains(token))
          return;
        double strike = m_tokenToStrike[token];

        if (!m_strikeData.contains(strike))
          return;
        OptionStrikeData &data = m_strikeData[strike];

        bool isCall = (data.callToken == (int)token);
        int row = m_strikes.indexOf(strike);
        if (row < 0)
          return;

        if (isCall) {
          data.callIV = result.impliedVolatility;
          data.callBidIV = result.bidIV;
          data.callAskIV = result.askIV;
          data.callDelta = result.delta;
          data.callGamma = result.gamma;
          data.callVega = result.vega;
          data.callTheta = result.theta;

          m_callModel->item(row, CALL_IV)
              ->setText(QString::number(data.callIV * 100.0, 'f', 2));
          m_callModel->item(row, CALL_BID_IV)
              ->setText(QString::number(data.callBidIV * 100.0, 'f', 2));
          m_callModel->item(row, CALL_ASK_IV)
              ->setText(QString::number(data.callAskIV * 100.0, 'f', 2));
          m_callModel->item(row, CALL_DELTA)
              ->setText(QString::number(data.callDelta, 'f', 2));
          m_callModel->item(row, CALL_GAMMA)
              ->setText(QString::number(data.callGamma, 'f', 4));
          m_callModel->item(row, CALL_VEGA)
              ->setText(QString::number(data.callVega, 'f', 2));
          m_callModel->item(row, CALL_THETA)
              ->setText(QString::number(data.callTheta, 'f', 2));
        } else {
          data.putIV = result.impliedVolatility;
          data.putBidIV = result.bidIV;
          data.putAskIV = result.askIV;
          data.putDelta = result.delta;
          data.putGamma = result.gamma;
          data.putVega = result.vega;
          data.putTheta = result.theta;

          m_putModel->item(row, PUT_IV)
              ->setText(QString::number(data.putIV * 100.0, 'f', 2));
          m_putModel->item(row, PUT_BID_IV)
              ->setText(QString::number(data.putBidIV * 100.0, 'f', 2));
          m_putModel->item(row, PUT_ASK_IV)
              ->setText(QString::number(data.putAskIV * 100.0, 'f', 2));
          m_putModel->item(row, PUT_DELTA)
              ->setText(QString::number(data.putDelta, 'f', 2));
          m_putModel->item(row, PUT_GAMMA)
              ->setText(QString::number(data.putGamma, 'f', 4));
          m_putModel->item(row, PUT_VEGA)
              ->setText(QString::number(data.putVega, 'f', 2));
          m_putModel->item(row, PUT_THETA)
              ->setText(QString::number(data.putTheta, 'f', 2));
        }
      });

  // Keyboard: Enter key activates trade for Call / Put / Strike
  connect(m_callTable, &QTableView::activated, this,
          [this](const QModelIndex &index) {
            int row = index.row();
            m_selectedCallRow = row;
            double strike = getStrikeAtRow(row);
            if (strike > 0)
              emit tradeRequested(m_currentSymbol, m_currentExpiry, strike, "CE");
          });

  connect(m_putTable, &QTableView::activated, this,
          [this](const QModelIndex &index) {
            int row = index.row();
            m_selectedPutRow = row;
            double strike = getStrikeAtRow(row);
            if (strike > 0)
              emit tradeRequested(m_currentSymbol, m_currentExpiry, strike, "PE");
          });

  connect(m_strikeTable, &QTableView::activated, this,
          [this](const QModelIndex &index) {
            int row = index.row();
            m_selectedCallRow = row;
            m_selectedPutRow  = row;
            m_callTable->selectRow(row);
            m_putTable->selectRow(row);
            m_callTable->setFocus();
          });
}

void OptionChainWindow::setupShortcuts() {
  auto getActiveTable = [this]() -> QTableView * {
    if (m_callTable->hasFocus())   return m_callTable;
    if (m_strikeTable->hasFocus()) return m_strikeTable;
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
    if (t->model() && row >= 0 && row < t->model()->rowCount()) {
      t->selectRow(row);
      t->scrollTo(t->model()->index(row, 0), QAbstractItemView::EnsureVisible);
    }
  };

  // Ctrl+Right: Call → Strike → Put → (wraps to Call)
  auto *nextSC = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right), this);
  connect(nextSC, &QShortcut::activated, this, [this, getActiveTable, getRow, focusTable]() {
    QTableView *cur = getActiveTable();
    int row = getRow(cur);
    if      (cur == m_callTable)   focusTable(m_strikeTable, row);
    else if (cur == m_strikeTable) focusTable(m_putTable,    row);
    else                           focusTable(m_callTable,   row);
  });

  // Ctrl+Left: Put → Strike → Call → (wraps to Put)
  auto *prevSC = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left), this);
  connect(prevSC, &QShortcut::activated, this, [this, getActiveTable, getRow, focusTable]() {
    QTableView *cur = getActiveTable();
    int row = getRow(cur);
    if      (cur == m_putTable)    focusTable(m_strikeTable, row);
    else if (cur == m_strikeTable) focusTable(m_callTable,   row);
    else                           focusTable(m_putTable,    row);
  });

  // Ctrl+R: Refresh
  auto *refreshSC = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this);
  refreshSC->setContext(Qt::WidgetWithChildrenShortcut);
  connect(refreshSC, &QShortcut::activated, this, &OptionChainWindow::onRefreshClicked);

  // Ctrl+S: Focus Symbol combo + open dropdown
  auto *symbolSC = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this);
  symbolSC->setContext(Qt::WidgetWithChildrenShortcut);
  connect(symbolSC, &QShortcut::activated, this, [this]() {
    m_symbolCombo->setFocus();
    m_symbolCombo->showPopup();
  });

  // Ctrl+E: Focus Expiry combo + open dropdown
  auto *expirySC = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_E), this);
  expirySC->setContext(Qt::WidgetWithChildrenShortcut);
  connect(expirySC, &QShortcut::activated, this, [this]() {
    m_expiryCombo->setFocus();
    m_expiryCombo->showPopup();
  });

  // Ctrl+G: Scroll and select ATM strike row
  auto *gotoSC = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);
  connect(gotoSC, &QShortcut::activated, this, [this]() {
    if (m_strikes.isEmpty()) return;
    int atmRow = 0;
    if (m_atmStrike > 0) {
      double minDist = std::numeric_limits<double>::max();
      for (int i = 0; i < m_strikes.size(); ++i) {
        double dist = std::abs(m_strikes[i] - m_atmStrike);
        if (dist < minDist) { minDist = dist; atmRow = i; }
      }
    }
    auto scrollAll = [this, atmRow](QTableView *t) {
      t->scrollTo(t->model()->index(atmRow, 0), QAbstractItemView::PositionAtCenter);
      t->selectRow(atmRow);
    };
    scrollAll(m_callTable);
    scrollAll(m_strikeTable);
    scrollAll(m_putTable);
    m_callTable->setFocus();
  });
}

void OptionChainWindow::highlightATMStrike() {
  int atmRow = -1;
  for (int i = 0; i < m_strikes.size(); ++i) {
    if (m_strikes[i] == m_atmStrike) {
      atmRow = i;
      break;
    }
  }

  if (atmRow < 0)
    return;

  QColor atmColor("#dbeafe");

  for (int col = 0; col < m_callModel->columnCount(); ++col) {
    QStandardItem *item = m_callModel->item(atmRow, col);
    if (item) {
      item->setBackground(atmColor);
    }
  }

  QStandardItem *strikeItem = m_strikeModel->item(atmRow, 0);
  if (strikeItem) {
    strikeItem->setBackground(QColor("#bfdbfe"));
    strikeItem->setForeground(QColor("#1e40af"));
  }

  for (int col = 0; col < m_putModel->columnCount(); ++col) {
    QStandardItem *item = m_putModel->item(atmRow, col);
    if (item) {
      item->setBackground(atmColor);
    }
  }
}

void OptionChainWindow::updateTableColors() {
  m_callTable->viewport()->update();
  m_putTable->viewport()->update();
}

QModelIndex OptionChainWindow::getStrikeIndex(int row) const {
  return m_strikeModel->index(row, 0);
}

double OptionChainWindow::getStrikeAtRow(int row) const {
  if (row >= 0 && row < m_strikes.size()) {
    return m_strikes[row];
  }
  return 0.0;
}

void OptionChainWindow::applyColumnVisibility() {
  for (int colId = 0; colId < 16; ++colId) {
    bool visible = m_columnProfile.isColumnVisible(colId);

    int callIdx = colId + 1;
    if (callIdx > 0 && callIdx < m_callModel->columnCount())
      m_callTable->setColumnHidden(callIdx, !visible);

    int putIdx = putColumnIndex(colId);
    if (putIdx >= 0 && putIdx < m_putModel->columnCount() - 1)
      m_putTable->setColumnHidden(putIdx, !visible);

    int w = m_columnProfile.columnWidth(colId);
    if (w > 0) {
      if (callIdx > 0 && callIdx < m_callModel->columnCount())
        m_callTable->setColumnWidth(callIdx, w);
      if (putIdx >= 0 && putIdx < m_putModel->columnCount() - 1)
        m_putTable->setColumnWidth(putIdx, w);
    }
  }

  // Persist the profile as JSON in QSettings
  QSettings settings("configs/config.ini", QSettings::IniFormat);
  settings.beginGroup("OPTION_CHAIN_PROFILE");
  QJsonDocument doc(m_columnProfile.toJson());
  settings.setValue("profile_json", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
  settings.endGroup();
  settings.sync();

  qDebug() << "[OptionChain] Column visibility applied from profile:" << m_columnProfile.name();
}

void OptionChainWindow::showColumnDialog() {
  captureColumnWidths();

  QList<GenericColumnInfo> colMeta = buildColumnMetadata();
  GenericProfileDialog dialog("Option Chain", colMeta, m_profileManager, m_columnProfile, this);
  if (dialog.exec() == QDialog::Accepted) {
    m_columnProfile = dialog.getProfile();
    applyColumnVisibility();

    // Persist to JSON file so it survives restart
    m_profileManager->saveLastUsedProfile(m_columnProfile);  // always works, even for preset names
    m_profileManager->saveCustomProfile(m_columnProfile);
    m_profileManager->saveDefaultProfileName(m_columnProfile.name());

    qInfo() << "[OptionChain] Column profile updated:" << m_columnProfile.name();
  }
}
