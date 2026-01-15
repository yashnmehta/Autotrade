#ifndef PREFERENCES_WORKSPACE_TAB_H
#define PREFERENCES_WORKSPACE_TAB_H

#include <QObject>
#include <QWidget>
#include <QLineEdit>

class PreferencesManager;

class PreferencesWorkSpaceTab : public QObject
{
    Q_OBJECT

public:
    explicit PreferencesWorkSpaceTab(QWidget *tabWidget, PreferencesManager *prefsManager, QObject *parent = nullptr);
    
    void loadPreferences();
    void savePreferences();
    void restoreDefaults();

private:
    void findWidgets();
    void setupConnections();

    QWidget *m_tabWidget;
    PreferencesManager *m_prefsManager;
    
    // Configurable Widgets
    QLineEdit *m_defaultWorkspaceLineEdit;
};

#endif // PREFERENCES_WORKSPACE_TAB_H
