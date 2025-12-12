#include "ui/ScripBar.h"
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QPushButton>
#include <QDebug>

ScripBar::ScripBar(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    populateExchanges();
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
    setMaximumHeight(15);
#endif

    // Exchange button + menu
    m_exchangeBtn = new QToolButton(this);
    m_exchangeBtn->setPopupMode(QToolButton::InstantPopup);
    m_exchangeBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_exchangeBtn->setMinimumWidth(56);
    m_exchangeMenu = new QMenu(m_exchangeBtn);
    m_exchangeBtn->setMenu(m_exchangeMenu);
    m_layout->addWidget(m_exchangeBtn);

    // Segment button + menu
    m_segmentBtn = new QToolButton(this);
    m_segmentBtn->setPopupMode(QToolButton::InstantPopup);
    m_segmentBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_segmentBtn->setMinimumWidth(48);
    m_segmentMenu = new QMenu(m_segmentBtn);
    m_segmentBtn->setMenu(m_segmentMenu);
    m_layout->addWidget(m_segmentBtn);

    // Instrument combo
    m_instrumentCombo = new QComboBox(this);
    m_instrumentCombo->setMinimumWidth(80);
    m_layout->addWidget(m_instrumentCombo);
    connect(m_instrumentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScripBar::onInstrumentChanged);

    // Symbol (editable)
    m_symbolCombo = new QComboBox(this);
    m_symbolCombo->setEditable(true);
    m_symbolCombo->setMinimumWidth(100);
    m_layout->addWidget(m_symbolCombo);
    connect(m_symbolCombo, &QComboBox::currentTextChanged, this, &ScripBar::onSymbolChanged);

    // Expiry
    m_expiryCombo = new QComboBox(this);
    m_expiryCombo->setMinimumWidth(80);
    m_layout->addWidget(m_expiryCombo);
    connect(m_expiryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScripBar::onExpiryChanged);

    // Strike
    m_strikeCombo = new QComboBox(this);
    m_strikeCombo->setMinimumWidth(64);
    m_layout->addWidget(m_strikeCombo);
    connect(m_strikeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScripBar::onStrikeChanged);

    // Option Type
    m_optionTypeCombo = new QComboBox(this);
    m_optionTypeCombo->setMinimumWidth(48);
    m_layout->addWidget(m_optionTypeCombo);
    connect(m_optionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScripBar::onOptionTypeChanged);

    // Add to Watch
    m_addToWatchButton = new QPushButton(tr("Add"), this);
    m_addToWatchButton->setMinimumWidth(64);
    m_layout->addWidget(m_addToWatchButton);
    connect(m_addToWatchButton, &QPushButton::clicked, this, &ScripBar::onAddToWatchClicked);

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
    m_exchangeMenu->clear();
    struct Item
    {
        const char *text;
        const char *data;
    } items[] = {{"NSE", "NSE"}, {"BSE", "BSE"}, {"MCX", "MCX"}};
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
    // default
    m_currentExchange = QString("NSE");
    m_exchangeBtn->setText(m_currentExchange);
    populateSegments(m_currentExchange);
}

void ScripBar::populateSegments(const QString &exchange)
{
    m_segmentMenu->clear();
    QStringList segs;
    if (exchange == "NSE")
        segs << "CM" << "FO" << "CD";
    else if (exchange == "BSE")
        segs << "CM" << "FO";
    else
        segs << "CM";

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
    // default
    m_currentSegment = segs.isEmpty() ? QString() : segs.first();
    m_segmentBtn->setText(m_currentSegment);
    populateInstruments(m_currentSegment);
}

void ScripBar::populateInstruments(const QString &segment)
{
    m_instrumentCombo->clear();
    if (segment == "CM")
        m_instrumentCombo->addItem("EQUITY", "EQUITY");
    else if (segment == "FO")
    {
        m_instrumentCombo->addItem("FUTIDX", "FUTIDX");
        m_instrumentCombo->addItem("OPTIDX", "OPTIDX");
    }
    else
        m_instrumentCombo->addItem("EQUITY", "EQUITY");
    onInstrumentChanged(0);
}

void ScripBar::populateSymbols(const QString &instrument)
{
    m_symbolCombo->clear();
    if (instrument == "EQUITY")
    {
        m_symbolCombo->addItem("RELIANCE");
        m_symbolCombo->addItem("INFY");
    }
    else
    {
        m_symbolCombo->addItem("NIFTY");
        m_symbolCombo->addItem("BANKNIFTY");
    }
    onSymbolChanged(m_symbolCombo->currentText());
}

void ScripBar::populateExpiries(const QString &symbol)
{
    Q_UNUSED(symbol)
    m_expiryCombo->clear();
    m_expiryCombo->addItem("26-Dec-2024", "26-Dec-2024");
    m_expiryCombo->addItem("30-Jan-2025", "30-Jan-2025");
    onExpiryChanged(0);
}

void ScripBar::populateStrikes(const QString &expiry)
{
    Q_UNUSED(expiry)
    m_strikeCombo->clear();
    m_strikeCombo->addItem("18000", "18000");
    m_strikeCombo->addItem("18500", "18500");
    onStrikeChanged(0);
}

void ScripBar::populateOptionTypes(const QString &strike)
{
    Q_UNUSED(strike)
    m_optionTypeCombo->clear();
    m_optionTypeCombo->addItem("CE", "CE");
    m_optionTypeCombo->addItem("PE", "PE");
}

// Slot implementations
void ScripBar::onExchangeChanged(int) {}
void ScripBar::onSegmentChanged(int) {}
void ScripBar::onInstrumentChanged(int index)
{
    Q_UNUSED(index)
    QString inst = m_instrumentCombo->currentData().toString();
    populateSymbols(inst);
}
void ScripBar::onSymbolChanged(const QString &text)
{
    Q_UNUSED(text)
    populateExpiries(text);
}
void ScripBar::onExpiryChanged(int) { populateStrikes(m_expiryCombo->currentData().toString()); }
void ScripBar::onStrikeChanged(int) { populateOptionTypes(m_strikeCombo->currentData().toString()); }
void ScripBar::onOptionTypeChanged(int) {}

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
