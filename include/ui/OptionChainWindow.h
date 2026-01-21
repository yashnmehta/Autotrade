#ifndef OPTIONCHAINWINDOW_H
#define OPTIONCHAINWINDOW_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QMap>
#include "api/XTSTypes.h"
#include "udp/UDPTypes.h"
#include "repository/ContractData.h"
#include "models/WindowContext.h"

/**
 * @brief Data structure for a single strike in the option chain
 */
struct OptionStrikeData {
    double strikePrice;
    
    // Call option data
    int callOI;
    int callChngInOI;
    int callVolume;
    double callIV;
    double callLTP;
    double callChng;
    int callBidQty;
    double callBid;
    double callAsk;
    int callAskQty;
    
    // Call Greeks
    double callDelta;
    double callGamma;
    double callVega;
    double callTheta;
    
    // Put option data
    int putOI;
    int putChngInOI;
    int putVolume;
    double putIV;
    double putLTP;
    double putChng;
    int putBidQty;
    double putBid;
    double putAsk;
    int putAskQty;
    
    // Put Greeks
    double putDelta;
    double putGamma;
    double putVega;
    double putTheta;
    // Token IDs for subscription
    int callToken = 0;
    int putToken = 0;
    
    OptionStrikeData() 
        : strikePrice(0.0), callOI(0), callChngInOI(0), callVolume(0), 
          callIV(0.0), callLTP(0.0), callChng(0.0), callBidQty(0), 
          callBid(0.0), callAsk(0.0), callAskQty(0),
          callDelta(0.0), callGamma(0.0), callVega(0.0), callTheta(0.0),
          putOI(0), putChngInOI(0), putVolume(0), 
          putIV(0.0), putLTP(0.0), putChng(0.0), putBidQty(0),
          putBid(0.0), putAsk(0.0), putAskQty(0),
          putDelta(0.0), putGamma(0.0), putVega(0.0), putTheta(0.0),
          callToken(0), putToken(0) {}
};

/**
 * @brief Custom delegate for color-coding cells based on value changes
 */
class OptionChainDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit OptionChainDelegate(QObject *parent = nullptr);
    
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

/**
 * @brief Option Chain Window with synchronized Call and Put tables
 * 
 * Features:
 * - Three synchronized table views (Calls, Strike, Puts)
 * - Color-coded cells (green for positive, red for negative changes)
 * - ATM strike highlighting
 * - Symbol and expiry selection
 * - Real-time data updates
 * - Trade button integration
 */
class OptionChainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit OptionChainWindow(QWidget *parent = nullptr);
    ~OptionChainWindow();
    
    // Data management
    void setSymbol(const QString &symbol, const QString &expiry);
    void updateStrikeData(double strike, const OptionStrikeData &data);
    void clearData();
    
    // Configuration
    void setStrikeRange(double minStrike, double maxStrike, double interval);
    void setATMStrike(double atmStrike);
    
    // Data retrieval
    QString currentSymbol() const { return m_currentSymbol; }
    QString currentExpiry() const { return m_currentExpiry; }
    WindowContext getSelectedContext() const;
    
signals:
    void tradeRequested(const QString &symbol, const QString &expiry, 
                       double strike, const QString &optionType);
    void calculatorRequested(const QString &symbol, const QString &expiry,
                            double strike, const QString &optionType);
    void refreshRequested();

private slots:
    void onSymbolChanged(const QString &symbol);
    void onExpiryChanged(const QString &expiry);
    void onRefreshClicked();
    void onTradeClicked();
    void onCalculatorClicked();
    void onCallTableClicked(const QModelIndex &index);
    void onPutTableClicked(const QModelIndex &index);
    void onStrikeTableClicked(const QModelIndex &index);
    void synchronizeScrollBars(int value);
    void onTickUpdate(const UDP::MarketTick &tick);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUI();
    void setupModels();
    void setupConnections();
    void refreshData();
    void updateTableColors();
    void highlightATMStrike();
    
    // UI Population Helpers
    void populateSymbols();
    void populateExpiries(const QString &symbol);
    
    QModelIndex getStrikeIndex(int row) const;
    double getStrikeAtRow(int row) const;
    
    // UI Components - Header
    QComboBox *m_symbolCombo;
    QComboBox *m_expiryCombo;
    QPushButton *m_refreshButton;
    QPushButton *m_calculatorButton;
    QLabel *m_titleLabel;
    
    // Table views
    QTableView *m_callTable;
    QTableView *m_strikeTable;
    QTableView *m_putTable;
    
    // Models
    QStandardItemModel *m_callModel;
    QStandardItemModel *m_strikeModel;
    QStandardItemModel *m_putModel;
    
    // Delegates
    OptionChainDelegate *m_callDelegate;
    OptionChainDelegate *m_putDelegate;
    
    // Data storage
    QMap<double, OptionStrikeData> m_strikeData;
    QList<double> m_strikes;
    
    // Quick lookup for updates
    QMap<int, double> m_tokenToStrike;
    
    // Current state
    QString m_currentSymbol;
    QString m_currentExpiry;
    double m_atmStrike;
    int m_selectedCallRow;
    int m_selectedPutRow;
    
    // Column indices for Call table
    enum CallColumns {
        CALL_CHECKBOX = 0,
        CALL_OI,
        CALL_CHNG_IN_OI,
        CALL_VOLUME,
        CALL_IV,
        CALL_DELTA,
        CALL_GAMMA,
        CALL_VEGA,
        CALL_THETA,
        CALL_LTP,
        CALL_CHNG,
        CALL_BID_QTY,
        CALL_BID,
        CALL_ASK,
        CALL_ASK_QTY,
        CALL_COLUMN_COUNT
    };
    
    // Column indices for Put table
    enum PutColumns {
        PUT_BID_QTY = 0,
        PUT_BID,
        PUT_ASK,
        PUT_ASK_QTY,
        PUT_CHNG,
        PUT_LTP,
        PUT_IV,
        PUT_DELTA,
        PUT_GAMMA,
        PUT_VEGA,
        PUT_THETA,
        PUT_VOLUME,
        PUT_CHNG_IN_OI,
        PUT_OI,
        PUT_CHECKBOX,
        PUT_COLUMN_COUNT
    };
};

#endif // OPTIONCHAINWINDOW_H
