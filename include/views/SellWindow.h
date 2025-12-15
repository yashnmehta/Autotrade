#ifndef SELLWINDOW_H
#define SELLWINDOW_H

#include <QWidget>
#include <QUiLoader>
#include <QFile>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QDebug>

class SellWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SellWindow(QWidget *parent = nullptr);
    ~SellWindow();

    // Access to form widgets
    QComboBox* getExchangeCombo() const { return m_cbEx; }
    QLineEdit* getTokenEdit() const { return m_leToken; }
    QLineEdit* getSymbolEdit() const { return m_leSymbol; }
    QLineEdit* getQuantityEdit() const { return m_leQty; }
    QLineEdit* getPriceEdit() const { return m_leRate; }
    QPushButton* getSubmitButton() const { return m_pbSubmit; }

    // Set scrip details
    void setScripDetails(const QString &exchange, int token, const QString &symbol);

signals:
    void orderSubmitted(const QString &exchange, int token, const QString &symbol, 
                       int quantity, double price, const QString &orderType);

protected:
    // Override to keep focus within the window (prevent tab escape)
    bool focusNextPrevChild(bool next) override;

private slots:
    void onSubmitClicked();
    void onClearClicked();

private:
    void setupConnections();
    void populateComboBoxes();

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
};

#endif // SELLWINDOW_H
