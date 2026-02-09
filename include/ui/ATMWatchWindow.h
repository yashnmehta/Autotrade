#ifndef ATM_WATCH_WINDOW_H
#define ATM_WATCH_WINDOW_H

#include "services/ATMWatchManager.h"
#include "udp/UDPTypes.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>


/**
 * @brief Professional ATM Watch Window
 * Displays ATM Call, Underlying Symbol, and ATM Put in three synchronized
 * tables.
 */
class ATMWatchWindow : public QWidget {
  Q_OBJECT

public:
  explicit ATMWatchWindow(QWidget *parent = nullptr);
  ~ATMWatchWindow();

private slots:
  void onATMUpdated();
  void synchronizeScrollBars(int value);
  void onTickUpdate(const UDP::MarketTick &tick);
  void onExchangeChanged(int index);
  void onExpiryChanged(int index);
  void onBasePriceUpdate();                  // Timer for LTP updates
  void onSymbolsLoaded(int count);           // Background load completion
  void onSettingsClicked();                  // Open settings dialog
  void onShowContextMenu(const QPoint &pos); // Right-click context menu
  void
  onSymbolDoubleClicked(const QModelIndex &index); // Double-click to open chain

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void setupUI();
  void setupModels();
  void setupConnections();
  void refreshData();
  void updateDataIncrementally(); // P2: Incremental updates (no flicker)
  void loadAllSymbols();          // Now runs in background thread
  void populateCommonExpiries(const QString &exchange);
  QString getNearestExpiry(const QString &symbol, const QString &exchange);
  void updateBasePrices(); // Update all base prices from price stores
  void openOptionChain(const QString &symbol,
                       const QString &expiry); // Open option chain window

  // UI Components - Toolbar
  QToolBar *m_toolbar;

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
  QStandardItemModel *m_callModel;
  QStandardItemModel *m_symbolModel;
  QStandardItemModel *m_putModel;

  // Logic storage
  QMap<QString, int> m_symbolToRow;
  QMap<int64_t, std::pair<QString, bool>>
      m_tokenToInfo; // token -> {symbol, isCall}
  QMap<uint32_t, int> m_underlyingToRow;
  QMap<QString, int64_t>
      m_symbolToUnderlyingToken; // symbol -> underlying token (cash/future)
  QMap<int64_t, QString> m_underlyingTokenToSymbol; // underlying token ->
                                                    // symbol (for live updates)
  QMap<QString, ATMWatchManager::ATMInfo>
      m_previousATMData; // P2: Track previous state for incremental updates

  // Timer for LTP updates
  QTimer *m_basePriceTimer;

  enum CallCols {
    CALL_LTP = 0,
    CALL_CHG,
    CALL_BID,
    CALL_ASK,
    CALL_VOL,
    CALL_OI,
    CALL_IV,
    CALL_DELTA,
    CALL_GAMMA,
    CALL_VEGA,
    CALL_THETA,
    CALL_COUNT
  };
  enum SymbolCols { SYM_NAME = 0, SYM_PRICE, SYM_ATM, SYM_EXPIRY, SYM_COUNT };
  enum PutCols {
    PUT_BID = 0,
    PUT_ASK,
    PUT_LTP,
    PUT_CHG,
    PUT_VOL,
    PUT_OI,
    PUT_IV,
    PUT_DELTA,
    PUT_GAMMA,
    PUT_VEGA,
    PUT_THETA,
    PUT_COUNT
  };
};

#endif // ATM_WATCH_WINDOW_H
