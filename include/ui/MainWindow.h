#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ui/CustomMainWindow.h"

class CustomMDIArea;
class QToolBar;
class QStatusBar;
class QCloseEvent;
class ScripBar;

/**
 * @brief Trading Terminal Main Window
 *
 * This is the application-specific main window that uses CustomMainWindow
 * as its base. CustomMainWindow handles all the frameless window mechanics,
 * while this class focuses on trading terminal specific UI and logic.
 */
class MainWindow : public CustomMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    CustomMDIArea *mdiArea() const { return m_mdiArea; }

private slots:
    // Window actions
    void createMarketWatch();
    void createBuyWindow();
    void onAddToWatchRequested(const QString &exchange, const QString &segment,
                               const QString &instrument, const QString &symbol,
                               const QString &expiry, const QString &strike,
                               const QString &optionType);
    void resetLayout();

private:
    void setupContent();
    void createMenuBar();
    void createToolBar();
    void createConnectionBar();
    void createStatusBar();
    void createInfoBar();
    // Persist state
    void closeEvent(QCloseEvent *event) override;

    CustomMDIArea *m_mdiArea;
    QToolBar *m_toolBar;
    QToolBar *m_connectionToolBar;
    QWidget *m_connectionBar;
    QStatusBar *m_statusBar;
    QWidget *m_infoBar;
    ScripBar *m_scripBar;
    QToolBar *m_scripToolBar;
};

#endif // MAINWINDOW_H