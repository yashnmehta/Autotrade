#include "views/BuyWindow.h"
#include "views/SellWindow.h"
#include "utils/PreferencesManager.h"
#include "repository/RepositoryManager.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QApplication>
#include <cmath>
#include <QKeyEvent>
#include "core/widgets/CustomMDISubWindow.h"
#include "utils/WindowSettingsHelper.h"
#include "utils/TableProfileHelper.h"

// Static member initialization
BuyWindow* BuyWindow::s_instance = nullptr;

BuyWindow::BuyWindow(QWidget *parent)
    : QWidget(parent), m_formWidget(nullptr)
{
    qDebug() << "[BuyWindow::Constructor] Creating BuyWindow instance. Current s_instance:" << s_instance;
    
    // Set this as the current instance
    setInstance(this);
    // Load UI file
    QUiLoader loader;
    QFile file(":/forms/BuyWindow.ui");

    if (!file.open(QFile::ReadOnly))
    {
        qWarning() << "[BuyWindow] Failed to open UI file";
        return;
    }

    m_formWidget = loader.load(&file, this);
    file.close();

    if (!m_formWidget)
    {
        qWarning() << "[BuyWindow] Failed to load UI";
        return;
    }

    // set minimum width 1200
    setMinimumWidth(1200);
    setMinimumHeight(200);

    // Set focus policy to keep tab cycling within this window
    setFocusPolicy(Qt::StrongFocus);
    m_formWidget->setFocusPolicy(Qt::StrongFocus);

    // Set layout
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_formWidget);

    // Find widgets - Row 1 (matching new layout)
    m_cbOrdType = m_formWidget->findChild<QComboBox *>("cbOrdType");
    m_leCol = m_formWidget->findChild<QLineEdit *>("leCol");
    m_cbInstrName = m_formWidget->findChild<QComboBox *>("cbInstrName");
    m_leSymbol = m_formWidget->findChild<QLineEdit *>("leSymbol");
    m_cbExp = m_formWidget->findChild<QComboBox *>("cbExp");
    m_cbStrike = m_formWidget->findChild<QComboBox *>("cbStrike");
    m_cbOptType = m_formWidget->findChild<QComboBox *>("cbOptType");
    m_leQty = m_formWidget->findChild<QLineEdit *>("leQty");
    m_leRate = m_formWidget->findChild<QLineEdit *>("leRate");
    m_leProdPercent = m_formWidget->findChild<QLineEdit *>("leProdPercent");
    m_cbOC = m_formWidget->findChild<QComboBox *>("cbOC");
    m_cbProCli = m_formWidget->findChild<QComboBox *>("cbProCli");
    m_cbCPBroker = m_formWidget->findChild<QComboBox *>("cbCPBroker");
    m_cbMFAON = m_formWidget->findChild<QComboBox *>("cbMFAON");
    m_leMEQty = m_formWidget->findChild<QLineEdit *>("leMEQty");

    // Find widgets - Row 2 (matching new layout)
    m_leSubBroker = m_formWidget->findChild<QLineEdit *>("leSubBroker");
    m_leClient = m_formWidget->findChild<QLineEdit *>("leClient");
    m_leSetflor = m_formWidget->findChild<QLineEdit *>("leSetflor");
    m_leTrigPrice = m_formWidget->findChild<QLineEdit *>("leTrigPrice");
    m_cbOrderType2 = m_formWidget->findChild<QComboBox *>("cbOrderType2");
    m_cbValidity = m_formWidget->findChild<QComboBox *>("cbValidity");
    m_leRemarks = m_formWidget->findChild<QLineEdit *>("leRemarks");
    m_pbAMO = m_formWidget->findChild<QPushButton *>("pbAMO");
    m_pbSubmit = m_formWidget->findChild<QPushButton *>("pbSubmit");
    m_pbClear = m_formWidget->findChild<QPushButton *>("pbClear");

    // Legacy widgets (for compatibility)
    m_cbEx = m_formWidget->findChild<QComboBox *>("cbEx");
    m_leToken = m_formWidget->findChild<QLineEdit *>("leToken");
    m_leInsType = m_formWidget->findChild<QLineEdit *>("leInsType");
    m_leDiscloseQty = m_formWidget->findChild<QLineEdit *>("leDiscloseQty");
    m_leMFQty = m_formWidget->findChild<QLineEdit *>("leMFQty");
    m_cbProduct = m_formWidget->findChild<QComboBox *>("cbProduct");

    populateComboBoxes();
    setupConnections();
    loadPreferences();
    
    // Load and apply previous runtime state and customizations
    WindowSettingsHelper::loadAndApplyWindowSettings(this, "BuyWindow");
    
    // Set initial focus to Total Qty field and select all text for immediate editing
    if (m_leQty) {
        m_leQty->setFocus();
        m_leQty->selectAll();
    }

    qDebug() << "[BuyWindow] Created successfully";
}

BuyWindow::BuyWindow(const WindowContext &context, QWidget *parent)
    : BuyWindow(parent)  // Delegate to default constructor
{
    // Load context after UI is initialized
    loadFromContext(context);
    qDebug() << "[BuyWindow] Created with context:" << context.toString();
}

BuyWindow::~BuyWindow()
{
    qDebug() << "[BuyWindow::Destructor] Destroying BuyWindow instance. s_instance:" << s_instance << "this:" << this;
    
    // Clear the singleton instance when this window is destroyed
    if (s_instance == this) {
        s_instance = nullptr;
        qDebug() << "[BuyWindow::Destructor] Cleared s_instance";
    }
}

void BuyWindow::populateComboBoxes()
{
    if (m_cbEx)
    {
        m_cbEx->addItems({"NSE", "BSE", "MCX"});
    }

    if (m_cbInstrName)
    {
        m_cbInstrName->addItems({"RL", "FUTIDX", "FUTSTK", "OPTIDX", "OPTSTK"});
    }

    if (m_cbOrdType)
    {
        m_cbOrdType->addItems({"LIMIT", "MARKET", "SL", "SL-M"});
    }
    
    if (m_cbOrderType2)
    {
        m_cbOrderType2->addItems({"CarryForward", "DELIVERY", "INTRADAY"});
    }

    if (m_cbOptType)
    {
        m_cbOptType->addItems({"CE", "PE"});
    }

    if (m_cbMFAON)
    {
        m_cbMFAON->addItems({"None", "MF", "AON"});
    }

    if (m_cbProduct)
    {
        m_cbProduct->addItems({"INTRADAY", "DELIVERY", "CO", "BO"});
    }

    if (m_cbValidity)
    {
        m_cbValidity->addItems({"DAY", "IOC", "GTD"});
    }
    
    // New combo boxes from image
    if (m_cbOC)
    {
        m_cbOC->addItems({"OPEN", "CLOSE"});
    }
    
    if (m_cbProCli)
    {
        m_cbProCli->addItems({"PRO", "CLI"});
    }
    
    if (m_cbCPBroker)
    {
        m_cbCPBroker->addItems({"", "Broker1", "Broker2"});
    }
    
    if (m_cbExp)
    {
        m_cbExp->addItems({"23DEC2025", "30DEC2025", "06JAN2026"});
    }
    
    if (m_cbStrike)
    {
        m_cbStrike->addItems({"26200.0", "26250.0", "26300.0", "26350.0"});
    }
}

void BuyWindow::setupConnections()
{
    if (m_pbSubmit)
    {
        connect(m_pbSubmit, &QPushButton::clicked, this, &BuyWindow::onSubmitClicked);
    }

    if (m_pbClear)
    {
        connect(m_pbClear, &QPushButton::clicked, this, &BuyWindow::onClearClicked);
    }
    
    if (m_pbAMO)
    {
        connect(m_pbAMO, &QPushButton::clicked, this, &BuyWindow::onAMOClicked);
    }

    // Connect order type change to update field states
    if (m_cbOrdType)
    {
        connect(m_cbOrdType, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &BuyWindow::onOrderTypeChanged);
    }
}

void BuyWindow::setScripDetails(const QString &exchange, int token, const QString &symbol)
{
    if (m_cbEx)
    {
        int idx = m_cbEx->findText(exchange);
        if (idx >= 0)
            m_cbEx->setCurrentIndex(idx);
    }

    if (m_leToken)
    {
        m_leToken->setText(QString::number(token));
    }

    if (m_leSymbol)
    {
        m_leSymbol->setText(symbol);
    }

    qDebug() << "[BuyWindow] Set scrip details:" << exchange << token << symbol;
}

void BuyWindow::onSubmitClicked()
{
    if (!m_leQty || !m_leRate || !m_cbOrdType)
    {
        QMessageBox::warning(this, "Buy Order", "Missing required fields");
        return;
    }

    bool qtyOk, priceOk;
    int quantity = m_leQty->text().toInt(&qtyOk);
    double price = m_leRate->text().toDouble(&priceOk);

    if (!qtyOk || quantity <= 0)
    {
        QMessageBox::warning(this, "Buy Order", "Invalid quantity");
        return;
    }

    // Validate price
    if (!priceOk || price <= 0)
    {
        QMessageBox::warning(this, "Buy Order", "Invalid price");
        return;
    }

    // Validate quantity is multiple of lot size
    PreferencesManager &prefs = PreferencesManager::instance();
    int lotSize = (m_context.lotSize > 0) ? m_context.lotSize : prefs.getDefaultQuantity(m_context.exchange);
    if (lotSize <= 0) lotSize = 1;
    if ((quantity % lotSize) != 0) {
        QMessageBox::warning(this, "Buy Order", QString("Quantity must be a multiple of lot size (%1)").arg(lotSize));
        return;
    }

    QString exchange = m_cbEx ? m_cbEx->currentText() : "";
    int token = m_leToken ? m_leToken->text().toInt() : 0;
    QString symbol = m_leSymbol ? m_leSymbol->text() : "";
    QString orderType = m_cbOrdType ? m_cbOrdType->currentText() : "LIMIT";

    emit orderSubmitted(exchange, token, symbol, quantity, price, orderType);

    QMessageBox::information(this, "Buy Order",
                             QString("Buy order submitted:\n%1 @ %2 x %3")
                                 .arg(symbol)
                                 .arg(price)
                                 .arg(quantity));

    qDebug() << "[BuyWindow] Order submitted:" << symbol << quantity << "@" << price;
}

void BuyWindow::onClearClicked()
{
    // Clear input fields
    if (m_leQty)
        m_leQty->clear();
    if (m_leDiscloseQty)
        m_leDiscloseQty->clear();
    if (m_leRate)
        m_leRate->clear();
    if (m_leSetflor)
        m_leSetflor->clear();
    if (m_leTrigPrice)
        m_leTrigPrice->clear();
    if (m_leMFQty)
        m_leMFQty->clear();
    if (m_leRemarks)
        m_leRemarks->clear();

    // Reset comboboxes to first item
    if (m_cbOrdType)
        m_cbOrdType->setCurrentIndex(0);
    if (m_cbOptType)
        m_cbOptType->setCurrentIndex(0);
    if (m_cbMFAON)
        m_cbMFAON->setCurrentIndex(0);
    if (m_cbProduct)
        m_cbProduct->setCurrentIndex(0);
    if (m_cbValidity)
        m_cbValidity->setCurrentIndex(0);

    qDebug() << "[BuyWindow] Form cleared";
}

void BuyWindow::onAMOClicked()
{
    // AMO (After Market Order) functionality
    QMessageBox::information(this, "AMO", "After Market Order functionality will be implemented here.");
    qDebug() << "[BuyWindow] AMO button clicked";
}

void BuyWindow::onOrderTypeChanged(const QString &orderType)
{
    // Update field states based on order type according to the specification:
    // MARKET: Price=Disabled, Trigger=Disabled
    // LIMIT: Price=Enabled, Trigger=Disabled  
    // SL: Price=Enabled, Trigger=Enabled
    // SL-M: Price=Disabled, Trigger=Enabled
    
    bool priceEnabled = false;
    bool triggerEnabled = false;
    
    if (orderType == "MARKET") {
        priceEnabled = false;
        triggerEnabled = false;
    } else if (orderType == "LIMIT") {
        priceEnabled = true;
        triggerEnabled = false;
    } else if (orderType == "SL") {
        priceEnabled = true;
        triggerEnabled = true;
    } else if (orderType == "SL-M") {
        priceEnabled = false;
        triggerEnabled = true;
    }
    
    // Apply field states
    if (m_leRate) {
        m_leRate->setEnabled(priceEnabled);
        if (!priceEnabled) {
            m_leRate->clear(); // Clear disabled fields
        }
    }
    
    if (m_leTrigPrice) {
        m_leTrigPrice->setEnabled(triggerEnabled);
        if (!triggerEnabled) {
            m_leTrigPrice->clear(); // Clear disabled fields
        }
    }
    
    // Auto-recalculate price if enabled and context is available
    if (priceEnabled && m_context.isValid()) {
        calculateDefaultPrice(m_context);
    }
    
    qDebug() << "[BuyWindow] Order type changed to:" << orderType 
             << "Price enabled:" << priceEnabled 
             << "Trigger enabled:" << triggerEnabled;
}

bool BuyWindow::focusNextPrevChild(bool next)
{
    // Define the specific tab order: Total Qty -> Price (if enabled) -> Trig Price (if enabled) -> Submit
    QList<QWidget *> tabOrder;
    
    // Always include Total Qty as the starting field
    if (m_leQty && m_leQty->isEnabled()) {
        tabOrder.append(m_leQty);
    }
    
    // Add Price field if enabled (depends on order type)
    if (m_leRate && m_leRate->isEnabled()) {
        tabOrder.append(m_leRate);
    }
    
    // Add Trigger Price field if enabled (depends on order type)
    if (m_leTrigPrice && m_leTrigPrice->isEnabled()) {
        tabOrder.append(m_leTrigPrice);
    }
    
    // Always include Submit button as the final field
    if (m_pbSubmit && m_pbSubmit->isEnabled()) {
        tabOrder.append(m_pbSubmit);
    }

    if (tabOrder.isEmpty())
    {
        return false;
    }

    // Find the current focused widget
    QWidget *currentFocus = QApplication::focusWidget();
    int currentIndex = tabOrder.indexOf(currentFocus);

    // If no widget has focus or focused widget is not in our tab order, focus the first one (Total Qty)
    if (currentIndex == -1)
    {
        tabOrder.first()->setFocus();
        return true;
    }

    // Calculate next index with wrapping
    int nextIndex;
    if (next)
    {
        nextIndex = (currentIndex + 1) % tabOrder.size();
    }
    else
    {
        nextIndex = (currentIndex - 1 + tabOrder.size()) % tabOrder.size();
    }

    // Set focus to the next/previous widget
    tabOrder[nextIndex]->setFocus();

    qDebug() << "[BuyWindow] Tab navigation:" << (next ? "forward" : "backward")
             << "from index" << currentIndex << "to" << nextIndex
             << "Widget:" << tabOrder[nextIndex]->objectName();

    return true; // We handled the focus change
}

void BuyWindow::loadPreferences()
{
    PreferencesManager &prefs = PreferencesManager::instance();
    
    // Apply default order type
    if (m_cbOrdType) {
        QString orderType = prefs.getDefaultOrderType();
        int idx = m_cbOrdType->findText(orderType);
        if (idx >= 0) {
            m_cbOrdType->setCurrentIndex(idx);
        }
        // Trigger field state update for the selected order type
        onOrderTypeChanged(m_cbOrdType->currentText());
    }
    
    // Apply default product
    if (m_cbProduct) {
        QString product = prefs.getDefaultProduct();
        int idx = m_cbProduct->findText(product);
        if (idx >= 0) {
            m_cbProduct->setCurrentIndex(idx);
        }
    }
    
    // Apply default validity
    if (m_cbValidity) {
        QString validity = prefs.getDefaultValidity();
        int idx = m_cbValidity->findText(validity);
        if (idx >= 0) {
            m_cbValidity->setCurrentIndex(idx);
        }
    }
    
    qDebug() << "[BuyWindow] Loaded preferences: Order=" << prefs.getDefaultOrderType()
             << "Product=" << prefs.getDefaultProduct()
             << "Validity=" << prefs.getDefaultValidity();
}

void BuyWindow::loadFromContext(const WindowContext &context)
{
    if (!context.isValid()) {
        qWarning() << "[BuyWindow] Invalid context provided";
        return;
    }
    
    m_context = context;
    
    // Set exchange
    if (m_cbEx) {
        QString exchange = context.exchange.left(3);  // NSEFO -> NSE
        int idx = m_cbEx->findText(exchange, Qt::MatchStartsWith);
        if (idx >= 0) {
            m_cbEx->setCurrentIndex(idx);
        }
    }
    
    // Set token
    if (m_leToken) {
        m_leToken->setText(QString::number(context.token));
    }
    
    // Set symbol
    if (m_leSymbol) {
        m_leSymbol->setText(context.displayName.isEmpty() ? context.symbol : context.displayName);
    }
    
    // Set instrument type
    if (m_leInsType) {
        m_leInsType->setText(context.instrumentType);
    }
    
    // Set instrument name (segment-based)
    if (m_cbInstrName && !context.instrumentType.isEmpty()) {
        int idx = m_cbInstrName->findText(context.instrumentType, Qt::MatchStartsWith);
        if (idx >= 0) {
            m_cbInstrName->setCurrentIndex(idx);
        }
    }
    
    // Set option details if applicable
    if (!context.expiry.isEmpty() && m_cbExp) {
        m_cbExp->clear();
        m_cbExp->addItem(context.expiry);
        m_cbExp->setCurrentIndex(0);
    }
    
    if (context.strikePrice > 0.0 && m_cbStrike) {
        m_cbStrike->clear();
        m_cbStrike->addItem(QString::number(context.strikePrice, 'f', 2));
        m_cbStrike->setCurrentIndex(0);
    }
    
    if (!context.optionType.isEmpty() && context.optionType != "XX" && m_cbOptType) {
        int idx = m_cbOptType->findText(context.optionType);
        if (idx >= 0) {
            m_cbOptType->setCurrentIndex(idx);
        }
    }
    
    // Initialize quantity to lotSize (always)
    if (m_leQty) {
        PreferencesManager &prefs = PreferencesManager::instance();
        int qty = context.lotSize;
        if (qty <= 0) {
            qty = prefs.getDefaultQuantity(context.segment);
        }
        if (qty <= 0) {
            qty = 1;  // Absolute fallback
        }
        m_leQty->setText(QString::number(qty));
    }
    
    // Auto-calculate price
    PreferencesManager &prefs = PreferencesManager::instance();
    if (prefs.getAutoFillPrice()) {
        calculateDefaultPrice(context);
    }
    
    // Set focus to Total Qty field after loading context and select all text for immediate editing
    if (m_leQty) {
        m_leQty->setFocus();
        m_leQty->selectAll();
    }
    
    qDebug() << "[BuyWindow] Loaded from context:" << context.exchange << context.symbol 
             << "Token:" << context.token << "LTP:" << context.ltp;
}

void BuyWindow::calculateDefaultPrice(const WindowContext &context)
{
    if (!m_leRate) return;
    
    PreferencesManager &prefs = PreferencesManager::instance();
    QString orderType = m_cbOrdType ? m_cbOrdType->currentText() : "LIMIT";
    
    if (orderType == "MARKET" || orderType == "SL-M") {
        // Market orders and SL-M don't need price
        m_leRate->clear();
        m_leRate->setEnabled(false);
        return;
    }
    
    m_leRate->setEnabled(true);
    
    // For limit orders and SL orders, calculate price
    double price = 0.0;
    
    if (context.ask > 0.0) {
        // Use ask price (seller's price) for buying
        price = context.ask;
    } else if (context.ltp > 0.0) {
        // Fallback to LTP
        price = context.ltp;
    } else {
        // No price data available
        return;
    }
    
    if (prefs.getAutoCalculatePrice()) {
        double offset = prefs.getBuyPriceOffset();
        price += offset;
        
        // Round to tick size
        if (context.tickSize > 0.0) {
            price = std::round(price / context.tickSize) * context.tickSize;
        }
    }
    
    m_leRate->setText(QString::number(price, 'f', 2));
    qDebug() << "[BuyWindow] Calculated buy price:" << price 
             << "(Ask:" << context.ask << "Offset:" << prefs.getBuyPriceOffset() << ")";
}

void BuyWindow::keyPressEvent(QKeyEvent *event)
{
    if (!event) return;

    int key = event->key();

    // ESC -> close parent MDI subwindow (if any)
    if (key == Qt::Key_Escape) {
        CustomMDISubWindow *parentWin = qobject_cast<CustomMDISubWindow*>(parent());
        if (parentWin) parentWin->close();
        else close();
        event->accept();
        return;
    }

    // Enter -> submit order
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        onSubmitClicked();
        event->accept();
        return;
    }

    // F2 -> request SellWindow for same contract
    if (key == Qt::Key_F2) {
        if (m_context.isValid()) {
            emit requestSellWithContext(m_context);
        }
        // Close this window
        CustomMDISubWindow *parentWin = qobject_cast<CustomMDISubWindow*>(parent());
        if (parentWin) parentWin->close();
        else close();
        event->accept();
        return;
    }

    // Up / Down arrows: when focus on qty or price, adjust by lotSize/tickSize
    QWidget *fw = QApplication::focusWidget();
    if (key == Qt::Key_Up || key == Qt::Key_Down) {
        int direction = (key == Qt::Key_Up) ? 1 : -1;

        // Quantity adjustment
        if (fw == m_leQty) {
            PreferencesManager &prefs = PreferencesManager::instance();
            int lot = (m_context.lotSize > 0) ? m_context.lotSize : prefs.getDefaultQuantity(m_context.exchange);
            if (lot <= 0) lot = 1;
            bool ok; int cur = m_leQty->text().toInt(&ok);
            if (!ok) cur = lot;
            int next = cur + direction * lot;
            if (next < lot) next = lot;
            m_leQty->setText(QString::number(next));
            event->accept();
            return;
        }

        // Price adjustment
        if (fw == m_leRate) {
            double tick = (m_context.tickSize > 0.0) ? m_context.tickSize : 1.0;
            bool ok; double cur = m_leRate->text().toDouble(&ok);
            if (!ok) cur = (m_context.ltp > 0) ? m_context.ltp : 0.0;
            double next = cur + direction * tick;
            if (m_context.tickSize > 0.0) {
                next = std::round(next / m_context.tickSize) * m_context.tickSize;
            }
            m_leRate->setText(QString::number(next, 'f', 2));
            event->accept();
            return;
        }
    }

    // Default handling
    QWidget::keyPressEvent(event);
}

// Static singleton management methods
BuyWindow* BuyWindow::getInstance(QWidget *parent)
{
    qDebug() << "[BuyWindow::getInstance] Current instance:" << s_instance;
    
    if (s_instance) {
        // Bring existing window to front
        qDebug() << "[BuyWindow::getInstance] Reusing existing window";
        s_instance->raise();
        s_instance->activateWindow();
        // Set focus to Total Qty field and select all text for immediate editing
        if (s_instance->m_leQty) {
            s_instance->m_leQty->setFocus();
            s_instance->m_leQty->selectAll();
        }
        return s_instance;
    }
    
    // Close any existing SellWindow before creating new BuyWindow
    SellWindow::closeCurrentWindow();
    
    // Create new instance if none exists
    qDebug() << "[BuyWindow::getInstance] Creating new BuyWindow";
    return new BuyWindow(parent);
}

BuyWindow* BuyWindow::getInstance(const WindowContext &context, QWidget *parent)
{
    qDebug() << "[BuyWindow::getInstance] Current instance:" << s_instance << "Context:" << context.symbol;
    
    if (s_instance) {
        // Update existing window with new context
        qDebug() << "[BuyWindow::getInstance] Updating existing window with new context";
        s_instance->loadFromContext(context);
        s_instance->raise();
        s_instance->activateWindow();
        // Focus is already set in loadFromContext method
        return s_instance;
    }
    
    // Close any existing SellWindow before creating new BuyWindow
    SellWindow::closeCurrentWindow();
    
    // Create new instance if none exists
    qDebug() << "[BuyWindow::getInstance] Creating new BuyWindow with context";
    return new BuyWindow(context, parent);
}

void BuyWindow::closeCurrentWindow()
{
    qDebug() << "[BuyWindow::closeCurrentWindow] s_instance:" << s_instance;
    
    if (s_instance) {
        // Find the MDI container by looking for CustomMDISubWindow class
        QWidget *parent = s_instance->parentWidget();
        while (parent) {
            if (parent->metaObject()->className() == QString("CustomMDISubWindow")) {
                qDebug() << "[BuyWindow::closeCurrentWindow] Found MDI container:" << parent;
                parent->close();
                s_instance = nullptr;
                return;
            }
            parent = parent->parentWidget();
        }
        
        // Fallback: close the widget directly
        qDebug() << "[BuyWindow::closeCurrentWindow] Closing widget directly";
        s_instance->close();
        s_instance = nullptr;
    }
}

bool BuyWindow::hasActiveWindow()
{
    return s_instance != nullptr;
}

void BuyWindow::setInstance(BuyWindow* instance)
{
    s_instance = instance;
}

void BuyWindow::closeEvent(QCloseEvent *event) {
    WindowSettingsHelper::saveWindowSettings(this, "BuyWindow");
    QWidget::closeEvent(event);
}
