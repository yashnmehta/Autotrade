#include "views/SnapQuoteWindow.h"
#include <QUiLoader>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QShortcut>
#include <QTimer>
#include <QFile>
#include <QDebug>

void SnapQuoteWindow::initUI()
{
    QUiLoader loader;
    QFile file(":/forms/SnapQuote.ui");
    if (!file.open(QFile::ReadOnly)) return;
    m_formWidget = loader.load(&file);
    file.close();
    if (!m_formWidget) return;

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_formWidget);

    m_cbEx = m_formWidget->findChild<QComboBox*>("cbEx");
    m_cbSegment = m_formWidget->findChild<QComboBox*>("cbSegment");
    m_leToken = m_formWidget->findChild<QLineEdit*>("leToken");
    m_leInstType = m_formWidget->findChild<QLineEdit*>("leInstType");
    m_leSymbol = m_formWidget->findChild<QLineEdit*>("leSymbol");
    m_cbExpiry = m_formWidget->findChild<QComboBox*>("cbExpiry");
    m_pbRefresh = m_formWidget->findChild<QPushButton*>("pbRefresh");

    m_lbLTPQty = m_formWidget->findChild<QLabel*>("lbLTPQty");
    m_lbLTPPrice = m_formWidget->findChild<QLabel*>("lbLTPPrice");
    m_lbLTPIndicator = m_formWidget->findChild<QLabel*>("lbLTPIndicator");
    m_lbLTPTime = m_formWidget->findChild<QLabel*>("lbLTPTime");
    
    m_lbOpen = m_formWidget->findChild<QLabel*>("lbOpen");
    m_lbHigh = m_formWidget->findChild<QLabel*>("lbHigh");
    m_lbLow = m_formWidget->findChild<QLabel*>("lbLow");
    m_lbClose = m_formWidget->findChild<QLabel*>("lbClose");
    
    m_lbVolume = m_formWidget->findChild<QLabel*>("lbVolume");
    m_lbATP = m_formWidget->findChild<QLabel*>("lbATP");
    m_lbPercentChange = m_formWidget->findChild<QLabel*>("lbPercentChange");
    
    // Depth labels
    m_lbBidPrice1 = m_formWidget->findChild<QLabel*>("lbBidPrice1");
    m_lbAskPrice1 = m_formWidget->findChild<QLabel*>("lbAskPrice1");
    m_lbBidQty1 = m_formWidget->findChild<QLabel*>("lbBidQty1");
    m_lbAskQty1 = m_formWidget->findChild<QLabel*>("lbAskQty1");

    populateComboBoxes();
    setupConnections();
    setupKeyboardShortcuts();
}

void SnapQuoteWindow::populateComboBoxes()
{
    if (m_cbEx) m_cbEx->addItems({"NSE", "BSE", "MCX"});
    if (m_cbSegment) m_cbSegment->addItems({"F", "C", "D"});
}

void SnapQuoteWindow::setupConnections() {}
void SnapQuoteWindow::setupKeyboardShortcuts() {}
