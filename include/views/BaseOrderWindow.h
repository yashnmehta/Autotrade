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
 */
class BaseOrderWindow : public QWidget {
    Q_OBJECT
public:
    explicit BaseOrderWindow(const QString& uiFile, QWidget* parent = nullptr);
    virtual ~BaseOrderWindow();

    void loadFromContext(const WindowContext &context);
    void setScripDetails(const QString &exchange, int token, const QString &symbol);

signals:
    void orderSubmitted(const XTS::OrderParams &params);

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

    bool eventFilter(QObject *obj, QEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

    WindowContext m_context;
    QWidget *m_formWidget;

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
