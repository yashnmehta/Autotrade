#include "views/MarketWatchWindow.h"
#include "models/TokenAddressBook.h"
#include "services/TokenSubscriptionManager.h"
#include "services/PriceCache.h"
#include "utils/ClipboardHelpers.h"
#include "utils/SelectionHelpers.h"
#include "views/helpers/MarketWatchHelpers.h"
#include "repository/RepositoryManager.h"
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <algorithm>

bool MarketWatchWindow::addScrip(const QString &symbol, const QString &exchange, int token)
{
    if (token <= 0) return false;
    if (hasDuplicate(token)) return false;
    
    ScripData scrip;
    
    qDebug() << "[MarketWatch] addScrip requested - Symbol:" << symbol << "Exchange:" << exchange << "Token:" << token;
    
    // Try to get full contract details from repository to populate missing fields
    const ContractData* contract = RepositoryManager::getInstance()->getContractByToken(exchange, token);
    if (contract) {
        qDebug() << "[MarketWatch] contract found in repository:" << contract->displayName 
                 << "Series:" << contract->series << "Strike:" << contract->strikePrice;
        
        // Prefer 'name' (Symbol) over 'displayName' (Description) for the symbol field
        scrip.symbol = contract->name;
        if (scrip.symbol.isEmpty()) scrip.symbol = contract->displayName;
        
        scrip.exchange = exchange;
        scrip.token = token;
        scrip.instrumentType = contract->series;
        scrip.strikePrice = contract->strikePrice;
        scrip.optionType = contract->optionType;
        scrip.seriesExpiry = contract->expiryDate;
        scrip.close = contract->prevClose; // Essential for change calculation if not in tick
        scrip.code = token;
    } else {
        qDebug() << "[MarketWatch] ⚠ contract NOT found in repository for" << exchange << token;
        scrip.symbol = symbol;
        scrip.exchange = exchange;
        scrip.token = token;
        scrip.code = token;
    }
    
    scrip.isBlankRow = false;
    
    int newRow = m_model->rowCount();
    m_model->addScrip(scrip);
    
    TokenSubscriptionManager::instance()->subscribe(exchange, token);
    
    // Subscribe to UDP ticks (new) - Convert exchange string to segment
    UDP::ExchangeSegment segment = UDP::ExchangeSegment::NSECM;  // default
    if (exchange == "NSEFO") segment = UDP::ExchangeSegment::NSEFO;
    else if (exchange == "NSECM") segment = UDP::ExchangeSegment::NSECM;
    else if (exchange == "BSEFO") segment = UDP::ExchangeSegment::BSEFO;
    else if (exchange == "BSECM") segment = UDP::ExchangeSegment::BSECM;
    FeedHandler::instance().subscribeUDP(segment, token, this, &MarketWatchWindow::onUdpTickUpdate);
    
    // Also subscribe to legacy XTS::Tick for backward compatibility
    FeedHandler::instance().subscribe(token, this, &MarketWatchWindow::onTickUpdate);
    
    // Initial Load from Cache
    if (auto cached = PriceCache::instance().getPrice(static_cast<int>(segment), token)) {
        onTickUpdate(*cached);
    }
    
    m_tokenAddressBook->addToken(token, newRow);
    
    emit scripAdded(scrip.symbol, exchange, token);
    return true;
}

bool MarketWatchWindow::addScripFromContract(const ScripData &contractData)
{
    if (contractData.token <= 0) return false;
    if (hasDuplicate(contractData.token)) return false;
    
    ScripData scrip = contractData;
    
    // Enrich from repository if crucial metadata is missing
    if (scrip.instrumentType.isEmpty() || (scrip.strikePrice == 0 && scrip.optionType.isEmpty())) {
        const ContractData* contract = RepositoryManager::getInstance()->getContractByToken(scrip.exchange, scrip.token);
        if (contract) {
            if (scrip.instrumentType.isEmpty()) scrip.instrumentType = contract->series;
            if (scrip.strikePrice == 0) scrip.strikePrice = contract->strikePrice;
            if (scrip.optionType.isEmpty()) scrip.optionType = contract->optionType;
            if (scrip.seriesExpiry.isEmpty()) scrip.seriesExpiry = contract->expiryDate;
            if (scrip.close <= 0) scrip.close = contract->prevClose;
        }
    }
    
    int newRow = m_model->rowCount();
    m_model->addScrip(scrip);
    
    TokenSubscriptionManager::instance()->subscribe(scrip.exchange, scrip.token);
    
    // Subscribe to UDP ticks (new) - Convert exchange string to segment
    UDP::ExchangeSegment segment = UDP::ExchangeSegment::NSECM;  // default
    if (scrip.exchange == "NSEFO") segment = UDP::ExchangeSegment::NSEFO;
    else if (scrip.exchange == "NSECM") segment = UDP::ExchangeSegment::NSECM;
    else if (scrip.exchange == "BSEFO") segment = UDP::ExchangeSegment::BSEFO;
    else if (scrip.exchange == "BSECM") segment = UDP::ExchangeSegment::BSECM;
    FeedHandler::instance().subscribeUDP(segment, scrip.token, this, &MarketWatchWindow::onUdpTickUpdate);
    
    // Also subscribe to legacy XTS::Tick for backward compatibility
    FeedHandler::instance().subscribe(scrip.token, this, &MarketWatchWindow::onTickUpdate);
    
    // Initial Load from Cache
    if (auto cached = PriceCache::instance().getPrice(static_cast<int>(segment), scrip.token)) {
        onTickUpdate(*cached);
    }
    
    m_tokenAddressBook->addToken(scrip.token, newRow);
    
    emit scripAdded(scrip.symbol, scrip.exchange, scrip.token);
    return true;
}

void MarketWatchWindow::removeScrip(int row)
{
    if (row < 0 || row >= m_model->rowCount()) return;
    const ScripData &scrip = m_model->getScripAt(row);
    if (!scrip.isBlankRow && scrip.isValid()) {
        TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange, scrip.token);
        // Note: Individual token unsubscription for a specific receiver 
        // will be handled by FeedHandler::unsubscribe(token, this)
        FeedHandler::instance().unsubscribe(scrip.token, this);
        m_tokenAddressBook->removeToken(scrip.token, row);
        emit scripRemoved(scrip.token);
    }
    m_model->removeScrip(row);
}

void MarketWatchWindow::clearAll()
{
    FeedHandler::instance().unsubscribeAll(this);
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
            
            // Enrich from repository
            const ContractData* contract = RepositoryManager::getInstance()->getContractByToken(scrip.exchange, scrip.token);
            if (contract) {
                scrip.instrumentType = contract->series;
                scrip.strikePrice = contract->strikePrice;
                scrip.optionType = contract->optionType;
                scrip.seriesExpiry = contract->expiryDate;
                if (scrip.close <= 0) scrip.close = contract->prevClose;
            }

            m_model->insertScrip(currentInsertPos, scrip);
            TokenSubscriptionManager::instance()->subscribe(scrip.exchange, scrip.token);
            
            // Subscribe to UDP ticks (new) - Convert exchange string to segment
            UDP::ExchangeSegment segment = UDP::ExchangeSegment::NSECM;  // default
            if (scrip.exchange == "NSEFO") segment = UDP::ExchangeSegment::NSEFO;
            else if (scrip.exchange == "NSECM") segment = UDP::ExchangeSegment::NSECM;
            else if (scrip.exchange == "BSEFO") segment = UDP::ExchangeSegment::BSEFO;
            else if (scrip.exchange == "BSECM") segment = UDP::ExchangeSegment::BSECM;
            FeedHandler::instance().subscribeUDP(segment, scrip.token, this, &MarketWatchWindow::onUdpTickUpdate);
            
            // Also subscribe to legacy XTS::Tick for backward compatibility
            FeedHandler::instance().subscribe(scrip.token, this, &MarketWatchWindow::onTickUpdate);
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

void MarketWatchWindow::onSavePortfolio()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Portfolio"), "", tr("Portfolio Files (*.json);;All Files (*)"));
    if (fileName.isEmpty()) return;

    QList<ScripData> scrips;
    int rows = m_model->rowCount();
    for (int i = 0; i < rows; ++i) {
        scrips.append(m_model->getScripAt(i));
    }

    if (MarketWatchHelpers::savePortfolio(fileName, scrips)) {
        QMessageBox::information(this, tr("Success"), tr("Portfolio saved successfully."));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save portfolio."));
    }
}

void MarketWatchWindow::onLoadPortfolio()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Load Portfolio"), "", tr("Portfolio Files (*.json);;All Files (*)"));
    if (fileName.isEmpty()) return;

    QList<ScripData> scrips;
    if (!MarketWatchHelpers::loadPortfolio(fileName, scrips)) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load portfolio."));
        return;
    }

    // Confirm before clearing
    if (m_model->rowCount() > 0) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Confirm Load"), 
                                     tr("Loading a portfolio will clear current list. Continue?"),
                                     QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::No) return;
    }

    clearAll();

    for (const auto &scrip : scrips) {
        if (scrip.isBlankRow) {
             insertBlankRow(m_model->rowCount());
        } else {
             addScripFromContract(scrip);
        }
    }
    
    QMessageBox::information(this, tr("Success"), tr("Portfolio loaded successfully."));
}
