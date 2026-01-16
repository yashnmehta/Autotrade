#include "views/PreferencesPortfolioTab.h"
#include "utils/PreferencesManager.h"
#include <QDebug>

PreferencesPortfolioTab::PreferencesPortfolioTab(QWidget *tabWidget, PreferencesManager *prefsManager, QObject *parent)
    : QObject(parent)
    , m_tabWidget(tabWidget)
    , m_prefsManager(prefsManager)
    , m_defaultPositionViewComboBox(nullptr)
{
    findWidgets();
    setupConnections();
}

void PreferencesPortfolioTab::findWidgets()
{
    if (!m_tabWidget) return;
    
    m_defaultPositionViewComboBox = m_tabWidget->findChild<QComboBox*>("comboBox_defaultPositionView");
    if (!m_defaultPositionViewComboBox) {
        qWarning() << "PreferencesPortfolioTab: 'comboBox_defaultPositionView' not found";
    }
}

void PreferencesPortfolioTab::setupConnections()
{
    // Add connections if needed
}

void PreferencesPortfolioTab::loadPreferences()
{
    if (!m_prefsManager) return;
    
    if (m_defaultPositionViewComboBox) {
        QString view = m_prefsManager->getPositionBookDefaultView();
        int index = m_defaultPositionViewComboBox->findText(view);
        if (index >= 0) {
            m_defaultPositionViewComboBox->setCurrentIndex(index);
        } else {
             // Default to index 0 if not found
             if (m_defaultPositionViewComboBox->count() > 0)
                 m_defaultPositionViewComboBox->setCurrentIndex(0);
        }
    }
}

void PreferencesPortfolioTab::savePreferences()
{
    if (!m_prefsManager) return;
    
    if (m_defaultPositionViewComboBox) {
        m_prefsManager->setPositionBookDefaultView(m_defaultPositionViewComboBox->currentText());
    }
}

void PreferencesPortfolioTab::restoreDefaults()
{
    if (m_defaultPositionViewComboBox) {
        // Reset to default (usually Net)
        int index = m_defaultPositionViewComboBox->findText("Net");
        if (index >= 0) {
            m_defaultPositionViewComboBox->setCurrentIndex(index);
        }
    }
}
