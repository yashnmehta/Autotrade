#include "views/MarketWatchWindow.h"
#include "views/ColumnProfileDialog.h"
#include "models/MarketWatchModel.h"
#include "models/TokenAddressBook.h"
#include "services/TokenSubscriptionManager.h"
#include "utils/ClipboardHelpers.h"
#include "utils/SelectionHelpers.h"
#include "views/helpers/MarketWatchHelpers.h"
#include <QHeaderView>
#include <QShortcut>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QClipboard>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <algorithm>

MarketWatchWindow::MarketWatchWindow(QWidget *parent)
    : CustomMarketWatch(parent)
    , m_model(nullptr)
    , m_tokenAddressBook(nullptr)
{
    setupUI();
    setupConnections();
    setupKeyboardShortcuts();
}

MarketWatchWindow::~MarketWatchWindow()
{
    // Unsubscribe all tokens from this watch
    if (m_model) {
        for (int row = 0; row < m_model->rowCount(); ++row) {
            const ScripData &scrip = m_model->getScripAt(row);
            if (scrip.isValid()) {
                TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange, scrip.token);
            }
        }
    }
}

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

void MarketWatchWindow::setupConnections()
{
    // Connect model signals to address book
    connect(m_model, &MarketWatchModel::rowsInserted,
            this, [this](const QModelIndex &, int first, int last) {
        m_tokenAddressBook->onRowsInserted(first, last - first + 1);
    });
    
    connect(m_model, &MarketWatchModel::rowsRemoved,
            this, [this](const QModelIndex &, int first, int last) {
        m_tokenAddressBook->onRowsRemoved(first, last - first + 1);
    });
    
    // Context menu
    connect(this, &QTableView::customContextMenuRequested,
            this, &MarketWatchWindow::showContextMenu);
}

void MarketWatchWindow::setupKeyboardShortcuts()
{
    // Copy shortcut
    QShortcut *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, &MarketWatchWindow::copyToClipboard);
    
    // Cut shortcut
    QShortcut *cutShortcut = new QShortcut(QKeySequence::Cut, this);
    connect(cutShortcut, &QShortcut::activated, this, &MarketWatchWindow::cutToClipboard);
    
    // Paste shortcut
    QShortcut *pasteShortcut = new QShortcut(QKeySequence::Paste, this);
    connect(pasteShortcut, &QShortcut::activated, this, &MarketWatchWindow::pasteFromClipboard);
    
    // Select All
    QShortcut *selectAllShortcut = new QShortcut(QKeySequence::SelectAll, this);
    connect(selectAllShortcut, &QShortcut::activated, this, &QTableView::selectAll);
}

bool MarketWatchWindow::addScrip(const QString &symbol, const QString &exchange, int token)
{
    // Validate token
    if (token <= 0) {
        QMessageBox::warning(this, "Invalid Token", 
                           QString("Cannot add scrip '%1': Invalid token ID (%2).")
                           .arg(symbol).arg(token));
        return false;
    }
    
    // Check for duplicate
    if (hasDuplicate(token)) {
        int existingRow = findTokenRow(token);
        
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Duplicate Scrip");
        msgBox.setText(QString("Scrip '%1' is already in this watch list.").arg(symbol));
        msgBox.setInformativeText(QString("Token: %1\nRow: %2\n\nWould you like to view the existing entry?")
                                  .arg(token).arg(existingRow + 1));
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);
        
        if (msgBox.exec() == QMessageBox::Yes) {
            highlightRow(existingRow);
        }
        
        return false;
    }
    
    // Create scrip data with basic fields
    ScripData scrip;
    scrip.symbol = symbol;
    scrip.exchange = exchange;  // Should be NSEFO, NSECM, BSEFO, BSECM format
    scrip.token = token;
    scrip.isBlankRow = false;
    
    // Initialize all fields to zero/empty
    // These will be populated by real-time updates
    scrip.code = token;  // Use token as code for now
    scrip.ltp = 0.0;
    scrip.change = 0.0;
    scrip.changePercent = 0.0;
    scrip.volume = 0;
    scrip.bid = 0.0;
    scrip.ask = 0.0;
    scrip.high = 0.0;
    scrip.low = 0.0;
    scrip.open = 0.0;
    scrip.close = 0.0;
    scrip.openInterest = 0;
    
    // Extended fields remain empty until populated by feed
    scrip.buyPrice = 0.0;
    scrip.sellPrice = 0.0;
    
    int newRow = m_model->rowCount();
    
    // Add to model
    m_model->addScrip(scrip);
    
    // Subscribe to price updates
    TokenSubscriptionManager::instance()->subscribe(exchange, token);
    
    // Register in address book
    m_tokenAddressBook->addToken(token, newRow);
    
    qDebug() << "[MarketWatchWindow] Added scrip" << symbol << "with token" << token << "at row" << newRow;
    
    emit scripAdded(symbol, exchange, token);
    
    return true;
}

bool MarketWatchWindow::addScripFromContract(const ScripData &contractData)
{
    // Validate token
    if (contractData.token <= 0) {
        QMessageBox::warning(this, "Invalid Token", 
                           QString("Cannot add scrip '%1': Invalid token ID (%2).")
                           .arg(contractData.symbol).arg(contractData.token));
        return false;
    }
    
    // Check for duplicate
    if (hasDuplicate(contractData.token)) {
        int existingRow = findTokenRow(contractData.token);
        
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Duplicate Scrip");
        msgBox.setText(QString("Scrip '%1' is already in this watch list.").arg(contractData.symbol));
        msgBox.setInformativeText(QString("Token: %1\nRow: %2\n\nWould you like to view the existing entry?")
                                  .arg(contractData.token).arg(existingRow + 1));
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);
        
        if (msgBox.exec() == QMessageBox::Yes) {
            highlightRow(existingRow);
        }
        
        return false;
    }
    
    int newRow = m_model->rowCount();
    
    // Add to model with full contract data
    m_model->addScrip(contractData);
    
    // Subscribe to price updates
    TokenSubscriptionManager::instance()->subscribe(contractData.exchange, contractData.token);
    
    // Register in address book
    m_tokenAddressBook->addToken(contractData.token, newRow);
    
    qDebug() << "[MarketWatchWindow] Added scrip" << contractData.symbol << "with token" << contractData.token << "at row" << newRow;
    
    emit scripAdded(contractData.symbol, contractData.exchange, contractData.token);
    
    return true;
}

void MarketWatchWindow::removeScrip(int row)
{
    if (row < 0 || row >= m_model->rowCount()) {
        return;
    }
    
    const ScripData &scrip = m_model->getScripAt(row);
    
    if (!scrip.isBlankRow && scrip.isValid()) {
        // Unsubscribe from price updates
        TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange, scrip.token);
        
        // Remove from address book
        m_tokenAddressBook->removeToken(scrip.token, row);
        
        emit scripRemoved(scrip.token);
    }
    
    // Remove from model
    m_model->removeScrip(row);
    
    qDebug() << "[MarketWatchWindow] Removed scrip at row" << row;
}

void MarketWatchWindow::removeScripByToken(int token)
{
    int row = findTokenRow(token);
    if (row >= 0) {
        removeScrip(row);
    }
}

void MarketWatchWindow::clearAll()
{
    // Unsubscribe all tokens
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const ScripData &scrip = m_model->getScripAt(row);
        if (scrip.isValid()) {
            TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange, scrip.token);
            emit scripRemoved(scrip.token);
        }
    }
    
    // Clear address book
    m_tokenAddressBook->clear();
    
    // Clear model
    m_model->clearAll();
    
    qDebug() << "[MarketWatchWindow] Cleared all scrips";
}

void MarketWatchWindow::insertBlankRow(int position)
{
    m_model->insertBlankRow(position);
    qDebug() << "[MarketWatchWindow] Inserted blank row at position" << position;
}

bool MarketWatchWindow::hasDuplicate(int token) const
{
    return m_tokenAddressBook->hasToken(token);
}

int MarketWatchWindow::findTokenRow(int token) const
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    return rows.isEmpty() ? -1 : rows.first();
}

void MarketWatchWindow::updatePrice(int token, double ltp, double change, double changePercent)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    
    for (int row : rows) {
        m_model->updatePrice(row, ltp, change, changePercent);
    }
}

void MarketWatchWindow::updateVolume(int token, qint64 volume)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    
    for (int row : rows) {
        m_model->updateVolume(row, volume);
    }
}

void MarketWatchWindow::updateBidAsk(int token, double bid, double ask)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    
    for (int row : rows) {
        m_model->updateBidAsk(row, bid, ask);
    }
}

void MarketWatchWindow::updateOHLC(int token, double open, double high, double low, double close)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    
    for (int row : rows) {
        m_model->updateOHLC(row, open, high, low, close);
    }
}

void MarketWatchWindow::updateBidAskQuantities(int token, int bidQty, int askQty)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    
    for (int row : rows) {
        m_model->updateBidAskQuantities(row, bidQty, askQty);
    }
}

void MarketWatchWindow::updateTotalBuySellQty(int token, int totalBuyQty, int totalSellQty)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    
    for (int row : rows) {
        m_model->updateTotalBuySellQty(row, totalBuyQty, totalSellQty);
    }
}

void MarketWatchWindow::updateOpenInterest(int token, qint64 oi, double oiChangePercent)
{
    QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
    
    for (int row : rows) {
        m_model->updateOpenInterestWithChange(row, oi, oiChangePercent);
    }
}

void MarketWatchWindow::deleteSelectedRows()
{
    QModelIndexList selected = this->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }
    
    // Use helper to map proxy rows to source rows (descending order)
    QList<int> sourceRows = SelectionHelpers::getSourceRowsDescending(
        this->selectionModel(), proxyModel());
    
    // Remove rows from source model
    for (int sourceRow : sourceRows) {
        removeScrip(sourceRow);
    }
    
    qDebug() << "[MarketWatchWindow] Deleted" << sourceRows.size() << "rows";
}

void MarketWatchWindow::copyToClipboard()
{
    QModelIndexList selected = this->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }
    
    QString text;
    for (const QModelIndex &proxyIndex : selected) {
        // Get source row to access the actual data with token
        int sourceRow = mapToSource(proxyIndex.row());
        if (sourceRow < 0) continue;
        
        const ScripData &scrip = m_model->getScripAt(sourceRow);
        
        // Use helper to format scrip to TSV
        text += MarketWatchHelpers::formatScripToTSV(scrip) + "\n";
    }
    
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text);
    
    qDebug() << "[MarketWatchWindow] Copied" << selected.size() << "rows to clipboard (with token data)";
}

void MarketWatchWindow::cutToClipboard()
{
    copyToClipboard();
    deleteSelectedRows();
}

void MarketWatchWindow::pasteFromClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString text = clipboard->text();
    
    if (text.isEmpty()) {
        return;
    }
    
    // Validate TSV format
    if (!ClipboardHelpers::isValidTSV(text, 3)) {
        QMessageBox::warning(this, "Invalid Format",
                           "Clipboard data format is invalid.\n\n"
                           "Expected format: Symbol\\tExchange\\tToken\\t...\n"
                           "Tip: Copy rows from another Market Watch window.");
        return;
    }
    
    // Get insertion position (current row or end)
    int insertPosition = m_model->rowCount();  // Default: end
    QModelIndex currentProxyIndex = this->currentIndex();
    if (currentProxyIndex.isValid()) {
        int sourceRow = mapToSource(currentProxyIndex.row());
        if (sourceRow >= 0) {
            insertPosition = sourceRow + 1;  // Insert after current row
        }
    }
    
    // Parse TSV format using helper
    QList<QStringList> rows = ClipboardHelpers::parseTSV(text);
    int addedCount = 0;
    int skippedCount = 0;
    int currentInsertPos = insertPosition;
    
    // Temporarily collect scrips to insert
    QList<ScripData> scripsToInsert;
    
    for (const QStringList &fields : rows) {
        QString line = fields.join('\t');
        
        // Check if blank line
        if (MarketWatchHelpers::isBlankLine(line)) {
            skippedCount++;
            continue;
        }
        
        // Parse scrip using helper
        ScripData scrip;
        if (!MarketWatchHelpers::parseScripFromTSV(line, scrip)) {
            skippedCount++;
            continue;
        }
        
        // Validate scrip
        if (!MarketWatchHelpers::isValidScrip(scrip)) {
            skippedCount++;
            continue;
        }
        
        // Check for duplicates
        if (hasDuplicate(scrip.token)) {
            skippedCount++;
            continue;
        }
        
        scripsToInsert.append(scrip);
    }
    
    // Insert scrips at the specified position
    for (const ScripData &scrip : scripsToInsert) {
        // Insert at position
        m_model->insertScrip(currentInsertPos, scrip);
        
        // Subscribe to updates
        TokenSubscriptionManager::instance()->subscribe(scrip.exchange, scrip.token);
        
        // Register in address book
        m_tokenAddressBook->addToken(scrip.token, currentInsertPos);
        
        emit scripAdded(scrip.symbol, scrip.exchange, scrip.token);
        
        addedCount++;
        currentInsertPos++;
        
        qDebug() << "[MarketWatchWindow] Pasted scrip" << scrip.symbol << "at position" << currentInsertPos - 1;
    }
    
    skippedCount = rows.size() - addedCount;
    
    QString message;
    if (addedCount > 0) {
        message = QString("Successfully pasted %1 scrip(s) at position %2.").arg(addedCount).arg(insertPosition + 1);
        if (skippedCount > 0) {
            message += QString("\n%1 scrip(s) were skipped (duplicates or invalid).").arg(skippedCount);
        }
        qDebug() << "[MarketWatchWindow] Pasted" << addedCount << "scrips at position" << insertPosition;
    } else {
        message = "No scrips were pasted. ";
        if (skippedCount > 0) {
            message += QString("All %1 scrip(s) were duplicates or invalid.").arg(skippedCount);
        }
    }
    
    if (addedCount > 0) {
        QMessageBox::information(this, "Paste Scrips", message);
    }
}

void MarketWatchWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        deleteSelectedRows();
        event->accept();
        return;
    }
    
    QWidget::keyPressEvent(event);
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

void MarketWatchWindow::onBuyAction()
{
    QModelIndex proxyIndex = this->currentIndex();
    if (!proxyIndex.isValid()) {
        return;
    }
    
    int sourceRow = mapToSource(proxyIndex.row());
    if (sourceRow < 0) return;
    
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    if (scrip.isValid()) {
        emit buyRequested(scrip.symbol, scrip.token);
        qDebug() << "[MarketWatchWindow] Buy requested for" << scrip.symbol;
    }
}

void MarketWatchWindow::onSellAction()
{
    QModelIndex proxyIndex = this->currentIndex();
    if (!proxyIndex.isValid()) {
        return;
    }
    
    int sourceRow = mapToSource(proxyIndex.row());
    if (sourceRow < 0) return;
    
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    if (scrip.isValid()) {
        emit sellRequested(scrip.symbol, scrip.token);
        qDebug() << "[MarketWatchWindow] Sell requested for" << scrip.symbol;
    }
}

void MarketWatchWindow::onAddScripAction()
{
    // This would typically open the ScripBar or a scrip selection dialog
    qDebug() << "[MarketWatchWindow] Add scrip requested - use ScripBar";
    
    QMessageBox::information(this, "Add Scrip",
                           "Use the ScripBar (above the Market Watch area) to search and add scrips.\n\n"
                           "Press Ctrl+S to focus the ScripBar.");
}

void MarketWatchWindow::performRowMoveByTokens(const QList<int> &tokens, int targetSourceRow)
{
    if (tokens.isEmpty()) {
        qDebug() << "[PerformMoveByTokens] No tokens, aborting";
        return;
    }
    
    // CRITICAL FIX: Clear sort indicator during manual reorder
    // This shows the user that data is no longer sorted
    // Sorting remains enabled - user can re-sort by clicking headers
    int currentSortColumn = this->horizontalHeader()->sortIndicatorSection();
    if (currentSortColumn >= 0) {
        qDebug() << "[PerformMoveByTokens] Clearing sort indicator (was column" << currentSortColumn << ") - manual reorder in progress";
        this->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    }
    
    // Resolve tokens to CURRENT source rows (fresh lookups avoid stale indices)
    QList<int> sourceRows;
    QList<ScripData> scrips;
    for (int token : tokens) {
        int row = m_model->findScripByToken(token);
        if (row >= 0) {
            sourceRows.append(row);
            scrips.append(m_model->getScripAt(row));
            qDebug() << "[PerformMoveByTokens] Token" << token << "-> source row" << row;
        } else {
            qDebug() << "[PerformMoveByTokens] WARNING: token" << token << "not found in model!";
        }
    }
    
    if (sourceRows.isEmpty()) {
        qDebug() << "[PerformMoveByTokens] No valid source rows resolved, aborting";
        return;
    }
    
    qDebug() << "[PerformMoveByTokens] === Moving" << sourceRows.size() << "token(s) to target source row" << targetSourceRow << "===";
    
    // Single row move (most common case)
    if (sourceRows.size() == 1) {
        int sourceRow = sourceRows.first();
        const ScripData &scrip = scrips.first();
        
        if (sourceRow == targetSourceRow || sourceRow == targetSourceRow - 1) {
            qDebug() << "[PerformMoveByTokens] No-op move (same position), aborting";
            return;
        }
        
        qDebug() << "[PerformMoveByTokens] Single row move: source=" << sourceRow << "target=" << targetSourceRow;
        qDebug() << "[PerformMoveByTokens] Moving scrip:" << scrip.symbol << "token:" << scrip.token;
        
        // Notify address book BEFORE the move
        m_tokenAddressBook->onRowsRemoved(sourceRow, 1);
        
        // Perform the move in the model
        m_model->moveRow(sourceRow, targetSourceRow);
        
        // Calculate final position
        int finalPos = targetSourceRow;
        if (sourceRow < targetSourceRow) {
            finalPos = targetSourceRow - 1;
        }
        qDebug() << "[PerformMoveByTokens] Final source position:" << finalPos;
        
        // Notify address book AFTER the move
        m_tokenAddressBook->onRowsInserted(finalPos, 1);
        
        // Update selection/current index to the moved row (in proxy coordinates)
        // With sorting disabled, proxy row == source row
        int proxyPos = mapToProxy(finalPos);
        if (proxyPos >= 0) {
            QModelIndex proxyIndex = proxyModel()->index(proxyPos, 0);
            this->selectionModel()->clearSelection();
            this->selectionModel()->select(proxyIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            this->setCurrentIndex(proxyIndex);
            qDebug() << "[PerformMoveByTokens] Selection updated to proxy row" << proxyPos << "(source" << finalPos << ")";
        }
    } else {
        // Multi-row move
        qDebug() << "[PerformMoveByTokens] Multi-row move not fully implemented yet, using remove/insert";
        
        // Sort rows in descending order for removal
        QList<int> sortedRows = sourceRows;
        std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());
        
        int adjustment = 0;
        for (int sourceRow : sortedRows) {
            if (sourceRow < targetSourceRow) {
                adjustment++;
            }
            removeScrip(sourceRow);
        }
        
        // Adjust target
        int adjustedTarget = targetSourceRow - adjustment;
        if (adjustedTarget < 0) adjustedTarget = 0;
        if (adjustedTarget > m_model->rowCount()) adjustedTarget = m_model->rowCount();
        
        // Insert at new position
        for (int i = 0; i < scrips.size(); ++i) {
            m_model->insertScrip(adjustedTarget + i, scrips[i]);
            m_tokenAddressBook->addToken(scrips[i].token, adjustedTarget + i);
        }
        
        // Update selection
        int proxyStart = mapToProxy(adjustedTarget);
        if (proxyStart >= 0) {
            this->selectionModel()->clearSelection();
            for (int i = 0; i < scrips.size(); ++i) {
                QModelIndex pidx = proxyModel()->index(proxyStart + i, 0);
                this->selectionModel()->select(pidx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            }
            QModelIndex firstIdx = proxyModel()->index(proxyStart, 0);
            this->setCurrentIndex(firstIdx);
            qDebug() << "[PerformMoveByTokens] Selection updated to proxy range" << proxyStart << ".." << proxyStart + scrips.size() - 1;
        }
    }
}

// === Virtual Method Overrides for Base Class ===

int MarketWatchWindow::getTokenForRow(int sourceRow) const
{
    if (sourceRow < 0 || sourceRow >= m_model->rowCount()) {
        return -1;
    }
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    return scrip.isValid() ? scrip.token : -1;
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

bool MarketWatchWindow::isBlankRow(int sourceRow) const
{
    if (sourceRow < 0 || sourceRow >= m_model->rowCount()) {
        return false;
    }
    return m_model->isBlankRow(sourceRow);
}
