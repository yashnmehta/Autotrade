#ifndef MARKETWATCHWINDOW_H
#define MARKETWATCHWINDOW_H

#include <QWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QSortFilterProxyModel>

class MarketWatchModel;
class TokenAddressBook;

/**
 * @brief Professional Market Watch window with real-time updates
 * 
 * Features:
 * - QTableView with custom model for efficient updates
 * - Token-based duplicate prevention
 * - Blank row separators for organization
 * - Context menu with trading actions
 * - Clipboard operations (Ctrl+C/X/V)
 * - Auto-subscription management
 * - Fast O(1) price updates
 */
class MarketWatchWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MarketWatchWindow(QWidget *parent = nullptr);
    virtual ~MarketWatchWindow();

    // === Core Operations ===
    
    /**
     * @brief Add a scrip to the market watch
     * @param symbol Symbol name (e.g., "NIFTY 50")
     * @param exchange Exchange name (e.g., "NSE")
     * @param token Unique token ID for API
     * @return true if added, false if duplicate or invalid
     */
    bool addScrip(const QString &symbol, const QString &exchange, int token);
    
    /**
     * @brief Remove a scrip by row index
     * @param row Row index to remove
     */
    void removeScrip(int row);
    
    /**
     * @brief Remove a scrip by token
     * @param token Token ID to remove
     */
    void removeScripByToken(int token);
    
    /**
     * @brief Clear all scrips
     */
    void clearAll();
    
    // === Blank Row Operations ===
    
    /**
     * @brief Insert a blank separator row
     * @param position Position to insert (-1 for end)
     */
    void insertBlankRow(int position = -1);
    
    // === Query Operations ===
    
    /**
     * @brief Check if a token already exists
     * @param token Token ID to check
     * @return true if token exists
     */
    bool hasDuplicate(int token) const;
    
    /**
     * @brief Find the row index for a token
     * @param token Token ID
     * @return Row index or -1 if not found
     */
    int findTokenRow(int token) const;
    
    /**
     * @brief Get the token address book for price updates
     * @return Pointer to TokenAddressBook
     */
    TokenAddressBook* getTokenAddressBook() const { return m_tokenAddressBook; }
    
    /**
     * @brief Get the underlying model
     * @return Pointer to MarketWatchModel
     */
    MarketWatchModel* getModel() const { return m_model; }
    
    // === Price Update Operations ===
    
    /**
     * @brief Update price for a token (O(1) lookup)
     * @param token Token ID
     * @param ltp Last traded price
     * @param change Absolute change
     * @param changePercent Percentage change
     */
    void updatePrice(int token, double ltp, double change, double changePercent);
    
    /**
     * @brief Update volume for a token
     * @param token Token ID
     * @param volume Volume
     */
    void updateVolume(int token, qint64 volume);
    
    /**
     * @brief Update bid/ask for a token
     * @param token Token ID
     * @param bid Bid price
     * @param ask Ask price
     */
    void updateBidAsk(int token, double bid, double ask);

public slots:
    /**
     * @brief Delete selected rows
     */
    void deleteSelectedRows();
    
    /**
     * @brief Copy selected rows to clipboard
     */
    void copyToClipboard();
    
    /**
     * @brief Cut selected rows to clipboard
     */
    void cutToClipboard();
    
    /**
     * @brief Paste from clipboard
     */
    void pasteFromClipboard();

signals:
    /**
     * @brief Emitted when a scrip is added
     */
    void scripAdded(const QString &symbol, const QString &exchange, int token);
    
    /**
     * @brief Emitted when a scrip is removed
     */
    void scripRemoved(int token);
    
    /**
     * @brief Emitted when Buy action is requested
     */
    void buyRequested(const QString &symbol, int token);
    
    /**
     * @brief Emitted when Sell action is requested
     */
    void sellRequested(const QString &symbol, int token);

protected:
    /**
     * @brief Handle keyboard events
     */
    void keyPressEvent(QKeyEvent *event) override;
    
    /**
     * @brief Handle mouse press for drag initiation
     */
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void showContextMenu(const QPoint &pos);
    void onBuyAction();
    void onSellAction();
    void onAddScripAction();

private:
    void setupUI();
    void setupConnections();
    void setupKeyboardShortcuts();
    void highlightRow(int row);
    
    // Helper methods for proxy model index mapping
    int mapToSource(int proxyRow) const;
    int mapToProxy(int sourceRow) const;
    
    // Drag & drop helpers
    void startRowDrag(const QPoint &pos);
    int getDropRow(const QPoint &pos) const;
    void performRowMove(const QList<int> &sourceRows, int targetRow);  // DEPRECATED: use token-based version
    void performRowMoveByTokens(const QList<int> &tokens, int targetSourceRow);  // NEW: token-based drag
    
    // UI Components
    QVBoxLayout *m_layout;
    QTableView *m_tableView;
    
    // Data Components
    MarketWatchModel *m_model;
    QSortFilterProxyModel *m_proxyModel;
    TokenAddressBook *m_tokenAddressBook;
    
    // UI State
    QTimer *m_highlightTimer;
    
    // Drag & drop state
    QPoint m_dragStartPos;
    bool m_isDragging;
    QList<int> m_draggedRows;  // DEPRECATED: kept for compatibility, use m_draggedTokens
    QList<int> m_draggedTokens;  // Stable token IDs of scrips being dragged
    
    // Selection state
    int m_selectionAnchor;  // Anchor row for Shift-selection (proxy row)
};

#endif // MARKETWATCHWINDOW_H
