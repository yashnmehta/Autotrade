// ============================================================================
// Actions.cpp — ATMWatch user-initiated actions (exchange/expiry change,
//               header clicks, symbol double-click, option chain launch)
// ============================================================================
#include "views/ATMWatchWindow.h"
#include "views/OptionChainWindow.h"
#include <QDebug>
#include <QHeaderView>

void ATMWatchWindow::onExchangeChanged(int index) {
  m_currentExchange = m_exchangeCombo->currentText();
  qDebug() << "[ATMWatch] Exchange changed to:" << m_currentExchange;

  // Repopulate expiries for new exchange
  populateCommonExpiries(m_currentExchange);

  // Reload all symbols
  loadAllSymbols();
}

void ATMWatchWindow::onExpiryChanged(int index) {
  m_currentExpiry = m_expiryCombo->currentData().toString();
  qDebug() << "[ATMWatch] Expiry changed to:" << m_currentExpiry;

  // Reload all symbols with new expiry
  loadAllSymbols();
}

void ATMWatchWindow::onSymbolDoubleClicked(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }

  int row = index.row();
  QString symbol =
      m_symbolModel->data(m_symbolModel->index(row, SYM_NAME)).toString();
  QString expiry =
      m_symbolModel->data(m_symbolModel->index(row, SYM_EXPIRY)).toString();

  openOptionChain(symbol, expiry);
}

void ATMWatchWindow::openOptionChain(const QString &symbol,
                                     const QString &expiry) {
  qInfo() << "[ATMWatch] Opening Option Chain for" << symbol
          << "expiry:" << expiry;

  // Emit signal so MainWindow opens it as a proper CustomMDISubWindow
  emit openOptionChainRequested(symbol, expiry);

  m_statusLabel->setText(QString("Opened Option Chain for %1").arg(symbol));
}

void ATMWatchWindow::onHeaderClicked(int logicalIndex) {
  // Clear sort indicators on other tables
  m_callTable->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
  m_putTable->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);

  if (m_sortSource == SORT_SYMBOL_TABLE && logicalIndex == m_sortColumn) {
    m_sortOrder = (m_sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder
                                                      : Qt::AscendingOrder;
  } else {
    m_sortSource = SORT_SYMBOL_TABLE;
    m_sortColumn = logicalIndex;
    m_sortOrder = Qt::AscendingOrder;
  }

  m_symbolTable->horizontalHeader()->setSortIndicator(m_sortColumn,
                                                      m_sortOrder);
  refreshData();
}

void ATMWatchWindow::onCallHeaderClicked(int logicalIndex) {
  // Clear sort indicators on other tables
  m_symbolTable->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
  m_putTable->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);

  if (m_sortSource == SORT_CALL_TABLE && logicalIndex == m_sortColumn) {
    m_sortOrder = (m_sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder
                                                      : Qt::AscendingOrder;
  } else {
    m_sortSource = SORT_CALL_TABLE;
    m_sortColumn = logicalIndex;
    m_sortOrder = Qt::AscendingOrder;
  }

  m_callTable->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);
  refreshData();
}

void ATMWatchWindow::onPutHeaderClicked(int logicalIndex) {
  // Clear sort indicators on other tables
  m_symbolTable->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
  m_callTable->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);

  if (m_sortSource == SORT_PUT_TABLE && logicalIndex == m_sortColumn) {
    m_sortOrder = (m_sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder
                                                      : Qt::AscendingOrder;
  } else {
    m_sortSource = SORT_PUT_TABLE;
    m_sortColumn = logicalIndex;
    m_sortOrder = Qt::AscendingOrder;
  }

  m_putTable->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);
  refreshData();
}
