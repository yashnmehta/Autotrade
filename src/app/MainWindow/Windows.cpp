/**
 * @file Windows.cpp
 * @brief Thin delegators from MainWindow slots to WindowFactory
 *
 * After Phase 3.2 refactor, all window creation logic lives in
 * WindowFactory.  These slot bodies simply forward to it so that
 * menu actions, shortcuts, and Q_INVOKABLE calls continue to work.
 */

#include "app/MainWindow.h"
#include "app/WindowFactory.h"
#include "app/WorkspaceManager.h"

// ── Window creation delegators ─────────────────────────────────────────

void MainWindow::createMarketWatch() {
  m_windowFactory->createMarketWatch();
}

void MainWindow::createBuyWindow() {
  m_windowFactory->createBuyWindow();
}

void MainWindow::createSellWindow() {
  m_windowFactory->createSellWindow();
}

void MainWindow::createSnapQuoteWindow() {
  m_windowFactory->createSnapQuoteWindow();
}

void MainWindow::createOptionChainWindow() {
  m_windowFactory->createOptionChainWindow();
}

void MainWindow::createATMWatchWindow() {
  m_windowFactory->createATMWatchWindow();
}

void MainWindow::createTradeBookWindow() {
  m_windowFactory->createTradeBookWindow();
}

void MainWindow::createOrderBookWindow() {
  m_windowFactory->createOrderBookWindow();
}

void MainWindow::createPositionWindow() {
  m_windowFactory->createPositionWindow();
}

void MainWindow::createStrategyManagerWindow() {
  m_windowFactory->createStrategyManagerWindow();
}

void MainWindow::createGlobalSearchWindow() {
  m_windowFactory->createGlobalSearchWindow();
}

void MainWindow::createChartWindow() {
  m_windowFactory->createChartWindow();
}

void MainWindow::createIndicatorChartWindow() {
  m_windowFactory->createIndicatorChartWindow();
}

void MainWindow::createMarketMovementWindow() {
  m_windowFactory->createMarketMovementWindow();
}

void MainWindow::createOptionCalculatorWindow() {
  m_windowFactory->createOptionCalculatorWindow();
}

void MainWindow::onAddToWatchRequested(const InstrumentData &instrument) {
  m_windowFactory->onAddToWatchRequested(instrument);
}

// ── Widget-aware creation (Q_INVOKABLE) ────────────────────────────────

void MainWindow::createBuyWindowFromWidget(QWidget *initiatingWidget) {
  m_windowFactory->createBuyWindowFromWidget(initiatingWidget);
}

void MainWindow::createSellWindowFromWidget(QWidget *initiatingWidget) {
  m_windowFactory->createSellWindowFromWidget(initiatingWidget);
}

// ── Workspace management delegators ────────────────────────────────────

void MainWindow::saveCurrentWorkspace() {
  m_workspaceManager->saveCurrentWorkspace();
}

void MainWindow::loadWorkspace() {
  m_workspaceManager->loadWorkspace();
}

bool MainWindow::loadWorkspaceByName(const QString &name) {
  return m_workspaceManager->loadWorkspaceByName(name);
}

void MainWindow::manageWorkspaces() {
  m_workspaceManager->manageWorkspaces();
}
