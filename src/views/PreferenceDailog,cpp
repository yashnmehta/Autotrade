#include "views/PreferenceDialog.h"
#include "ui_preference.h"
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
    
    // Set fixed size to prevent resizing
    setFixedSize(620, 480);
    
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
    // Button box connections
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &PreferenceDialog::onOkClicked);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    // Apply button
    if (auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply)) {
        connect(applyButton, &QPushButton::clicked, this, &PreferenceDialog::onApplyClicked);
    }
    
    // Restore defaults buttons
    connect(ui->restoreDefaultsButton, &QPushButton::clicked, this, &PreferenceDialog::onRestoreDefaultsClicked);
    connect(ui->restoreDefaultsButton_2, &QPushButton::clicked, this, &PreferenceDialog::onRestoreDefaultsClicked);
    
    // Browse buttons
    connect(ui->browseButton, &QPushButton::clicked, this, &PreferenceDialog::onBrowseClicked);
    connect(ui->browseButton_2, &QPushButton::clicked, this, &PreferenceDialog::onBrowseClicked);
    
    // Color button
    connect(ui->colorButton, &QPushButton::clicked, this, &PreferenceDialog::onColorButtonClicked);
    
    // Move up/down buttons
    connect(ui->moveUpButton, &QPushButton::clicked, this, &PreferenceDialog::onMoveUpClicked);
    connect(ui->moveDownButton, &QPushButton::clicked, this, &PreferenceDialog::onMoveUpClicked);
    connect(ui->moveUpButton_2, &QPushButton::clicked, this, &PreferenceDialog::onMoveUpClicked);
    connect(ui->moveDownButton_2, &QPushButton::clicked, this, &PreferenceDialog::onMoveDownClicked);
    
    // Apply button in Order tab
    connect(ui->applyButton, &QPushButton::clicked, this, &PreferenceDialog::onApplyClicked);
}

void PreferenceDialog::loadPreferences()
{
    // Load preferences from PreferencesManager
    // General Tab
    ui->symbolTrendColorCheckBox->setChecked(
        m_prefsManager->value("General/SymbolTrendColor", false).toBool()
    );
    ui->includeUnderlyingCheckBox->setChecked(
        m_prefsManager->value("General/IncludeUnderlying", false).toBool()
    );
    ui->includeOtherFUTCheckBox->setChecked(
        m_prefsManager->value("General/IncludeOtherFUT", false).toBool()
    );
    
    // Market Watch combo
    QString marketWatch = m_prefsManager->value("General/MarketWatch", "21").toString();
    ui->marketWatchCombo->setCurrentText(marketWatch);
    
    // Refresh interval
    ui->refreshAfterFixedTimeCheckBox->setChecked(
        m_prefsManager->value("General/RefreshEnabled", false).toBool()
    );
    ui->refreshIntervalSpinBox->setValue(
        m_prefsManager->value("General/RefreshInterval", 0).toInt()
    );
    
    // Pop Up Mode
    bool alertMode = m_prefsManager->value("General/AlertMessage", true).toBool();
    ui->alertMessageRadio->setChecked(alertMode);
    ui->gridOrderEntryRadio->setChecked(!alertMode);
    
    // Optimization Settings
    ui->greekRequiredCheckBox->setChecked(
        m_prefsManager->value("General/GreekRequired", false).toBool()
    );
    ui->alertRequiredCheckBox->setChecked(
        m_prefsManager->value("General/AlertRequired", false).toBool()
    );
    ui->algoRequiredCheckBox->setChecked(
        m_prefsManager->value("General/AlgoRequired", false).toBool()
    );
    ui->globalDealerCheckBox->setChecked(
        m_prefsManager->value("General/GlobalDealer", false).toBool()
    );
    
    // S&W Sequence
    ui->enableSAWForEQCheckBox->setChecked(
        m_prefsManager->value("General/EnableSAW", false).toBool()
    );
    ui->nseCheckBox->setChecked(
        m_prefsManager->value("General/NSE", false).toBool()
    );
    ui->bseCheckBox->setChecked(
        m_prefsManager->value("General/BSE", false).toBool()
    );
    
    // Order Tab
    ui->marketSegmentCombo->setCurrentText(
        m_prefsManager->value("Order/MarketSegment", "NSE EQ").toString()
    );
    ui->productTypeCombo->setCurrentText(
        m_prefsManager->value("Order/ProductType", "Margin").toString()
    );
    
    // Order Confirmations
    ui->freshOrderCheckBox->setChecked(
        m_prefsManager->value("Order/FreshOrderConfirm", false).toBool()
    );
    ui->orderModificationCheckBox->setChecked(
        m_prefsManager->value("Order/ModificationConfirm", false).toBool()
    );
    ui->marketOrderCheckBox->setChecked(
        m_prefsManager->value("Order/MarketOrderConfirm", false).toBool()
    );
    ui->orderCancellationCheckBox->setChecked(
        m_prefsManager->value("Order/CancellationConfirm", false).toBool()
    );
    
    // Work Space Tab
    ui->defaultWorkspaceCombo->setCurrentText(
        m_prefsManager->value("WorkSpace/Default", "HARSHIL01").toString()
    );
    ui->autoLockWorkstationCheckBox->setChecked(
        m_prefsManager->value("WorkSpace/AutoLock", false).toBool()
    );
    ui->autoLockMinutesSpinBox->setValue(
        m_prefsManager->value("WorkSpace/AutoLockMinutes", 0).toInt()
    );
    ui->timeWithSecondsCheckBox->setChecked(
        m_prefsManager->value("WorkSpace/TimeWithSeconds", false).toBool()
    );
    ui->showDateTimeCheckBox->setChecked(
        m_prefsManager->value("WorkSpace/ShowDateTime", false).toBool()
    );
    
    // Column Profile
    QString columnProfile = m_prefsManager->value("WorkSpace/ColumnProfile", "LastUsed").toString();
    ui->allColumnsRadio->setChecked(columnProfile == "AllColumns");
    ui->defaultProfileRadio->setChecked(columnProfile == "DefaultProfile");
    ui->lastUsedRadio->setChecked(columnProfile == "LastUsed");
    
    // Market Watch Double Click
    QString doubleClickAction = m_prefsManager->value("WorkSpace/DoubleClickAction", "OrderEntry").toString();
    ui->marketPictureRadio->setChecked(doubleClickAction == "MarketPicture");
    ui->chartsRadio->setChecked(doubleClickAction == "Charts");
    ui->derivativeChainRadio->setChecked(doubleClickAction == "DerivativeChain");
    ui->marketPictureChartRadio->setChecked(doubleClickAction == "MarketPictureChart");
    ui->orderEntryRadio->setChecked(doubleClickAction == "OrderEntry");
    
    // Derivatives Tab
    ui->greekUserCombo->setCurrentText(
        m_prefsManager->value("Derivatives/User", "ASCIB02").toString()
    );
    ui->defExchCombo->setCurrentText(
        m_prefsManager->value("Derivatives/DefExch", "NSE").toString()
    );
    ui->defSymbolLineEdit->setText(
        m_prefsManager->value("Derivatives/DefSymbol", "").toString()
    );
    
    ui->dividendLineEdit->setText(
        m_prefsManager->value("Derivatives/Dividend", "0.00").toString()
    );
    ui->interestRateLineEdit->setText(
        m_prefsManager->value("Derivatives/InterestRate", "0.00").toString()
    );
    ui->considerForIndexCheckBox->setChecked(
        m_prefsManager->value("Derivatives/ConsiderForIndex", false).toBool()
    );
    
    // Volatility
    ui->volatilityCLineEdit->setText(
        m_prefsManager->value("Derivatives/VolatilityC", "0.00").toString()
    );
    ui->volatilityPLineEdit->setText(
        m_prefsManager->value("Derivatives/VolatilityP", "0.00").toString()
    );
    
    // Underlying Price
    bool spotPrice = m_prefsManager->value("Derivatives/SpotPrice", false).toBool();
    ui->spotPriceRadio->setChecked(spotPrice);
    ui->futuresRadio->setChecked(!spotPrice);
    
    // Parameters
    ui->onlineVolatilityCheckBox->setChecked(
        m_prefsManager->value("Derivatives/OnlineVolatility", false).toBool()
    );
    ui->theoreticalPriceCheckBox->setChecked(
        m_prefsManager->value("Derivatives/TheoreticalPrice", false).toBool()
    );
    ui->useATMCheckBox->setChecked(
        m_prefsManager->value("Derivatives/UseATM", false).toBool()
    );
    ui->openPayoffCheckBox->setChecked(
        m_prefsManager->value("Derivatives/OpenPayoff", false).toBool()
    );
    
    // Alerts Msg Tab
    ui->eventCombo->setCurrentText(
        m_prefsManager->value("Alerts/Event", "Order Error").toString()
    );
    ui->beepCheckBox->setChecked(
        m_prefsManager->value("Alerts/Beep", false).toBool()
    );
    ui->staticMsgCheckBox->setChecked(
        m_prefsManager->value("Alerts/StaticMsg", false).toBool()
    );
    ui->messageBoxCheckBox->setChecked(
        m_prefsManager->value("Alerts/MessageBox", false).toBool()
    );
    ui->playSoundCheckBox->setChecked(
        m_prefsManager->value("Alerts/PlaySound", false).toBool()
    );
    
    // Transaction Backup
    ui->automaticCheckBox->setChecked(
        m_prefsManager->value("Alerts/AutomaticBackup", false).toBool()
    );
    ui->retainBackupPathCheckBox->setChecked(
        m_prefsManager->value("Alerts/RetainBackupPath", false).toBool()
    );
    ui->retainLastSettingCheckBox->setChecked(
        m_prefsManager->value("Alerts/RetainLastSetting", false).toBool()
    );
    ui->defaultOnlinePathCheckBox->setChecked(
        m_prefsManager->value("Alerts/DefaultOnlinePath", false).toBool()
    );
}

void PreferenceDialog::savePreferences()
{
    // Save all preferences to PreferencesManager
    // General Tab
    m_prefsManager->setValue("General/SymbolTrendColor", ui->symbolTrendColorCheckBox->isChecked());
    m_prefsManager->setValue("General/IncludeUnderlying", ui->includeUnderlyingCheckBox->isChecked());
    m_prefsManager->setValue("General/IncludeOtherFUT", ui->includeOtherFUTCheckBox->isChecked());
    m_prefsManager->setValue("General/MarketWatch", ui->marketWatchCombo->currentText());
    m_prefsManager->setValue("General/RefreshEnabled", ui->refreshAfterFixedTimeCheckBox->isChecked());
    m_prefsManager->setValue("General/RefreshInterval", ui->refreshIntervalSpinBox->value());
    m_prefsManager->setValue("General/AlertMessage", ui->alertMessageRadio->isChecked());
    m_prefsManager->setValue("General/GreekRequired", ui->greekRequiredCheckBox->isChecked());
    m_prefsManager->setValue("General/AlertRequired", ui->alertRequiredCheckBox->isChecked());
    m_prefsManager->setValue("General/AlgoRequired", ui->algoRequiredCheckBox->isChecked());
    m_prefsManager->setValue("General/GlobalDealer", ui->globalDealerCheckBox->isChecked());
    m_prefsManager->setValue("General/EnableSAW", ui->enableSAWForEQCheckBox->isChecked());
    m_prefsManager->setValue("General/NSE", ui->nseCheckBox->isChecked());
    m_prefsManager->setValue("General/BSE", ui->bseCheckBox->isChecked());
    
    // Order Tab
    m_prefsManager->setValue("Order/MarketSegment", ui->marketSegmentCombo->currentText());
    m_prefsManager->setValue("Order/ProductType", ui->productTypeCombo->currentText());
    m_prefsManager->setValue("Order/FreshOrderConfirm", ui->freshOrderCheckBox->isChecked());
    m_prefsManager->setValue("Order/ModificationConfirm", ui->orderModificationCheckBox->isChecked());
    m_prefsManager->setValue("Order/MarketOrderConfirm", ui->marketOrderCheckBox->isChecked());
    m_prefsManager->setValue("Order/CancellationConfirm", ui->orderCancellationCheckBox->isChecked());
    
    // Work Space Tab
    m_prefsManager->setValue("WorkSpace/Default", ui->defaultWorkspaceCombo->currentText());
    m_prefsManager->setValue("WorkSpace/AutoLock", ui->autoLockWorkstationCheckBox->isChecked());
    m_prefsManager->setValue("WorkSpace/AutoLockMinutes", ui->autoLockMinutesSpinBox->value());
    m_prefsManager->setValue("WorkSpace/TimeWithSeconds", ui->timeWithSecondsCheckBox->isChecked());
    m_prefsManager->setValue("WorkSpace/ShowDateTime", ui->showDateTimeCheckBox->isChecked());
    
    // Column Profile
    QString columnProfile = "LastUsed";
    if (ui->allColumnsRadio->isChecked()) columnProfile = "AllColumns";
    else if (ui->defaultProfileRadio->isChecked()) columnProfile = "DefaultProfile";
    m_prefsManager->setValue("WorkSpace/ColumnProfile", columnProfile);
    
    // Market Watch Double Click
    QString doubleClickAction = "OrderEntry";
    if (ui->marketPictureRadio->isChecked()) doubleClickAction = "MarketPicture";
    else if (ui->chartsRadio->isChecked()) doubleClickAction = "Charts";
    else if (ui->derivativeChainRadio->isChecked()) doubleClickAction = "DerivativeChain";
    else if (ui->marketPictureChartRadio->isChecked()) doubleClickAction = "MarketPictureChart";
    m_prefsManager->setValue("WorkSpace/DoubleClickAction", doubleClickAction);
    
    // Derivatives Tab
    m_prefsManager->setValue("Derivatives/User", ui->greekUserCombo->currentText());
    m_prefsManager->setValue("Derivatives/DefExch", ui->defExchCombo->currentText());
    m_prefsManager->setValue("Derivatives/DefSymbol", ui->defSymbolLineEdit->text());
    m_prefsManager->setValue("Derivatives/Dividend", ui->dividendLineEdit->text());
    m_prefsManager->setValue("Derivatives/InterestRate", ui->interestRateLineEdit->text());
    m_prefsManager->setValue("Derivatives/ConsiderForIndex", ui->considerForIndexCheckBox->isChecked());
    m_prefsManager->setValue("Derivatives/VolatilityC", ui->volatilityCLineEdit->text());
    m_prefsManager->setValue("Derivatives/VolatilityP", ui->volatilityPLineEdit->text());
    m_prefsManager->setValue("Derivatives/SpotPrice", ui->spotPriceRadio->isChecked());
    m_prefsManager->setValue("Derivatives/OnlineVolatility", ui->onlineVolatilityCheckBox->isChecked());
    m_prefsManager->setValue("Derivatives/TheoreticalPrice", ui->theoreticalPriceCheckBox->isChecked());
    m_prefsManager->setValue("Derivatives/UseATM", ui->useATMCheckBox->isChecked());
    m_prefsManager->setValue("Derivatives/OpenPayoff", ui->openPayoffCheckBox->isChecked());
    
    // Alerts Msg Tab
    m_prefsManager->setValue("Alerts/Event", ui->eventCombo->currentText());
    m_prefsManager->setValue("Alerts/Beep", ui->beepCheckBox->isChecked());
    m_prefsManager->setValue("Alerts/StaticMsg", ui->staticMsgCheckBox->isChecked());
    m_prefsManager->setValue("Alerts/MessageBox", ui->messageBoxCheckBox->isChecked());
    m_prefsManager->setValue("Alerts/PlaySound", ui->playSoundCheckBox->isChecked());
    m_prefsManager->setValue("Alerts/AutomaticBackup", ui->automaticCheckBox->isChecked());
    m_prefsManager->setValue("Alerts/RetainBackupPath", ui->retainBackupPathCheckBox->isChecked());
    m_prefsManager->setValue("Alerts/RetainLastSetting", ui->retainLastSettingCheckBox->isChecked());
    m_prefsManager->setValue("Alerts/DefaultOnlinePath", ui->defaultOnlinePathCheckBox->isChecked());
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

void PreferenceDialog::onColorButtonClicked()
{
    QColor color = QColorDialog::getColor(Qt::white, this, "Select Color");
    if (color.isValid()) {
        // Set the button color
        QString styleSheet = QString("background-color: %1;").arg(color.name());
        ui->colorButton->setStyleSheet(styleSheet);
    }
}

void PreferenceDialog::onMoveUpClicked()
{
    // Move selected item up in the stock watch list
    int currentRow = ui->stockWatchList->currentRow();
    if (currentRow > 0) {
        QListWidgetItem *item = ui->stockWatchList->takeItem(currentRow);
        ui->stockWatchList->insertItem(currentRow - 1, item);
        ui->stockWatchList->setCurrentRow(currentRow - 1);
    }
}

void PreferenceDialog::onMoveDownClicked()
{
    // Move selected item down in the stock watch list
    int currentRow = ui->stockWatchList->currentRow();
    if (currentRow >= 0 && currentRow < ui->stockWatchList->count() - 1) {
        QListWidgetItem *item = ui->stockWatchList->takeItem(currentRow);
        ui->stockWatchList->insertItem(currentRow + 1, item);
        ui->stockWatchList->setCurrentRow(currentRow + 1);
    }
}
