#ifndef MARKETWATCHWINDOW_H
#define MARKETWATCHWINDOW_H

#include "core/widgets/CustomMarketWatch.h"
#include "models/MarketWatchModel.h"  // For ScripData
#include "models/IMarketWatchViewCallback.h"  // For native C++ callbacks
#include "models/WindowContext.h"  // For context-aware window opening
#include "services/FeedHandler.h"  // Phase 2: Direct callback-based updates
#include "udp/UDPTypes.h"  // Phase 3: UDP-specific tick data
#include <QVBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <unordered_map>

class TokenAddressBook;
class XTSMarketDataClient;

/**
 * @brief Professional Market Watch window with real-time updates
 * 
 * Inherits from CustomMarketWatch for table view behavior.
 * Adds market data management and scrip subscription logic.
 * 
 * Implements IMarketWatchViewCallback for ultra-low latency updates (65ns vs 15ms).
 * 
 * Features:
 * - Token-based duplicate prevention
 * - Blank row separators for organization
 * - Context menu with trading actions
 * - Clipboard operations (Ctrl+C/X/V)
 * - Auto-subscription management
 * - Fast O(1) price updates
 * - Native C++ callbacks (no Qt signal latency)
 */
class MarketWatchWindow : public CustomMarketWatch, public IMarketWatchViewCallback
{
    Q_OBJECT

public:
    explicit MarketWatchWindow(QWidget *parent = nullptr);
    virtual ~MarketWatchWindow();

    // === Core Operations ===
    
    /**
     * @brief Add a scrip to the market watch (basic method)
     * @param symbol Symbol name (e.g., "NIFTY 50")
     * @param exchange Exchange segment (e.g., "NSEFO", "NSECM", "BSEFO", "BSECM")
     * @param token Unique token ID for API
     * @return true if added, false if duplicate or invalid
     */
    bool addScrip(const QString &symbol, const QString &exchange, int token);
    
    /**
     * @brief Add a scrip with full contract information
     * @param contractData Complete contract data from repository
     * @return true if added, false if duplicate or invalid
     */
    bool addScripFromContract(const ScripData &contractData);
    
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
     * @brief Set XTS Market Data Client for Quote API fallback
     * @param client XTSMarketDataClient instance
     */
    void setXTSClient(XTSMarketDataClient *client) { m_xtsClient = client; }
    
    /**
     * @brief Get the token address book for price updates
     * @return Pointer to TokenAddressBook
     */
    TokenAddressBook* getTokenAddressBook() const { return m_tokenAddressBook; }
    
    /**
     * @brief Get WindowContext for selected scrip
     * @return WindowContext with contract and market data, or invalid context if no selection
     */
    WindowContext getSelectedContractContext() const;
    
    /**
     * @brief Check if there's a valid selection
     * @return true if at least one non-blank row is selected
     */
    bool hasValidSelection() const;
    
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
    void updateLastTradedQuantity(int token, qint64 ltq);
    
    /**
     * @brief Update OHLC data for a token
     * @param token Token ID
     * @param open Open price
     * @param high High price
     * @param low Low price
     * @param close Close price
     */
    void updateOHLC(int token, double open, double high, double low, double close);
    
    /**
     * @brief Update bid/ask quantities for a token
     * @param token Token ID
     * @param bidQty Bid quantity
     * @param askQty Ask quantity
     */
    void updateBidAskQuantities(int token, int bidQty, int askQty);
    
    /**
     * @brief Update total buy/sell quantities for a token
     * @param token Token ID
     * @param totalBuyQty Total buy quantity
     * @param totalSellQty Total sell quantity
     */
    void updateTotalBuySellQty(int token, int totalBuyQty, int totalSellQty);
    
    /**
     * @brief Update open interest for a token
     * @param token Token ID
     * @param oi Open interest
     * @param oiChangePercent OI change percentage
     */
    void updateOpenInterest(int token, qint64 oi, double oiChangePercent);
    
    // ===================================================================
    // IMarketWatchViewCallback Implementation (Ultra-Low Latency Mode)
    // ===================================================================
    
    /**
     * @brief Native C++ callback for row updates (replaces Qt signal dataChanged)
     * 
     * Performance: 10-50ns (direct viewport invalidation) vs 500ns-15ms (Qt signal)
     * 
     * @param row Source model row index
     * @param firstColumn First affected column
     * @param lastColumn Last affected column (inclusive)
     */
    void onRowUpdated(int row, int firstColumn, int lastColumn) override;
    
    /**
     * @brief Native C++ callback for rows inserted
     */
    void onRowsInserted(int firstRow, int lastRow) override;
    
    /**
     * @brief Native C++ callback for rows removed
     */
    void onRowsRemoved(int firstRow, int lastRow) override;
    
    /**
     * @brief Native C++ callback for model reset
     */
    void onModelReset() override;

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
    
    // Context-aware signals (new) - includes full contract and market data
    void buyRequestedWithContext(const WindowContext &context);
    void sellRequestedWithContext(const WindowContext &context);

protected:
    /**
     * @brief Handle keyboard events (Delete key)
     */
    void keyPressEvent(QKeyEvent *event) override;
    
    /**
     * @brief Override to provide token for drag-and-drop
     */
    int getTokenForRow(int sourceRow) const override;
    
    /**
     * @brief Override to check if row is blank
     */
    bool isBlankRow(int sourceRow) const override;
    
    /**
     * @brief Override to handle row moves
     */
    void performRowMoveByTokens(const QList<int> &tokens, int targetSourceRow) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void showContextMenu(const QPoint &pos);
    void showColumnProfileDialog();
    void onBuyAction();
    void onSellAction();
    void onAddScripAction();
    void onSavePortfolio();
    void onLoadPortfolio();

private:
    void setupUI();
    void setupConnections();
    void setupKeyboardShortcuts();
    
    // Phase 2: FeedHandler callback for direct tick updates (legacy)
    void onTickUpdate(const XTS::Tick& tick);
    
    // Phase 3: UDP-specific tick updates (cleaner semantics)
    void onUdpTickUpdate(const UDP::MarketTick& tick);
    
    // Data Components
    MarketWatchModel *m_model;
    TokenAddressBook *m_tokenAddressBook;
    XTSMarketDataClient *m_xtsClient;  // For BSE quote API fallback
};

#endif // MARKETWATCHWINDOW_H
