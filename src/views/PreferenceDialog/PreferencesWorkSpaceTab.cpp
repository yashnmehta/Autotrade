#include "views/PreferencesWorkSpaceTab.h"
#include "utils/PreferencesManager.h"
#include "core/widgets/CustomMDIArea.h"
#include <QDebug>
#include <QColorDialog>
#include <QFileDialog>

PreferencesWorkSpaceTab::PreferencesWorkSpaceTab(QWidget *tabWidget, PreferencesManager *prefsManager, QObject *parent)
    : QObject(parent)
    , m_tabWidget(tabWidget)
    , m_prefsManager(prefsManager)
    , m_workspaceComboBox(nullptr)
    , m_browseWorkspaceButton(nullptr)
    , m_autoLockWorkstationCheckBox(nullptr)
    , m_autoLockMinutesSpinBox(nullptr)
    , m_stockWatchListWidget(nullptr)
    , m_moveUpButton(nullptr)
    , m_moveDownButton(nullptr)
    , m_restoreDefaultsButton(nullptr)
    , m_marketPictureRadio(nullptr)
    , m_chartsRadio(nullptr)
    , m_chartTypeComboBox(nullptr)
    , m_derivativeChainRadio(nullptr)
    , m_marketPictureChartRadio(nullptr)
    , m_orderEntryRadio(nullptr)
    , m_colorSelectionComboBox(nullptr)
    , m_selectColorButton(nullptr)
    , m_showDateTimeCheckBox(nullptr)
    , m_timeWithSecondsCheckBox(nullptr)
    , m_dprPriceFreezeComboBox(nullptr)
    , m_askToSaveDevFileCheckBox(nullptr)
    , m_allColumnsRadio(nullptr)
    , m_defaultProfileRadio(nullptr)
    , m_lastUsedRadio(nullptr)
{
    findWidgets();
    setupConnections();
    populateWorkspaceComboBox();
}

void PreferencesWorkSpaceTab::findWidgets()
{
    if (!m_tabWidget) return;
    
    // Workspace Selection
    m_workspaceComboBox = m_tabWidget->findChild<QComboBox*>("comboBox_workspace");
    m_browseWorkspaceButton = m_tabWidget->findChild<QPushButton*>("pushButton_browseWorkspace");
    m_autoLockWorkstationCheckBox = m_tabWidget->findChild<QCheckBox*>("checkBox_autoLockWorkstation");
    m_autoLockMinutesSpinBox = m_tabWidget->findChild<QSpinBox*>("spinBox_autoLockMinutes");
    
    // Stock Watch Sequence
    m_stockWatchListWidget = m_tabWidget->findChild<QListWidget*>("listWidget_stockWatch");
    m_moveUpButton = m_tabWidget->findChild<QPushButton*>("pushButton_moveUp");
    m_moveDownButton = m_tabWidget->findChild<QPushButton*>("pushButton_moveDown");
    m_restoreDefaultsButton = m_tabWidget->findChild<QPushButton*>("pushButton_restoreDefaults");
    
    // Market Watch
    m_marketPictureRadio = m_tabWidget->findChild<QRadioButton*>("radioButton_marketPicture");
    m_chartsRadio = m_tabWidget->findChild<QRadioButton*>("radioButton_charts");
    m_chartTypeComboBox = m_tabWidget->findChild<QComboBox*>("comboBox_chartType");
    m_derivativeChainRadio = m_tabWidget->findChild<QRadioButton*>("radioButton_derivativeChain");
    m_marketPictureChartRadio = m_tabWidget->findChild<QRadioButton*>("radioButton_marketPictureChart");
    m_orderEntryRadio = m_tabWidget->findChild<QRadioButton*>("radioButton_orderEntry");
    m_colorSelectionComboBox = m_tabWidget->findChild<QComboBox*>("comboBox_colorSelection");
    m_selectColorButton = m_tabWidget->findChild<QPushButton*>("pushButton_selectColor");
    
    // General Settings
    m_showDateTimeCheckBox = m_tabWidget->findChild<QCheckBox*>("checkBox_showDateTime");
    m_timeWithSecondsCheckBox = m_tabWidget->findChild<QCheckBox*>("checkBox_timeWithSeconds");
    m_dprPriceFreezeComboBox = m_tabWidget->findChild<QComboBox*>("comboBox_dprPriceFreeze");
    m_askToSaveDevFileCheckBox = m_tabWidget->findChild<QCheckBox*>("checkBox_askToSaveDevFile");
    
    // Column Profile
    m_allColumnsRadio = m_tabWidget->findChild<QRadioButton*>("radioButton_allColumns");
    m_defaultProfileRadio = m_tabWidget->findChild<QRadioButton*>("radioButton_defaultProfile");
    m_lastUsedRadio = m_tabWidget->findChild<QRadioButton*>("radioButton_lastUsed");
}

void PreferencesWorkSpaceTab::setupConnections()
{
    if (m_browseWorkspaceButton) {
        connect(m_browseWorkspaceButton, &QPushButton::clicked, this, &PreferencesWorkSpaceTab::onBrowseWorkspace);
    }
    
    if (m_moveUpButton) {
        connect(m_moveUpButton, &QPushButton::clicked, this, &PreferencesWorkSpaceTab::onMoveUp);
    }
    
    if (m_moveDownButton) {
        connect(m_moveDownButton, &QPushButton::clicked, this, &PreferencesWorkSpaceTab::onMoveDown);
    }
    
    if (m_restoreDefaultsButton) {
        connect(m_restoreDefaultsButton, &QPushButton::clicked, this, &PreferencesWorkSpaceTab::onRestoreDefaults);
    }
    
    if (m_selectColorButton) {
        connect(m_selectColorButton, &QPushButton::clicked, this, &PreferencesWorkSpaceTab::onSelectColor);
    }
}

void PreferencesWorkSpaceTab::populateWorkspaceComboBox()
{
    if (!m_workspaceComboBox) return;
    
    m_workspaceComboBox->clear();
    
    // Get available workspaces from settings
    QSettings settings("TradingCompany", "TradingTerminal");
    settings.beginGroup("workspaces");
    QStringList workspaces = settings.childGroups();
    settings.endGroup();
    
    // Add default workspace
    if (!workspaces.contains("Default")) {
        workspaces.prepend("Default");
    }
    
    m_workspaceComboBox->addItems(workspaces);
}

void PreferencesWorkSpaceTab::loadPreferences()
{
    if (!m_prefsManager) return;
    
    // Workspace Selection
    if (m_workspaceComboBox) {
        QString defaultWorkspace = m_prefsManager->getDefaultWorkspace();
        int index = m_workspaceComboBox->findText(defaultWorkspace);
        if (index >= 0) {
            m_workspaceComboBox->setCurrentIndex(index);
        }
    }
    
    if (m_autoLockWorkstationCheckBox) {
        m_autoLockWorkstationCheckBox->setChecked(
            m_prefsManager->value("workspace/auto_lock_enabled", false).toBool()
        );
    }
    
    if (m_autoLockMinutesSpinBox) {
        m_autoLockMinutesSpinBox->setValue(
            m_prefsManager->value("workspace/auto_lock_minutes", 0).toInt()
        );
    }
    
    // Stock Watch Sequence - load from settings
    if (m_stockWatchListWidget) {
        QStringList exchanges = m_prefsManager->value("workspace/stock_watch_sequence", 
                                                      QStringList() << "NSE").toStringList();
        m_stockWatchListWidget->clear();
        m_stockWatchListWidget->addItems(exchanges);
    }
    
    // Market Watch - Double Click Action
    QString doubleClickAction = m_prefsManager->value("workspace/double_click_action", "market_picture").toString();
    if (doubleClickAction == "market_picture" && m_marketPictureRadio) {
        m_marketPictureRadio->setChecked(true);
    } else if (doubleClickAction == "charts" && m_chartsRadio) {
        m_chartsRadio->setChecked(true);
    } else if (doubleClickAction == "derivative_chain" && m_derivativeChainRadio) {
        m_derivativeChainRadio->setChecked(true);
    }
    
    if (m_chartTypeComboBox) {
        QString chartType = m_prefsManager->value("workspace/chart_type", "Intraday Chart").toString();
        int idx = m_chartTypeComboBox->findText(chartType);
        if (idx >= 0) m_chartTypeComboBox->setCurrentIndex(idx);
    }
    
    // Market Picture/Chart vs Order Entry
    QString secondAction = m_prefsManager->value("workspace/second_action", "market_picture_chart").toString();
    if (secondAction == "order_entry" && m_orderEntryRadio) {
        m_orderEntryRadio->setChecked(true);
    } else if (m_marketPictureChartRadio) {
        m_marketPictureChartRadio->setChecked(true);
    }
    
    // Color Selection
    if (m_selectColorButton) {
        QString colorStr = m_prefsManager->value("workspace/52week_color", "#FFFFFF").toString();
        m_selectColorButton->setStyleSheet(QString("background-color: %1;").arg(colorStr));
    }
    
    // General Settings
    if (m_showDateTimeCheckBox) {
        m_showDateTimeCheckBox->setChecked(
            m_prefsManager->value("workspace/show_datetime", true).toBool()
        );
    }
    
    if (m_timeWithSecondsCheckBox) {
        m_timeWithSecondsCheckBox->setChecked(
            m_prefsManager->value("workspace/time_with_seconds", false).toBool()
        );
    }
    
    if (m_dprPriceFreezeComboBox) {
        QString dprValue = m_prefsManager->value("workspace/dpr_price_freeze", "8").toString();
        int idx = m_dprPriceFreezeComboBox->findText(dprValue);
        if (idx >= 0) m_dprPriceFreezeComboBox->setCurrentIndex(idx);
    }
    
    if (m_askToSaveDevFileCheckBox) {
        m_askToSaveDevFileCheckBox->setChecked(
            m_prefsManager->value("workspace/ask_save_dev_file", false).toBool()
        );
    }
    
    // Column Profile
    QString columnProfile = m_prefsManager->value("workspace/column_profile", "default_profile").toString();
    if (columnProfile == "all_columns" && m_allColumnsRadio) {
        m_allColumnsRadio->setChecked(true);
    } else if (columnProfile == "last_used" && m_lastUsedRadio) {
        m_lastUsedRadio->setChecked(true);
    } else if (m_defaultProfileRadio) {
        m_defaultProfileRadio->setChecked(true);
    }
}

void PreferencesWorkSpaceTab::savePreferences()
{
    if (!m_prefsManager) return;
    
    // Workspace Selection
    if (m_workspaceComboBox) {
        m_prefsManager->setDefaultWorkspace(m_workspaceComboBox->currentText());
    }
    
    if (m_autoLockWorkstationCheckBox) {
        m_prefsManager->setValue("workspace/auto_lock_enabled", m_autoLockWorkstationCheckBox->isChecked());
    }
    
    if (m_autoLockMinutesSpinBox) {
        m_prefsManager->setValue("workspace/auto_lock_minutes", m_autoLockMinutesSpinBox->value());
    }
    
    // Stock Watch Sequence
    if (m_stockWatchListWidget) {
        QStringList exchanges;
        for (int i = 0; i < m_stockWatchListWidget->count(); ++i) {
            exchanges << m_stockWatchListWidget->item(i)->text();
        }
        m_prefsManager->setValue("workspace/stock_watch_sequence", exchanges);
    }
    
    // Market Watch - Double Click Action
    QString doubleClickAction = "market_picture";
    if (m_chartsRadio && m_chartsRadio->isChecked()) {
        doubleClickAction = "charts";
    } else if (m_derivativeChainRadio && m_derivativeChainRadio->isChecked()) {
        doubleClickAction = "derivative_chain";
    }
    m_prefsManager->setValue("workspace/double_click_action", doubleClickAction);
    
    if (m_chartTypeComboBox) {
        m_prefsManager->setValue("workspace/chart_type", m_chartTypeComboBox->currentText());
    }
    
    // Market Picture/Chart vs Order Entry
    QString secondAction = "market_picture_chart";
    if (m_orderEntryRadio && m_orderEntryRadio->isChecked()) {
        secondAction = "order_entry";
    }
    m_prefsManager->setValue("workspace/second_action", secondAction);
    
    // Color Selection - save the current color
    if (m_selectColorButton) {
        QString styleSheet = m_selectColorButton->styleSheet();
        // Extract color from stylesheet
        QRegularExpression rx("background-color:\\s*([^;]+);");
        auto match = rx.match(styleSheet);
        if (match.hasMatch()) {
            QString color = match.captured(1).trimmed();
            m_prefsManager->setValue("workspace/52week_color", color);
        }
    }
    
    // General Settings
    if (m_showDateTimeCheckBox) {
        m_prefsManager->setValue("workspace/show_datetime", m_showDateTimeCheckBox->isChecked());
    }
    
    if (m_timeWithSecondsCheckBox) {
        m_prefsManager->setValue("workspace/time_with_seconds", m_timeWithSecondsCheckBox->isChecked());
    }
    
    if (m_dprPriceFreezeComboBox) {
        m_prefsManager->setValue("workspace/dpr_price_freeze", m_dprPriceFreezeComboBox->currentText());
    }
    
    if (m_askToSaveDevFileCheckBox) {
        m_prefsManager->setValue("workspace/ask_save_dev_file", m_askToSaveDevFileCheckBox->isChecked());
    }
    
    // Column Profile
    QString columnProfile = "default_profile";
    if (m_allColumnsRadio && m_allColumnsRadio->isChecked()) {
        columnProfile = "all_columns";
    } else if (m_lastUsedRadio && m_lastUsedRadio->isChecked()) {
        columnProfile = "last_used";
    }
    m_prefsManager->setValue("workspace/column_profile", columnProfile);
}

void PreferencesWorkSpaceTab::restoreDefaults()
{
    // Workspace Selection
    if (m_workspaceComboBox) {
        int idx = m_workspaceComboBox->findText("Default");
        if (idx >= 0) m_workspaceComboBox->setCurrentIndex(idx);
    }
    
    if (m_autoLockWorkstationCheckBox) {
        m_autoLockWorkstationCheckBox->setChecked(false);
    }
    
    if (m_autoLockMinutesSpinBox) {
        m_autoLockMinutesSpinBox->setValue(0);
    }
    
    // Stock Watch Sequence
    if (m_stockWatchListWidget) {
        m_stockWatchListWidget->clear();
        m_stockWatchListWidget->addItem("NSE");
    }
    
    // Market Watch
    if (m_marketPictureRadio) {
        m_marketPictureRadio->setChecked(true);
    }
    
    if (m_marketPictureChartRadio) {
        m_marketPictureChartRadio->setChecked(true);
    }
    
    if (m_selectColorButton) {
        m_selectColorButton->setStyleSheet("background-color: rgb(255, 255, 255);");
    }
    
    // General Settings
    if (m_showDateTimeCheckBox) {
        m_showDateTimeCheckBox->setChecked(true);
    }
    
    if (m_timeWithSecondsCheckBox) {
        m_timeWithSecondsCheckBox->setChecked(false);
    }
    
    if (m_dprPriceFreezeComboBox) {
        int idx = m_dprPriceFreezeComboBox->findText("8");
        if (idx >= 0) m_dprPriceFreezeComboBox->setCurrentIndex(idx);
    }
    
    if (m_askToSaveDevFileCheckBox) {
        m_askToSaveDevFileCheckBox->setChecked(false);
    }
    
    // Column Profile
    if (m_defaultProfileRadio) {
        m_defaultProfileRadio->setChecked(true);
    }
}

void PreferencesWorkSpaceTab::onBrowseWorkspace()
{
    // Allow user to browse for workspace files
    QString fileName = QFileDialog::getOpenFileName(
        m_tabWidget,
        tr("Select Workspace File"),
        QDir::homePath(),
        tr("Workspace Files (*.workspace);;All Files (*)")
    );
    
    if (!fileName.isEmpty()) {
        // Extract workspace name from file
        QFileInfo fileInfo(fileName);
        QString workspaceName = fileInfo.baseName();
        
        // Add to combo box if not already present
        if (m_workspaceComboBox && m_workspaceComboBox->findText(workspaceName) < 0) {
            m_workspaceComboBox->addItem(workspaceName);
        }
        
        // Select the workspace
        if (m_workspaceComboBox) {
            int idx = m_workspaceComboBox->findText(workspaceName);
            if (idx >= 0) m_workspaceComboBox->setCurrentIndex(idx);
        }
    }
}

void PreferencesWorkSpaceTab::onMoveUp()
{
    if (!m_stockWatchListWidget) return;
    
    int currentRow = m_stockWatchListWidget->currentRow();
    if (currentRow > 0) {
        QListWidgetItem *item = m_stockWatchListWidget->takeItem(currentRow);
        m_stockWatchListWidget->insertItem(currentRow - 1, item);
        m_stockWatchListWidget->setCurrentRow(currentRow - 1);
    }
}

void PreferencesWorkSpaceTab::onMoveDown()
{
    if (!m_stockWatchListWidget) return;
    
    int currentRow = m_stockWatchListWidget->currentRow();
    if (currentRow >= 0 && currentRow < m_stockWatchListWidget->count() - 1) {
        QListWidgetItem *item = m_stockWatchListWidget->takeItem(currentRow);
        m_stockWatchListWidget->insertItem(currentRow + 1, item);
        m_stockWatchListWidget->setCurrentRow(currentRow + 1);
    }
}

void PreferencesWorkSpaceTab::onRestoreDefaults()
{
    if (!m_stockWatchListWidget) return;
    
    m_stockWatchListWidget->clear();
    m_stockWatchListWidget->addItem("NSE");
}

void PreferencesWorkSpaceTab::onSelectColor()
{
    if (!m_selectColorButton) return;
    
    // Get current color
    QString styleSheet = m_selectColorButton->styleSheet();
    QColor currentColor = QColor(255, 255, 255);
    
    QRegularExpression rx("background-color:\\s*rgb\\((\\d+),\\s*(\\d+),\\s*(\\d+)\\)");
    auto match = rx.match(styleSheet);
    if (match.hasMatch()) {
        currentColor = QColor(match.captured(1).toInt(), match.captured(2).toInt(), match.captured(3).toInt());
    }
    
    // Show color dialog
    QColor color = QColorDialog::getColor(currentColor, m_tabWidget, tr("Select Color"));
    
    if (color.isValid()) {
        m_selectColorButton->setStyleSheet(
            QString("background-color: rgb(%1, %2, %3);")
                .arg(color.red())
                .arg(color.green())
                .arg(color.blue())
        );
    }
}
