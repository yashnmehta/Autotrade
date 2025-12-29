#include "views/MarketWatchWindow.h"
#include "models/TokenAddressBook.h"
#include "services/TokenSubscriptionManager.h"
#include "utils/ClipboardHelpers.h"
#include "utils/SelectionHelpers.h"
#include "views/helpers/MarketWatchHelpers.h"
#include "repository/RepositoryManager.h"
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QDebug>
#include <algorithm>

bool MarketWatchWindow::addScrip(const QString &symbol, const QString &exchange, int token)
{
    if (token <= 0) return false;
    if (hasDuplicate(token)) return false;
    
    ScripData scrip;
    scrip.symbol = symbol;
    scrip.exchange = exchange;
    scrip.token = token;
    scrip.isBlankRow = false;
    scrip.code = token;
    
    int newRow = m_model->rowCount();
    m_model->addScrip(scrip);
    
    TokenSubscriptionManager::instance()->subscribe(exchange, token);
    auto subId = FeedHandler::instance().subscribe(
        (uint32_t)token,
        [this](const XTS::Tick& tick) { this->onTickUpdate(tick); },
        this
    );
    m_feedSubscriptions[token] = subId;
    m_tokenAddressBook->addToken(token, newRow);
    
    emit scripAdded(symbol, exchange, token);
    return true;
}

bool MarketWatchWindow::addScripFromContract(const ScripData &contractData)
{
    if (contractData.token <= 0) return false;
    if (hasDuplicate(contractData.token)) return false;
    
    int newRow = m_model->rowCount();
    m_model->addScrip(contractData);
    
    TokenSubscriptionManager::instance()->subscribe(contractData.exchange, contractData.token);
    auto subId = FeedHandler::instance().subscribe(
        (uint32_t)contractData.token,
        [this](const XTS::Tick& tick) { this->onTickUpdate(tick); },
        this
    );
    m_feedSubscriptions[contractData.token] = subId;
    m_tokenAddressBook->addToken(contractData.token, newRow);
    
    emit scripAdded(contractData.symbol, contractData.exchange, contractData.token);
    return true;
}

void MarketWatchWindow::removeScrip(int row)
{
    if (row < 0 || row >= m_model->rowCount()) return;
    const ScripData &scrip = m_model->getScripAt(row);
    if (!scrip.isBlankRow && scrip.isValid()) {
        TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange, scrip.token);
        auto it = m_feedSubscriptions.find(scrip.token);
        if (it != m_feedSubscriptions.end()) {
            FeedHandler::instance().unsubscribe(it->second);
            m_feedSubscriptions.erase(it);
        }
        m_tokenAddressBook->removeToken(scrip.token, row);
        emit scripRemoved(scrip.token);
    }
    m_model->removeScrip(row);
}

void MarketWatchWindow::clearAll()
{
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const ScripData &scrip = m_model->getScripAt(row);
        if (scrip.isValid()) {
            TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange, scrip.token);
            emit scripRemoved(scrip.token);
        }
    }
    m_tokenAddressBook->clear();
    m_model->clearAll();
}

void MarketWatchWindow::insertBlankRow(int position)
{
    m_model->insertBlankRow(position);
}

void MarketWatchWindow::deleteSelectedRows()
{
    QList<int> sourceRows = SelectionHelpers::getSourceRowsDescending(this->selectionModel(), proxyModel());
    for (int sourceRow : sourceRows) {
        removeScrip(sourceRow);
    }
}

void MarketWatchWindow::copyToClipboard()
{
    QModelIndexList selected = this->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;
    
    QString text;
    for (const QModelIndex &proxyIndex : selected) {
        int sourceRow = mapToSource(proxyIndex.row());
        if (sourceRow < 0) continue;
        const ScripData &scrip = m_model->getScripAt(sourceRow);
        text += MarketWatchHelpers::formatScripToTSV(scrip) + "\n";
    }
    QApplication::clipboard()->setText(text);
}

void MarketWatchWindow::cutToClipboard()
{
    copyToClipboard();
    deleteSelectedRows();
}

void MarketWatchWindow::pasteFromClipboard()
{
    QString text = QApplication::clipboard()->text();
    if (text.isEmpty() || !ClipboardHelpers::isValidTSV(text, 3)) return;
    
    int insertPosition = m_model->rowCount();
    QModelIndex currentProxyIndex = this->currentIndex();
    if (currentProxyIndex.isValid()) {
        int sourceRow = mapToSource(currentProxyIndex.row());
        if (sourceRow >= 0) insertPosition = sourceRow + 1;
    }
    
    QList<QStringList> rows = ClipboardHelpers::parseTSV(text);
    int currentInsertPos = insertPosition;
    
    for (const QStringList &fields : rows) {
        QString line = fields.join('\t');
        ScripData scrip;
        if (MarketWatchHelpers::parseScripFromTSV(line, scrip) && MarketWatchHelpers::isValidScrip(scrip) && !hasDuplicate(scrip.token)) {
            m_model->insertScrip(currentInsertPos, scrip);
            TokenSubscriptionManager::instance()->subscribe(scrip.exchange, scrip.token);
            m_tokenAddressBook->addToken(scrip.token, currentInsertPos);
            emit scripAdded(scrip.symbol, scrip.exchange, scrip.token);
            currentInsertPos++;
        }
    }
}

void MarketWatchWindow::onBuyAction()
{
    QModelIndex proxyIndex = this->currentIndex();
    if (!proxyIndex.isValid()) return;
    int sourceRow = mapToSource(proxyIndex.row());
    if (sourceRow < 0) return;
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    if (scrip.isValid()) emit buyRequested(scrip.symbol, scrip.token);
}

void MarketWatchWindow::onSellAction()
{
    QModelIndex proxyIndex = this->currentIndex();
    if (!proxyIndex.isValid()) return;
    int sourceRow = mapToSource(proxyIndex.row());
    if (sourceRow < 0) return;
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    if (scrip.isValid()) emit sellRequested(scrip.symbol, scrip.token);
}

void MarketWatchWindow::onAddScripAction()
{
    QMessageBox::information(this, "Add Scrip", "Use ScripBar (Ctrl+S) to search and add scrips.");
}

void MarketWatchWindow::performRowMoveByTokens(const QList<int> &tokens, int targetSourceRow)
{
    if (tokens.isEmpty()) return;
    this->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    
    QList<int> sourceRows;
    QList<ScripData> scrips;
    for (int token : tokens) {
        int row = m_model->findScripByToken(token);
        if (row >= 0) {
            sourceRows.append(row);
            scrips.append(m_model->getScripAt(row));
        }
    }
    if (sourceRows.isEmpty()) return;
    
    if (sourceRows.size() == 1) {
        int sourceRow = sourceRows.first();
        const ScripData &scrip = scrips.first();
        if (sourceRow == targetSourceRow || sourceRow == targetSourceRow - 1) return;
        m_tokenAddressBook->onRowsRemoved(sourceRow, 1);
        m_model->moveRow(sourceRow, targetSourceRow);
        int finalPos = (sourceRow < targetSourceRow) ? targetSourceRow - 1 : targetSourceRow;
        m_tokenAddressBook->onRowsInserted(finalPos, 1);
        m_tokenAddressBook->addToken(scrip.token, finalPos);
        
        int proxyPos = mapToProxy(finalPos);
        if (proxyPos >= 0) {
            QModelIndex pidx = proxyModel()->index(proxyPos, 0);
            selectionModel()->select(pidx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            setCurrentIndex(pidx);
        }
    } else {
        QList<int> sortedRows = sourceRows;
        std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());
        int adjustment = 0;
        for (int sourceRow : sortedRows) {
            if (sourceRow < targetSourceRow) adjustment++;
            removeScrip(sourceRow);
        }
        int adjustedTarget = std::max(0, std::min(m_model->rowCount(), targetSourceRow - adjustment));
        for (int i = 0; i < scrips.size(); ++i) {
            m_model->insertScrip(adjustedTarget + i, scrips[i]);
            m_tokenAddressBook->addToken(scrips[i].token, adjustedTarget + i);
        }
    }
}
