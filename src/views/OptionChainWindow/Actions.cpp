// ============================================================================
// Actions.cpp — OptionChain user-initiated actions (symbol/expiry change,
//               table clicks, trade/calculator requests, event filter)
// ============================================================================
#include "views/OptionChainWindow.h"
#include <QDebug>
#include <QScrollBar>
#include <QWheelEvent>

void OptionChainWindow::onSymbolChanged(const QString &symbol) {
  if (symbol == m_currentSymbol)
    return;

  m_currentSymbol = symbol;
  m_titleLabel->setText(symbol);

  // Update expiries for the new symbol
  populateExpiries(symbol);

  emit refreshRequested();
}

void OptionChainWindow::onExpiryChanged(const QString &expiry) {
  if (expiry == m_currentExpiry)
    return;

  m_currentExpiry = expiry;
  emit refreshRequested();
}

void OptionChainWindow::onRefreshClicked() { emit refreshRequested(); }

void OptionChainWindow::onTradeClicked() {
  if (m_selectedCallRow >= 0) {
    double strike = getStrikeAtRow(m_selectedCallRow);
    emit tradeRequested(m_currentSymbol, m_currentExpiry, strike, "CE");
  } else if (m_selectedPutRow >= 0) {
    double strike = getStrikeAtRow(m_selectedPutRow);
    emit tradeRequested(m_currentSymbol, m_currentExpiry, strike, "PE");
  }
}

void OptionChainWindow::onCalculatorClicked() {
  emit calculatorRequested(m_currentSymbol, m_currentExpiry, 0.0, "");
}

void OptionChainWindow::onCallTableClicked(const QModelIndex &index) {
  int row = index.row();
  int col = index.column();

  // Handle checkbox column (column 0)
  if (col == 0) {
    QStandardItem *item = m_callModel->item(row, 0);
    if (item) {
      bool isChecked = item->checkState() == Qt::Checked;
      item->setCheckState(isChecked ? Qt::Unchecked : Qt::Checked);
    }
    return;
  }

  m_selectedCallRow = row;
  m_callTable->selectRow(row);
  m_putTable->clearSelection();

  qDebug() << "Call selected at strike:" << getStrikeAtRow(row);
}

void OptionChainWindow::onPutTableClicked(const QModelIndex &index) {
  int row = index.row();
  int col = index.column();

  // Handle checkbox column (last column)
  if (col == PUT_COLUMN_COUNT - 1) {
    QStandardItem *item = m_putModel->item(row, PUT_COLUMN_COUNT - 1);
    if (item) {
      bool isChecked = item->checkState() == Qt::Checked;
      item->setCheckState(isChecked ? Qt::Unchecked : Qt::Checked);
    }
    return;
  }

  m_selectedPutRow = row;
  m_putTable->selectRow(row);
  m_callTable->clearSelection();

  qDebug() << "Put selected at strike:" << getStrikeAtRow(row);
}

void OptionChainWindow::onStrikeTableClicked(const QModelIndex &index) {
  int row = index.row();

  m_selectedCallRow = row;
  m_selectedPutRow = row;

  m_callTable->selectRow(row);
  m_putTable->selectRow(row);
  m_strikeTable->selectRow(row);

  qDebug() << "Strike selected:" << getStrikeAtRow(row)
           << "- Both Call and Put selected";
}

bool OptionChainWindow::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::Wheel) {
    if (obj == m_callTable->viewport() || obj == m_putTable->viewport() ||
        obj == m_strikeTable->viewport()) {

      QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);

      int currentValue = m_strikeTable->verticalScrollBar()->value();
      int delta = wheelEvent->angleDelta().y();
      int step = m_strikeTable->verticalScrollBar()->singleStep();

      int newValue = currentValue - (delta > 0 ? step : -step);

      m_strikeTable->verticalScrollBar()->setValue(newValue);

      return true;
    }
  }

  return QWidget::eventFilter(obj, event);
}
