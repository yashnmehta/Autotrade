#ifndef ATM_WATCH_WINDOW_H
#define ATM_WATCH_WINDOW_H

#include "models/domain/WindowContext.h"
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

    // Light-theme defaults
    QColor bgColor = Qt::transparent;
    QColor textColor("#1e293b"); // Primary text
    Qt::Alignment alignment = Qt::AlignCenter;

    QString headerText =
        index.model()->headerData(index.column(), Qt::Horizontal).toString();

    // Middle table (Symbol/Spot/ATM/Expiry): distinct surface bg, NO tick colors
    if (m_isMiddle) {
      bgColor = QColor("#f0f4ff"); // Slightly blue-tinted surface
      textColor = QColor("#1e293b");
      // Left-align Symbol column
      if (headerText == "Symbol") {
        alignment = Qt::AlignVCenter | Qt::AlignLeft;
      }
      // No tick-direction coloring for middle panel
    } else {
      // Call/Put tables: tick direction coloring
      int direction = index.data(Qt::UserRole + 1).toInt();
      if (direction == 1) {
        bgColor = QColor("#dbeafe"); // Light blue for Up tick
        textColor = QColor("#1d4ed8");
      } else if (direction == 2) {
        bgColor = QColor("#fee2e2"); // Light red for Down tick
        textColor = QColor("#dc2626");
      }

      // IV column highlight
      if (headerText == "IV") {
        if (bgColor == Qt::transparent)
          bgColor = QColor("#fef9c3"); // Warm yellow tint
        textColor = QColor("#92400e"); // Amber-brown text
        QFont f = option.font;
        f.setBold(true);
        painter->setFont(f);
      }
    }

    // Selection overrides
    if (option.state & QStyle::State_Selected) {
      bgColor = QColor("#dbeafe");
      textColor = QColor("#1e40af");
    }

    // Draw background
    painter->fillRect(option.rect, bgColor);

    // Draw text
    QString text = index.data(Qt::DisplayRole).toString();

    // Change color logic (for Chg column)
    if (headerText == "Chg") {
      bool ok;
      double value = text.toDouble(&ok);
      if (ok && value != 0.0) {
        textColor = (value > 0) ? QColor("#16a34a") : QColor("#dc2626");
      }
    }

    painter->setPen(textColor);
    QRect textRect = option.rect;
    if (alignment != Qt::AlignCenter) {
      textRect.adjust(4, 0, -4, 0); // Add padding for left-aligned text
    }
    painter->drawText(textRect, alignment, text);

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
  void buyRequested(const WindowContext &context);
  void sellRequested(const WindowContext &context);
  void snapQuoteRequested(const WindowContext &context);

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
  void applyColumnVisibility();  // Show/hide columns from settings

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
