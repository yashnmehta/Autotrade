#include "views/BaseOrderWindow.h"
#include "utils/PreferencesManager.h"
#include "utils/WindowSettingsHelper.h"
#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <cmath>

BaseOrderWindow::BaseOrderWindow(const QString& uiFile, QWidget* parent)
    : QWidget(parent), m_formWidget(nullptr) {
    setupBaseUI(uiFile);
}

BaseOrderWindow::~BaseOrderWindow() {}

void BaseOrderWindow::setupBaseUI(const QString& uiFile) {
    QUiLoader loader;
    QFile file(uiFile);

    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "[BaseOrderWindow] Failed to open UI file:" << uiFile;
        return;
    }

    m_formWidget = loader.load(&file, this);
    file.close();

    if (!m_formWidget) return;

    setMinimumWidth(1200);
    setMinimumHeight(200);
    setFocusPolicy(Qt::StrongFocus);
    m_formWidget->setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_formWidget);

    findBaseWidgets();
    populateBaseComboBoxes();
    setupBaseConnections();
    loadBasePreferences();
}

void BaseOrderWindow::findBaseWidgets() {
    m_cbEx = m_formWidget->findChild<QComboBox *>("cbEx");
    m_cbInstrName = m_formWidget->findChild<QComboBox *>("cbInstrName");
    m_cbOrdType = m_formWidget->findChild<QComboBox *>("cbOrdType");
    m_leToken = m_formWidget->findChild<QLineEdit *>("leToken");
    m_leInsType = m_formWidget->findChild<QLineEdit *>("leInsType");
    m_leSymbol = m_formWidget->findChild<QLineEdit *>("leSymbol");
    m_cbExp = m_formWidget->findChild<QComboBox *>("cbExp");
    m_cbStrike = m_formWidget->findChild<QComboBox *>("cbStrike");
    m_cbOptType = m_formWidget->findChild<QComboBox *>("cbOptType");
    m_leQty = m_formWidget->findChild<QLineEdit *>("leQty");
    m_leDiscloseQty = m_formWidget->findChild<QLineEdit *>("leDiscloseQty");
    m_leRate = m_formWidget->findChild<QLineEdit *>("leRate");
    m_leSetflor = m_formWidget->findChild<QLineEdit *>("leSetflor");
    m_leTrigPrice = m_formWidget->findChild<QLineEdit *>("leTrigPrice");
    m_cbMFAON = m_formWidget->findChild<QComboBox *>("cbMFAON");
    m_leMFQty = m_formWidget->findChild<QLineEdit *>("leMFQty");
    m_cbProduct = m_formWidget->findChild<QComboBox *>("cbProduct");
    m_cbValidity = m_formWidget->findChild<QComboBox *>("cbValidity");
    m_leRemarks = m_formWidget->findChild<QLineEdit *>("leRemarks");
    m_pbSubmit = m_formWidget->findChild<QPushButton *>("pbSubmit");
    m_pbClear = m_formWidget->findChild<QPushButton *>("pbClear");
    m_pbAMO = m_formWidget->findChild<QPushButton *>("pbAMO");

    // Redesign widgets
    m_leCol = m_formWidget->findChild<QLineEdit *>("leCol");
    m_leProdPercent = m_formWidget->findChild<QLineEdit *>("leProdPercent");
    m_cbOC = m_formWidget->findChild<QComboBox *>("cbOC");
    m_cbProCli = m_formWidget->findChild<QComboBox *>("cbProCli");
    m_cbCPBroker = m_formWidget->findChild<QComboBox *>("cbCPBroker");
    m_leMEQty = m_formWidget->findChild<QLineEdit *>("leMEQty");
    m_leSubBroker = m_formWidget->findChild<QLineEdit *>("leSubBroker");
    m_leClient = m_formWidget->findChild<QLineEdit *>("leClient");
    m_cbOrderType2 = m_formWidget->findChild<QComboBox *>("cbOrderType2");
}

void BaseOrderWindow::setupBaseConnections() {
    if (m_pbSubmit) connect(m_pbSubmit, &QPushButton::clicked, this, &BaseOrderWindow::onSubmitClicked);
    if (m_pbClear) connect(m_pbClear, &QPushButton::clicked, this, &BaseOrderWindow::onClearClicked);
    if (m_pbAMO) connect(m_pbAMO, &QPushButton::clicked, this, &BaseOrderWindow::onAMOClicked);
    if (m_cbOrdType) {
        connect(m_cbOrdType, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &BaseOrderWindow::onOrderTypeChanged);
    }
    if (m_leQty) m_leQty->installEventFilter(this);
    if (m_leRate) m_leRate->installEventFilter(this);
}

void BaseOrderWindow::populateBaseComboBoxes() {
    if (m_cbEx) m_cbEx->addItems({"NSECM", "NSEFO", "NSECD", "BSECM", "BSEFO", "BSECD", "MCXFO"});
    if (m_cbInstrName) m_cbInstrName->addItems({"FUTIDX", "FUTSTK", "OPTIDX", "OPTSTK"});
    if (m_cbOrdType) m_cbOrdType->addItems({"Limit", "Market", "StopLimit", "StopMarket"});
    if (m_cbOrderType2) m_cbOrderType2->addItems({"CarryForward", "DELIVERY", "INTRADAY"});
    if (m_cbOptType) m_cbOptType->addItems({"CE", "PE"});
    if (m_cbMFAON) m_cbMFAON->addItems({"None", "MF", "AON"});
    if (m_cbProduct) m_cbProduct->addItems({"MIS", "NRML", "CNC", "CO", "BO"}); // API compatible codes
    if (m_cbValidity) m_cbValidity->addItems({"DAY", "IOC", "GTD"});
    if (m_cbOC) m_cbOC->addItems({"OPEN", "CLOSE"});
    if (m_cbProCli) m_cbProCli->addItems({"PRO", "CLI"});
    if (m_cbCPBroker) m_cbCPBroker->addItems({"", "Broker1", "Broker2"});
}

void BaseOrderWindow::loadBasePreferences() {
    PreferencesManager &prefs = PreferencesManager::instance();
    if (m_cbOrdType) {
        int idx = m_cbOrdType->findText(prefs.getDefaultOrderType());
        if (idx >= 0) m_cbOrdType->setCurrentIndex(idx);
        onOrderTypeChanged(m_cbOrdType->currentText());
    }
    if (m_cbProduct) {
        QString defProd = prefs.getDefaultProduct();
        // Map old preferences if they exist
        if (defProd == "INTRADAY") defProd = "MIS";
        else if (defProd == "DELIVERY") defProd = "NRML"; 
        
        int idx = m_cbProduct->findText(defProd);
        if (idx < 0) idx = m_cbProduct->findText("MIS"); 
        if (idx >= 0) m_cbProduct->setCurrentIndex(idx);
    }
    if (m_cbValidity) {
        int idx = m_cbValidity->findText(prefs.getDefaultValidity());
        if (idx >= 0) m_cbValidity->setCurrentIndex(idx);
    }
}

void BaseOrderWindow::onClearClicked() {
    QList<QLineEdit*> edits = findChildren<QLineEdit*>();
    for (auto e : edits) e->clear();
    if (m_cbOrdType) m_cbOrdType->setCurrentIndex(0);
}

void BaseOrderWindow::onOrderTypeChanged(const QString &orderType) {
    bool priceEnabled = (orderType == "Limit" || orderType == "StopLimit");
    bool triggerEnabled = (orderType == "StopLimit" || orderType == "StopMarket");
    if (m_leRate) {
        m_leRate->setEnabled(priceEnabled);
        if (!priceEnabled) m_leRate->clear();
    }
    if (m_leTrigPrice) {
        m_leTrigPrice->setEnabled(triggerEnabled);
        if (!triggerEnabled) m_leTrigPrice->clear();
    }
    if (priceEnabled && m_context.isValid()) calculateDefaultPrice(m_context);
}

void BaseOrderWindow::onAMOClicked() {
    qDebug() << "[BaseOrderWindow] AMO clicked";
}

void BaseOrderWindow::setScripDetails(const QString &exchange, int token, const QString &symbol) {
    if (m_cbEx) {
        // Try to find an item in m_cbEx that is a prefix of 'exchange'
        // e.g. "NSE" matches "NSEFO"
        int foundIdx = -1;
        for (int i = 0; i < m_cbEx->count(); ++i) {
            if (exchange.startsWith(m_cbEx->itemText(i), Qt::CaseInsensitive)) {
                foundIdx = i;
                break;
            }
        }
        if (foundIdx >= 0) {
            m_cbEx->setCurrentIndex(foundIdx);
        } else {
            // Fallback to exact match or starts with
            int idx = m_cbEx->findText(exchange, Qt::MatchStartsWith);
            if (idx >= 0) m_cbEx->setCurrentIndex(idx);
        }
    }
    if (m_leToken) m_leToken->setText(QString::number(token));
    if (m_leSymbol) m_leSymbol->setText(symbol);
}

void BaseOrderWindow::loadFromContext(const WindowContext &context) {
    if (!context.isValid()) return;
    m_context = context;
    setScripDetails(context.exchange, context.token, context.symbol);
    if (m_leInsType) m_leInsType->setText(context.instrumentType);
    if (m_leQty) {
        int qty = context.lotSize > 0 ? context.lotSize : 1;
        m_leQty->setText(QString::number(qty));
        m_leQty->setFocus();
        m_leQty->selectAll();
    }
    if (m_cbExp) {
        m_cbExp->clear();
        if (!context.expiry.isEmpty()) {
            m_cbExp->addItem(context.expiry);
            m_cbExp->setCurrentIndex(0);
        }
    }
    if (m_cbStrike) {
        m_cbStrike->clear();
        if (context.strikePrice > 0) {
            m_cbStrike->addItem(QString::number(context.strikePrice, 'f', 2));
            m_cbStrike->setCurrentIndex(0);
        }
    }
    if (m_cbOptType && !context.optionType.isEmpty()) {
        int idx = m_cbOptType->findText(context.optionType);
        if (idx >= 0) m_cbOptType->setCurrentIndex(idx);
    }
    calculateDefaultPrice(context);
}

bool BaseOrderWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_leQty && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
            bool ok;
            int currentQty = m_leQty->text().toInt(&ok);
            if (!ok) currentQty = 0;

            int lotSize = m_context.lotSize > 0 ? m_context.lotSize : 1;

            if (keyEvent->key() == Qt::Key_Up) {
                currentQty += lotSize;
            } else {
                currentQty -= lotSize;
                if (currentQty < lotSize) currentQty = lotSize;
            }

            m_leQty->setText(QString::number(currentQty));
            return true;
        }
    } else if (obj == m_leRate && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
            bool ok;
            double currentPrice = m_leRate->text().toDouble(&ok);
            if (!ok) currentPrice = 0.0;

            double tickSize = m_context.tickSize > 0 ? m_context.tickSize : 0.05;

            if (keyEvent->key() == Qt::Key_Up) {
                currentPrice += tickSize;
            } else {
                currentPrice -= tickSize;
                if (currentPrice < tickSize) currentPrice = tickSize;
            }
            
            // Snap to tick size
            if (tickSize > 0) currentPrice = std::round(currentPrice / tickSize) * tickSize;

            m_leRate->setText(QString::number(currentPrice, 'f', 2));
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

bool BaseOrderWindow::focusNextPrevChild(bool next) {
    // Define a fast-path sequence for trading
    QList<QWidget*> widgets;
    
    // 1. Order Type
    if (m_cbOrdType && m_cbOrdType->isEnabled()) widgets << m_cbOrdType;
    
    // 2. Product
    if (m_cbProduct && m_cbProduct->isEnabled()) widgets << m_cbProduct;
    
    // 3. Quantity
    if (m_leQty && m_leQty->isEnabled()) widgets << m_leQty;
    
    // 3. Price (if enabled)
    if (m_leRate && m_leRate->isEnabled()) widgets << m_leRate;
    
    // 4. Trigger Price (if enabled)
    if (m_leTrigPrice && m_leTrigPrice->isEnabled()) widgets << m_leTrigPrice;
    
    // 5. Submit Button
    if (m_pbSubmit && m_pbSubmit->isEnabled()) widgets << m_pbSubmit;

    if (widgets.isEmpty()) return QWidget::focusNextPrevChild(next);
    
    QWidget *curr = QApplication::focusWidget();
    int idx = widgets.indexOf(curr);
    
    // If current focused widget is not in our fast-path, let default logic handle it
    if (idx == -1) {
        // Exception: if starting tab from somewhere else, maybe jump into the fast path?
        // For now, let default logic find the next widget based on UI tab order
        return QWidget::focusNextPrevChild(next);
    }
    
    int nextIdx = next ? (idx + 1) % widgets.size() : (idx - 1 + widgets.size()) % widgets.size();
    widgets[nextIdx]->setFocus();
    
    // Select all text if it's a line edit for easier replacement
    QLineEdit *le = qobject_cast<QLineEdit*>(widgets[nextIdx]);
    if (le) le->selectAll();
    
    return true;
}

void BaseOrderWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) close();
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) onSubmitClicked();
    else QWidget::keyPressEvent(event);
}

void BaseOrderWindow::closeEvent(QCloseEvent *event) {
    QWidget::closeEvent(event);
}
