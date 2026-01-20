#include "ui/LoginWindow.h"
#include "repository/RepositoryManager.h"
#include "ui_LoginWindow.h"
#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QScreen>


LoginWindow::LoginWindow(QWidget *parent)
    : QDialog(parent), ui(new Ui::LoginWindow), m_onLoginCallback(nullptr),
      m_onContinueCallback(nullptr) {
  ui->setupUi(this);

  // Set window flags
  setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
  setModal(true);

  // Initially hide continue button
  ui->pbNext->setVisible(false);

  // Check if master files exist
  QString mastersDir = RepositoryManager::getMastersDirectory();
  QString masterFile = mastersDir + "/master_contracts_latest.txt";
  if (!QFile::exists(masterFile)) {
    ui->cbCmaster->setChecked(true);
  }

  // Connect signals
  connect(ui->pbLogin, &QPushButton::clicked, this,
          &LoginWindow::onLoginButtonClicked);
  connect(ui->pbNext, &QPushButton::clicked, this,
          &LoginWindow::onContinueButtonClicked);
  connect(ui->pbExit, &QPushButton::clicked, this,
          &LoginWindow::onExitButtonClicked);
}

LoginWindow::~LoginWindow() { delete ui; }

QString LoginWindow::getMarketDataAppKey() const {
  return ui->lb_md_appKey->text().trimmed();
}

QString LoginWindow::getMarketDataSecretKey() const {
  return ui->lb_md_secretKey->text().trimmed();
}

QString LoginWindow::getInteractiveAppKey() const {
  return ui->lb_ia_appKey->text().trimmed();
}

QString LoginWindow::getInteractiveSecretKey() const {
  return ui->lb_ia_secretKey->text().trimmed();
}

QString LoginWindow::getLoginID() const {
  return ui->lb_loginId->text().trimmed();
}

QString LoginWindow::getClientCode() const {
  return ui->leClient->text().trimmed();
}

bool LoginWindow::shouldDownloadMasters() const {
  return ui->cbCmaster->isChecked();
}

void LoginWindow::setMarketDataAppKey(const QString &key) {
  ui->lb_md_appKey->setText(key);
}

void LoginWindow::setMarketDataSecretKey(const QString &key) {
  ui->lb_md_secretKey->setText(key);
}

void LoginWindow::setInteractiveAppKey(const QString &key) {
  ui->lb_ia_appKey->setText(key);
}

void LoginWindow::setInteractiveSecretKey(const QString &key) {
  ui->lb_ia_secretKey->setText(key);
}

void LoginWindow::setLoginID(const QString &id) { ui->lb_loginId->setText(id); }

void LoginWindow::setMDStatus(const QString &status, bool isError) {
  ui->lbMDStatus->setText(status);
  if (isError) {
    ui->lbMDStatus->setStyleSheet("color: #ff4444; font-weight: bold;");
  } else {
    ui->lbMDStatus->setStyleSheet("color: #44ff44; font-weight: bold;");
  }
}

void LoginWindow::setIAStatus(const QString &status, bool isError) {
  ui->lbIAStatus->setText(status);
  if (isError) {
    ui->lbIAStatus->setStyleSheet("color: #ff4444; font-weight: bold;");
  } else {
    ui->lbIAStatus->setStyleSheet("color: #44ff44; font-weight: bold;");
  }
}

void LoginWindow::enableLoginButton() { ui->pbLogin->setEnabled(true); }

void LoginWindow::disableLoginButton() { ui->pbLogin->setEnabled(false); }

void LoginWindow::showContinueButton() {
  ui->pbNext->setVisible(true);
  ui->pbLogin->setVisible(false);
}

void LoginWindow::hideContinueButton() {
  ui->pbNext->setVisible(false);
  ui->pbLogin->setVisible(true);
}

void LoginWindow::setOnLoginClicked(std::function<void()> callback) {
  m_onLoginCallback = callback;
}

void LoginWindow::setOnContinueClicked(std::function<void()> callback) {
  m_onContinueCallback = callback;
}

void LoginWindow::onLoginButtonClicked() {
  // Validate inputs
  if (getMarketDataAppKey().isEmpty() || getMarketDataSecretKey().isEmpty()) {
    QMessageBox::warning(this, "Validation Error",
                         "Market Data credentials are required");
    return;
  }

  if (getInteractiveAppKey().isEmpty() || getInteractiveSecretKey().isEmpty()) {
    QMessageBox::warning(this, "Validation Error",
                         "Interactive API credentials are required");
    return;
  }

  if (getLoginID().isEmpty()) {
    QMessageBox::warning(this, "Validation Error", "Login ID is required");
    return;
  }

  emit loginClicked();

  if (m_onLoginCallback) {
    m_onLoginCallback();
  }
}

void LoginWindow::onContinueButtonClicked() {
  emit continueClicked();

  if (m_onContinueCallback) {
    m_onContinueCallback();
  }
}

void LoginWindow::onExitButtonClicked() {
  emit exitClicked();
  reject(); // Close dialog with rejected status
}

void LoginWindow::showCentered() {
  show();

  // Center on screen
  QScreen *screen = QApplication::primaryScreen();
  if (screen) {
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);
  }

  raise();
  activateWindow();

  // ✅ Removed QApplication::processEvents() - let Qt event loop handle
  // naturally processEvents() creates nested event loops causing race
  // conditions
}
