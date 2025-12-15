#include "app/ScripBar.h"
#include "api/XTSMarketDataClient.h"
#include "repository/RepositoryManager.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QShortcut>
#include <QKeySequence>

ScripBar::ScripBar(QWidget *parent)
    : QWidget(parent)
    , m_xtsClient(nullptr)
{
    setupUI();
    populateExchanges();
    setupShortcuts();
}

ScripBar::~ScripBar() = default;

void ScripBar::setXTSClient(XTSMarketDataClient *client)
{
    m_xtsClient = client;
    qDebug() << "XTS client set for ScripBar";
}

void ScripBar::setupUI()
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 2, 4, 2);
    m_layout->setSpacing(6);
    m_layout->setAlignment(Qt::AlignVCenter);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMaximumHeight(28);

    // Exchange combo (custom)
    m_exchangeCombo = new CustomScripComboBox(this);
    m_exchangeCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);
    m_exchangeCombo->setMinimumWidth(56);
    m_exchangeCombo->setObjectName("cbExchange");  // For keyboard shortcut
    m_layout->addWidget(m_exchangeCombo);
    connect(m_exchangeCombo, &CustomScripComboBox::itemSelected, this, &ScripBar::onExchangeChanged);

    // Segment combo (custom)
    m_segmentCombo = new CustomScripComboBox(this);
    m_segmentCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);
    m_segmentCombo->setMinimumWidth(48);
    m_layout->addWidget(m_segmentCombo);
    connect(m_segmentCombo, &CustomScripComboBox::itemSelected, this, &ScripBar::onSegmentChanged);

    // Instrument combo (custom)
    m_instrumentCombo = new CustomScripComboBox(this);
    m_instrumentCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);
    m_instrumentCombo->setMinimumWidth(80);
    m_layout->addWidget(m_instrumentCombo);
    connect(m_instrumentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onInstrumentChanged);

    // Symbol combo (custom)
    m_symbolCombo = new CustomScripComboBox(this);
    m_symbolCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);
    m_symbolCombo->setMinimumWidth(100);
    m_layout->addWidget(m_symbolCombo);
    connect(m_symbolCombo, &CustomScripComboBox::itemSelected, this, &ScripBar::onSymbolChanged);

    // Expiry combo (custom with date sorting)
    m_expiryCombo = new CustomScripComboBox(this);
    m_expiryCombo->setSortMode(CustomScripComboBox::ChronologicalSort);
    m_expiryCombo->setMinimumWidth(80);
    m_layout->addWidget(m_expiryCombo);
    connect(m_expiryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onExpiryChanged);

    // Strike combo (custom with numeric sorting)
    m_strikeCombo = new CustomScripComboBox(this);
    m_strikeCombo->setSortMode(CustomScripComboBox::NumericSort);
    m_strikeCombo->setMinimumWidth(64);
    m_layout->addWidget(m_strikeCombo);
    connect(m_strikeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onStrikeChanged);

    // Option Type combo (custom)
    m_optionTypeCombo = new CustomScripComboBox(this);
    m_optionTypeCombo->setSortMode(CustomScripComboBox::NoSort);
    m_optionTypeCombo->setMinimumWidth(48);
    m_layout->addWidget(m_optionTypeCombo);
    connect(m_optionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onOptionTypeChanged);

    // Add to Watch button
    m_addToWatchButton = new QPushButton(tr("Add"), this);
    m_addToWatchButton->setMinimumWidth(64);
    m_layout->addWidget(m_addToWatchButton);
    connect(m_addToWatchButton, &QPushButton::clicked, this, &ScripBar::onAddToWatchClicked);

    // Connect Enter key (when dropdown closed) from all comboboxes to trigger Add button
    connect(m_exchangeCombo, &CustomScripComboBox::enterPressedWhenClosed,
            m_addToWatchButton, &QPushButton::click);
    connect(m_segmentCombo, &CustomScripComboBox::enterPressedWhenClosed,
            m_addToWatchButton, &QPushButton::click);
    connect(m_instrumentCombo, &CustomScripComboBox::enterPressedWhenClosed,
            m_addToWatchButton, &QPushButton::click);
    connect(m_symbolCombo, &CustomScripComboBox::enterPressedWhenClosed,
            m_addToWatchButton, &QPushButton::click);
    connect(m_expiryCombo, &CustomScripComboBox::enterPressedWhenClosed,
            m_addToWatchButton, &QPushButton::click);
    connect(m_strikeCombo, &CustomScripComboBox::enterPressedWhenClosed,
            m_addToWatchButton, &QPushButton::click);
    connect(m_optionTypeCombo, &CustomScripComboBox::enterPressedWhenClosed,
            m_addToWatchButton, &QPushButton::click);

    m_layout->addStretch();

    // Compact flat styling (matches existing toolbar)
    setStyleSheet(
        "QWidget { background-color: #2d2d30; }"
        "QToolButton, QComboBox, QPushButton { color: #ffffff; background: #1e1e1e; border: none; padding: 2px 6px; }"
        "QComboBox QAbstractItemView { background: #1e1e1e; color: #ffffff; selection-background-color: #094771; }"
        "QPushButton { background: #0e639c; border-radius: 2px; }");
}

void ScripBar::populateExchanges()
{
    m_exchangeCombo->clearItems();
    QStringList exchanges = {"NSE", "BSE", "MCX"};
    m_exchangeCombo->addItems(exchanges);

    // Set default
    m_currentExchange = "NSE";
    if (m_exchangeCombo->lineEdit()) {
        m_exchangeCombo->lineEdit()->setText(m_currentExchange);
    }
    populateSegments(m_currentExchange);
}

void ScripBar::populateSegments(const QString &exchange)
{
    m_segmentCombo->clearItems();
    QStringList segs;
    if (exchange == "NSE")
        segs << "CM" << "FO" << "CD";
    else if (exchange == "BSE")
        segs << "CM" << "FO";
    else if (exchange == "MCX")
        segs << "FO";
    else
        segs << "CM";

    m_segmentCombo->addItems(segs);

    // Set default
    m_currentSegment = segs.isEmpty() ? QString() : segs.first();
    if (m_segmentCombo->lineEdit()) {
        m_segmentCombo->lineEdit()->setText(m_currentSegment);
    }
    populateInstruments(m_currentSegment);
}

void ScripBar::populateInstruments(const QString &segment)
{
    m_instrumentCombo->clearItems();
    QStringList instruments;
    if (segment == "CM")
        instruments << "EQUITY";
    else if (segment == "FO")
        instruments << "FUTIDX" << "FUTSTK" << "OPTIDX" << "OPTSTK";
    else if (segment == "CD")
        instruments << "FUTCUR" << "OPTCUR";
    else
        instruments << "EQUITY";

    m_instrumentCombo->addItems(instruments);

    // Set default
    if (!instruments.isEmpty() && m_instrumentCombo->lineEdit()) {
        m_instrumentCombo->lineEdit()->setText(instruments.first());
    }
    onInstrumentChanged(0);
}

void ScripBar::populateSymbols(const QString &instrument)
{
    m_symbolCombo->clearItems();
    
    // Use RepositoryManager for array-based search (NO API CALLS)
    RepositoryManager* repo = RepositoryManager::getInstance();
    
    if (!repo->isLoaded()) {
        qWarning() << "[ScripBar] Repository not loaded yet";
        m_symbolCombo->addItem("Loading master data...");
        return;
    }
    
    QString exchange = getCurrentExchange();
    QString segment = getCurrentSegment();
    
    qDebug() << "[ScripBar] Array-based search:" << exchange << segment << instrument;
    
    // Get all scrips for this segment and series
    QVector<ContractData> contracts = repo->getScrips(exchange, segment, instrument);
    
    qDebug() << "[ScripBar] Found" << contracts.size() << "contracts in local array";
    
    if (contracts.isEmpty()) {
        m_symbolCombo->addItem("No instruments found");
        return;
    }
    
    // Extract unique symbols
    QSet<QString> uniqueSymbols;
    for (const ContractData& contract : contracts) {
        if (!contract.name.isEmpty()) {
            uniqueSymbols.insert(contract.name);
        }
    }
    
    // Convert to sorted list
    QStringList symbols = uniqueSymbols.values();
    symbols.sort();
    
    qDebug() << "[ScripBar] Found" << symbols.size() << "unique symbols";
    
    // Update cache with full contracts for later use
    m_instrumentCache.clear();
    for (const ContractData& contract : contracts) {
        InstrumentData inst;
        inst.exchangeInstrumentID = contract.exchangeInstrumentID;
        inst.name = contract.name;
        inst.symbol = contract.name;
        inst.series = contract.series;
        inst.instrumentType = contract.series;
        inst.expiryDate = contract.expiryDate;
        inst.strikePrice = contract.strikePrice;
        inst.optionType = contract.optionType;
        inst.exchangeSegment = RepositoryManager::getExchangeSegmentID(exchange, segment);
        m_instrumentCache.append(inst);
    }
    
    // Populate combo box
    m_symbolCombo->addItems(symbols);
    
    if (m_symbolCombo->count() > 0) {
        onSymbolChanged(m_symbolCombo->currentText());
    }
}

void ScripBar::populateExpiries(const QString &symbol)
{
    m_expiryCombo->clearItems();
    
    // TODO: Parse expiry dates from instrument cache based on symbol
    // For now, check if we have cached instruments with this symbol
    QStringList expiries;
    for (const auto &inst : m_instrumentCache) {
        if (inst.symbol == symbol && !inst.expiryDate.isEmpty()) {
            if (!expiries.contains(inst.expiryDate)) {
                expiries.append(inst.expiryDate);
            }
        }
    }
    
    if (expiries.isEmpty()) {
        // For equity or if no expiry data, add N/A
        expiries << "N/A";
    }
    
    m_expiryCombo->addItems(expiries);
    
    if (m_expiryCombo->count() > 0) {
        onExpiryChanged(0);
    }
}

void ScripBar::populateStrikes(const QString &expiry)
{
    m_strikeCombo->clearItems();
    
    // TODO: Parse strike prices from instrument cache based on symbol + expiry
    QString currentSymbol = m_symbolCombo->currentText();
    QStringList strikes;
    
    for (const auto &inst : m_instrumentCache) {
        if (inst.symbol == currentSymbol && inst.expiryDate == expiry && inst.strikePrice > 0) {
            QString strikeStr = QString::number(inst.strikePrice, 'f', 2);
            if (!strikes.contains(strikeStr)) {
                strikes.append(strikeStr);
            }
        }
    }
    
    if (strikes.isEmpty()) {
        // For equity/futures or if no strike data, add N/A
        strikes << "N/A";
    }
    
    m_strikeCombo->addItems(strikes);
    
    if (m_strikeCombo->count() > 0) {
        onStrikeChanged(0);
    }
}

void ScripBar::populateOptionTypes(const QString &strike)
{
    Q_UNUSED(strike)
    m_optionTypeCombo->clearItems();
    QStringList types = {"CE", "PE"};
    m_optionTypeCombo->addItems(types);

    // Set default
    if (m_optionTypeCombo->lineEdit()) {
        m_optionTypeCombo->lineEdit()->setText("CE");
    }
}

// Slot implementations
void ScripBar::onExchangeChanged(const QString &text)
{
    m_currentExchange = text;
    populateSegments(text);
}

void ScripBar::onSegmentChanged(const QString &text)
{
    m_currentSegment = text;
    populateInstruments(text);
}

void ScripBar::onInstrumentChanged(int index)
{
    Q_UNUSED(index)
    QString inst = m_instrumentCombo->currentText();
    populateSymbols(inst);
}

void ScripBar::onSymbolChanged(const QString &text)
{
    populateExpiries(text);
}

void ScripBar::onExpiryChanged(int index)
{
    Q_UNUSED(index)
    populateStrikes(m_expiryCombo->currentText());
}

void ScripBar::onStrikeChanged(int index)
{
    Q_UNUSED(index)
    populateOptionTypes(m_strikeCombo->currentText());
}

void ScripBar::onOptionTypeChanged(int index)
{
    Q_UNUSED(index)
    // No cascading needed after option type
}

void ScripBar::onAddToWatchClicked()
{
    QString exchange = m_exchangeCombo->currentText();
    QString segment = m_segmentCombo->currentText();
    QString instrument = m_instrumentCombo->currentText();
    QString symbol = m_symbolCombo->currentText();
    QString expiry = m_expiryCombo->currentText();
    QString strike = m_strikeCombo->currentText();
    QString optionType = m_optionTypeCombo->currentText();
    
    emit addToWatchRequested(exchange, segment, instrument, symbol, expiry, strike, optionType);
    qDebug() << "Add to watch requested:" << symbol << expiry << strike << optionType;
}

void ScripBar::setupShortcuts()
{
    // Ctrl+S (Windows/Linux) or Cmd+S (Mac) to focus Exchange combobox
    QShortcut *focusExchangeShortcut = new QShortcut(QKeySequence("Ctrl+S"), this);
    connect(focusExchangeShortcut, &QShortcut::activated, this, [this]() {
        qDebug() << "[ScripBar] Shortcut Ctrl+S activated - focusing Exchange";
        if (m_exchangeCombo) {
            m_exchangeCombo->setFocus();
            m_exchangeCombo->selectAllText();
            qDebug() << "[ScripBar] Exchange focused";
        }
    });

    qDebug() << "[ScripBar] Keyboard shortcut registered: Ctrl+S (or Cmd+S on Mac) -> Focus Exchange";
}

QString ScripBar::getCurrentExchange() const
{
    return m_currentExchange;
}

QString ScripBar::getCurrentSegment() const
{
    return m_currentSegment;
}

int ScripBar::getCurrentExchangeSegmentCode() const
{
    // Map to XTS exchange segment codes
    QString key = m_currentExchange + "_" + m_currentSegment;
    
    static QMap<QString, int> segmentMap = {
        {"NSE_CM", 1},   // NSECM
        {"NSE_FO", 2},   // NSEFO
        {"NSE_CD", 13},  // NSECD
        {"BSE_CM", 11},  // BSECM
        {"BSE_FO", 12},  // BSEFO
        {"BSE_CD", 61},  // BSECD
        {"MCX_FO", 51}   // MCXFO
    };
    
    return segmentMap.value(key, 1); // Default to NSECM
}

void ScripBar::searchInstrumentsAsync(const QString &searchText)
{
    if (!m_xtsClient || !m_xtsClient->isLoggedIn()) {
        qWarning() << "Cannot search: XTS client not available or not logged in";
        return;
    }
    
    int exchangeSegment = getCurrentExchangeSegmentCode();
    
    // Call XTS search API
    m_xtsClient->searchInstruments(searchText, exchangeSegment,
        [this](bool success, const QVector<XTS::Instrument> &instruments, const QString &error) {
            if (success) {
                // Convert XTS instruments to our InstrumentData format
                QVector<InstrumentData> data;
                for (const auto &inst : instruments) {
                    InstrumentData item;
                    item.exchangeInstrumentID = inst.exchangeInstrumentID;
                    item.name = inst.instrumentName;
                    item.symbol = inst.instrumentName;
                    item.series = inst.series;
                    item.exchangeSegment = inst.exchangeSegment;
                    // TODO: Parse instrument type, expiry, strike, option type from name
                    data.append(item);
                }
                onInstrumentsReceived(data);
            } else {
                qWarning() << "Instrument search failed:" << error;
            }
        }
    );
}

void ScripBar::onInstrumentsReceived(const QVector<InstrumentData> &instruments)
{
    m_instrumentCache = instruments;
    
    // Extract unique symbols and populate symbol combo
    QStringList symbols;
    for (const auto &inst : instruments) {
        if (!symbols.contains(inst.symbol)) {
            symbols.append(inst.symbol);
        }
    }
    
    m_symbolCombo->clearItems();
    m_symbolCombo->addItems(symbols);
    
    qDebug() << "Received" << instruments.size() << "instruments," << symbols.size() << "unique symbols";
}
