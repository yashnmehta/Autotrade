#ifndef PREFERENCES_ORDER_TAB_H
#define PREFERENCES_ORDER_TAB_H

#include <QObject>
#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>

class PreferencesManager;

class PreferencesOrderTab : public QObject
{
    Q_OBJECT

public:
    explicit PreferencesOrderTab(QWidget *tabWidget, PreferencesManager *prefsManager, QObject *parent = nullptr);
    
    void loadPreferences();
    void savePreferences();
    void restoreDefaults();

private:
    void findWidgets();
    void setupConnections();

    QWidget *m_tabWidget;
    PreferencesManager *m_prefsManager;
    
    // Configurable Widgets
    QComboBox *m_defaultFocusComboBox;
};

#endif // PREFERENCES_ORDER_TAB_H
