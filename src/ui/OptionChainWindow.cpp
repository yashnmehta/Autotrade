#include "ui/OptionChainWindow.h"
#include <QHeaderView>
#include <QSplitter>
#include <QScrollBar>
#include <QPainter>
#include <QDebug>
#include <QPushButton>
#include <QGroupBox>
#include <QWheelEvent>
#include <cmath>

// ============================================================================
// OptionChainDelegate Implementation
// ============================================================================

OptionChainDelegate::OptionChainDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void OptionChainDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    // For checkbox columns, use default rendering
    QString headerText = index.model()->headerData(index.column(), Qt::Horizontal).toString();
    if (headerText.isEmpty() && index.column() == 0) {
        // This is a checkbox column (empty header in first or last column)
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    if (headerText.isEmpty() && index.column() == index.model()->columnCount() - 1) {
        // This is a checkbox column (empty header in first or last column)
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    
    painter->save();
    
    // Get the value and determine color
    QString text = index.data(Qt::DisplayRole).toString();
    QColor bgColor = QColor("#2A3A50"); // Default background
    QColor textColor = Qt::white;
    
    // Check for numeric values indicating change
    bool isChangeColumn = false;
    
    if (headerText.contains("Chng") || headerText.contains("Change")) {
        isChangeColumn = true;
        bool ok;
        double value = text.toDouble(&ok);
        
        if (ok && value != 0.0) {
            if (value > 0) {
                textColor = QColor("#00FF00"); // Green for positive
            } else {
                textColor = QColor("#FF4444"); // Red for negative
            }
        }
    }
    
    // Draw background
    if (option.state & QStyle::State_Selected) {
        bgColor = QColor("#3A5A70"); // Highlighted selection
    }
    
    painter->fillRect(option.rect, bgColor);
    
    // Draw text
    painter->setPen(textColor);
    painter->drawText(option.rect, Qt::AlignCenter, text);
    
    painter->restore();
}

QSize OptionChainDelegate::sizeHint(const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    return QStyledItemDelegate::sizeHint(option, index);
}

// ============================================================================
// OptionChainWindow Implementation
// ============================================================================

OptionChainWindow::OptionChainWindow(QWidget *parent)
    : QWidget(parent)
    , m_symbolCombo(nullptr)
    , m_expiryCombo(nullptr)
    , m_refreshButton(nullptr)
    , m_calculatorButton(nullptr)
    , m_titleLabel(nullptr)
    , m_callTable(nullptr)
    , m_strikeTable(nullptr)
    , m_putTable(nullptr)
    , m_callModel(nullptr)
    , m_strikeModel(nullptr)
    , m_putModel(nullptr)
    , m_callDelegate(nullptr)
    , m_putDelegate(nullptr)
    , m_atmStrike(0.0)
    , m_selectedCallRow(-1)
    , m_selectedPutRow(-1)
{
    setupUI();
    setupModels();
    setupConnections();
    populateDemoData();
    
    setWindowTitle("Option Chain");
    resize(1600, 800);
}

OptionChainWindow::~OptionChainWindow()
{
}

void OptionChainWindow::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // ========================================================================
    // Header Section
    // ========================================================================
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);
    
    // Title
    m_titleLabel = new QLabel("NIFTY");
    m_titleLabel->setStyleSheet("QLabel { font-size: 18px; font-weight: bold; color: #FFFFFF; }");
    headerLayout->addWidget(m_titleLabel);
    
    headerLayout->addStretch();
    
    // Symbol selection
    QLabel *symbolLabel = new QLabel("Symbol:");
    symbolLabel->setStyleSheet("QLabel { color: #FFFFFF; }");
    headerLayout->addWidget(symbolLabel);
    
    m_symbolCombo = new QComboBox();
    m_symbolCombo->addItems({"NIFTY", "BANKNIFTY", "FINNIFTY", "RELIANCE", "TCS", "INFY"});
    m_symbolCombo->setMinimumWidth(120);
    m_symbolCombo->setStyleSheet(
        "QComboBox { background: #2A3A50; color: #FFFFFF; border: 1px solid #3A4A60; padding: 5px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox::down-arrow { image: url(none); }"
    );
    headerLayout->addWidget(m_symbolCombo);
    
    // Expiry selection
    QLabel *expiryLabel = new QLabel("Expiry:");
    expiryLabel->setStyleSheet("QLabel { color: #FFFFFF; }");
    headerLayout->addWidget(expiryLabel);
    
    m_expiryCombo = new QComboBox();
    m_expiryCombo->setMinimumWidth(120);
    m_expiryCombo->setStyleSheet(
        "QComboBox { background: #2A3A50; color: #FFFFFF; border: 1px solid #3A4A60; padding: 5px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox::down-arrow { image: url(none); }"
    );
    headerLayout->addWidget(m_expiryCombo);
    
    // Refresh button
    m_refreshButton = new QPushButton("Refresh");
    m_refreshButton->setStyleSheet(
        "QPushButton { background: #2A5A80; color: #FFFFFF; border: none; padding: 6px 15px; border-radius: 3px; }"
        "QPushButton:hover { background: #3A6A90; }"
        "QPushButton:pressed { background: #1A4A70; }"
    );
    headerLayout->addWidget(m_refreshButton);
    
    // Calculator button
    m_calculatorButton = new QPushButton("View Calculators");
    m_calculatorButton->setStyleSheet(
        "QPushButton { background: #2A5A80; color: #FFFFFF; border: none; padding: 6px 15px; border-radius: 3px; }"
        "QPushButton:hover { background: #3A6A90; }"
        "QPushButton:pressed { background: #1A4A70; }"
    );
    headerLayout->addWidget(m_calculatorButton);
    
    mainLayout->addLayout(headerLayout);
    
    // ========================================================================
    // Table Section - Horizontal Splitter
    // ========================================================================
    QHBoxLayout *tableLayout = new QHBoxLayout();
    tableLayout->setSpacing(0);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    
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
    m_callTable->setStyleSheet(
        "QTableView {"
        "   background-color: #1E2A38;"
        "   color: #FFFFFF;"
        "   gridline-color: #2A3A50;"
        "   border: 1px solid #3A4A60;"
        "}"
        "QTableView::item {"
        "   padding: 5px;"
        "}"
        "QHeaderView::section {"
        "   background-color: #2A3A50;"
        "   color: #FFFFFF;"
        "   padding: 6px;"
        "   border: 1px solid #3A4A60;"
        "   font-weight: bold;"
        "}"
    );
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
    m_strikeTable->setStyleSheet(
        "QTableView {"
        "   background-color: #2A3A50;"
        "   color: #FFFF00;"
        "   gridline-color: #3A4A60;"
        "   border: 1px solid #3A4A60;"
        "   font-weight: bold;"
        "   font-size: 13px;"
        "}"
        "QTableView::item {"
        "   padding: 5px;"
        "}"
        "QHeaderView::section {"
        "   background-color: #3A4A60;"
        "   color: #FFFFFF;"
        "   padding: 6px;"
        "   border: 1px solid #4A5A70;"
        "   font-weight: bold;"
        "}"
    );
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
    m_putTable->setStyleSheet(
        "QTableView {"
        "   background-color: #1E2A38;"
        "   color: #FFFFFF;"
        "   gridline-color: #2A3A50;"
        "   border: 1px solid #3A4A60;"
        "}"
        "QTableView::item {"
        "   padding: 5px;"
        "}"
        "QHeaderView::section {"
        "   background-color: #2A3A50;"
        "   color: #FFFFFF;"
        "   padding: 6px;"
        "   border: 1px solid #3A4A60;"
        "   font-weight: bold;"
        "}"
    );
    tableLayout->addWidget(m_putTable, 4);
    
    mainLayout->addLayout(tableLayout);
    
    // Set main widget style
    setStyleSheet("QWidget { background-color: #1E2A38; }");
}

void OptionChainWindow::setupModels()
{
    // ========================================================================
    // Call Model
    // ========================================================================
    m_callModel = new QStandardItemModel(this);
    m_callModel->setColumnCount(CALL_COLUMN_COUNT);
    m_callModel->setHorizontalHeaderLabels({
        "", "OI", "Chng in OI", "Volume", "IV", "LTP", "Chng", "BID QTY", "BID", "ASK", "ASK QTY"
    });
    
    m_callTable->setModel(m_callModel);
    
    // Set column widths
    m_callTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_callTable->setColumnWidth(0, 30); // Checkbox column
    m_callTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    
    // Custom delegate for color coding
    m_callDelegate = new OptionChainDelegate(this);
    m_callTable->setItemDelegate(m_callDelegate);
    
    // ========================================================================
    // Strike Model
    // ========================================================================
    m_strikeModel = new QStandardItemModel(this);
    m_strikeModel->setColumnCount(1);
    m_strikeModel->setHorizontalHeaderLabels({"Strike"});
    
    m_strikeTable->setModel(m_strikeModel);
    m_strikeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    
    // ========================================================================
    // Put Model
    // ========================================================================
    m_putModel = new QStandardItemModel(this);
    m_putModel->setColumnCount(PUT_COLUMN_COUNT);
    m_putModel->setHorizontalHeaderLabels({
        "BID QTY", "BID", "ASK", "ASK QTY", "Chng", "LTP", "IV", "Volume", "Chng in OI", "OI", ""
    });
    
    m_putTable->setModel(m_putModel);
    
    // Set column widths
    m_putTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_putTable->setColumnWidth(PUT_COLUMN_COUNT - 1, 30); // Checkbox column
    m_putTable->horizontalHeader()->setSectionResizeMode(PUT_COLUMN_COUNT - 1, QHeaderView::Fixed);
    
    // Custom delegate for color coding
    m_putDelegate = new OptionChainDelegate(this);
    m_putTable->setItemDelegate(m_putDelegate);
}

void OptionChainWindow::setupConnections()
{
    // Header controls
    connect(m_symbolCombo, &QComboBox::currentTextChanged, 
            this, &OptionChainWindow::onSymbolChanged);
    connect(m_expiryCombo, &QComboBox::currentTextChanged,
            this, &OptionChainWindow::onExpiryChanged);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &OptionChainWindow::onRefreshClicked);
    connect(m_calculatorButton, &QPushButton::clicked,
            this, &OptionChainWindow::onCalculatorClicked);
    
    // Table interactions
    connect(m_callTable, &QTableView::clicked,
            this, &OptionChainWindow::onCallTableClicked);
    connect(m_putTable, &QTableView::clicked,
            this, &OptionChainWindow::onPutTableClicked);
    connect(m_strikeTable, &QTableView::clicked,
            this, &OptionChainWindow::onStrikeTableClicked);
    
    // Synchronize scroll bars - Only use strike table as master
    connect(m_strikeTable->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &OptionChainWindow::synchronizeScrollBars);
}

void OptionChainWindow::populateDemoData()
{
    // Set demo expiries
    QStringList expiries = {
        "26-DEC-2025", "02-JAN-2026", "09-JAN-2026", "16-JAN-2026",
        "23-JAN-2026", "30-JAN-2026"
    };
    m_expiryCombo->addItems(expiries);
    
    // Set current symbol and expiry
    m_currentSymbol = "NIFTY";
    m_currentExpiry = "26-DEC-2025";
    
    // Generate strikes (assuming NIFTY at 23,500)
    double spotPrice = 23500.0;
    m_atmStrike = std::round(spotPrice / 50.0) * 50.0; // Round to nearest 50
    
    // Generate strikes from -1000 to +1000 around ATM
    m_strikes.clear();
    for (double strike = m_atmStrike - 1000.0; strike <= m_atmStrike + 1000.0; strike += 50.0) {
        m_strikes.append(strike);
    }
    
    // Populate models with demo data
    for (double strike : m_strikes) {
        OptionStrikeData data;
        data.strikePrice = strike;
        
        // Generate realistic demo data based on distance from ATM
        double distanceFromATM = std::abs(strike - m_atmStrike);
        bool isOTM = (strike > m_atmStrike); // For calls
        
        // Call data (ITM has higher OI, OTM has lower)
        data.callOI = (int)(10000 + (isOTM ? -distanceFromATM : distanceFromATM) * 10);
        data.callChngInOI = (int)((rand() % 2000) - 1000);
        data.callVolume = (int)(rand() % 5000);
        data.callIV = 15.0 + (distanceFromATM / 100.0);
        data.callLTP = strike < spotPrice ? (spotPrice - strike) + 10.0 : 5.0 / (1.0 + distanceFromATM / 100.0);
        data.callChng = ((rand() % 200) - 100) / 10.0;
        data.callBidQty = (rand() % 1000) * 75;
        data.callBid = data.callLTP - 0.5;
        data.callAsk = data.callLTP + 0.5;
        data.callAskQty = (rand() % 1000) * 75;
        
        // Put data (ITM has higher OI, OTM has lower)
        isOTM = (strike < m_atmStrike); // For puts
        data.putOI = (int)(10000 + (isOTM ? -distanceFromATM : distanceFromATM) * 10);
        data.putChngInOI = (int)((rand() % 2000) - 1000);
        data.putVolume = (int)(rand() % 5000);
        data.putIV = 15.0 + (distanceFromATM / 100.0);
        data.putLTP = strike > spotPrice ? (strike - spotPrice) + 10.0 : 5.0 / (1.0 + distanceFromATM / 100.0);
        data.putChng = ((rand() % 200) - 100) / 10.0;
        data.putBidQty = (rand() % 1000) * 75;
        data.putBid = data.putLTP - 0.5;
        data.putAsk = data.putLTP + 0.5;
        data.putAskQty = (rand() % 1000) * 75;
        
        m_strikeData[strike] = data;
        
        // Add rows to models
        QList<QStandardItem*> callRow;
        
        // Checkbox column
        QStandardItem *callCheckbox = new QStandardItem("");
        callCheckbox->setCheckable(true);
        callCheckbox->setCheckState(Qt::Unchecked);
        callCheckbox->setTextAlignment(Qt::AlignCenter);
        callCheckbox->setEditable(false);
        callRow << callCheckbox;
        
        callRow << new QStandardItem(QString::number(data.callOI));
        callRow << new QStandardItem(QString::number(data.callChngInOI));
        callRow << new QStandardItem(QString::number(data.callVolume));
        callRow << new QStandardItem(QString::number(data.callIV, 'f', 2));
        callRow << new QStandardItem(QString::number(data.callLTP, 'f', 2));
        callRow << new QStandardItem(QString::number(data.callChng, 'f', 2));
        callRow << new QStandardItem(QString::number(data.callBidQty));
        callRow << new QStandardItem(QString::number(data.callBid, 'f', 2));
        callRow << new QStandardItem(QString::number(data.callAsk, 'f', 2));
        callRow << new QStandardItem(QString::number(data.callAskQty));
        
        for (int i = 1; i < callRow.size(); ++i) {
            callRow[i]->setTextAlignment(Qt::AlignCenter);
        }
        m_callModel->appendRow(callRow);
        
        // Strike row
        QStandardItem *strikeItem = new QStandardItem(QString::number(strike, 'f', 2));
        strikeItem->setTextAlignment(Qt::AlignCenter);
        m_strikeModel->appendRow(strikeItem);
        
        // Put row
        QList<QStandardItem*> putRow;
        putRow << new QStandardItem(QString::number(data.putBidQty));
        putRow << new QStandardItem(QString::number(data.putBid, 'f', 2));
        putRow << new QStandardItem(QString::number(data.putAsk, 'f', 2));
        putRow << new QStandardItem(QString::number(data.putAskQty));
        putRow << new QStandardItem(QString::number(data.putChng, 'f', 2));
        putRow << new QStandardItem(QString::number(data.putLTP, 'f', 2));
        putRow << new QStandardItem(QString::number(data.putIV, 'f', 2));
        putRow << new QStandardItem(QString::number(data.putVolume));
        putRow << new QStandardItem(QString::number(data.putChngInOI));
        putRow << new QStandardItem(QString::number(data.putOI));
        
        // Checkbox column
        QStandardItem *putCheckbox = new QStandardItem("");
        putCheckbox->setCheckable(true);
        putCheckbox->setCheckState(Qt::Unchecked);
        putCheckbox->setTextAlignment(Qt::AlignCenter);
        putCheckbox->setEditable(false);
        putRow << putCheckbox;
        
        for (int i = 0; i < putRow.size() - 1; ++i) {
            putRow[i]->setTextAlignment(Qt::AlignCenter);
        }
        m_putModel->appendRow(putRow);
    }
    
    highlightATMStrike();
}

void OptionChainWindow::setSymbol(const QString &symbol, const QString &expiry)
{
    m_currentSymbol = symbol;
    m_currentExpiry = expiry;
    
    m_symbolCombo->setCurrentText(symbol);
    m_expiryCombo->setCurrentText(expiry);
    m_titleLabel->setText(symbol);
    
    // Refresh data
    emit refreshRequested();
}

void OptionChainWindow::updateStrikeData(double strike, const OptionStrikeData &data)
{
    m_strikeData[strike] = data;
    
    // Find the row for this strike
    int row = m_strikes.indexOf(strike);
    if (row < 0) return;
    
    // Update call data
    m_callModel->item(row, CALL_OI)->setText(QString::number(data.callOI));
    m_callModel->item(row, CALL_CHNG_IN_OI)->setText(QString::number(data.callChngInOI));
    m_callModel->item(row, CALL_VOLUME)->setText(QString::number(data.callVolume));
    m_callModel->item(row, CALL_IV)->setText(QString::number(data.callIV, 'f', 2));
    m_callModel->item(row, CALL_LTP)->setText(QString::number(data.callLTP, 'f', 2));
    m_callModel->item(row, CALL_CHNG)->setText(QString::number(data.callChng, 'f', 2));
    m_callModel->item(row, CALL_BID_QTY)->setText(QString::number(data.callBidQty));
    m_callModel->item(row, CALL_BID)->setText(QString::number(data.callBid, 'f', 2));
    m_callModel->item(row, CALL_ASK)->setText(QString::number(data.callAsk, 'f', 2));
    m_callModel->item(row, CALL_ASK_QTY)->setText(QString::number(data.callAskQty));
    
    // Update put data
    m_putModel->item(row, PUT_BID_QTY)->setText(QString::number(data.putBidQty));
    m_putModel->item(row, PUT_BID)->setText(QString::number(data.putBid, 'f', 2));
    m_putModel->item(row, PUT_ASK)->setText(QString::number(data.putAsk, 'f', 2));
    m_putModel->item(row, PUT_ASK_QTY)->setText(QString::number(data.putAskQty));
    m_putModel->item(row, PUT_CHNG)->setText(QString::number(data.putChng, 'f', 2));
    m_putModel->item(row, PUT_LTP)->setText(QString::number(data.putLTP, 'f', 2));
    m_putModel->item(row, PUT_IV)->setText(QString::number(data.putIV, 'f', 2));
    m_putModel->item(row, PUT_VOLUME)->setText(QString::number(data.putVolume));
    m_putModel->item(row, PUT_CHNG_IN_OI)->setText(QString::number(data.putChngInOI));
    m_putModel->item(row, PUT_OI)->setText(QString::number(data.putOI));
}

void OptionChainWindow::clearData()
{
    m_callModel->removeRows(0, m_callModel->rowCount());
    m_strikeModel->removeRows(0, m_strikeModel->rowCount());
    m_putModel->removeRows(0, m_putModel->rowCount());
    m_strikeData.clear();
    m_strikes.clear();
}

void OptionChainWindow::setStrikeRange(double minStrike, double maxStrike, double interval)
{
    clearData();
    
    for (double strike = minStrike; strike <= maxStrike; strike += interval) {
        m_strikes.append(strike);
    }
}

void OptionChainWindow::setATMStrike(double atmStrike)
{
    m_atmStrike = atmStrike;
    highlightATMStrike();
}

void OptionChainWindow::onSymbolChanged(const QString &symbol)
{
    m_currentSymbol = symbol;
    m_titleLabel->setText(symbol);
    emit refreshRequested();
}

void OptionChainWindow::onExpiryChanged(const QString &expiry)
{
    m_currentExpiry = expiry;
    emit refreshRequested();
}

void OptionChainWindow::onRefreshClicked()
{
    emit refreshRequested();
}

void OptionChainWindow::onTradeClicked()
{
    // Determine which option is selected
    if (m_selectedCallRow >= 0) {
        double strike = getStrikeAtRow(m_selectedCallRow);
        emit tradeRequested(m_currentSymbol, m_currentExpiry, strike, "CE");
    } else if (m_selectedPutRow >= 0) {
        double strike = getStrikeAtRow(m_selectedPutRow);
        emit tradeRequested(m_currentSymbol, m_currentExpiry, strike, "PE");
    }
}

void OptionChainWindow::onCalculatorClicked()
{
    emit calculatorRequested(m_currentSymbol, m_currentExpiry, 0.0, "");
}

void OptionChainWindow::onCallTableClicked(const QModelIndex &index)
{
    int row = index.row();
    int col = index.column();
    
    // Handle checkbox column (column 0)
    if (col == 0) {
        QStandardItem *item = m_callModel->item(row, 0);
        if (item) {
            bool isChecked = item->checkState() == Qt::Checked;
            item->setCheckState(isChecked ? Qt::Unchecked : Qt::Checked);
        }
        return;
    }
    
    // Regular row click - select only call
    m_selectedCallRow = row;
    m_callTable->selectRow(row);
    m_putTable->clearSelection();
    
    qDebug() << "Call selected at strike:" << getStrikeAtRow(row);
}

void OptionChainWindow::onPutTableClicked(const QModelIndex &index)
{
    int row = index.row();
    int col = index.column();
    
    // Handle checkbox column (last column)
    if (col == PUT_COLUMN_COUNT - 1) {
        QStandardItem *item = m_putModel->item(row, PUT_COLUMN_COUNT - 1);
        if (item) {
            bool isChecked = item->checkState() == Qt::Checked;
            item->setCheckState(isChecked ? Qt::Unchecked : Qt::Checked);
        }
        return;
    }
    
    // Regular row click - select only put
    m_selectedPutRow = row;
    m_putTable->selectRow(row);
    m_callTable->clearSelection();
    
    qDebug() << "Put selected at strike:" << getStrikeAtRow(row);
}

void OptionChainWindow::onStrikeTableClicked(const QModelIndex &index)
{
    int row = index.row();
    
    // Strike click - select both call and put for the same strike
    m_selectedCallRow = row;
    m_selectedPutRow = row;
    
    m_callTable->selectRow(row);
    m_putTable->selectRow(row);
    m_strikeTable->selectRow(row);
    
    qDebug() << "Strike selected:" << getStrikeAtRow(row) << "- Both Call and Put selected";
}

bool OptionChainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Handle wheel events on all table viewports for synchronized scrolling
    if (event->type() == QEvent::Wheel) {
        if (obj == m_callTable->viewport() || 
            obj == m_putTable->viewport() || 
            obj == m_strikeTable->viewport()) {
            
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            
            // Get current scroll position
            int currentValue = m_strikeTable->verticalScrollBar()->value();
            int delta = wheelEvent->angleDelta().y();
            int step = m_strikeTable->verticalScrollBar()->singleStep();
            
            // Calculate new position
            int newValue = currentValue - (delta > 0 ? step : -step);
            
            // Apply to all tables
            m_strikeTable->verticalScrollBar()->setValue(newValue);
            m_callTable->verticalScrollBar()->setValue(newValue);
            m_putTable->verticalScrollBar()->setValue(newValue);
            
            return true; // Event handled
        }
    }
    
    return QWidget::eventFilter(obj, event);
}

void OptionChainWindow::synchronizeScrollBars(int value)
{
    // Strike table is the master - sync call and put tables to it
    // No need to block signals since only strike table emits
    m_callTable->verticalScrollBar()->setValue(value);
    m_putTable->verticalScrollBar()->setValue(value);
}

void OptionChainWindow::highlightATMStrike()
{
    // Find ATM row
    int atmRow = -1;
    for (int i = 0; i < m_strikes.size(); ++i) {
        if (m_strikes[i] == m_atmStrike) {
            atmRow = i;
            break;
        }
    }
    
    if (atmRow < 0) return;
    
    // Highlight the ATM strike row with different background
    QColor atmColor("#3A5A70");
    
    for (int col = 0; col < m_callModel->columnCount(); ++col) {
        QStandardItem *item = m_callModel->item(atmRow, col);
        if (item) {
            item->setBackground(atmColor);
        }
    }
    
    QStandardItem *strikeItem = m_strikeModel->item(atmRow, 0);
    if (strikeItem) {
        strikeItem->setBackground(QColor("#4A6A80"));
        strikeItem->setForeground(QColor("#FFFF00"));
    }
    
    for (int col = 0; col < m_putModel->columnCount(); ++col) {
        QStandardItem *item = m_putModel->item(atmRow, col);
        if (item) {
            item->setBackground(atmColor);
        }
    }
}

void OptionChainWindow::updateTableColors()
{
    // This method can be called after data updates to refresh colors
    m_callTable->viewport()->update();
    m_putTable->viewport()->update();
}

QModelIndex OptionChainWindow::getStrikeIndex(int row) const
{
    return m_strikeModel->index(row, 0);
}

double OptionChainWindow::getStrikeAtRow(int row) const
{
    if (row >= 0 && row < m_strikes.size()) {
        return m_strikes[row];
    }
    return 0.0;
}
