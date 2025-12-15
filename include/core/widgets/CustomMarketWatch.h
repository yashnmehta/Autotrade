#ifndef CUSTOMMARKETWATCH_H
#define CUSTOMMARKETWATCH_H

#include <QTableView>
#include <QHeaderView>
#include <QMenu>

/**
 * @brief Base class for market watch style table views
 * 
 * This class provides a pre-configured QTableView optimized for displaying
 * market data with consistent styling and behavior across the application.
 * 
 * Features:
 * - Pre-configured table styling (white background, no grid, etc.)
 * - Header customization
 * - Context menu support
 * - Column management
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
    QMenu *m_contextMenu;
};

#endif // CUSTOMMARKETWATCH_H
