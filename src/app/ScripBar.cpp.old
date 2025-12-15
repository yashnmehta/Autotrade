#include "app/ScripBar.h"
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QPushButton>
#include <QDebug>
#include <QShortcut>
#include <QKeySequence>

ScripBar::ScripBar(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    populateExchanges();
    setupShortcuts();
}

ScripBar::~ScripBar() = default;

void ScripBar::setupUI()
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 2, 4, 2);
    m_layout->setSpacing(6);
    m_layout->setAlignment(Qt::AlignVCenter);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMaximumHeight(28);
#ifdef Q_OS_MAC
    setMaximumHeight(28);
#endif

    // Exchange combo (custom)
    m_exchangeCombo = new CustomScripComboBox(this);
    m_exchangeCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);
    m_exchangeCombo->setMinimumWidth(56);
    m_exchangeCombo->setObjectName("cbExchange");  // For keyboard shortcut
    m_layout->addWidget(m_exchangeCombo);
    connect(m_exchangeCombo, &CustomScripComboBox::itemSelected, this, &ScripBar::onExchangeChanged);
    
    // Keep references for compatibility
    m_exchangeBtn = nullptr;
    m_exchangeMenu = nullptr;

    // Segment combo (custom)
    m_segmentCombo = new CustomScripComboBox(this);
    m_segmentCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);
    m_segmentCombo->setMinimumWidth(48);
    m_layout->addWidget(m_segmentCombo);
    connect(m_segmentCombo, &CustomScripComboBox::itemSelected, this, &ScripBar::onSegmentChanged);
    
    // Keep references for compatibility
    m_segmentBtn = nullptr;
    m_segmentMenu = nullptr;

    // Instrument combo (custom)
    m_instrumentCombo = new CustomScripComboBox(this);
    m_instrumentCombo->setSortMode(CustomScripComboBox::AlphabeticalSort);
    m_instrumentCombo->setMinimumWidth(80);
    m_layout->addWidget(m_instrumentCombo);
    connect(m_instrumentCombo, &CustomScripComboBox::itemSelected, 
            this, [this](const QString &text) { onInstrumentChanged(0); });

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
    connect(m_expiryCombo, &CustomScripComboBox::itemSelected, 
            this, [this](const QString &text) { onExpiryChanged(0); });

    // Strike combo (custom with numeric sorting)
    m_strikeCombo = new CustomScripComboBox(this);
    m_strikeCombo->setSortMode(CustomScripComboBox::NumericSort);
    m_strikeCombo->setMinimumWidth(64);
    m_layout->addWidget(m_strikeCombo);
    connect(m_strikeCombo, &CustomScripComboBox::itemSelected, 
            this, [this](const QString &text) { onStrikeChanged(0); });

    // Option Type combo (custom)
    m_optionTypeCombo = new CustomScripComboBox(this);
    m_optionTypeCombo->setSortMode(CustomScripComboBox::NoSort);
    m_optionTypeCombo->setMinimumWidth(48);
    m_layout->addWidget(m_optionTypeCombo);
    connect(m_optionTypeCombo, &CustomScripComboBox::itemSelected, 
            this, [this](const QString &text) { onOptionTypeChanged(0); });

    // Add to Watch
    m_addToWatchButton = new QPushButton(tr("Add"), this);
    m_addToWatchButton->setMinimumWidth(64);
    m_layout->addWidget(m_addToWatchButton);
    connect(m_addToWatchButton, &QPushButton::clicked, this, &ScripBar::onAddToWatchClicked);

    // Connect Enter key (when dropdown closed) from all comboboxes to trigger Add button
    // This enables keyboard workflow: Ctrl+S -> type -> Enter -> Enter (adds to watchlist)
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

    // Compact flat styling (inherit main toolbar look but flat)
    setStyleSheet(
        "QWidget{ background-color: #2d2d30; }"
        "QToolButton, QComboBox, QPushButton{ color: #ffffff; background: #1e1e1e; border: none; padding: 2px 6px; }"
        "QComboBox QAbstractItemView{ background: #1e1e1e; color: #ffffff; selection-background-color: #094771;}"
        "QPushButton{ background: #0e639c; border-radius: 2px;}");
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
        instruments << "FUTIDX" << "OPTIDX";
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
    if (instrument == "EQUITY")
    {
        // Add more symbols for better demonstration
        QStringList symbols = {
            "RELIANCE", "TCS", "INFY", "HDFCBANK", "ICICIBANK",
            "HINDUNILVR", "ITC", "SBIN", "BHARTIARTL", "KOTAKBANK",
            "LT", "AXISBANK", "ASIANPAINT", "MARUTI", "TITAN"
        };
        m_symbolCombo->addItems(symbols);
    }
    else
    {
        QStringList symbols = {
            "NIFTY", "BANKNIFTY", "FINNIFTY", "MIDCPNIFTY"
        };
        m_symbolCombo->addItems(symbols);
    }
    onSymbolChanged(m_symbolCombo->currentText());
}

void ScripBar::populateExpiries(const QString &symbol)
{
    Q_UNUSED(symbol)
    m_expiryCombo->clearItems();
    // Add more expiry dates for demonstration (chronological order will be applied)
    QStringList expiries = {
        "19-Dec-2024", "26-Dec-2024", "02-Jan-2025", "09-Jan-2025",
        "16-Jan-2025", "23-Jan-2025", "30-Jan-2025", "06-Feb-2025",
        "13-Feb-2025", "27-Feb-2025"
    };
    m_expiryCombo->addItems(expiries);
    onExpiryChanged(0);
}

void ScripBar::populateStrikes(const QString &expiry)
{
    Q_UNUSED(expiry)
    m_strikeCombo->clearItems();
    // Add more strike prices for demonstration (numeric sort will apply)
    QStringList strikes = {
        "17000", "17500", "18000", "18500", "19000", "19500",
        "20000", "20500", "21000", "21500", "22000", "22500",
        "23000", "23500", "24000", "24500", "25000"
    };
    m_strikeCombo->addItems(strikes);
    onStrikeChanged(0);
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
void ScripBar::onExchangeChanged(const QString &text) {
    m_currentExchange = text;
    populateSegments(text);
}

void ScripBar::onSegmentChanged(const QString &text) {
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
    Q_UNUSED(text)
    populateExpiries(text);
}
void ScripBar::onExpiryChanged(int) { populateStrikes(m_expiryCombo->currentText()); }
void ScripBar::onStrikeChanged(int) { populateOptionTypes(m_strikeCombo->currentText()); }
void ScripBar::onOptionTypeChanged(int) {}

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
    qDebug() << "Add to watch requested:" << symbol;
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
