#ifndef PREFERENCES_PORTFOLIO_TAB_H
#define PREFERENCES_PORTFOLIO_TAB_H

#include <QObject>
#include <QWidget>
#include <QComboBox>

class PreferencesManager;

class PreferencesPortfolioTab : public QObject
{
    Q_OBJECT

public:
    explicit PreferencesPortfolioTab(QWidget *tabWidget, PreferencesManager *prefsManager, QObject *parent = nullptr);
    
    void loadPreferences();
    void savePreferences();
    void restoreDefaults();

private:
    void findWidgets();
    void setupConnections();

    QWidget *m_tabWidget;
    PreferencesManager *m_prefsManager;
    
    // Configurable Widgets
    QComboBox *m_defaultPositionViewComboBox;
};

#endif // PREFERENCES_PORTFOLIO_TAB_H
