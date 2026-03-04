#include "views/GenericProfileDialog.h"
#include "ui_GenericProfileDialog.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QDebug>

// ============================================================================
// Constructor
// ============================================================================
GenericProfileDialog::GenericProfileDialog(const QString &windowTitle,
                                           const QList<GenericColumnInfo> &allColumns,
                                           GenericProfileManager *manager,
                                           const GenericTableProfile &currentProfile,
                                           QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::GenericProfileDialog)
    , m_allColumns(allColumns)
    , m_manager(manager)
    , m_profile(currentProfile)
    , m_accepted(false)
{
    ui->setupUi(this);
    setWindowTitle(windowTitle + " — Column Selection");

    // Assign convenience pointers from ui
    m_profileCombo   = ui->m_profileCombo;
    m_availableList  = ui->m_availableList;
    m_selectedList   = ui->m_selectedList;
    m_addButton      = ui->m_addButton;
    m_removeButton   = ui->m_removeButton;
    m_moveUpButton   = ui->m_moveUpButton;
    m_moveDownButton = ui->m_moveDownButton;
    m_saveButton     = ui->m_saveButton;
    m_deleteButton   = ui->m_deleteButton;
    m_setDefaultButton = ui->m_setDefaultButton;
    m_okButton       = ui->m_okButton;
    m_cancelButton   = ui->m_cancelButton;

    refreshProfileList();
    setupConnections();
    loadProfile(currentProfile);
}

GenericProfileDialog::~GenericProfileDialog() { delete ui; }

// ============================================================================
// Connections
// ============================================================================
void GenericProfileDialog::setupConnections()
{
    connect(m_profileCombo, &QComboBox::currentTextChanged, this, &GenericProfileDialog::onProfileChanged);
    connect(m_addButton,      &QPushButton::clicked, this, &GenericProfileDialog::onAddColumn);
    connect(m_removeButton,   &QPushButton::clicked, this, &GenericProfileDialog::onRemoveColumn);
    connect(m_moveUpButton,   &QPushButton::clicked, this, &GenericProfileDialog::onMoveUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &GenericProfileDialog::onMoveDown);
    connect(m_saveButton,     &QPushButton::clicked, this, &GenericProfileDialog::onSaveProfile);
    connect(m_deleteButton,   &QPushButton::clicked, this, &GenericProfileDialog::onDeleteProfile);
    connect(m_setDefaultButton, &QPushButton::clicked, this, &GenericProfileDialog::onSetAsDefault);
    connect(m_okButton,       &QPushButton::clicked, this, &GenericProfileDialog::onAccepted);
    connect(m_cancelButton,   &QPushButton::clicked, this, &GenericProfileDialog::onRejected);
}

// ============================================================================
// Helpers
// ============================================================================
QString GenericProfileDialog::columnNameById(int id) const
{
    for (const auto &c : m_allColumns) {
        if (c.id == id) return c.name;
    }
    return QString("Column %1").arg(id);
}

void GenericProfileDialog::loadProfile(const GenericTableProfile &profile)
{
    m_profile = profile;
    updateAvailableColumns();
    updateSelectedColumns();
}

void GenericProfileDialog::updateAvailableColumns()
{
    m_availableList->clear();
    for (const auto &info : m_allColumns) {
        if (!m_profile.isColumnVisible(info.id)) {
            auto *item = new QListWidgetItem(info.name);
            item->setData(Qt::UserRole, info.id);
            item->setForeground(QColor("#1e293b"));
            m_availableList->addItem(item);
        }
    }
}

void GenericProfileDialog::updateSelectedColumns()
{
    m_selectedList->clear();
    for (int id : m_profile.columnOrder()) {
        if (m_profile.isColumnVisible(id)) {
            auto *item = new QListWidgetItem(columnNameById(id));
            item->setData(Qt::UserRole, id);
            item->setForeground(QColor("#1e293b"));
            m_selectedList->addItem(item);
        }
    }
}

void GenericProfileDialog::refreshProfileList()
{
    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();

    // Presets first
    for (const QString &name : m_manager->presetNames())
        m_profileCombo->addItem(name);

    // Custom profiles (with separator)
    QStringList custom = m_manager->customProfileNames();
    if (!custom.isEmpty()) {
        m_profileCombo->insertSeparator(m_profileCombo->count());
        for (const QString &name : custom)
            m_profileCombo->addItem(name);
    }

    // Select current
    int idx = m_profileCombo->findText(m_profile.name());
    if (idx >= 0)
        m_profileCombo->setCurrentIndex(idx);

    m_profileCombo->blockSignals(false);
}

// ============================================================================
// Slots
// ============================================================================
void GenericProfileDialog::onProfileChanged(const QString &profileName)
{
    if (!m_manager->hasProfile(profileName)) return;
    loadProfile(m_manager->getProfile(profileName));
}

void GenericProfileDialog::onAddColumn()
{
    auto items = m_availableList->selectedItems();
    if (items.isEmpty()) return;

    QList<int> order = m_profile.columnOrder();
    for (auto *item : items) {
        int id = item->data(Qt::UserRole).toInt();
        m_profile.setColumnVisible(id, true);
        if (!order.contains(id)) order.append(id);
    }
    m_profile.setColumnOrder(order);
    updateAvailableColumns();
    updateSelectedColumns();
}

void GenericProfileDialog::onRemoveColumn()
{
    auto items = m_selectedList->selectedItems();
    if (items.isEmpty()) return;

    if (m_profile.visibleColumnCount() - items.size() < 1) {
        QMessageBox::warning(this, "Cannot Remove",
                             "At least one column must remain visible.");
        return;
    }

    for (auto *item : items) {
        m_profile.setColumnVisible(item->data(Qt::UserRole).toInt(), false);
    }
    updateAvailableColumns();
    updateSelectedColumns();
}

void GenericProfileDialog::onMoveUp()
{
    int row = m_selectedList->currentRow();
    if (row <= 0) return;
    auto *item = m_selectedList->takeItem(row);
    m_selectedList->insertItem(row - 1, item);
    m_selectedList->setCurrentRow(row - 1);
}

void GenericProfileDialog::onMoveDown()
{
    int row = m_selectedList->currentRow();
    if (row < 0 || row >= m_selectedList->count() - 1) return;
    auto *item = m_selectedList->takeItem(row);
    m_selectedList->insertItem(row + 1, item);
    m_selectedList->setCurrentRow(row + 1);
}

void GenericProfileDialog::onSaveProfile()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Save Profile",
                                         "Profile name:", QLineEdit::Normal,
                                         m_profile.name(), &ok);
    if (!ok || name.isEmpty()) return;

    if (m_manager->isPreset(name)) {
        QMessageBox::warning(this, "Invalid Name",
                             "Cannot overwrite a preset profile. Choose a different name.");
        return;
    }

    m_profile.setName(name);

    // Rebuild order from UI (visible in UI order, then hidden in current order)
    QList<int> newOrder;
    for (int i = 0; i < m_selectedList->count(); ++i)
        newOrder.append(m_selectedList->item(i)->data(Qt::UserRole).toInt());
    for (int id : m_profile.columnOrder()) {
        if (!m_profile.isColumnVisible(id))
            newOrder.append(id);
    }
    m_profile.setColumnOrder(newOrder);

    m_manager->saveCustomProfile(m_profile);
    m_manager->saveLastUsedProfile(m_profile);       // always persist as last-used too

    QMessageBox::information(this, "Profile Saved",
                             QString("Profile '%1' has been saved.").arg(name));
    refreshProfileList();
}

void GenericProfileDialog::onDeleteProfile()
{
    QString name = m_profileCombo->currentText();

    if (m_manager->isPreset(name)) {
        QMessageBox::warning(this, "Cannot Delete",
                             "Preset profiles cannot be deleted.");
        return;
    }

    auto reply = QMessageBox::question(this, "Delete Profile",
                                       QString("Delete profile '%1'?").arg(name),
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        if (m_manager->deleteCustomProfile(name)) {
            refreshProfileList();
            m_profileCombo->setCurrentIndex(0);
        }
    }
}

void GenericProfileDialog::onSetAsDefault()
{
    QString profileName = m_profileCombo->currentText();
    m_manager->saveDefaultProfileName(profileName);
    // Also persist the selected profile as last-used so it survives restart
    if (m_manager->hasProfile(profileName)) {
        m_manager->saveLastUsedProfile(m_manager->getProfile(profileName));
    }
    QMessageBox::information(this, "Default Set",
                             QString("Profile '%1' set as default.").arg(profileName));
}

void GenericProfileDialog::onAccepted()
{
    // Rebuild final column order from UI
    QList<int> newOrder;
    for (int i = 0; i < m_selectedList->count(); ++i)
        newOrder.append(m_selectedList->item(i)->data(Qt::UserRole).toInt());
    for (int id : m_profile.columnOrder()) {
        if (!m_profile.isColumnVisible(id))
            newOrder.append(id);
    }
    m_profile.setColumnOrder(newOrder);

    m_accepted = true;
    accept();
}

void GenericProfileDialog::onRejected()
{
    m_accepted = false;
    reject();
}
