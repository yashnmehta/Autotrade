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
    // setMinimumHeight(140);
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
    cacheLineEdits();  // Cache line edits for fast clearing
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
    if (m_leQty) {
        m_leQty->installEventFilter(this);
        qDebug() << "[BaseOrderWindow] Installed event filter on Quantity field:" << m_leQty;
    }
    if (m_leRate) {
        m_leRate->installEventFilter(this);
        qDebug() << "[BaseOrderWindow] Installed event filter on Price field:" << m_leRate;
    }
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

void BaseOrderWindow::cacheLineEdits() {
    // Cache all line edits once for fast clearing
    m_cachedLineEdits.clear();
    if (m_formWidget) {
        m_cachedLineEdits = m_formWidget->findChildren<QLineEdit*>();
    }
    qDebug() << "[BaseOrderWindow] Cached" << m_cachedLineEdits.size() << "line edits for fast clearing";
}

void BaseOrderWindow::applyDefaultFocus() {
    PreferencesManager &prefs = PreferencesManager::instance();
    PreferencesManager::FocusField focusField = prefs.getOrderWindowFocusField();
    
    QWidget* targetWidget = nullptr;
    
    switch (focusField) {
        case PreferencesManager::FocusField::Price:
            if (m_leRate && m_leRate->isEnabled()) {
                targetWidget = m_leRate;
            } else if (m_leQty) {
                targetWidget = m_leQty;  // Fallback if price is disabled
            }
            break;
        case PreferencesManager::FocusField::Scrip:
            if (m_leSymbol) {
                targetWidget = m_leSymbol;
            } else if (m_leQty) {
                targetWidget = m_leQty;  // Fallback
            }
            break;
        case PreferencesManager::FocusField::Quantity:
        default:
            if (m_leQty) {
                targetWidget = m_leQty;
            }
            break;
    }
    
    if (targetWidget) {
        targetWidget->setFocus();
        // Position cursor at end and deselect for immediate editing
        QLineEdit* le = qobject_cast<QLineEdit*>(targetWidget);
        if (le) {
            // Position cursor at the end of the text for immediate typing
            le->deselect();
            le->setCursorPosition(le->text().length());
            qDebug() << "[BaseOrderWindow] Applied focus to:" << targetWidget->objectName() << "Value:" << le->text() << "Cursor at end, ready for editing";
        }
    } else {
        qDebug() << "[BaseOrderWindow] WARNING: No target widget found for focus!";
    }
}

void BaseOrderWindow::onClearClicked() {
    // Use cached line edits for fast clearing (avoid expensive findChildren call)
    for (auto e : m_cachedLineEdits) {
        e->clear();
    }
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
    }
    
    // NOTE: Focus is applied asynchronously by WindowCacheManager after window is shown
    // Applying it here causes black screen rendering issues
    
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
    
    // Set Product Type from context if available (e.g. from Net Position)
    if (m_cbProduct && !context.productType.isEmpty()) {
        int idx = m_cbProduct->findText(context.productType);
        if (idx >= 0) m_cbProduct->setCurrentIndex(idx);
    }

    calculateDefaultPrice(context);
}

void BaseOrderWindow::loadFromOrder(const XTS::Order &order) {
    // Store original order for modification
    m_originalOrder = order;
    m_originalOrderID = order.appOrderID;
    m_orderMode = ModifyOrder;
    
    // Populate UI fields from the order
    if (m_cbEx) {
        int idx = m_cbEx->findText(order.exchangeSegment, Qt::MatchContains);
        if (idx >= 0) m_cbEx->setCurrentIndex(idx);
    }
    if (m_leToken) m_leToken->setText(QString::number(order.exchangeInstrumentID));
    if (m_leSymbol) m_leSymbol->setText(order.tradingSymbol);
    
    // Order type
    if (m_cbOrdType) {
        int idx = m_cbOrdType->findText(order.orderType);
        if (idx >= 0) m_cbOrdType->setCurrentIndex(idx);
    }
    
    // Product type
    if (m_cbProduct) {
        int idx = m_cbProduct->findText(order.productType);
        if (idx >= 0) m_cbProduct->setCurrentIndex(idx);
    }
    
    // Quantity - show leaves quantity (remaining)
    if (m_leQty) {
        m_leQty->setText(QString::number(order.orderQuantity));
    }
    
    // Set focus based on user preference
    applyDefaultFocus();
    
    // Disclosed quantity
    if (m_leDiscloseQty) {
        m_leDiscloseQty->setText(QString::number(order.orderDisclosedQuantity));
    }
    
    // Price
    if (m_leRate) {
        m_leRate->setText(QString::number(order.orderPrice, 'f', 2));
    }
    
    // Trigger price (for stop orders)
    if (m_leTrigPrice) {
        if (order.orderStopPrice > 0) {
            m_leTrigPrice->setText(QString::number(order.orderStopPrice, 'f', 2));
        }
    }
    
    // Validity
    if (m_cbValidity) {
        int idx = m_cbValidity->findText(order.timeInForce);
        if (idx >= 0) m_cbValidity->setCurrentIndex(idx);
    }
    
    // Configure UI for modification mode
    setModifyMode(true);
    
    qDebug() << "[BaseOrderWindow] Loaded order for modification. AppOrderID:" << order.appOrderID 
             << "Symbol:" << order.tradingSymbol << "Qty:" << order.orderQuantity 
             << "Price:" << order.orderPrice;
}

void BaseOrderWindow::loadFromOrders(const QVector<XTS::Order> &orders) {
    if (orders.isEmpty()) return;
    
    // Validate that all orders are compatible for batch modification
    const XTS::Order &first = orders.first();
    bool compatible = true;
    for (int i = 1; i < orders.size(); ++i) {
        if (orders[i].exchangeSegment != first.exchangeSegment ||
            orders[i].exchangeInstrumentID != first.exchangeInstrumentID ||
            orders[i].productType != first.productType ||
            orders[i].timeInForce != first.timeInForce ||
            orders[i].orderSide != first.orderSide) {
            compatible = false;
            break;
        }
    }
    
    if (!compatible) {
        qWarning() << "[BaseOrderWindow] Incompatible orders for batch modification";
        return;
    }

    m_batchOrders = orders;
    loadFromOrder(first); // Reuse single order load for UI population
    
    // Switch to Batch Mode
    m_orderMode = BatchModifyOrder;
    m_originalOrderID = 0; // Not applicable for batch
    
    // Update title
    QString type = (first.orderSide.toUpper() == "BUY") ? "Buy" : "Sell";
    setWindowTitle(QString("Batch Modify %1 Order (%2 orders)").arg(type).arg(orders.size()));
    
    qDebug() << "[BaseOrderWindow] Loaded batch of" << orders.size() << "orders for modification.";
}

void BaseOrderWindow::setModifyMode(bool enabled) {
    // Per NSE protocol, these fields CANNOT be modified:
    // - Buy/Sell (handled by using correct window)
    // - Symbol/Series/Contract
    // - Participant
    // - Pro/Cli (CM only)
    // - Product type (cannot be changed after order is placed)
    
    // Disable immutable fields
    if (m_cbEx) m_cbEx->setEnabled(!enabled);
    if (m_leToken) m_leToken->setEnabled(!enabled);
    if (m_leSymbol) m_leSymbol->setEnabled(!enabled);
    if (m_leInsType) m_leInsType->setEnabled(!enabled);
    if (m_cbInstrName) m_cbInstrName->setEnabled(!enabled);
    if (m_cbExp) m_cbExp->setEnabled(!enabled);
    if (m_cbStrike) m_cbStrike->setEnabled(!enabled);
    if (m_cbOptType) m_cbOptType->setEnabled(!enabled);
    if (m_cbProduct) m_cbProduct->setEnabled(!enabled);  // Product type cannot be modified
    if (m_cbProCli) m_cbProCli->setEnabled(!enabled);
    if (m_cbOC) m_cbOC->setEnabled(!enabled);
    
    // Editable fields remain enabled:
    // - Quantity (m_leQty) - Disabled in Batch Mode to prevent accidental mass change
    // - Disclosed Quantity (m_leDiscloseQty) - Disabled in Batch Mode
    // - Price (m_leRate)
    // - Trigger Price (m_leTrigPrice)
    // - Validity (m_cbValidity)
    // - Order Type (m_cbOrdType) - can toggle between RL/ST/SL
    // - Remarks (m_leRemarks)
    
    bool isBatch = (m_orderMode == BatchModifyOrder);
    if (m_leQty) m_leQty->setEnabled(!isBatch); 
    if (m_leDiscloseQty) m_leDiscloseQty->setEnabled(!isBatch);
    
    // Update submit button text
    if (m_pbSubmit) {
        if (isBatch) m_pbSubmit->setText(QString("Modify %1 Orders").arg(m_batchOrders.size()));
        else m_pbSubmit->setText(enabled ? "Modify Order" : "Submit Order");
    }
    
    // Update window title indicator
    if (enabled && !isBatch) {
        setWindowTitle(windowTitle().replace("Buy", "Modify Buy").replace("Sell", "Modify Sell"));
    }
}

void BaseOrderWindow::resetToNewOrderMode(bool fastMode) {
    m_orderMode = NewOrder;
    m_originalOrderID = 0;
    m_originalOrder = XTS::Order();
    m_batchOrders.clear();
    
    if (fastMode) {
        // Fast path for cached hidden windows - skip expensive UI updates
        // Just clear data, UI will be updated when window is shown
        for (auto e : m_cachedLineEdits) {
            e->clear();
        }
        if (m_cbOrdType) m_cbOrdType->setCurrentIndex(0);
        return;
    }
    
    // Full reset path for visible windows
    // Re-enable all fields
    setModifyMode(false);
    
    // Clear the form
    onClearClicked();
    
    // Reset window title
    QString title = windowTitle();
    title.replace("Modify Buy", "Buy").replace("Modify Sell", "Sell");
    setWindowTitle(title);
    
    if (m_pbSubmit) {
        m_pbSubmit->setText("Submit Order");
    }
}

bool BaseOrderWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        
        // Handle Quantity field arrow keys
        if (obj == m_leQty) {
            if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
                qDebug() << "[BaseOrderWindow] Arrow key pressed in Quantity field:" << (keyEvent->key() == Qt::Key_Up ? "UP" : "DOWN");
                
                bool ok;
                int currentQty = m_leQty->text().toInt(&ok);
                if (!ok) currentQty = 0;

                int lotSize = m_context.lotSize > 0 ? m_context.lotSize : 1;
                qDebug() << "  Current Qty:" << currentQty << "Lot Size:" << lotSize;

                if (keyEvent->key() == Qt::Key_Up) {
                    currentQty += lotSize;
                } else {
                    currentQty -= lotSize;
                    if (currentQty < lotSize) currentQty = lotSize;
                }

                m_leQty->setText(QString::number(currentQty));
                qDebug() << "  New Qty:" << currentQty;
                return true;  // Event handled, don't pass to default handler
            }
            // Let all other keys pass through to QLineEdit for normal typing
            return QWidget::eventFilter(obj, event);
        }
        
        // Handle Price field arrow keys
        else if (obj == m_leRate) {
            if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
                qDebug() << "[BaseOrderWindow] Arrow key pressed in Price field:" << (keyEvent->key() == Qt::Key_Up ? "UP" : "DOWN");
                
                bool ok;
                double currentPrice = m_leRate->text().toDouble(&ok);
                if (!ok) currentPrice = 0.0;

                double tickSize = m_context.tickSize > 0 ? m_context.tickSize : 0.05;
                qDebug() << "  Current Price:" << currentPrice << "Tick Size:" << tickSize;

                if (keyEvent->key() == Qt::Key_Up) {
                    currentPrice += tickSize;
                } else {
                    currentPrice -= tickSize;
                    if (currentPrice < tickSize) currentPrice = tickSize;
                }
                
                // Snap to tick size
                if (tickSize > 0) currentPrice = std::round(currentPrice / tickSize) * tickSize;

                m_leRate->setText(QString::number(currentPrice, 'f', 2));
                qDebug() << "  New Price:" << currentPrice;
                return true;  // Event handled, don't pass to default handler
            }
            // Let all other keys pass through to QLineEdit for normal typing
            return QWidget::eventFilter(obj, event);
        }
    }
    return QWidget::eventFilter(obj, event);
}

bool BaseOrderWindow::focusNextPrevChild(bool next) {
    QWidget *curr = QApplication::focusWidget();
    
    if (next) {
        // Forward navigation (Tab): Extended path
        // Total Qty → Price → Pro/CLI → Product → Validity → User Remarks → Submit
        QList<QWidget*> forwardWidgets;
        
        if (m_leQty && m_leQty->isEnabled()) forwardWidgets << m_leQty;
        if (m_leRate && m_leRate->isEnabled()) forwardWidgets << m_leRate;
        if (m_cbProCli && m_cbProCli->isEnabled()) forwardWidgets << m_cbProCli;
        if (m_cbProduct && m_cbProduct->isEnabled()) forwardWidgets << m_cbProduct;
        if (m_cbValidity && m_cbValidity->isEnabled()) forwardWidgets << m_cbValidity;
        if (m_leRemarks && m_leRemarks->isEnabled()) forwardWidgets << m_leRemarks;
        if (m_pbSubmit && m_pbSubmit->isEnabled()) forwardWidgets << m_pbSubmit;
        
        if (!forwardWidgets.isEmpty()) {
            int idx = forwardWidgets.indexOf(curr);
            
            qDebug() << "[BaseOrderWindow] Tab pressed. Current widget:" 
                     << (curr ? curr->objectName() : "NULL") 
                     << "Found at index:" << idx 
                     << "Total widgets:" << forwardWidgets.size();
            
            if (idx != -1) {
                // Found current widget - move to next in sequence
                int nextIdx = (idx + 1) % forwardWidgets.size();
                QWidget* nextWidget = forwardWidgets[nextIdx];
                nextWidget->setFocus();
                
                qDebug() << "[BaseOrderWindow] Moving to widget:" << nextWidget->objectName();
                
                QLineEdit *le = qobject_cast<QLineEdit*>(nextWidget);
                if (le) {
                    le->deselect();
                    le->setCursorPosition(le->text().length());
                }
                
                return true;
            } else if (curr == nullptr || !this->isAncestorOf(curr)) {
                // No focus or focus outside this window - set to first widget
                qDebug() << "[BaseOrderWindow] Focus outside window, setting to first widget:" 
                         << forwardWidgets[0]->objectName();
                forwardWidgets[0]->setFocus();
                QLineEdit *le = qobject_cast<QLineEdit*>(forwardWidgets[0]);
                if (le) {
                    le->deselect();
                    le->setCursorPosition(le->text().length());
                }
                return true;
            }
        }
    } else {
        // Backward navigation (Shift+Tab): Complete path
        // Submit → User Remarks → Validity → Order Type → Pro/CLI → Price → Total Qty → 
        // Opt Type → Strike Price → Expiry Date → Inst Type → Token → Type → Exchange
        QList<QWidget*> backwardWidgets;
        
        // Second row fields (in reverse order for backward navigation)
        if (m_pbSubmit && m_pbSubmit->isEnabled()) backwardWidgets << m_pbSubmit;
        if (m_leRemarks && m_leRemarks->isEnabled()) backwardWidgets << m_leRemarks;
        if (m_cbValidity && m_cbValidity->isEnabled()) backwardWidgets << m_cbValidity;
        if (m_cbProduct && m_cbProduct->isEnabled()) backwardWidgets << m_cbProduct;
        if (m_cbProCli && m_cbProCli->isEnabled()) backwardWidgets << m_cbProCli;
        
        // First row fields (right to left)
        if (m_leRate && m_leRate->isEnabled()) backwardWidgets << m_leRate;
        if (m_leQty && m_leQty->isEnabled()) backwardWidgets << m_leQty;
        if (m_cbOptType && m_cbOptType->isEnabled()) backwardWidgets << m_cbOptType;
        if (m_cbStrike && m_cbStrike->isEnabled()) backwardWidgets << m_cbStrike;
        if (m_cbExp && m_cbExp->isEnabled()) backwardWidgets << m_cbExp;
        if (m_cbInstrName && m_cbInstrName->isEnabled()) backwardWidgets << m_cbInstrName;
        if (m_leToken && m_leToken->isEnabled()) backwardWidgets << m_leToken;
        if (m_cbOrdType && m_cbOrdType->isEnabled()) backwardWidgets << m_cbOrdType;
        if (m_cbEx && m_cbEx->isEnabled()) backwardWidgets << m_cbEx;
        
        if (!backwardWidgets.isEmpty()) {
            int idx = backwardWidgets.indexOf(curr);
            if (idx != -1) {
                if (idx < backwardWidgets.size() - 1) {
                    // Move to next widget in backward sequence
                    int nextIdx = idx + 1;
                    backwardWidgets[nextIdx]->setFocus();
                    
                    QLineEdit *le = qobject_cast<QLineEdit*>(backwardWidgets[nextIdx]);
                    if (le) {
                        le->deselect();
                        le->setCursorPosition(le->text().length());
                    }
                    
                    return true;
                }
                // If at last widget (Exchange), let Qt handle further backward navigation
            }
        }
    }
    
    // Use default Qt behavior for widgets not in our custom paths
    return QWidget::focusNextPrevChild(next);
}

void BaseOrderWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        // Try to close parent MDI window first
        QWidget *p = parentWidget();
        while (p) {
            if (p->inherits("CustomMDISubWindow")) {
                p->close();
                return;
            }
            p = p->parentWidget();
        }
        close();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // Trigger submit when Enter is pressed
        qDebug() << "[BaseOrderWindow] Enter key pressed - triggering submit";
        onSubmitClicked();
    }
    else QWidget::keyPressEvent(event);
}

void BaseOrderWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    
    // Apply focus immediately when window is shown
    // This ensures the field is ready for immediate editing with visible cursor
    applyDefaultFocus();
}

void BaseOrderWindow::closeEvent(QCloseEvent *event) {
    QWidget::closeEvent(event);
}
