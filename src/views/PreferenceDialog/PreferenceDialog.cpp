#include "views/PreferenceDialog.h"
#include "ui_PreferencesWindow.h"
#include "utils/PreferencesManager.h"
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>

PreferenceDialog::PreferenceDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PreferenceDialog)
    , m_prefsManager(&PreferencesManager::instance())
{
    ui->setupUi(this);
    setupConnections();
    
    // Set dialog properties BEFORE loading preferences
    setWindowTitle("Preferences");
    setModal(true);
    
    // Set fixed size to prevent resizing (updated for new UI to match ODIN)
    setFixedSize(1000, 750);
    
    // Remove resize grip
    setSizeGripEnabled(false);
    
    // Make tabs scrollable
    if (ui->tabWidget) {
        // Add scroll area to each tab
        for (int i = 0; i < ui->tabWidget->count(); ++i) {
            QWidget *tab = ui->tabWidget->widget(i);
            if (tab) {
                QLayout *originalLayout = tab->layout();
                if (originalLayout) {
                    // Set proper spacing for the original layout
                    originalLayout->setSpacing(6);
                    originalLayout->setContentsMargins(8, 8, 8, 8);
                    
                    // Create scroll area
                    QScrollArea *scrollArea = new QScrollArea(tab);
                    scrollArea->setWidgetResizable(true);
                    scrollArea->setFrameShape(QFrame::NoFrame);
                    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
                    
                    // Create container widget for the content
                    QWidget *container = new QWidget();
                    container->setLayout(originalLayout);
                    
                    scrollArea->setWidget(container);
                    
                    // Create new layout for the tab
                    QVBoxLayout *newLayout = new QVBoxLayout(tab);
                    newLayout->setContentsMargins(0, 0, 0, 0);
                    newLayout->setSpacing(0);
                    newLayout->addWidget(scrollArea);
                }
            }
        }
    }
    
    loadPreferences();
    
    // Center the dialog on parent window AFTER setting size
    if (parent) {
        QRect parentGeometry = parent->geometry();
        int x = parentGeometry.x() + (parentGeometry.width() - width()) / 2;
        int y = parentGeometry.y() + (parentGeometry.height() - height()) / 2;
        move(x, y);
    }
}

PreferenceDialog::~PreferenceDialog()
{
    delete ui;
}

void PreferenceDialog::setupConnections()
{
    // Button connections for new UI
    connect(ui->pushButton_ok, &QPushButton::clicked, this, &PreferenceDialog::onOkClicked);
    connect(ui->pushButton_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->pushButton_apply, &QPushButton::clicked, this, &PreferenceDialog::onApplyClicked);
    
    // Restore defaults button
    connect(ui->pushButton_restoreDefaults, &QPushButton::clicked, this, &PreferenceDialog::onRestoreDefaultsClicked);
    
    // Browse button
    connect(ui->pushButton_browse, &QPushButton::clicked, this, &PreferenceDialog::onBrowseClicked);
}

void PreferenceDialog::loadPreferences()
{
    // Load preferences from PreferencesManager
    // General Tab - On Event
    ui->comboBox_event->setCurrentText(
        m_prefsManager->value("General/Event", "Order Error").toString()
    );
    ui->checkBox_beep->setChecked(
        m_prefsManager->value("General/Beep", false).toBool()
    );
    ui->checkBox_flashMessageBar->setChecked(
        m_prefsManager->value("General/FlashMessageBar", false).toBool()
    );
    ui->checkBox_messageBox->setChecked(
        m_prefsManager->value("General/MessageBox", false).toBool()
    );
    ui->checkBox_playSound->setChecked(
        m_prefsManager->value("General/PlaySound", false).toBool()
    );
    
    // Shortcut Key Schemes
    ui->comboBox_shortcutScheme->setCurrentText(
        m_prefsManager->value("General/ShortcutScheme", "NSE").toString()
    );
    
    // Time Format
    ui->comboBox_timeFormat->setCurrentText(
        m_prefsManager->value("General/TimeFormat", "Time in 12-Hour Format").toString()
    );
    
    // Preferred Exchange
    ui->comboBox_preferredExchange->setCurrentText(
        m_prefsManager->value("General/PreferredExchange", "NSE").toString()
    );
    
    // Order Book
    ui->comboBox_orderBookOpen->setCurrentText(
        m_prefsManager->value("General/OrderBookOpen", "Pending").toString()
    );
    
    // Checkboxes
    ui->checkBox_closeAllWindows->setChecked(
        m_prefsManager->value("General/CloseAllWindows", true).toBool()
    );
    ui->checkBox_autoReconnect->setChecked(
        m_prefsManager->value("General/AutoReconnect", false).toBool()
    );
    ui->checkBox_displayDate->setChecked(
        m_prefsManager->value("General/DisplayDate", true).toBool()
    );
}

void PreferenceDialog::savePreferences()
{
    // Save all preferences to PreferencesManager
    // General Tab
    m_prefsManager->setValue("General/Event", ui->comboBox_event->currentText());
    m_prefsManager->setValue("General/Beep", ui->checkBox_beep->isChecked());
    m_prefsManager->setValue("General/FlashMessageBar", ui->checkBox_flashMessageBar->isChecked());
    m_prefsManager->setValue("General/MessageBox", ui->checkBox_messageBox->isChecked());
    m_prefsManager->setValue("General/PlaySound", ui->checkBox_playSound->isChecked());
    m_prefsManager->setValue("General/ShortcutScheme", ui->comboBox_shortcutScheme->currentText());
    m_prefsManager->setValue("General/TimeFormat", ui->comboBox_timeFormat->currentText());
    m_prefsManager->setValue("General/PreferredExchange", ui->comboBox_preferredExchange->currentText());
    m_prefsManager->setValue("General/OrderBookOpen", ui->comboBox_orderBookOpen->currentText());
    m_prefsManager->setValue("General/CloseAllWindows", ui->checkBox_closeAllWindows->isChecked());
    m_prefsManager->setValue("General/AutoReconnect", ui->checkBox_autoReconnect->isChecked());
    m_prefsManager->setValue("General/DisplayDate", ui->checkBox_displayDate->isChecked());
}

void PreferenceDialog::applyPreferences()
{
    savePreferences();
    QMessageBox::information(this, "Preferences", "Preferences have been applied successfully.");
}

void PreferenceDialog::onApplyClicked()
{
    applyPreferences();
}

void PreferenceDialog::onOkClicked()
{
    savePreferences();
    accept();
}

void PreferenceDialog::onRestoreDefaultsClicked()
{
    auto reply = QMessageBox::question(
        this,
        "Restore Defaults",
        "Are you sure you want to restore default preferences?",
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        // Clear all preferences
        m_prefsManager->clear();
        // Reload with default values
        loadPreferences();
    }
}

void PreferenceDialog::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Directory",
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty()) {
        // Handle the selected directory
        QMessageBox::information(this, "Directory Selected", "Selected: " + dir);
    }
}
