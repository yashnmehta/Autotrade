#include "views/MarketWatchWindow.h"
#include "models/TokenAddressBook.h"
#include "services/TokenSubscriptionManager.h"
#include "services/PriceCache.h"
#include "data/PriceStoreGateway.h"
#include "data/UnifiedPriceState.h"
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
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

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
    
    // Initial Load from Distributed Store
    if (m_useZeroCopyPriceCache) {
        if (auto data = MarketData::PriceStoreGateway::instance().getUnifiedState(static_cast<int>(segment), token)) {
            m_tokenUnifiedPointers[token] = data;
            if (data->ltp > 0) {
                // Update via onUdpTickUpdate for consistency (it takes UnifiedState fields soon)
                // For now, let's just trigger a model update if it has data
                UDP::MarketTick udpTick;
                udpTick.exchangeSegment = segment;
                udpTick.token = token;
                udpTick.ltp = data->ltp;
                udpTick.open = data->open;
                udpTick.high = data->high;
                udpTick.low = data->low;
                udpTick.prevClose = data->close;
                udpTick.volume = data->volume;
                udpTick.atp = data->avgPrice;
                
                // Copy best depth
                udpTick.bids[0].price = data->bids[0].price;
                udpTick.bids[0].quantity = data->bids[0].quantity;
                udpTick.asks[0].price = data->asks[0].price;
                udpTick.asks[0].quantity = data->asks[0].quantity;
                
                onUdpTickUpdate(udpTick);
            }
        }
    } else {
        // Use legacy cache for initial load
        if (auto cached = PriceCache::instance().getPrice(static_cast<int>(segment), token)) {
            onTickUpdate(*cached);
        }
    }
    
    m_tokenAddressBook->addCompositeToken(exchange, "", token, newRow);
    m_tokenAddressBook->addIntKeyToken(static_cast<int>(segment), token, newRow);
    
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
    
    // Initial Load from Distributed Store
    if (m_useZeroCopyPriceCache) {
        if (auto data = MarketData::PriceStoreGateway::instance().getUnifiedState(static_cast<int>(segment), scrip.token)) {
            m_tokenUnifiedPointers[scrip.token] = data;
            if (data->ltp > 0) {
                UDP::MarketTick udpTick;
                udpTick.exchangeSegment = segment;
                udpTick.token = scrip.token;
                udpTick.ltp = data->ltp;
                udpTick.open = data->open;
                udpTick.high = data->high;
                udpTick.low = data->low;
                udpTick.prevClose = data->close;
                udpTick.volume = data->volume;
                udpTick.atp = data->avgPrice;
                
                // Copy best depth
                udpTick.bids[0].price = data->bids[0].price;
                udpTick.bids[0].quantity = data->bids[0].quantity;
                udpTick.asks[0].price = data->asks[0].price;
                udpTick.asks[0].quantity = data->asks[0].quantity;
                
                onUdpTickUpdate(udpTick);
            }
        }
    } else {
        // Use legacy cache for initial load
        if (auto cached = PriceCache::instance().getPrice(static_cast<int>(segment), scrip.token)) {
            onTickUpdate(*cached);
        }
    }
    
    m_tokenAddressBook->addCompositeToken(scrip.exchange, "", scrip.token, newRow);
    m_tokenAddressBook->addIntKeyToken(static_cast<int>(segment), scrip.token, newRow);
    
    emit scripAdded(scrip.symbol, scrip.exchange, scrip.token);
    return true;
}

void MarketWatchWindow::removeScrip(int row)
{
    if (row < 0 || row >= m_model->rowCount()) return;
    const ScripData &scrip = m_model->getScripAt(row);
    if (!scrip.isBlankRow && scrip.isValid()) {
        TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange, scrip.token);
        
        // Unsubscribe from UDP ticks
        // Convert exchange string to segment for specific unsubscription
        UDP::ExchangeSegment segment = UDP::ExchangeSegment::NSECM;  // default
        if (scrip.exchange == "NSEFO") segment = UDP::ExchangeSegment::NSEFO;
        else if (scrip.exchange == "NSECM") segment = UDP::ExchangeSegment::NSECM;
        else if (scrip.exchange == "BSEFO") segment = UDP::ExchangeSegment::BSEFO;
        else if (scrip.exchange == "BSECM") segment = UDP::ExchangeSegment::BSECM;
        
        // Note: Individual token unsubscription for a specific receiver 
        // FeedHandler::unsubscribe(token, this) is DEPRECATED as it unsubscribes all segments
        FeedHandler::instance().unsubscribe(static_cast<int>(segment), scrip.token, this);
        
        m_tokenAddressBook->removeCompositeToken(scrip.exchange, "", scrip.token, row);
        m_tokenAddressBook->removeIntKeyToken(static_cast<int>(segment), scrip.token, row);
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

            m_tokenAddressBook->addCompositeToken(scrip.exchange, "", scrip.token, currentInsertPos);
            m_tokenAddressBook->addIntKeyToken(static_cast<int>(segment), scrip.token, currentInsertPos);
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
        m_tokenAddressBook->addCompositeToken(scrip.exchange, "", scrip.token, finalPos);
        
        // Derive segment for int key (similar to addScrip logic)
        // We can optimize this by storing segment in scrip but for now simple mapping is fine for move operations
        int segment = RepositoryManager::getExchangeSegmentID(scrip.exchange.left(3), scrip.exchange.mid(3));
        if (segment > 0) {
             m_tokenAddressBook->addIntKeyToken(segment, scrip.token, finalPos);
        }
        
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
            m_tokenAddressBook->addCompositeToken(scrips[i].exchange, "", scrips[i].token, adjustedTarget + i);
            
            int segment = RepositoryManager::getExchangeSegmentID(scrips[i].exchange.left(3), scrips[i].exchange.mid(3));
            if (segment > 0) {
                 m_tokenAddressBook->addIntKeyToken(segment, scrips[i].token, adjustedTarget + i);
            }
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

    // Capture current visual state (widths & order) into model's profile
    MarketWatchColumnProfile currentProfile = m_model->getColumnProfile();
    captureProfileFromView(currentProfile);
    
    // DON'T update model during save - it triggers reset and corrupts state!
    // The profile is saved to file, model doesn't need updating here.

    if (MarketWatchHelpers::savePortfolio(fileName, scrips, currentProfile)) {
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
    MarketWatchColumnProfile profile;
    
    // Attempt to load scrips AND profile
    if (!MarketWatchHelpers::loadPortfolio(fileName, scrips, profile)) {
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
    
    // Apply the loaded profile keys to view
    if (!profile.name().isEmpty()) {
        m_model->setColumnProfile(profile);
        applyProfileToView(profile);
        qDebug() << "[MarketWatchWindow] Applied loaded profile:" << profile.name();
    }

    QMessageBox::information(this, tr("Success"), tr("Portfolio loaded successfully."));
}

void MarketWatchWindow::exportPriceCacheDebug()
{
    QMessageBox::information(this, "Debug", "This feature is currently being refactored for the new Distributed Price Store architecture.");
}

void MarketWatchWindow::saveState(QSettings &settings)
{
    // Save Scrips
    QJsonArray scripsArray;
    int rows = m_model->rowCount();
    for (int i = 0; i < rows; ++i) {
        // Use helper to serialize scrip
        scripsArray.append(MarketWatchHelpers::scripToJson(m_model->getScripAt(i)));
    }
    settings.setValue("scrips", QJsonDocument(scripsArray).toJson(QJsonDocument::Compact));

    // Save Column Profile & Geometry (delegated to helper which saves to specific keys)
    // However, WindowSettingsHelper saves to a *global* location usually based on window type.
    // For workspace-specific saving, we should save the profile into THIS settings object.
    
    // Serialize current column profile
    settings.setValue("columnProfile", QJsonDocument(m_model->getColumnProfile().toJson()).toJson(QJsonDocument::Compact));
}

void MarketWatchWindow::restoreState(QSettings &settings)
{
    // Restore Scrips
    if (settings.contains("scrips")) {
        QByteArray data = settings.value("scrips").toByteArray();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            QJsonArray scripsArray = doc.array();
            for (const QJsonValue &val : scripsArray) {
                ScripData scrip = MarketWatchHelpers::scripFromJson(val.toObject());
                if (scrip.isBlankRow) {
                    insertBlankRow(m_model->rowCount());
                } else {
                    addScripFromContract(scrip);
                }
            }
        }
    }

    // Restore Column Profile
    if (settings.contains("columnProfile")) {
        QByteArray data = settings.value("columnProfile").toByteArray();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            MarketWatchColumnProfile profile;
            if (profile.fromJson(doc.object())) {
                m_model->setColumnProfile(profile);
                applyProfileToView(profile);  // Apply to view (widths + order)
                qDebug() << "[MarketWatchWindow] Restored column profile:" << profile.name();
            } else {
                qWarning() << "[MarketWatchWindow] Failed to parse column profile from settings";
            }
        }
    }
}


void MarketWatchWindow::captureProfileFromView(MarketWatchColumnProfile &profile)
{
    QHeaderView *header = this->horizontalHeader();
    QList<MarketWatchColumn> modelColumns = m_model->getColumnProfile().visibleColumns();
    
    // Build a map of logical index -> MarketWatchColumn for currently visible columns
    QMap<int, MarketWatchColumn> logicalToColumn;
    for (int i = 0; i < modelColumns.size(); ++i) {
        logicalToColumn[i] = modelColumns[i];
    }
    
    QList<MarketWatchColumn> newVisibleOrder;
    QList<MarketWatchColumn> capturedVisible;
    
    // 1. Capture ACTUAL visual order from QHeaderView
    // Iterate through visual indices (user's displayed order after drag-and-drop)
    for (int visualIdx = 0; visualIdx < header->count(); ++visualIdx) {
        int logicalIdx = header->logicalIndex(visualIdx);  // Get logical index at this visual position
        
        if (logicalToColumn.contains(logicalIdx)) {
            MarketWatchColumn col = logicalToColumn[logicalIdx];
            
            // Capture width from the logical section
            int width = header->sectionSize(logicalIdx);
            if (width > 0) {
                profile.setColumnWidth(col, width);
            }
            
            // Column is visible if not hidden
            bool isHidden = header->isSectionHidden(logicalIdx);
            profile.setColumnVisible(col, !isHidden);
            
            if (!isHidden) {
                newVisibleOrder.append(col);
                capturedVisible.append(col);
            }
        }
    }
    
    // 2. Build complete column order: visible columns in visual order, then hidden columns
    QList<MarketWatchColumn> completeOrder = newVisibleOrder;
    
    // Add hidden columns at the end, maintaining their original relative order
    QList<MarketWatchColumn> existingOrder = profile.columnOrder();
    for (MarketWatchColumn col : existingOrder) {
        if (!capturedVisible.contains(col)) {
            completeOrder.append(col);
        }
    }
    
    profile.setColumnOrder(completeOrder);
    
    qDebug() << "[captureProfileFromView] Captured" << newVisibleOrder.size() 
             << "visible columns in visual order, total order size:" << completeOrder.size();
}

void MarketWatchWindow::applyProfileToView(const MarketWatchColumnProfile &profile)
{
    QHeaderView *header = this->horizontalHeader();
    QList<MarketWatchColumn> visibleCols = profile.visibleColumns();
    
    // The model provides columns in logical order (0..N matching profile.visibleColumns())
    // But the VIEW may have different visual order due to user drag-and-drop.
    // We need to reorder the view to match the profile's intended order.
    
    // 1. First, reset view order to match model logical order (in case it was reordered)
    // Build map of where each column currently is
    QMap<MarketWatchColumn, int> columnToLogicalIndex;
    for (int i = 0; i < visibleCols.size(); ++i) {
        columnToLogicalIndex[visibleCols[i]] = i;
    }
    
    // 2. Move sections to match profile's intended visual order
    for (int targetVisualIdx = 0; targetVisualIdx < visibleCols.size(); ++targetVisualIdx) {
        if (targetVisualIdx >= header->count()) break;
        
        MarketWatchColumn col = visibleCols[targetVisualIdx];
        int logicalIdx = columnToLogicalIndex.value(col, -1);
        
        if (logicalIdx >= 0) {
            // Move this logical section to the target visual position
            int currentVisualIdx = header->visualIndex(logicalIdx);
            if (currentVisualIdx != targetVisualIdx) {
                header->moveSection(currentVisualIdx, targetVisualIdx);
            }
            
            // Apply width
            int width = profile.columnWidth(col);
            if (width > 0) {
                header->resizeSection(logicalIdx, width);
            }
            
            // Ensure visible
            header->setSectionHidden(logicalIdx, false);
        }
    }
    
    qDebug() << "[applyProfileToView] Applied column order and widths for" 
             << visibleCols.size() << "columns";
}
