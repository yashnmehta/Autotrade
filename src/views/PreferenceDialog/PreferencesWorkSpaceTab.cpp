#include "views/PreferencesWorkSpaceTab.h"
#include "utils/PreferencesManager.h"
#include <QDebug>

PreferencesWorkSpaceTab::PreferencesWorkSpaceTab(QWidget *tabWidget, PreferencesManager *prefsManager, QObject *parent)
    : QObject(parent)
    , m_tabWidget(tabWidget)
    , m_prefsManager(prefsManager)
    , m_defaultWorkspaceLineEdit(nullptr)
{
    findWidgets();
    setupConnections();
}

void PreferencesWorkSpaceTab::findWidgets()
{
    if (!m_tabWidget) return;
    
    m_defaultWorkspaceLineEdit = m_tabWidget->findChild<QLineEdit*>("lineEdit_defaultWorkspace");
    if (!m_defaultWorkspaceLineEdit) {
        qWarning() << "PreferencesWorkSpaceTab: 'lineEdit_defaultWorkspace' not found";
    }
}

void PreferencesWorkSpaceTab::setupConnections()
{
    // Add connections if needed (e.g. validitors)
}

void PreferencesWorkSpaceTab::loadPreferences()
{
    if (!m_prefsManager) return;
    
    if (m_defaultWorkspaceLineEdit) {
        m_defaultWorkspaceLineEdit->setText(m_prefsManager->getDefaultWorkspace());
    }
}

void PreferencesWorkSpaceTab::savePreferences()
{
    if (!m_prefsManager) return;
    
    if (m_defaultWorkspaceLineEdit) {
        m_prefsManager->setDefaultWorkspace(m_defaultWorkspaceLineEdit->text());
    }
}

void PreferencesWorkSpaceTab::restoreDefaults()
{
    if (m_defaultWorkspaceLineEdit) {
        m_defaultWorkspaceLineEdit->setText("Default");
    }
}
