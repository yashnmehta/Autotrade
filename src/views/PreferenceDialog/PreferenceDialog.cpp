#include "views/PreferenceDialog.h"
#include "views/PreferencesGeneralTab.h"
#include "views/PreferencesOrderTab.h"
#include "views/PreferencesWorkSpaceTab.h"
#include "views/PreferencesPortfolioTab.h"
#include "ui_PreferencesWindowTab.h"
#include "utils/PreferencesManager.h"
#include <QUiLoader>
#include <QFile>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QDebug>

PreferenceDialog::PreferenceDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PreferencesWindowTab)
    , m_prefsManager(&PreferencesManager::instance())
    , m_generalTab(nullptr)
    , m_orderTab(nullptr)
{
    ui->setupUi(this);
    
    // Set dialog properties
    setWindowTitle("Preferences");
    setModal(true);
    
    // Set fixed size to prevent resizing
    setFixedSize(770, 670);
    
    // Remove resize grip
    setSizeGripEnabled(false);
    
    // Load individual tab UI files into tab containers
    loadTabContent("tabGeneral", ":/forms/PreferencesGeneralTab.ui");
    loadTabContent("tabOrder", ":/forms/PreferencesOrderTab.ui");
    loadTabContent("tabDerivatives", ":/forms/PreferencesDerivativeTab.ui");
    loadTabContent("tabAlertsMsg", ":/forms/PreferencesAlertMessageTab.ui");
    loadTabContent("tabMarginPlusOrder", ":/forms/PreferencesMarginPlusOrderTab.ui");
    loadTabContent("tabWorkSpace", ":/forms/PreferencesWorkSpaceTab.ui");
    loadTabContent("tabPortfolio", ":/forms/PreferencesPortfolioTab.ui");
    
    // Initialize tab handlers after loading tab content
    QWidget *generalTabWidget = findChild<QWidget*>("tabGeneral");
    if (generalTabWidget) {
        m_generalTab = new PreferencesGeneralTab(generalTabWidget, m_prefsManager, this);
    }
    
    QWidget *orderTabWidget = findChild<QWidget*>("tabOrder");
    if (orderTabWidget) {
        m_orderTab = new PreferencesOrderTab(orderTabWidget, m_prefsManager, this);
    }
    
    QWidget *workSpaceTabWidget = findChild<QWidget*>("tabWorkSpace");
    if (workSpaceTabWidget) {
        m_workSpaceTab = new PreferencesWorkSpaceTab(workSpaceTabWidget, m_prefsManager, this);
    }

    QWidget *portfolioTabWidget = findChild<QWidget*>("tabPortfolio");
    if (portfolioTabWidget) {
        m_portfolioTab = new PreferencesPortfolioTab(portfolioTabWidget, m_prefsManager, this);
    }
    
    setupConnections();
    loadPreferences();
    
    // Center the dialog on parent window
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

void PreferenceDialog::loadTabContent(const QString &tabName, const QString &uiFilePath)
{
    // Find the tab widget by name
    QWidget *tabWidget = findChild<QWidget*>(tabName);
    if (!tabWidget) {
        qWarning() << "Tab widget not found:" << tabName;
        return;
    }
    
    // Get the layout of the tab
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(tabWidget->layout());
    if (!layout) {
        qWarning() << "Tab layout not found for:" << tabName;
        return;
    }
    
    // Load the UI file
    QUiLoader loader;
    QFile file(uiFilePath);
    
    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "Could not open UI file:" << uiFilePath;
        return;
    }
    
    QWidget *tabContent = loader.load(&file, tabWidget);
    file.close();
    
    if (tabContent) {
        // Add the loaded content to the tab's layout
        layout->addWidget(tabContent);
    } else {
        qWarning() << "Failed to load UI file:" << uiFilePath;
    }
}


void PreferenceDialog::setupConnections()
{
    // Button connections for new UI
    connect(ui->pushButton_ok, &QPushButton::clicked, this, &PreferenceDialog::onOkClicked);
    connect(ui->pushButton_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->pushButton_apply, &QPushButton::clicked, this, &PreferenceDialog::onApplyClicked);
    
    // Note: Browse and Restore Defaults buttons are not in the container UI
    // They would be in individual tab UIs if needed
}

void PreferenceDialog::loadPreferences()
{
    // Load General tab preferences
    if (m_generalTab) {
        m_generalTab->loadPreferences();
    }
    
    // Load other tab preferences when their handlers are created
    if (m_orderTab) m_orderTab->loadPreferences();
    if (m_workSpaceTab) m_workSpaceTab->loadPreferences();
    if (m_portfolioTab) m_portfolioTab->loadPreferences();
    
    qDebug() << "PreferenceDialog: All preferences loaded";
}

void PreferenceDialog::savePreferences()
{
    // Save General tab preferences
    if (m_generalTab) {
        m_generalTab->savePreferences();
    }
    
    // Save other tab preferences when their handlers are created
    if (m_orderTab) m_orderTab->savePreferences();
    if (m_workSpaceTab) m_workSpaceTab->savePreferences();
    if (m_portfolioTab) m_portfolioTab->savePreferences();
    
    qDebug() << "PreferenceDialog: All preferences saved";
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

void PreferenceDialog::onCancelClicked()
{
    reject();
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
        // Restore defaults for General tab
        if (m_generalTab) {
            m_generalTab->restoreDefaults();
        }
        
        // Restore defaults for other tabs when their handlers are created
        if (m_orderTab) m_orderTab->restoreDefaults();
        
        // Clear all preferences from manager
        m_prefsManager->clear();
        
        // Reload with default values
        loadPreferences();
        
        QMessageBox::information(this, "Restore Defaults", "Default preferences have been restored.");
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
