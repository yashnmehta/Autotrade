#include "ui/ScripBar.h"
#include <QDebug>
#include <QToolButton>
#include "ui/ScripBar.h"
#include <QDebug>
#include <QToolButton>
#include <QMenu>
#include <QAction>

ScripBar::ScripBar(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    populateExchanges();
}

ScripBar::~ScripBar()
{
}

void ScripBar::setupUI()
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(6, 2, 6, 2);
    m_layout->setSpacing(6);
    m_layout->setAlignment(Qt::AlignVCenter);

    // Prefer compact fixed height suitable for toolbars
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMaximumHeight(32);
#ifdef Q_OS_MAC
    // On mac we want an ultra-compact toolbar embedding
    setMaximumHeight(15);
#endif

    // Exchange as compact button with menu
    m_exchangeBtn = new QToolButton(this);
    m_exchangeBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_exchangeBtn->setPopupMode(QToolButton::InstantPopup);
    m_exchangeBtn->setMinimumWidth(72);
    m_exchangeBtn->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_exchangeBtn->setMaximumHeight(15);
#endif
    m_exchangeMenu = new QMenu(m_exchangeBtn);
    m_exchangeBtn->setMenu(m_exchangeMenu);
    m_exchangeBtn->setText("NSE");
    m_layout->addWidget(m_exchangeBtn);

    // Segment as compact button with menu
    m_segmentBtn = new QToolButton(this);
    m_segmentBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_segmentBtn->setPopupMode(QToolButton::InstantPopup);
    m_segmentBtn->setMinimumWidth(64);
    m_segmentBtn->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_segmentBtn->setMaximumHeight(15);
#endif
    m_segmentMenu = new QMenu(m_segmentBtn);
    m_segmentBtn->setMenu(m_segmentMenu);
    m_segmentBtn->setText("CM");
    m_layout->addWidget(m_segmentBtn);

    // Instrument (no label)
    m_instrumentCombo = new QComboBox(this);
    m_instrumentCombo->setMinimumWidth(88);
    m_instrumentCombo->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_instrumentCombo->setMaximumHeight(15);
#endif
    connect(m_instrumentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onInstrumentChanged);
    m_layout->addWidget(m_instrumentCombo);

    // Symbol (no label)
    m_symbolCombo = new QComboBox(this);
    m_symbolCombo->setMinimumWidth(110);
    m_symbolCombo->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_symbolCombo->setMaximumHeight(15);
#endif
    m_symbolCombo->setEditable(true);
    connect(m_symbolCombo, &QComboBox::currentTextChanged,
            this, &ScripBar::onSymbolChanged);
    m_layout->addWidget(m_symbolCombo);

    // Expiry (no label)
    m_expiryCombo = new QComboBox(this);
    m_expiryCombo->setMinimumWidth(84);
    m_expiryCombo->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_expiryCombo->setMaximumHeight(15);
#endif
    connect(m_expiryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onExpiryChanged);
    m_layout->addWidget(m_expiryCombo);

    // Strike (no label)
    m_strikeCombo = new QComboBox(this);
    m_strikeCombo->setMinimumWidth(72);
    m_strikeCombo->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_strikeCombo->setMaximumHeight(15);
#endif
    connect(m_strikeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onStrikeChanged);
    m_layout->addWidget(m_strikeCombo);

    // Option Type (no label)
    m_optionTypeCombo = new QComboBox(this);
    m_optionTypeCombo->setMinimumWidth(58);
    m_optionTypeCombo->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_optionTypeCombo->setMaximumHeight(15);
#endif
    connect(m_optionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onOptionTypeChanged);
    m_layout->addWidget(m_optionTypeCombo);

    // Add to Watch button
    m_addToWatchButton = new QPushButton("Add to Watch", this);
    m_addToWatchButton->setMinimumWidth(86);
    m_addToWatchButton->setMaximumHeight(24);
    m_addToWatchButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
#ifdef Q_OS_MAC
    m_addToWatchButton->setMaximumHeight(15);
#endif
    connect(m_addToWatchButton, &QPushButton::clicked, this, &ScripBar::onAddToWatchClicked);
    m_layout->addWidget(m_addToWatchButton);

    m_layout->addStretch();

    // Ensure the widget prefers a compact toolbar-friendly height and vertical centering
    setContentsMargins(0, 0, 0, 0);

    // Dark theme styling
    setStyleSheet(
        "QWidget { "
        "   background-color: #2d2d30; "
        "   border-bottom: 1px solid #3e3e42; "
        "} "
        "QLabel { "
        "   color: #cccccc; "
        "   font-size: 10px; "
        "} "
        "QComboBox { "
        "   background-color: #1e1e1e; "
        "   color: #ffffff; "
        "   border: none; "
        "   padding: 2px 6px; "
        "   min-height: 16px; "
        "   max-height: 24px; "
        "} "
        "QToolButton { "
        "   background-color: transparent; "
        "   color: #ffffff; "
        "   border: none; "
        "   padding: 2px 6px; "
        "} "
        "QComboBox::drop-down { "
        "   border: none; "
        "} "
        "QComboBox::down-arrow { "
        "   image: url(:/icons/down_arrow.png); "
        "   width: 12px; "
        "   height: 12px; "
        "} "
        "QComboBox QAbstractItemView { "
        "   background-color: #1e1e1e; "
        "   color: #ffffff; "
        "   selection-background-color: #094771; "
        "} "
        "QPushButton { "
        "   background-color: #0e639c; "
        "   color: #ffffff; "
        "   border: none; "
        "   padding: 2px 6px; "
        "   min-height: 16px; "
        "   max-height: 24px; "
        "   border-radius: 2px; "
        "} "
        "QPushButton:hover { "
        "   background-color: #1177bb; "
        "} "
        "QPushButton:pressed { "
        "   background-color: #094771; "
        "}");

    // Populate exchange/segment menus after creating UI
    populateExchanges();
}

void ScripBar::populateExchanges()
{
    m_exchangeMenu->clear();
    struct Item
    {
        const char *text;
        const char *data;
    } items[] = {
        {"NSE", "NSE"},
        {"BSE", "BSE"},
        {"MCX", "MCX"}};

    for (auto &it : items)
    {
        QAction *a = m_exchangeMenu->addAction(QString::fromUtf8(it.text));
        a->setData(QString::fromUtf8(it.data));
        connect(a, &QAction::triggered, this, [this, a]()
                {
            QString ex = a->data().toString();
            m_exchangeBtn->setText(ex);
            m_currentExchange = ex;
            populateSegments(ex); });
    }

    // select default
    m_currentExchange = QString("NSE");
    m_exchangeBtn->setText(m_currentExchange);
    populateSegments(m_currentExchange);
}

void ScripBar::onExchangeChanged(int index)
{
    Q_UNUSED(index)
}

void ScripBar::populateSegments(const QString &exchange)
{
    m_segmentMenu->clear();

    if (exchange == "NSE")
    {
        QStringList segs = {"CM", "FO", "CD"};
        for (const QString &s : segs)
        {
            QAction *a = m_segmentMenu->addAction(s);
            a->setData(s);
            connect(a, &QAction::triggered, this, [this, a]()
                    {
                QString seg = a->data().toString();
                m_segmentBtn->setText(seg);
                m_currentSegment = seg;
                populateInstruments(seg); });
        }
    }
    else if (exchange == "BSE")
    {
        QStringList segs = {"CM", "FO"};
        for (const QString &s : segs)
        {
            QAction *a = m_segmentMenu->addAction(s);
            a->setData(s);
            connect(a, &QAction::triggered, this, [this, a]()
                    {
                QString seg = a->data().toString();
                m_segmentBtn->setText(seg);
                m_currentSegment = seg;
                populateInstruments(seg); });
        }
    }
    else if (exchange == "MCX")
    {
        QAction *a = m_segmentMenu->addAction("CM");
        a->setData("CM");
        connect(a, &QAction::triggered, this, [this, a]()
                {
            QString seg = a->data().toString();
            m_segmentBtn->setText(seg);
            m_currentSegment = seg;
            populateInstruments(seg); });
    }

    // default selection
    m_currentSegment = m_segmentMenu->actions().isEmpty() ? QString() : m_segmentMenu->actions().first()->data().toString();
    if (!m_currentSegment.isEmpty())
    {
        m_segmentBtn->setText(m_currentSegment);
        populateInstruments(m_currentSegment);
    }
}

void ScripBar::onSegmentChanged(int index)
{
    Q_UNUSED(index)
}

void ScripBar::populateInstruments(const QString &segment)
{
    m_instrumentCombo->clear();

    if (segment == "CM")
    {
        m_instrumentCombo->addItem("EQUITY", "EQUITY");
    }
    else if (segment == "FO")
    {
        m_instrumentCombo->addItem("FUTIDX", "FUTIDX");
        m_instrumentCombo->addItem("FUTSTK", "FUTSTK");
        m_instrumentCombo->addItem("OPTIDX", "OPTIDX");
        m_instrumentCombo->addItem("OPTSTK", "OPTSTK");
    }
    else if (segment == "CD")
    {
        m_instrumentCombo->addItem("FUTCUR", "FUTCUR");
        m_instrumentCombo->addItem("OPTCUR", "OPTCUR");
    }

    // Trigger symbol population
    onInstrumentChanged(0);
}

void ScripBar::onInstrumentChanged(int index)
{
    if (index < 0)
        return;
    QString instrument = m_instrumentCombo->itemData(index).toString();
    populateSymbols(instrument);
}

void ScripBar::populateSymbols(const QString &instrument)
{
    m_symbolCombo->clear();

    // Sample symbols based on instrument type
    if (instrument == "EQUITY")
    {
        m_symbolCombo->addItem("RELIANCE");
        m_symbolCombo->addItem("INFY");
        m_symbolCombo->addItem("TCS");
        m_symbolCombo->addItem("HDFCBANK");
        m_symbolCombo->addItem("NIFTY");
    }
    else if (instrument.startsWith("FUT"))
    {
        m_symbolCombo->addItem("NIFTY");
        m_symbolCombo->addItem("BANKNIFTY");
        m_symbolCombo->addItem("RELIANCE");
    }
    else if (instrument.startsWith("OPT"))
    {
        m_symbolCombo->addItem("NIFTY");
        m_symbolCombo->addItem("BANKNIFTY");
    }

    // Trigger expiry population
    onSymbolChanged(0);
}

void ScripBar::onSymbolChanged(const QString &text)
{
    QString symbol = text;
    if (symbol.isEmpty())
        return;

    QString instrument = m_instrumentCombo->currentData().toString();
    populateExpiries(symbol);
}

void ScripBar::populateExpiries(const QString &symbol)
{
    m_expiryCombo->clear();

    // Sample expiries (would come from API)
    m_expiryCombo->addItem("26-Dec-2024", "26-Dec-2024");
    m_expiryCombo->addItem("30-Jan-2025", "30-Jan-2025");
    m_expiryCombo->addItem("27-Feb-2025", "27-Feb-2025");

    // Trigger strike population
    onExpiryChanged(0);
}

void ScripBar::onExpiryChanged(int index)
{
    if (index < 0)
        return;
    QString expiry = m_expiryCombo->itemData(index).toString();
    populateStrikes(expiry);
}

void ScripBar::populateStrikes(const QString &expiry)
{
    m_strikeCombo->clear();

    QString instrument = m_instrumentCombo->currentData().toString();

    if (instrument.startsWith("OPT"))
    {
        // Sample strikes for options
        m_strikeCombo->addItem("18000", "18000");
        m_strikeCombo->addItem("18500", "18500");
        m_strikeCombo->addItem("19000", "19000");
        m_strikeCombo->addItem("19500", "19500");
    }

    // Trigger option type population
    onStrikeChanged(0);
}

void ScripBar::onStrikeChanged(int index)
{
    if (index < 0)
        return;
    QString strike = m_strikeCombo->itemData(index).toString();
    populateOptionTypes(strike);
}

void ScripBar::populateOptionTypes(const QString &strike)
{
    m_optionTypeCombo->clear();

    QString instrument = m_instrumentCombo->currentData().toString();

    if (instrument.startsWith("OPT"))
    {
        m_optionTypeCombo->addItem("CE", "CE");
        m_optionTypeCombo->addItem("PE", "PE");
    }
}

void ScripBar::onOptionTypeChanged(int index)
{
    Q_UNUSED(index)
}

void ScripBar::onAddToWatchClicked()
{
    QString exchange = m_currentExchange;
    QString segment = m_currentSegment;
    QString instrument = m_instrumentCombo->currentData().toString();
    QString symbol = m_symbolCombo->currentText();
    QString expiry = m_expiryCombo->currentData().toString();
    QString strike = m_strikeCombo->currentData().toString();
    QString optionType = m_optionTypeCombo->currentData().toString();

    emit addToWatchRequested(exchange, segment, instrument, symbol, expiry, strike, optionType);

    qDebug() << "Add to watch requested:" << symbol;
}

#include <QMenu>
#include <QAction>

ScripBar::ScripBar(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    // Instrument (label removed for compact toolbar)
    m_instrumentCombo = new QComboBox(this);
}

void ScripBar::setupUI()
{
    m_layout = new QHBoxLayout(this);
    // Symbol (label removed)
    m_symbolCombo = new QComboBox(this);
    setMaximumHeight(32);
#ifdef Q_OS_MAC
    // On mac we want an ultra-compact toolbar embedding
    setMaximumHeight(15);
#endif

    // Expiry (label removed)
    m_expiryCombo = new QComboBox(this);
#ifdef Q_OS_MAC
    m_exchangeBtn->setMaximumHeight(15);
#endif
    m_exchangeMenu = new QMenu(m_exchangeBtn);
    m_exchangeBtn->setMenu(m_exchangeMenu);
    // Strike (label removed)
    m_strikeCombo = new QComboBox(this);
    m_segmentBtn->setPopupMode(QToolButton::InstantPopup);
    m_segmentBtn->setMinimumWidth(64);
    m_segmentBtn->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_segmentBtn->setMaximumHeight(15);
    // Option Type (label removed)
    m_optionTypeCombo = new QComboBox(this);
    // Instrument
    QLabel *instrumentLabel = new QLabel("Instrument:", this);
    m_layout->addWidget(instrumentLabel);

    m_instrumentCombo = new QComboBox(this);
    m_instrumentCombo->setMinimumWidth(88);
    m_instrumentCombo->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_instrumentCombo->setMaximumHeight(15);
#endif
    connect(m_instrumentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onInstrumentChanged);
    m_layout->addWidget(m_instrumentCombo);

    // Symbol
    QLabel *symbolLabel = new QLabel("Symbol:", this);
    m_layout->addWidget(symbolLabel);

    m_symbolCombo = new QComboBox(this);
    m_symbolCombo->setMinimumWidth(110);
    m_symbolCombo->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_symbolCombo->setMaximumHeight(15);
#endif
    m_symbolCombo->setEditable(true);
    connect(m_symbolCombo, &QComboBox::currentTextChanged,
            this, &ScripBar::onSymbolChanged);
    m_layout->addWidget(m_symbolCombo);

    // Expiry
    QLabel *expiryLabel = new QLabel("Expiry:", this);
    m_layout->addWidget(expiryLabel);

    m_expiryCombo = new QComboBox(this);
    m_expiryCombo->setMinimumWidth(84);
    m_expiryCombo->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_expiryCombo->setMaximumHeight(15);
#endif
    connect(m_expiryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onExpiryChanged);
    m_layout->addWidget(m_expiryCombo);

    // Strike
    QLabel *strikeLabel = new QLabel("Strike:", this);
    m_layout->addWidget(strikeLabel);

    m_strikeCombo = new QComboBox(this);
    m_strikeCombo->setMinimumWidth(72);
    m_strikeCombo->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_strikeCombo->setMaximumHeight(15);
#endif
    connect(m_strikeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onStrikeChanged);
    m_layout->addWidget(m_strikeCombo);

    // Option Type
    QLabel *optionTypeLabel = new QLabel("Type:", this);
    m_layout->addWidget(optionTypeLabel);

    m_optionTypeCombo = new QComboBox(this);
    m_optionTypeCombo->setMinimumWidth(58);
    m_optionTypeCombo->setMaximumHeight(22);
#ifdef Q_OS_MAC
    m_optionTypeCombo->setMaximumHeight(15);
#endif
    connect(m_optionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScripBar::onOptionTypeChanged);
    m_layout->addWidget(m_optionTypeCombo);

    // Add to Watch button
    m_addToWatchButton = new QPushButton("Add to Watch", this);
    m_addToWatchButton->setMinimumWidth(86);
    m_addToWatchButton->setMaximumHeight(24);
    m_addToWatchButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
#ifdef Q_OS_MAC
    m_addToWatchButton->setMaximumHeight(15);
#endif
    connect(m_addToWatchButton, &QPushButton::clicked, this, &ScripBar::onAddToWatchClicked);
    m_layout->addWidget(m_addToWatchButton);

    m_layout->addStretch();

    // Ensure the widget prefers a compact toolbar-friendly height and vertical centering
    setContentsMargins(0, 0, 0, 0);

    // Dark theme styling
    setStyleSheet(
        "QWidget { "
        "   background-color: #2d2d30; "
        "   border-bottom: 1px solid #3e3e42; "
        "} "
        "QLabel { "
        "   color: #cccccc; "
        "   font-size: 10px; "
        "} "
        "QComboBox { "
        "   background-color: #1e1e1e; "
        "   color: #ffffff; "
        "   border: none; "
        "   padding: 2px 6px; "
        "   min-height: 16px; "
        "   max-height: 24px; "
        "} "
        "QToolButton { "
        "   background-color: transparent; "
        "   color: #ffffff; "
        "   border: none; "
        "   padding: 2px 6px; "
        "} "
        "QComboBox::drop-down { "
        "   border: none; "
        "} "
        "QComboBox::down-arrow { "
        "   image: url(:/icons/down_arrow.png); "
        "   width: 12px; "
        "   height: 12px; "
        "} "
        "QComboBox QAbstractItemView { "
        "   background-color: #1e1e1e; "
        "   color: #ffffff; "
        "   selection-background-color: #094771; "
        "} "
        "QPushButton { "
        "   background-color: #0e639c; "
        "   color: #ffffff; "
        "   border: none; "
        "   padding: 2px 6px; "
        "   min-height: 16px; "
        "   max-height: 24px; "
        "   border-radius: 2px; "
        "} "
        "QPushButton:hover { "
        "   background-color: #1177bb; "
        "} "
        "QPushButton:pressed { "
        "   background-color: #094771; "
        "}");

    // Populate exchange/segment menus after creating UI
    populateExchanges();
}

void ScripBar::populateExchanges()
{
    m_exchangeMenu->clear();
    struct Item
    {
        const char *text;
        const char *data;
    } items[] = {
        {"NSE", "NSE"},
        {"BSE", "BSE"},
        {"MCX", "MCX"}};

    for (auto &it : items)
    {
        QAction *a = m_exchangeMenu->addAction(QString::fromUtf8(it.text));
        a->setData(QString::fromUtf8(it.data));
        connect(a, &QAction::triggered, this, [this, a]()
                {
            QString ex = a->data().toString();
            m_exchangeBtn->setText(ex);
            m_currentExchange = ex;
            populateSegments(ex); });
    }

    // select default
    m_currentExchange = QString("NSE");
    m_exchangeBtn->setText(m_currentExchange);
    populateSegments(m_currentExchange);
}

void ScripBar::onExchangeChanged(int index)
{
    if (index < 0)
        return;
    QString exchange = m_exchangeCombo->itemData(index).toString();
    populateSegments(exchange);
}

void ScripBar::populateSegments(const QString &exchange)
{
    m_segmentMenu->clear();

    if (exchange == "NSE")
    {
        QStringList segs = {"CM", "FO", "CD"};
        for (const QString &s : segs)
        {
            QAction *a = m_segmentMenu->addAction(s);
            a->setData(s);
            connect(a, &QAction::triggered, this, [this, a]()
                    {
                QString seg = a->data().toString();
                m_segmentBtn->setText(seg);
                m_currentSegment = seg;
                populateInstruments(seg); });
        }
    }
    else if (exchange == "BSE")
    {
        QStringList segs = {"CM", "FO"};
        for (const QString &s : segs)
        {
            QAction *a = m_segmentMenu->addAction(s);
            a->setData(s);
            connect(a, &QAction::triggered, this, [this, a]()
                    {
                QString seg = a->data().toString();
                m_segmentBtn->setText(seg);
                m_currentSegment = seg;
                populateInstruments(seg); });
        }
    }
    else if (exchange == "MCX")
    {
        QAction *a = m_segmentMenu->addAction("CM");
        a->setData("CM");
        connect(a, &QAction::triggered, this, [this, a]()
                {
            QString seg = a->data().toString();
            m_segmentBtn->setText(seg);
            m_currentSegment = seg;
            populateInstruments(seg); });
    }

    // default selection
    m_currentSegment = m_segmentMenu->actions().isEmpty() ? QString() : m_segmentMenu->actions().first()->data().toString();
    if (!m_currentSegment.isEmpty())
    {
        m_segmentBtn->setText(m_currentSegment);
        populateInstruments(m_currentSegment);
    }
}

void ScripBar::onSegmentChanged(int index)
{
    if (index < 0)
        return;
    QString segment = m_segmentCombo->itemData(index).toString();
    populateInstruments(segment);
}

void ScripBar::populateInstruments(const QString &segment)
{
    m_instrumentCombo->clear();

    if (segment == "CM")
    {
        m_instrumentCombo->addItem("EQUITY", "EQUITY");
    }
    else if (segment == "FO")
    {
        m_instrumentCombo->addItem("FUTIDX", "FUTIDX");
        m_instrumentCombo->addItem("FUTSTK", "FUTSTK");
        m_instrumentCombo->addItem("OPTIDX", "OPTIDX");
        m_instrumentCombo->addItem("OPTSTK", "OPTSTK");
    }
    else if (segment == "CD")
    {
        m_instrumentCombo->addItem("FUTCUR", "FUTCUR");
        m_instrumentCombo->addItem("OPTCUR", "OPTCUR");
    }

    // Trigger symbol population
    onInstrumentChanged(0);
}

void ScripBar::onInstrumentChanged(int index)
{
    if (index < 0)
        return;
    QString instrument = m_instrumentCombo->itemData(index).toString();
    populateSymbols(instrument);
}

void ScripBar::populateSymbols(const QString &instrument)
{
    m_symbolCombo->clear();

    // Sample symbols based on instrument type
    if (instrument == "EQUITY")
    {
        m_symbolCombo->addItem("RELIANCE");
        m_symbolCombo->addItem("INFY");
        m_symbolCombo->addItem("TCS");
        m_symbolCombo->addItem("HDFCBANK");
        m_symbolCombo->addItem("NIFTY");
    }
    else if (instrument.startsWith("FUT"))
    {
        m_symbolCombo->addItem("NIFTY");
        m_symbolCombo->addItem("BANKNIFTY");
        m_symbolCombo->addItem("RELIANCE");
    }
    else if (instrument.startsWith("OPT"))
    {
        m_symbolCombo->addItem("NIFTY");
        m_symbolCombo->addItem("BANKNIFTY");
    }

    // Trigger expiry population
    onSymbolChanged(0);
}

void ScripBar::onSymbolChanged(const QString &text)
{
    QString symbol = text;
    if (symbol.isEmpty())
        return;

    QString instrument = m_instrumentCombo->currentData().toString();
    populateExpiries(symbol);
}

void ScripBar::populateExpiries(const QString &symbol)
{
    m_expiryCombo->clear();

    // Sample expiries (would come from API)
    m_expiryCombo->addItem("26-Dec-2024", "26-Dec-2024");
    m_expiryCombo->addItem("30-Jan-2025", "30-Jan-2025");
    m_expiryCombo->addItem("27-Feb-2025", "27-Feb-2025");

    // Trigger strike population
    onExpiryChanged(0);
}

void ScripBar::onExpiryChanged(int index)
{
    if (index < 0)
        return;
    QString expiry = m_expiryCombo->itemData(index).toString();
    populateStrikes(expiry);
}

void ScripBar::populateStrikes(const QString &expiry)
{
    m_strikeCombo->clear();

    QString instrument = m_instrumentCombo->currentData().toString();

    if (instrument.startsWith("OPT"))
    {
        // Sample strikes for options
        m_strikeCombo->addItem("18000", "18000");
        m_strikeCombo->addItem("18500", "18500");
        m_strikeCombo->addItem("19000", "19000");
        m_strikeCombo->addItem("19500", "19500");
    }

    // Trigger option type population
    onStrikeChanged(0);
}

void ScripBar::onStrikeChanged(int index)
{
    if (index < 0)
        return;
    QString strike = m_strikeCombo->itemData(index).toString();
    populateOptionTypes(strike);
}

void ScripBar::populateOptionTypes(const QString &strike)
{
    m_optionTypeCombo->clear();

    QString instrument = m_instrumentCombo->currentData().toString();

    if (instrument.startsWith("OPT"))
    {
        m_optionTypeCombo->addItem("CE", "CE");
        m_optionTypeCombo->addItem("PE", "PE");
    }
}

void ScripBar::onOptionTypeChanged(int index)
{
    // Option type changed - ready for add to watch
}

void ScripBar::onAddToWatchClicked()
{
    QString exchange = m_exchangeCombo->currentData().toString();
    QString segment = m_segmentCombo->currentData().toString();
    QString instrument = m_instrumentCombo->currentData().toString();
    QString symbol = m_symbolCombo->currentText();
    QString expiry = m_expiryCombo->currentData().toString();
    QString strike = m_strikeCombo->currentData().toString();
    QString optionType = m_optionTypeCombo->currentData().toString();

    emit addToWatchRequested(exchange, segment, instrument, symbol, expiry, strike, optionType);

    qDebug() << "Add to watch requested:" << symbol;
}