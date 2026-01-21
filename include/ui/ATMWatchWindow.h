#ifndef ATM_WATCH_WINDOW_H
#define ATM_WATCH_WINDOW_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QMap>
#include <QComboBox>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include "services/ATMWatchManager.h"
#include "udp/UDPTypes.h"
#include "models/WindowContext.h"

/**
 * @brief Professional ATM Watch Window
 * Displays ATM Call, Underlying Symbol, and ATM Put in three synchronized tables.
 */
class ATMWatchWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ATMWatchWindow(QWidget *parent = nullptr);
    WindowContext getSelectedContext() const;
    ~ATMWatchWindow();

private slots:
    void onATMUpdated();
    void synchronizeScrollBars(int value);
    void onTickUpdate(const UDP::MarketTick &tick);
    void onExchangeChanged(int index);
    void onExpiryChanged(int index);
    void onBasePriceUpdate();  // Timer for LTP updates
    void onSymbolsLoaded(int count);  // Background load completion

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

    void setupUI();
    void setupModels();
    void setupConnections();
    void refreshData();
    void loadAllSymbols();  // Now runs in background thread
    void populateCommonExpiries(const QString& exchange);
    QString getNearestExpiry(const QString& symbol, const QString& exchange);
    void updateBasePrices();  // Update all base prices from price stores
    
    // UI Components - Controls
    QComboBox *m_exchangeCombo;
    QComboBox *m_expiryCombo;
    QLabel *m_statusLabel;
    
    // Current filter state
    QString m_currentExchange;
    QString m_currentExpiry;
    
    // UI Components - Tables
    QTableView *m_callTable;
    QTableView *m_symbolTable;
    QTableView *m_putTable;
    
    // Models
    // Unified Model and Proxy for all 3 tables
    QStandardItemModel *m_mainModel;
    QSortFilterProxyModel *m_mainProxy;
    
    // Logic storage
    QMap<QString, int> m_symbolToRow;
    QMap<int64_t, std::pair<QString, bool>> m_tokenToInfo; // token -> {symbol, isCall}
    QMap<int64_t, QString> m_underlyingToSymbol;         // token -> symbol
    QMap<uint32_t, int> m_underlyingToRow;
    QMap<QString, int64_t> m_symbolToUnderlyingToken;  // symbol -> underlying token (cash/future)
    
    // Timer for LTP updates
    QTimer* m_basePriceTimer;

    enum CallCols { CALL_LTP = 0, CALL_CHG, CALL_BID, CALL_ASK, CALL_VOL, CALL_OI, CALL_COUNT };
    enum SymbolCols { SYM_NAME = 0, SYM_PRICE, SYM_ATM, SYM_EXPIRY, SYM_COUNT };
    enum PutCols { PUT_BID = 0, PUT_ASK, PUT_LTP, PUT_CHG, PUT_VOL, PUT_OI, PUT_COUNT };
};

#endif // ATM_WATCH_WINDOW_H
