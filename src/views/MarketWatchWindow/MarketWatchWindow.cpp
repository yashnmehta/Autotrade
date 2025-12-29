#include "views/MarketWatchWindow.h"
#include "models/TokenAddressBook.h"
#include "services/TokenSubscriptionManager.h"
#include "services/FeedHandler.h"
#include "utils/WindowSettingsHelper.h"
#include "repository/RepositoryManager.h"
#include <QDebug>
#include <QKeyEvent>

MarketWatchWindow::MarketWatchWindow(QWidget *parent)
    : CustomMarketWatch(parent)
    , m_model(nullptr)
    , m_tokenAddressBook(nullptr)
{
    setupUI();
    setupConnections();
    setupKeyboardShortcuts();

    // Load and apply previous runtime state and customizations
    WindowSettingsHelper::loadAndApplyWindowSettings(this, "MarketWatch");
}

MarketWatchWindow::~MarketWatchWindow()
{
    FeedHandler::instance().unsubscribeAll(this);
    m_feedSubscriptions.clear();
    
    if (m_model) {
        for (int row = 0; row < m_model->rowCount(); ++row) {
            const ScripData &scrip = m_model->getScripAt(row);
            if (scrip.isValid()) {
                TokenSubscriptionManager::instance()->unsubscribe(scrip.exchange, scrip.token);
            }
        }
    }
}

void MarketWatchWindow::removeScripByToken(int token)
{
    int row = findTokenRow(token);
    if (row >= 0) removeScrip(row);
}

void MarketWatchWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        deleteSelectedRows();
        event->accept();
        return;
    }
    CustomMarketWatch::keyPressEvent(event);
}

int MarketWatchWindow::getTokenForRow(int sourceRow) const
{
    if (sourceRow < 0 || sourceRow >= m_model->rowCount()) return -1;
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    return scrip.isValid() ? scrip.token : -1;
}

bool MarketWatchWindow::isBlankRow(int sourceRow) const
{
    if (sourceRow < 0 || sourceRow >= m_model->rowCount()) return false;
    return m_model->isBlankRow(sourceRow);
}

WindowContext MarketWatchWindow::getSelectedContractContext() const
{
    WindowContext context;
    context.sourceWindow = "MarketWatch";
    
    QModelIndexList selection = selectionModel()->selectedRows();
    if (selection.isEmpty()) return context;
    
    QModelIndex lastIndex = selection.last();
    int sourceRow = mapToSource(lastIndex.row());
    if (sourceRow < 0 || sourceRow >= m_model->rowCount()) return context;
    
    context.sourceRow = sourceRow;
    const ScripData &scrip = m_model->getScripAt(sourceRow);
    if (!scrip.isValid() || scrip.isBlankRow) return context;
    
    context.exchange = scrip.exchange;
    context.token = scrip.token;
    context.symbol = scrip.symbol;
    context.displayName = scrip.symbol;
    context.ltp = scrip.ltp;
    context.bid = scrip.bid;
    context.ask = scrip.ask;
    context.high = scrip.high;
    context.low = scrip.low;
    context.close = scrip.close;
    context.volume = scrip.volume;
    
    RepositoryManager *repo = RepositoryManager::getInstance();
    QString segment = (context.exchange == "NSEFO" || context.exchange == "BSEFO") ? "FO" : "CM";
    QString exchangeName = context.exchange.left(3);
    
    const ContractData *contract = repo->getContractByToken(exchangeName, segment, scrip.token);
    if (contract) {
        context.lotSize = contract->lotSize;
        context.tickSize = contract->tickSize;
        context.freezeQty = contract->freezeQty;
        context.expiry = contract->expiryDate;
        context.strikePrice = contract->strikePrice;
        context.optionType = contract->optionType;
        context.instrumentType = contract->series;
        context.segment = (segment == "FO") ? "F" : "E";
    }
    
    return context;
}

bool MarketWatchWindow::hasValidSelection() const
{
    QModelIndexList selection = selectionModel()->selectedRows();
    for (const QModelIndex &index : selection) {
        int sourceRow = mapToSource(index.row());
        if (sourceRow >= 0 && sourceRow < m_model->rowCount()) {
            const ScripData &scrip = m_model->getScripAt(sourceRow);
            if (scrip.isValid() && !scrip.isBlankRow) return true;
        }
    }
    return false;
}

void MarketWatchWindow::onRowUpdated(int row, int firstColumn, int lastColumn) {}
void MarketWatchWindow::onRowsInserted(int firstRow, int lastRow) { viewport()->repaint(); }
void MarketWatchWindow::onRowsRemoved(int firstRow, int lastRow) {}
void MarketWatchWindow::onModelReset() {}

void MarketWatchWindow::closeEvent(QCloseEvent *event) {
    WindowSettingsHelper::saveWindowSettings(this, "MarketWatch");
    QWidget::closeEvent(event);
}
