#ifndef CUSTOMMARKETWATCH_H
#define CUSTOMMARKETWATCH_H

#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QPoint>

class QAbstractItemModel;

/**
 * @brief Base class for market watch style table views
 * 
 * This class provides a pre-configured QTableView optimized for displaying
 * market data with consistent styling and behavior across the application.
 * 
 * Features:
 * - Pre-configured table styling (dark theme)
 * - Header customization
 * - Drag-and-drop row reordering
 * - Multi-select with Ctrl/Shift
 * - Context menu support
 * - Proxy model support for sorting
 * 
 * Usage:
 * Subclass this in views/ directory for specific market watch implementations:
 * - MarketWatchWindow
 * - OptionChainWindow
 * - etc.
 * 
 * @note This is a STABLE widget in core/widgets. Modifications should be rare.
 */
class CustomMarketWatch : public QTableView
{
    Q_OBJECT

public:
    explicit CustomMarketWatch(QWidget *parent = nullptr);
    virtual ~CustomMarketWatch();

    /**
     * @brief Apply default market watch styling
     * Called automatically in constructor, but can be re-called if needed
     */
    void applyDefaultStyling();

    /**
     * @brief Setup default header configuration
     */
    void setupHeader();

    /**
     * @brief Get proxy model for sorting
     */
    QSortFilterProxyModel* proxyModel() const { return m_proxyModel; }

    /**
     * @brief Set the source model (wraps with proxy model)
     * @param model Source model to display
     */
    void setSourceModel(QAbstractItemModel *model);

protected:
    /**
     * @brief Context menu event for right-click actions
     * Override in subclasses to add custom menu items
     */
    void contextMenuEvent(QContextMenuEvent *event) override;

    /**
     * @brief Create default context menu
     * Can be called from subclass contextMenuEvent
     */
    virtual QMenu* createContextMenu();

    /**
     * @brief Event filter for drag-and-drop functionality
     */
    bool eventFilter(QObject *obj, QEvent *event) override;

    /**
     * @brief Map proxy row to source row
     */
    int mapToSource(int proxyRow) const;

    /**
     * @brief Map source row to proxy row
     */
    int mapToProxy(int sourceRow) const;

    /**
     * @brief Called when rows are dragged and dropped
     * Override in subclass to handle the actual move operation
     * @param tokens List of token IDs being moved
     * @param targetSourceRow Target row in source model
     */
    virtual void performRowMoveByTokens(const QList<int> &tokens, int targetSourceRow);

    /**
     * @brief Get token ID for a given source row
     * Override in subclass to return the actual token
     * @param sourceRow Row in source model
     * @return Token ID or -1 if invalid
     */
    virtual int getTokenForRow(int sourceRow) const { return -1; }

    /**
     * @brief Check if a row is blank/separator
     * Override in subclass if model has blank rows
     */
    virtual bool isBlankRow(int sourceRow) const { return false; }

    /**
     * @brief Highlight a source row with flash effect
     * @param sourceRow Row in source model to highlight
     * 
     * Selects and scrolls to the row, then applies a brief flash animation.
     * Useful for drawing user attention to specific rows.
     */
    void highlightRow(int sourceRow);

signals:
    /**
     * @brief Emitted when user requests to add a new scrip
     */
    void addScripRequested();

    /**
     * @brief Emitted when user requests to remove selected scrip(s)
     */
    void removeScripRequested();

protected:
    // Proxy model for sorting
    QSortFilterProxyModel *m_proxyModel;

    // Drag & drop state
    QPoint m_dragStartPos;
    bool m_isDragging;
    QList<int> m_draggedTokens;

    // Selection state
    int m_selectionAnchor;  // Anchor row for Shift-selection (proxy coordinates)

protected:
    // Override key events to dynamically change selection mode
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    // Helper methods
    int getDropRow(const QPoint &pos) const;
};

#endif // CUSTOMMARKETWATCH_H
