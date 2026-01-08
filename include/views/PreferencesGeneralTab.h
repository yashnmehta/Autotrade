#ifndef PREFERENCESGENERALTAB_H
#define PREFERENCESGENERALTAB_H

#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QString>

class PreferencesManager;

/**
 * @brief Handles the General tab in Preferences dialog
 * 
 * Manages:
 * - Event notifications (Beep, Flash Message, Static Message Box, Play Sound)
 * - Shortcut key schemes
 * - Tick data window settings
 * - Chart settings
 * - Title bar preferences
 * - Market watch settings
 * - Scrip exchange preferences
 */
class PreferencesGeneralTab : public QObject
{
    Q_OBJECT

public:
    explicit PreferencesGeneralTab(QWidget *tabWidget, PreferencesManager *prefsManager, QObject *parent = nullptr);
    ~PreferencesGeneralTab() = default;

    // Load preferences from PreferencesManager to UI
    void loadPreferences();
    
    // Save preferences from UI to PreferencesManager
    void savePreferences();
    
    // Restore default values
    void restoreDefaults();

    // Check if beep sound is enabled
    bool isBeepEnabled() const;
    
    // Get the selected event type
    QString getSelectedEvent() const;
    
    // Check if flash message bar is enabled for selected event
    bool isFlashMessageEnabled() const;
    
    // Check if static message box is enabled for selected event
    bool isStaticMessageEnabled() const;
    
    // Check if play sound is enabled for selected event
    bool isPlaySoundEnabled() const;
    
    // Get the custom sound file path
    QString getSoundFilePath() const;

private slots:
    void onEventChanged(int index);
    void onBeepChanged(int state);
    void onFlashMessageChanged(int state);
    void onStaticMessageChanged(int state);
    void onPlaySoundChanged(int state);
    void onBrowseClicked();
    void onShortcutSchemeChanged(int index);

private:
    void setupConnections();
    void findWidgets();
    void loadEventPreferences(const QString &eventName);
    void saveEventPreferences(const QString &eventName);
    
    // Preference keys
    QString getBeepKey(const QString &eventName) const;
    QString getFlashMessageKey(const QString &eventName) const;
    QString getStaticMessageKey(const QString &eventName) const;
    QString getPlaySoundKey(const QString &eventName) const;
    QString getSoundPathKey(const QString &eventName) const;

    QWidget *m_tabWidget;
    PreferencesManager *m_prefsManager;
    
    // UI Widgets from PreferencesGeneralTab.ui
    // On Event Group
    QComboBox *m_eventComboBox;
    QCheckBox *m_beepCheckBox;
    QCheckBox *m_flashMessageCheckBox;
    QCheckBox *m_staticMessageCheckBox;
    QCheckBox *m_playSoundCheckBox;
    QPushButton *m_browseSoundButton;
    
    // Shortcut Key Schemes
    QComboBox *m_shortcutSchemeComboBox;
    
    // Order Book Settings
    QComboBox *m_orderBookStatusComboBox;
    
    // Tick Data Window
    QSpinBox *m_tickDataRowsSpinBox;
    QCheckBox *m_autoScrollTickDataCheckBox;
    QCheckBox *m_showTimeStampCheckBox;
    
    // Chart Settings
    QCheckBox *m_enableChartCheckBox;
    QComboBox *m_chartTypeComboBox;
    QSpinBox *m_chartPeriodSpinBox;
    
    // Title Bar Settings
    QCheckBox *m_showScripNameCheckBox;
    QCheckBox *m_showLTPCheckBox;
    QCheckBox *m_showChangeCheckBox;
    QCheckBox *m_showPercentChangeCheckBox;
    
    // Market Watch Settings
    QCheckBox *m_autoSortMarketWatchCheckBox;
    QComboBox *m_sortColumnComboBox;
    QComboBox *m_sortOrderComboBox;
    
    // Scrip Exchange Settings
    QCheckBox *m_showExchangeCheckBox;
    QComboBox *m_defaultExchangeComboBox;
    
    // Store current sound file path
    QString m_currentSoundPath;
};

#endif // PREFERENCESGENERALTAB_H
