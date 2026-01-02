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
    setMaximumHeight(40);

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

    // BSE Scrip Code combo (only visible for BSE + E segment)
    m_bseScripCodeCombo = new CustomScripComboBox(this);
    m_bseScripCodeCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);
    m_bseScripCodeCombo->setMinimumWidth(120);
    m_bseScripCodeCombo->setMaximumWidth(150);
    m_layout->addWidget(m_bseScripCodeCombo);
    m_bseScripCodeCombo->setVisible(false);  // Hidden by default
    connect(m_bseScripCodeCombo, &CustomScripComboBox::itemSelected, this, &ScripBar::onBseScripCodeChanged);

    // Symbol combo (custom)
    m_symbolCombo = new CustomScripComboBox(this);
    m_symbolCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);
    m_symbolCombo->setMinimumWidth(100);
    m_layout->addWidget(m_symbolCombo);
    connect(m_symbolCombo, &CustomScripComboBox::itemSelected, this, &ScripBar::onSymbolChanged);

    // Expiry combo (custom with date sorting)
    m_expiryCombo = new CustomScripComboBox(this);
    m_expiryCombo->setSortMode(CustomScripComboBox::ChronologicalSort);
    m_expiryCombo->setMinimumWidth(125);
    m_layout->addWidget(m_expiryCombo);
    connect(m_expiryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onExpiryChanged);

    // Strike combo (custom with numeric sorting)
    m_strikeCombo = new CustomScripComboBox(this);
    m_strikeCombo->setSortMode(CustomScripComboBox::NumericSort);
    m_strikeCombo->setMinimumWidth(140);
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

    // Token line edit (read-only display of selected token)
    m_tokenEdit = new QLineEdit(this);
    m_tokenEdit->setMinimumWidth(80);
    m_tokenEdit->setMaximumWidth(120);
    m_tokenEdit->setPlaceholderText("Token");
    m_tokenEdit->setReadOnly(true);
    m_tokenEdit->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(m_tokenEdit);

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

    // Dark premium styling 
    setStyleSheet(
        "QWidget { background-color: #2d2d30; border: none; }"
        "CustomScripComboBox { color: #d4d4d8; background: #3e3e42; border: 1px solid #454545; padding: 2px 4px; border-radius: 4px; }"
        "CustomScripComboBox:hover { border: 1px solid #007acc; }"
        // "CustomScripComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 20px; border-left: 1px solid #454545; background: #3e3e42; }"
        // "CustomScripComboBox::down-arrow { image: url(none); width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 6px solid #d4d4d8; margin-right: 6px; }"
        // "CustomScripComboBox::down-arrow:hover { border-top-color: #007acc; }"
        "CustomScripComboBox QAbstractItemView { background: #252526; color: #d4d4d8; selection-background-color: #094771; selection-color: #ffffff; outline: none; border: 1px solid #454545; }"
        "QPushButton { background: #007acc; color: #ffffff; border: none; border-radius: 4px; font-weight: bold; padding: 4px 12px; }"
        "QPushButton:hover { background: #0062a3; }"
        "QPushButton:pressed { background: #004d80; }"
        "QLineEdit { background: #3c3c3c; color: #d4d4d8; border: 1px solid #454545; border-radius: 4px; padding: 2px 6px; }"
        "QLabel { color: #cccccc; }"
    );
}

// future implementation 
// exchange and segment should be populated from client profile api so if client has nsecm or nsefo then nse should be populated in exchange so on and so forth 
 



void ScripBar::populateExchanges()
{
    m_exchangeCombo->clearItems();
    QStringList exchanges = {"NSE", "NSECDS", "BSE", "MCX"};
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
        segs << "E" << "F" << "O";  // E=NSECM, F=NSEFO, O=NSEFO
    else if (exchange == "NSECDS")
        segs << "F" << "O";  // F=NSECD, O=NSECD
    else if (exchange == "BSE")
        segs << "E" << "F" << "O";
    else if (exchange == "MCX")
        segs << "F" << "O";  // F=MCX, O=MCX
    else
        segs << "E";

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
    
    QString exchange = getCurrentExchange();
    
 // 
    if (segment == "E") {
        instruments << "EQUITY";
    } else if (segment == "F") {
        if (exchange == "NSE") {
            instruments << "FUTIDX" << "FUTSTK";
        } else if (exchange == "NSECDS") {
            instruments << "FUTCUR";
        } else if (exchange == "MCX") {
            instruments << "FUTCOM";
        } else if (exchange == "BSE") {
            instruments << "FUTIDX" << "FUTSTK";
        } else {
            instruments << "FUTIDX" << "FUTSTK";
        }
    } else if (segment == "O") {
        if (exchange == "NSE") {
            instruments << "OPTIDX" << "OPTSTK";
        } else if (exchange == "NSECDS") {
            instruments << "OPTCUR";
        } else if (exchange == "MCX") {
            instruments << "OPTFUT";
        } else if (exchange == "BSE") {
            instruments << "OPTIDX" << "OPTSTK";
        } else {
            instruments << "OPTIDX" << "OPTSTK";
        }
    } else {
        instruments << "EQUITY";
    }

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
    
    // Map instrument dropdown value to ContractData series field
    QString seriesFilter = mapInstrumentToSeries(instrument);
    
    qDebug() << "[ScripBar] ========== populateSymbols DEBUG ==========";
    qDebug() << "[ScripBar] Array-based search:" << exchange << segment << "instrument:" << instrument << "-> series:" << seriesFilter;
    
    // Get all scrips for this segment and series
    QVector<ContractData> contracts = repo->getScrips(exchange, segment, seriesFilter);
    
    qDebug() << "[ScripBar] Found" << contracts.size() << "contracts in local array";
    
    // Debug: Show first 3 contracts from repository
    if (contracts.size() > 0) {
        qDebug() << "[ScripBar] First 3 contracts from repository:";
        for (int i = 0; i < qMin(3, contracts.size()); i++) {
            qDebug() << "  [" << i << "] Token:" << contracts[i].exchangeInstrumentID 
                     << "Name:'" << contracts[i].name << "' Series:'" << contracts[i].series << "'"
                     << "Expiry:'" << contracts[i].expiryDate << "' Strike:" << contracts[i].strikePrice 
                     << "Opt:'" << contracts[i].optionType << "'";
        }
    }
    
    if (contracts.isEmpty()) {
        qDebug() << "[ScripBar] ❌ NO contracts found - adding placeholder";
        m_symbolCombo->addItem("No instruments found");
        return;
    }
    
    // Pre-allocate cache for better performance
    m_instrumentCache.clear();
    m_instrumentCache.reserve(contracts.size());
    
    // Extract unique symbols and build cache in single pass
    QSet<QString> uniqueSymbols;
    int exchangeSegmentId = RepositoryManager::getExchangeSegmentID(exchange, segment);
    
    for (const ContractData& contract : contracts) {
        // Filter out dummy/test symbols
        QString upperName = contract.name.toUpper();
        if (upperName.contains("DUMMY") || upperName.contains("TEST") || 
            upperName == "SCRIP" || upperName.startsWith("ZZZ")) {
            continue;
        }
        
        // FACT-BASED: Filter out spread contracts using instrumentType field (4 = Spread)
        if (contract.instrumentType == 4) {
            continue;
        }

        if (!contract.name.isEmpty()) {
            uniqueSymbols.insert(contract.name);
        }
        
        InstrumentData inst;
        inst.exchangeInstrumentID = contract.exchangeInstrumentID;
        inst.name = contract.name;
        inst.symbol = contract.name;
        inst.series = contract.series;
        inst.instrumentType = QString::number(contract.instrumentType);  // Store numeric instrumentType (1=Future, 2=Option, 4=Spread)
        inst.expiryDate = contract.expiryDate;
        inst.strikePrice = contract.strikePrice;
        inst.optionType = contract.optionType;
        inst.exchangeSegment = exchangeSegmentId;
        inst.scripCode = contract.scripCode;  // Store BSE scrip code
        m_instrumentCache.append(inst);
    }
    
    // Convert to sorted list
    QStringList symbols = uniqueSymbols.values();
    symbols.sort();
    
    qDebug() << "[ScripBar] Found" << symbols.size() << "unique symbols from" << contracts.size() << "contracts";
    qDebug() << "[ScripBar] Cache now has" << m_instrumentCache.size() << "entries";
    qDebug() << "[ScripBar] ============================================";
    
    // Update BSE scrip code visibility
    updateBseScripCodeVisibility();
    
    // Populate combo box with all symbols at once (prevents UI freeze)
    m_symbolCombo->addItems(symbols);
    
    if (m_symbolCombo->count() > 0) {
        m_symbolCombo->setCurrentIndex(0);
        onSymbolChanged(m_symbolCombo->currentText());
    }
}

void ScripBar::populateExpiries(const QString &symbol)
{
    m_expiryCombo->clearItems();
    
    QString currentInstrument = m_instrumentCombo->currentText();
    bool isEquity = (currentInstrument == "EQUITY");
    
    // For equity, skip expiry logic entirely
    if (isEquity) {
        updateTokenDisplay();
        return;
    }
    
    // For F&O instruments, extract expiry dates from cache
    QStringList expiries;
    for (const auto &inst : m_instrumentCache) {
        if (inst.symbol == symbol && !inst.expiryDate.isEmpty() && inst.expiryDate != "N/A") {
            if (!expiries.contains(inst.expiryDate)) {
                expiries.append(inst.expiryDate);
            }
        }
    }
    
    if (expiries.isEmpty()) {
        expiries << "N/A";
    }
    
    qDebug() << "[ScripBar] populateExpiries: Found" << expiries.size() << "expiry dates for symbol" << symbol;
    
    m_expiryCombo->addItems(expiries);
    
    if (m_expiryCombo->count() > 0) {
        m_expiryCombo->setCurrentIndex(0);
        onExpiryChanged(0);
    }
}

void ScripBar::populateStrikes(const QString &expiry)
{
    m_strikeCombo->clearItems();
    
    QString currentInstrument = m_instrumentCombo->currentText();
    bool isFuture = (currentInstrument == "FUTIDX" || currentInstrument == "FUTSTK" || currentInstrument == "FUTCUR");
    bool isOption = (currentInstrument == "OPTIDX" || currentInstrument == "OPTSTK" || currentInstrument == "OPTCUR");
    
    // For futures or equity, skip strike logic and update token directly
    if (isFuture || !isOption) {
        updateTokenDisplay();
        return;
    }
    
    // For options, extract strike prices from cache
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
        strikes << "N/A";
    }
    
    qDebug() << "[ScripBar] populateStrikes: Found" << strikes.size() << "strikes for expiry" << expiry;
    
    m_strikeCombo->addItems(strikes);
    
    if (m_strikeCombo->count() > 0) {
        m_strikeCombo->setCurrentIndex(0);
        onStrikeChanged(0);
    }
}

void ScripBar::populateOptionTypes(const QString &strike)
{
    Q_UNUSED(strike)
    
    QString currentInstrument = m_instrumentCombo->currentText();
    bool isOption = (currentInstrument == "OPTIDX" || currentInstrument == "OPTSTK" || currentInstrument == "OPTCUR");
    
    if (!isOption) {
        // For non-options, skip this and update token
        updateTokenDisplay();
        return;
    }
    
    m_optionTypeCombo->clearItems();
    QStringList types = {"CE", "PE"};
    m_optionTypeCombo->addItems(types);

    // Set default
    if (m_optionTypeCombo->lineEdit()) {
        m_optionTypeCombo->lineEdit()->setText("CE");
    }
    
    updateTokenDisplay();
}

// Slot implementations
void ScripBar::onExchangeChanged(const QString &text)
{
    m_currentExchange = text;
    populateSegments(text);
    updateBseScripCodeVisibility();
}

void ScripBar::onSegmentChanged(const QString &text)
{
    m_currentSegment = text;
    populateInstruments(text);
    updateBseScripCodeVisibility();
}

void ScripBar::onInstrumentChanged(int index)
{
    Q_UNUSED(index)
    QString inst = m_instrumentCombo->currentText();
    
    // Show/hide expiry, strike, option type based on instrument
    bool isFuture = (inst == "FUTIDX" || inst == "FUTSTK" || inst == "FUTCUR");
    bool isOption = (inst == "OPTIDX" || inst == "OPTSTK" || inst == "OPTCUR");
    bool isEquity = (inst == "EQUITY");
    
    // Expiry: Show for futures and options
    m_expiryCombo->setVisible(isFuture || isOption);
    
    // Strike: Show for options only
    m_strikeCombo->setVisible(isOption);
    
    // Option Type: Show for options only
    m_optionTypeCombo->setVisible(isOption);
    
    populateSymbols(inst);
}

void ScripBar::onSymbolChanged(const QString &text)
{
    populateExpiries(text);
    updateTokenDisplay();
}

void ScripBar::onExpiryChanged(int index)
{
    Q_UNUSED(index)
    QString expiry = m_expiryCombo->currentText();
    qDebug() << "[ScripBar] onExpiryChanged: expiry =" << expiry << "(index" << index << ")";
    populateStrikes(expiry);
    
    // For futures (no strikes), update token display now
    QString currentInstrument = m_instrumentCombo->currentText();
    bool isFuture = (currentInstrument == "FUTIDX" || currentInstrument == "FUTSTK" || currentInstrument == "FUTCUR");
    if (isFuture) {
        qDebug() << "[ScripBar] Future instrument - updating token display after expiry change";
        updateTokenDisplay();
    }
}

void ScripBar::onStrikeChanged(int index)
{
    Q_UNUSED(index)
    QString strike = m_strikeCombo->currentText();
    qDebug() << "[ScripBar] onStrikeChanged: strike =" << strike << "(index" << index << ")";
    populateOptionTypes(strike);
}

void ScripBar::onOptionTypeChanged(int index)
{
    Q_UNUSED(index)
    QString optionType = m_optionTypeCombo->currentText();
    qDebug() << "[ScripBar] onOptionTypeChanged: optionType =" << optionType << "(index" << index << ")";
    updateTokenDisplay();
}

void ScripBar::onAddToWatchClicked()
{
    InstrumentData inst = getCurrentInstrument();
    emit addToWatchRequested(inst);
    qDebug() << "Add to watch requested:" << inst.symbol << inst.expiryDate << inst.strikePrice 
             << inst.optionType << "token:" << inst.exchangeInstrumentID;
}

void ScripBar::refreshSymbols()
{
    // Re-populate symbols based on current selections
    QString currentInstrument = m_instrumentCombo->currentText();
    if (!currentInstrument.isEmpty()) {
        qDebug() << "[ScripBar] Refreshing symbols for instrument:" << currentInstrument;
        populateSymbols(currentInstrument);
    }
}

void ScripBar::updateBseScripCodeVisibility()
{
    // Show BSE Scrip Code field only when exchange=BSE and segment=E
    bool showBseCode = (m_currentExchange == "BSE" && m_currentSegment == "E");
    m_bseScripCodeCombo->setVisible(showBseCode);
    
    // Populate BSE scrip codes when visible
    if (showBseCode) {
        populateBseScripCodes();
    } else {
        m_bseScripCodeCombo->clearItems();
    }
}

InstrumentData ScripBar::getCurrentInstrument() const
{
    InstrumentData result;
    
    QString symbol = m_symbolCombo->currentText();
    QString currentInstrument = m_instrumentCombo->currentText();
    QString expiry = m_expiryCombo->isVisible() ? m_expiryCombo->currentText() : "N/A";
    QString strike = m_strikeCombo->isVisible() ? m_strikeCombo->currentText() : "N/A";
    QString optionType = m_optionTypeCombo->isVisible() ? m_optionTypeCombo->currentText() : "";
    
    // Normalize empty strings to "N/A" for consistent matching
    if (expiry.isEmpty()) expiry = "N/A";
    if (strike.isEmpty()) strike = "N/A";
    
    bool isEquity = (currentInstrument == "EQUITY");
    bool isFuture = (currentInstrument == "FUTIDX" || currentInstrument == "FUTSTK" || currentInstrument == "FUTCUR");
    bool isOption = (currentInstrument == "OPTIDX" || currentInstrument == "OPTSTK" || currentInstrument == "OPTCUR");
    
    // For options, if any required field is missing/empty, skip cache search and use token
    if (isOption && (expiry == "N/A" || strike == "N/A" || optionType.isEmpty())) {
        qDebug() << "[ScripBar] getCurrentInstrument: Incomplete option selection, using token";
        qint64 tokenFromDisplay = m_tokenEdit->text().toLongLong();
        if (tokenFromDisplay > 0) {
            for (const auto &inst : m_instrumentCache) {
                if (inst.exchangeInstrumentID == tokenFromDisplay) {
                    return inst;
                }
            }
        }
    }
    
    // Search in instrument cache for exact match
    for (const auto &inst : m_instrumentCache) {
        bool matchSymbol = (inst.symbol == symbol);
        
        // For equity: only match symbol
        if (isEquity && matchSymbol) {
            return inst;
        }
        
        // For futures: match symbol + expiry (exclude spreads)
        if (isFuture && matchSymbol) {
            bool matchExpiry = (expiry == "N/A" || inst.expiryDate == expiry);
            bool isSpread = inst.name.contains("SPD", Qt::CaseInsensitive);
            
            if (matchExpiry && !isSpread) {
                return inst;
            }
        }
        
        // For options: match symbol + expiry + strike + option type
        if (isOption && matchSymbol) {
            bool matchExpiry = (expiry == "N/A" || inst.expiryDate == expiry);
            bool matchStrike = false;
            if (strike == "N/A") {
                matchStrike = true;
            } else {
                bool ok = false;
                double strikeVal = strike.toDouble(&ok);
                if (ok) {
                    matchStrike = qAbs(inst.strikePrice - strikeVal) < 0.001;
                } else {
                    matchStrike = QString::number(inst.strikePrice, 'f', 2) == strike;
                }
            }

            bool matchOption = (!optionType.isEmpty() && inst.optionType.compare(optionType, Qt::CaseInsensitive) == 0);
            
            if (matchExpiry && matchStrike && matchOption) {
                return inst;
            }
        }
    }
    
    // If no exact match in cache, try to find by token (which updateTokenDisplay already resolved)
    qint64 tokenFromDisplay = m_tokenEdit->text().toLongLong();
    if (tokenFromDisplay > 0) {
        for (const auto &inst : m_instrumentCache) {
            if (inst.exchangeInstrumentID == tokenFromDisplay) {
                qDebug() << "[ScripBar] getCurrentInstrument: Found by token" << tokenFromDisplay;
                return inst;
            }
        }
    }
    
    // Final fallback: build from current UI selections (token from display field)
    qDebug() << "[ScripBar] getCurrentInstrument: Using fallback with token" << tokenFromDisplay 
             << "Symbol:" << symbol << "Expiry:" << expiry << "Strike:" << strike << "OptionType:" << optionType;
    result.exchangeInstrumentID = tokenFromDisplay;
    result.symbol = symbol;
    result.name = symbol;
    result.series = "";
    result.instrumentType = currentInstrument;
    result.expiryDate = expiry;
    result.strikePrice = (strike != "N/A" && !strike.isEmpty()) ? strike.toDouble() : 0.0;
    result.optionType = (isOption && !optionType.isEmpty()) ? optionType : "XX";
    result.exchangeSegment = getCurrentExchangeSegmentCode();
    result.scripCode = m_bseScripCodeCombo->currentText();
    
    return result;
}

void ScripBar::updateTokenDisplay()
{
    // Find the matching contract and display its token
    QString symbol = m_symbolCombo->currentText();
    QString currentInstrument = m_instrumentCombo->currentText();
    QString expiry = m_expiryCombo->isVisible() ? m_expiryCombo->currentText() : "N/A";
    QString strike = m_strikeCombo->isVisible() ? m_strikeCombo->currentText() : "N/A";
    QString optionType = m_optionTypeCombo->isVisible() ? m_optionTypeCombo->currentText() : "";
    
    qDebug() << "[ScripBar] updateTokenDisplay: Searching for - Symbol:" << symbol 
             << "Instrument:" << currentInstrument << "Expiry:" << expiry 
             << "Strike:" << strike << "OptionType:" << optionType;
    
    if (symbol.isEmpty()) {
        m_tokenEdit->clear();
        m_bseScripCodeCombo->clearItems();
        return;
    }
    
    bool isEquity = (currentInstrument == "EQUITY");
    bool isFuture = (currentInstrument == "FUTIDX" || currentInstrument == "FUTSTK" || currentInstrument == "FUTCUR");
    bool isOption = (currentInstrument == "OPTIDX" || currentInstrument == "OPTSTK" || currentInstrument == "OPTCUR");
    
    // For options, require all fields to be set before updating token
    // This prevents caching wrong token during intermediate typing states
    if (isOption && (expiry.isEmpty() || expiry == "N/A" || 
                     strike.isEmpty() || strike == "N/A" || 
                     optionType.isEmpty())) {
        qDebug() << "[ScripBar] updateTokenDisplay: Incomplete option selection - clearing token";
        m_tokenEdit->clear();  // Clear stale token
        return;
    }
    
    // For futures, require expiry to be set
    if (isFuture && (expiry.isEmpty() || expiry == "N/A")) {
        qDebug() << "[ScripBar] updateTokenDisplay: Incomplete future selection - clearing token";
        m_tokenEdit->clear();  // Clear stale token
        return;
    }
    
    // Search in instrument cache for matching contract
    // ENHANCED DEBUG: Always show cache status for options
    if (isOption) {
        qDebug() << "[ScripBar] CACHE STATUS: size=" << m_instrumentCache.size() << "contracts";
        if (m_instrumentCache.size() > 0) {
            // Show first matching symbol entry (if any)
            int matchCount = 0;
            for (int i = 0; i < qMin(10, (int)m_instrumentCache.size()); i++) {
                const auto& inst = m_instrumentCache[i];
                if (inst.symbol == symbol) {
                    qDebug() << "  Found matching symbol [" << i << "]: Symbol='" << inst.symbol 
                             << "' Exp='" << inst.expiryDate << "' Strike=" << inst.strikePrice 
                             << " Opt='" << inst.optionType << "' Token=" << inst.exchangeInstrumentID;
                    matchCount++;
                    if (matchCount >= 3) break; // Show first 3 matches
                }
            }
            if (matchCount == 0) {
                qDebug() << "  ❌ NO entries with symbol='" << symbol << "' in cache!";
                qDebug() << "  First 3 cache entries:";
                for (int i = 0; i < qMin(3, (int)m_instrumentCache.size()); i++) {
                    const auto& inst = m_instrumentCache[i];
                    qDebug() << "    [" << i << "] Symbol='" << inst.symbol << "' Series='" << inst.series 
                             << "' Type='" << inst.instrumentType << "'";
                }
            }
        } else {
            qDebug() << "  ❌ CACHE IS EMPTY!";
        }
    }
    
    for (const auto &inst : m_instrumentCache) {
        bool matchSymbol = (inst.symbol == symbol);
        
        // For equity: only match symbol
        if (isEquity && matchSymbol) {
            m_tokenEdit->setText(QString::number(inst.exchangeInstrumentID));
            if (m_currentExchange == "BSE" && !inst.scripCode.isEmpty()) {
                if (m_bseScripCodeCombo->lineEdit()) m_bseScripCodeCombo->lineEdit()->setText(inst.scripCode);
            }
            return;
        }
        
        // For futures: match symbol + expiry (no strike or option type)
        // FACT-BASED: Use instrumentType field to distinguish futures (1) from spreads (4)
        if (isFuture && matchSymbol) {
            bool matchExpiry = (expiry == "N/A" || inst.expiryDate == expiry);
            bool ok = false;
            int32_t instType = inst.instrumentType.toInt(&ok);
            
            // Only match regular futures (instrumentType == 1), not spreads (instrumentType == 4)
            if (matchExpiry && inst.strikePrice == 0.0 && ok && instType == 1) {
                qDebug() << "[ScripBar] ✓ Found regular future (instrumentType=1):" << inst.name << "Token:" << inst.exchangeInstrumentID;
                m_tokenEdit->setText(QString::number(inst.exchangeInstrumentID));
                if (m_currentExchange == "BSE" && !inst.scripCode.isEmpty()) {
                    if (m_bseScripCodeCombo->lineEdit()) m_bseScripCodeCombo->lineEdit()->setText(inst.scripCode);
                }
                return;
            }
        }
        
        // For options: match all fields
        if (isOption && matchSymbol) {
            bool matchExpiry = (expiry == "N/A" || inst.expiryDate == expiry);
            bool matchStrike = false;
            if (strike == "N/A") {
                matchStrike = true;
            } else {
                bool ok = false;
                double strikeVal = strike.toDouble(&ok);
                if (ok) {
                    matchStrike = qAbs(inst.strikePrice - strikeVal) < 0.001;
                } else {
                    matchStrike = QString::number(inst.strikePrice, 'f', 2) == strike;
                }
            }

            bool matchOption = inst.optionType.compare(optionType, Qt::CaseInsensitive) == 0;
            
            // Debug first matching symbol to see cache data format
            static int debugCount = 0;
            if (debugCount < 2 && matchSymbol) {
                qDebug() << "[ScripBar] Cache entry for" << symbol << "- Exp:" << inst.expiryDate << "vs" << expiry << "="<<matchExpiry
                         << "Strike:" << inst.strikePrice << "vs" << strike << "="<<matchStrike
                         << "Opt:'" << inst.optionType << "' vs '" << optionType << "' ="<<matchOption;
                debugCount++;
            }
            
            if (matchExpiry && matchStrike && matchOption) {
                qDebug() << "[ScripBar] updateTokenDisplay: ✅ FOUND matching contract - Token:" << inst.exchangeInstrumentID
                         << "Expiry:" << inst.expiryDate << "Strike:" << inst.strikePrice << "OptionType:" << inst.optionType;
                m_tokenEdit->setText(QString::number(inst.exchangeInstrumentID));
                if (m_currentExchange == "BSE" && !inst.scripCode.isEmpty()) {
                    if (m_bseScripCodeCombo->lineEdit()) m_bseScripCodeCombo->lineEdit()->setText(inst.scripCode);
                }
                return;
            }
        }
    }

    // No matching contract found - clear token and BSE scrip code
    qDebug() << "[ScripBar] updateTokenDisplay: ❌ NO matching contract found - clearing token";
    qDebug() << "[ScripBar] Cache size:" << m_instrumentCache.size() << "contracts";
    m_tokenEdit->clear();
    m_bseScripCodeCombo->clearItems();
}

// Slot implementations
void ScripBar::focusInput() {
    if (m_exchangeCombo) {
        m_exchangeCombo->setFocus();
        if (m_exchangeCombo->lineEdit()) {
            m_exchangeCombo->lineEdit()->selectAll();
        }
    }
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
    // Per reference: NSE F/O both map to NSEFO, NSECDS F/O both map to NSECD, MCX F/O both map to MCX
    QString exchange = m_currentExchange;
    QString segment = m_currentSegment;
    
    // NSE mappings
    if (exchange == "NSE") {
        if (segment == "E") return 1;   // NSECM
        if (segment == "F" || segment == "O" || segment == "D") return 2;  // NSEFO (F, O, D)
    }
    
    // NSECDS mappings
    if (exchange == "NSECDS") {
        if (segment == "F" || segment == "O") return 13;  // NSECD (both F and O)
    }
    
    // BSE mappings
    if (exchange == "BSE") {
        if (segment == "E") return 11;  // BSECM
        if (segment == "F" || segment == "O") return 12;  // BSEFO
    }
    
    // MCX mappings
    if (exchange == "MCX") {
        if (segment == "F" || segment == "O") return 51;  // MCXFO (both F and O)
    }
    
    return 1; // Default to NSECM
}

QString ScripBar::mapInstrumentToSeries(const QString &instrument) const
{
    // Map dropdown instrument type to ContractData series field
    // The dropdown shows user-friendly names like "EQUITY", "FUTIDX", etc.
    // But ContractData.series field contains actual series codes like "EQ", "FUTIDX", etc.
    
    if (instrument == "EQUITY") {
        // For NSE CM, series is "EQ", "BE", "BZ", etc.
        // For BSE CM, series is "A", "B", "X", "F", "G", etc. (NO "EQ"!)
        // Return empty string to get ALL equity series (RepositoryManager will handle this)
        return "";  // Empty = get all series for this segment
    }
    
    // BSE F&O uses different series codes than NSE
    if (m_currentExchange == "BSE") {
        if (instrument == "FUTIDX") return "IF";  // BSE Index Futures
        if (instrument == "OPTIDX") return "IO";  // BSE Index Options
        // BSE doesn't have stock futures/options currently, but keep for future
        if (instrument == "FUTSTK") return "SF";  // BSE Stock Futures (if added)
        if (instrument == "OPTSTK") return "SO";  // BSE Stock Options (if added)
    }
    
    // For NSE F&O, the series field matches the instrument type
    // FUTIDX -> FUTIDX
    // FUTSTK -> FUTSTK
    // OPTIDX -> OPTIDX
    // OPTSTK -> OPTSTK
    // FUTCUR -> FUTCUR
    // OPTCUR -> OPTCUR
    
    return instrument;
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

void ScripBar::populateBseScripCodes()
{
    m_bseScripCodeCombo->clearItems();
    
    // Only populate if BSE + E is selected
    if (m_currentExchange != "BSE" || m_currentSegment != "E") {
        return;
    }
    
    // Extract unique scrip codes from instrument cache
    QSet<QString> uniqueScripCodes;
    for (const auto &inst : m_instrumentCache) {
        if (!inst.scripCode.isEmpty()) {
            uniqueScripCodes.insert(inst.scripCode);
        }
    }
    
    // Convert to sorted list
    QStringList scripCodes = uniqueScripCodes.values();
    scripCodes.sort();
    
    qDebug() << "[ScripBar] Populated" << scripCodes.size() << "BSE scrip codes";
    
    // Populate combo box
    m_bseScripCodeCombo->addItems(scripCodes);
}

void ScripBar::onBseScripCodeChanged(const QString &text)
{
    qDebug() << "[ScripBar] BSE scrip code selected:" << text;
    
    // Find the instrument with this scrip code in the cache
    for (const auto &inst : m_instrumentCache) {
        if (inst.scripCode == text) {
            // Update symbol combo to show this instrument's symbol
            if (m_symbolCombo->lineEdit()) {
                m_symbolCombo->lineEdit()->setText(inst.symbol);
            }
            
            // Update token display
            updateTokenDisplay();
            break;
        }
    }
}
