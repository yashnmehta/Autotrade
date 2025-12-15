#include "views/BuyWindow.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QApplication>

BuyWindow::BuyWindow(QWidget *parent)
    : QWidget(parent), m_formWidget(nullptr)
{
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
    setMinimumWidth(1000);

    // Set focus policy to keep tab cycling within this window
    setFocusPolicy(Qt::StrongFocus);
    m_formWidget->setFocusPolicy(Qt::StrongFocus);

    // Set layout
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_formWidget);

    // Find widgets - Row 1
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

    // Find widgets - Row 2
    m_leSetflor = m_formWidget->findChild<QLineEdit *>("leSetflor");
    m_leTrigPrice = m_formWidget->findChild<QLineEdit *>("leTrigPrice");
    m_cbMFAON = m_formWidget->findChild<QComboBox *>("cbMFAON");
    m_leMFQty = m_formWidget->findChild<QLineEdit *>("leMFQty");
    m_cbProduct = m_formWidget->findChild<QComboBox *>("cbProduct");
    m_cbValidity = m_formWidget->findChild<QComboBox *>("cbValidity");
    m_leRemarks = m_formWidget->findChild<QLineEdit *>("leRemarks");
    m_pbSubmit = m_formWidget->findChild<QPushButton *>("pbSubmit");
    m_pbClear = m_formWidget->findChild<QPushButton *>("pbClear");

    populateComboBoxes();
    setupConnections();

    qDebug() << "[BuyWindow] Created successfully";
}

BuyWindow::~BuyWindow()
{
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

    if (!priceOk || price <= 0)
    {
        QMessageBox::warning(this, "Buy Order", "Invalid price");
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

bool BuyWindow::focusNextPrevChild(bool next)
{
    // Get all focusable widgets in this window's tab order
    QList<QWidget *> focusableWidgets;

    // Manually build the list based on our tab order
    if (m_cbEx && m_cbEx->isEnabled())
        focusableWidgets.append(m_cbEx);
    if (m_cbInstrName && m_cbInstrName->isEnabled())
        focusableWidgets.append(m_cbInstrName);
    if (m_cbOrdType && m_cbOrdType->isEnabled())
        focusableWidgets.append(m_cbOrdType);
    if (m_leToken && m_leToken->isEnabled())
        focusableWidgets.append(m_leToken);
    if (m_leInsType && m_leInsType->isEnabled())
        focusableWidgets.append(m_leInsType);
    if (m_leSymbol && m_leSymbol->isEnabled())
        focusableWidgets.append(m_leSymbol);
    if (m_cbExp && m_cbExp->isEnabled())
        focusableWidgets.append(m_cbExp);
    if (m_cbStrike && m_cbStrike->isEnabled())
        focusableWidgets.append(m_cbStrike);
    if (m_cbOptType && m_cbOptType->isEnabled())
        focusableWidgets.append(m_cbOptType);
    if (m_leQty && m_leQty->isEnabled())
        focusableWidgets.append(m_leQty);
    if (m_leDiscloseQty && m_leDiscloseQty->isEnabled())
        focusableWidgets.append(m_leDiscloseQty);
    if (m_leRate && m_leRate->isEnabled())
        focusableWidgets.append(m_leRate);
    if (m_leSetflor && m_leSetflor->isEnabled())
        focusableWidgets.append(m_leSetflor);
    if (m_leTrigPrice && m_leTrigPrice->isEnabled())
        focusableWidgets.append(m_leTrigPrice);
    if (m_cbMFAON && m_cbMFAON->isEnabled())
        focusableWidgets.append(m_cbMFAON);
    if (m_leMFQty && m_leMFQty->isEnabled())
        focusableWidgets.append(m_leMFQty);
    if (m_cbProduct && m_cbProduct->isEnabled())
        focusableWidgets.append(m_cbProduct);
    if (m_cbValidity && m_cbValidity->isEnabled())
        focusableWidgets.append(m_cbValidity);
    if (m_leRemarks && m_leRemarks->isEnabled())
        focusableWidgets.append(m_leRemarks);
    if (m_pbSubmit && m_pbSubmit->isEnabled())
        focusableWidgets.append(m_pbSubmit);
    if (m_pbClear && m_pbClear->isEnabled())
        focusableWidgets.append(m_pbClear);

    if (focusableWidgets.isEmpty())
    {
        return false;
    }

    // Find the current focused widget
    QWidget *currentFocus = QApplication::focusWidget();
    int currentIndex = focusableWidgets.indexOf(currentFocus);

    // If no widget has focus or focused widget is not in our list, focus the first one
    if (currentIndex == -1)
    {
        focusableWidgets.first()->setFocus();
        return true;
    }

    // Calculate next index with wrapping
    int nextIndex;
    if (next)
    {
        nextIndex = (currentIndex + 1) % focusableWidgets.size();
    }
    else
    {
        nextIndex = (currentIndex - 1 + focusableWidgets.size()) % focusableWidgets.size();
    }

    // Set focus to the next/previous widget
    focusableWidgets[nextIndex]->setFocus();

    qDebug() << "[BuyWindow] Tab navigation:" << (next ? "forward" : "backward")
             << "from index" << currentIndex << "to" << nextIndex;

    return true; // We handled the focus change
}
