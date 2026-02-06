#ifndef PREFERENCES_WORKSPACE_TAB_H
#define PREFERENCES_WORKSPACE_TAB_H

#include <QObject>
#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QPushButton>
#include <QListWidget>

class PreferencesManager;

class PreferencesWorkSpaceTab : public QObject
{
    Q_OBJECT

public:
    explicit PreferencesWorkSpaceTab(QWidget *tabWidget, PreferencesManager *prefsManager, QObject *parent = nullptr);
    
    void loadPreferences();
    void savePreferences();
    void restoreDefaults();

private slots:
    void onBrowseWorkspace();
    void onMoveUp();
    void onMoveDown();
    void onRestoreDefaults();
    void onSelectColor();

private:
    void findWidgets();
    void setupConnections();
    void populateWorkspaceComboBox();

    QWidget *m_tabWidget;
    PreferencesManager *m_prefsManager;
    
    // Workspace Selection
    QComboBox *m_workspaceComboBox;
    QPushButton *m_browseWorkspaceButton;
    QCheckBox *m_autoLockWorkstationCheckBox;
    QSpinBox *m_autoLockMinutesSpinBox;
    
    // Stock Watch Sequence
    QListWidget *m_stockWatchListWidget;
    QPushButton *m_moveUpButton;
    QPushButton *m_moveDownButton;
    QPushButton *m_restoreDefaultsButton;
    
    // Market Watch
    QRadioButton *m_marketPictureRadio;
    QRadioButton *m_chartsRadio;
    QComboBox *m_chartTypeComboBox;
    QRadioButton *m_derivativeChainRadio;
    QRadioButton *m_marketPictureChartRadio;
    QRadioButton *m_orderEntryRadio;
    QComboBox *m_colorSelectionComboBox;
    QPushButton *m_selectColorButton;
    
    // General Settings
    QCheckBox *m_showDateTimeCheckBox;
    QCheckBox *m_timeWithSecondsCheckBox;
    QComboBox *m_dprPriceFreezeComboBox;
    QCheckBox *m_askToSaveDevFileCheckBox;
    
    // Column Profile
    QRadioButton *m_allColumnsRadio;
    QRadioButton *m_defaultProfileRadio;
    QRadioButton *m_lastUsedRadio;
};

#endif // PREFERENCES_WORKSPACE_TAB_H
