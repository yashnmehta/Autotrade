#include "app/MainWindow.h"
#include "views/MarketWatchWindow.h"
#include "views/SnapQuoteWindow.h"
#include "views/OrderBookWindow.h"
#include "views/TradeBookWindow.h"
#include "views/PositionWindow.h"
#include "services/UdpBroadcastService.h"
#include "services/TradingDataService.h"
#include "app/ScripBar.h"
#include "core/widgets/InfoBar.h"
#include "models/TokenAddressBook.h"
#include "models/MarketWatchModel.h"
#include <QDebug>
#include <QPushButton>

/**
 * @brief Centralized signal/slot management for the entire application.
 * User requested all connections for all classes to be in a single dedicated file.
 */

// --- MAIN WINDOW ---
void MainWindow::setupConnections() {
    // Connections are now handled in the respective setup functions (setupContent, createInfoBar, etc.)
}

// --- MARKET WATCH ---
void MarketWatchWindow::setupConnections() {
    connect(m_model, &MarketWatchModel::rowsInserted, this, [this](const QModelIndex &, int first, int last) {
        m_tokenAddressBook->onRowsInserted(first, last - first + 1);
    });
    connect(m_model, &MarketWatchModel::rowsRemoved, this, [this](const QModelIndex &, int first, int last) {
        m_tokenAddressBook->onRowsRemoved(first, last - first + 1);
    });
    connect(this, &QTableView::customContextMenuRequested, this, &MarketWatchWindow::showContextMenu);
}

// --- SNAP QUOTE ---
// Note: SnapQuoteWindow::setupConnections is currently in its UI.cpp or SnapQuoteWindow.cpp
// I will keep it there or redefine here if I want to centralize.

// --- ORDER BOOK ---
void OrderBookWindow::setupConnections() {
    if (m_applyFilterBtn) connect(m_applyFilterBtn, &QPushButton::clicked, this, &OrderBookWindow::applyFilters);
    if (m_clearFilterBtn) connect(m_clearFilterBtn, &QPushButton::clicked, this, &OrderBookWindow::clearFilters);
    if (m_exportBtn) connect(m_exportBtn, &QPushButton::clicked, this, &OrderBookWindow::exportToCSV);
}

// --- TRADE BOOK ---
void TradeBookWindow::setupConnections() {
    // Similar to OrderBook
}

// --- POSITION BOOK ---
void PositionWindow::setupConnections() {
    // Similar connections
}
