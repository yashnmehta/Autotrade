#include "views/PreferencesOrderTab.h"
#include "utils/PreferencesManager.h"
#include <QDebug>

PreferencesOrderTab::PreferencesOrderTab(QWidget *tabWidget, PreferencesManager *prefsManager, QObject *parent)
    : QObject(parent)
    , m_tabWidget(tabWidget)
    , m_prefsManager(prefsManager)
    , m_defaultFocusComboBox(nullptr)
{
    findWidgets();
    setupConnections();
}

void PreferencesOrderTab::findWidgets()
{
    if (!m_tabWidget) return;

    // Find widgets by object name from PreferencesOrderTab.ui
    m_defaultFocusComboBox = m_tabWidget->findChild<QComboBox*>("comboBox_defaultFocus");
}

void PreferencesOrderTab::setupConnections()
{
    // Add connections if needed (e.g., specific logic when values change)
}

void PreferencesOrderTab::loadPreferences()
{
    if (!m_prefsManager) return;

    if (m_defaultFocusComboBox) {
        // Load default focus field (Quantity, Price, Scrip)
        // We use the FocusField enum but store as int or string in settings
        // PreferencesManager handles translation
        // For simplicity, we just match text
        QString focusFieldStr = m_prefsManager->focusFieldToString(m_prefsManager->getOrderWindowFocusField());
        
        // Convert "qty" to "Quantity", "price" to "Price", "symbol" to "Scrip" matching UI
        // Actually, let's just find the text
        if (focusFieldStr == "qty") focusFieldStr = "Quantity";
        else if (focusFieldStr == "rate") focusFieldStr = "Price";
        else if (focusFieldStr == "symbol") focusFieldStr = "Scrip";
        
        int index = m_defaultFocusComboBox->findText(focusFieldStr);
        if (index >= 0) {
            m_defaultFocusComboBox->setCurrentIndex(index);
        }
    }
}

void PreferencesOrderTab::savePreferences()
{
    if (!m_prefsManager) return;

    if (m_defaultFocusComboBox) {
        QString currentText = m_defaultFocusComboBox->currentText();
        PreferencesManager::FocusField field = PreferencesManager::FocusField::Quantity;
        
        if (currentText == "Price") field = PreferencesManager::FocusField::Price;
        else if (currentText == "Scrip") field = PreferencesManager::FocusField::Scrip;
        
        m_prefsManager->setOrderWindowFocusField(field);
    }
}

void PreferencesOrderTab::restoreDefaults()
{
    if (m_defaultFocusComboBox) {
        int index = m_defaultFocusComboBox->findText("Quantity");
        if (index >= 0) m_defaultFocusComboBox->setCurrentIndex(index);
    }
}
