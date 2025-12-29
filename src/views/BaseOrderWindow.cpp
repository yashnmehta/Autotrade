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
}

void BaseOrderWindow::populateBaseComboBoxes() {
    if (m_cbEx) m_cbEx->addItems({"NSE", "BSE", "MCX"});
    if (m_cbInstrName) m_cbInstrName->addItems({"RL", "FUTIDX", "FUTSTK", "OPTIDX", "OPTSTK"});
    if (m_cbOrdType) m_cbOrdType->addItems({"LIMIT", "MARKET", "SL", "SL-M"});
    if (m_cbOrderType2) m_cbOrderType2->addItems({"CarryForward", "DELIVERY", "INTRADAY"});
    if (m_cbOptType) m_cbOptType->addItems({"CE", "PE"});
    if (m_cbMFAON) m_cbMFAON->addItems({"None", "MF", "AON"});
    if (m_cbProduct) m_cbProduct->addItems({"INTRADAY", "DELIVERY", "CO", "BO"});
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
        int idx = m_cbProduct->findText(prefs.getDefaultProduct());
        if (idx < 0) idx = m_cbProduct->findText("INTRADAY"); // Simple mapping
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
    bool priceEnabled = (orderType == "LIMIT" || orderType == "SL");
    bool triggerEnabled = (orderType == "SL" || orderType == "SL-M");
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
        int idx = m_cbEx->findText(exchange, Qt::MatchStartsWith);
        if (idx >= 0) m_cbEx->setCurrentIndex(idx);
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
    calculateDefaultPrice(context);
}

bool BaseOrderWindow::focusNextPrevChild(bool next) {
    QList<QWidget*> widgets;
    if (m_leQty && m_leQty->isEnabled()) widgets << m_leQty;
    if (m_leRate && m_leRate->isEnabled()) widgets << m_leRate;
    if (m_leTrigPrice && m_leTrigPrice->isEnabled()) widgets << m_leTrigPrice;
    if (m_pbSubmit && m_pbSubmit->isEnabled()) widgets << m_pbSubmit;

    if (widgets.isEmpty()) return false;
    QWidget *curr = QApplication::focusWidget();
    int idx = widgets.indexOf(curr);
    if (idx == -1) { widgets.first()->setFocus(); return true; }
    int nextIdx = next ? (idx + 1) % widgets.size() : (idx - 1 + widgets.size()) % widgets.size();
    widgets[nextIdx]->setFocus();
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
