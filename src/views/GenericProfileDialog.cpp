#include "views/GenericProfileDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
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
    , m_allColumns(allColumns)
    , m_manager(manager)
    , m_profile(currentProfile)
    , m_accepted(false)
{
    setWindowTitle(windowTitle + " — Column Selection");
    setModal(true);
    resize(480, 340);

    // ── Light-theme stylesheet (compact) ─────────────────────────────────
    setStyleSheet(
        "QDialog { background-color: #ffffff; color: #1e293b; font-size: 11px; }"
        "QLabel { color: #334155; font-size: 11px; font-weight: 500; }"
        "QComboBox {"
        "    background-color: #ffffff; color: #1e293b;"
        "    border: 1px solid #cbd5e1; border-radius: 3px;"
        "    padding: 3px 6px; min-height: 18px; font-size: 11px;"
        "}"
        "QComboBox:hover { border: 1px solid #94a3b8; }"
        "QComboBox:focus { border: 1px solid #3b82f6; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 16px; border: none; }"
        "QComboBox::down-arrow { image: none; border-left: 3px solid transparent; border-right: 3px solid transparent; border-top: 4px solid #64748b; margin-right: 4px; }"
        "QComboBox QAbstractItemView { background-color: #ffffff; color: #1e293b; selection-background-color: #dbeafe; selection-color: #1e40af; border: 1px solid #e2e8f0; }"
        "QComboBox QAbstractItemView::item { min-height: 18px; padding: 2px 6px; }"
        "QListWidget { background-color: #ffffff; color: #1e293b; border: 1px solid #e2e8f0; border-radius: 3px; outline: none; font-size: 11px; padding: 1px; }"
        "QListWidget::item { padding: 2px 6px; border: none; border-radius: 2px; margin: 0px; }"
        "QListWidget::item:selected { background-color: #dbeafe; color: #1e40af; }"
        "QListWidget::item:hover:!selected { background-color: #f1f5f9; }"
        "QListWidget::item:focus { outline: none; }"
        "QPushButton { background-color: #2563eb; color: #ffffff; border: 1px solid #2563eb; border-radius: 3px; padding: 3px 10px; font-size: 11px; font-weight: 500; min-width: 54px; min-height: 20px; }"
        "QPushButton:hover { background-color: #1d4ed8; border: 1px solid #1d4ed8; }"
        "QPushButton:pressed { background-color: #1e40af; border: 1px solid #1e40af; }"
        "QPushButton:disabled { background-color: #f1f5f9; color: #94a3b8; border: 1px solid #e2e8f0; }"
        "QPushButton:focus { outline: none; border: 1px solid #3b82f6; }"
    );

    setupUI();
    setupConnections();
    loadProfile(currentProfile);
}

// ============================================================================
// UI
// ============================================================================
void GenericProfileDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // ── Top: profile combo + buttons ─────────────────────────────────────
    QHBoxLayout *profileLayout = new QHBoxLayout();
    profileLayout->setSpacing(4);
    profileLayout->addWidget(new QLabel("Column Profile:"));

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(130);
    profileLayout->addWidget(m_profileCombo);

    m_saveButton       = new QPushButton("Save", this);
    m_deleteButton     = new QPushButton("Delete", this);
    m_setDefaultButton = new QPushButton("Set Default", this);

    profileLayout->addWidget(m_saveButton);
    profileLayout->addWidget(m_deleteButton);
    profileLayout->addWidget(m_setDefaultButton);
    profileLayout->addStretch();
    mainLayout->addLayout(profileLayout);

    // ── Middle: dual list ────────────────────────────────────────────────
    QHBoxLayout *listsLayout = new QHBoxLayout();
    listsLayout->setSpacing(4);

    QVBoxLayout *availLayout = new QVBoxLayout();
    availLayout->addWidget(new QLabel("Available Columns"));
    m_availableList = new QListWidget(this);
    m_availableList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    availLayout->addWidget(m_availableList);
    listsLayout->addLayout(availLayout);

    QVBoxLayout *arrowLayout = new QVBoxLayout();
    arrowLayout->addStretch();
    m_addButton    = new QPushButton("Add >", this);
    m_removeButton = new QPushButton("< Remove", this);
    arrowLayout->addWidget(m_addButton);
    arrowLayout->addWidget(m_removeButton);
    arrowLayout->addStretch();
    listsLayout->addLayout(arrowLayout);

    QVBoxLayout *selLayout = new QVBoxLayout();
    selLayout->addWidget(new QLabel("Selected Columns"));
    m_selectedList = new QListWidget(this);
    m_selectedList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    selLayout->addWidget(m_selectedList);
    listsLayout->addLayout(selLayout);

    QVBoxLayout *moveLayout = new QVBoxLayout();
    moveLayout->addStretch();
    m_moveUpButton   = new QPushButton("Move Up", this);
    m_moveDownButton = new QPushButton("Move Down", this);
    moveLayout->addWidget(m_moveUpButton);
    moveLayout->addWidget(m_moveDownButton);
    moveLayout->addStretch();
    listsLayout->addLayout(moveLayout);

    mainLayout->addLayout(listsLayout);

    // ── Bottom: OK / Cancel ──────────────────────────────────────────────
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_okButton     = new QPushButton("OK", this);
    m_cancelButton = new QPushButton("Cancel", this);
    m_okButton->setDefault(true);
    bottomLayout->addWidget(m_okButton);
    bottomLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(bottomLayout);

    refreshProfileList();
}

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
    m_manager->saveDefaultProfileName(m_profileCombo->currentText());
    QMessageBox::information(this, "Default Set",
                             QString("Profile '%1' set as default.").arg(m_profileCombo->currentText()));
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
