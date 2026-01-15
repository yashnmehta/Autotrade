#include "views/ColumnProfileDialog.h"
#include "models/MarketWatchColumnProfile.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QDebug>

ColumnProfileDialog::ColumnProfileDialog(const MarketWatchColumnProfile &currentProfile, ProfileContext context, QWidget *parent)
    : QDialog(parent)
    , m_profile(currentProfile)
    , m_context(context)
    , m_accepted(false)
{
    setWindowTitle("Market Watch - Column Selection");
    setModal(true);
    resize(700, 500);
    
    // Apply optimized modern dark theme
    setStyleSheet(
        "QDialog {"
        "    background-color: #1e1e1e;"
        "    color: #cccccc;"
        "}"
        "QLabel {"
        "    color: #e0e0e0;"
        "    font-size: 13px;"
        "    font-weight: 500;"
        "}"
        "QComboBox {"
        "    background-color: #2d2d30;"
        "    color: #cccccc;"
        "    border: 1px solid #454545;"
        "    border-radius: 2px;"
        "    padding: 6px 10px;"
        "    min-height: 24px;"
        "    font-size: 13px;"
        "}"
        "QComboBox:hover {"
        "    border: 1px solid #007acc;"
        "    background-color: #2d2d30;"
        "}"
        "QComboBox:focus {"
        "    border: 1px solid #007acc;"
        "}"
        "QComboBox::drop-down {"
        "    subcontrol-origin: padding;"
        "    subcontrol-position: top right;"
        "    width: 20px;"
        "    border: none;"
        "}"
        "QComboBox::down-arrow {"
        "    image: none;"
        "    border-left: 4px solid transparent;"
        "    border-right: 4px solid transparent;"
        "    border-top: 5px solid #cccccc;"
        "    margin-right: 5px;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background-color: #2d2d30;"
        "    color: #cccccc;"
        "    selection-background-color: #094771;"
        "    selection-color: #ffffff;"
        "    border: 1px solid #454545;"
        "    outline: none;"
        "    padding: 2px;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "    min-height: 24px;"
        "    padding: 4px 8px;"
        "}"
        "QListWidget {"
        "    background-color: #252526;"
        "    color: #cccccc;"
        "    border: 1px solid #454545;"
        "    border-radius: 2px;"
        "    outline: none;"
        "    font-size: 13px;"
        "    padding: 2px;"
        "}"
        "QListWidget::item {"
        "    padding: 6px 10px;"
        "    border: none;"
        "    border-radius: 2px;"
        "    margin: 1px;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #094771;"
        "    color: #ffffff;"
        "}"
        "QListWidget::item:hover:!selected {"
        "    background-color: #2a2d2e;"
        "}"
        "QListWidget::item:focus {"
        "    outline: none;"
        "}"
        "QPushButton {"
        "    background-color: #0e639c;"
        "    color: #ffffff;"
        "    border: 1px solid #0e639c;"
        "    border-radius: 2px;"
        "    padding: 7px 18px;"
        "    font-size: 13px;"
        "    font-weight: 500;"
        "    min-width: 75px;"
        "    min-height: 28px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #1177bb;"
        "    border: 1px solid #1177bb;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #0d5a8f;"
        "    border: 1px solid #0d5a8f;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #2d2d30;"
        "    color: #656565;"
        "    border: 1px solid #454545;"
        "}"
        "QPushButton:focus {"
        "    outline: none;"
        "    border: 1px solid #007acc;"
        "}"
        "QGroupBox {"
        "    color: #e0e0e0;"
        "    font-size: 13px;"
        "    font-weight: 500;"
        "    border: 1px solid #454545;"
        "    border-radius: 3px;"
        "    margin-top: 8px;"
        "    padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    subcontrol-position: top left;"
        "    padding: 0 5px;"
        "    color: #e0e0e0;"
        "}"
    );
    
    setupUI();
    setupConnections();
    loadProfile(currentProfile);
}

void ColumnProfileDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Top section: Profile selection
    QHBoxLayout *profileLayout = new QHBoxLayout();
    profileLayout->addWidget(new QLabel("Column Profile Name:"));
    
    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(200);
    profileLayout->addWidget(m_profileCombo);
    
    m_saveButton = new QPushButton("Save", this);
    m_deleteButton = new QPushButton("Delete", this);
    m_setDefaultButton = new QPushButton("Set As Default", this);
    
    profileLayout->addWidget(m_saveButton);
    profileLayout->addWidget(m_deleteButton);
    profileLayout->addWidget(m_setDefaultButton);
    profileLayout->addStretch();
    
    mainLayout->addLayout(profileLayout);
    
    // Middle section: Column lists
    QHBoxLayout *listsLayout = new QHBoxLayout();
    
    // Available Columns
    QVBoxLayout *availableLayout = new QVBoxLayout();
    availableLayout->addWidget(new QLabel("Available Columns"));
    m_availableList = new QListWidget(this);
    m_availableList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    availableLayout->addWidget(m_availableList);
    listsLayout->addLayout(availableLayout);
    
    // Add/Remove buttons
    QVBoxLayout *buttonLayout = new QVBoxLayout();
    buttonLayout->addStretch();
    m_addButton = new QPushButton("Add >", this);
    m_removeButton = new QPushButton("< Remove", this);
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addStretch();
    listsLayout->addLayout(buttonLayout);
    
    // Selected Columns
    QVBoxLayout *selectedLayout = new QVBoxLayout();
    selectedLayout->addWidget(new QLabel("Selected Columns"));
    m_selectedList = new QListWidget(this);
    m_selectedList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    selectedLayout->addWidget(m_selectedList);
    listsLayout->addLayout(selectedLayout);
    
    // Move Up/Down buttons
    QVBoxLayout *moveLayout = new QVBoxLayout();
    moveLayout->addStretch();
    m_moveUpButton = new QPushButton("Move Up", this);
    m_moveDownButton = new QPushButton("Move Down", this);
    moveLayout->addWidget(m_moveUpButton);
    moveLayout->addWidget(m_moveDownButton);
    moveLayout->addStretch();
    listsLayout->addLayout(moveLayout);
    
    mainLayout->addLayout(listsLayout);
    
    // Bottom section: OK/Cancel
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_okButton = new QPushButton("OK", this);
    m_cancelButton = new QPushButton("Cancel", this);
    m_okButton->setDefault(true);
    bottomLayout->addWidget(m_okButton);
    bottomLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(bottomLayout);
    
    // Refresh profile list
    refreshProfileList();
}

void ColumnProfileDialog::setupConnections()
{
    connect(m_profileCombo, &QComboBox::currentTextChanged, this, &ColumnProfileDialog::onProfileChanged);
    connect(m_addButton, &QPushButton::clicked, this, &ColumnProfileDialog::onAddColumn);
    connect(m_removeButton, &QPushButton::clicked, this, &ColumnProfileDialog::onRemoveColumn);
    connect(m_moveUpButton, &QPushButton::clicked, this, &ColumnProfileDialog::onMoveUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &ColumnProfileDialog::onMoveDown);
    connect(m_saveButton, &QPushButton::clicked, this, &ColumnProfileDialog::onSaveProfile);
    connect(m_deleteButton, &QPushButton::clicked, this, &ColumnProfileDialog::onDeleteProfile);
    connect(m_setDefaultButton, &QPushButton::clicked, this, &ColumnProfileDialog::onSetAsDefault);
    connect(m_okButton, &QPushButton::clicked, this, &ColumnProfileDialog::onAccepted);
    connect(m_cancelButton, &QPushButton::clicked, this, &ColumnProfileDialog::onRejected);
}

void ColumnProfileDialog::loadProfile(const MarketWatchColumnProfile &profile)
{
    m_profile = profile;
    updateAvailableColumns();
    updateSelectedColumns();
}

void ColumnProfileDialog::updateAvailableColumns()
{
    m_availableList->clear();
    
    QList<MarketWatchColumn> allCols = MarketWatchColumnProfile::getAllColumns();
    QList<MarketWatchColumn> visibleCols = m_profile.visibleColumns();
    
    for (MarketWatchColumn col : allCols) {
        if (!visibleCols.contains(col)) {
            QString name = MarketWatchColumnProfile::getColumnName(col);
            QListWidgetItem *item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, static_cast<int>(col));
            m_availableList->addItem(item);
        }
    }
}

void ColumnProfileDialog::updateSelectedColumns()
{
    m_selectedList->clear();
    
    QList<MarketWatchColumn> visibleCols = m_profile.visibleColumns();
    
    for (MarketWatchColumn col : visibleCols) {
        QString name = MarketWatchColumnProfile::getColumnName(col);
        QListWidgetItem *item = new QListWidgetItem(name);
        item->setData(Qt::UserRole, static_cast<int>(col));
        m_selectedList->addItem(item);
    }
}

void ColumnProfileDialog::refreshProfileList()
{
    m_profileCombo->clear();
    
    // Add preset profiles
    m_profileCombo->addItem("Default");
    m_profileCombo->addItem("Compact");
    m_profileCombo->addItem("Detailed");
    m_profileCombo->addItem("F&O");
    m_profileCombo->addItem("Equity");
    m_profileCombo->addItem("Trading");
    
    // Add custom profiles
    // Add custom profiles matching current context
    MarketWatchProfileManager &manager = MarketWatchProfileManager::instance();
    QStringList customProfiles = manager.profileNames();
    
    if (!customProfiles.isEmpty()) {
        bool separatorAdded = false;
        
        for (const QString &name : customProfiles) {
            // Filter by context
            if (manager.getProfile(name).context() == m_context) {
                if (!separatorAdded) {
                    m_profileCombo->insertSeparator(m_profileCombo->count());
                    separatorAdded = true;
                }
                m_profileCombo->addItem(name);
            }
        }
    }
    
    // Set current profile
    int index = m_profileCombo->findText(m_profile.name());
    if (index >= 0) {
        m_profileCombo->setCurrentIndex(index);
    }
}

void ColumnProfileDialog::onProfileChanged(const QString &profileName)
{
    MarketWatchColumnProfile profile;
    
    // Load preset profiles
    if (profileName == "Default") {
        profile = MarketWatchColumnProfile::createDefaultProfile();
    } else if (profileName == "Compact") {
        profile = MarketWatchColumnProfile::createCompactProfile();
    } else if (profileName == "Detailed") {
        profile = MarketWatchColumnProfile::createDetailedProfile();
    } else if (profileName == "F&O") {
        profile = MarketWatchColumnProfile::createFOProfile();
    } else if (profileName == "Equity") {
        profile = MarketWatchColumnProfile::createEquityProfile();
    } else if (profileName == "Trading") {
        profile = MarketWatchColumnProfile::createTradingProfile();
    } else {
        // Load custom profile
        MarketWatchProfileManager &manager = MarketWatchProfileManager::instance();
        if (manager.hasProfile(profileName)) {
            profile = manager.getProfile(profileName);
        } else {
            return;
        }
    }
    
    loadProfile(profile);
}

void ColumnProfileDialog::onAddColumn()
{
    QList<QListWidgetItem*> selectedItems = m_availableList->selectedItems();
    
    if (selectedItems.isEmpty()) {
        return;
    }
    
    // Get current visible columns order
    QList<MarketWatchColumn> newOrder = m_profile.columnOrder();
    
    // Add selected columns
    for (QListWidgetItem *item : selectedItems) {
        MarketWatchColumn col = static_cast<MarketWatchColumn>(item->data(Qt::UserRole).toInt());
        m_profile.setColumnVisible(col, true);
        
        // Add to order if not already there
        if (!newOrder.contains(col)) {
            newOrder.append(col);
        }
    }
    
    m_profile.setColumnOrder(newOrder);
    
    updateAvailableColumns();
    updateSelectedColumns();
}

void ColumnProfileDialog::onRemoveColumn()
{
    QList<QListWidgetItem*> selectedItems = m_selectedList->selectedItems();
    
    if (selectedItems.isEmpty()) {
        return;
    }
    
    // Must keep at least one column
    if (m_profile.visibleColumnCount() - selectedItems.size() < 1) {
        QMessageBox::warning(this, "Cannot Remove", 
                           "At least one column must remain visible.");
        return;
    }
    
    for (QListWidgetItem *item : selectedItems) {
        MarketWatchColumn col = static_cast<MarketWatchColumn>(item->data(Qt::UserRole).toInt());
        m_profile.setColumnVisible(col, false);
    }
    
    updateAvailableColumns();
    updateSelectedColumns();
}

void ColumnProfileDialog::onMoveUp()
{
    int currentRow = m_selectedList->currentRow();
    if (currentRow <= 0) return;
    
    QListWidgetItem *currentItem = m_selectedList->takeItem(currentRow);
    m_selectedList->insertItem(currentRow - 1, currentItem);
    m_selectedList->setCurrentRow(currentRow - 1);
    
    // Update profile order immediately to reflect UI state
    // (This ensures consistency if multiple moves happen)
}

void ColumnProfileDialog::onMoveDown()
{
    int currentRow = m_selectedList->currentRow();
    if (currentRow < 0 || currentRow >= m_selectedList->count() - 1) return;
    
    QListWidgetItem *currentItem = m_selectedList->takeItem(currentRow);
    m_selectedList->insertItem(currentRow + 1, currentItem);
    m_selectedList->setCurrentRow(currentRow + 1);
}

void ColumnProfileDialog::onSaveProfile()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Save Profile", 
                                        "Profile name:", QLineEdit::Normal,
                                        m_profile.name(), &ok);
    
    if (!ok || name.isEmpty()) {
        return;
    }
    
    // Don't allow overwriting preset profiles
    QStringList presets = {"Default", "Compact", "Detailed", "F&O", "Equity", "Trading"};
    if (presets.contains(name)) {
        QMessageBox::warning(this, "Invalid Name", 
                           "Cannot overwrite preset profile. Please choose a different name.");
        return;
    }
    
    m_profile.setName(name);
    m_profile.setContext(m_context); // Ensure generic context is set
    
    // Reconstruct column order from UI before saving
    QList<MarketWatchColumn> newOrder;
    QList<MarketWatchColumn> currentFullOrder = m_profile.columnOrder();
    
    // 1. Add visible columns in UI order
    for(int i = 0; i < m_selectedList->count(); ++i) {
        QListWidgetItem *item = m_selectedList->item(i);
        newOrder.append(static_cast<MarketWatchColumn>(item->data(Qt::UserRole).toInt()));
    }
    
    // 2. Append hidden columns (maintain their relative order if possible, or just append)
    for(auto col : currentFullOrder) {
        if (!m_profile.isColumnVisible(col)) {
            newOrder.append(col);
        }
    }
    m_profile.setColumnOrder(newOrder);
    
    MarketWatchProfileManager &manager = MarketWatchProfileManager::instance();
    manager.addProfile(m_profile);
    manager.saveAllProfiles("profiles/marketwatch");  // Persist to disk
    
    QMessageBox::information(this, "Profile Saved", 
                           QString("Profile '%1' has been saved.").arg(name));
    
    refreshProfileList();
}

void ColumnProfileDialog::onDeleteProfile()
{
    QString profileName = m_profileCombo->currentText();
    
    // Can't delete preset profiles
    QStringList presets = {"Default", "Compact", "Detailed", "F&O", "Equity", "Trading"};
    if (presets.contains(profileName)) {
        QMessageBox::warning(this, "Cannot Delete", 
                           "Preset profiles cannot be deleted.");
        return;
    }
    
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Delete Profile",
                                 QString("Are you sure you want to delete profile '%1'?").arg(profileName),
                                 QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        MarketWatchProfileManager &manager = MarketWatchProfileManager::instance();
        if (manager.removeProfile(profileName)) {
            QMessageBox::information(this, "Profile Deleted", 
                                   QString("Profile '%1' has been deleted.").arg(profileName));
            refreshProfileList();
            m_profileCombo->setCurrentIndex(0);  // Reset to Default
        }
    }
}

void ColumnProfileDialog::onSetAsDefault()
{
    QString profileName = m_profileCombo->currentText();
    
    MarketWatchProfileManager &manager = MarketWatchProfileManager::instance();
    manager.setCurrentProfile(profileName);
    
    QMessageBox::information(this, "Default Set", 
                           QString("Profile '%1' set as default.").arg(profileName));
}

void ColumnProfileDialog::onAccepted()
{
    // Reconstruct final column order from UI
    QList<MarketWatchColumn> newOrder;
    QList<MarketWatchColumn> currentFullOrder = m_profile.columnOrder();
    
    // 1. Add visible columns in UI order
    for(int i = 0; i < m_selectedList->count(); ++i) {
        QListWidgetItem *item = m_selectedList->item(i);
        newOrder.append(static_cast<MarketWatchColumn>(item->data(Qt::UserRole).toInt()));
    }
    
    // 2. Append hidden columns
    for(auto col : currentFullOrder) {
        if (!m_profile.isColumnVisible(col)) {
            newOrder.append(col);
        }
    }
    m_profile.setColumnOrder(newOrder);

    m_accepted = true;
    accept();
}

void ColumnProfileDialog::onRejected()
{
    m_accepted = false;
    reject();
}
