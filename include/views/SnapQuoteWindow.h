#ifndef SNAPQUOTEWINDOW_H
#define SNAPQUOTEWINDOW_H

#include <QWidget>
#include <QUiLoader>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QDebug>
#include <QShortcut>
#include "models/WindowContext.h"
#include "api/XTSTypes.h"
#include "udp/UDPTypes.h"
#include "app/ScripBar.h"

class XTSMarketDataClient;

class SnapQuoteWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SnapQuoteWindow(QWidget *parent = nullptr);
    explicit SnapQuoteWindow(const WindowContext &context, QWidget *parent = nullptr);
    ~SnapQuoteWindow();

    // Set scrip details
    void setScripDetails(const QString &exchange, const QString &segment, 
                        int token, const QString &instType, const QString &symbol);
    
    // Context-aware loading
    void loadFromContext(const WindowContext &context, bool fetchFromAPI = true);
    // Return last loaded context
    WindowContext getContext() const { return m_context; }
    
    // Set XTS client for market data
    void setXTSClient(XTSMarketDataClient *client);
    
    // Fetch current quote from API
    void fetchQuote();
    
    // Update quote data
    void updateQuote(double ltpPrice, int ltpQty, const QString &ltpTime,
                    double open, double high, double low, double close,
                    qint64 volume, double value, double atp, double percentChange);
    
    // Update additional statistics
    void updateStatistics(const QString &dpr, qint64 oi, double oiPercent,
                         double gainLoss, double mtmValue, double mtmPos);
    
    // Update bid depth (level: 1-5)
    void updateBidDepth(int level, int qty, double price, int orders = 0);
    
    // Update ask depth (level: 1-5)
    void updateAskDepth(int level, double price, int qty, int orders);
    
    // Set LTP indicator (up/down)
    void setLTPIndicator(bool isUp);
    
    // Set color based on change
    void setChangeColor(double change);

public slots:
    void onRefreshClicked();
    void onTickUpdate(const UDP::MarketTick& tick);

private slots:
    void onScripSelected(const InstrumentData &data);

signals:
    void refreshRequested(const QString &exchange, int token);
    void closed();

private:
    void populateComboBoxes();
    void setupConnections();
    void setupKeyboardShortcuts();
    
    // ⚡ Load data from GStore (common function - < 1ms!)
    bool loadFromGStore();

private:
    // UI widgets loaded from .ui file
    QWidget *m_formWidget;
    
    // Header widgets
    ScripBar *m_scripBar;
    QPushButton *m_pbRefresh;
    
    // LTP Section
    QLabel *m_lbLTPQty;
    QLabel *m_lbLTPPrice;
    QLabel *m_lbLTPIndicator;
    QLabel *m_lbLTPTime;
    
    // Market Statistics (Left Panel)
    QLabel *m_lbVolume;
    QLabel *m_lbValue;
    QLabel *m_lbATP;
    QLabel *m_lbPercentChange;
    
    // Price Data (Right Panel)
    QLabel *m_lbOpen;
    QLabel *m_lbHigh;
    QLabel *m_lbLow;
    QLabel *m_lbClose;
    
    // Additional Statistics
    QLabel *m_lbDPR;
    QLabel *m_lbOI;
    QLabel *m_lbOIPercent;
    QLabel *m_lbGainLoss;
    QLabel *m_lbMTMValue;
    QLabel *m_lbMTMPos;
    
    // Bid Depth (5 levels)
    QLabel *m_lbBidQty1, *m_lbBidQty2, *m_lbBidQty3, *m_lbBidQty4, *m_lbBidQty5;
    QLabel *m_lbBidPrice1, *m_lbBidPrice2, *m_lbBidPrice3, *m_lbBidPrice4, *m_lbBidPrice5;
    QLabel *m_lbBidOrders1, *m_lbBidOrders2, *m_lbBidOrders3, *m_lbBidOrders4, *m_lbBidOrders5;
    
    // Ask Depth (5 levels)
    QLabel *m_lbAskPrice1, *m_lbAskPrice2, *m_lbAskPrice3, *m_lbAskPrice4, *m_lbAskPrice5;
    QLabel *m_lbAskQty1, *m_lbAskQty2, *m_lbAskQty3, *m_lbAskQty4, *m_lbAskQty5;
    QLabel *m_lbAskOrders1, *m_lbAskOrders2, *m_lbAskOrders3, *m_lbAskOrders4, *m_lbAskOrders5;

    // Data members
    QString m_exchange;
    QString m_segment;
    QString m_symbol;
    int m_token;
    
    // Context data
    WindowContext m_context;
    
    // Market data client
    XTSMarketDataClient *m_xtsClient;
    QShortcut* m_escShortcut;
    QShortcut* m_refreshShortcut;

    // Total counts
    QLabel *m_lbTotalBuyers;
    QLabel *m_lbTotalSellers;

    void initUI();
};

#endif // SNAPQUOTEWINDOW_H
