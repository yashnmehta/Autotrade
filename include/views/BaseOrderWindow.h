#ifndef BASEORDERWINDOW_H
#define BASEORDERWINDOW_H

#include <QWidget>
#include <QUiLoader>
#include <QFile>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QKeyEvent>
#include <QCloseEvent>
#include "models/WindowContext.h"
#include "api/XTSTypes.h"

/**
 * @brief Base class for Buy and Sell order entry windows.
 * Supports both new order placement and order modification modes.
 */
class BaseOrderWindow : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Order entry mode
     */
    enum OrderMode {
        NewOrder,       ///< Placing a new order
        ModifyOrder,    ///< Modifying an existing order
        BatchModifyOrder ///< Modifying multiple orders in batch
    };

    explicit BaseOrderWindow(const QString& uiFile, QWidget* parent = nullptr);
    virtual ~BaseOrderWindow();

    void loadFromContext(const WindowContext &context);
    void loadFromOrder(const XTS::Order &order);  ///< Load from existing order for modification
    void loadFromOrders(const QVector<XTS::Order> &orders); ///< Load multiple orders for batch modification
    
    void setScripDetails(const QString &exchange, int token, const QString &symbol);
    
    bool isModifyMode() const { return m_orderMode == ModifyOrder; }
    bool isBatchModifyMode() const { return m_orderMode == BatchModifyOrder; }
    OrderMode orderMode() const { return m_orderMode; }
    int64_t originalOrderID() const { return m_originalOrderID; }

signals:
    void orderSubmitted(const XTS::OrderParams &params);
    void orderModificationSubmitted(const XTS::ModifyOrderParams &params);

protected slots:
    virtual void onSubmitClicked() = 0;
    virtual void onClearClicked();
    virtual void onOrderTypeChanged(const QString &orderType);
    virtual void onAMOClicked();

protected:
    void setupBaseUI(const QString& uiFile);
    void findBaseWidgets();
    void setupBaseConnections();
    void populateBaseComboBoxes();
    void loadBasePreferences();
    virtual void calculateDefaultPrice(const WindowContext &context) = 0;
    void setModifyMode(bool enabled);  ///< Configure UI for modify mode (disable immutable fields)
    void resetToNewOrderMode();        ///< Reset window to new order mode

    bool eventFilter(QObject *obj, QEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

    WindowContext m_context;
    QWidget *m_formWidget;
    
    // Order mode tracking
    OrderMode m_orderMode = NewOrder;
    int64_t m_originalOrderID = 0;          ///< AppOrderID of order being modified
    XTS::Order m_originalOrder;              ///< Full order data for modification
    QVector<XTS::Order> m_batchOrders;       ///< List of orders for batch modification

    // Shared widgets
    QComboBox *m_cbEx;
    QComboBox *m_cbInstrName;
    QComboBox *m_cbOrdType;
    QLineEdit *m_leToken;
    QLineEdit *m_leInsType;
    QLineEdit *m_leSymbol;
    QComboBox *m_cbExp;
    QComboBox *m_cbStrike;
    QComboBox *m_cbOptType;
    QLineEdit *m_leQty;
    QLineEdit *m_leDiscloseQty;
    QLineEdit *m_leRate;
    QLineEdit *m_leSetflor;
    QLineEdit *m_leTrigPrice;
    QComboBox *m_cbMFAON;
    QLineEdit *m_leMFQty;
    QComboBox *m_cbProduct;
    QComboBox *m_cbValidity;
    QLineEdit *m_leRemarks;
    QPushButton *m_pbSubmit;
    QPushButton *m_pbClear;
    QPushButton *m_pbAMO;

    // Redesign widgets
    QLineEdit *m_leCol;
    QLineEdit *m_leProdPercent;
    QComboBox *m_cbOC;
    QComboBox *m_cbProCli;
    QComboBox *m_cbCPBroker;
    QLineEdit *m_leMEQty;
    QLineEdit *m_leSubBroker;
    QLineEdit *m_leClient;
    QComboBox *m_cbOrderType2;
};

#endif // BASEORDERWINDOW_H

