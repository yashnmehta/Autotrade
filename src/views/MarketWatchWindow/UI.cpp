#include "views/MarketWatchWindow.h"
#include "views/ColumnProfileDialog.h"
#include "utils/WindowSettingsHelper.h"
#include "models/TokenAddressBook.h"
#include <QHeaderView>
#include <QShortcut>
#include <QMenu>
#include <QDebug>

#include <QStyledItemDelegate>
#include <QPainter>

/**
 * @brief Custom delegate for Market Watch to handle special background coloring
 * 
 * Ensures that background colors provided by the model (e.g., for LTP/Bid/Ask ticks)
 * take precedence over the standard selection highlighting.
 */
class MarketWatchDelegate : public QStyledItemDelegate {
public:
    explicit MarketWatchDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Look for model-provided background color (Ticks)
        QVariant bg = index.data(Qt::BackgroundRole);
        
        if (bg.isValid()) {
            painter->save();
            // Fill cell with model color - this overrides the standard selection blue
            painter->fillRect(opt.rect, bg.value<QColor>());
            painter->restore();

            // Set text color to white for contrast on vibrant tick backgrounds
            opt.palette.setColor(QPalette::Text, Qt::white);
            opt.palette.setColor(QPalette::HighlightedText, Qt::white);
            
            // Prevent standard delegate from drawing its own background (including selection)
            opt.backgroundBrush = Qt::NoBrush;
            opt.state &= ~QStyle::State_Selected;
        }

        QStyledItemDelegate::paint(painter, opt, index);
    }
};

void MarketWatchWindow::setupUI()
{
    // Create model
    m_model = new MarketWatchModel(this);
    
    // Set model on base class (which wraps it in proxy model)
    setSourceModel(m_model);
    
    // Set custom delegate for tick highlighting behavior
    setItemDelegate(new MarketWatchDelegate(this));
    
    // Enable ultra-low latency (65ns vs 15ms) mode by registering direct callback ✅
    m_model->setViewCallback(this);

    // Enable column reordering via drag-and-drop
    horizontalHeader()->setSectionsMovable(true);
    
    // Create token address book
    m_tokenAddressBook = new TokenAddressBook(this);

    // Set context menu policy
    setContextMenuPolicy(Qt::CustomContextMenu);
}

void MarketWatchWindow::showContextMenu(const QPoint &pos)
{
    QModelIndex proxyIndex = this->indexAt(pos);
    int sourceRow = mapToSource(proxyIndex.row());
    
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu {"
        "    background-color: #252526;"
        "    color: #ffffff;"
        "    border: 1px solid #3e3e42;"
        "}"
        "QMenu::item {"
        "    padding: 6px 20px;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #094771;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background: #3e3e42;"
        "    margin: 4px 0px;"
        "}"
    );
    
    // Trading actions (if valid row selected)
    if (proxyIndex.isValid() && sourceRow >= 0 && !m_model->isBlankRow(sourceRow)) {
        menu.addAction("Buy (F1)", this, &MarketWatchWindow::onBuyAction);
        menu.addAction("Sell (F2)", this, &MarketWatchWindow::onSellAction);
        menu.addSeparator();
    }
    
    // Add scrip
    menu.addAction("Add Scrip", this, &MarketWatchWindow::onAddScripAction);
    
    // Blank row insertion
    menu.addSeparator();
    if (proxyIndex.isValid() && sourceRow >= 0) {
        menu.addAction("Insert Blank Row Above", this, [this, sourceRow]() {
            insertBlankRow(sourceRow);
        });
        menu.addAction("Insert Blank Row Below", this, [this, sourceRow]() {
            insertBlankRow(sourceRow + 1);
        });
    } else {
        menu.addAction("Insert Blank Row", this, [this]() {
            insertBlankRow(m_model->rowCount());
        });
    }
    
    // Delete and clipboard operations
    if (proxyIndex.isValid()) {
        menu.addSeparator();
        menu.addAction("Delete (Del)", this, &MarketWatchWindow::deleteSelectedRows);
        menu.addSeparator();
        menu.addAction("Copy (Ctrl+C)", this, &MarketWatchWindow::copyToClipboard);
        menu.addAction("Cut (Ctrl+X)", this, &MarketWatchWindow::cutToClipboard);
    }
    
    menu.addAction("Paste (Ctrl+V)", this, &MarketWatchWindow::pasteFromClipboard);
    
    // Column profile
    menu.addSeparator();
    menu.addAction("Column Profile...", this, &MarketWatchWindow::showColumnProfileDialog);

    // Portfolio Management
    menu.addSeparator();
    menu.addAction("Save Portfolio...", this, &MarketWatchWindow::onSavePortfolio);
    menu.addAction("Load Portfolio...", this, &MarketWatchWindow::onLoadPortfolio);
    
    // Debug Tools
    menu.addSeparator();
    menu.addAction("Export Cache Debug (Ctrl+Shift+E)", this, &MarketWatchWindow::exportPriceCacheDebug);
    
    menu.exec(this->viewport()->mapToGlobal(pos));
}

void MarketWatchWindow::showColumnProfileDialog()
{
    ColumnProfileDialog dialog(m_model->getColumnProfile(), ProfileContext::MarketWatch, this);
    
    if (dialog.exec() == QDialog::Accepted && dialog.wasAccepted()) {
        MarketWatchColumnProfile newProfile = dialog.getProfile();
        m_model->setColumnProfile(newProfile);
        applyProfileToView(newProfile);
        
        qDebug() << "[MarketWatchWindow] Column profile updated to:" << newProfile.name();
    }
}

void MarketWatchWindow::setupKeyboardShortcuts()
{
    // Ctrl+Shift+E - Export price cache for debugging
    QShortcut *exportShortcut = new QShortcut(QKeySequence("Ctrl+Shift+E"), this);
    connect(exportShortcut, &QShortcut::activated, this, &MarketWatchWindow::exportPriceCacheDebug);
    
    qDebug() << "[MarketWatch] Keyboard shortcuts registered: Ctrl+Shift+E = Export Cache";
}
