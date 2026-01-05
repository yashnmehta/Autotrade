#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QDialog>
#include <QString>
#include <functional>

namespace Ui {
class LoginWindow;
}

class LoginWindow : public QDialog
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow();

    // Get credentials
    QString getMarketDataAppKey() const;
    QString getMarketDataSecretKey() const;
    QString getInteractiveAppKey() const;
    QString getInteractiveSecretKey() const;
    QString getLoginID() const;
    QString getClientCode() const;
    bool shouldDownloadMasters() const;

    // Set credentials (for loading from config)
    void setMarketDataAppKey(const QString &key);
    void setMarketDataSecretKey(const QString &key);
    void setInteractiveAppKey(const QString &key);
    void setInteractiveSecretKey(const QString &key);
    void setLoginID(const QString &id);

    // Status messages
    void setMDStatus(const QString &status, bool isError = false);
    void setIAStatus(const QString &status, bool isError = false);

    // Button control
    void enableLoginButton();
    void disableLoginButton();
    void showContinueButton();
    void hideContinueButton();

    // Callbacks
    void setOnLoginClicked(std::function<void()> callback);
    void setOnContinueClicked(std::function<void()> callback);

    // Display
    void showCentered();

signals:
    void loginClicked();
    void continueClicked();
    void exitClicked();

private slots:
    void onLoginButtonClicked();
    void onContinueButtonClicked();
    void onExitButtonClicked();

private:
    Ui::LoginWindow *ui;
    std::function<void()> m_onLoginCallback;
    std::function<void()> m_onContinueCallback;
};

#endif // LOGINWINDOW_H
