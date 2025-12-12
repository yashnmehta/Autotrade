#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ui/CustomMainWindow.h"

class CustomMDIArea;
class QToolBar;
class QStatusBar;
class QCloseEvent;
class ScripBar;
class QLabel;
class QDockWidget;

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
    void showInfoBarContextMenu(const QPoint &pos);
    // Persist state
    void closeEvent(QCloseEvent *event) override;

    CustomMDIArea *m_mdiArea;
    QToolBar *m_toolBar;
    QToolBar *m_connectionToolBar;
    QWidget *m_connectionBar;
    QStatusBar *m_statusBar;
    QDockWidget *m_infoDock;
    QWidget *m_infoBarWidget;
    QLabel *m_infoTextLabel;
    QLabel *m_connIconLabel;
    QLabel *m_lastUpdateLabel;
    QAction *m_statusBarAction;
    QAction *m_infoBarAction;
    ScripBar *m_scripBar;
    QToolBar *m_scripToolBar;
};

#endif // MAINWINDOW_H