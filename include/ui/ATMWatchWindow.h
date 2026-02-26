#ifndef ATM_WATCH_WINDOW_H
#define ATM_WATCH_WINDOW_H

#include "models/WindowContext.h"
#include "services/ATMWatchManager.h"
#include "udp/UDPTypes.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

/**
 * @brief Custom delegate for color-coding cells based on value changes
 */
class ATMWatchDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit ATMWatchDelegate(bool isMiddle = false, QObject *parent = nullptr)
      : QStyledItemDelegate(parent), m_isMiddle(isMiddle) {}

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    painter->save();

    // Check for dynamic tick update color
    int direction = index.data(Qt::UserRole + 1).toInt();
    QColor bgColor = Qt::transparent;

    if (!m_isMiddle) {
      if (direction == 1) {
        bgColor = QColor("#0000FF"); // Blue for Up
      } else if (direction == 2) {
        bgColor = QColor("#FF0000"); // Red for Down
      }
    }

    if (bgColor != Qt::transparent) {
      painter->fillRect(option.rect, bgColor);
    } else if (option.state & QStyle::State_Selected) {
      painter->fillRect(option.rect, QColor("#3A5A70"));
    } else if (m_isMiddle) {
      painter->fillRect(option.rect,
                        QColor("#222222")); // Slightly greyer for middle
    }

    // Draw text
    QString text = index.data(Qt::DisplayRole).toString();
    QColor textColor = Qt::white;

    // Change color logic (for Chng column)
    QString headerText =
        index.model()->headerData(index.column(), Qt::Horizontal).toString();
    if (headerText == "Chg") {
      bool ok;
      double value = text.toDouble(&ok);
      if (ok && value != 0.0) {
        textColor = (value > 0) ? QColor("#00FF00") : QColor("#FF4444");
      }
    }

    // Specific highlight for LTP, Bid, Ask
    if (headerText == "LTP" || headerText == "Bid" || headerText == "Ask" ||
        headerText == "Spot/Fut") {
      // Already handled by bgColor if direction is set
    } else {
      // For other columns, we might NOT want the blue/red background if it's
      // too noisy But the user asked for background changes for values.
    }

    painter->setPen(textColor);
    painter->drawText(option.rect, Qt::AlignCenter, text);

    painter->restore();
  }

private:
  bool m_isMiddle;
};

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

  // Public Context Access for Global Shortcuts
  WindowContext getCurrentContext();

signals:
  void openOptionChainRequested(const QString &symbol, const QString &expiry);

private slots:
  void onATMUpdated();
  void onTickUpdate(const UDP::MarketTick &tick);
  void onExchangeChanged(int index);
  void onExpiryChanged(int index);
  void onBasePriceUpdate();                  // Timer for LTP updates
  void onSymbolsLoaded(int count);           // Background load completion
  void onSettingsClicked();                  // Open settings dialog
  void onShowContextMenu(const QPoint &pos); // Right-click context menu
  void
  onSymbolDoubleClicked(const QModelIndex &index); // Double-click to open chain
  void onHeaderClicked(int logicalIndex);          // Sort by column
  void onCallHeaderClicked(int logicalIndex);       // Sort by call table column
  void onPutHeaderClicked(int logicalIndex);        // Sort by put table column

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void setupUI();
  void setupModels();
  void setupConnections();
  void setupShortcuts();
  void refreshData();
  void updateDataIncrementally(); // P2: Incremental updates (no flicker)
  void loadAllSymbols();          // Now runs in background thread
  void populateCommonExpiries(const QString &exchange);
  QString getNearestExpiry(const QString &symbol, const QString &exchange);
  void updateBasePrices(); // Update all base prices from price stores
  void openOptionChain(const QString &symbol,
                       const QString &expiry); // Open option chain window
  void sortATMList(QVector<ATMWatchManager::ATMInfo> &list); // Sort helper
  void updateItemWithColor(QStandardItemModel *model, int row, int col,
                           double newValue,
                           int precision = 2); // Color helper

  // Sort State
  enum SortSource { SORT_SYMBOL_TABLE, SORT_CALL_TABLE, SORT_PUT_TABLE };
  SortSource m_sortSource = SORT_SYMBOL_TABLE;
  int m_sortColumn = 0; // Default: Symbol
  Qt::SortOrder m_sortOrder = Qt::AscendingOrder;

  // Context & Key Handling
  void keyPressEvent(QKeyEvent *event) override;

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

  // Delegates
  ATMWatchDelegate *m_callDelegate;
  ATMWatchDelegate *m_putDelegate;
  ATMWatchDelegate *m_symbolDelegate;

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

  bool m_syncingScroll = false; // Re-entrancy guard for tri-directional scroll sync

  // Timer for LTP updates
  QTimer *m_basePriceTimer;

  enum CallCols {
    CALL_CHG = 0,
    CALL_VOL,
    CALL_OI,
    CALL_IV,
    CALL_DELTA,
    CALL_GAMMA,
    CALL_VEGA,
    CALL_THETA,
    CALL_LTP,
    CALL_BID,
    CALL_ASK,
    CALL_COUNT
  };
  enum SymbolCols { SYM_NAME = 0, SYM_PRICE, SYM_ATM, SYM_EXPIRY, SYM_COUNT };
  enum PutCols {
    PUT_LTP = 0,
    PUT_BID,
    PUT_ASK,
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
