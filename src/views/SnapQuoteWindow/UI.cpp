#include "views/SnapQuoteWindow.h"
#include "app/SnapQuoteScripBar.h"
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QUiLoader>
#include <QVBoxLayout>


void SnapQuoteWindow::initUI() {
  QUiLoader loader;
  QFile file(":/forms/SnapQuote.ui");
  if (!file.open(QFile::ReadOnly))
    return;
  m_formWidget = loader.load(&file);
  file.close();
  if (!m_formWidget)
    return;

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_formWidget);

  // Setup Header with ScripBar
  QWidget *headerWidget = m_formWidget->findChild<QWidget *>("headerWidget");
  if (headerWidget) {
    if (!headerWidget->layout()) {
      new QHBoxLayout(headerWidget);
    }

    m_scripBar = new SnapQuoteScripBar(this);
    m_scripBar->setXTSClient(m_xtsClient);

    // Add ScripBar to header layout
    // We add it at index 0
    QLayout *headerLayout = headerWidget->layout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    // Remove existing items from UI if any left (though we cleared them in UI
    // file)

    headerLayout->addWidget(m_scripBar);
    // pbRefresh is still in UI file or we can re-find it?
    // In UI file we kept pbRefresh.
    m_pbRefresh = m_formWidget->findChild<QPushButton *>("pbRefresh");
    if (m_pbRefresh) {
      headerLayout->addWidget(m_pbRefresh);
    }
  }

  // LTP section
  m_lbLTPQty = m_formWidget->findChild<QLabel *>("lbLTPQty");
  m_lbLTPPrice = m_formWidget->findChild<QLabel *>("lbLTPPrice");
  m_lbLTPIndicator = m_formWidget->findChild<QLabel *>("lbLTPIndicator");
  m_lbLTPTime = m_formWidget->findChild<QLabel *>("lbLTPTime");

  // OHLC and stats
  m_lbOpen = m_formWidget->findChild<QLabel *>("lbOpen");
  m_lbHigh = m_formWidget->findChild<QLabel *>("lbHigh");
  m_lbLow = m_formWidget->findChild<QLabel *>("lbLow");
  m_lbClose = m_formWidget->findChild<QLabel *>("lbClose");
  m_lbVolume = m_formWidget->findChild<QLabel *>("lbVolume");
  m_lbATP = m_formWidget->findChild<QLabel *>("lbATP");
  m_lbPercentChange = m_formWidget->findChild<QLabel *>("lbPercentChange");
  m_lbOI = m_formWidget->findChild<QLabel *>("lbOI");
  m_lbOIPercent = m_formWidget->findChild<QLabel *>("lbOIPercent");
  m_lbDPR = m_formWidget->findChild<QLabel *>("lbDPR");

  // Bid Depth (5 levels)
  m_lbBidQty1 = m_formWidget->findChild<QLabel *>("lbBidQty1");
  m_lbBidQty2 = m_formWidget->findChild<QLabel *>("lbBidQty2");
  m_lbBidQty3 = m_formWidget->findChild<QLabel *>("lbBidQty3");
  m_lbBidQty4 = m_formWidget->findChild<QLabel *>("lbBidQty4");
  m_lbBidQty5 = m_formWidget->findChild<QLabel *>("lbBidQty5");
  m_lbBidPrice1 = m_formWidget->findChild<QLabel *>("lbBidPrice1");
  m_lbBidPrice2 = m_formWidget->findChild<QLabel *>("lbBidPrice2");
  m_lbBidPrice3 = m_formWidget->findChild<QLabel *>("lbBidPrice3");
  m_lbBidPrice4 = m_formWidget->findChild<QLabel *>("lbBidPrice4");
  m_lbBidPrice5 = m_formWidget->findChild<QLabel *>("lbBidPrice5");
  m_lbBidOrders1 = m_formWidget->findChild<QLabel *>("bidAt1");
  m_lbBidOrders2 = m_formWidget->findChild<QLabel *>("bidAt2");
  m_lbBidOrders3 = m_formWidget->findChild<QLabel *>("bidAt3");
  m_lbBidOrders4 = m_formWidget->findChild<QLabel *>("bidAt4");
  m_lbBidOrders5 = m_formWidget->findChild<QLabel *>("bidAt5");

  // Ask Depth (5 levels)
  m_lbAskQty1 = m_formWidget->findChild<QLabel *>("lbAskQty1");
  m_lbAskQty2 = m_formWidget->findChild<QLabel *>("lbAskQty2");
  m_lbAskQty3 = m_formWidget->findChild<QLabel *>("lbAskQty3");
  m_lbAskQty4 = m_formWidget->findChild<QLabel *>("lbAskQty4");
  m_lbAskQty5 = m_formWidget->findChild<QLabel *>("lbAskQty5");
  m_lbAskPrice1 = m_formWidget->findChild<QLabel *>("lbAskPrice1");
  m_lbAskPrice2 = m_formWidget->findChild<QLabel *>("lbAskPrice2");
  m_lbAskPrice3 = m_formWidget->findChild<QLabel *>("lbAskPrice3");
  m_lbAskPrice4 = m_formWidget->findChild<QLabel *>("lbAskPrice4");
  m_lbAskPrice5 = m_formWidget->findChild<QLabel *>("lbAskPrice5");
  m_lbAskOrders1 = m_formWidget->findChild<QLabel *>("lbAskOrders1");
  m_lbAskOrders2 = m_formWidget->findChild<QLabel *>("lbAskOrders2");
  m_lbAskOrders3 = m_formWidget->findChild<QLabel *>("lbAskOrders3");
  m_lbAskOrders4 = m_formWidget->findChild<QLabel *>("lbAskOrders4");
  m_lbAskOrders5 = m_formWidget->findChild<QLabel *>("lbAskOrders5");

  // Total buyers/sellers
  m_lbTotalBuyers = m_formWidget->findChild<QLabel *>("lb_allBuyers");
  m_lbTotalSellers = m_formWidget->findChild<QLabel *>("lb_allSellers");

  populateComboBoxes(); // May be empty now
  setupConnections();
  setupKeyboardShortcuts();
}

void SnapQuoteWindow::populateComboBoxes() {
  // No explicit combo boxes to populate anymore
}

void SnapQuoteWindow::setupConnections() {
  if (m_pbRefresh) {
    connect(m_pbRefresh, &QPushButton::clicked, this,
            &SnapQuoteWindow::onRefreshClicked);
  }
  if (m_scripBar) {
    connect(m_scripBar, &ScripBar::addToWatchRequested, this,
            &SnapQuoteWindow::onScripSelected);
  }
}

void SnapQuoteWindow::setupKeyboardShortcuts() {
  // F5 and Escape are handled by MainWindow and CustomMDISubWindow
  // respectively.
}
