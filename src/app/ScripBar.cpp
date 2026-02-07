#include "app/ScripBar.h"
#include "api/XTSMarketDataClient.h"
#include "repository/RepositoryManager.h"
#include <QDebug>
#include <QElapsedTimer> // For performance measurement
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QShortcut>

ScripBar::ScripBar(QWidget *parent, ScripBarMode mode)
    : QWidget(parent), m_xtsClient(nullptr), m_mode(mode) {
  setupUI();

  // ⚡ OPTIMIZATION: In DisplayMode, skip expensive population (save ~300ms)
  // Only populate in SearchMode (default for MarketWatch/OrderWindow)
  if (m_mode == SearchMode) {
    populateExchanges();
  }
}

void ScripBar::setScripBarMode(ScripBarMode mode) { m_mode = mode; }

ScripBar::~ScripBar() = default;

void ScripBar::setXTSClient(XTSMarketDataClient *client) {
  m_xtsClient = client;
  // qDebug() << "XTS client set for ScripBar";
}

void ScripBar::setupUI() {
  m_layout = new QHBoxLayout(this);
  m_layout->setContentsMargins(4, 2, 4, 2);
  m_layout->setSpacing(6);
  m_layout->setAlignment(Qt::AlignVCenter);

  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setMaximumHeight(40);

  // Exchange combo (custom)
  m_exchangeCombo = new CustomScripComboBox(this);
  m_exchangeCombo->setMode(CustomScripComboBox::SelectorMode);
  m_exchangeCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);

  m_exchangeCombo->setMinimumWidth(56);
  m_exchangeCombo->setObjectName("cbExchange"); // For keyboard shortcut
  m_layout->addWidget(m_exchangeCombo);
  connect(m_exchangeCombo, &CustomScripComboBox::itemSelected, this,
          &ScripBar::onExchangeChanged);

  // Segment combo (custom)
  m_segmentCombo = new CustomScripComboBox(this);
  m_segmentCombo->setMode(CustomScripComboBox::SelectorMode);
  m_segmentCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);

  m_segmentCombo->setMinimumWidth(48);
  m_layout->addWidget(m_segmentCombo);
  connect(m_segmentCombo, &CustomScripComboBox::itemSelected, this,
          &ScripBar::onSegmentChanged);

  // Instrument combo (custom)
  m_instrumentCombo = new CustomScripComboBox(this);
  m_instrumentCombo->setMode(CustomScripComboBox::SelectorMode);
  m_instrumentCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);

  m_instrumentCombo->setMinimumWidth(80);
  m_layout->addWidget(m_instrumentCombo);
  connect(m_instrumentCombo, &CustomScripComboBox::itemSelected, this,
          &ScripBar::onInstrumentChanged);

  // BSE Scrip Code combo (only visible for BSE + E segment)
  m_bseScripCodeCombo = new CustomScripComboBox(this);
  m_bseScripCodeCombo->setMode(CustomScripComboBox::SearchMode);
  m_bseScripCodeCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);

  m_bseScripCodeCombo->setMinimumWidth(120);
  m_bseScripCodeCombo->setMaximumWidth(150);
  m_layout->addWidget(m_bseScripCodeCombo);
  m_bseScripCodeCombo->setVisible(false); // Hidden by default
  connect(m_bseScripCodeCombo, &CustomScripComboBox::itemSelected, this,
          &ScripBar::onBseScripCodeChanged);

  // Symbol combo (custom)
  m_symbolCombo = new CustomScripComboBox(this);
  m_symbolCombo->setMode(CustomScripComboBox::SearchMode);
  m_symbolCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);

  m_symbolCombo->setMinimumWidth(100);
  m_layout->addWidget(m_symbolCombo);
  connect(m_symbolCombo, &CustomScripComboBox::itemSelected, this,
          &ScripBar::onSymbolChanged);

  // Expiry combo (custom with date sorting)
  m_expiryCombo = new CustomScripComboBox(this);
  m_expiryCombo->setMode(CustomScripComboBox::SearchMode);
  m_expiryCombo->setSortMode(CustomScripComboBox::ChronologicalSort);

  m_expiryCombo->setMinimumWidth(125);
  m_layout->addWidget(m_expiryCombo);
  connect(m_expiryCombo, &CustomScripComboBox::itemSelected, this,
          &ScripBar::onExpiryChanged);

  // Strike combo (custom with numeric sorting)
  m_strikeCombo = new CustomScripComboBox(this);
  m_strikeCombo->setMode(CustomScripComboBox::SearchMode);
  m_strikeCombo->setSortMode(CustomScripComboBox::NumericSort);

  m_strikeCombo->setMinimumWidth(140);
  m_layout->addWidget(m_strikeCombo);
  connect(m_strikeCombo, &CustomScripComboBox::itemSelected, this,
          &ScripBar::onStrikeChanged);

  // Option Type combo (custom) - SelectorMode since only CE/PE options
  m_optionTypeCombo = new CustomScripComboBox(this);
  m_optionTypeCombo->setMode(CustomScripComboBox::SelectorMode);

  m_optionTypeCombo->setSortMode(CustomScripComboBox::NoSort);

  m_optionTypeCombo->setMinimumWidth(48);
  m_layout->addWidget(m_optionTypeCombo);
  connect(m_optionTypeCombo, &CustomScripComboBox::itemSelected, this,
          &ScripBar::onOptionTypeChanged);

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
  connect(m_addToWatchButton, &QPushButton::clicked, this,
          &ScripBar::onAddToWatchClicked);

  // Connect Enter key (when dropdown closed) from all comboboxes to trigger Add
  // button
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

  // Connect ESC key handling
  connect(m_exchangeCombo, &CustomScripComboBox::escapePressed, this,
          &ScripBar::scripBarEscapePressed);
  connect(m_segmentCombo, &CustomScripComboBox::escapePressed, this,
          &ScripBar::scripBarEscapePressed);
  connect(m_instrumentCombo, &CustomScripComboBox::escapePressed, this,
          &ScripBar::scripBarEscapePressed);
  connect(m_bseScripCodeCombo, &CustomScripComboBox::escapePressed, this,
          &ScripBar::scripBarEscapePressed);
  connect(m_symbolCombo, &CustomScripComboBox::escapePressed, this,
          &ScripBar::scripBarEscapePressed);
  connect(m_expiryCombo, &CustomScripComboBox::escapePressed, this,
          &ScripBar::scripBarEscapePressed);
  connect(m_strikeCombo, &CustomScripComboBox::escapePressed, this,
          &ScripBar::scripBarEscapePressed);
  connect(m_optionTypeCombo, &CustomScripComboBox::escapePressed, this,
          &ScripBar::scripBarEscapePressed);

  m_layout->addStretch();

  // Dark premium styling
  // Dark premium styling with Light Input fields for contrast and focus
  // visibility
  setStyleSheet(
      "QWidget { background-color: #2d2d30; border: none; }"

      // ComboBox Styles - Light Mode for Inputs
      "CustomScripComboBox {"
      "    color: #000000;"
      "    background: #ffffff;"
      "    border: 1px solid #cccccc;"
      "    padding: 2px 4px;"
      "    border-radius: 4px;"
      "}"
      "CustomScripComboBox:hover {"
      "    border: 1px solid #999999;"
      "}"
      "CustomScripComboBox:focus, CustomScripComboBox:on {"
      "    border: 2px solid #007acc;" /* Clear Blue Focus Border */
      "    padding: 1px 3px;"          /* Adjust padding for 2px border */
      "}"

      "CustomScripComboBox::drop-down {"
      "    subcontrol-origin: padding;"
      "    subcontrol-position: top right;"
      "    width: 20px;"
      "    border-left: 1px solid #cccccc;"
      "    background: #f0f0f0;"
      "}"
      "CustomScripComboBox::down-arrow {"
      "    image: url(none);"
      "    width: 0; height: 0;"
      "    border-left: 4px solid transparent;"
      "    border-right: 4px solid transparent;"
      "    border-top: 6px solid #555555;"
      "    margin-right: 6px;"
      "}"
      "CustomScripComboBox::down-arrow:hover {"
      "    border-top-color: #000000;"
      "}"

      // Popup List Styles
      "CustomScripComboBox QAbstractItemView {"
      "    background: #ffffff;"
      "    color: #000000;"
      "    selection-background-color: #007acc;"
      "    selection-color: #ffffff;"
      "    outline: none;"
      "    border: 1px solid #cccccc;"
      "}"

      // Explicitly style the LineEdit inside the ComboBox to match basic combo
      // style
      "CustomScripComboBox QLineEdit {"
      "    background: transparent;"
      "    color: #000000;"
      "    border: none;"
      "}"

      // Shared Button Styles
      "QPushButton { background: #007acc; color: #ffffff; border: none; "
      "border-radius: 4px; font-weight: bold; padding: 4px 12px; }"
      "QPushButton:hover { background: #0062a3; }"
      "QPushButton:pressed { background: #004d80; }"

      // LineEdit Styles (Token display)
      "QLineEdit { background: #ffffff; color: #000000; border: 1px solid "
      "#cccccc; border-radius: 4px; padding: 2px 6px; "
      "selection-background-color: #007acc; selection-color: #ffffff; }"
      "QLineEdit:focus { border: 2px solid #007acc; padding: 1px 5px; }"

      "QLabel { color: #cccccc; }");
}

// future implementation
// exchange and segment should be populated from client profile api so if client
// has nsecm or nsefo then nse should be populated in exchange so on and so
// forth

void ScripBar::populateExchanges() {
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

void ScripBar::populateSegments(const QString &exchange) {
  m_segmentCombo->clearItems();
  QStringList segs;
  if (exchange == "NSE")
    segs << "E" << "F" << "O"; // E=NSECM, F=NSEFO, O=NSEFO
  else if (exchange == "NSECDS")
    segs << "F" << "O"; // F=NSECD, O=NSECD
  else if (exchange == "BSE")
    segs << "E" << "F" << "O";
  else if (exchange == "MCX")
    segs << "F" << "O"; // F=MCX, O=MCX
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

void ScripBar::populateInstruments(const QString &segment) {
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
  onInstrumentChanged();
}

void ScripBar::populateSymbols(const QString &instrument) {
  m_symbolCombo->clearItems();

  // ⚡ OPTIMIZATION: Use RepositoryManager::getUniqueSymbols() directly
  // This method has lazy caching built-in (0.02ms cached, 2-5ms first call)
  // Much simpler than the old dual-cache system!

  QString exchange = getCurrentExchange();
  QString segment = getCurrentSegment();
  QString seriesFilter =
      RepositoryManager::mapInstrumentToSeries(exchange, instrument);

  RepositoryManager *repo = RepositoryManager::getInstance();

  if (!repo->isLoaded()) {
    qWarning() << "[ScripBar] Repository not loaded yet";
    m_symbolCombo->addItem("Loading master data...");
    return;
  }

  QElapsedTimer perfTimer;
  perfTimer.start();

  // Get unique symbols directly from RepositoryManager (fast, cached)
  QStringList symbols = repo->getUniqueSymbols(exchange, segment, seriesFilter);

  qDebug() << "[ScripBar] Loaded" << symbols.size() << "symbols in"
           << perfTimer.elapsed() << "ms";

  if (symbols.isEmpty()) {
    qDebug() << "[ScripBar] No symbols found for" << exchange << segment
             << "series:" << seriesFilter;
    m_symbolCombo->addItem("No instruments found");
    return;
  }

  // Update BSE scrip code visibility
  updateBseScripCodeVisibility();

  // Populate combo box
  m_symbolCombo->addItems(symbols);

  if (m_symbolCombo->count() > 0) {
    m_symbolCombo->setCurrentIndex(0);
    onSymbolChanged(m_symbolCombo->currentText());
  }
}

void ScripBar::populateExpiries(const QString &symbol) {
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
    if (inst.symbol == symbol && !inst.expiryDate.isEmpty() &&
        inst.expiryDate != "N/A") {
      if (!expiries.contains(inst.expiryDate)) {
        expiries.append(inst.expiryDate);
      }
    }
  }

  if (expiries.isEmpty()) {
    expiries << "N/A";
  }

  qDebug() << "[ScripBar] populateExpiries: Found" << expiries.size()
           << "expiry dates for symbol" << symbol;

  m_expiryCombo->addItems(expiries);

  if (m_expiryCombo->count() > 0) {
    m_expiryCombo->setCurrentIndex(0);
    onExpiryChanged();
  }
}

void ScripBar::populateStrikes(const QString &expiry) {
  m_strikeCombo->clearItems();

  QString currentInstrument = m_instrumentCombo->currentText();
  bool isFuture =
      (currentInstrument == "FUTIDX" || currentInstrument == "FUTSTK" ||
       currentInstrument == "FUTCUR");
  bool isOption =
      (currentInstrument == "OPTIDX" || currentInstrument == "OPTSTK" ||
       currentInstrument == "OPTCUR");

  // For futures or equity, skip strike logic and update token directly
  if (isFuture || !isOption) {
    updateTokenDisplay();
    return;
  }

  // For options, extract strike prices from cache
  QString currentSymbol = m_symbolCombo->currentText();
  QStringList strikes;

  for (const auto &inst : m_instrumentCache) {
    if (inst.symbol == currentSymbol && inst.expiryDate == expiry &&
        inst.strikePrice > 0) {
      QString strikeStr = QString::number(inst.strikePrice, 'f', 2);
      if (!strikes.contains(strikeStr)) {
        strikes.append(strikeStr);
      }
    }
  }

  if (strikes.isEmpty()) {
    strikes << "N/A";
  }

  qDebug() << "[ScripBar] populateStrikes: Found" << strikes.size()
           << "strikes for expiry" << expiry;

  m_strikeCombo->addItems(strikes);

  if (m_strikeCombo->count() > 0) {
    m_strikeCombo->setCurrentIndex(0);
    onStrikeChanged();
  }
}

void ScripBar::populateOptionTypes(const QString &strike) {
  Q_UNUSED(strike)

  QString currentInstrument = m_instrumentCombo->currentText();
  bool isOption =
      (currentInstrument == "OPTIDX" || currentInstrument == "OPTSTK" ||
       currentInstrument == "OPTCUR");

  if (!isOption) {
    // For non-options, skip this and update token
    updateTokenDisplay();
    return;
  }

  m_optionTypeCombo->clearItems();
  QStringList types = {"CE", "PE"};
  m_optionTypeCombo->addItems(types);

  // Set default to CE (first item)
  m_optionTypeCombo->setCurrentIndex(0);

  updateTokenDisplay();
}

// Slot implementations
void ScripBar::onExchangeChanged(const QString &text) {
  m_currentExchange = text;
  populateSegments(text);
  updateBseScripCodeVisibility();
}

void ScripBar::onSegmentChanged(const QString &text) {
  m_currentSegment = text;
  populateInstruments(text);
  updateBseScripCodeVisibility();
}

void ScripBar::onInstrumentChanged(const QString &text) {
  Q_UNUSED(text)
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

void ScripBar::onSymbolChanged(const QString &text) {
  // ⚡ Lazy-load contract details for this symbol only
  // This is much faster than loading all contracts upfront

  QString exchange = getCurrentExchange();
  QString segment = getCurrentSegment();
  QString seriesFilter = RepositoryManager::mapInstrumentToSeries(
      exchange, m_instrumentCombo->currentText());

  RepositoryManager *repo = RepositoryManager::getInstance();

  if (!repo->isLoaded()) {
    return;
  }

  // Get all contracts for this symbol
  QVector<ContractData> contracts =
      repo->getScrips(exchange, segment, seriesFilter);

  // Filter to only this symbol and build instrument cache
  m_instrumentCache.clear();
  int exchangeSegmentId =
      RepositoryManager::getExchangeSegmentID(exchange, segment);

  for (const ContractData &contract : contracts) {
    if (contract.name == text) {
      InstrumentData inst;
      inst.exchangeInstrumentID = contract.exchangeInstrumentID;
      inst.name = contract.name;
      inst.symbol = contract.name;
      inst.series = contract.series;
      inst.instrumentType = QString::number(contract.instrumentType);
      inst.expiryDate = contract.expiryDate;
      inst.strikePrice = contract.strikePrice;
      inst.optionType = contract.optionType;
      inst.exchangeSegment = exchangeSegmentId;
      inst.scripCode = contract.scripCode;
      m_instrumentCache.append(inst);
    }
  }

  qDebug() << "[ScripBar] Lazy-loaded" << m_instrumentCache.size()
           << "contracts for symbol:" << text;

  populateExpiries(text);
  updateTokenDisplay();
}

void ScripBar::onExpiryChanged(const QString &text) {
  Q_UNUSED(text)
  QString expiry = m_expiryCombo->currentText();
  // qDebug() << "[ScripBar] onExpiryChanged: expiry =" << expiry;
  populateStrikes(expiry);

  // For futures (no strikes), update token display now
  QString currentInstrument = m_instrumentCombo->currentText();
  bool isFuture =
      (currentInstrument == "FUTIDX" || currentInstrument == "FUTSTK" ||
       currentInstrument == "FUTCUR");
  if (isFuture) {
    /* qDebug() << "[ScripBar] Future instrument - updating token display after
       " "expiry change"; */
    updateTokenDisplay();
  }
}

void ScripBar::onStrikeChanged(const QString &text) {
  Q_UNUSED(text)
  QString strike = m_strikeCombo->currentText();
  // qDebug() << "[ScripBar] onStrikeChanged: strike =" << strike;
  populateOptionTypes(strike);
}

void ScripBar::onOptionTypeChanged(const QString &text) {
  Q_UNUSED(text)
  QString optionType = m_optionTypeCombo->currentText();
  // qDebug() << "[ScripBar] onOptionTypeChanged: optionType =" << optionType;
  updateTokenDisplay();
}

void ScripBar::onAddToWatchClicked() {
  InstrumentData inst = getCurrentInstrument();
  emit addToWatchRequested(inst);
  /* qDebug() << "Add to watch requested:" << inst.symbol << inst.expiryDate
           << inst.strikePrice << inst.optionType
           << "token:" << inst.exchangeInstrumentID; */
}

void ScripBar::refreshSymbols() {
  // Re-populate symbols based on current selections
  QString currentInstrument = m_instrumentCombo->currentText();
  if (!currentInstrument.isEmpty()) {
    /* qDebug() << "[ScripBar] Refreshing symbols for instrument:"
             << currentInstrument; */
    populateSymbols(currentInstrument);
  }
}

void ScripBar::updateBseScripCodeVisibility() {
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

InstrumentData ScripBar::getCurrentInstrument() const {
  if (m_mode == DisplayMode) {
    return m_displayData;
  }

  InstrumentData result;

  QString symbol = m_symbolCombo->currentText();
  QString currentInstrument = m_instrumentCombo->currentText();
  QString expiry =
      m_expiryCombo->isVisible() ? m_expiryCombo->currentText() : "N/A";
  QString strike =
      m_strikeCombo->isVisible() ? m_strikeCombo->currentText() : "N/A";
  QString optionType =
      m_optionTypeCombo->isVisible() ? m_optionTypeCombo->currentText() : "";

  if (expiry.isEmpty())
    expiry = "N/A";
  if (strike.isEmpty())
    strike = "N/A";

  bool isEquity = (currentInstrument == "EQUITY");
  bool isFuture = (currentInstrument.startsWith("FUT"));
  bool isOption = (currentInstrument.startsWith("OPT"));

  // ⚡ OPTIMIZATION: Check token display first
  qint64 tokenFromDisplay = m_tokenEdit->text().toLongLong();
  if (tokenFromDisplay > 0) {
    for (const auto &inst : m_instrumentCache) {
      if (inst.exchangeInstrumentID == tokenFromDisplay) {
        return inst;
      }
    }
  }

  // Linear search fallback
  for (const auto &inst : m_instrumentCache) {
    if (inst.symbol != symbol)
      continue;

    if (isEquity)
      return inst;

    if (isFuture) {
      // Filter out spread contracts (instrumentType == 4)
      // Note: instrumentType is stored as string representation of int: 1=Future, 2=Option, 4=Spread
      if ((expiry == "N/A" || inst.expiryDate == expiry) &&
          inst.instrumentType != "4") {
        return inst;
      }
    }

    if (isOption) {
      if (inst.expiryDate == expiry && inst.optionType == optionType) {
        double strikeVal = strike.toDouble();
        if (qAbs(inst.strikePrice - strikeVal) < 0.001)
          return inst;
      }
    }
  }

  // Fallback: build from UI
  result.exchangeInstrumentID = tokenFromDisplay;
  result.symbol = symbol;
  result.name = symbol;
  result.instrumentType = currentInstrument;
  result.expiryDate = expiry;
  result.strikePrice = (strike != "N/A") ? strike.toDouble() : 0.0;
  result.optionType = optionType;
  result.exchangeSegment = RepositoryManager::getExchangeSegmentID(
      getCurrentExchange(), getCurrentSegment());
  return result;
}

void ScripBar::updateTokenDisplay() {
  QString symbol = m_symbolCombo->currentText();
  QString currentInstrument = m_instrumentCombo->currentText();
  QString expiry =
      m_expiryCombo->isVisible() ? m_expiryCombo->currentText() : "N/A";
  QString strike =
      m_strikeCombo->isVisible() ? m_strikeCombo->currentText() : "N/A";
  QString optionType =
      m_optionTypeCombo->isVisible() ? m_optionTypeCombo->currentText() : "";

  if (symbol.isEmpty()) {
    m_tokenEdit->clear();
    m_bseScripCodeCombo->clearItems();
    return;
  }

  bool isEquity = (currentInstrument == "EQUITY");
  bool isFuture = currentInstrument.startsWith("FUT");
  bool isOption = currentInstrument.startsWith("OPT");

  // Validation
  if (isOption &&
      (expiry == "N/A" || strike == "N/A" || optionType.isEmpty())) {
    m_tokenEdit->clear();
    return;
  }
  if (isFuture && expiry == "N/A") {
    m_tokenEdit->clear();
    return;
  }

  // ⚡ SEARCH: Linear scan over symbol-filtered cache
  for (const auto &inst : m_instrumentCache) {
    if (inst.symbol != symbol)
      continue;

    if (isEquity) {
      m_tokenEdit->setText(QString::number(inst.exchangeInstrumentID));
      return;
    }

    if (isFuture) {
      // Filter out spread contracts (instrumentType == 4)
      // Note: instrumentType is stored as string representation of int: 1=Future, 2=Option, 4=Spread
      if (inst.expiryDate == expiry && inst.instrumentType != "4") {
        m_tokenEdit->setText(QString::number(inst.exchangeInstrumentID));
        return;
      }
    }

    if (isOption) {
      if (inst.expiryDate == expiry && inst.optionType == optionType) {
        double strikeVal = strike.toDouble();
        if (qAbs(inst.strikePrice - strikeVal) < 0.001) {
          m_tokenEdit->setText(QString::number(inst.exchangeInstrumentID));
          return;
        }
      }
    }
  }

  m_tokenEdit->clear();
}

void ScripBar::setScripDetails(const InstrumentData &data) {
  // ⚡ OPTIMIZATION: If in DisplayMode, use O(1) display logic and skip
  // population This avoids rebuilding cache and repopulating combos (saves
  // ~200-400ms)
  if (m_mode == DisplayMode) {
    displaySingleContract(data);
    return;
  }

  // Block signals to prevent cascading updates during setup
  const QSignalBlocker blockerEx(m_exchangeCombo);
  const QSignalBlocker blockerSeg(m_segmentCombo);
  const QSignalBlocker blockerInst(m_instrumentCombo);
  const QSignalBlocker blockerSym(m_symbolCombo);
  const QSignalBlocker blockerExp(m_expiryCombo);
  const QSignalBlocker blockerStr(m_strikeCombo);
  const QSignalBlocker blockerOpt(m_optionTypeCombo);

  // 1. Set Exchange
  QString segKey =
      RepositoryManager::getExchangeSegmentName(data.exchangeSegment);
  QString exchange = "NSE";
  if (segKey.startsWith("BSE"))
    exchange = "BSE";
  else if (segKey.startsWith("MCX"))
    exchange = "MCX";
  else if (segKey.startsWith("NSECD"))
    exchange = "NSECDS";
  // else NSE (NSECM, NSEFO)

  int idxEx = m_exchangeCombo->findText(exchange);
  if (idxEx >= 0) {
    m_exchangeCombo->setCurrentIndex(idxEx);
    m_currentExchange = exchange;
    // Populate segments manually since signals are blocked
    populateSegments(exchange);
  }

  // 2. Set Segment
  QString targetSeg;
  if (data.instrumentType == "EQUITY")
    targetSeg = "E";
  else if (data.instrumentType.startsWith("FUT"))
    targetSeg = "F";
  else if (data.instrumentType.startsWith("OPT"))
    targetSeg = "O";

  if (!targetSeg.isEmpty()) {
    int idxSeg = m_segmentCombo->findText(targetSeg);
    if (idxSeg >= 0) {
      m_segmentCombo->setCurrentIndex(idxSeg);
      m_currentSegment = targetSeg;
      populateInstruments(targetSeg);
    }
  }

  // 3. Set Instrument
  if (!data.instrumentType.isEmpty()) {
    int idxInst = m_instrumentCombo->findText(data.instrumentType);
    if (idxInst >= 0) {
      m_instrumentCombo->setCurrentIndex(idxInst);
      // Setup visibility manually since signals blocked
      // Call onInstrumentChanged logic (without emitting populateSymbols if we
      // do it manually next) But onInstrumentChanged logic is needed to
      // show/hide combos.

      // Logic duplicated from onInstrumentChanged:
      QString inst = data.instrumentType;
      bool isFuture =
          (inst == "FUTIDX" || inst == "FUTSTK" || inst == "FUTCUR");
      bool isOption =
          (inst == "OPTIDX" || inst == "OPTSTK" || inst == "OPTCUR");

      m_expiryCombo->setVisible(isFuture || isOption);
      m_strikeCombo->setVisible(isOption);
      m_optionTypeCombo->setVisible(isOption);
    }
  }

  // 4. Set Symbol
  populateSymbols(data.instrumentType);

  if (!data.symbol.isEmpty()) {
    int idxSym = m_symbolCombo->findText(data.symbol);
    if (idxSym >= 0) {
      m_symbolCombo->setCurrentIndex(idxSym);
      populateExpiries(data.symbol);
    }
  }

  // 5. Expiry
  if (m_expiryCombo->isVisible() && !data.expiryDate.isEmpty()) {
    int idxExp = m_expiryCombo->findText(data.expiryDate);
    if (idxExp >= 0) {
      m_expiryCombo->setCurrentIndex(idxExp);
      populateStrikes(data.expiryDate);
    }
  }

  // 6. Strike
  if (m_strikeCombo->isVisible() && data.strikePrice > 0) {
    QString strikeStr = QString::number(data.strikePrice, 'f', 2);
    int idxStr = m_strikeCombo->findText(strikeStr);
    if (idxStr >= 0) {
      m_strikeCombo->setCurrentIndex(idxStr);
      populateOptionTypes(strikeStr);
    }
  }

  // 7. Option Type
  if (m_optionTypeCombo->isVisible() && !data.optionType.isEmpty()) {
    int idxOpt = m_optionTypeCombo->findText(data.optionType);
    if (idxOpt >= 0) {
      m_optionTypeCombo->setCurrentIndex(idxOpt);
    }
  }

  // Update display token
  updateTokenDisplay();
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

QString ScripBar::getCurrentExchange() const { return m_currentExchange; }

QString ScripBar::getCurrentSegment() const { return m_currentSegment; }

// ✅ REMOVED: getCurrentExchangeSegmentCode() - now using
// RepositoryManager::getExchangeSegmentID() ✅ REMOVED: mapInstrumentToSeries()
// - now using RepositoryManager::mapInstrumentToSeries()

void ScripBar::searchInstrumentsAsync(const QString &searchText) {
  if (!m_xtsClient || !m_xtsClient->isLoggedIn()) {
    qWarning() << "Cannot search: XTS client not available or not logged in";
    return;
  }

  int exchangeSegment = RepositoryManager::getExchangeSegmentID(
      getCurrentExchange(), getCurrentSegment());

  // Connect to instruments received signal
  connect(
      m_xtsClient, &XTSMarketDataClient::instrumentsReceived, this,
      [this](bool success, const QVector<XTS::Instrument> &instruments,
             const QString &error) {
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
            // TODO: Parse instrument type, expiry, strike, option type from
            // name
            data.append(item);
          }
          onInstrumentsReceived(data);
        } else {
          qWarning() << "Instrument search failed:" << error;
        }
      },
      Qt::UniqueConnection);

  // Trigger the actual search
  m_xtsClient->searchInstruments(searchText, exchangeSegment);
}

void ScripBar::onInstrumentsReceived(
    const QVector<InstrumentData> &instruments) {
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

  qDebug() << "Received" << instruments.size() << "instruments,"
           << symbols.size() << "unique symbols";
}

void ScripBar::populateBseScripCodes() {
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

void ScripBar::onBseScripCodeChanged(const QString &text) {
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

// ⚡ DISPLAYMODE: Direct O(1) display without cache rebuild
// This is ~200-400ms faster than SearchMode's populateSymbols()
void ScripBar::displaySingleContract(const InstrumentData &data) {
  qDebug() << "[ScripBar] ⚡ DisplayMode: O(1) display for token"
           << data.exchangeInstrumentID;

  // Store for getCurrentInstrument()
  m_displayData = data;

  // Block signals to prevent cascading updates
  const QSignalBlocker blockerEx(m_exchangeCombo);
  const QSignalBlocker blockerSeg(m_segmentCombo);
  const QSignalBlocker blockerInst(m_instrumentCombo);
  const QSignalBlocker blockerSym(m_symbolCombo);
  const QSignalBlocker blockerExp(m_expiryCombo);
  const QSignalBlocker blockerStr(m_strikeCombo);
  const QSignalBlocker blockerOpt(m_optionTypeCombo);

  // 1. Set Exchange display
  QString segKey =
      RepositoryManager::getExchangeSegmentName(data.exchangeSegment);
  QString exchange = "NSE";
  if (segKey.startsWith("BSE"))
    exchange = "BSE";
  else if (segKey.startsWith("MCX"))
    exchange = "MCX";
  else if (segKey.startsWith("NSECD"))
    exchange = "NSECDS";

  m_exchangeCombo->clearItems();
  m_exchangeCombo->addItem(exchange);
  m_exchangeCombo->setCurrentIndex(0);
  m_currentExchange = exchange;

  // 2. Set Segment display
  QString targetSeg = "E";
  if (data.instrumentType.startsWith("FUT"))
    targetSeg = "F";
  else if (data.instrumentType.startsWith("OPT"))
    targetSeg = "O";

  m_segmentCombo->clearItems();
  m_segmentCombo->addItem(targetSeg);
  m_segmentCombo->setCurrentIndex(0);
  m_currentSegment = targetSeg;

  // 3. Set Instrument Type display
  if (!data.instrumentType.isEmpty()) {
    m_instrumentCombo->clearItems();
    m_instrumentCombo->addItem(data.instrumentType);
    m_instrumentCombo->setCurrentIndex(0);
  }

  // 4. Setup visibility based on instrument type
  bool isFuture = data.instrumentType.startsWith("FUT");
  bool isOption = data.instrumentType.startsWith("OPT");
  m_expiryCombo->setVisible(isFuture || isOption);
  m_strikeCombo->setVisible(isOption);
  m_optionTypeCombo->setVisible(isOption);

  // 5. Set Symbol display
  if (!data.symbol.isEmpty()) {
    m_symbolCombo->clearItems();
    m_symbolCombo->addItem(data.symbol);
    m_symbolCombo->setCurrentIndex(0);
  }

  // 6. Set Expiry display
  if (m_expiryCombo->isVisible() && !data.expiryDate.isEmpty()) {
    m_expiryCombo->clearItems();
    m_expiryCombo->addItem(data.expiryDate);
    m_expiryCombo->setCurrentIndex(0);
  }

  // 7. Set Strike display
  if (m_strikeCombo->isVisible() && data.strikePrice > 0) {
    QString strikeStr = QString::number(data.strikePrice, 'f', 2);
    m_strikeCombo->clearItems();
    m_strikeCombo->addItem(strikeStr);
    m_strikeCombo->setCurrentIndex(0);
  }

  // 8. Set Option Type display
  if (m_optionTypeCombo->isVisible() && !data.optionType.isEmpty()) {
    m_optionTypeCombo->clearItems();
    m_optionTypeCombo->addItem(data.optionType);
    m_optionTypeCombo->setCurrentIndex(0);
  }

  // 9. Set Token display
  m_tokenEdit->setText(QString::number(data.exchangeInstrumentID));

  /* qDebug() << "[ScripBar] ⚡ DisplayMode complete - Exchange:" << exchange
           << "Symbol:" << data.symbol << "Token:" << data.exchangeInstrumentID;
   */
}
