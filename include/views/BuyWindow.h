#ifndef BUYWINDOW_H
#define BUYWINDOW_H

#include <QWidget>
#include <QUiLoader>
#include <QFile>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QDebug>
#include "models/WindowContext.h"
#include <QKeyEvent>

class BuyWindow : public QWidget
{
    Q_OBJECT

public:
    explicit BuyWindow(QWidget *parent = nullptr);
    explicit BuyWindow(const WindowContext &context, QWidget *parent = nullptr);
    ~BuyWindow();

    // Access to form widgets
    QComboBox* getExchangeCombo() const { return m_cbEx; }
    QLineEdit* getTokenEdit() const { return m_leToken; }
    QLineEdit* getSymbolEdit() const { return m_leSymbol; }
    QLineEdit* getQuantityEdit() const { return m_leQty; }
    QLineEdit* getPriceEdit() const { return m_leRate; }
    QPushButton* getSubmitButton() const { return m_pbSubmit; }

    // Set scrip details
    void setScripDetails(const QString &exchange, int token, const QString &symbol);
    
    // Load from context (called from constructor or externally)
    void loadFromContext(const WindowContext &context);
    
    // Static singleton management
    static BuyWindow* getInstance(QWidget *parent = nullptr);
    static BuyWindow* getInstance(const WindowContext &context, QWidget *parent = nullptr);
    static void closeCurrentWindow();
    static bool hasActiveWindow();

signals:
    void orderSubmitted(const QString &exchange, int token, const QString &symbol, 
                       int quantity, double price, const QString &orderType);
    // Request MainWindow to open SellWindow for same contract and close this one
    void requestSellWithContext(const WindowContext &context);

protected:
    // Override to keep focus within the window (prevent tab escape)
    bool focusNextPrevChild(bool next) override;
    // Handle key events: Esc (close), Enter (submit), F2 (switch to Sell), Up/Down (qty/price)
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onSubmitClicked();
    void onClearClicked();

private:
    void setupConnections();
    void populateComboBoxes();
    void loadPreferences();
    void calculateDefaultPrice(const WindowContext &context);
    void onOrderTypeChanged(const QString &orderType);
    void onAMOClicked();
    static void setInstance(BuyWindow* instance);
    
    WindowContext m_context;
    static BuyWindow* s_instance;

    // UI widgets loaded from .ui file
    QWidget *m_formWidget;
    
    // Row 1 widgets
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
    
    // Row 2 widgets
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
    
    // New UI widgets from redesign
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

#endif // BUYWINDOW_H
