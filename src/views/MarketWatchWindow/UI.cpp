#include "views/MarketWatchWindow.h"
#include "views/ColumnProfileDialog.h"
#include "utils/WindowSettingsHelper.h"
#include "models/TokenAddressBook.h"
#include <QHeaderView>
#include <QShortcut>
#include <QMenu>
#include <QDebug>

void MarketWatchWindow::setupUI()
{
    // Create model
    m_model = new MarketWatchModel(this);
    
    // Set model on base class (which wraps it in proxy model)
    setSourceModel(m_model);
    
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
    
    menu.exec(this->viewport()->mapToGlobal(pos));
}

void MarketWatchWindow::showColumnProfileDialog()
{
    ColumnProfileDialog dialog(m_model->getColumnProfile(), this);
    
    if (dialog.exec() == QDialog::Accepted && dialog.wasAccepted()) {
        MarketWatchColumnProfile newProfile = dialog.getProfile();
        m_model->setColumnProfile(newProfile);
        
        qDebug() << "[MarketWatchWindow] Column profile updated to:" << newProfile.name();
    }
}
