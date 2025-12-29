#ifndef CUSTOMNETPOSITION_H
#define CUSTOMNETPOSITION_H

#include <QTableView>
#include <QHeaderView>
#include <QMenu>

/**
 * @brief Base class for position/P&L style table views
 * 
 * This class provides a pre-configured QTableView optimized for displaying
 * position data, P&L, and related financial information with consistent
 * styling and behavior across the application.
 * 
 * Features:
 * - Pre-configured table styling for financial data
 * - Header customization
 * - Context menu support
 * - Column management
 * - Summary row support
 * 
 * Usage:
 * Subclass this in views/ directory for specific position implementations:
 * - PositionWindow (Integrated Net Position)
 * - OrderBookWindow
 * - TradeBookWindow
 * - etc.
 * 
 * @note This is a STABLE widget in core/widgets. Modifications should be rare.
 */
class CustomNetPosition : public QTableView
{
    Q_OBJECT

public:
    explicit CustomNetPosition(QWidget *parent = nullptr);
    virtual ~CustomNetPosition();

    /**
     * @brief Apply default position table styling
     * Called automatically in constructor, but can be re-called if needed
     */
    void applyDefaultStyling();

    /**
     * @brief Setup default header configuration
     */
    void setupHeader();

public slots:
    void setSummaryRowEnabled(bool enabled);

    /**
     * @brief Check if summary row is enabled
     */
    bool isSummaryRowEnabled() const { return m_summaryRowEnabled; }

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
     * @brief Emitted when user requests to close a position
     */
    void closePositionRequested();

    /**
     * @brief Emitted when user requests to export data
     */
    void exportRequested();

protected:
    QMenu *m_contextMenu;
    bool m_summaryRowEnabled;
};

#endif // CUSTOMNETPOSITION_H
