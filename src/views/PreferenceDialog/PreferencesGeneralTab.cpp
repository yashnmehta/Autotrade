#include "views/PreferencesGeneralTab.h"
#include "utils/PreferencesManager.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>

// Preference key prefixes
static const QString KEY_PREFIX_EVENT = "General/Event/";
static const QString KEY_BEEP_SUFFIX = "/Beep";
static const QString KEY_FLASH_SUFFIX = "/FlashMessage";
static const QString KEY_STATIC_MSG_SUFFIX = "/StaticMessage";
static const QString KEY_PLAY_SOUND_SUFFIX = "/PlaySound";
static const QString KEY_SOUND_PATH_SUFFIX = "/SoundPath";
static const QString KEY_SHORTCUT_SCHEME = "General/ShortcutScheme";
static const QString KEY_ORDERBOOK_STATUS = "General/OrderBookDefaultStatus";
static const QString KEY_TICK_DATA_ROWS = "General/TickData/Rows";
static const QString KEY_AUTO_SCROLL_TICK = "General/TickData/AutoScroll";
static const QString KEY_SHOW_TIMESTAMP = "General/TickData/ShowTimestamp";

PreferencesGeneralTab::PreferencesGeneralTab(QWidget *tabWidget, PreferencesManager *prefsManager, QObject *parent)
    : QObject(parent)
    , m_tabWidget(tabWidget)
    , m_prefsManager(prefsManager)
    , m_eventComboBox(nullptr)
    , m_beepCheckBox(nullptr)
    , m_flashMessageCheckBox(nullptr)
    , m_staticMessageCheckBox(nullptr)
    , m_playSoundCheckBox(nullptr)
    , m_browseSoundButton(nullptr)
    , m_shortcutSchemeComboBox(nullptr)
    , m_orderBookStatusComboBox(nullptr)
    , m_tickDataRowsSpinBox(nullptr)
    , m_autoScrollTickDataCheckBox(nullptr)
    , m_showTimeStampCheckBox(nullptr)
    , m_defaultWorkspaceLineEdit(nullptr)
    , m_defaultPositionViewComboBox(nullptr)
{
    findWidgets();
    setupConnections();
}

void PreferencesGeneralTab::findWidgets()
{
    if (!m_tabWidget) {
        return;
    }

    // Find widgets by object name from PreferencesGeneralTab.ui
    // On Event Group
    m_eventComboBox = m_tabWidget->findChild<QComboBox*>("comboBox_event");
    m_beepCheckBox = m_tabWidget->findChild<QCheckBox*>("checkBox_beep");
    m_flashMessageCheckBox = m_tabWidget->findChild<QCheckBox*>("checkBox_flashMessageBar");
    m_staticMessageCheckBox = m_tabWidget->findChild<QCheckBox*>("checkBox_messageBox");
    m_playSoundCheckBox = m_tabWidget->findChild<QCheckBox*>("checkBox_playSound");
    m_browseSoundButton = m_tabWidget->findChild<QPushButton*>("pushButton_browse");
    
    // Shortcut Key Schemes
    m_shortcutSchemeComboBox = m_tabWidget->findChild<QComboBox*>("comboBox_shortcutScheme");
    
    // Order Book Settings
    m_orderBookStatusComboBox = m_tabWidget->findChild<QComboBox*>("comboBox_orderBookStatus");
    
    // Tick Data Window
    m_tickDataRowsSpinBox = m_tabWidget->findChild<QSpinBox*>("spinBox_tickDataRows");
    m_autoScrollTickDataCheckBox = m_tabWidget->findChild<QCheckBox*>("checkBox_autoScrollTickData");
    m_showTimeStampCheckBox = m_tabWidget->findChild<QCheckBox*>("checkBox_showTimestamp");
    
    // New Settings
    m_defaultWorkspaceLineEdit = m_tabWidget->findChild<QLineEdit*>("lineEdit_defaultWorkspace");
    m_defaultPositionViewComboBox = m_tabWidget->findChild<QComboBox*>("comboBox_defaultPositionView");
}

void PreferencesGeneralTab::setupConnections()
{
    // Connect event combo box to load event-specific preferences
    if (m_eventComboBox) {
        connect(m_eventComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &PreferencesGeneralTab::onEventChanged);
    }
    
    // Connect checkboxes
    if (m_beepCheckBox) {
        connect(m_beepCheckBox, &QCheckBox::stateChanged,
                this, &PreferencesGeneralTab::onBeepChanged);
    }
    
    if (m_flashMessageCheckBox) {
        connect(m_flashMessageCheckBox, &QCheckBox::stateChanged,
                this, &PreferencesGeneralTab::onFlashMessageChanged);
    }
    
    if (m_staticMessageCheckBox) {
        connect(m_staticMessageCheckBox, &QCheckBox::stateChanged,
                this, &PreferencesGeneralTab::onStaticMessageChanged);
    }
    
    if (m_playSoundCheckBox) {
        connect(m_playSoundCheckBox, &QCheckBox::stateChanged,
                this, &PreferencesGeneralTab::onPlaySoundChanged);
    }
    
    // Connect browse button for sound file
    if (m_browseSoundButton) {
        connect(m_browseSoundButton, &QPushButton::clicked,
                this, &PreferencesGeneralTab::onBrowseClicked);
    }
    
    // Connect shortcut scheme combo box
    if (m_shortcutSchemeComboBox) {
        connect(m_shortcutSchemeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &PreferencesGeneralTab::onShortcutSchemeChanged);
    }
}

void PreferencesGeneralTab::loadPreferences()
{
    if (!m_prefsManager) {
        return;
    }

    // Load shortcut scheme
    if (m_shortcutSchemeComboBox) {
        QString scheme = m_prefsManager->value(KEY_SHORTCUT_SCHEME, "NSE").toString();
        int index = m_shortcutSchemeComboBox->findText(scheme);
        if (index >= 0) {
            m_shortcutSchemeComboBox->setCurrentIndex(index);
        }
    }
    
    // Load Order Book default status
    if (m_orderBookStatusComboBox) {
        QString status = m_prefsManager->value(KEY_ORDERBOOK_STATUS, "All").toString();
        int index = m_orderBookStatusComboBox->findText(status);
        if (index >= 0) {
            m_orderBookStatusComboBox->setCurrentIndex(index);
        }
    }
    
    // Load tick data settings
    if (m_tickDataRowsSpinBox) {
        int rows = m_prefsManager->value(KEY_TICK_DATA_ROWS, 100).toInt();
        m_tickDataRowsSpinBox->setValue(rows);
    }
    
    if (m_autoScrollTickDataCheckBox) {
        bool autoScroll = m_prefsManager->value(KEY_AUTO_SCROLL_TICK, true).toBool();
        m_autoScrollTickDataCheckBox->setChecked(autoScroll);
    }
    
    if (m_showTimeStampCheckBox) {
        bool showTimestamp = m_prefsManager->value(KEY_SHOW_TIMESTAMP, true).toBool();
        m_showTimeStampCheckBox->setChecked(showTimestamp);
    }
    
    // Load Default Workspace
    if (m_defaultWorkspaceLineEdit) {
        m_defaultWorkspaceLineEdit->setText(m_prefsManager->getDefaultWorkspace());
    }
    
    // Load Position Book View
    if (m_defaultPositionViewComboBox) {
        QString view = m_prefsManager->getPositionBookDefaultView();
        // UI has "Net" and "DayWise"
        QString searchText = view;
        if (view == "NetWise") searchText = "Net";
        
        int index = m_defaultPositionViewComboBox->findText(searchText);
        if (index >= 0) {
            m_defaultPositionViewComboBox->setCurrentIndex(index);
        }
    }
    
    // Load event preferences for the currently selected event
    if (m_eventComboBox && m_eventComboBox->count() > 0) {
        loadEventPreferences(m_eventComboBox->currentText());
    }
}

void PreferencesGeneralTab::savePreferences()
{
    if (!m_prefsManager) {
        return;
    }

    // Save shortcut scheme
    if (m_shortcutSchemeComboBox) {
        m_prefsManager->setValue(KEY_SHORTCUT_SCHEME, m_shortcutSchemeComboBox->currentText());
    }
    
    // Save Order Book default status
    if (m_orderBookStatusComboBox) {
        m_prefsManager->setValue(KEY_ORDERBOOK_STATUS, m_orderBookStatusComboBox->currentText());
    }
    
    // Save tick data settings
    if (m_tickDataRowsSpinBox) {
        m_prefsManager->setValue(KEY_TICK_DATA_ROWS, m_tickDataRowsSpinBox->value());
    }
    
    if (m_autoScrollTickDataCheckBox) {
        m_prefsManager->setValue(KEY_AUTO_SCROLL_TICK, m_autoScrollTickDataCheckBox->isChecked());
    }
    
    if (m_showTimeStampCheckBox) {
        m_prefsManager->setValue(KEY_SHOW_TIMESTAMP, m_showTimeStampCheckBox->isChecked());
    }
    
    // Save Default Workspace
    if (m_defaultWorkspaceLineEdit) {
        m_prefsManager->setDefaultWorkspace(m_defaultWorkspaceLineEdit->text());
    }
    
    // Save Position Book View
    if (m_defaultPositionViewComboBox) {
        QString currentText = m_defaultPositionViewComboBox->currentText();
        QString value = currentText;
        if (currentText == "Net") value = "NetWise";
        
        m_prefsManager->setPositionBookDefaultView(value);
    }
    
    // Save event preferences for the currently selected event
    if (m_eventComboBox && m_eventComboBox->count() > 0) {
        saveEventPreferences(m_eventComboBox->currentText());
    }
}

void PreferencesGeneralTab::restoreDefaults()
{
    // Restore default values for all settings
    if (m_shortcutSchemeComboBox) {
        m_shortcutSchemeComboBox->setCurrentText("NSE");
    }
    
    if (m_tickDataRowsSpinBox) {
        m_tickDataRowsSpinBox->setValue(100);
    }
    
    if (m_autoScrollTickDataCheckBox) {
        m_autoScrollTickDataCheckBox->setChecked(true);
    }
    
    if (m_showTimeStampCheckBox) {
        m_showTimeStampCheckBox->setChecked(true);
    }
    
    // Default Workspace
    if (m_defaultWorkspaceLineEdit) {
        m_defaultWorkspaceLineEdit->setText("Default");
    }
    
    // Default Position View "Net"
    if (m_defaultPositionViewComboBox) {
        int index = m_defaultPositionViewComboBox->findText("Net");
        if (index >= 0) m_defaultPositionViewComboBox->setCurrentIndex(index);
    }
    
    // Restore default event settings (all disabled)
    if (m_beepCheckBox) {
        m_beepCheckBox->setChecked(false);
    }
    
    if (m_flashMessageCheckBox) {
        m_flashMessageCheckBox->setChecked(false);
    }
    
    if (m_staticMessageCheckBox) {
        m_staticMessageCheckBox->setChecked(false);
    }
    
    if (m_playSoundCheckBox) {
        m_playSoundCheckBox->setChecked(false);
    }
    
    m_currentSoundPath.clear();
}

void PreferencesGeneralTab::loadEventPreferences(const QString &eventName)
{
    if (!m_prefsManager || eventName.isEmpty()) {
        return;
    }

    // Load event-specific preferences
    bool beepEnabled = m_prefsManager->value(getBeepKey(eventName), false).toBool();
    bool flashEnabled = m_prefsManager->value(getFlashMessageKey(eventName), false).toBool();
    bool staticMsgEnabled = m_prefsManager->value(getStaticMessageKey(eventName), false).toBool();
    bool playSoundEnabled = m_prefsManager->value(getPlaySoundKey(eventName), false).toBool();
    QString soundPath = m_prefsManager->value(getSoundPathKey(eventName), "").toString();
    
    // Update UI
    if (m_beepCheckBox) {
        m_beepCheckBox->setChecked(beepEnabled);
    }
    
    if (m_flashMessageCheckBox) {
        m_flashMessageCheckBox->setChecked(flashEnabled);
    }
    
    if (m_staticMessageCheckBox) {
        m_staticMessageCheckBox->setChecked(staticMsgEnabled);
    }
    
    if (m_playSoundCheckBox) {
        m_playSoundCheckBox->setChecked(playSoundEnabled);
    }
    
    m_currentSoundPath = soundPath;
}

void PreferencesGeneralTab::saveEventPreferences(const QString &eventName)
{
    if (!m_prefsManager || eventName.isEmpty()) {
        return;
    }

    // Save event-specific preferences
    if (m_beepCheckBox) {
        m_prefsManager->setValue(getBeepKey(eventName), m_beepCheckBox->isChecked());
    }
    
    if (m_flashMessageCheckBox) {
        m_prefsManager->setValue(getFlashMessageKey(eventName), m_flashMessageCheckBox->isChecked());
    }
    
    if (m_staticMessageCheckBox) {
        m_prefsManager->setValue(getStaticMessageKey(eventName), m_staticMessageCheckBox->isChecked());
    }
    
    if (m_playSoundCheckBox) {
        m_prefsManager->setValue(getPlaySoundKey(eventName), m_playSoundCheckBox->isChecked());
    }
    
    m_prefsManager->setValue(getSoundPathKey(eventName), m_currentSoundPath);
}

// Preference key generators
QString PreferencesGeneralTab::getBeepKey(const QString &eventName) const
{
    return KEY_PREFIX_EVENT + eventName + KEY_BEEP_SUFFIX;
}

QString PreferencesGeneralTab::getFlashMessageKey(const QString &eventName) const
{
    return KEY_PREFIX_EVENT + eventName + KEY_FLASH_SUFFIX;
}

QString PreferencesGeneralTab::getStaticMessageKey(const QString &eventName) const
{
    return KEY_PREFIX_EVENT + eventName + KEY_STATIC_MSG_SUFFIX;
}

QString PreferencesGeneralTab::getPlaySoundKey(const QString &eventName) const
{
    return KEY_PREFIX_EVENT + eventName + KEY_PLAY_SOUND_SUFFIX;
}

QString PreferencesGeneralTab::getSoundPathKey(const QString &eventName) const
{
    return KEY_PREFIX_EVENT + eventName + KEY_SOUND_PATH_SUFFIX;
}

// Public getter methods
bool PreferencesGeneralTab::isBeepEnabled() const
{
    return m_beepCheckBox ? m_beepCheckBox->isChecked() : false;
}

QString PreferencesGeneralTab::getSelectedEvent() const
{
    return m_eventComboBox ? m_eventComboBox->currentText() : QString();
}

bool PreferencesGeneralTab::isFlashMessageEnabled() const
{
    return m_flashMessageCheckBox ? m_flashMessageCheckBox->isChecked() : false;
}

bool PreferencesGeneralTab::isStaticMessageEnabled() const
{
    return m_staticMessageCheckBox ? m_staticMessageCheckBox->isChecked() : false;
}

bool PreferencesGeneralTab::isPlaySoundEnabled() const
{
    return m_playSoundCheckBox ? m_playSoundCheckBox->isChecked() : false;
}

QString PreferencesGeneralTab::getSoundFilePath() const
{
    return m_currentSoundPath;
}

// Slot implementations
void PreferencesGeneralTab::onEventChanged(int index)
{
    Q_UNUSED(index);
    
    // Load preferences for the newly selected event
    if (m_eventComboBox && m_eventComboBox->count() > 0) {
        QString currentEvent = m_eventComboBox->currentText();
        loadEventPreferences(currentEvent);
    }
}

void PreferencesGeneralTab::onBeepChanged(int state)
{
    Q_UNUSED(state);
}

void PreferencesGeneralTab::onFlashMessageChanged(int state)
{
    Q_UNUSED(state);
}

void PreferencesGeneralTab::onStaticMessageChanged(int state)
{
    Q_UNUSED(state);
}

void PreferencesGeneralTab::onPlaySoundChanged(int state)
{
    
    // Enable/disable browse button based on checkbox state
    if (m_browseSoundButton) {
        m_browseSoundButton->setEnabled(state == Qt::Checked);
    }
}

void PreferencesGeneralTab::onBrowseClicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        qobject_cast<QWidget*>(parent()),
        "Select Sound File",
        m_currentSoundPath.isEmpty() ? QDir::homePath() : m_currentSoundPath,
        "Sound Files (*.wav *.mp3 *.ogg);;All Files (*.*)"
    );
    
    if (!fileName.isEmpty()) {
        m_currentSoundPath = fileName;
        QMessageBox::information(
            qobject_cast<QWidget*>(parent()),
            "Sound File Selected",
            "Selected: " + QFileInfo(fileName).fileName()
        );
    }
}

void PreferencesGeneralTab::onShortcutSchemeChanged(int index)
{
    Q_UNUSED(index);
}
